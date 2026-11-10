#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Models/Map/MapAtlasServiceRebaseInternal.hpp"

using namespace MapAtlasServiceInternal;

namespace MapAtlasService
{
MapAtlasRebasePreview previewRebase(const MapModel &child, const MapModel &parent)
{
    MapAtlasRebasePreview preview;
    preview.childMapId = child.metadata.id;
    preview.parentMapId = parent.metadata.id;
    preview.expectedChildRevision = child.revision;
    preview.expectedParentRevision = parent.revision;
    preview.expectedChildContentFingerprint = rebaseContentFingerprint(child);
    preview.expectedParentContentFingerprint = rebaseContentFingerprint(parent);

    if (child.atlasPlacement.parentId.empty())
        preview.errors.push_back("The child map has no normalized atlas placement.");
    else if (child.atlasPlacement.parentId != parent.metadata.id)
        preview.errors.push_back("The selected map is not this child map's atlas parent.");
    if (!isDeeperLevel(child.atlasLevel, parent.atlasLevel))
        preview.errors.push_back("The child map must be below its atlas parent.");
    if (!isNormalizedRectValid(child.atlasPlacement.parentRect))
        preview.errors.push_back("The child map has an invalid atlas placement.");
    if (!preview.errors.empty())
        return preview;

    const MapAtlasExtractionResult extraction = extractFromParent(parent, child.atlasPlacement, child.width, child.height);
    if (!extraction.valid)
    {
        preview.errors = extraction.errors;
        return preview;
    }
    preview.inheritedBoundaryConstraints = extraction.generationConstraints;

    std::unordered_map<std::string, const MapGeneratedFeature *> localInherited;
    std::unordered_set<std::string> localFeatureIds;
    for (const MapGeneratedFeature &feature : child.generation.graph.features)
    {
        if (inheritedFrom(feature, parent))
        {
            if (!localInherited.emplace(feature.id, &feature).second)
                preview.errors.push_back("The child contains duplicate inherited feature IDs: " + feature.id);
        }
        else if (!localFeatureIds.insert(feature.id).second)
            preview.errors.push_back("The child contains duplicate local feature IDs: " + feature.id);
    }

    std::unordered_set<std::string> matched;
    std::unordered_set<std::string> extractedFeatureIds;
    for (const MapGeneratedFeature &parentFeature : extraction.graph.features)
    {
        if (!extractedFeatureIds.insert(parentFeature.id).second)
        {
            preview.errors.push_back("The parent extraction produced a duplicate feature ID: " +
                                     parentFeature.id);
            continue;
        }
        if (localFeatureIds.find(parentFeature.id) != localFeatureIds.end())
        {
            preview.errors.push_back("An inherited feature ID conflicts with local child content: " +
                                     parentFeature.id);
            continue;
        }
        MapAtlasRebaseItem item;
        item.id = rebaseItemId(MapAtlasRebaseItemKind::Feature, parentFeature.id);
        item.kind = MapAtlasRebaseItemKind::Feature;
        item.sourceEntityId = parentFeature.lineage.sourceFeatureId;
        item.parentFeature = parentFeature;
        const auto localIt = localInherited.find(parentFeature.id);
        if (localIt == localInherited.end())
        {
            item.change = MapAtlasRebaseChange::AddedByParent;
        }
        else
        {
            item.localEntityId = localIt->second->id;
            item.localFeature = *localIt->second;
            matched.insert(localIt->first);
            item.change = sameFeature(*localIt->second, parentFeature)
                              ? MapAtlasRebaseChange::Unchanged
                              : MapAtlasRebaseChange::ChangedByParent;
        }
        item.requiresDecision = item.change != MapAtlasRebaseChange::Unchanged;
        preview.items.push_back(std::move(item));
    }

    for (const MapGeneratedFeature &feature : child.generation.graph.features)
    {
        if (!inheritedFrom(feature, parent) || matched.find(feature.id) != matched.end())
            continue;
        MapAtlasRebaseItem item;
        item.id = rebaseItemId(MapAtlasRebaseItemKind::Feature, feature.id);
        item.kind = MapAtlasRebaseItemKind::Feature;
        item.sourceEntityId = feature.lineage.sourceFeatureId;
        item.localEntityId = feature.id;
        item.change = MapAtlasRebaseChange::RemovedFromParent;
        item.requiresDecision = true;
        item.localFeature = feature;
        preview.items.push_back(std::move(item));
    }

    appendAtlasCollectionRebaseItems(
        child.regions, extraction.regions, parent, "region",
        MapAtlasRebaseItemKind::Region, sameRegion, preview.items, preview.errors);
    appendAtlasCollectionRebaseItems(
        child.routes, extraction.routes, parent, "route",
        MapAtlasRebaseItemKind::Route, sameRoute, preview.items, preview.errors);

    for (const MapGeneratedFeature &feature : child.generation.graph.features)
        if (inheritedFrom(feature, parent))
            for (const MapCellCoordinate &coordinate : mapGeneratedFeatureRasterCells(child, feature))
                preview.affectedRasterCellIndices.push_back(mapCellIndex(child, coordinate.x, coordinate.y));
    for (const MapGeneratedFeature &feature : extraction.graph.features)
        for (const MapCellCoordinate &coordinate : mapGeneratedFeatureRasterCells(child, feature))
            preview.affectedRasterCellIndices.push_back(mapCellIndex(child, coordinate.x, coordinate.y));
    const std::size_t expectedCellCount = static_cast<std::size_t>(std::max(0, child.width)) *
                                          static_cast<std::size_t>(std::max(0, child.height));
    if (child.cells.size() != expectedCellCount)
    {
        preview.warnings.push_back("The child raster was incomplete and will be initialized during rebase.");
        preview.affectedRasterCellIndices.clear();
        preview.affectedRasterCellIndices.reserve(expectedCellCount);
        for (std::size_t index = 0; index < expectedCellCount; ++index)
            preview.affectedRasterCellIndices.push_back(static_cast<int>(index));
    }
    else
    {
        std::sort(preview.affectedRasterCellIndices.begin(), preview.affectedRasterCellIndices.end());
        preview.affectedRasterCellIndices.erase(
            std::unique(preview.affectedRasterCellIndices.begin(),
                        preview.affectedRasterCellIndices.end()),
            preview.affectedRasterCellIndices.end());
    }

    if (child.atlasPlacement.parentRevision == parent.revision)
        preview.warnings.push_back("The child already references the current parent revision.");
    preview.valid = preview.errors.empty();
    return preview;
}
}
