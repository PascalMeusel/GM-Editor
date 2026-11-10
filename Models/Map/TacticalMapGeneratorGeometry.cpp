#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "Models/Map/TacticalMapGeneratorInternal.hpp"

namespace TacticalMapGeneratorInternal
{
std::uint32_t tacticalHash(std::uint32_t seed, int x, int y, std::uint32_t salt)
{
    std::uint32_t value = seed ^ salt ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u) ^
                          (static_cast<std::uint32_t>(y) * 0x85ebca6bu);
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

bool cellCanSpawn(const MapCellModel *cell)
{
    return cell && mapGenQualityTraversable(*cell) && cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Wall &&
           cell->kind != MapTileKind::Building && cell->kind != MapTileKind::Pit &&
           cell->kind != MapTileKind::Chasm && cell->kind != MapTileKind::Lava;
}

const char *templateName(TacticalMapTemplate value)
{
    switch (value)
    {
    case TacticalMapTemplate::OpenGround: return "Open Ground";
    case TacticalMapTemplate::ForestRoad: return "Forest Road";
    case TacticalMapTemplate::RiverCrossing: return "River Crossing";
    case TacticalMapTemplate::Ruin: return "Ruins";
    case TacticalMapTemplate::VillageStreet: return "Village Street";
    case TacticalMapTemplate::CitySquare: return "City Square";
    case TacticalMapTemplate::Cave: return "Cave";
    case TacticalMapTemplate::StoneDungeon: return "Stone Dungeon";
    }
    return "Battle Map";
}

bool isSubterranean(TacticalMapTemplate value)
{
    return value == TacticalMapTemplate::Cave || value == TacticalMapTemplate::StoneDungeon;
}

void setCell(MapModel &map, int x, int y, MapTileKind kind, MapWayKind way)
{
    MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell)
        return;
    applyMapCellTerrainDefaults(*cell, kind);
    cell->wayKind = way;
}

void fill(MapModel &map, MapTileKind kind)
{
    resizeMapCells(map);
    for (MapCellModel &cell : map.cells)
    {
        cell = MapCellModel{};
        applyMapCellTerrainDefaults(cell, kind);
    }
}

void paintRect(MapModel &map, int x, int y, int width, int height, MapTileKind kind, MapWayKind way)
{
    for (int cy = std::max(0, y); cy < std::min(map.height, y + height); ++cy)
        for (int cx = std::max(0, x); cx < std::min(map.width, x + width); ++cx)
            setCell(map, cx, cy, kind, way);
}

void paintEllipse(MapModel &map, int centerX, int centerY, int radiusX, int radiusY, MapTileKind kind,
                  std::uint32_t seed, bool irregular)
{
    radiusX = std::max(1, radiusX);
    radiusY = std::max(1, radiusY);
    for (int y = centerY - radiusY; y <= centerY + radiusY; ++y)
    {
        for (int x = centerX - radiusX; x <= centerX + radiusX; ++x)
        {
            const float dx = static_cast<float>(x - centerX) / static_cast<float>(radiusX);
            const float dy = static_cast<float>(y - centerY) / static_cast<float>(radiusY);
            const float fringe = irregular ? static_cast<float>(tacticalHash(seed, x, y, 0x31u) % 21u) / 100.0f - 0.10f : 0.0f;
            if (dx * dx + dy * dy <= 1.0f + fringe)
                setCell(map, x, y, kind);
        }
    }
}

void addFeature(MapModel &map, MapGeneratedFeatureKind kind, const std::string &idSuffix, const std::string &name,
                const std::string &materialId, float width, const std::vector<Point> &points)
{
    if (points.empty())
        return;
    MapGeneratedFeature feature;
    feature.id = map.metadata.id + "::" + idSuffix;
    feature.kind = kind;
    feature.name = name;
    feature.materialId = materialId;
    feature.width = width;
    feature.points.reserve(points.size());
    for (const Point point : points)
        feature.points.push_back({std::clamp(point.x + 0.5f, 0.0f, static_cast<float>(map.width)),
                                  std::clamp(point.y + 0.5f, 0.0f, static_cast<float>(map.height))});
    if(map.generation.recipe.recipeVersion>=6 &&
       (kind==MapGeneratedFeatureKind::Road || kind==MapGeneratedFeatureKind::Trail) && points.size()>1) {
        const auto meetEdge=[&](MapFeaturePoint &point) {
            if(point.x<=.5f)point.x=0.f;else if(point.x>=map.width-.5f)point.x=float(map.width);
            if(point.y<=.5f)point.y=0.f;else if(point.y>=map.height-.5f)point.y=float(map.height);
        };
        meetEdge(feature.points.front());meetEdge(feature.points.back());
    }
    map.generation.graph.features.push_back(std::move(feature));
}

