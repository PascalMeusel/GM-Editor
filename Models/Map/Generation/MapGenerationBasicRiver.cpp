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

std::vector<MapCellCoordinate> mapGenV2BuildRiver(MapModel &map, const MapGenerationOptions &options,
                                                  const std::vector<float> &elevation, std::mt19937 &rng)
{
    int sourceX = map.width / 2;
    int sourceY = 0;
    float bestElevation = -1.0f;
    for (int y = 0; y < std::max(2, map.height / 4); ++y)
    {
        for (int x = 2; x < map.width - 2; ++x)
        {
            const float value = elevation[static_cast<std::size_t>(y * map.width + x)] +
                                mapGenV2Hash01(options.seed + 97u, x, y) * 0.12f;
            if (value > bestElevation)
            {
                bestElevation = value;
                sourceX = x;
                sourceY = y;
            }
        }
    }
    std::vector<MapCellCoordinate> path;
    MapCellCoordinate current{sourceX, sourceY};
    int previousStepX = 0;
    path.push_back(current);
    // A river may meander, but it must never walk back uphill and knot around
    // a local minimum. Each step advances one row toward its outlet while the
    // elevation field, directional inertia and seeded variation choose the
    // neighboring valley cell.
    while (current.y < map.height - 1)
    {
        MapCellCoordinate best{current.x, current.y + 1};
        float bestCost = std::numeric_limits<float>::max();
        for (int stepX = -1; stepX <= 1; ++stepX)
        {
            const int x = current.x + stepX;
            const int y = current.y + 1;
            if (!isMapCellInside(map, x, y))
                continue;
            const int index = mapCellIndex(map, x, y);
            const float meanderNoise =
                mapGenV2ValueNoise(options.seed + 887u, static_cast<float>(y), 0.0f, std::max(5, map.height / 4));
            const float meanderTarget =
                static_cast<float>(sourceX) + (meanderNoise - 0.5f) * static_cast<float>(map.width) * 0.42f;
            const float edgePenalty = (x < 2 || x >= map.width - 2) ? 0.18f : 0.0f;
            const float turnPenalty = std::abs(stepX - previousStepX) * 0.018f;
            const float valleyCost = elevation[static_cast<std::size_t>(index)] * 0.72f;
            const float meanderCost = std::abs(static_cast<float>(x) - meanderTarget) * 0.024f;
            const float seededVariation = mapGenV2Hash01(options.seed + 313u, x, y) * 0.055f;
            const float cost = valleyCost + meanderCost + edgePenalty + turnPenalty + seededVariation;
            if (cost < bestCost)
            {
                bestCost = cost;
                best = {x, y};
            }
        }
        previousStepX = best.x - current.x;
        current = best;
        path.push_back(current);
    }
    for (const MapCellCoordinate &coordinate : path)
        mapGenSetRiverCell(map, coordinate.x, coordinate.y);
    mapGenFillDiagonalRiverCornerGaps(map);
    mapGenV2AddPathFeature(map, MapGeneratedFeatureKind::River, mapGenV2FeatureId(options.seed, "river", 0),
                           "Main river", "fantasy_neutral.water.river", options.waterLevel > 48 ? 1.5f : 1.0f, path);
    return path;
}

