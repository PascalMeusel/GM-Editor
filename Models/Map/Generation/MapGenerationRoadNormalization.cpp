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


void mapGenNormalizeGeneratedArtContractWays(MapModel &map)
{
    for (MapCellModel &cell : map.cells)
    {
        if (cell.kind == MapTileKind::Bridge && cell.wayKind == MapWayKind::Trail)
            cell.wayKind = MapWayKind::DirtRoad;
    }

    constexpr MapCellCoordinate offsets[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !mapGenIsStructureLike(cell->kind))
                continue;
            for (const MapCellCoordinate &offset : offsets)
            {
                MapCellModel *neighbor = mapCellAt(map, x + offset.x, y + offset.y);
                if (neighbor && neighbor->wayKind == MapWayKind::Trail)
                    neighbor->wayKind = MapWayKind::DirtRoad;
            }
        }
    }

    std::vector<bool> visited(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const int startIndex = y * map.width + x;
            const MapCellModel *start = mapCellAt(map, x, y);
            if (!start || start->kind != MapTileKind::Bridge || visited[static_cast<size_t>(startIndex)])
                continue;

            bool hasCrossingContext = false;
            std::vector<MapCellCoordinate> componentCells;
            std::vector<MapCellCoordinate> approachEndpoints;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visited[static_cast<size_t>(startIndex)] = true;

            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                componentCells.push_back(current);

                const MapCellCoordinate neighbors[] = {{current.x, current.y - 1},
                                                       {current.x + 1, current.y},
                                                       {current.x, current.y + 1},
                                                       {current.x - 1, current.y}};
                for (const MapCellCoordinate &neighbor : neighbors)
                {
                    if (!isMapCellInside(map, neighbor.x, neighbor.y))
                        continue;

                    const int neighborIndex = neighbor.y * map.width + neighbor.x;
                    const MapCellModel *neighborCell = mapCellAt(map, neighbor.x, neighbor.y);
                    if (!neighborCell)
                        continue;

                    if (neighborCell->kind == MapTileKind::Bridge)
                    {
                        if (!visited[static_cast<size_t>(neighborIndex)])
                        {
                            visited[static_cast<size_t>(neighborIndex)] = true;
                            pending.push(neighbor);
                        }
                        continue;
                    }

                    if (mapGenBridgeApproachLikeCell(*neighborCell))
                    {
                        const auto found = std::find_if(approachEndpoints.begin(), approachEndpoints.end(),
                                                        [&](const MapCellCoordinate &existing) {
                                                            return existing.x == neighbor.x && existing.y == neighbor.y;
                                                        });
                        if (found == approachEndpoints.end())
                            approachEndpoints.push_back(neighbor);
                    }
                    if (mapGenBridgeCrossingContextCell(*neighborCell))
                        hasCrossingContext = true;
                }
            }

            if (hasCrossingContext && approachEndpoints.size() >= 2)
                continue;

            for (const MapCellCoordinate &bridgeCell : componentCells)
            {
                MapCellModel *cell = mapCellAt(map, bridgeCell.x, bridgeCell.y);
                if (!cell || cell->kind != MapTileKind::Bridge)
                    continue;
                MapWayKind wayKind = cell->wayKind;
                if (wayKind == MapWayKind::None || wayKind == MapWayKind::Trail)
                    wayKind = MapWayKind::DirtRoad;
                mapGenSetCell(map, bridgeCell.x, bridgeCell.y, MapTileKind::Road);
                cell = mapCellAt(map, bridgeCell.x, bridgeCell.y);
                if (cell)
                    cell->wayKind = wayKind;
            }
        }
    }
}

void mapGenClearIsolatedWays(MapModel &map, MapWayKind kind)
{
    std::vector<MapCellCoordinate> isolated;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->wayKind != kind)
                continue;

            bool connected = false;
            for (int dy = -1; dy <= 1 && !connected; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0)
                        continue;
                    const MapCellModel *neighbor = mapCellAt(map, x + dx, y + dy);
                    if (neighbor && mapWayKindsConnect(kind, neighbor->wayKind))
                    {
                        connected = true;
                        break;
                    }
                }
            }
            if (!connected)
                isolated.push_back({x, y});
        }
    }

    for (const MapCellCoordinate &cellCoordinate : isolated)
    {
        MapCellModel *cell = mapCellAt(map, cellCoordinate.x, cellCoordinate.y);
        if (cell && cell->wayKind == kind)
            cell->wayKind = MapWayKind::None;
    }
}

