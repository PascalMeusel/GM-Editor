#pragma once

#include "Models/Map/MapModels.hpp"

#include <optional>
#include <string>
#include <vector>

enum class MapAtlasBoundaryEdge
{
    North,
    East,
    South,
    West
};

struct MapAtlasPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct MapAtlasValidationIssue
{
    std::string code;
    EntityId mapId;
    EntityId relatedMapId;
    std::string message;
};

struct MapAtlasValidationResult
{
    bool valid = true;
    std::vector<MapAtlasValidationIssue> issues;
};

struct MapAtlasHierarchyNode
{
    EntityId mapId;
    std::string name;
    MapLevel level = MapLevel::World;
    std::vector<MapAtlasHierarchyNode> children;
};

struct MapAtlasBreadcrumb
{
    EntityId mapId;
    std::string name;
    MapLevel level = MapLevel::World;
};

struct MapAtlasBoundaryConstraint
{
    std::string id;
    MapGeneratedFeatureKind featureKind = MapGeneratedFeatureKind::Landmark;
    MapAtlasBoundaryEdge edge = MapAtlasBoundaryEdge::North;
    MapFeaturePoint childPoint;
    MapFeatureLineage lineage;
};

struct MapAtlasExtractionResult
{
    bool valid = false;
    MapFeatureGraph graph;
    // Regions and routes are atlas-derived semantic representations just like
    // the feature graph. They carry stable source metadata and participate in
    // the same explicit rebase decision contract.
    std::vector<MapRegionModel> regions;
    std::vector<MapRouteModel> routes;
    std::vector<MapAtlasBoundaryConstraint> boundaryConstraints;
    std::vector<MapGenerationConstraint> generationConstraints;
    std::vector<std::string> errors;
};

struct MapAtlasStaleReport
{
    bool stale = false;
    bool missingParent = false;
    bool parentChanged = false;
    bool sourceChanged = false;
    MapAtlasStatus suggestedStatus = MapAtlasStatus::Standalone;
    std::vector<std::string> reasons;
};

struct MapAtlasPromotionConflict
{
    std::string sourceFeatureId;
    std::string targetFeatureId;
    std::string message;
};

struct MapAtlasPromotionPreview
{
    bool valid = false;
    EntityId sourceMapId;
    EntityId parentMapId;
    std::uint64_t expectedSourceRevision = 0;
    std::uint64_t expectedParentRevision = 0;
    // Older authoring paths did not always advance revisions. Bind the
    // preview to the complete atlas-relevant state as well, so a same-revision
    // edit cannot be promoted from a stale preview.
    std::uint64_t expectedSourceContentFingerprint = 0;
    std::uint64_t expectedParentContentFingerprint = 0;
    std::vector<MapGeneratedFeature> promotedFeatures;
    std::vector<MapAtlasPromotionConflict> conflicts;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

enum class MapAtlasRebaseChange
{
    Unchanged,
    AddedByParent,
    ChangedByParent,
    RemovedFromParent
};

enum class MapAtlasRebaseResolution
{
    UseParent,
    KeepLocal,
    Detach
};

enum class MapAtlasRebaseItemKind
{
    Feature,
    Region,
    Route
};

struct MapAtlasRebaseItem
{
    // Decision IDs are type-namespaced and stable for one inherited entity.
    // sourceEntityId is the parent identity; localEntityId is the current
    // child identity. Keeping these generic prevents regions and routes from
    // becoming a second, weaker rebase API beside feature decisions.
    std::string id;
    MapAtlasRebaseItemKind kind = MapAtlasRebaseItemKind::Feature;
    std::string sourceEntityId;
    std::string localEntityId;
    MapAtlasRebaseChange change = MapAtlasRebaseChange::Unchanged;
    bool requiresDecision = false;
    std::optional<MapGeneratedFeature> localFeature;
    std::optional<MapGeneratedFeature> parentFeature;
    std::optional<MapRegionModel> localRegion;
    std::optional<MapRegionModel> parentRegion;
    std::optional<MapRouteModel> localRoute;
    std::optional<MapRouteModel> parentRoute;
};

struct MapAtlasRebasePreview
{
    bool valid = false;
    EntityId childMapId;
    EntityId parentMapId;
    std::uint64_t expectedChildRevision = 0;
    std::uint64_t expectedParentRevision = 0;
    // Revisions alone are not sufficient because older authoring code did not
    // always advance them. Fingerprints bind the preview to all atlas-relevant
    // graph, raster, constraint, region and route input.
    std::uint64_t expectedChildContentFingerprint = 0;
    std::uint64_t expectedParentContentFingerprint = 0;
    std::vector<MapAtlasRebaseItem> items;
    std::vector<MapGenerationConstraint> inheritedBoundaryConstraints;
    std::vector<int> affectedRasterCellIndices;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

struct MapAtlasRebaseDecision
{
    std::string itemId;
    MapAtlasRebaseResolution resolution = MapAtlasRebaseResolution::UseParent;
};

struct MapAtlasRebaseResult
{
    bool applied = false;
    MapModel map;
    std::vector<int> synchronizedRasterCellIndices;
    std::vector<std::string> errors;
};

namespace MapAtlasService
{
int levelDepth(MapLevel level);
bool isDeeperLevel(MapLevel child, MapLevel parent);
bool isNextLevel(MapLevel child, MapLevel parent);
MapLevel levelFromLegacyScope(MapScope scope);
MapKind kindFromLegacyScope(MapScope scope);
EntityId effectiveParentId(const MapModel &map);
bool isNormalizedRectValid(const MapNormalizedRect &rect);

MapAtlasValidationResult validate(const std::vector<MapModel> &maps);
const MapModel *primaryWorldMap(const std::vector<MapModel> &maps);
std::vector<MapAtlasHierarchyNode> buildHierarchy(const std::vector<MapModel> &maps);
std::vector<MapAtlasBreadcrumb> breadcrumbs(const std::vector<MapModel> &maps, const EntityId &mapId);

MapAtlasPoint childToParentNormalized(const MapAtlasPlacement &placement, MapAtlasPoint childPoint);
std::optional<MapAtlasPoint> parentToChildNormalized(const MapAtlasPlacement &placement, MapAtlasPoint parentPoint);

MapAtlasExtractionResult extractFromParent(
    const MapModel &parent,
    const MapAtlasPlacement &placement,
    int childWidth,
    int childHeight);

MapAtlasStaleReport detectStale(const MapModel &map, const std::vector<MapModel> &maps);
MapAtlasPromotionPreview previewPromotion(const MapModel &source, const MapModel &parent);
bool promotionPreviewIsCurrent(
    const MapModel &source,
    const MapModel &parent,
    const MapAtlasPromotionPreview &preview);
MapAtlasRebasePreview previewRebase(const MapModel &child, const MapModel &parent);
MapAtlasRebaseResult applyRebase(
    const MapModel &child,
    const MapModel &parent,
    const MapAtlasRebasePreview &preview,
    const std::vector<MapAtlasRebaseDecision> &decisions);
// In-place convenience for callers that need service-level transactionality.
// The child is swapped only after the complete candidate validates; every
// failure leaves it unchanged and returns the original map in result.map.
MapAtlasRebaseResult applyRebaseAtomically(
    MapModel &child,
    const MapModel &parent,
    const MapAtlasRebasePreview &preview,
    const std::vector<MapAtlasRebaseDecision> &decisions);
}
