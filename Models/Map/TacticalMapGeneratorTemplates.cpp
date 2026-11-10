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
bool addBuildingBlock(MapModel &map, int x, int y, int width, int height)
{
    if(map.generation.recipe.generatorVersion >= 12) {
        if(x<1 || y<1 || x+width>=map.width || y+height>=map.height)return false;
        // Distinct roofs need a real alley between footprints. Touching corner
        // blocks otherwise become ambiguous merged buildings in the plan.
        for(int cy=y-1;cy<=y+height;++cy)for(int cx=x-1;cx<=x+width;++cx)
            if(const auto *cell=mapCellAt(map,cx,cy);cell && cell->kind==MapTileKind::Building)return false;
    }
    if(map.generation.recipe.generatorVersion >= 10)
        for(int cy=y;cy<y+height;++cy) for(int cx=x;cx<x+width;++cx) {
            const auto *cell=mapCellAt(map,cx,cy);
            if(!cell || cell->wayKind != MapWayKind::None) return false;
        }
    for (int cy = y; cy < y + height; ++cy)
        for (int cx = x; cx < x + width; ++cx)
            if (MapCellModel *cell = mapCellAt(map, cx, cy); cell && cell->wayKind == MapWayKind::None)
                setCell(map, cx, cy, MapTileKind::Building);
    return true;
}

void composeVillageStreet(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Empty);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    carveRoute(map, "village_main", "Tactical Route 1", {{1, map.height / 2}, {map.width / 3, map.height / 2 - 1},
               {map.width * 2 / 3, map.height / 2 + 1}, {map.width - 2, map.height / 2}}, 1, MapTileKind::Dirt, MapWayKind::DirtRoad);
    carveRoute(map, "village_lane", "Tactical Route 2", {{1, upper}, {map.width / 4, upper - 1}, {map.width / 2, lower},
               {map.width * 3 / 4, lower + 1}, {map.width - 2, lower}}, 0, MapTileKind::Dirt, MapWayKind::Trail);
    connectRouteEnds(map, upper, lower, MapTileKind::Dirt, MapWayKind::Trail);
    const int houseWidth = map.width >= 32 ? 3 : 2;
    for (int x = 5; x + houseWidth < map.width - 4; x += houseWidth + 3)
    {
        const int jitter = static_cast<int>(tacticalHash(seed, x, 0, 0x611u) % 2u);
        addBuildingBlock(map, x, 1 + jitter, houseWidth, 2);
        if ((x / (houseWidth + 3)) % 2 == 0)
            addBuildingBlock(map, x + 1, map.height - 3 - jitter, houseWidth, 2);
    }
    setCell(map, map.width / 2, map.height / 2, MapTileKind::Hazard);
    addChokepoint(map, map.width / 2, map.height / 2, "A market cart partially blocks the village street");
    addRouteZone(map, 0, map.height / 2);
    addRouteZone(map, 1, lower);
}

void composeCitySquare(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Stone);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    const int squareLeft = map.width / 3;
    const int squareRight = map.width * 2 / 3;
    const int squareTop = map.height / 4;
    const int squareBottom = map.height * 3 / 4;
    paintRect(map, squareLeft, squareTop, squareRight - squareLeft + 1, squareBottom - squareTop + 1, MapTileKind::Stone);
    carveRoute(map, "square_north", "Tactical Route 1", {{1, upper}, {squareLeft, upper}, {squareRight, upper}, {map.width - 2, upper}},
               1, MapTileKind::Stone, MapWayKind::StoneRoad);
    carveRoute(map, "square_south", "Tactical Route 2", {{1, lower}, {squareLeft, lower}, {squareRight, lower}, {map.width - 2, lower}},
               1, MapTileKind::Stone, MapWayKind::StoneRoad);
    carveRoute(map, "square_cross", "Square Cross Street", {{map.width / 2, 1}, {map.width / 2, map.height - 2}},
               0, MapTileKind::Stone, MapWayKind::StoneRoad);
    connectRouteEnds(map, upper, lower, MapTileKind::Stone, MapWayKind::StoneRoad);
    const int blockWidth = map.width >= 32 ? 3 : 2;
    for (int x = 4; x + blockWidth < map.width - 3; x += blockWidth + 3)
    {
        if (x >= squareLeft - 1 && x <= squareRight)
            continue;
        addBuildingBlock(map, x, 1, blockWidth, 2);
        addBuildingBlock(map, x, map.height - 3, blockWidth, 2);
    }
    addBuildingBlock(map, squareLeft - 3, squareTop, 2, 3);
    addBuildingBlock(map, squareRight + 2, squareBottom - 2, 2, 3);
    const int hazardY = (seed & 1u) ? upper : lower;
    setCell(map, map.width / 2 - 2, hazardY, MapTileKind::Hazard);
    setCell(map, map.width / 2 + 2, hazardY, MapTileKind::Cover);
    addChokepoint(map, map.width / 2, hazardY, "Barricades narrow one side of the public square");
    addRouteZone(map, 0, upper);
    addRouteZone(map, 1, lower);
}

