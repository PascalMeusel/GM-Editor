#pragma once

#include "Models/Map/MapRoutePresentationNetwork.hpp"

namespace gm::map_route_presentation::detail
{
inline std::vector<int> networkPath(const MapModel &map,
                                    const Network &network,
                                    int start,
                                    int goal)
{
    const std::size_t count = network.cells.size();
    std::vector<int> parent(count, -1);
    std::vector<std::uint8_t> visited(count, 0u);
    std::deque<int> queue;
    visited[static_cast<std::size_t>(start)] = 1u;
    queue.push_back(start);
    constexpr std::array<Cell, 8> neighbors{{
        {0, -1}, {1, 0}, {0, 1}, {-1, 0},
        {1, -1}, {1, 1}, {-1, 1}, {-1, -1}}};
    while (!queue.empty() && !visited[static_cast<std::size_t>(goal)])
    {
        const int current = queue.front();
        queue.pop_front();
        const Cell from = cellOf(map, current);
        for (const Cell delta : neighbors)
        {
            const int nextX = from.x + delta.x;
            const int nextY = from.y + delta.y;
            if (!isMapCellInside(map, nextX, nextY))
                continue;
            const int next = indexOf(map, nextX, nextY);
            if (visited[static_cast<std::size_t>(next)] ||
                !network.cells[static_cast<std::size_t>(next)])
                continue;
            visited[static_cast<std::size_t>(next)] = 1u;
            parent[static_cast<std::size_t>(next)] = current;
            queue.push_back(next);
        }
    }
    if (!visited[static_cast<std::size_t>(goal)])
        return {};
    std::vector<int> reversed;
    for (int current = goal; current >= 0; current = parent[static_cast<std::size_t>(current)])
    {
        reversed.push_back(current);
        if (current == start)
            break;
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

inline std::vector<int> terrainPath(const MapModel &map,
                                    const Network &network,
                                    int start,
                                    int goal)
{
    const std::size_t count = network.cells.size();
    if (!terrainPassable(map, network, start) || !terrainPassable(map, network, goal))
        return {};
    std::vector<int> parent(count, -1);
    std::vector<std::uint8_t> visited(count, 0u);
    std::deque<int> queue;
    visited[static_cast<std::size_t>(start)] = 1u;
    queue.push_back(start);
    constexpr std::array<Cell, 8> neighbors{{
        {0, -1}, {1, 0}, {0, 1}, {-1, 0},
        {1, -1}, {1, 1}, {-1, 1}, {-1, -1}}};
    while (!queue.empty() && !visited[static_cast<std::size_t>(goal)])
    {
        const int current = queue.front();
        queue.pop_front();
        const Cell from = cellOf(map, current);
        for (const Cell delta : neighbors)
        {
            const int nextX = from.x + delta.x;
            const int nextY = from.y + delta.y;
            if (!isMapCellInside(map, nextX, nextY))
                continue;
            const int next = indexOf(map, nextX, nextY);
            if (visited[static_cast<std::size_t>(next)] ||
                !terrainPassable(map, network, next))
                continue;
            visited[static_cast<std::size_t>(next)] = 1u;
            parent[static_cast<std::size_t>(next)] = current;
            queue.push_back(next);
        }
    }
    if (!visited[static_cast<std::size_t>(goal)])
        return {};
    std::vector<int> reversed;
    for (int current = goal; current >= 0; current = parent[static_cast<std::size_t>(current)])
    {
        reversed.push_back(current);
        if (current == start)
            break;
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

inline void appendUnique(std::vector<int> &target, const std::vector<int> &source)
{
    for (const int index : source)
        if (target.empty() || target.back() != index)
            target.push_back(index);
}

inline std::vector<MapFeaturePoint> pointsFromCells(const MapModel &map,
                                                    const std::vector<int> &cells,
                                                    const Network *network = nullptr)
{
    std::vector<MapFeaturePoint> points;
    std::vector<int> pointCells;
    points.reserve(cells.size());
    pointCells.reserve(cells.size());
    for (const int index : cells)
    {
        const Cell cell = cellOf(map, index);
        const MapFeaturePoint point{static_cast<float>(cell.x) + 0.5f,
                                    static_cast<float>(cell.y) + 0.5f};
        if (!points.empty() && points.back().x == point.x && points.back().y == point.y)
            continue;
        if (points.size() >= 2)
        {
            const MapFeaturePoint &a = points[points.size() - 2];
            const MapFeaturePoint &b = points.back();
            const float firstX = b.x - a.x;
            const float firstY = b.y - a.y;
            const float secondX = point.x - b.x;
            const float secondY = point.y - b.y;
            const int middleCell = pointCells.back();
            const bool crossingAnchor = network != nullptr && middleCell >= 0 &&
                                        static_cast<std::size_t>(middleCell) <
                                            network->bridges.size() &&
                                        network->bridges[static_cast<std::size_t>(middleCell)] != 0u;
            // A bridge may lie on an otherwise perfectly straight or diagonal
            // run. Removing that point makes the rendered route lose its
            // explicit valid-crossing anchor even though the raster BFS used
            // it. Keep all declared bridge cells during simplification.
            if (!crossingAnchor &&
                std::abs(firstX * secondY - firstY * secondX) < 0.0001f &&
                firstX * secondX + firstY * secondY > 0.0f)
            {
                points.back() = point;
                pointCells.back() = index;
                continue;
            }
        }
        points.push_back(point);
        pointCells.push_back(index);
    }
    return points;
}
} // namespace gm::map_route_presentation::detail

