#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
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

std::vector<MapFeaturePoint> mapGenV5TraceNationLandBoundary(const MapModel &map,
                                                             const std::vector<MapFeaturePoint> &centers,
                                                             std::size_t nationIndex)
{
    if (map.width <= 0 || map.height <= 0 || nationIndex >= centers.size())
        return {};

    const int area = map.width * map.height;
    std::vector<int> owner;
    {
        // Political ownership is a land-constrained weighted distance rather
        // than an Euclidean Voronoi diagram. Each cell is reached through a
        // continuous path from its national seed, so a retained territory is
        // connected by construction. High traversal costs at ridges, river
        // channels and coastal bottlenecks make those features natural
        // borders; open ocean and lakes are never owned. River projection
        // remains finitely traversable so a nation is not split into two
        // disconnected polygons merely because its land contains a channel.
        struct NationFrontier
        {
            int cost = 0;
            int cellIndex = -1;
            int nation = -1;
        };
        struct NationFrontierLater
        {
            bool operator()(const NationFrontier &left, const NationFrontier &right) const
            {
                if (left.cost != right.cost)
                    return left.cost > right.cost;
                if (left.nation != right.nation)
                    return left.nation > right.nation;
                return left.cellIndex > right.cellIndex;
            }
        };
        const auto isRiverProjection = [&](int x, int y) {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind != MapTileKind::Water)
                return false;
            if (cell->wayKind == MapWayKind::River || cell->wayKind == MapWayKind::Canal)
                return true;
            if (cell->wayKind != MapWayKind::None)
                return false;
            const auto riverLike = [&](int neighborX, int neighborY) {
                const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
                return neighbor && (neighbor->wayKind == MapWayKind::River || neighbor->wayKind == MapWayKind::Canal);
            };
            const bool north = riverLike(x, y - 1);
            const bool east = riverLike(x + 1, y);
            const bool south = riverLike(x, y + 1);
            const bool west = riverLike(x - 1, y);
            return (north && east) || (east && south) || (south && west) || (west && north);
        };
        const auto isPoliticalDomain = [&](int x, int y) {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                return false;
            if (cell->kind == MapTileKind::Water)
                return isRiverProjection(x, y);
            return cell->kind != MapTileKind::Lava && cell->kind != MapTileKind::Pit &&
                   cell->kind != MapTileKind::Chasm;
        };
        const int noiseScale = std::max(5, std::min(map.width, map.height) / 7);
        const bool hasExplicitPoliticalRelations = std::any_of(
            map.generation.recipe.facts.begin(), map.generation.recipe.facts.end(), [](const MapGenerationFact &fact) {
                return fact.type == MapGenerationFactType::RelativePosition &&
                       fact.strength == MapGenerationFactStrength::Must;
            });
        const auto expansionCost = [&](int fromX, int fromY, int toX, int toY, int nation) {
            const MapCellModel *from = mapCellAt(map, fromX, fromY);
            const MapCellModel *to = mapCellAt(map, toX, toY);
            if (!from || !to)
                return 1000000;
            int cost = 100;
            switch (to->kind)
            {
            case MapTileKind::Water:
                cost += 440;
                break;
            case MapTileKind::Mountain:
                cost += 330;
                break;
            case MapTileKind::Snow:
                cost += 240;
                break;
            case MapTileKind::Hill:
                cost += 105;
                break;
            case MapTileKind::Swamp:
                cost += 95;
                break;
            case MapTileKind::Forest:
                cost += 48;
                break;
            case MapTileKind::Desert:
                cost += 36;
                break;
            default:
                break;
            }
            // Uphill expansion is deliberately more expensive than descending
            // from the same ridge, causing opposing fronts to meet at the
            // watershed rather than on an arbitrary straight bisector.
            cost += std::max(0, to->elevation - from->elevation) * 14;
            if (mapGenWayKindIsRoadLike(to->wayKind))
                cost -= 24;

            int riverBanks = 0;
            int coastalEdges = 0;
            constexpr MapCellCoordinate neighbors[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
            for (const MapCellCoordinate direction : neighbors)
            {
                const MapCellModel *neighbor = mapCellAt(map, toX + direction.x, toY + direction.y);
                if (!neighbor || neighbor->kind != MapTileKind::Water)
                    continue;
                if (neighbor->wayKind == MapWayKind::River || neighbor->wayKind == MapWayKind::Canal)
                    ++riverBanks;
                else
                    ++coastalEdges;
            }
            cost += riverBanks * 34 + coastalEdges * 13;

            // One broad deterministic friction field bends otherwise straight
            // fronts without introducing one-cell political noise. It is part
            // of the recipe seed and therefore stable across threads/platforms.
            const float administrativeFriction = mapGenV2ValueNoise(
                map.generation.recipe.seed + 7331u, static_cast<float>(toX), static_cast<float>(toY), noiseScale);
            cost += static_cast<int>(std::lround(administrativeFriction * 92.0f));
            // A friction field shared by every expansion front still leaves
            // the equal-cost boundary between two seeds close to a geometric
            // bisector. Give each nation a second, broad and deterministic
            // suitability field. This models different administrative reach
            // (valleys, historic claims and easy approaches) and bends the
            // shared frontier without introducing disconnected one-cell
            // enclaves: every claimed cell still retains a same-nation path
            // back to its unique seed through this multi-source Dijkstra run.
            if (!hasExplicitPoliticalRelations)
            {
                const float nationalSuitability = mapGenV2ValueNoise(
                    map.generation.recipe.seed + 9109u + static_cast<std::uint32_t>(std::max(0, nation)) * 1013u,
                    static_cast<float>(toX), static_cast<float>(toY), noiseScale + 2);
                cost += static_cast<int>(std::lround(nationalSuitability * 58.0f));
            }
            return std::max(24, cost);
        };

        owner.assign(static_cast<std::size_t>(area), -1);
        std::vector<int> distance(static_cast<std::size_t>(area), std::numeric_limits<int>::max());
        std::vector<unsigned char> claimedSeed(static_cast<std::size_t>(area), 0);
        std::vector<unsigned char> finalized(static_cast<std::size_t>(area), 0);
        std::priority_queue<NationFrontier, std::vector<NationFrontier>, NationFrontierLater> frontier;
        for (std::size_t centerIndex = 0; centerIndex < centers.size(); ++centerIndex)
        {
            int bestIndex = -1;
            float bestDistance = std::numeric_limits<float>::max();
            for (int y = 0; y < map.height; ++y)
                for (int x = 0; x < map.width; ++x)
                {
                    const int index = mapCellIndex(map, x, y);
                    const MapCellModel *seedCell = mapCellAt(map, x, y);
                    if (index < 0 || claimedSeed[static_cast<std::size_t>(index)] || !isPoliticalDomain(x, y) ||
                        !seedCell || seedCell->kind == MapTileKind::Water)
                        continue;
                    const float deltaX = static_cast<float>(x) + 0.5f - centers[centerIndex].x;
                    const float deltaY = static_cast<float>(y) + 0.5f - centers[centerIndex].y;
                    const float centerDistance = deltaX * deltaX + deltaY * deltaY;
                    if (centerDistance < bestDistance)
                    {
                        bestDistance = centerDistance;
                        bestIndex = index;
                    }
                }
            if (bestIndex < 0)
                continue;
            claimedSeed[static_cast<std::size_t>(bestIndex)] = 1;
            distance[static_cast<std::size_t>(bestIndex)] = 0;
            owner[static_cast<std::size_t>(bestIndex)] = static_cast<int>(centerIndex);
            frontier.push({0, bestIndex, static_cast<int>(centerIndex)});
        }

        constexpr MapCellCoordinate cardinalDirections[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        while (!frontier.empty())
        {
            const NationFrontier current = frontier.top();
            frontier.pop();
            if (current.cellIndex < 0 || current.cost != distance[static_cast<std::size_t>(current.cellIndex)] ||
                current.nation != owner[static_cast<std::size_t>(current.cellIndex)] ||
                finalized[static_cast<std::size_t>(current.cellIndex)])
                continue;
            finalized[static_cast<std::size_t>(current.cellIndex)] = 1;
            const int currentX = current.cellIndex % map.width;
            const int currentY = current.cellIndex / map.width;
            for (const MapCellCoordinate direction : cardinalDirections)
            {
                const int nextX = currentX + direction.x;
                const int nextY = currentY + direction.y;
                if (!isPoliticalDomain(nextX, nextY))
                    continue;
                const int nextIndex = mapCellIndex(map, nextX, nextY);
                if (finalized[static_cast<std::size_t>(nextIndex)])
                    continue;
                const int candidateCost =
                    current.cost + expansionCost(currentX, currentY, nextX, nextY, current.nation);
                // Equal costs keep the first deterministic queue winner. Every
                // ownership chain therefore retains a same-owner predecessor
                // all the way back to its unique seed.
                if (candidateCost >= distance[static_cast<std::size_t>(nextIndex)])
                    continue;
                distance[static_cast<std::size_t>(nextIndex)] = candidateCost;
                owner[static_cast<std::size_t>(nextIndex)] = current.nation;
                frontier.push({candidateCost, nextIndex, current.nation});
            }
        }
    }
    if (owner.size() != static_cast<std::size_t>(area))
        return {};

    std::vector<unsigned char> visited(static_cast<std::size_t>(area), 0);
    std::vector<MapCellCoordinate> largestComponent;
    constexpr MapCellCoordinate cardinal[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = mapCellIndex(map, x, y);
            if (startIndex < 0 || visited[static_cast<std::size_t>(startIndex)] ||
                owner[static_cast<std::size_t>(startIndex)] != static_cast<int>(nationIndex))
                continue;
            std::vector<MapCellCoordinate> component;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<std::size_t>(startIndex)] = 1;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                component.push_back(current);
                for (const MapCellCoordinate direction : cardinal)
                {
                    const int nextX = current.x + direction.x;
                    const int nextY = current.y + direction.y;
                    // mapCellIndex deliberately only performs the row-major
                    // conversion; it does not validate coordinates. Checking
                    // only for a negative converted index lets left/right
                    // neighbors wrap into adjacent rows and lets a bottom
                    // neighbor read beyond visited/owner. Besides being an
                    // out-of-bounds read, that made territory outlines (and
                    // therefore candidate eligibility) depend on heap state.
                    if (!isMapCellInside(map, nextX, nextY))
                        continue;
                    const int nextIndex = mapCellIndex(map, nextX, nextY);
                    if (visited[static_cast<std::size_t>(nextIndex)] ||
                        owner[static_cast<std::size_t>(nextIndex)] != static_cast<int>(nationIndex))
                        continue;
                    visited[static_cast<std::size_t>(nextIndex)] = 1;
                    pending.push({nextX, nextY});
                }
            }
            if (component.size() > largestComponent.size())
                largestComponent = std::move(component);
        }
    }
    if (largestComponent.empty())
        return {};

    std::vector<unsigned char> componentMask(static_cast<std::size_t>(area), 0);
    for (const MapCellCoordinate cell : largestComponent)
        componentMask[static_cast<std::size_t>(mapCellIndex(map, cell.x, cell.y))] = 1;
    const auto belongs = [&](int x, int y) {
        if (!isMapCellInside(map, x, y))
            return false;
        const int index = mapCellIndex(map, x, y);
        return componentMask[static_cast<std::size_t>(index)] != 0;
    };

    struct BoundaryEdge
    {
        int startX = 0;
        int startY = 0;
        int endX = 0;
        int endY = 0;
        bool used = false;
    };
    std::vector<BoundaryEdge> edges;
    edges.reserve(largestComponent.size() * 2);
    for (const MapCellCoordinate cell : largestComponent)
    {
        if (!belongs(cell.x, cell.y - 1))
            edges.push_back({cell.x, cell.y, cell.x + 1, cell.y, false});
        if (!belongs(cell.x + 1, cell.y))
            edges.push_back({cell.x + 1, cell.y, cell.x + 1, cell.y + 1, false});
        if (!belongs(cell.x, cell.y + 1))
            edges.push_back({cell.x + 1, cell.y + 1, cell.x, cell.y + 1, false});
        if (!belongs(cell.x - 1, cell.y))
            edges.push_back({cell.x, cell.y + 1, cell.x, cell.y, false});
    }

    std::vector<MapFeaturePoint> bestLoop;
    float bestArea = 0.0f;
    for (std::size_t startEdge = 0; startEdge < edges.size(); ++startEdge)
    {
        if (edges[startEdge].used)
            continue;
        std::vector<MapFeaturePoint> loop;
        std::size_t currentEdge = startEdge;
        const int loopStartX = edges[startEdge].startX;
        const int loopStartY = edges[startEdge].startY;
        for (std::size_t guard = 0; guard <= edges.size(); ++guard)
        {
            BoundaryEdge &edge = edges[currentEdge];
            if (edge.used)
                break;
            edge.used = true;
            loop.push_back({static_cast<float>(edge.startX), static_cast<float>(edge.startY)});
            if (edge.endX == loopStartX && edge.endY == loopStartY)
                break;
            const auto next = std::find_if(edges.begin(), edges.end(), [&](const BoundaryEdge &candidate) {
                return !candidate.used && candidate.startX == edge.endX && candidate.startY == edge.endY;
            });
            if (next == edges.end())
            {
                loop.clear();
                break;
            }
            currentEdge = static_cast<std::size_t>(std::distance(edges.begin(), next));
        }
        if (loop.size() < 3)
            continue;

        // Remove grid points that do not change direction. The remaining
        // controls follow the coastline and shared land borders without
        // storing hundreds of redundant points on straight segments.
        bool changed = true;
        while (changed && loop.size() > 3)
        {
            changed = false;
            for (std::size_t index = 0; index < loop.size(); ++index)
            {
                const MapFeaturePoint &before = loop[(index + loop.size() - 1) % loop.size()];
                const MapFeaturePoint &current = loop[index];
                const MapFeaturePoint &after = loop[(index + 1) % loop.size()];
                const float cross =
                    (current.x - before.x) * (after.y - current.y) - (current.y - before.y) * (after.x - current.x);
                if (std::abs(cross) > 0.0001f)
                    continue;
                loop.erase(loop.begin() + static_cast<std::ptrdiff_t>(index));
                changed = true;
                break;
            }
        }
        float signedArea = 0.0f;
        for (std::size_t index = 0; index < loop.size(); ++index)
        {
            const MapFeaturePoint &first = loop[index];
            const MapFeaturePoint &second = loop[(index + 1) % loop.size()];
            signedArea += first.x * second.y - second.x * first.y;
        }
        const float areaMagnitude = std::abs(signedArea) * 0.5f;
        if (areaMagnitude > bestArea)
        {
            bestArea = areaMagnitude;
            bestLoop = std::move(loop);
        }
    }
    return bestLoop;
}

