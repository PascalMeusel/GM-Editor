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

void MapGenerationOverworldBuilder::addRegionalRiverCrossing()
{
    // A regional river may legitimately miss every settlement-to-settlement
    // road while still dividing the usable map into two halves. Provide one
    // deliberate, complete bank-to-bank crossing on the primary river. An
    // earlier road can touch a diagonal river cell yet leave the companion
    // corner-water cells blocking both banks; spanning to actual dry cells
    // here repairs that subtle case as well.
    // V10 crossings follow actual travel routes; do not add an optional
    // bridge loop around an already-connected short headwater.
    if (options.generatorVersion < 10 && !river.empty() && options.scale == MapLevel::Region &&
        options.roadNetwork != MapRoadNetworkMode::None && options.routeDensity > 0 &&
        !mapGenV6RegionHasCompleteRiverCrossing(map, river))
    {
        const int first = std::max(1, static_cast<int>(river.size()) / 5);
        const int last = std::max(first + 1, static_cast<int>(river.size()) * 4 / 5);
        std::vector<MapCellCoordinate> crossing;
        for (int pointIndex = first; pointIndex < last && crossing.empty(); ++pointIndex)
        {
            const MapCellCoordinate &center = river[static_cast<std::size_t>(pointIndex)];
            const MapCellCoordinate &before = river[static_cast<std::size_t>(std::max(0, pointIndex - 1))];
            const MapCellCoordinate &after =
                river[static_cast<std::size_t>(std::min(static_cast<int>(river.size()) - 1, pointIndex + 1))];
            const bool mostlyVertical = std::abs(after.y - before.y) >= std::abs(after.x - before.x);
            const MapCellCoordinate across = mostlyVertical ? MapCellCoordinate{1, 0} : MapCellCoordinate{0, 1};
            MapCellCoordinate negative{-1, -1};
            MapCellCoordinate positive{-1, -1};
            for (int distance = 1; distance <= 5 && negative.x < 0; ++distance)
            {
                const MapCellCoordinate candidate{center.x - across.x * distance, center.y - across.y * distance};
                const MapCellModel *cell = mapCellAt(map, candidate.x, candidate.y);
                if (cell && !mapGenRiverInfluenceCell(cell) && mapGenV2RoadTraversalCost(*cell) < 100000)
                    negative = candidate;
            }
            for (int distance = 1; distance <= 5 && positive.x < 0; ++distance)
            {
                const MapCellCoordinate candidate{center.x + across.x * distance, center.y + across.y * distance};
                const MapCellModel *cell = mapCellAt(map, candidate.x, candidate.y);
                if (cell && !mapGenRiverInfluenceCell(cell) && mapGenV2RoadTraversalCost(*cell) < 100000)
                    positive = candidate;
            }
            if (negative.x < 0 || positive.x < 0)
                continue;
            for (MapCellCoordinate current = negative;; current.x += across.x, current.y += across.y)
            {
                crossing.push_back(current);
                if (current.x == positive.x && current.y == positive.y)
                    break;
            }
            int wetSpanCells = 0;
            bool crossesMappedRiver = false;
            bool crossesUnrelatedWater = false;
            for (const MapCellCoordinate &coordinate : crossing)
            {
                const MapCellModel *crossingCell = mapCellAt(map, coordinate.x, coordinate.y);
                const bool wetCell = mapGenRiverInfluenceCell(crossingCell) ||
                                     (crossingCell && crossingCell->kind == MapTileKind::Bridge);
                if (!wetCell)
                    continue;
                ++wetSpanCells;
                float riverX = 0.0f;
                float riverY = 0.0f;
                const bool nearMappedRiver = mapGenV2NearestFeatureTangent(
                    map.generation.graph, MapGeneratedFeatureKind::River, static_cast<float>(coordinate.x) + 0.5f,
                    static_cast<float>(coordinate.y) + 0.5f, 1.6f, riverX, riverY);
                crossesMappedRiver = crossesMappedRiver || nearMappedRiver;
                crossesUnrelatedWater = crossesUnrelatedWater || !nearMappedRiver;
            }
            // Do not build the automatic regional crossing through a lake or
            // other broad water body merely because the primary river happens
            // to flow into it. River cells and diagonal corner-fill cells stay
            // within this narrow distance from the editable river spline.
            if (!crossesMappedRiver || crossesUnrelatedWater || wetSpanCells > 4)
                crossing.clear();
            if (gm::map_routing::usesBoundedMountainPassRouting(map) &&
                std::any_of(crossing.begin(), crossing.end(), [&](const MapCellCoordinate &coordinate) {
                    const auto *cell = mapCellAt(map, coordinate.x, coordinate.y);
                    return cell && cell->kind == MapTileKind::Mountain;
                }))
                crossing.clear();
        }
        if (crossing.size() >= 3)
        {
            // A bridge must join the existing travel network. Resolve the
            // nearest approach before the crossing becomes road terrain, or
            // the nearest-road query would simply select the new bridge.
            MapCellCoordinate nearestRoad{-1, -1};
            int nearestRoadDistance = std::numeric_limits<int>::max();
            for (int roadY = 0; roadY < map.height; ++roadY)
            {
                for (int roadX = 0; roadX < map.width; ++roadX)
                {
                    const MapCellModel *roadCell = mapCellAt(map, roadX, roadY);
                    if (!roadCell || !mapGenWayKindIsRoadLike(roadCell->wayKind))
                        continue;
                    const int distance = std::abs(crossing.front().x - roadX) + std::abs(crossing.front().y - roadY);
                    if (distance < nearestRoadDistance)
                    {
                        nearestRoadDistance = distance;
                        nearestRoad = {roadX, roadY};
                    }
                }
            }
            MapFeaturePoint roadAnchor;
            std::vector<MapCellCoordinate> approach;
            bool vectorApproach = false;
            if (options.generatorVersion >= 7)
            {
                auto chosenApproach = mapGenV7ChooseBridgeApproach(map, crossing);
                approach = std::move(chosenApproach.cells);
                roadAnchor = chosenApproach.roadAnchor;
                vectorApproach = !approach.empty();
            }
            else if (nearestRoad.x >= 0)
                approach = mapGenV2FindRoadPath(map, crossing.front(), nearestRoad);
            std::vector<std::string> approachTargetIds;
            if (vectorApproach)
                for (const auto &feature : map.generation.graph.features)
                    if (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail)
                        approachTargetIds.push_back(feature.id);
            if (vectorApproach && approach.size() == 1 &&
                std::hypot(roadAnchor.x - (approach.front().x + 0.5f), roadAnchor.y - (approach.front().y + 0.5f)) >
                    0.00001f)
                approach.push_back(approach.front()); // A real sub-cell bank-to-road join still needs a vector segment.

            // V8 optional crossings need a useful dry destination on both
            // banks. A pretty bridge ending in empty ground is not a network.
            MapGenV7BridgeApproach oppositeApproach;
            bool bothBanksConnected = true;
            if (options.generatorVersion >= 8)
            {
                bothBanksConnected = false;
                if (!approach.empty())
                {
                    const auto start = approach.front();
                    const auto bank = start.x == crossing.front().x && start.y == crossing.front().y ? crossing.back()
                                                                                                     : crossing.front();
                    oppositeApproach = mapGenV7ChooseBridgeApproach(map, {bank, bank});
                    const float mx = (crossing.front().x + crossing.back().x + 1.f) * .5f;
                    const float my = (crossing.front().y + crossing.back().y + 1.f) * .5f;
                    const float dx = bank.x + .5f - mx, dy = bank.y + .5f - my;
                    bothBanksConnected =
                        !oppositeApproach.cells.empty() &&
                        (oppositeApproach.roadAnchor.x - mx) * dx + (oppositeApproach.roadAnchor.y - my) * dy > 0.f;
                }
            }

            // An optional V7 crossing with no dry connection to the existing
            // network must not manufacture a second river crossing or remain
            // an isolated bridge. Explicit/preserved bridge facts are separate.
            if ((options.generatorVersion < 7 || vectorApproach) && bothBanksConnected)
            {
                mapGenV2RasterizeRoad(map, crossing, roadKind);
                mapGenV2AddPathFeature(
                    map, roadKind == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road,
                    mapGenV2FeatureId(options.seed, "road", roadIndex++), "Regional river crossing",
                    "fantasy_neutral.road.dirt", roadKind == MapWayKind::Trail ? 0.55f : 0.82f, crossing);
                if (approach.size() >= 2)
                {
                    auto previousApproachCells = vectorApproach ? map.cells : std::vector<MapCellModel>{};
                    mapGenV2RasterizeRoad(map, approach, roadKind);
                    mapGenV2AddPathFeature(
                        map,
                        roadKind == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road,
                        mapGenV2FeatureId(options.seed, "road", roadIndex++), "Bridge approach",
                        "fantasy_neutral.road.dirt", roadKind == MapWayKind::Trail ? 0.55f : 0.76f, approach);
                    if (vectorApproach)
                        map.generation.graph.features.back().points.back() = roadAnchor;
                    if (options.generatorVersion >= 6)
                        mapGenV5SmoothWorldRoadPathFeature(map, map.generation.graph.features.back(), 3);
                    if (vectorApproach)
                    {
                        mapGenV7JoinFirstExistingRoad(map.generation.graph.features.back(), map.generation.graph,
                                                      &approachTargetIds);
                        mapGenV7CommitFittedRoadProjection(map, map.generation.graph.features.back(),
                                                           std::move(previousApproachCells), roadKind);
                    }
                }
                if (options.generatorVersion >= 8 && !oppositeApproach.cells.empty())
                {
                    if (oppositeApproach.cells.size() == 1)
                        oppositeApproach.cells.push_back(oppositeApproach.cells.front());
                    auto previousCells = map.cells;
                    mapGenV2RasterizeRoad(map, oppositeApproach.cells, roadKind);
                    mapGenV2AddPathFeature(map,
                                           roadKind == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail
                                                                         : MapGeneratedFeatureKind::Road,
                                           mapGenV2FeatureId(options.seed, "road", roadIndex++),
                                           "Far-bank bridge approach", "fantasy_neutral.road.dirt",
                                           roadKind == MapWayKind::Trail ? .55f : .76f, oppositeApproach.cells);
                    auto &feature = map.generation.graph.features.back();
                    feature.points.back() = oppositeApproach.roadAnchor;
                    mapGenV5SmoothWorldRoadPathFeature(map, feature, 3);
                    mapGenV7JoinFirstExistingRoad(feature, map.generation.graph, &approachTargetIds);
                    mapGenV7CommitFittedRoadProjection(map, feature, std::move(previousCells), roadKind);
                }
            }
        }
    }
}

