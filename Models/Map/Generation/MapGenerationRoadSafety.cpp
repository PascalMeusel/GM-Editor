#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
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


int mapGenV2RoadTraversalCost(const MapCellModel &cell)
{
    if (cell.kind == MapTileKind::Building || cell.kind == MapTileKind::Wall || cell.kind == MapTileKind::Lava ||
        cell.kind == MapTileKind::Pit || cell.kind == MapTileKind::Chasm)
        return 100000;
    if (cell.kind == MapTileKind::Water)
        return mapWayKindIsWaterLike(cell.wayKind) ? 24 : 100000;
    if (cell.kind == MapTileKind::Mountain)
        return 18;
    if (cell.kind == MapTileKind::Swamp)
        return 10;
    if (cell.kind == MapTileKind::Forest)
        return 6;
    if (cell.kind == MapTileKind::Hill || cell.kind == MapTileKind::Stone)
        return 5;
    return 2;
}

bool mapGenV6RegionHasCompleteRiverCrossing(const MapModel &map, const std::vector<MapCellCoordinate> &river)
{
    if (map.generation.recipe.generatorVersion < 6 || map.atlasLevel != MapLevel::Region)
        return false;
    for (std::size_t index = 1; index + 1 < river.size(); ++index)
    {
        const auto center = river[index];
        const auto *middle = mapCellAt(map, center.x, center.y);
        if (!middle || middle->kind != MapTileKind::Bridge || !mapGenWayKindIsRoadLike(middle->wayKind))
            continue;
        const float tx = static_cast<float>(river[index + 1].x - river[index - 1].x);
        const float ty = static_cast<float>(river[index + 1].y - river[index - 1].y);
        for (const auto across : {MapCellCoordinate{1, 0}, {0, 1}, {1, 1}, {1, -1}})
        {
            const float divisor =
                std::sqrt((tx * tx + ty * ty) * static_cast<float>(across.x * across.x + across.y * across.y));
            if (divisor < .001f || std::abs(tx * across.x + ty * across.y) / divisor > .72f)
                continue;
            bool bothBanks = true;
            for (const int side : {-1, 1})
            {
                bool dryBank = false;
                for (int distance = 1; distance <= 6; ++distance)
                {
                    const auto *cell =
                        mapCellAt(map, center.x + across.x * side * distance, center.y + across.y * side * distance);
                    if (!cell)
                        break;
                    if (cell->kind == MapTileKind::Bridge && mapGenWayKindIsRoadLike(cell->wayKind))
                        continue;
                    dryBank = cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Mountain &&
                              !mapWayKindIsWaterLike(cell->wayKind) && mapGenV2RoadTraversalCost(*cell) < 100000 &&
                              (mapGenWayKindIsRoadLike(cell->wayKind) || cell->kind == MapTileKind::Settlement);
                    break;
                }
                bothBanks = bothBanks && dryBank;
            }
            if (bothBanks)
                return true;
        }
    }
    return false;
}

bool mapGenV5WorldOpenWaterCell(const MapCellModel *cell)
{
    return cell && cell->kind == MapTileKind::Water && cell->wayKind != MapWayKind::River &&
           cell->wayKind != MapWayKind::Canal;
}

bool mapGenV5WorldDiagonalRiverHelperCell(const MapModel &map, int x, int y)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!mapGenV5WorldOpenWaterCell(cell))
        return false;
    const auto riverLike = [&](int neighborX, int neighborY) {
        const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
        return neighbor && (neighbor->wayKind == MapWayKind::River || neighbor->wayKind == MapWayKind::Canal);
    };
    const bool north = riverLike(x, y - 1);
    const bool east = riverLike(x + 1, y);
    const bool south = riverLike(x, y + 1);
    const bool west = riverLike(x - 1, y);
    // Match the World ground-projection contract exactly: only the
    // orthogonal shoulder between a diagonal pair is projection water.
    return (north && east) || (east && south) || (south && west) || (west && north);
}

bool mapGenV5WorldOpenWaterAt(const MapModel &map, int x, int y)
{
    return mapGenV5WorldOpenWaterCell(mapCellAt(map, x, y)) && !mapGenV5WorldDiagonalRiverHelperCell(map, x, y);
}

int mapGenV5WorldOpenWaterDistance(const MapModel &map, int x, int y, int maximumDistance)
{
    for (int distance = 0; distance <= maximumDistance; ++distance)
        for (int offsetY = -distance; offsetY <= distance; ++offsetY)
            for (int offsetX = -distance; offsetX <= distance; ++offsetX)
            {
                if (std::max(std::abs(offsetX), std::abs(offsetY)) != distance)
                    continue;
                if (mapGenV5WorldOpenWaterAt(map, x + offsetX, y + offsetY))
                    return distance;
            }
    return maximumDistance + 1;
}

