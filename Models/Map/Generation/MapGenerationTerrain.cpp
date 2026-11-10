#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"

int mapGenClampPercent(int value)
{
    return std::clamp(value, 0, 100);
}

int mapGenRandomBetween(std::mt19937 &rng, int minimum, int maximum)
{
    if (maximum < minimum)
        return minimum;
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(rng);
}

bool mapGenChance(std::mt19937 &rng, int percent)
{
    return mapGenRandomBetween(rng, 1, 100) <= mapGenClampPercent(percent);
}

bool mapGenWayKindIsRoadLike(MapWayKind kind)
{
    return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad || kind == MapWayKind::StoneRoad;
}

bool mapGenTerrainSupportsWay(MapTileKind terrainKind, MapWayKind wayKind)
{
    switch (wayKind)
    {
    case MapWayKind::None:
        return true;
    case MapWayKind::Trail:
    case MapWayKind::DirtRoad:
    case MapWayKind::StoneRoad:
        return terrainKind == MapTileKind::Empty || terrainKind == MapTileKind::Dirt ||
               terrainKind == MapTileKind::Stone || terrainKind == MapTileKind::Desert ||
               terrainKind == MapTileKind::Snow || terrainKind == MapTileKind::Ice ||
               terrainKind == MapTileKind::Forest || terrainKind == MapTileKind::Hill ||
               terrainKind == MapTileKind::Swamp || terrainKind == MapTileKind::Water ||
               terrainKind == MapTileKind::Road || terrainKind == MapTileKind::Bridge ||
               terrainKind == MapTileKind::Door || terrainKind == MapTileKind::Settlement ||
               terrainKind == MapTileKind::Landmark;
    case MapWayKind::River:
    case MapWayKind::Canal:
        return terrainKind == MapTileKind::Water;
    case MapWayKind::DungeonCorridor:
        return terrainKind == MapTileKind::Room || terrainKind == MapTileKind::Door ||
               terrainKind == MapTileKind::Stairs || terrainKind == MapTileKind::Landmark;
    case MapWayKind::CaveTunnel:
        return terrainKind == MapTileKind::Cave || terrainKind == MapTileKind::Stone ||
               terrainKind == MapTileKind::Stairs || terrainKind == MapTileKind::Landmark;
    }
    return false;
}

MapTileKind mapGenRoadbedForTerrain(MapTileKind terrainKind)
{
    switch (terrainKind)
    {
    case MapTileKind::Water:
    case MapTileKind::Swamp:
        return MapTileKind::Bridge;
    case MapTileKind::Forest:
        return MapTileKind::Dirt;
    case MapTileKind::Hill:
        return MapTileKind::Dirt;
    case MapTileKind::Mountain:
    case MapTileKind::Chasm:
    case MapTileKind::Cave:
    case MapTileKind::Rubble:
        return MapTileKind::Stone;
    case MapTileKind::Building:
    case MapTileKind::Wall:
    case MapTileKind::Lava:
    case MapTileKind::Pit:
    case MapTileKind::Cover:
    case MapTileKind::Hazard:
        return MapTileKind::Dirt;
    default:
        return terrainKind;
    }
}

MapTileKind mapGenArtContractRoadbedForWay(MapWayKind wayKind, MapTileKind terrainKind)
{
    if (!mapGenWayKindIsRoadLike(wayKind))
        return terrainKind;
    if (terrainKind == MapTileKind::Water || terrainKind == MapTileKind::Swamp || terrainKind == MapTileKind::Bridge)
        return MapTileKind::Bridge;
    if (wayKind == MapWayKind::StoneRoad)
        return terrainKind == MapTileKind::Stone ? MapTileKind::Stone : MapTileKind::Road;
    if (terrainKind == MapTileKind::Empty || terrainKind == MapTileKind::Dirt || terrainKind == MapTileKind::Road)
        return terrainKind;
    return MapTileKind::Dirt;
}

MapWayKind mapGenGeneratedRoadKind(const MapGenerationOptions &options)
{
    if (options.scope == MapScope::Dungeon)
        return MapWayKind::DungeonCorridor;
    if (options.roadNetwork == MapRoadNetworkMode::Grid)
        return MapWayKind::StoneRoad;
    if (options.roadNetwork == MapRoadNetworkMode::Sparse && options.routeDensity <= 45)
        return MapWayKind::Trail;
    if (options.settlementDensity >= 28 || options.routeDensity >= 78)
        return MapWayKind::StoneRoad;
    return MapWayKind::DirtRoad;
}