void mapGenBuildRoutes(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    const int density = mapGenClampPercent(options.routeDensity);
    if (density == 0 || options.roadNetwork == MapRoadNetworkMode::None)
        return;

    const MapWayKind roadKind = mapGenGeneratedRoadKind(options);
    MapCellCoordinate representativeStart{-1, -1};
    MapCellCoordinate representativeEnd{-1, -1};
    if (options.roadNetwork == MapRoadNetworkMode::Grid)
    {
        const int spacing = std::max(4, 10 - density / 14);
        for (int x = spacing / 2; x < map.width; x += spacing + mapGenRandomBetween(rng, -1, 2))
        {
            const int shiftedX = std::clamp(x + mapGenRandomBetween(rng, -1, 1), 1, std::max(1, map.width - 2));
            mapGenDrawWay(map, shiftedX, 1, shiftedX, map.height - 2, roadKind, rng, 0, false, MapTileKind::Empty, 0,
                          false);
            if (representativeStart.x < 0)
            {
                representativeStart = {shiftedX, 1};
                representativeEnd = {shiftedX, map.height - 2};
            }
        }
        for (int y = spacing / 2; y < map.height; y += spacing + mapGenRandomBetween(rng, -1, 2))
        {
            const int shiftedY = std::clamp(y + mapGenRandomBetween(rng, -1, 1), 1, std::max(1, map.height - 2));
            mapGenDrawWay(map, 1, shiftedY, map.width - 2, shiftedY, roadKind, rng, 0, false, MapTileKind::Empty, 0,
                          false);
        }
    }
    else
    {
        const int routeCount =
            options.roadNetwork == MapRoadNetworkMode::Sparse ? std::max(1, density / 35) : std::max(1, density / 22);
        for (int i = 0; i < routeCount; ++i)
        {
            const int startX = mapGenRandomBetween(rng, 1, std::max(1, map.width / 5));
            const int startYMin =
                options.roadNetwork == MapRoadNetworkMode::Connected ? std::max(1, map.height / 5) : 1;
            const int startYMax = options.roadNetwork == MapRoadNetworkMode::Connected
                                      ? std::max(startYMin, map.height - map.height / 5 - 1)
                                      : map.height - 2;
            const int startY = mapGenRandomBetween(rng, startYMin, startYMax);
            int yOffset = mapGenRandomBetween(rng, -map.height / 3, map.height / 3);
            if (options.roadNetwork == MapRoadNetworkMode::Connected)
            {
                const int minDelta = std::max(3, map.height / 3);
                int direction = yOffset < 0 ? -1 : 1;
                if (startY - minDelta <= 1)
                    direction = 1;
                else if (startY + minDelta >= map.height - 2)
                    direction = -1;
                if (std::abs(yOffset) < minDelta)
                    yOffset = direction * minDelta;
            }
            const int waypointX = mapGenRandomBetween(rng, std::max(1, map.width / 3), std::max(1, map.width * 2 / 3));
            int waypointY = std::clamp(startY + yOffset / 2 + mapGenRandomBetween(rng, -map.height / 5, map.height / 5),
                                       1, map.height - 2);
            if (options.roadNetwork == MapRoadNetworkMode::Connected &&
                std::abs(waypointY - startY) < std::max(2, map.height / 6))
                waypointY =
                    std::clamp(startY + (yOffset < 0 ? -1 : 1) * std::max(2, map.height / 6), 1, map.height - 2);
            const int endX = mapGenRandomBetween(rng, std::max(1, map.width * 4 / 5), std::max(1, map.width - 2));
            const int endY = std::clamp(startY + yOffset, 1, map.height - 2);
            if (representativeStart.x < 0)
            {
                representativeStart = {startX, startY};
                representativeEnd = {endX, endY};
            }
            if (options.roadNetwork == MapRoadNetworkMode::Connected)
            {
                const int bendX = std::clamp((waypointX + endX) / 2, 1, map.width - 2);
                int bendY = std::clamp(endY - (yOffset < 0 ? -1 : 1) * std::max(2, map.height / 7), 1, map.height - 2);
                if (bendY == waypointY)
                    bendY = std::clamp(bendY + (yOffset < 0 ? 1 : -1) * 2, 1, map.height - 2);
                mapGenDrawWay(map, startX, startY, waypointX, waypointY, roadKind, rng, 18);
                mapGenDrawWay(map, waypointX, waypointY, bendX, bendY, roadKind, rng, 18);
                mapGenDrawWay(map, bendX, bendY, endX, endY, roadKind, rng, 18);
            }
            else
            {
                mapGenDrawWay(map, startX, startY, waypointX, waypointY, roadKind, rng, 10);
                mapGenDrawWay(map, waypointX, waypointY, endX, endY, roadKind, rng, 10);
            }
        }
    }

    MapRouteModel route;
    route.name = "Generated Route";
    route.fromMapId = map.metadata.id;
    route.startX = representativeStart.x >= 0 ? representativeStart.x : 0;
    route.startY = representativeStart.y >= 0 ? representativeStart.y : map.height / 2;
    route.endX = representativeEnd.x >= 0 ? representativeEnd.x : map.width - 1;
    route.endY = representativeEnd.y >= 0 ? representativeEnd.y : map.height / 2;
    route.distanceMiles = static_cast<float>(std::max(1, map.width / 2));
    route.terrain = mapWayKindLabel(roadKind);
    route.notes = "Representative travel route generated from the semantic way network.";
    map.routes.push_back(route);
}

