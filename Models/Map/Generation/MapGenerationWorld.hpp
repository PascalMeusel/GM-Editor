#pragma once
#include "Models/Map/Generation/MapGenerationTypes.hpp"

#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStyleRegistry.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <vector>

float mapGenV2Hash01(std::uint32_t seed, int x, int y);

float mapGenV2Smooth(float value);

float mapGenV2ValueNoise(std::uint32_t seed, float x, float y, int scale);

float mapGenV2FractalNoise(std::uint32_t seed, float x, float y, int baseScale);

std::string mapGenV2FeatureId(std::uint32_t seed, const char *family, int index);

std::string mapGenNormalizedNameCulture(std::string culture);

template <std::size_t Count>
inline std::string mapGenPickCultureName(const std::array<const char *, Count> &names, std::uint32_t seed, int index,
                                         std::uint32_t salt, std::size_t step)
{
    const std::size_t offset = static_cast<std::size_t>((seed ^ (seed >> 16u) ^ salt) % Count);
    return names[(offset + static_cast<std::size_t>(std::max(0, index)) * step) % Count];
}

std::string mapGenV4SettlementName(std::uint32_t seed, int index, const std::string &nameCulture = "neutral_fantasy");

std::string mapGenV4LandmarkName(std::uint32_t seed, int index, const std::string &nameCulture = "neutral_fantasy");

bool mapGenV4TextRequestsSharedGreatRiver(const std::vector<std::string> &knownFacts,
                                          const std::vector<std::string> &requiredElements);

bool mapGenV4RequestsSharedGreatRiver(const MapGenerationOptions &options);

bool mapGenV4RequestsSharedGreatRiver(const MapModel &map);

void mapGenV2AddPathFeature(MapModel &map, MapGeneratedFeatureKind kind, const std::string &id, const std::string &name,
                            const std::string &material, float width, const std::vector<MapCellCoordinate> &cells);

bool mapGenV6RegionJoinFirstRiverConfluence(const MapModel &map, std::vector<MapCellCoordinate> &path,
                                            MapFeaturePoint &confluence);

void mapGenV5SmoothWorldPathFeature(MapGeneratedFeature &feature, int passes = 3);

int mapGenV2RoadTraversalCost(const MapCellModel &cell);

bool mapGenV6RegionHasCompleteRiverCrossing(const MapModel &map, const std::vector<MapCellCoordinate> &river);

bool mapGenV5WorldOpenWaterCell(const MapCellModel *cell);

bool mapGenV5WorldDiagonalRiverHelperCell(const MapModel &map, int x, int y);

bool mapGenV5WorldOpenWaterAt(const MapModel &map, int x, int y);

int mapGenV5WorldOpenWaterDistance(const MapModel &map, int x, int y, int maximumDistance = 3);

bool mapGenV5WorldRoadSegmentIsSafe(const MapModel &map, const MapFeaturePoint &start, const MapFeaturePoint &end);

bool mapGenV5WorldRoadSegmentIsDry(const MapModel &map, const MapFeaturePoint &start, const MapFeaturePoint &end);

int mapGenV7OverviewTerrainFriction(const MapCellModel &cell);

float mapGenV7RoadSegmentEffort(const MapModel &map, MapFeaturePoint start, MapFeaturePoint end);

bool mapGenV7RoadChordIsSafe(const MapModel &map, MapFeaturePoint start, MapFeaturePoint end);

void mapGenV7FitOverviewRoadPathFeature(const MapModel &map, MapGeneratedFeature &feature);

bool mapGenV7NearestDryRoadAnchor(const MapModel &map, MapFeaturePoint from, MapFeaturePoint &anchor);

void mapGenV7JoinFirstExistingRoad(MapGeneratedFeature &road, const MapFeatureGraph &graph,
                                   const std::vector<std::string> *eligibleIds = nullptr,
                                   const MapModel *junctionMap = nullptr);

MapFeaturePoint mapGenV11RoadBoundaryAnchor(const MapModel &map, MapCellCoordinate cell);

void mapGenV5SmoothWorldRoadPathFeature(const MapModel &map, MapGeneratedFeature &feature, int passes = 3);

void mapGenV2RasterizeRoad(MapModel &map, const std::vector<MapCellCoordinate> &path, MapWayKind kind);

void mapGenV7CommitFittedRoadProjection(MapModel &map, const MapGeneratedFeature &feature,
                                        std::vector<MapCellModel> previousCells, MapWayKind roadKind);

bool mapGenV2NearestFeatureTangent(const MapFeatureGraph &graph, MapGeneratedFeatureKind kind, float x, float y,
                                   float maximumDistance, float &tangentX, float &tangentY);

std::vector<MapCellCoordinate> mapGenV2FindRoadPath(const MapModel &map, MapCellCoordinate start,
                                                    MapCellCoordinate goal, bool avoidNewWaterCrossings = false);

MapGenV7BridgeApproach mapGenV7ChooseBridgeApproach(const MapModel &map,
                                                    const std::vector<MapCellCoordinate> &crossing);

void mapGenV2RasterizeRoad(MapModel &map, const std::vector<MapCellCoordinate> &path, MapWayKind kind);

