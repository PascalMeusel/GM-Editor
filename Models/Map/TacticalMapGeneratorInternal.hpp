#pragma once

#include "Models/Map/TacticalMapGenerator.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace TacticalMapGeneratorInternal
{
using Point = MapCellCoordinate;

std::uint32_t tacticalHash(std::uint32_t seed, int x, int y, std::uint32_t salt = 0u);
bool cellCanSpawn(const MapCellModel *cell);
const char *templateName(TacticalMapTemplate value);
bool isSubterranean(TacticalMapTemplate value);
void setCell(MapModel &map, int x, int y, MapTileKind kind, MapWayKind way = MapWayKind::None);
void fill(MapModel &map, MapTileKind kind);
void paintRect(MapModel &map, int x, int y, int width, int height, MapTileKind kind,
               MapWayKind way = MapWayKind::None);
void paintEllipse(MapModel &map, int centerX, int centerY, int radiusX, int radiusY,
                  MapTileKind kind, std::uint32_t seed = 0u, bool irregular = false);
void addFeature(MapModel &map, MapGeneratedFeatureKind kind, const std::string &idSuffix,
                const std::string &name, const std::string &materialId,
                float width, const std::vector<Point> &points);
void paintSegment(MapModel &map, Point start, Point end, int radius, MapTileKind terrain,
                  MapWayKind way, bool bridgeWater);
void carveRoute(MapModel &map, const std::string &idSuffix, const std::string &name,
                const std::vector<Point> &points, int radius, MapTileKind terrain,
                MapWayKind way, bool bridgeWater = false, bool renderFeature = true);
void addRouteZone(MapModel &map, int index, int centerY);
void connectRouteEnds(MapModel &map, int upperY, int lowerY, MapTileKind terrain,
                      MapWayKind way);
bool tacticalSpawnFootprintIsUsable(const MapModel &map, int x, int y, int width, int height);
Point findTacticalSpawnOrigin(const MapModel &map, Point preferredCenter, int width, int height);
bool addSpawnZone(MapModel &map, const char *name, int x, int y, int width, int height);
void addObjectiveZone(MapModel &map, int index, int x, int y);
void addChokepoint(MapModel &map, int x, int y, const std::string &description);
void composeOpenGround(MapModel &map, std::uint32_t seed);
void composeForestRoad(MapModel &map, std::uint32_t seed);
void composeForestRoadV7(MapModel &map, std::uint32_t seed);
int tacticalCrossingUpperY(const MapModel &map);
int tacticalCrossingLowerY(const MapModel &map);
void composeRiverCrossing(MapModel &map, std::uint32_t seed);
void composeRuin(MapModel &map, std::uint32_t seed);
void composeRuinV8(MapModel &map, std::uint32_t seed);
bool addBuildingBlock(MapModel &map, int x, int y, int width, int height);
void composeVillageStreet(MapModel &map, std::uint32_t seed);
void composeCitySquare(MapModel &map, std::uint32_t seed);
void composeCave(MapModel &map, std::uint32_t seed);
void composeStoneDungeon(MapModel &map, std::uint32_t seed);
void composeTemplate(MapModel &map, const TacticalMapRecipe &recipe, std::uint32_t seed);
void installTacticalPreservedFeatures(MapModel &map, const TacticalMapRecipe &recipe);
void connectTacticalPreservedRoutes(MapModel &map, const TacticalMapRecipe &recipe);
void publishTacticalInheritedCrossings(MapModel &map);
std::array<Point, 2> tacticalDeploymentPreferences(const MapModel &map,
                                                   const TacticalMapRecipe &recipe);
bool tacticalPreservedFeatureMatches(const MapModel &map,
                                     const MapGeneratedFeature &preserved);
const MapRegionModel *findRegion(const MapModel &map, const std::string &name);
bool protectedZoneCell(const MapModel &map, int x, int y);
Point findTacticalObjective(const MapModel &map, Point preferred);
bool addSetpiece(MapModel &map, int index, int x, int y, MapTileKind kind,
                 const std::string &name);
void addFairCover(MapModel &map, const TacticalMapRecipe &recipe, std::uint32_t seed);
void addRequiredElements(MapModel &map, const TacticalMapRecipe &recipe, std::uint32_t seed);
bool regionReachable(const MapModel &map, const MapRegionModel &from,
                     const MapRegionModel &to);
bool hasFootprint(const MapModel &map, int size);
bool hasRouteAcrossBand(const MapModel &map, int routeY, bool upperBand);
std::vector<std::string> validate(const MapModel &map, const TacticalMapRecipe &recipe);
}