void mapGenSetCell(MapModel &map, int x, int y, MapTileKind kind, MapSymbolVariantMode symbolMode)
{
    MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell)
        return;
    const MapWayKind previousWayKind = cell->wayKind;
    applyMapCellTerrainDefaults(*cell, kind);
    cell->symbolVariantMode = symbolMode;
    cell->wayKind = mapGenTerrainSupportsWay(kind, previousWayKind) ? previousWayKind : MapWayKind::None;
}

void mapGenClear(MapModel &map, MapTileKind kind)
{
    resizeMapCells(map);
    for (MapCellModel &cell : map.cells)
    {
        applyMapCellTerrainDefaults(cell, kind);
        cell.symbolVariantMode = MapSymbolVariantMode::Auto;
        cell.wayKind = MapWayKind::None;
    }
}

void mapGenPaintDisc(MapModel &map, int centerX, int centerY, int radius, MapTileKind kind, std::mt19937 &rng,
                     int raggedPercent, MapSymbolVariantMode symbolMode)
{
    const int radiusSquared = radius * radius;
    for (int y = centerY - radius; y <= centerY + radius; ++y)
    {
        for (int x = centerX - radius; x <= centerX + radius; ++x)
        {
            const int dx = x - centerX;
            const int dy = y - centerY;
            if (dx * dx + dy * dy > radiusSquared || mapGenChance(rng, raggedPercent))
                continue;

            const MapCellModel *existing = mapCellAt(map, x, y);
            if (kind == MapTileKind::Forest && existing &&
                (existing->kind == MapTileKind::Mountain || existing->kind == MapTileKind::Water ||
                 existing->kind == MapTileKind::Settlement || existing->kind == MapTileKind::Building))
                continue;

            mapGenSetCell(map, x, y, kind, symbolMode);
        }
    }
}

bool mapGenNearKind(const MapModel &map, int x, int y, MapTileKind kind, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const MapCellModel *cell = mapCellAt(map, x + dx, y + dy);
            if (cell && cell->kind == kind)
                return true;
        }
    }
    return false;
}

int mapGenOrthogonalNeighborCount(const MapModel &map, int x, int y, MapTileKind kind)
{
    int count = 0;
    const MapCellCoordinate neighbors[] = {{x, y - 1}, {x + 1, y}, {x, y + 1}, {x - 1, y}};
    for (const MapCellCoordinate &neighbor : neighbors)
    {
        const MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
        if (cell && cell->kind == kind)
            ++count;
    }
    return count;
}

void mapGenAddTerrainBorder(MapModel &map, MapTileKind anchorKind, MapTileKind borderKind, MapTileKind replaceKind,
                            int radius, int percent, std::mt19937 &rng)
{
    std::vector<MapCellCoordinate> cells;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell && cell->kind == replaceKind && mapGenNearKind(map, x, y, anchorKind, radius) &&
                mapGenChance(rng, percent))
                cells.push_back({x, y});
        }
    }

    for (const MapCellCoordinate &cell : cells)
        mapGenSetCell(map, cell.x, cell.y, borderKind);
}

int mapGenStepToward(int value, int target)
{
    if (value < target)
        return 1;
    if (value > target)
        return -1;
    return 0;
}

void mapGenDrawMountainRidge(MapModel &map, int startX, int startY, int endX, int endY,
                             const MapGenerationOptions &options, std::mt19937 &rng)
{
    int x = std::clamp(startX, 1, std::max(1, map.width - 2));
    int y = std::clamp(startY, 1, std::max(1, map.height - 2));
    const int targetX = std::clamp(endX, 1, std::max(1, map.width - 2));
    const int targetY = std::clamp(endY, 1, std::max(1, map.height - 2));
    const int density = mapGenClampPercent(options.mountainDensity);
    const int safetyLimit = std::max(16, (map.width + map.height) * 4);

    for (int step = 0; step < safetyLimit; ++step)
    {
        if (mapGenChance(rng, std::min(96, 54 + density / 2)))
        {
            if (options.mountainPattern == MapMountainPattern::Ranges ||
                options.mountainPattern == MapMountainPattern::Mixed)
            {
                mapGenSetCell(map, x, y, MapTileKind::Mountain, MapSymbolVariantMode::Continuous);
                const int dx = targetX - x;
                const int dy = targetY - y;
                const bool mostlyHorizontal = std::abs(dx) >= std::abs(dy);
                if (mapGenChance(rng, 32 + density / 4))
                {
                    const int side = mapGenChance(rng, 50) ? 1 : -1;
                    const int shoulderX = mostlyHorizontal ? x : x + side;
                    const int shoulderY = mostlyHorizontal ? y + side : y;
                    mapGenSetCell(map, shoulderX, shoulderY, MapTileKind::Mountain, MapSymbolVariantMode::Continuous);
                }
            }
            else
            {
                const int radius = mapGenChance(rng, std::min(12, 3 + density / 14)) ? 2 : 1;
                mapGenPaintDisc(map, x, y, radius, MapTileKind::Mountain, rng, 34, MapSymbolVariantMode::Continuous);
            }
        }
        if (x == targetX && y == targetY)
            break;

        const int dx = targetX - x;
        const int dy = targetY - y;
        if (std::abs(dx) >= std::abs(dy) || mapGenChance(rng, 55))
            x += mapGenStepToward(x, targetX);
        if (dy != 0 && (std::abs(dy) >= std::abs(dx) || mapGenChance(rng, 38)))
            y += mapGenStepToward(y, targetY);
        if (mapGenChance(rng, 22))
            y += mapGenRandomBetween(rng, -1, 1);
        if (mapGenChance(rng, 10))
            x += mapGenRandomBetween(rng, -1, 1);
        x = std::clamp(x, 1, std::max(1, map.width - 2));
        y = std::clamp(y, 1, std::max(1, map.height - 2));
    }
}

