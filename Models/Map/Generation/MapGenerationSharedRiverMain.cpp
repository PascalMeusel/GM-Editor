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

bool MapGenerationSharedRiverBuilder::buildMainRiver()
{
    const auto meanderAnchor = [&](MapCellCoordinate start, MapCellCoordinate end, float side) {
        const float deltaX = static_cast<float>(end.x - start.x);
        const float deltaY = static_cast<float>(end.y - start.y);
        const float length = std::max(0.001f, std::sqrt(deltaX * deltaX + deltaY * deltaY));
        const float requestedAmplitude = length * 0.20f;
        const float scaledAmplitudeCap = static_cast<float>(std::min(map.width, map.height)) * 0.13f;
        // Legacy 24x18 maps have a scaled cap below the preferred three-cell
        // bend. Preserve the historical Windows Release comparison order,
        // without passing inverted bounds to std::clamp (undefined behavior).
        const float amplitude =
            requestedAmplitude > scaledAmplitudeCap ? scaledAmplitudeCap : std::max(requestedAmplitude, 3.0f);
        MapCellCoordinate desired{
            std::clamp(static_cast<int>(std::lround((start.x + end.x) * 0.5f - deltaY / length * amplitude * side)), 1,
                       map.width - 2),
            std::clamp(static_cast<int>(std::lround((start.y + end.y) * 0.5f + deltaX / length * amplitude * side)), 1,
                       map.height - 2)};
        MapCellCoordinate selected{-1, -1};
        for (int radius = 0; radius <= maximumRadius && selected.x < 0; ++radius)
        {
            for (int offsetY = -radius; offsetY <= radius && selected.x < 0; ++offsetY)
                for (int offsetX = -radius; offsetX <= radius; ++offsetX)
                {
                    if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                        continue;
                    const MapCellCoordinate candidate{desired.x + offsetX, desired.y + offsetY};
                    if (usableWaypoint(mapCellAt(map, candidate.x, candidate.y)))
                    {
                        selected = candidate;
                        break;
                    }
                }
        }
        return selected.x >= 0 ? selected : desired;
    };

    const float firstSide = mapGenV2Hash01(options.seed + 3457u, source.x, outlet.y) < 0.5f ? -1.0f : 1.0f;
    const MapCellCoordinate headBend = meanderAnchor(source, waypoint, firstSide);
    const MapCellCoordinate tailBend = meanderAnchor(waypoint, outlet, -firstSide);
    const MapCellCoordinate anchors[] = {source, headBend, waypoint, tailBend, outlet};
    path.clear();
    constexpr std::size_t anchorCount = sizeof(anchors) / sizeof(anchors[0]);
    for (std::size_t anchorIndex = 1; anchorIndex < anchorCount; ++anchorIndex)
    {
        std::vector<MapCellCoordinate> section =
            mapGenV4FindDrainagePath(map, elevation, anchors[anchorIndex - 1], anchors[anchorIndex],
                                     options.seed + 3463u + static_cast<std::uint32_t>(anchorIndex * 29u));
        if (section.empty())
            return false;
        path.insert(path.end(), section.begin() + (path.empty() ? 0 : 1), section.end());
    }

    // Remove any loop created where the two cost-routed halves meet. This is a
    // deterministic loop erasure, retaining adjacent cells and one continuous
    // editable spline from source to mouth.
    std::vector<int> position(static_cast<std::size_t>(map.width * map.height), -1);
    std::vector<MapCellCoordinate> simplePath;
    simplePath.reserve(path.size());
    for (const MapCellCoordinate coordinate : path)
    {
        const int cellIndex = mapCellIndex(map, coordinate.x, coordinate.y);
        if (cellIndex < 0)
            continue;
        const int prior = position[static_cast<std::size_t>(cellIndex)];
        if (prior >= 0)
        {
            for (std::size_t removeIndex = static_cast<std::size_t>(prior + 1); removeIndex < simplePath.size();
                 ++removeIndex)
            {
                const int removedCell = mapCellIndex(map, simplePath[removeIndex].x, simplePath[removeIndex].y);
                if (removedCell >= 0)
                    position[static_cast<std::size_t>(removedCell)] = -1;
            }
            simplePath.resize(static_cast<std::size_t>(prior + 1));
            continue;
        }
        position[static_cast<std::size_t>(cellIndex)] = static_cast<int>(simplePath.size());
        simplePath.push_back(coordinate);
    }
    path = std::move(simplePath);
    if (path.size() < 2)
        return false;

    const int sampledSourceElevation =
        std::clamp(static_cast<int>(
                       std::lround(elevation[static_cast<std::size_t>(mapCellIndex(map, source.x, source.y))] * 20.0f)),
                   1, 20);
    for (std::size_t pathIndex = 0; pathIndex < path.size(); ++pathIndex)
    {
        const MapCellCoordinate coordinate = path[pathIndex];
        mapGenSetRiverCell(map, coordinate.x, coordinate.y);
        if (MapCellModel *riverCell = mapCellAt(map, coordinate.x, coordinate.y))
        {
            const float progress =
                path.size() <= 1 ? 1.0f : static_cast<float>(pathIndex) / static_cast<float>(path.size() - 1);
            riverCell->elevation = std::clamp(
                static_cast<int>(std::lround(static_cast<float>(sampledSourceElevation) * (1.0f - progress))), 0, 20);
        }
    }
    mapGenV2AddPathFeature(map, MapGeneratedFeatureKind::River, mapGenV2FeatureId(options.seed, "river", 0),
                           "Main river", "fantasy_neutral.water.river", 1.8f, path);
    // V5 hydrology stores controls in downstream order. `width` is the mouth
    // width (the renderer derives a narrow source from the river material),
    // while the material distinguishes a primary river from its tributaries.
    // Keeping that convention on the feature itself makes editor, export and
    // player rendering agree without a second direction heuristic.
    map.generation.graph.features.back().locked = true;
    if (options.generatorVersion >= 5 && options.scale == MapLevel::World)
        mapGenV5SmoothWorldPathFeature(map.generation.graph.features.back(), 4);
    mapGenFillDiagonalRiverCornerGaps(map);
    return true;
}

