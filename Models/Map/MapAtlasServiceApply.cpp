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
MapAtlasRebaseResult applyRebase(
    const MapModel &child,
    const MapModel &parent,
    const MapAtlasRebasePreview &preview,
    const std::vector<MapAtlasRebaseDecision> &decisions)
{
    MapAtlasRebaseResult result;
    result.map = child;
    const MapAtlasRebasePreview fresh = previewRebase(child, parent);
    if (!preview.valid || !fresh.valid)
        result.errors.push_back("The rebase preview is invalid.");
    if (preview.childMapId != child.metadata.id || preview.parentMapId != parent.metadata.id)
        result.errors.push_back("The rebase preview belongs to different maps.");
    if (preview.expectedChildRevision != child.revision ||
        preview.expectedParentRevision != parent.revision ||
        preview.expectedChildContentFingerprint != rebaseContentFingerprint(child) ||
        preview.expectedParentContentFingerprint != rebaseContentFingerprint(parent))
        result.errors.push_back("A map changed after the rebase preview was created. Create a new preview.");
    if (fresh.valid && !rebasePreviewContractMatches(preview, fresh))
        result.errors.push_back("The rebase preview no longer matches the current atlas extraction.");
    if (!result.errors.empty())
        return result;

    std::unordered_map<std::string, const MapAtlasRebaseItem *> itemsById;
    for (const MapAtlasRebaseItem &item : fresh.items)
        itemsById.emplace(item.id, &item);

    std::unordered_map<std::string, MapAtlasRebaseResolution> resolved;
    for (const MapAtlasRebaseDecision &decision : decisions)
    {
        const auto itemIt = itemsById.find(decision.itemId);
        if (itemIt == itemsById.end() || !itemIt->second->requiresDecision)
        {
            result.errors.push_back("The rebase contains a decision for an unknown or unchanged item: " +
                                    decision.itemId);
            continue;
        }
        if (!resolved.emplace(decision.itemId, decision.resolution).second)
            result.errors.push_back("The rebase contains duplicate decisions for: " + decision.itemId);
    }
    for (const MapAtlasRebaseItem &item : fresh.items)
        if (item.requiresDecision && resolved.find(item.id) == resolved.end())
            result.errors.push_back("A changed atlas item has no rebase decision: " + item.id);

    std::unordered_map<std::string, MapAtlasRebaseResolution> resolutionBySource;
    for (const MapAtlasRebaseItem &item : fresh.items)
    {
        if (item.kind != MapAtlasRebaseItemKind::Feature)
            continue;
        const MapAtlasRebaseResolution resolution = item.requiresDecision
                                                        ? resolved[item.id]
                                                        : MapAtlasRebaseResolution::UseParent;
        const auto inserted = resolutionBySource.emplace(item.sourceEntityId, resolution);
        if (!inserted.second && inserted.first->second != resolution)
            result.errors.push_back("All extracted segments of one parent feature require the same rebase decision: " +
                                    item.sourceEntityId);
    }
    if (!result.errors.empty())
        return result;

    std::unordered_set<std::string> inheritedIds;
    for (const MapAtlasRebaseItem &item : fresh.items)
        if (item.kind == MapAtlasRebaseItemKind::Feature && !item.localEntityId.empty())
            inheritedIds.insert(item.localEntityId);

    std::vector<MapGeneratedFeature> merged;
    merged.reserve(child.generation.graph.features.size() + fresh.items.size());
    for (const MapGeneratedFeature &feature : child.generation.graph.features)
        if (inheritedIds.find(feature.id) == inheritedIds.end())
            merged.push_back(feature);

    for (const MapAtlasRebaseItem &item : fresh.items)
    {
        if (item.kind != MapAtlasRebaseItemKind::Feature)
            continue;
        const MapAtlasRebaseResolution resolution = item.requiresDecision
                                                        ? resolved[item.id]
                                                        : MapAtlasRebaseResolution::UseParent;
        if (resolution == MapAtlasRebaseResolution::UseParent)
        {
            // A feature removed from the parent intentionally disappears.
            if (item.parentFeature)
                merged.push_back(*item.parentFeature);
        }
        else if (item.localFeature)
        {
            MapGeneratedFeature retained = *item.localFeature;
            if (resolution == MapAtlasRebaseResolution::Detach)
            {
                retained.lineage = {};
                retained.locked = false;
            }
            merged.push_back(std::move(retained));
        }
    }
    if (!featureIdsUnique(merged))
    {
        result.errors.push_back("The rebased feature graph contains empty or duplicate stable IDs.");
        return result;
    }

    MapModel candidate = child;
    candidate.generation.graph.schemaVersion = fresh.valid
                                                   ? std::max(candidate.generation.graph.schemaVersion,
                                                              parent.generation.graph.schemaVersion)
                                                   : candidate.generation.graph.schemaVersion;
    candidate.generation.graph.features = std::move(merged);

    std::vector<std::string> lockedFeatureIds;
    for (const std::string &id : child.generation.recipe.lockedFeatureIds)
        if (inheritedIds.find(id) == inheritedIds.end() &&
            std::find(lockedFeatureIds.begin(), lockedFeatureIds.end(), id) ==
                lockedFeatureIds.end())
            lockedFeatureIds.push_back(id);
    for (const MapGeneratedFeature &feature : candidate.generation.graph.features)
        if (inheritedFrom(feature, parent) && feature.locked &&
            std::find(lockedFeatureIds.begin(), lockedFeatureIds.end(), feature.id) ==
                lockedFeatureIds.end())
            lockedFeatureIds.push_back(feature.id);
    candidate.generation.recipe.lockedFeatureIds = std::move(lockedFeatureIds);

    std::vector<MapGenerationConstraint> constraints;
    std::unordered_set<std::string> constraintIds;
    for (const MapGenerationConstraint &constraint : child.generation.recipe.constraints)
    {
        const std::string sourceId = atlasConstraintSourceId(constraint.id);
        if (sourceId.empty())
        {
            if (constraintIds.insert(constraint.id).second)
                constraints.push_back(constraint);
            continue;
        }
        const auto resolution = resolutionBySource.find(sourceId);
        if (resolution != resolutionBySource.end() &&
            resolution->second == MapAtlasRebaseResolution::KeepLocal &&
            constraintIds.insert(constraint.id).second)
            constraints.push_back(constraint);
    }
    for (const MapGenerationConstraint &constraint : fresh.inheritedBoundaryConstraints)
    {
        const std::string sourceId = atlasConstraintSourceId(constraint.id);
        const auto resolution = resolutionBySource.find(sourceId);
        if ((resolution == resolutionBySource.end() ||
             resolution->second == MapAtlasRebaseResolution::UseParent) &&
            constraintIds.insert(constraint.id).second)
            constraints.push_back(constraint);
    }
    candidate.generation.recipe.constraints = std::move(constraints);

    if (!mergeAtlasCollectionByDecision(candidate.regions, parent, "region",
                                        MapAtlasRebaseItemKind::Region,
                                        fresh.items, resolved, result.errors) ||
        !mergeAtlasCollectionByDecision(candidate.routes, parent, "route",
                                        MapAtlasRebaseItemKind::Route,
                                        fresh.items, resolved, result.errors))
        return result;

    synchronizeRasterProjection(child, parent, candidate, fresh.affectedRasterCellIndices);
    if (!validateRasterSynchronization(child, parent, candidate,
                                       fresh.affectedRasterCellIndices, result.errors))
        return result;

    candidate.atlasPlacement.parentRevision = parent.revision;
    candidate.revision = child.revision + 1;
    candidate.atlasPlacement.sourceRevision = candidate.revision;
    candidate.atlasStatus = MapAtlasStatus::Linked;
    candidate.metadata.touch();
    result.map = std::move(candidate);
    result.synchronizedRasterCellIndices = fresh.affectedRasterCellIndices;
    result.applied = true;
    return result;
}

MapAtlasRebaseResult applyRebaseAtomically(
    MapModel &child,
    const MapModel &parent,
    const MapAtlasRebasePreview &preview,
    const std::vector<MapAtlasRebaseDecision> &decisions)
{
    MapAtlasRebaseResult result = applyRebase(child, parent, preview, decisions);
    if (!result.applied)
        return result;
    MapModel committed = result.map;
    using std::swap;
    swap(child, committed);
    result.map = child;
    return result;
}
}