bool mapGenV5WorldRoadSegmentIsSafe(const MapModel &map, const MapFeaturePoint &start, const MapFeaturePoint &end)
{
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;
    const int samples = std::max(1, static_cast<int>(std::ceil(std::sqrt(deltaX * deltaX + deltaY * deltaY) * 5.0f)));
    for (int sample = 0; sample <= samples; ++sample)
    {
        const float amount = static_cast<float>(sample) / static_cast<float>(samples);
        const float x = start.x + deltaX * amount;
        const float y = start.y + deltaY * amount;
        const int cellX = std::clamp(static_cast<int>(std::floor(x)), 0, map.width - 1);
        const int cellY = std::clamp(static_cast<int>(std::floor(y)), 0, map.height - 1);
        const MapCellModel *cell = mapCellAt(map, cellX, cellY);
        if (!cell || mapGenV5WorldOpenWaterAt(map, cellX, cellY) || cell->kind == MapTileKind::Mountain ||
            cell->kind == MapTileKind::Wall || cell->kind == MapTileKind::Building || cell->kind == MapTileKind::Lava ||
            cell->kind == MapTileKind::Pit || cell->kind == MapTileKind::Chasm)
            return false;
        // Smoothing must preserve some dry ground between the road's visible
        // stroke and the shoreline, not merely keep its centre on land.
        if (map.generation.recipe.generatorVersion >= 11 && map.atlasLevel == MapLevel::World &&
            cell->kind != MapTileKind::Bridge)
        {
            bool nearTown = false;
            for (const auto &town : map.generation.graph.features)
                if (town.kind == MapGeneratedFeatureKind::Settlement && !town.points.empty() &&
                    std::hypot(x - town.points.front().x, y - town.points.front().y) < 1.5f)
                {
                    nearTown = true;
                    break;
                }
            if (!nearTown)
                for (int yy = cellY - 1; yy <= cellY + 1; ++yy)
                    for (int xx = cellX - 1; xx <= cellX + 1; ++xx)
                        if (mapGenV5WorldOpenWaterAt(map, xx, yy))
                        {
                            const float dx =
                                std::max({static_cast<float>(xx) - x, 0.f, x - static_cast<float>(xx + 1)});
                            const float dy =
                                std::max({static_cast<float>(yy) - y, 0.f, y - static_cast<float>(yy + 1)});
                            if (dx * dx + dy * dy < .75f * .75f)
                                return false;
                        }
        }
    }
    return true;
}

bool mapGenV5WorldRoadSegmentIsDry(const MapModel &map, const MapFeaturePoint &start, const MapFeaturePoint &end)
{
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;
    const int samples = std::max(1, static_cast<int>(std::ceil(std::sqrt(deltaX * deltaX + deltaY * deltaY) * 5.0f)));
    int previousX = -1;
    int previousY = -1;
    for (int sample = 0; sample <= samples; ++sample)
    {
        const float amount = static_cast<float>(sample) / static_cast<float>(samples);
        const int x = std::clamp(static_cast<int>(std::floor(start.x + deltaX * amount)), 0, map.width - 1);
        const int y = std::clamp(static_cast<int>(std::floor(start.y + deltaY * amount)), 0, map.height - 1);
        const MapCellModel *cell = mapCellAt(map, x, y);
        if (!cell || cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Bridge)
            return false;
        if (previousX >= 0 && x != previousX && y != previousY)
        {
            const MapCellModel *sideX = mapCellAt(map, x, previousY);
            const MapCellModel *sideY = mapCellAt(map, previousX, y);
            const auto wet = [](const MapCellModel *side) {
                return side && (side->kind == MapTileKind::Water || side->kind == MapTileKind::Bridge);
            };
            if (wet(sideX) || wet(sideY))
                return false;
        }
        previousX = x;
        previousY = y;
    }
    return true;
}

int mapGenV7OverviewTerrainFriction(const MapCellModel &cell)
{
    const int cover = cell.kind == MapTileKind::Swamp    ? 100
                      : cell.kind == MapTileKind::Forest ? 44
                      : cell.kind == MapTileKind::Hill   ? 30
                                                         : 0;
    // Broad relief matters even where the biome remains grassland. Following
    // a low saddle should beat crossing the crown of an otherwise empty hill.
    return cover + std::max(0, cell.elevation - 4) * 6;
}

float mapGenV7RoadSegmentEffort(const MapModel &map, MapFeaturePoint start, MapFeaturePoint end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    const int samples = std::max(1, static_cast<int>(std::ceil(length * 4.0f)));
    float effort = 0.0f;
    const MapCellModel *previousCell = mapCellAt(map, static_cast<int>(start.x), static_cast<int>(start.y));
    for (int index = 1; index <= samples; ++index)
    {
        const float t = static_cast<float>(index) / samples;
        const MapCellModel *cell =
            mapCellAt(map, static_cast<int>(start.x + dx * t), static_cast<int>(start.y + dy * t));
        if (!cell)
            return std::numeric_limits<float>::max();
        const int grade = previousCell ? std::abs(cell->elevation - previousCell->elevation) : 0;
        effort += length / samples * (1.0f + mapGenV7OverviewTerrainFriction(*cell) * 0.01f) + grade * grade * 0.15f;
        previousCell = cell;
    }
    return effort;
}

bool mapGenV7RoadChordIsSafe(const MapModel &map, MapFeaturePoint start, MapFeaturePoint end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    const float nx = length > 0.001f ? -dy / length * 0.38f : 0.0f;
    const float ny = length > 0.001f ? dx / length * 0.38f : 0.0f;
    // Include the road shoulder, and retain every wet crossing as authored.
    for (const float side : {-1.0f, 0.0f, 1.0f})
    {
        const MapFeaturePoint a{start.x + nx * side, start.y + ny * side};
        const MapFeaturePoint b{end.x + nx * side, end.y + ny * side};
        if (!mapGenV5WorldRoadSegmentIsSafe(map, a, b) || !mapGenV5WorldRoadSegmentIsDry(map, a, b))
            return false;
    }
    return true;
}

