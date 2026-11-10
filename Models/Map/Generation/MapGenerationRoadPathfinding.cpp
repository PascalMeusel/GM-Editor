#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
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


std::vector<MapCellCoordinate> mapGenV2FindRoadPath(const MapModel &map, MapCellCoordinate start,
                                                    MapCellCoordinate goal, bool avoidNewWaterCrossings)
{
    struct Pending
    {
        int priority = 0;
        int index = 0;
        int arrivalCost = 0;
        bool operator<(const Pending &other) const
        {
            if (priority != other.priority)
                return priority > other.priority;
            return index > other.index;
        }
    };
    const int area = map.width * map.height;
    const int startIndex = mapCellIndex(map, start.x, start.y);
    const int goalIndex = mapCellIndex(map, goal.x, goal.y);
    if (!mapCellAt(map, start.x, start.y) || !mapCellAt(map, goal.x, goal.y))
        return {};
    const bool boundedPasses = gm::map_routing::usesBoundedMountainPassRouting(map);
    const bool terrainRoutes = map.generation.recipe.generatorVersion >= 7 &&
                               (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region);
    // A pass or river crossing can only continue in its entry direction.
    // A cheaper arrival from another direction must not discard a valid
    // crossing state. V7 also remembers dry-road arrival for bend cost; V6 dry
    // cells and older recipes retain their original state/tie ordering.
    const int stateStride = boundedPasses || terrainRoutes ? 9 : 1;
    const int freeDirection = stateStride == 9 ? 8 : 0;
    const int startState = startIndex * stateStride + freeDirection;
    std::vector<int> cost(static_cast<std::size_t>(area) * stateStride, std::numeric_limits<int>::max());
    std::vector<int> previous(static_cast<std::size_t>(area) * stateStride, -1);
    std::priority_queue<Pending> open;
    const int routeManhattanDistance = std::abs(goal.x - start.x) + std::abs(goal.y - start.y);
    // Long inter-regional routes may use a short perpendicular approach from
    // a coastal settlement. On a short city-to-city link, a fixed three-cell
    // exemption at both ends covers nearly the complete route and lets A*
    // publish a visible shoreline causeway. Scale the exemption down for such
    // links so their middle is still routed inland.
    const int coastalEndpointGrace = std::clamp(routeManhattanDistance / 8, 1, 3);
    const auto mountainField = boundedPasses ? gm::map_routing::buildMountainPassRoutingField(map)
                                             : gm::map_routing::MountainPassRoutingField{};
    struct CachedRiverTangent
    {
        bool ready = false;
        bool found = false;
        float x = 0.0f;
        float y = 0.0f;
    };
    std::vector<CachedRiverTangent> crossingTangents(terrainRoutes ? static_cast<std::size_t>(area) : 0);
    std::vector<CachedRiverTangent> nearbyTangents(terrainRoutes ? static_cast<std::size_t>(area) : 0);
    std::vector<int> oceanDistances(terrainRoutes ? static_cast<std::size_t>(area) : 0, -1);
    const auto riverTangent = [&](int index, bool crossing, float &x, float &y) {
        auto &entry = (crossing ? crossingTangents : nearbyTangents)[static_cast<std::size_t>(index)];
        if (!entry.ready)
        {
            entry.ready = true;
            entry.found = mapGenV2NearestFeatureTangent(
                map.generation.graph, MapGeneratedFeatureKind::River, static_cast<float>(index % map.width) + 0.5f,
                static_cast<float>(index / map.width) + 0.5f, crossing ? 2.0f : 2.4f, entry.x, entry.y);
        }
        if (entry.found)
        {
            x = entry.x;
            y = entry.y;
        }
        return entry.found;
    };
    cost[static_cast<std::size_t>(startState)] = 0;
    open.push({0, startState});
    int reachedGoalState = -1;
    const MapCellCoordinate directions[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
    while (!open.empty())
    {
        const int queuedCost = open.top().arrivalCost;
        const int currentState = open.top().index;
        const int current = currentState / stateStride;
        open.pop();
        if (terrainRoutes && queuedCost != cost[static_cast<std::size_t>(currentState)])
            continue;
        if (current == goalIndex)
        {
            reachedGoalState = currentState;
            break;
        }
        const int currentX = current % map.width;
        const int currentY = current / map.width;
        const MapCellModel *currentCell = mapCellAt(map, currentX, currentY);
        for (std::size_t directionIndex = 0; directionIndex < std::size(directions); ++directionIndex)
        {
            const auto direction = directions[directionIndex];
            const int nextX = currentX + direction.x;
            const int nextY = currentY + direction.y;
            const int nextIndex = mapCellIndex(map, nextX, nextY);
            const MapCellModel *next = mapCellAt(map, nextX, nextY);
            if (nextIndex < 0 || !next || (avoidNewWaterCrossings && next->kind == MapTileKind::Water))
                continue;
            const bool diagonal = direction.x != 0 && direction.y != 0;
            const auto mountainDirection = static_cast<std::uint16_t>(1u << directionIndex);
            const bool restrictedNext =
                terrainRoutes ||
                (boundedPasses && (next->kind == MapTileKind::Mountain ||
                                   (next->kind == MapTileKind::Water && mapWayKindIsWaterLike(next->wayKind))));
            const int nextState =
                nextIndex * stateStride + (restrictedNext ? static_cast<int>(directionIndex) : freeDirection);
            int traversal = mapGenV2RoadTraversalCost(*next);
            if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
                next->kind == MapTileKind::Mountain)
                traversal = 64;
            if (boundedPasses && next->kind == MapTileKind::Mountain)
            {
                if ((mountainField.crossingDirections[nextIndex] & mountainDirection) == 0)
                    continue;
                traversal = mountainField.crossingCost[nextIndex];
            }
            if (boundedPasses && currentCell && currentCell->kind == MapTileKind::Mountain)
            {
                if (currentState % stateStride != static_cast<int>(directionIndex))
                    continue;
            }
            if (boundedPasses && next->kind != MapTileKind::Mountain && mountainField.distance[nextIndex] <= 1 &&
                !mapGenWayKindIsRoadLike(next->wayKind) && nextIndex != goalIndex && nextIndex != startIndex &&
                (mountainField.approachDirections[nextIndex] & mountainDirection) == 0)
                continue;
            if (traversal >= 100000)
                continue;
            if (diagonal)
            {
                // A diagonal between two nominally dry cells can still cut
                // straight across the corner of an ocean cell.  With a wide
                // world-road spline that reads as a causeway through open
                // water even though the raster endpoints themselves are dry.
                // Keep both orthogonal shoulders routable; real waterways are
                // handled below and become explicit bridge cells/features.
                const MapCellModel *sideX = mapCellAt(map, nextX, currentY);
                const MapCellModel *sideY = mapCellAt(map, currentX, nextY);
                if (!sideX || !sideY || mapGenV2RoadTraversalCost(*sideX) >= 100000 ||
                    mapGenV2RoadTraversalCost(*sideY) >= 100000 || sideX->kind == MapTileKind::Water ||
                    sideY->kind == MapTileKind::Water || sideX->kind == MapTileKind::Bridge ||
                    sideY->kind == MapTileKind::Bridge)
                    continue;
                if (boundedPasses && next->kind != MapTileKind::Mountain &&
                    (!currentCell || currentCell->kind != MapTileKind::Mountain) &&
                    (sideX->kind == MapTileKind::Mountain || sideY->kind == MapTileKind::Mountain))
                    continue;
            }
            if (next->kind == MapTileKind::Water && mapWayKindIsWaterLike(next->wayKind))
            {
                float riverX = 0.0f;
                float riverY = 1.0f;
                if (terrainRoutes
                        ? riverTangent(nextIndex, true, riverX, riverY)
                        : mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::River,
                                                        static_cast<float>(nextX) + 0.5f,
                                                        static_cast<float>(nextY) + 0.5f, 2.0f, riverX, riverY))
                {
                    const float moveLength =
                        std::sqrt(static_cast<float>(direction.x * direction.x + direction.y * direction.y));
                    const float crossingDot = std::abs((direction.x * riverX + direction.y * riverY) / moveLength);
                    if (crossingDot > 0.72f)
                        continue;
                }
            }
            if (currentCell && currentCell->kind == MapTileKind::Water && mapWayKindIsWaterLike(currentCell->wayKind))
            {
                const int previousState = previous[static_cast<std::size_t>(currentState)];
                if (previousState >= 0)
                {
                    const int previousIndex = previousState / stateStride;
                    const int entryX = currentX - previousIndex % map.width;
                    const int entryY = currentY - previousIndex / map.width;
                    if (direction.x != entryX || direction.y != entryY)
                        continue;
                }
            }
            int overviewBias = 0;
            if (boundedPasses && next->kind != MapTileKind::Mountain && !mapGenWayKindIsRoadLike(next->wayKind))
                overviewBias += mountainField.distance[nextIndex] <= 1   ? 16
                                : mountainField.distance[nextIndex] == 2 ? 9
                                : mountainField.distance[nextIndex] == 3 ? 3
                                                                         : 0;
            if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World)
            {
                const int noiseScale = std::max(8, std::min(map.width, map.height) / 3);
                if (!terrainRoutes)
                    overviewBias += static_cast<int>(
                        std::floor(mapGenV2FractalNoise(map.generation.recipe.seed + 4721u, static_cast<float>(nextX),
                                                        static_cast<float>(nextY), noiseScale) *
                                   2.6f));
                if (mapGenWayKindIsRoadLike(next->wayKind))
                    overviewBias -= 1;
                const int fromStart = std::abs(nextX - start.x) + std::abs(nextY - start.y);
                const int fromGoal = std::abs(nextX - goal.x) + std::abs(nextY - goal.y);
                // A cartographic road is wider than its raster centerline. A
                // path that hugs the first dry coastal row therefore appears
                // to run through the sea after smoothing, even though every
                // visited cell is technically land. Keep ordinary world roads
                // one cell inland; a coastal settlement may still use a short
                // perpendicular approach because its endpoint is exempt.
                if (fromStart > coastalEndpointGrace && fromGoal > coastalEndpointGrace &&
                    next->kind != MapTileKind::Bridge)
                {
                    if (terrainRoutes && oceanDistances[static_cast<std::size_t>(nextIndex)] < 0)
                        oceanDistances[static_cast<std::size_t>(nextIndex)] =
                            mapGenV5WorldOpenWaterDistance(map, nextX, nextY, 2);
                    const int openWaterDistance = terrainRoutes ? oceanDistances[static_cast<std::size_t>(nextIndex)]
                                                                : mapGenV5WorldOpenWaterDistance(map, nextX, nextY, 2);
                    if (openWaterDistance == 1)
                        continue;
                    else if (openWaterDistance == 2)
                        overviewBias += map.generation.recipe.generatorVersion >= 11 ? 35 : 14;
                }
                if (fromStart > 4 && fromGoal > 4 && next->kind != MapTileKind::Water)
                {
                    float riverX = 0.0f;
                    float riverY = 0.0f;
                    if (terrainRoutes
                            ? riverTangent(nextIndex, false, riverX, riverY)
                            : mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::River,
                                                            static_cast<float>(nextX) + 0.5f,
                                                            static_cast<float>(nextY) + 0.5f, 2.4f, riverX, riverY))
                    {
                        const float moveLength =
                            std::sqrt(static_cast<float>(direction.x * direction.x + direction.y * direction.y));
                        const float parallel = std::abs(
                            (static_cast<float>(direction.x) * riverX + static_cast<float>(direction.y) * riverY) /
                            moveLength);
                        if (parallel > 0.72f)
                            overviewBias += 5;
                    }
                }
            }
            int stepCost = traversal + (diagonal ? 1 : 0) + std::max(-1, overviewBias);
            if (terrainRoutes)
            {
                // Precision removes the old 2/3-cost grid ties. Terrain and
                // grade determine the alignment; no decorative noise bends a
                // route across otherwise uniform ground. Existing corridors
                // attract junctions without making an enormous detour cheap.
                const int distance = diagonal ? 141 : 100;
                const int terrainFriction = next->kind == MapTileKind::Mountain ? traversal * 20
                                            : next->kind == MapTileKind::Water  ? 600
                                                                               : mapGenV7OverviewTerrainFriction(*next);
                const int grade = currentCell ? std::abs(next->elevation - currentCell->elevation) : 0;
                stepCost =
                    distance * (100 + terrainFriction) / 100 + grade * grade * 15 + std::max(0, overviewBias) * 16;
                if (mapGenWayKindIsRoadLike(next->wayKind))
                    stepCost = std::max(distance * 85 / 100, stepCost - 25);
                const int incoming = currentState % stateStride;
                if (incoming < 8 && incoming != static_cast<int>(directionIndex))
                {
                    const auto before = directions[incoming];
                    const float dot =
                        static_cast<float>(before.x * direction.x + before.y * direction.y) /
                        std::sqrt(static_cast<float>((before.x * before.x + before.y * before.y) *
                                                     (direction.x * direction.x + direction.y * direction.y)));
                    stepCost += static_cast<int>(std::lround((1.0f - dot) * 38.0f));
                }
            }
            const int nextCost = cost[static_cast<std::size_t>(currentState)] + stepCost;
            if (nextCost >= cost[static_cast<std::size_t>(nextState)])
                continue;
            cost[static_cast<std::size_t>(nextState)] = nextCost;
            previous[static_cast<std::size_t>(nextState)] = currentState;
            const int remainingX = std::abs(goal.x - nextX);
            const int remainingY = std::abs(goal.y - nextY);
            const int diagonalSteps = std::min(remainingX, remainingY);
            const int straightSteps = std::max(remainingX, remainingY) - diagonalSteps;
            const int heuristic =
                terrainRoutes ? diagonalSteps * 119 + straightSteps * 85 : diagonalSteps * 3 + straightSteps * 2;
            open.push({nextCost + heuristic, nextState, nextCost});
        }
    }
    if (reachedGoalState < 0)
        return {};
    std::vector<MapCellCoordinate> path;
    for (int state = reachedGoalState; state >= 0; state = previous[static_cast<std::size_t>(state)])
    {
        const int at = state / stateStride;
        path.push_back({at % map.width, at / map.width});
        if (state == startState)
            break;
    }
    std::reverse(path.begin(), path.end());
    if (boundedPasses && !gm::map_routing::mountainPassPathIsBounded(map, path, mountainField))
        return {};
    return path;
}

