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

MapGenerationSharedRiverBuilder::MapGenerationSharedRiverBuilder(
    MapModel &map, const MapGenerationOptions &options, const std::vector<float> &elevation)
    : map(map), options(options), elevation(elevation), maximumRadius(std::max(map.width, map.height))
{
}

std::vector<MapCellCoordinate> MapGenerationSharedRiverBuilder::build()
{
    if (!selectWaypoint())
        return {};
    measureCoastDistance();
    if (!selectSource() || !selectOutlet() || !buildMainRiver())
        return {};
    addTributaries();
    return path;
}

bool MapGenerationSharedRiverBuilder::usableWaypoint(const MapCellModel *cell) const
{
    return cell && cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Mountain &&
           cell->kind != MapTileKind::Lava && cell->kind != MapTileKind::Pit && cell->kind != MapTileKind::Chasm;
}

bool MapGenerationSharedRiverBuilder::selectWaypoint()
{
    // A note that independently mentions a capital and a river does not say
    // that the capital lies on that river. Keep the drainage waypoint near the
    // continental center; explicit spatial relations can constrain either
    // feature once that relation vocabulary is supported.
    MapCellCoordinate requestedCenter{map.width / 2, map.height / 2};

    waypoint = {-1, -1};
    const int maximumRadius = std::max(map.width, map.height);
    for (int radius = 0; radius <= maximumRadius && waypoint.x < 0; ++radius)
    {
        for (int offsetY = -radius; offsetY <= radius && waypoint.x < 0; ++offsetY)
        {
            for (int offsetX = -radius; offsetX <= radius; ++offsetX)
            {
                if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                    continue;
                const MapCellCoordinate candidate{requestedCenter.x + offsetX, requestedCenter.y + offsetY};
                if (usableWaypoint(mapCellAt(map, candidate.x, candidate.y)))
                {
                    waypoint = candidate;
                    break;
                }
            }
        }
    }
    if (waypoint.x < 0)
        return false;
    return true;
}

void MapGenerationSharedRiverBuilder::measureCoastDistance()
{
    // Measure the actual shape of this landmass instead of assuming every map
    // has six cells of coastal hinterland. The old fixed clearance made the
    // ordinary 24x18 editor world physically incapable of hosting a source.
    // River cells are excluded when this logic is also used by validation, so
    // the finished channel itself never counts as coastline.
    coastDistanceField = [&]() {
        const int unreachable = map.width + map.height + 1;
        std::vector<int> distance(static_cast<std::size_t>(map.width * map.height), unreachable);
        std::queue<MapCellCoordinate> pending;
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                if (!mapGenV5WorldOpenWaterAt(map, x, y))
                    continue;
                const int index = mapCellIndex(map, x, y);
                distance[static_cast<std::size_t>(index)] = 0;
                pending.push({x, y});
            }
        constexpr MapCellCoordinate directions[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        while (!pending.empty())
        {
            const MapCellCoordinate current = pending.front();
            pending.pop();
            const int currentIndex = mapCellIndex(map, current.x, current.y);
            for (const MapCellCoordinate direction : directions)
            {
                const int nextX = current.x + direction.x;
                const int nextY = current.y + direction.y;
                if (!isMapCellInside(map, nextX, nextY))
                    continue;
                const int nextIndex = mapCellIndex(map, nextX, nextY);
                if (distance[static_cast<std::size_t>(nextIndex)] <=
                    distance[static_cast<std::size_t>(currentIndex)] + 1)
                    continue;
                distance[static_cast<std::size_t>(nextIndex)] = distance[static_cast<std::size_t>(currentIndex)] + 1;
                pending.push({nextX, nextY});
            }
        }
        return distance;
    }();
    deepestLandCell = 0;
    for (int y = 1; y < map.height - 1; ++y)
        for (int x = 1; x < map.width - 1; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind == MapTileKind::Water)
                continue;
            deepestLandCell =
                std::max(deepestLandCell, coastDistanceField[static_cast<std::size_t>(mapCellIndex(map, x, y))]);
        }
    // A world river needs a recognisable inland headwater. Six cells of
    // clearance on the 72x48 preset was still close enough to a ragged bay to
    // read as a second sea outlet once the wide overview curve was rendered.
    const int desiredClearance = std::max(2, std::min(map.width, map.height) / 6);
    sourceCoastClearance = std::max(1, std::min(desiredClearance, deepestLandCell));
}
