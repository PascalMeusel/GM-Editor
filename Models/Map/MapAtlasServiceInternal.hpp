#pragma once

#include "Models/Map/MapAtlasService.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MapAtlasServiceInternal
{
inline constexpr float atlasEpsilon = 0.0001f;
using MapLookup = std::unordered_map<EntityId, const MapModel *>;

MapLookup buildLookup(const std::vector<MapModel> &maps);
bool pointInside(const MapNormalizedRect &rect, MapAtlasPoint point);
MapAtlasPoint featurePointToNormalized(const MapModel &map, const MapFeaturePoint &point);
MapFeaturePoint normalizedToFeaturePoint(MapAtlasPoint point, int width, int height);
bool clipSegmentToRect(MapAtlasPoint start, MapAtlasPoint end,
                       const MapNormalizedRect &rect, MapAtlasPoint &clippedStart,
                       MapAtlasPoint &clippedEnd);
std::optional<MapAtlasBoundaryEdge> boundaryEdge(const MapNormalizedRect &rect,
                                                 MapAtlasPoint point);
std::string edgeSuffix(MapAtlasBoundaryEdge edge);
bool samePoint(const MapFeaturePoint &left, const MapFeaturePoint &right);
bool sameFeature(const MapGeneratedFeature &left, const MapGeneratedFeature &right);
bool inheritedFrom(const MapGeneratedFeature &feature, const MapModel &parent);
void markAtlasDerived(EntityMetadata &metadata, const MapModel &parent,
                      const EntityId &sourceEntityId, const char *kind);
bool metadataInheritedFrom(const EntityMetadata &metadata, const MapModel &parent,
                           const char *kind);
std::string atlasSourceEntityId(const EntityMetadata &metadata);
std::map<std::string, std::string> authoredCustomProperties(const EntityMetadata &metadata);
bool sameAuthoredMetadata(const EntityMetadata &left, const EntityMetadata &right);
bool sameRegion(const MapRegionModel &left, const MapRegionModel &right);
bool sameRoute(const MapRouteModel &left, const MapRouteModel &right);
void detachAtlasLineage(EntityMetadata &metadata);
const char *rebaseItemKindName(MapAtlasRebaseItemKind kind);
std::string rebaseItemId(MapAtlasRebaseItemKind kind, const std::string &entityId);
std::uint64_t rebaseContentFingerprint(const MapModel &map);

bool cellEqual(const MapCellModel &left, const MapCellModel &right);
bool authoredCellStateEqual(const MapCellModel &left, const MapCellModel &right);
std::optional<MapTileKind> atlasFeatureTileKind(const MapGeneratedFeature &feature);
int projectionPass(const MapGeneratedFeature &feature);
bool affectedCell(const std::vector<int> &affected, int index);
void clearInheritedFeatureProjection(MapModel &map, const MapGeneratedFeature &feature,
                                     const std::vector<int> &affected);
void projectFeatureSemantics(MapModel &map, const MapGeneratedFeature &feature,
                             const std::vector<int> &affected);
void synchronizeRasterProjection(const MapModel &before, const MapModel &parent,
                                 MapModel &candidate, const std::vector<int> &affected);
bool validateRasterSynchronization(const MapModel &before, const MapModel &parent,
                                   const MapModel &candidate,
                                   const std::vector<int> &affected,
                                   std::vector<std::string> &errors);

void extractRegionsAndRoutes(MapAtlasExtractionResult &result, const MapModel &parent,
                             const MapAtlasPlacement &placement, int childWidth,
                             int childHeight);
void addBoundaryConstraint(MapAtlasExtractionResult &result, const MapModel &parent,
                           const MapGeneratedFeature &sourceFeature,
                           const MapAtlasPlacement &placement, MapAtlasPoint parentPoint,
                           int childWidth, int childHeight);
bool rebasePreviewContractMatches(const MapAtlasRebasePreview &provided,
                                  const MapAtlasRebasePreview &fresh);
std::string atlasConstraintSourceId(const std::string &constraintId);
bool featureIdsUnique(const std::vector<MapGeneratedFeature> &features);
MapAtlasHierarchyNode buildNode(
    const MapModel &map,
    const std::unordered_map<EntityId, std::vector<const MapModel *>> &children,
    std::unordered_set<EntityId> &path, std::unordered_set<EntityId> &emitted);
}
