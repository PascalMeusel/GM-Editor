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

#include "Models/Map/MapAtlasServiceInternal.hpp"

using namespace MapAtlasServiceInternal;

namespace MapAtlasService
{
MapAtlasExtractionResult extractFromParent(const MapModel &parent, const MapAtlasPlacement &placement, int childWidth, int childHeight)
{
    MapAtlasExtractionResult result;
    result.graph.schemaVersion = parent.generation.graph.schemaVersion;
    if (!isNormalizedRectValid(placement.parentRect))
        result.errors.push_back("The atlas placement rectangle is invalid.");
    if (childWidth <= 0 || childHeight <= 0)
        result.errors.push_back("The child map dimensions must be positive.");
    if (parent.width <= 0 || parent.height <= 0)
        result.errors.push_back("The parent map dimensions must be positive.");
    if (!placement.parentId.empty() && placement.parentId != parent.metadata.id)
        result.errors.push_back("The placement parent ID does not match the supplied parent map.");
    if (!result.errors.empty())
        return result;

    for (const MapGeneratedFeature &feature : parent.generation.graph.features)
    {
        if (feature.points.empty())
            continue;
        if (feature.points.size() == 1)
        {
            const MapAtlasPoint point = featurePointToNormalized(parent, feature.points.front());
            const std::optional<MapAtlasPoint> childPoint = parentToChildNormalized(placement, point);
            if (!childPoint)
                continue;
            MapGeneratedFeature extracted = feature;
            extracted.id = feature.id + "@atlas";
            extracted.points = {normalizedToFeaturePoint(*childPoint, childWidth, childHeight)};
            extracted.lineage = {parent.metadata.id, feature.id, parent.revision};
            extracted.locked = true;
            result.graph.features.push_back(std::move(extracted));
            continue;
        }

        const std::size_t segmentCount = feature.points.size() - 1 + (feature.closed ? 1 : 0);
        int extractedIndex = 0;
        for (std::size_t segment = 0; segment < segmentCount; ++segment)
        {
            const MapFeaturePoint &first = feature.points[segment % feature.points.size()];
            const MapFeaturePoint &second = feature.points[(segment + 1) % feature.points.size()];
            MapAtlasPoint clippedStart;
            MapAtlasPoint clippedEnd;
            if (!clipSegmentToRect(featurePointToNormalized(parent, first), featurePointToNormalized(parent, second), placement.parentRect, clippedStart, clippedEnd))
                continue;
            const std::optional<MapAtlasPoint> childStart = parentToChildNormalized(placement, clippedStart);
            const std::optional<MapAtlasPoint> childEnd = parentToChildNormalized(placement, clippedEnd);
            if (!childStart || !childEnd)
                continue;
            MapGeneratedFeature extracted = feature;
            extracted.id = feature.id + "@atlas_" + std::to_string(extractedIndex++);
            extracted.closed = false;
            extracted.points = {
                normalizedToFeaturePoint(*childStart, childWidth, childHeight),
                normalizedToFeaturePoint(*childEnd, childWidth, childHeight)};
            extracted.lineage = {parent.metadata.id, feature.id, parent.revision};
            extracted.locked = true;
            result.graph.features.push_back(std::move(extracted));
            addBoundaryConstraint(result, parent, feature, placement, clippedStart, childWidth, childHeight);
            addBoundaryConstraint(result, parent, feature, placement, clippedEnd, childWidth, childHeight);
        }
    }
    extractRegionsAndRoutes(result, parent, placement, childWidth, childHeight);
    result.valid = result.errors.empty();
    return result;
}

MapAtlasStaleReport detectStale(const MapModel &map, const std::vector<MapModel> &maps)
{
    MapAtlasStaleReport report;
    const EntityId parentId = effectiveParentId(map);
    if (parentId.empty())
    {
        report.suggestedStatus = MapAtlasStatus::Standalone;
        return report;
    }
    const MapLookup lookup = buildLookup(maps);
    const auto parentIt = lookup.find(parentId);
    if (parentIt == lookup.end())
    {
        report.stale = true;
        report.missingParent = true;
        report.suggestedStatus = MapAtlasStatus::Invalid;
        report.reasons.push_back("The linked parent map no longer exists.");
        return report;
    }
    if (map.atlasPlacement.parentId.empty())
    {
        report.suggestedStatus = MapAtlasStatus::Unpositioned;
        report.reasons.push_back("This legacy child map is in the hierarchy but has no normalized atlas placement yet.");
        return report;
    }
    if (map.atlasPlacement.parentRevision != parentIt->second->revision)
    {
        report.stale = true;
        report.parentChanged = true;
        report.reasons.push_back("The parent map changed after this placement was synchronized.");
    }
    if (map.atlasPlacement.sourceRevision != map.revision)
    {
        report.stale = true;
        report.sourceChanged = true;
        report.reasons.push_back("The child map changed after its atlas link was synchronized.");
    }
    report.suggestedStatus = report.stale ? MapAtlasStatus::Stale : MapAtlasStatus::Linked;
    return report;
}

MapAtlasPromotionPreview previewPromotion(const MapModel &source, const MapModel &parent)
{
    MapAtlasPromotionPreview preview;
    preview.sourceMapId = source.metadata.id;
    preview.parentMapId = parent.metadata.id;
    preview.expectedSourceRevision = source.revision;
    preview.expectedParentRevision = parent.revision;
    preview.expectedSourceContentFingerprint = rebaseContentFingerprint(source);
    preview.expectedParentContentFingerprint = rebaseContentFingerprint(parent);
    if (source.atlasPlacement.parentId.empty())
        preview.errors.push_back("The source map is unpositioned and must receive a normalized atlas placement before promotion.");
    else if (source.atlasPlacement.parentId != parent.metadata.id)
        preview.errors.push_back("The selected target is not the source map's atlas parent.");
    if (!isDeeperLevel(source.atlasLevel, parent.atlasLevel))
        preview.errors.push_back("Only a deeper child map can promote features to its parent.");
    if (!isNormalizedRectValid(source.atlasPlacement.parentRect))
        preview.errors.push_back("The source map has an invalid parent placement rectangle.");
    if (source.width <= 0 || source.height <= 0 || parent.width <= 0 || parent.height <= 0)
        preview.errors.push_back("Source and parent map dimensions must be positive.");
    if (!preview.errors.empty())
        return preview;

    if (source.atlasPlacement.parentRevision != parent.revision)
        preview.warnings.push_back("The parent changed since the atlas placement was synchronized.");
    if (source.atlasPlacement.sourceRevision != source.revision)
        preview.warnings.push_back("The source changed since the atlas placement was synchronized.");

    std::unordered_set<std::string> targetIds;
    for (const MapGeneratedFeature &feature : parent.generation.graph.features)
        targetIds.insert(feature.id);
    for (const MapGeneratedFeature &sourceFeature : source.generation.graph.features)
    {
        MapGeneratedFeature promoted = sourceFeature;
        promoted.id = source.metadata.id + "::" + sourceFeature.id;
        promoted.lineage = {source.metadata.id, sourceFeature.id, source.revision};
        for (MapFeaturePoint &point : promoted.points)
        {
            const MapAtlasPoint childNormalized{
                point.x / static_cast<float>(source.width),
                point.y / static_cast<float>(source.height)};
            const MapAtlasPoint parentNormalized = childToParentNormalized(source.atlasPlacement, childNormalized);
            point = normalizedToFeaturePoint(parentNormalized, parent.width, parent.height);
        }
        if (targetIds.find(promoted.id) != targetIds.end())
        {
            preview.conflicts.push_back({sourceFeature.id, promoted.id, "The parent already contains a promoted feature with this identity."});
            continue;
        }
        preview.promotedFeatures.push_back(std::move(promoted));
    }
    preview.valid = preview.errors.empty() && preview.conflicts.empty();
    return preview;
}

bool promotionPreviewIsCurrent(
    const MapModel &source,
    const MapModel &parent,
    const MapAtlasPromotionPreview &preview)
{
    return preview.valid &&
           preview.sourceMapId == source.metadata.id &&
           preview.parentMapId == parent.metadata.id &&
           preview.expectedSourceRevision == source.revision &&
           preview.expectedParentRevision == parent.revision &&
           preview.expectedSourceContentFingerprint == rebaseContentFingerprint(source) &&
           preview.expectedParentContentFingerprint == rebaseContentFingerprint(parent);
}
}
