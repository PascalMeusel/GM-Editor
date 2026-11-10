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

std::vector<MapCellCoordinate> mapGenV4FindDrainagePath(const MapModel &map, const std::vector<float> &elevation,
                                                        MapCellCoordinate source, MapCellCoordinate outlet,
                                                        std::uint32_t noiseSeed, bool composedValley)
{
    struct Node
    {
        float score = 0.0f;
        int index = -1;
        bool operator<(const Node &other) const
        {
            return score > other.score;
        }
    };
    const float valleyDx = static_cast<float>(outlet.x - source.x), valleyDy = static_cast<float>(outlet.y - source.y);
    const float valleyLength = std::max(1.f, std::hypot(valleyDx, valleyDy));
    const float valleySign = (noiseSeed & 1u) ? -1.f : 1.f;
    const float valleyAmplitude = std::min(4.2f, valleyLength * .18f);
    const float valleyPhase = mapGenV2Hash01(noiseSeed + 119u, 7, 13) * 6.2831853f;
    const int cellCount = map.width * map.height;
    std::vector<float> distance(static_cast<std::size_t>(cellCount), std::numeric_limits<float>::max());
    std::vector<int> previous(static_cast<std::size_t>(cellCount), -1);
    std::priority_queue<Node> pending;
    const int sourceIndex = mapCellIndex(map, source.x, source.y);
    const int outletIndex = mapCellIndex(map, outlet.x, outlet.y);
    distance[static_cast<std::size_t>(sourceIndex)] = 0.0f;
    pending.push({0.0f, sourceIndex});
    constexpr MapCellCoordinate offsets[] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    while (!pending.empty())
    {
        const Node current = pending.top();
        pending.pop();
        if (current.index == outletIndex)
            break;
        const int currentX = current.index % map.width;
        const int currentY = current.index / map.width;
        const float currentHeight = elevation[static_cast<std::size_t>(current.index)];
        for (const MapCellCoordinate offset : offsets)
        {
            const int nextX = currentX + offset.x;
            const int nextY = currentY + offset.y;
            if (!isMapCellInside(map, nextX, nextY))
                continue;
            const int nextIndex = mapCellIndex(map, nextX, nextY);
            const MapCellModel *nextCell = mapCellAt(map, nextX, nextY);
            // The selected outlet is the first lake/ocean cell.  Other open
            // water is never a drainage shortcut: allowing it made A* cross a
            // strait and the later truncation turned that route into a short,
            // apparently inland blue stroke. Existing river cells remain valid
            // confluences for ordinary tributaries.
            if (nextCell && nextCell->kind == MapTileKind::Water && nextIndex != outletIndex &&
                nextCell->wayKind != MapWayKind::River)
                continue;
            const float nextHeight = elevation[static_cast<std::size_t>(nextIndex)];
            const float uphill = std::max(0.0f, nextHeight - currentHeight);
            const float diagonal = offset.x != 0 && offset.y != 0 ? 1.4142f : 1.0f;
            const float valleyPreference = nextHeight * 0.48f;
            const float organicVariation = mapGenV2Hash01(noiseSeed, nextX, nextY) * 0.34f;
            const float existingRiverPenalty = nextCell && nextCell->wayKind == MapWayKind::River ? 8.0f : 0.0f;
            float valleyShapeCost = 0.f;
            if (composedValley && valleyLength >= 9.f)
            {
                const float rx = static_cast<float>(nextX - source.x), ry = static_cast<float>(nextY - source.y);
                const float t = std::clamp((rx * valleyDx + ry * valleyDy) / (valleyLength * valleyLength), 0.f, 1.f);
                const float lateral = (rx * -valleyDy + ry * valleyDx) / valleyLength;
                const float desired = valleySign * valleyAmplitude * std::sin(t * 3.14159265f) *
                                      (.68f * std::sin(t * 6.2831853f + valleyPhase) + .32f);
                // A broad lowland drainage corridor bends before crossings
                // and banks are created. Sources/outlets remain fixed; local
                // elevation and forbidden open water still dominate routing.
                valleyShapeCost = std::abs(lateral - desired) * .62f;
            }
            const float stepCost = diagonal + uphill * 28.0f + valleyPreference + organicVariation +
                                   existingRiverPenalty + valleyShapeCost;
            const float candidate = distance[static_cast<std::size_t>(current.index)] + stepCost;
            if (candidate >= distance[static_cast<std::size_t>(nextIndex)])
                continue;
            distance[static_cast<std::size_t>(nextIndex)] = candidate;
            previous[static_cast<std::size_t>(nextIndex)] = current.index;
            const float heuristic = static_cast<float>(std::abs(nextX - outlet.x) + std::abs(nextY - outlet.y)) * 0.75f;
            pending.push({candidate + heuristic, nextIndex});
        }
    }
    if (previous[static_cast<std::size_t>(outletIndex)] < 0)
        return {};
    std::vector<MapCellCoordinate> path;
    for (int index = outletIndex; index >= 0; index = previous[static_cast<std::size_t>(index)])
    {
        path.push_back({index % map.width, index / map.width});
        if (index == sourceIndex)
            break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