bool mapGenNearMountain(const MapModel &map, int x, int y, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const MapCellModel *cell = mapCellAt(map, x + dx, y + dy);
            if (cell && cell->kind == MapTileKind::Mountain)
                return true;
        }
    }
    return false;
}

void mapGenScatterClumps(MapModel &map, MapTileKind kind, int count, int minRadius, int maxRadius, std::mt19937 &rng,
                         int raggedPercent, MapSymbolVariantMode symbolMode)
{
    for (int i = 0; i < count; ++i)
    {
        mapGenPaintDisc(map, mapGenRandomBetween(rng, 1, std::max(1, map.width - 2)),
                        mapGenRandomBetween(rng, 1, std::max(1, map.height - 2)),
                        mapGenRandomBetween(rng, minRadius, maxRadius), kind, rng, raggedPercent, symbolMode);
    }
}

bool mapGenComponentLooksFilled(int size, int minX, int minY, int maxX, int maxY, int fillPercent)
{
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    const int boxArea = std::max(1, width * height);
    return size >= 18 && width >= 5 && height >= 4 && size * 100 >= boxArea * fillPercent;
}

void mapGenBreakUpMountainSlabs(MapModel &map, std::mt19937 &rng)
{
    std::vector<bool> visited(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || start->kind != MapTileKind::Mountain || visited[static_cast<size_t>(startIndex)])
                continue;

            std::vector<MapCellCoordinate> component;
            std::queue<MapCellCoordinate> pending;
            int minX = x;
            int minY = y;
            int maxX = x;
            int maxY = y;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;

            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                component.push_back(current);
                minX = std::min(minX, current.x);
                minY = std::min(minY, current.y);
                maxX = std::max(maxX, current.x);
                maxY = std::max(maxY, current.y);

                const MapCellCoordinate neighbors[] = {{current.x, current.y - 1},
                                                       {current.x + 1, current.y},
                                                       {current.x, current.y + 1},
                                                       {current.x - 1, current.y}};
                for (const MapCellCoordinate &neighbor : neighbors)
                {
                    if (!isMapCellInside(map, neighbor.x, neighbor.y))
                        continue;
                    const int neighborIndex = neighbor.y * map.width + neighbor.x;
                    if (visited[static_cast<size_t>(neighborIndex)])
                        continue;
                    const MapCellModel *neighborCell = mapCellAt(map, neighbor.x, neighbor.y);
                    if (neighborCell && neighborCell->kind == MapTileKind::Mountain)
                    {
                        visited[static_cast<size_t>(neighborIndex)] = true;
                        pending.push(neighbor);
                    }
                }
            }

            if (!mapGenComponentLooksFilled(static_cast<int>(component.size()), minX, minY, maxX, maxY, 64))
                continue;

            std::vector<MapCellCoordinate> carveCandidates;
            for (const MapCellCoordinate &cell : component)
            {
                if (cell.x <= minX || cell.x >= maxX || cell.y <= minY || cell.y >= maxY)
                    continue;
                if (mapGenOrthogonalNeighborCount(map, cell.x, cell.y, MapTileKind::Mountain) >= 3)
                    carveCandidates.push_back(cell);
            }

            std::shuffle(carveCandidates.begin(), carveCandidates.end(), rng);
            const int carveLimit = std::max(2, static_cast<int>(component.size()) / 9);
            for (int i = 0; i < carveLimit && i < static_cast<int>(carveCandidates.size()); ++i)
            {
                const MapCellCoordinate &cell = carveCandidates[static_cast<size_t>(i)];
                mapGenSetCell(map, cell.x, cell.y, mapGenChance(rng, 55) ? MapTileKind::Stone : MapTileKind::Hill);
            }
        }
    }
}
