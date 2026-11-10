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
#include "Models/Map/Generation/MapGenerationOverworldBuilder.hpp"

int MapGenerationOverworldBuilder::roadPathCost(const std::vector<MapCellCoordinate> &path) const
{
    int total = 0;
    for (std::size_t index = 1; index < path.size(); ++index)
    {
        const MapCellModel *cell = mapCellAt(map, path[index].x, path[index].y);
        if (!cell)
            return std::numeric_limits<int>::max();
        const bool diagonal = path[index].x != path[index - 1].x && path[index].y != path[index - 1].y;
        if (options.generatorVersion >= 7 &&
            (options.scale == MapLevel::World || options.scale == MapLevel::Region))
        {
            const auto *previousCell = mapCellAt(map, path[index - 1].x, path[index - 1].y);
            const int grade = previousCell ? std::abs(cell->elevation - previousCell->elevation) : 0;
            const int friction = cell->kind == MapTileKind::Mountain ? 500
                                 : cell->kind == MapTileKind::Water  ? 600
                                                                     : mapGenV7OverviewTerrainFriction(*cell);
            total += (diagonal ? 141 : 100) * (100 + friction) / 100 + grade * grade * 15;
        }
        else
        {
            total += mapGenV2RoadTraversalCost(*cell);
            if (diagonal)
                ++total;
        }
    }
    return total;
}