void paintSegment(MapModel &map, Point start, Point end, int radius, MapTileKind terrain, MapWayKind way, bool bridgeWater)
{
    const int dx = end.x - start.x;
    const int dy = end.y - start.y;
    const int steps = std::max(std::abs(dx), std::abs(dy));
    for (int step = 0; step <= std::max(1, steps); ++step)
    {
        const float t = steps == 0 ? 0.0f : static_cast<float>(step) / static_cast<float>(steps);
        const int cx = static_cast<int>(std::round(start.x + dx * t));
        const int cy = static_cast<int>(std::round(start.y + dy * t));
        for (int oy = -radius; oy <= radius; ++oy)
        {
            for (int ox = -radius; ox <= radius; ++ox)
            {
                if (ox * ox + oy * oy > radius * radius + radius)
                    continue;
                MapCellModel *cell = mapCellAt(map, cx + ox, cy + oy);
                if (!cell)
                    continue;
                if (bridgeWater && (cell->kind == MapTileKind::Water || cell->wayKind == MapWayKind::River))
                    setCell(map, cx + ox, cy + oy, MapTileKind::Bridge, way);
                else
                    setCell(map, cx + ox, cy + oy, terrain, way);
            }
        }
    }
}

void carveRoute(MapModel &map, const std::string &idSuffix, const std::string &name, const std::vector<Point> &points,
                int radius, MapTileKind terrain, MapWayKind way, bool bridgeWater, bool renderFeature)
{
    for (std::size_t index = 1; index < points.size(); ++index)
        paintSegment(map, points[index - 1], points[index], radius, terrain, way, bridgeWater);
    if (renderFeature)
    {
        const MapGeneratedFeatureKind featureKind = way == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road;
        const std::string material = way == MapWayKind::StoneRoad ? "fantasy_neutral.road.stone" :
                                     (way == MapWayKind::Trail ? "fantasy_neutral.road.trail" : "fantasy_neutral.road.dirt");
        addFeature(map, featureKind, idSuffix, name, material, radius > 0 ? 1.2f : 0.75f, points);
    }
}

void addRouteZone(MapModel &map, int index, int centerY)
{
    MapRegionModel route;
    route.name = "Tactical Route " + std::to_string(index + 1);
    route.description = "Verified alternative route between both deployment zones";
    route.x = 2;
    route.y = std::clamp(centerY - 1, 0, std::max(0, map.height - 3));
    route.width = std::max(1, map.width - 4);
    route.height = std::min(3, map.height);
    route.gmOnly = true;
    map.regions.push_back(std::move(route));
}

void connectRouteEnds(MapModel &map, int upperY, int lowerY, MapTileKind terrain, MapWayKind way)
{
    const int leftX = 2;
    const int rightX = map.width - 3;
    paintSegment(map, {leftX, upperY}, {leftX, lowerY}, 0, terrain, way, true);
    paintSegment(map, {rightX, upperY}, {rightX, lowerY}, 0, terrain, way, true);
}

bool tacticalSpawnFootprintIsUsable(const MapModel &map, int x, int y, int width, int height)
{
    if (x < 0 || y < 0 || x + width > map.width || y + height > map.height)
        return false;
    for (int cy = y; cy < y + height; ++cy)
        for (int cx = x; cx < x + width; ++cx)
            if (!cellCanSpawn(mapCellAt(map, cx, cy)))
                return false;
    return true;
}

Point findTacticalSpawnOrigin(const MapModel &map, Point preferredCenter, int width, int height)
{
    Point best{-1, -1};
    int bestScore = std::numeric_limits<int>::max();
    for (int y = 0; y + height <= map.height; ++y)
        for (int x = 0; x + width <= map.width; ++x)
        {
            if (!tacticalSpawnFootprintIsUsable(map, x, y, width, height))
                continue;
            const int centerX = x + width / 2;
            const int centerY = y + height / 2;
            const int score = std::abs(centerX - preferredCenter.x) +
                              std::abs(centerY - preferredCenter.y);
            if (score < bestScore)
            {
                best = {x, y};
                bestScore = score;
            }
        }
    return best;
}

bool addSpawnZone(MapModel &map, const char *name, int x, int y, int width, int height)
{
    if (!tacticalSpawnFootprintIsUsable(map, x, y, width, height))
        return false;
    MapRegionModel zone;
    zone.name = name;
    zone.description = "Encounter starting zone";
    zone.x = x;
    zone.y = y;
    zone.width = width;
    zone.height = height;
    zone.gmOnly = true;
    map.regions.push_back(zone);
    for (int cy = y; cy < y + height; ++cy)
        for (int cx = x; cx < x + width; ++cx)
            if (MapCellModel *cell = mapCellAt(map, cx, cy); cell)
            {
                cell->movementCost = 1;
                cell->movementBlockedMask = 0;
            }
    return true;
}