bool mapGenV2NearestFeatureTangent(const MapFeatureGraph &graph, MapGeneratedFeatureKind kind, float x, float y,
                                   float maximumDistance, float &tangentX, float &tangentY);

MapCellCoordinate mapGenV2ChooseSettlement(const MapModel &map, const std::vector<MapCellCoordinate> &existing,
                                           std::mt19937 &rng);

MapCellCoordinate mapGenV4ChooseSharedRiverSettlement(const MapModel &map, const std::vector<MapCellCoordinate> &river,
                                                      const std::vector<MapCellCoordinate> &existing, int ordinal,
                                                      int targetCount, std::uint32_t seed,
                                                      float requestedFraction = -1.0f);

void mapGenV2PaintSettlement(MapModel &map, MapCellCoordinate center, int index, std::mt19937 &rng,
                             bool paintIndividualBuildings);

std::string mapGenV5RelationNameKey(std::string value);

std::string mapGenV5RelationKindKey(const MapGenerationFact &fact);

int mapGenV5NamedNationIndex(const MapGenerationOptions &options, const std::string &name);

void mapGenV5ApplyNationRelations(std::vector<MapFeaturePoint> &centers, const MapGenerationOptions &options, int width,
                                  int height);

MapFeaturePoint mapGenV5SnapNationCenterToLand(const MapModel &map, MapFeaturePoint requested);

std::vector<MapFeaturePoint> mapGenV5TraceNationLandBoundary(const MapModel &map,
                                                             const std::vector<MapFeaturePoint> &centers,
                                                             std::size_t nationIndex);

void mapGenV2BuildNationTerritories(MapModel &map, const MapGenerationOptions &options);

bool mapGenV5PointInsideTerritory(const MapFeaturePoint &point, const MapGeneratedFeature &territory);

std::string mapGenV11AuthoredSettlementNationName(const std::vector<MapGenerationFact> &facts,
                                                  const std::string &settlementName);

MapCellCoordinate mapGenV5ChooseWorldSettlement(const MapModel &map, const std::vector<MapCellCoordinate> &existing,
                                                std::uint32_t seed, int ordinal,
                                                const std::string &requiredNation = {});

bool mapGenV5RelationFeatureCenter(const MapGeneratedFeature &feature, MapFeaturePoint &center);

const MapGeneratedFeature *mapGenV5FindRelationFeature(const MapModel &map, const std::string &name);

std::uint32_t mapGenV5RelationTextHash(const std::string &value);

std::vector<MapGenerationConstraint> mapGenV5BuildRelationalPointConstraints(const MapModel &map,
                                                                             const MapGenerationOptions &options);

std::vector<MapCellCoordinate> mapGenV2BuildRiver(MapModel &map, const MapGenerationOptions &options,
                                                  const std::vector<float> &elevation, std::mt19937 &rng);

std::vector<MapCellCoordinate> mapGenV4FindDrainagePath(const MapModel &map, const std::vector<float> &elevation,
                                                        MapCellCoordinate source, MapCellCoordinate outlet,
                                                        std::uint32_t noiseSeed, bool composedValley = false);

std::vector<MapCellCoordinate> mapGenV4BuildSharedGreatRiver(MapModel &map, const MapGenerationOptions &options,
                                                             const std::vector<float> &elevation);

std::vector<MapCellCoordinate> mapGenV4BuildHydrology(MapModel &map, const MapGenerationOptions &options,
                                                      const std::vector<float> &elevation);

float mapGenV4WorldLandScore(std::uint32_t seed, float normalizedX, float normalizedY);

// V10 bends the continental mass before sampling the coastline. Broad bays
// and capes are structural geography, independent of the display resolution.
// Shared-river facts retain their existing connected drainage-basin contract.
float mapGenV10WorldLandScore(std::uint32_t seed, float x, float y, int waterLevel, MapLandscapeProfile landscape);

float mapGenV4SharedRiverWorldLandScore(std::uint32_t seed, float normalizedX, float normalizedY);

float mapGenV5SharedRiverWorldLandScore(std::uint32_t seed, float normalizedX, float normalizedY);

float mapGenV5WorldMountainRidgeInfluence(std::uint32_t seed, float normalizedX, float normalizedY, int ridgeIndex,
                                          int mountainDensity);

float mapGenV5WorldForestClusterInfluence(std::uint32_t seed, float normalizedX, float normalizedY, int clusterIndex);

bool mapGenV5WorldTerrainOverlayGapCell(const MapModel &map, int x, int y, bool includeMountainPasses);

std::vector<std::vector<MapCellCoordinate>> mapGenV5WorldTerrainComponents(const MapModel &map, MapTileKind kind,
                                                                           bool includeMountainPasses);

void mapGenV5AddWorldMountainRangeFeatures(MapModel &map, const MapGenerationOptions &options);

void mapGenV2BuildOverworld(MapGenerationResult &result, const MapGenerationOptions &options, std::mt19937 &rng,
                            int totalSteps);

#include "Models/Map/MapSettlementCompositionV10.inl"

void mapGenV3BuildSettlement(MapGenerationResult &result, const MapGenerationOptions &options, std::mt19937 &rng,
                             int totalSteps);

