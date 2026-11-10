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

void MapGenerationOverworldBuilder::addUsableEdgeNode(int side, int preferred)
{
    const int extent = side < 2 ? map.height : map.width;
    for (int distance = 0; distance < extent; ++distance)
    {
        const int offsets[] = {distance, -distance};
        for (const int offset : offsets)
        {
            if (distance == 0 && offset < 0)
                continue;
            const int position = preferred + offset;
            if (position < 0 || position >= extent)
                continue;
            const MapCellCoordinate candidate = side == 0   ? MapCellCoordinate{0, position}
                                                : side == 1 ? MapCellCoordinate{map.width - 1, position}
                                                : side == 2 ? MapCellCoordinate{position, 0}
                                                            : MapCellCoordinate{position, map.height - 1};
            const MapCellModel *cell = mapCellAt(map, candidate.x, candidate.y);
            if (!cell || cell->kind == MapTileKind::Water || mapGenV2RoadTraversalCost(*cell) >= 100000)
                continue;
            if (options.generatorVersion >= 5 && options.scale == MapLevel::World && settlementNodeCount > 0)
            {
                bool directApproach = false;
                for (std::size_t settlementIndex = 0;
                     settlementIndex < settlementNodeCount && !directApproach; ++settlementIndex)
                {
                    const MapCellCoordinate &settlement = networkNodes[settlementIndex];
                    const std::vector<MapCellCoordinate> path = mapGenV2FindRoadPath(map, settlement, candidate);
                    if (path.size() < 2)
                        continue;
                    float pathLength = 0.0f;
                    for (std::size_t pathIndex = 1; pathIndex < path.size(); ++pathIndex)
                    {
                        const float deltaX = static_cast<float>(path[pathIndex].x - path[pathIndex - 1].x);
                        const float deltaY = static_cast<float>(path[pathIndex].y - path[pathIndex - 1].y);
                        pathLength += std::sqrt(deltaX * deltaX + deltaY * deltaY);
                    }
                    const float directX = static_cast<float>(candidate.x - settlement.x);
                    const float directY = static_cast<float>(candidate.y - settlement.y);
                    const float directLength = std::sqrt(directX * directX + directY * directY);
                    directApproach = pathLength <= 0.01f || directLength / pathLength >= 0.48f;
                }
                if (!directApproach)
                    continue;
            }
            if (std::find_if(networkNodes.begin(), networkNodes.end(), [&](const MapCellCoordinate &node) {
                    return node.x == candidate.x && node.y == candidate.y;
                }) == networkNodes.end())
                networkNodes.push_back(candidate);
            return;
        }
    }
}

void MapGenerationOverworldBuilder::prepareRoadNetwork()
{
    mapGenReportProgress(options, 5, totalSteps, "Planning roads");
    roadKind = mapGenGeneratedRoadKind(options);
    omitRoads = options.generatorVersion >= 11 && options.routeDensity == 0;
    networkNodes = omitRoads ? std::vector<MapCellCoordinate>{} : settlements;
    settlementNodeCount = networkNodes.size();
    // A sparse note-driven world has no declared off-map road destinations.
    // Fabricating four edge exits can create a one-cell coastal causeway that
    // looks like a road launched into the sea. Connect the known settlements;
    // explicit atlas/route constraints can add exits when the GM asks for them.
    const int edgeCount = omitRoads || (sharedGreatRiver && options.scale == MapLevel::World)
                              ? 0
                              : std::clamp(options.edgeConnectionCount, 0, 4);
    if (edgeCount > 0)
        addUsableEdgeNode(0, std::clamp(map.height / 3 + mapGenRandomBetween(rng, -2, 2), 1, map.height - 2));
    if (edgeCount > 1)
        addUsableEdgeNode(1, std::clamp(map.height * 2 / 3 + mapGenRandomBetween(rng, -2, 2), 1, map.height - 2));
    if (edgeCount > 2)
        addUsableEdgeNode(2, std::clamp(map.width / 3 + mapGenRandomBetween(rng, -2, 2), 1, map.width - 2));
    if (edgeCount > 3)
        addUsableEdgeNode(3, std::clamp(map.width * 2 / 3 + mapGenRandomBetween(rng, -2, 2), 1, map.width - 2));


    connected.assign(networkNodes.size(), false);
    if (!networkNodes.empty())
        connected[0] = true;
    plannedRoadEdges.clear();
    roadIndex = 0;
    growingOverviewTree =
        options.generatorVersion >= 7 && (options.scale == MapLevel::World || options.scale == MapLevel::Region);
    grownRoadIds.clear();
}

