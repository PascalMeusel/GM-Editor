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

void MapGenerationOverworldBuilder::addSecondaryRoadLoops()
{
    if (!growingOverviewTree && options.scale == MapLevel::World && !sharedGreatRiver && settlementNodeCount >= 3 &&
        options.roadNetwork == MapRoadNetworkMode::Connected)
    {
        std::vector<MapCellCoordinate> bestLoop;
        int bestLoopScore = std::numeric_limits<int>::max();
        for (int first = 0; first < static_cast<int>(settlementNodeCount); ++first)
        {
            for (int second = first + 1; second < static_cast<int>(settlementNodeCount); ++second)
            {
                const bool alreadyPlanned =
                    std::any_of(plannedRoadEdges.begin(), plannedRoadEdges.end(), [&](const PlannedRoadEdge &edge) {
                        return edge.first == first && edge.second == second;
                    });
                if (alreadyPlanned)
                    continue;
                std::vector<MapCellCoordinate> candidate =
                    mapGenV2FindRoadPath(map, networkNodes[first], networkNodes[second]);
                if (candidate.size() < 3)
                    continue;
                float candidateLength = 0.0f;
                for (std::size_t pathIndex = 1; pathIndex < candidate.size(); ++pathIndex)
                {
                    const float deltaX = static_cast<float>(candidate[pathIndex].x - candidate[pathIndex - 1].x);
                    const float deltaY = static_cast<float>(candidate[pathIndex].y - candidate[pathIndex - 1].y);
                    candidateLength += std::sqrt(deltaX * deltaX + deltaY * deltaY);
                }
                const float directX = static_cast<float>(networkNodes[second].x - networkNodes[first].x);
                const float directY = static_cast<float>(networkNodes[second].y - networkNodes[first].y);
                const float directLength = std::sqrt(directX * directX + directY * directY);
                // A secondary loop is optional.  Do not add one that circles
                // most of a continent merely to reconnect two already linked
                // places; it harms readability and is precisely the kind of
                // detour the unchanged quality floor is meant to reject.
                if (candidateLength > 0.01f && directLength / candidateLength < 0.48f)
                    continue;
                int newCells = 0;
                for (const MapCellCoordinate &coordinate : candidate)
                {
                    const MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
                    if (cell && !mapGenWayKindIsRoadLike(cell->wayKind))
                        ++newCells;
                }
                if (newCells < 2)
                    continue;
                const int overlapPenalty = (static_cast<int>(candidate.size()) - newCells) * 6;
                const int candidateScore = roadPathCost(candidate) + overlapPenalty;
                if (candidateScore < bestLoopScore)
                {
                    bestLoopScore = candidateScore;
                    bestLoop = std::move(candidate);
                }
            }
        }
        if (!bestLoop.empty())
        {
            mapGenV2RasterizeRoad(map, bestLoop, roadKind);
            mapGenV2AddPathFeature(map, MapGeneratedFeatureKind::Road,
                                   mapGenV2FeatureId(options.seed, "road", roadIndex++), "Secondary road",
                                   "fantasy_neutral.road.dirt", 0.68f, bestLoop);
            if ((options.generatorVersion >= 5 && options.scale == MapLevel::World) ||
                (options.generatorVersion >= 6 && options.scale == MapLevel::Region))
                mapGenV5SmoothWorldRoadPathFeature(map, map.generation.graph.features.back(), 3);
        }
    }
    else if (!growingOverviewTree && options.scale != MapLevel::World && networkNodes.size() >= 4 &&
             options.roadNetwork == MapRoadNetworkMode::Connected && options.routeDensity > 55)
    {
        const bool regionalSettlementLoop = options.generatorVersion >= 6 && options.scale == MapLevel::Region;
        std::vector<MapCellCoordinate> loop;
        if (!regionalSettlementLoop || settlementNodeCount >= 3)
            loop = mapGenV2FindRoadPath(map, networkNodes[1],
                                        regionalSettlementLoop ? networkNodes[settlementNodeCount - 1]
                                                               : networkNodes.back());
        if (!loop.empty())
        {
            mapGenV2RasterizeRoad(map, loop, roadKind);
            mapGenV2AddPathFeature(map, MapGeneratedFeatureKind::Road,
                                   mapGenV2FeatureId(options.seed, "road", roadIndex++), "Secondary road",
                                   "fantasy_neutral.road.dirt", 0.68f, loop);
            if ((options.generatorVersion >= 5 && options.scale == MapLevel::World) ||
                (options.generatorVersion >= 6 && options.scale == MapLevel::Region))
                mapGenV5SmoothWorldRoadPathFeature(map, map.generation.graph.features.back(), 3);
        }
    }
}
