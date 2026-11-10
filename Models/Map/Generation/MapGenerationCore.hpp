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

MapGenerationOptions mapGenerationOptionsFromCreationRequest(const MapCreationRequest &request);
void mapGenReportProgress(const MapGenerationOptions &options, int step, int totalSteps, const std::string &stage);

bool mapGenCancellationRequested(const MapGenerationOptions &options);

int mapGenClampPercent(int value);

int mapGenRandomBetween(std::mt19937 &rng, int minimum, int maximum);

bool mapGenChance(std::mt19937 &rng, int percent);

bool mapGenWayKindIsRoadLike(MapWayKind kind);

bool mapGenTerrainSupportsWay(MapTileKind terrainKind, MapWayKind wayKind);

MapTileKind mapGenRoadbedForTerrain(MapTileKind terrainKind);

MapTileKind mapGenArtContractRoadbedForWay(MapWayKind wayKind, MapTileKind terrainKind);

MapWayKind mapGenGeneratedRoadKind(const MapGenerationOptions &options);

void mapGenSetCell(MapModel &map, int x, int y, MapTileKind kind,
                   MapSymbolVariantMode symbolMode = MapSymbolVariantMode::Auto);

void mapGenClear(MapModel &map, MapTileKind kind);

bool mapGenIsAtlasBoundaryConstraint(const MapGenerationConstraint &constraint);

void mapGenUpsertPreservedFeature(MapModel &map, const MapGeneratedFeature &preserved);

void mapGenProjectPreservedFeaturesToCells(MapModel &map, const std::vector<MapGeneratedFeature> &preservedFeatures);

void mapGenInstallPreservedFeatures(MapModel &map, const std::vector<MapGeneratedFeature> &preservedFeatures);

void mapGenInstallPreservedFeatures(MapModel &map, const MapGenerationOptions &options);

void mapGenRestorePreservedFeatures(MapModel &map, const MapGenerationOptions &options);

void mapGenPaintDisc(MapModel &map, int centerX, int centerY, int radius, MapTileKind kind, std::mt19937 &rng,
                     int raggedPercent, MapSymbolVariantMode symbolMode = MapSymbolVariantMode::Auto);

bool mapGenNearKind(const MapModel &map, int x, int y, MapTileKind kind, int radius);

int mapGenOrthogonalNeighborCount(const MapModel &map, int x, int y, MapTileKind kind);

void mapGenAddTerrainBorder(MapModel &map, MapTileKind anchorKind, MapTileKind borderKind, MapTileKind replaceKind,
                            int radius, int percent, std::mt19937 &rng);

int mapGenStepToward(int value, int target);

bool mapGenBridgeCrossingLikeTerrain(MapTileKind kind);

bool mapGenDiagonalWayStepTouchesBridgeContext(const MapModel &map, int x, int y, int stepX, int stepY);

bool mapGenBridgeApproachLikeCell(const MapCellModel &cell);

bool mapGenBridgeCrossingContextCell(const MapCellModel &cell);

void mapGenSetWay(MapModel &map, int x, int y, MapWayKind wayKind);

void mapGenSetRiverCell(MapModel &map, int x, int y);

bool mapGenRiverInfluenceCell(const MapCellModel *cell);

void mapGenFillDiagonalRiverCornerGaps(MapModel &map);

void mapGenDrawWay(MapModel &map, int x1, int y1, int x2, int y2, MapWayKind kind, std::mt19937 &rng,
                   int wanderPercent = 18, bool paintTerrain = false, MapTileKind terrainKind = MapTileKind::Empty,
                   int terrainRadius = 0, bool allowDiagonal = true);

void mapGenDrawMountainRidge(MapModel &map, int startX, int startY, int endX, int endY,
                             const MapGenerationOptions &options, std::mt19937 &rng);

bool mapGenNearMountain(const MapModel &map, int x, int y, int radius);

void mapGenScatterClumps(MapModel &map, MapTileKind kind, int count, int minRadius, int maxRadius, std::mt19937 &rng,
                         int raggedPercent, MapSymbolVariantMode symbolMode = MapSymbolVariantMode::Auto);

bool mapGenComponentLooksFilled(int size, int minX, int minY, int maxX, int maxY, int fillPercent);

void mapGenBreakUpMountainSlabs(MapModel &map, std::mt19937 &rng);

bool mapGenIsStructureLike(MapTileKind kind);

bool mapGenCanStartStructureApproach(const MapCellModel *cell);

bool mapGenCellHasStructureApproach(const MapModel &map, int x, int y);

MapCellCoordinate mapGenNearestWayCell(const MapModel &map, int x, int y, int maxDistance, bool includeInteriorWays);

MapCellCoordinate mapGenChooseStructureApproachStart(const MapModel &map, int x, int y,
                                                     const MapCellCoordinate &target);

void mapGenConnectStructuresToWays(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

MapWayKind mapGenSecondaryRoadKind(MapWayKind primary);

void mapGenAddSameFamilyRoadBranches(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

void mapGenNormalizeGeneratedArtContractWays(MapModel &map);

void mapGenClearIsolatedWays(MapModel &map, MapWayKind kind);

void mapGenBuildMountains(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

void mapGenBuildWater(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

void mapGenBuildForestsAndSettlements(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

void mapGenBuildRoutes(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

bool mapGenCanHostTacticalFeature(const MapCellModel &cell, MapScope scope);

MapTileKind mapGenRandomTacticalFeature(MapScope scope, std::mt19937 &rng);

void mapGenBuildTacticalFeatures(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng);

std::string mapGenInteriorFeatureId(std::uint32_t seed, const char *family, std::size_t index);

void mapGenAddInteriorGraphFeature(MapModel &map, std::uint32_t seed, const char *family, std::size_t index,
                                   const char *name, const char *materialId, float width, bool closed,
                                   std::vector<MapFeaturePoint> points);

void mapGenBuildCaveNetwork(MapGenerationResult &result, const MapGenerationOptions &options, std::mt19937 &rng,
                            int totalSteps = 6);