void composeCave(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Wall);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    paintEllipse(map, map.width / 5, map.height / 2, std::max(3, map.width / 7), std::max(3, map.height / 4), MapTileKind::Cave, seed, true);
    paintEllipse(map, map.width / 2, map.height / 2, std::max(3, map.width / 8), std::max(3, map.height / 3), MapTileKind::Cave, seed ^ 0x71u, true);
    paintEllipse(map, map.width * 4 / 5, map.height / 2, std::max(3, map.width / 7), std::max(3, map.height / 4), MapTileKind::Cave, seed ^ 0x17u, true);
    carveRoute(map, "cave_upper", "Tactical Route 1", {{1, upper}, {map.width / 3, upper - 1}, {map.width / 2, map.height / 2 - 1}, {map.width * 2 / 3, upper + 1}, {map.width - 2, upper}},
               1, MapTileKind::Cave, MapWayKind::CaveTunnel, false, false);
    carveRoute(map, "cave_lower", "Tactical Route 2", {{1, lower}, {map.width / 3, lower + 1}, {map.width / 2, map.height / 2 + 1}, {map.width * 2 / 3, lower - 1}, {map.width - 2, lower}},
               1, MapTileKind::Cave, MapWayKind::CaveTunnel, false, false);
    connectRouteEnds(map, upper, lower, MapTileKind::Cave, MapWayKind::CaveTunnel);
    setCell(map, map.width / 2, map.height / 2, MapTileKind::Hazard);
    addChokepoint(map, map.width / 2, map.height / 2, "A narrow limestone throat connects the caverns");
    addRouteZone(map, 0, upper);
    addRouteZone(map, 1, lower);
}

void composeStoneDungeon(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Wall);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    const int roomWidth = std::max(4, map.width / 5);
    const int roomHeight = std::max(5, map.height / 2);
    paintRect(map, 1, map.height / 2 - roomHeight / 2, roomWidth, roomHeight, MapTileKind::Room);
    paintRect(map, map.width / 2 - roomWidth / 2, 2, roomWidth, map.height - 4, MapTileKind::Room);
    paintRect(map, map.width - roomWidth - 1, map.height / 2 - roomHeight / 2, roomWidth, roomHeight, MapTileKind::Room);
    // Carve corridors one cell to either side of the centerline. Radius-zero
    // diagonal interpolation can leave cells touching only at their corners;
    // that looks connected but is not a legal square-grid movement route.
    // The separated center bends keep the two approaches tactically distinct
    // while the three-cell passages remain usable by larger creatures.
    carveRoute(map, "dungeon_upper", "Tactical Route 1", {{1, upper}, {map.width / 3, upper}, {map.width / 2, map.height / 2 - 2}, {map.width * 2 / 3, upper}, {map.width - 2, upper}},
               1, MapTileKind::Room, MapWayKind::DungeonCorridor, false, false);
    carveRoute(map, "dungeon_lower", "Tactical Route 2", {{1, lower}, {map.width / 3, lower}, {map.width / 2, map.height / 2 + 2}, {map.width * 2 / 3, lower}, {map.width - 2, lower}},
               1, MapTileKind::Room, MapWayKind::DungeonCorridor, false, false);
    connectRouteEnds(map, upper, lower, MapTileKind::Room, MapWayKind::DungeonCorridor);
    const int doorX = map.width / 2 + ((seed & 1u) ? 1 : -1);
    setCell(map, doorX, upper, MapTileKind::Door, MapWayKind::DungeonCorridor);
    setCell(map, doorX, lower, MapTileKind::Door, MapWayKind::DungeonCorridor);
    setCell(map, map.width / 2, map.height / 2, MapTileKind::Hazard);
    addChokepoint(map, doorX, upper, "A defensible doorway controls the northern corridor");
    addRouteZone(map, 0, upper);
    addRouteZone(map, 1, lower);
}

#include "Models/Map/TacticalMapCompositionV10.inl"

void composeTemplate(MapModel &map, const TacticalMapRecipe &recipe, std::uint32_t seed)
{
    map.generation.graph.features.clear();
    map.regions.clear();
    map.routes.clear();
    switch (recipe.mapTemplate)
    {
    case TacticalMapTemplate::ForestRoad:
        if (recipe.generatorVersion >= 10) composeForestRoadV10(map, seed);
        else if (recipe.generatorVersion >= 7) composeForestRoadV7(map, seed);
        else composeForestRoad(map, seed);
        break;
    case TacticalMapTemplate::RiverCrossing: composeRiverCrossing(map, seed); break;
    case TacticalMapTemplate::Ruin:
        if (recipe.generatorVersion >= 10) composeRuinV10(map,seed);
        else if (recipe.generatorVersion >= 8) composeRuinV8(map,seed);
        else composeRuin(map,seed);
        break;
    case TacticalMapTemplate::VillageStreet:
        if(recipe.generatorVersion>=10) composeVillageStreetV10(map,seed); else composeVillageStreet(map, seed); break;
    case TacticalMapTemplate::CitySquare:
        if(recipe.generatorVersion>=10) composeCitySquareV10(map,seed); else composeCitySquare(map, seed); break;
    case TacticalMapTemplate::Cave: if(recipe.generatorVersion>=10) composeCaveV10(map,seed); else composeCave(map, seed); break;
    case TacticalMapTemplate::StoneDungeon: if(recipe.generatorVersion>=10) composeStoneDungeonV10(map,seed); else composeStoneDungeon(map, seed); break;
    case TacticalMapTemplate::OpenGround:
    default:
        if(recipe.generatorVersion>=12) composeOpenGroundV12(map,seed);
        else if(recipe.generatorVersion>=10) composeOpenGroundV10(map,seed); else composeOpenGround(map, seed); break;
    }
    if(recipe.generatorVersion>=12 && (recipe.mapTemplate==TacticalMapTemplate::VillageStreet || recipe.mapTemplate==TacticalMapTemplate::CitySquare))
        composeStreetFurnitureV12(map,seed);
    if(recipe.generatorVersion>=12)refineTacticalPathsV12(map);
}
}