void addObjectiveZone(MapModel &map, int index, int x, int y)
{
    MapRegionModel objective;
    objective.name = "Objective " + std::to_string(index + 1);
    objective.description = "Visible encounter objective";
    objective.x = std::clamp(x - 1, 0, std::max(0, map.width - 3));
    objective.y = std::clamp(y - 1, 0, std::max(0, map.height - 3));
    objective.width = std::min(3, map.width);
    objective.height = std::min(3, map.height);
    objective.gmOnly = false;
    map.regions.push_back(std::move(objective));
}

void addChokepoint(MapModel &map, int x, int y, const std::string &description)
{
    MapRegionModel choke;
    choke.name = "Chokepoint";
    choke.description = description;
    choke.x = std::clamp(x - 1, 0, std::max(0, map.width - 3));
    choke.y = std::clamp(y - 1, 0, std::max(0, map.height - 3));
    choke.width = std::min(3, map.width);
    choke.height = std::min(3, map.height);
    choke.gmOnly = false;
    map.regions.push_back(std::move(choke));
}

void composeOpenGround(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Empty);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    const int bend = (tacticalHash(seed, 1, 1) & 1u) ? 1 : -1;
    const int entryY = std::clamp(map.height / 2 + bend * 2, 2, map.height - 3);
    const int exitY = std::clamp(map.height / 2 - bend * 2, 2, map.height - 3);
    const int forkX = std::max(3, map.width / 5 + static_cast<int>(seed % 3u));
    const int joinX = std::min(map.width - 4, map.width * 4 / 5 - static_cast<int>((seed >> 4u) % 3u));
    // Routes fork around terrain instead of outlining a rectangular circuit.
    carveRoute(map, "route_upper", "Tactical Route 1", {{1, entryY}, {forkX, entryY},
                   {map.width / 2, upper - 1}, {joinX, exitY}, {map.width - 2, exitY}},
               0, MapTileKind::Dirt, MapWayKind::Trail);
    carveRoute(map, "route_lower", "Tactical Route 2", {{forkX, entryY},
                   {map.width / 3, lower + bend}, {map.width * 2 / 3, lower - bend}, {joinX, exitY}},
               0, MapTileKind::Dirt, MapWayKind::Trail);
    paintEllipse(map, map.width / 2, 1, std::max(3, map.width / 6), std::max(2, map.height / 5), MapTileKind::Hill, seed, true);
    paintEllipse(map, map.width / 2, map.height - 2, std::max(3, map.width / 7), std::max(2, map.height / 5), MapTileKind::Hill, seed ^ 0x41u, true);
    setCell(map, map.width / 2, map.height / 2, MapTileKind::Hazard);
    addChokepoint(map, map.width / 2, map.height / 2, "Exposed ground between two low ridges");
    addRouteZone(map, 0, upper);
    addRouteZone(map, 1, lower);
}

void composeForestRoad(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Empty);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    paintEllipse(map, map.width / 4, 1, std::max(4, map.width / 4), std::max(3, map.height / 3), MapTileKind::Forest, seed, true);
    paintEllipse(map, map.width * 3 / 4, map.height - 2, std::max(4, map.width / 4), std::max(3, map.height / 3), MapTileKind::Forest, seed ^ 0x91u, true);
    paintEllipse(map, map.width - 2, 2, std::max(3, map.width / 6), std::max(2, map.height / 4), MapTileKind::Forest, seed ^ 0x19u, true);
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (MapCellModel *cell = mapCellAt(map, x, y); cell && cell->kind == MapTileKind::Forest)
                cell->blocksLineOfSight = tacticalHash(seed, x, y, 0x76u) % 5u == 0u;
    const int sweep = 1 + static_cast<int>((seed >> 3u) % 3u);
    const int forkX = std::max(3, map.width / 5);
    const int joinX = std::min(map.width - 4, map.width * 4 / 5);
    carveRoute(map, "forest_road", "Tactical Route 1", {{1, lower}, {forkX, lower - 1},
                   {map.width / 2, lower - sweep}, {joinX, lower}, {map.width - 2, lower + 1}},
               1, MapTileKind::Dirt, MapWayKind::DirtRoad);
    carveRoute(map, "forest_trail", "Tactical Route 2", {{forkX, lower - 1},
                   {map.width / 3, upper}, {map.width / 2, upper - 1},
                   {map.width * 2 / 3, upper + 1}, {joinX, lower}},
               0, MapTileKind::Dirt, MapWayKind::Trail);
    const int chokeX = map.width / 2 + static_cast<int>(seed % 3u) - 1;
    setCell(map, chokeX, lower - 2, MapTileKind::Hazard);
    addChokepoint(map, chokeX, lower, "A fallen trunk narrows the main road");
    addRouteZone(map, 0, lower);
    addRouteZone(map, 1, upper);
}