void MapGenerationOverworldBuilder::growRoadNetwork()
{
    for (std::size_t connectedCount = networkNodes.empty() ? 0 : 1; connectedCount < networkNodes.size();
         ++connectedCount)
    {
        int bestFrom = -1;
        int bestTo = -1;
        int bestCost = std::numeric_limits<int>::max();
        int bestDistance = std::numeric_limits<int>::max();
        std::vector<MapCellCoordinate> bestPath;
        MapFeaturePoint bestJoin;
        if (growingOverviewTree)
        {
            struct Anchor
            {
                MapFeaturePoint point;
                int node;
                float distance;
            };
            for (int to = 0; to < static_cast<int>(networkNodes.size()); ++to)
            {
                if (connected[static_cast<std::size_t>(to)])
                    continue;
                const auto destination = networkNodes[to];
                std::vector<Anchor> anchors;
                const auto addAnchor = [&](MapFeaturePoint point, int node) {
                    const int x = static_cast<int>(point.x), y = static_cast<int>(point.y);
                    const auto *cell = mapCellAt(map, x, y);
                    if (!cell || cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Bridge ||
                        cell->kind == MapTileKind::Mountain || mapGenV2RoadTraversalCost(*cell) >= 100000 ||
                        !mapGenV7RoadChordIsSafe(map, {x + 0.5f, y + 0.5f}, point))
                        return;
                    const float dx = point.x - (destination.x + 0.5f), dy = point.y - (destination.y + 0.5f);
                    anchors.push_back({point, node, dx * dx + dy * dy});
                };
                for (int from = 0; from < static_cast<int>(networkNodes.size()); ++from)
                    if (connected[static_cast<std::size_t>(from)] &&
                        (settlementNodeCount == 0 || static_cast<std::size_t>(from) < settlementNodeCount))
                        addAnchor({networkNodes[from].x + 0.5f, networkNodes[from].y + 0.5f}, from);
                for (const auto &feature : map.generation.graph.features)
                    if (std::find(grownRoadIds.begin(), grownRoadIds.end(), feature.id) != grownRoadIds.end())
                        for (const auto point : feature.points)
                            // Edge exits stay leaves, including at the fitted
                            // centerline level rather than only in the raster.
                            if (point.x > 2.0f && point.y > 2.0f && point.x < map.width - 2.0f &&
                                point.y < map.height - 2.0f)
                                addAnchor(point, 0);
                std::stable_sort(anchors.begin(), anchors.end(),
                                 [](const auto &a, const auto &b) { return a.distance < b.distance; });
                std::vector<MapCellCoordinate> tried;
                for (const auto &anchor : anchors)
                {
                    const bool syntheticExit =
                        settlementNodeCount > 0 && static_cast<std::size_t>(to) >= settlementNodeCount;
                    const float maximumSpur = std::max(5.0f, std::min(map.width, map.height) * 0.45f);
                    if (syntheticExit && anchor.distance > maximumSpur * maximumSpur)
                        break;
                    const MapCellCoordinate goal{static_cast<int>(anchor.point.x), static_cast<int>(anchor.point.y)};
                    if (std::any_of(tried.begin(), tried.end(), [&](auto prior) {
                            return std::abs(prior.x - goal.x) + std::abs(prior.y - goal.y) < 4;
                        }))
                        continue;
                    tried.push_back(goal);
                    auto candidatePath = mapGenV2FindRoadPath(map, destination, goal);
                    if (syntheticExit && !candidatePath.empty())
                    {
                        float length = 0.0f;
                        for (std::size_t point = 1; point < candidatePath.size(); ++point)
                            length +=
                                std::hypot(static_cast<float>(candidatePath[point].x - candidatePath[point - 1].x),
                                           static_cast<float>(candidatePath[point].y - candidatePath[point - 1].y));
                        if (length > maximumSpur * 1.35f)
                            candidatePath.clear();
                    }
                    if (!candidatePath.empty())
                    {
                        const int candidateCost = roadPathCost(candidatePath);
                        if (candidateCost < bestCost)
                        {
                            bestCost = candidateCost;
                            bestFrom = anchor.node;
                            bestTo = to;
                            bestDistance = std::abs(destination.x - goal.x) + std::abs(destination.y - goal.y);
                            bestJoin = anchor.point;
                            bestPath = std::move(candidatePath);
                        }
                        // The nearest reachable corridor provides a natural
                        // junction. Avoid all-pairs rerouting after every edge.
                        break;
                    }
                    if (tried.size() >= 3)
                        break;
                }
            }
        }
        else
            for (int from = 0; from < static_cast<int>(networkNodes.size()); ++from)
            {
                if (!connected[static_cast<std::size_t>(from)])
                    continue;
                // Optional exits describe off-map destinations, not junctions at
                // the map border. Using one as a new branch root made two roads
                // run out to the edge and immediately reverse back into the map.
                if (options.generatorVersion >= 6 && options.scale == MapLevel::Region && settlementNodeCount > 0 &&
                    static_cast<std::size_t>(from) >= settlementNodeCount)
                    continue;
                for (int to = 0; to < static_cast<int>(networkNodes.size()); ++to)
                {
                    if (connected[static_cast<std::size_t>(to)])
                        continue;
                    const int distance = std::abs(networkNodes[from].x - networkNodes[to].x) +
                                         std::abs(networkNodes[from].y - networkNodes[to].y);
                    std::vector<MapCellCoordinate> candidatePath =
                        mapGenV2FindRoadPath(map, networkNodes[from], networkNodes[to]);
                    const int candidateCost = roadPathCost(candidatePath);
                    if (!candidatePath.empty() &&
                        (candidateCost < bestCost || (candidateCost == bestCost && distance < bestDistance)))
                    {
                        bestCost = candidateCost;
                        bestDistance = distance;
                        bestFrom = from;
                        bestTo = to;
                        bestPath = std::move(candidatePath);
                    }
                }
            }
        if (bestFrom < 0 || bestTo < 0)
            break;
        if (growingOverviewTree && bestPath.size() < 2)
        {
            connected[static_cast<std::size_t>(bestTo)] = true;
            continue;
        }
        auto previousRoadCells = growingOverviewTree ? map.cells : std::vector<MapCellModel>{};
        mapGenV2RasterizeRoad(map, bestPath, roadKind);
        mapGenV2AddPathFeature(
            map, roadKind == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road,
            mapGenV2FeatureId(options.seed, "road", roadIndex++), "Road connection", "fantasy_neutral.road.dirt",
            roadKind == MapWayKind::Trail ? 0.55f : 0.82f, bestPath);
        if (options.generatorVersion >= 7 && (options.scale == MapLevel::World || options.scale == MapLevel::Region))
        {
            const bool edgeSpur = static_cast<std::size_t>(bestTo) >= settlementNodeCount ||
                                  static_cast<std::size_t>(bestFrom) >= settlementNodeCount;
            const bool principalLink = !edgeSpur && bestDistance >= std::min(map.width, map.height) / 3;
            map.generation.graph.features.back().width = (edgeSpur        ? 0.50f
                                                          : principalLink ? 0.92f
                                                                          : 0.68f) *
                                                         (roadKind == MapWayKind::Trail ? 0.7f : 1.0f);
        }
        if ((options.generatorVersion >= 5 && options.scale == MapLevel::World) ||
            (options.generatorVersion >= 6 && options.scale == MapLevel::Region))
        {
            if (growingOverviewTree)
                map.generation.graph.features.back().points.back() = bestJoin;
            if (options.generatorVersion >= 11 && growingOverviewTree && settlementNodeCount > 0 &&
                static_cast<std::size_t>(bestTo) >= settlementNodeCount)
                map.generation.graph.features.back().points.front() =
                    mapGenV11RoadBoundaryAnchor(map, networkNodes[bestTo]);
            mapGenV5SmoothWorldRoadPathFeature(map, map.generation.graph.features.back(), 3);
            if (growingOverviewTree)
            {
                mapGenV7JoinFirstExistingRoad(map.generation.graph.features.back(), map.generation.graph, &grownRoadIds,
                                              options.generatorVersion >= 11 ? &map : nullptr);
                mapGenV7CommitFittedRoadProjection(map, map.generation.graph.features.back(),
                                                   std::move(previousRoadCells), roadKind);
                grownRoadIds.push_back(map.generation.graph.features.back().id);
            }
        }
        plannedRoadEdges.push_back({std::min(bestFrom, bestTo), std::max(bestFrom, bestTo)});
        connected[static_cast<std::size_t>(bestTo)] = true;
    }
}