MapGenV7BridgeApproach mapGenV7ChooseBridgeApproach(const MapModel &map, const std::vector<MapCellCoordinate> &crossing)
{
    MapGenV7BridgeApproach best;
    if (crossing.size() < 2)
        return best;
    float bestLength = std::numeric_limits<float>::max();
    for (const auto bank : {crossing.front(), crossing.back()})
    {
        MapFeaturePoint anchor;
        if (!mapGenV7NearestDryRoadAnchor(map, {bank.x + 0.5f, bank.y + 0.5f}, anchor))
            continue;
        auto path = mapGenV2FindRoadPath(map, bank, {static_cast<int>(anchor.x), static_cast<int>(anchor.y)}, true);
        if (path.empty())
            continue;
        float length = std::hypot(anchor.x - (path.back().x + 0.5f), anchor.y - (path.back().y + 0.5f));
        for (std::size_t index = 1; index < path.size(); ++index)
            length += std::hypot(static_cast<float>(path[index].x - path[index - 1].x),
                                 static_cast<float>(path[index].y - path[index - 1].y));
        if (length < bestLength)
        {
            bestLength = length;
            best.cells = std::move(path);
            best.roadAnchor = anchor;
        }
    }
    return best;
}

void mapGenV2RasterizeRoad(MapModel &map, const std::vector<MapCellCoordinate> &path, MapWayKind kind)
{
    for (const MapCellCoordinate &coordinate : path)
    {
        MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
        if (!cell)
            continue;
        // Every water cell in a diagonal river span, including corner-gap
        // water without a River way tag, must become a traversable bridge.
        // Do not route this through mapGenSetWay: its generic swamp handling
        // intentionally creates boardwalks, which would incorrectly publish
        // ordinary wetland road cells as river-bridge features here.
        if (cell->kind == MapTileKind::Water)
        {
            const int waterwayElevation = cell->elevation;
            mapGenSetCell(map, coordinate.x, coordinate.y, MapTileKind::Bridge);
            cell = mapCellAt(map, coordinate.x, coordinate.y);
            if (!cell)
                continue;
            cell->elevation = waterwayElevation;
        }
        else if (cell->kind == MapTileKind::Mountain)
        {
            // The cost-aware planner may deliberately choose the narrowest
            // mountain crossing. Project that choice as an actual stone pass,
            // rather than leaving an apparently routed but movement-blocked
            // mountain cell in the semantic grid.
            const int passElevation = cell->elevation;
            mapGenSetCell(map, coordinate.x, coordinate.y, MapTileKind::Stone);
            cell = mapCellAt(map, coordinate.x, coordinate.y);
            if (!cell)
                continue;
            cell->elevation = passElevation;
        }
        cell->wayKind = kind;
    }
}