void composeForestRoadV7(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Empty);
    const int centerX = map.width / 2 + static_cast<int>(seed % 3u) - 1;
    const int roadY = std::clamp(map.height / 2 + 1, 4, map.height - 4);
    const int halfBypass = std::clamp(map.width / 7, 2, 5);
    const int forkX = centerX - halfBypass;
    const int joinX = centerX + halfBypass;
    const int bypassY = roadY - 4;
    // Groves surround one readable through road. The shorter, single-width
    // trail skirts an actual dense stand beside a fallen trunk; it no longer
    // draws a second map-spanning road around an empty field.
    paintEllipse(map, map.width / 4, 2, std::max(3, map.width / 4), std::max(2, map.height / 3), MapTileKind::Forest, seed, true);
    paintEllipse(map, map.width * 3 / 4, map.height - 2, std::max(3, map.width / 3), std::max(2, map.height / 4), MapTileKind::Forest, seed ^ 0x91u, true);
    paintEllipse(map, map.width - 2, 2, std::max(3, map.width / 5), std::max(2, map.height / 4), MapTileKind::Forest, seed ^ 0x19u, true);
    paintEllipse(map, centerX, roadY - 2, std::max(1, halfBypass - 1), 2, MapTileKind::Forest, seed ^ 0x731u, true);
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (MapCellModel *cell = mapCellAt(map, x, y); cell && cell->kind == MapTileKind::Forest)
                cell->blocksLineOfSight = tacticalHash(seed, x, y, 0x76u) % 5u == 0u ||
                    (std::abs(x - centerX) < halfBypass && y >= roadY - 3 && y < roadY);
    const bool v9=map.generation.recipe.generatorVersion>=9;
    const std::vector<Point> throughRoad = v9 ? std::vector<Point>{{0,roadY+3},
        {std::max(1,forkX-3),roadY+2},{forkX,roadY},{centerX,roadY},{joinX,roadY},
        {std::min(map.width-2,joinX+3),roadY-2},{map.width-1,roadY-4}} :
        std::vector<Point>{{1,roadY+1},{forkX,roadY},{centerX,roadY},{joinX,roadY},{map.width-2,roadY-1}};
    carveRoute(map, "forest_road", "Tactical Route 1", throughRoad,
               1, MapTileKind::Dirt, MapWayKind::DirtRoad);
    carveRoute(map, "forest_trail", "Tactical Route 2", {{forkX, roadY}, {forkX + 1, roadY - 3},
                   {centerX - 1, bypassY}, {centerX + 1, bypassY + 1}, {joinX, roadY - 1}, {joinX, roadY}},
               0, MapTileKind::Dirt, MapWayKind::Trail);
    setCell(map, centerX, roadY - 1, MapTileKind::Hazard);
    for (int x = centerX - 1; x <= centerX + 1; ++x)
    {
        setCell(map, x, roadY - 2, MapTileKind::Forest);
        if (auto *thicket = mapCellAt(map, x, roadY - 2))
            thicket->blocksLineOfSight = true;
    }
    addChokepoint(map, centerX, roadY - 1, "A fallen trunk and dense thicket shelter the short forest bypass");
    addRouteZone(map, 0, roadY);
    addRouteZone(map, 1, std::max(2, roadY - 3));
}

int tacticalCrossingUpperY(const MapModel &map)
{
    if(map.generation.recipe.recipeVersion>=6) {
        const int limit=std::max(0,map.height/2-2);
        return std::clamp(map.height/2-std::max(3,map.height/(5+int(map.generation.recipe.seed%4u))),std::min(3,limit),limit);
    }
    return std::max(3,map.generation.recipe.generatorVersion>=10 ?
        map.height/2-std::max(3,map.height/8) : map.height/3);
}
int tacticalCrossingLowerY(const MapModel &map)
{
    if(map.generation.recipe.recipeVersion>=6) {
        const int start=std::min(std::max(0,map.height-1),map.height/2+2);
        return std::clamp(map.height/2+std::max(3,map.height/(4+int((map.generation.recipe.seed>>4)%5u))),start,std::max(start,map.height-4));
    }
    return std::min(map.height-4,map.generation.recipe.generatorVersion>=10 ?
        map.height/2+std::max(3,map.height/8) : map.height*2/3);
}
}
