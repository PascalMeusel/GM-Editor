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
#include "Models/Map/Generation/MapGenerationSharedRiverBuilder.hpp"

bool MapGenerationSharedRiverBuilder::selectSource()
{
    source = {-1, -1};
    float sourceScore = -std::numeric_limits<float>::max();
    const float minimumSourceDistance = static_cast<float>(std::min(map.width, map.height)) * 0.28f;
    const auto chooseSource = [&](bool requireHighland) {
        for (int y = 1; y < map.height - 1; ++y)
        {
            for (int x = 1; x < map.width - 1; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                const int cellIndex = mapCellIndex(map, x, y);
                if (!cell || cell->kind == MapTileKind::Water || cellIndex < 0 ||
                    coastDistanceField[static_cast<std::size_t>(cellIndex)] < sourceCoastClearance)
                    continue;
                const float sampledElevation = elevation[static_cast<std::size_t>(mapCellIndex(map, x, y))];
                const bool highland = cell->kind == MapTileKind::Mountain || cell->kind == MapTileKind::Hill;
                if (requireHighland && !highland)
                    continue;
                int nearbyMountains = 0;
                int nearbyHighlands = 0;
                const int contextRadius = std::max(2, std::min(4, std::min(map.width, map.height) / 6));
                for (int offsetY = -contextRadius; offsetY <= contextRadius; ++offsetY)
                    for (int offsetX = -contextRadius; offsetX <= contextRadius; ++offsetX)
                    {
                        const MapCellModel *nearby = mapCellAt(map, x + offsetX, y + offsetY);
                        if (!nearby)
                            continue;
                        nearbyMountains += nearby->kind == MapTileKind::Mountain ? 1 : 0;
                        nearbyHighlands +=
                            nearby->kind == MapTileKind::Mountain || nearby->kind == MapTileKind::Hill ? 1 : 0;
                    }
                if (requireHighland && nearbyMountains < 2 && nearbyHighlands < 4)
                    continue;
                const float deltaX = static_cast<float>(x - waypoint.x);
                const float deltaY = static_cast<float>(y - waypoint.y);
                const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
                if (distance < minimumSourceDistance)
                    continue;
                const float inlandScore = static_cast<float>(coastDistanceField[static_cast<std::size_t>(cellIndex)]) /
                                          std::max(1.0f, static_cast<float>(deepestLandCell));
                const float score = sampledElevation * 2.10f + inlandScore * 0.34f +
                                    static_cast<float>(nearbyMountains) * 0.025f +
                                    static_cast<float>(nearbyHighlands) * 0.006f +
                                    distance / std::max(1.0f, static_cast<float>(map.width + map.height)) * 0.28f +
                                    mapGenV2Hash01(options.seed + 3373u, x, y) * 0.10f;
                if (score > sourceScore)
                {
                    sourceScore = score;
                    source = {x, y};
                }
            }
        }
    };
    chooseSource(true);
    if (source.x < 0)
        chooseSource(false);
    if (source.x < 0)
        return false;
    return true;
}

bool MapGenerationSharedRiverBuilder::selectOutlet()
{
    const float incomingX = static_cast<float>(waypoint.x - source.x);
    const float incomingY = static_cast<float>(waypoint.y - source.y);
    const float incomingLength = std::max(0.001f, std::sqrt(incomingX * incomingX + incomingY * incomingY));
    outlet = {-1, -1};
    float outletScore = -std::numeric_limits<float>::max();
    constexpr MapCellCoordinate cardinal[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!mapGenV5WorldOpenWaterAt(map, x, y))
                continue;
            bool shoreline = false;
            for (const MapCellCoordinate offset : cardinal)
            {
                const MapCellModel *neighbor = mapCellAt(map, x + offset.x, y + offset.y);
                shoreline = shoreline || (neighbor && neighbor->kind != MapTileKind::Water);
            }
            if (!shoreline)
                continue;
            const float fromWaypointX = static_cast<float>(x - waypoint.x);
            const float fromWaypointY = static_cast<float>(y - waypoint.y);
            const float fromWaypointLength =
                std::max(0.001f, std::sqrt(fromWaypointX * fromWaypointX + fromWaypointY * fromWaypointY));
            const float continuation =
                (incomingX * fromWaypointX + incomingY * fromWaypointY) / (incomingLength * fromWaypointLength);
            const float sourceDistance =
                std::sqrt(static_cast<float>((x - source.x) * (x - source.x) + (y - source.y) * (y - source.y)));
            if (sourceDistance < static_cast<float>(std::min(map.width, map.height)) * 0.55f)
                continue;
            const float score = sourceDistance +
                                continuation * static_cast<float>(std::min(map.width, map.height)) * 0.72f +
                                mapGenV2Hash01(options.seed + 3433u, x, y) * 0.35f;
            if (score > outletScore)
            {
                outletScore = score;
                outlet = {x, y};
            }
        }
    }
    if (outlet.x < 0)
        return false;
    return true;
}
