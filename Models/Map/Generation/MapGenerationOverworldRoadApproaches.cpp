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

void MapGenerationOverworldBuilder::connectSettlementApproaches()
{
    // A settlement can be valid on an island or in a broad area whose
    // requested edge exits lie in the sea. In that case the spanning-tree
    // planner has no reachable external node and previously left the town
    // without even a local approach. Join every such town to the nearest
    // existing road when possible; otherwise establish a short civic road
    // that can later be extended by the GM or a derived regional map.
    for (const MapCellCoordinate &settlement : settlements)
    {
        if (omitRoads)
            continue;
        if (mapGenCellHasStructureApproach(map, settlement.x, settlement.y))
            continue;
        MapCellCoordinate nearestRoad{-1, -1};
        int nearestDistance = std::numeric_limits<int>::max();
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || !mapGenWayKindIsRoadLike(cell->wayKind))
                    continue;
                const int distance = std::abs(settlement.x - x) + std::abs(settlement.y - y);
                if (distance < nearestDistance)
                {
                    nearestDistance = distance;
                    nearestRoad = {x, y};
                }
            }
        }

        MapFeaturePoint settlementRoadAnchor;
        bool anchoredSettlementApproach =
            options.generatorVersion >= 11 &&
            mapGenV7NearestDryRoadAnchor(map, {settlement.x + .5f, settlement.y + .5f}, settlementRoadAnchor);
        if (anchoredSettlementApproach)
            nearestRoad = {static_cast<int>(settlementRoadAnchor.x), static_cast<int>(settlementRoadAnchor.y)};
        std::vector<MapCellCoordinate> approach;
        if (nearestRoad.x >= 0)
            approach = mapGenV2FindRoadPath(map, settlement, nearestRoad);
        if (approach.size() < 2)
        {
            anchoredSettlementApproach = false;
            const MapCellCoordinate directions[] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                                                    {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
            int bestCost = std::numeric_limits<int>::max();
            MapCellCoordinate best{-1, -1};
            for (const MapCellCoordinate &direction : directions)
            {
                const MapCellCoordinate candidate{settlement.x + direction.x, settlement.y + direction.y};
                const MapCellModel *cell = mapCellAt(map, candidate.x, candidate.y);
                if (!cell)
                    continue;
                const int cost = mapGenV2RoadTraversalCost(*cell);
                if (gm::map_routing::usesBoundedMountainPassRouting(map) &&
                    !gm::map_routing::mountainPassDryApproach(cell))
                    continue;
                if (cost < bestCost)
                {
                    bestCost = cost;
                    best = candidate;
                }
            }
            if (best.x >= 0 && bestCost < 100000)
                approach = gm::map_routing::usesBoundedMountainPassRouting(map)
                               ? mapGenV2FindRoadPath(map, settlement, best)
                               : std::vector<MapCellCoordinate>{settlement, best};
        }
        if (approach.size() < 2)
            continue;
        const bool fittedApproach =
            options.generatorVersion >= 9 && (options.scale == MapLevel::World || options.scale == MapLevel::Region);
        auto priorApproachCells = fittedApproach ? map.cells : std::vector<MapCellModel>{};
        mapGenV2RasterizeRoad(map, approach, roadKind);
        mapGenV2AddPathFeature(
            map, roadKind == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road,
            mapGenV2FeatureId(options.seed, "road", roadIndex++), "Settlement approach", "fantasy_neutral.road.dirt",
            roadKind == MapWayKind::Trail ? 0.55f : 0.76f, approach);
        if (anchoredSettlementApproach)
            map.generation.graph.features.back().points.back() = settlementRoadAnchor;
        if ((options.generatorVersion >= 5 && options.scale == MapLevel::World) ||
            (options.generatorVersion >= 6 && options.scale == MapLevel::Region))
            mapGenV5SmoothWorldRoadPathFeature(map, map.generation.graph.features.back(), 3);
        if (fittedApproach)
        {
            if (options.generatorVersion >= 11)
                mapGenV7JoinFirstExistingRoad(map.generation.graph.features.back(), map.generation.graph, nullptr,
                                              &map);
            mapGenV7CommitFittedRoadProjection(map, map.generation.graph.features.back(), std::move(priorApproachCells),
                                               roadKind);
        }
    }
}

