#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

void mapGenSetRiverCell(MapModel &map, int x, int y)
{
    if (x < 0 || y < 0 || x >= map.width || y >= map.height)
        return;
    mapGenSetCell(map, x, y, MapTileKind::Water);
    mapGenSetWay(map, x, y, MapWayKind::River);
}

bool mapGenRiverInfluenceCell(const MapCellModel *cell)
{
    return cell && (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Swamp ||
                    mapWayKindIsWaterLike(cell->wayKind));
}

void mapGenFillDiagonalRiverCornerGaps(MapModel &map)
{
    std::vector<MapCellCoordinate> cornerCells;
    const MapCellCoordinate diagonals[] = {{1, 1}, {-1, 1}};

    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind != MapWayKind::River)
                continue;

            for (const MapCellCoordinate &diagonal : diagonals)
            {
                const MapCellModel *diagonalCell = mapCellAt(map, x + diagonal.x, y + diagonal.y);
                if (!diagonalCell || diagonalCell->wayKind != MapWayKind::River)
                    continue;

                const bool sideXInfluenced = mapGenRiverInfluenceCell(mapCellAt(map, x + diagonal.x, y));
                const bool sideYInfluenced = mapGenRiverInfluenceCell(mapCellAt(map, x, y + diagonal.y));
                if (!sideXInfluenced && !sideYInfluenced)
                {
                    cornerCells.push_back({x + diagonal.x, y});
                    cornerCells.push_back({x, y + diagonal.y});
                }
            }
        }
    }

    for (const MapCellCoordinate &corner : cornerCells)
        mapGenSetCell(map, corner.x, corner.y, MapTileKind::Water);
}

void mapGenBuildWater(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    const int water = mapGenClampPercent(options.waterLevel);
    if (water == 0)
        return;

    if (water >= 14)
    {
        int riverX = mapGenRandomBetween(rng, map.width / 5, std::max(map.width / 5, map.width * 4 / 5));
        mapGenSetRiverCell(map, riverX, 0);
        for (int y = 1; y < map.height; ++y)
        {
            const int previousX = riverX;
            riverX = std::clamp(previousX + mapGenRandomBetween(rng, -1, 1), 0, map.width - 1);
            if (riverX != previousX)
            {
                mapGenSetCell(map, previousX, y, MapTileKind::Water);
                mapGenSetCell(map, riverX, y - 1, MapTileKind::Water);
            }
            mapGenSetRiverCell(map, riverX, y);
            if (water > 35 && mapGenChance(rng, 45))
                mapGenSetCell(map, std::min(map.width - 1, riverX + 1), y, MapTileKind::Water);
            if (water > 55 && mapGenChance(rng, 32))
                mapGenSetCell(map, std::max(0, riverX - 1), y, MapTileKind::Water);
        }
        mapGenFillDiagonalRiverCornerGaps(map);
    }

    const int lakeCount = std::max(0, water / 30);
    for (int i = 0; i < lakeCount; ++i)
        mapGenPaintDisc(map, mapGenRandomBetween(rng, 2, map.width - 3), mapGenRandomBetween(rng, 2, map.height - 3),
                        mapGenRandomBetween(rng, 2, std::max(2, water / 16)), MapTileKind::Water, rng, 18);

    mapGenAddTerrainBorder(map, MapTileKind::Water, MapTileKind::Dirt, MapTileKind::Empty, 1,
                           std::min(82, 34 + water / 2), rng);
    mapGenAddTerrainBorder(map, MapTileKind::Water, MapTileKind::Swamp, MapTileKind::Forest, 2, std::min(36, water / 3),
                           rng);
}

