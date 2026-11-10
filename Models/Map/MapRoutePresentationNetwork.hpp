#pragma once

#include "Models/Map/MapRoutePresentationTypes.hpp"

namespace gm::map_route_presentation::detail
{
struct Cell
{
    int x = 0;
    int y = 0;
};

inline bool operator==(Cell left, Cell right)
{
    return left.x == right.x && left.y == right.y;
}

inline int indexOf(const MapModel &map, int x, int y)
{
    return y * map.width + x;
}

inline Cell cellOf(const MapModel &map, int index)
{
    return {index % map.width, index / map.width};
}

inline bool validGrid(const MapModel &map)
{
    if (map.width <= 0 || map.height <= 0)
        return false;
    const std::uint64_t cells = static_cast<std::uint64_t>(map.width) *
                                static_cast<std::uint64_t>(map.height);
    return cells <= 16'777'216ull;
}

inline bool roadLike(MapWayKind kind)
{
    return kind == MapWayKind::Trail || kind == MapWayKind::DirtRoad ||
           kind == MapWayKind::StoneRoad ||
           kind == MapWayKind::DungeonCorridor ||
           kind == MapWayKind::CaveTunnel;
}

inline bool waterLike(const MapCellModel *cell)
{
    return cell && (cell->kind == MapTileKind::Water ||
                    cell->wayKind == MapWayKind::River ||
                    cell->wayKind == MapWayKind::Canal);
}

inline bool blockedTerrain(const MapCellModel *cell)
{
    if (!cell)
        return false;
    if (cell->movementCost >= 99)
        return true;
    switch (cell->kind)
    {
    case MapTileKind::Wall:
    case MapTileKind::Pit:
    case MapTileKind::Chasm:
    case MapTileKind::Lava:
        return true;
    default:
        return false;
    }
}

inline void rasterizePolyline(const MapModel &map,
                              const std::vector<MapFeaturePoint> &points,
                              std::vector<std::uint8_t> &mask)
{
    if (points.empty())
        return;
    auto mark = [&](float x, float y) {
        const int cellX = static_cast<int>(std::floor(x));
        const int cellY = static_cast<int>(std::floor(y));
        if (isMapCellInside(map, cellX, cellY))
            mask[static_cast<std::size_t>(indexOf(map, cellX, cellY))] = 1u;
    };
    mark(points.front().x, points.front().y);
    for (std::size_t pointIndex = 1; pointIndex < points.size(); ++pointIndex)
    {
        const MapFeaturePoint &a = points[pointIndex - 1];
        const MapFeaturePoint &b = points[pointIndex];
        if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
            !std::isfinite(b.x) || !std::isfinite(b.y))
            continue;
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const int steps = std::max(1, static_cast<int>(std::ceil(
            std::max(std::abs(dx), std::abs(dy)) * 4.0f)));
        for (int step = 0; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            mark(a.x + dx * t, a.y + dy * t);
        }
    }
}

struct Network
{
    std::vector<std::uint8_t> cells;
    std::vector<std::uint8_t> bridges;
    std::vector<int> components;
    int componentCount = 0;
};

inline Network buildNetwork(const MapModel &map, const BuildOptions &options)
{
    const std::size_t count = static_cast<std::size_t>(map.width * map.height);
    Network network;
    network.cells.assign(count, 0u);
    network.bridges.assign(count, 0u);
    network.components.assign(count, -1);

    if (options.includeRasterNetwork)
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                const std::size_t index = static_cast<std::size_t>(indexOf(map, x, y));
                if (cell && cell->kind == MapTileKind::Bridge)
                    network.bridges[index] = 1u;
            }
    const auto featureAllowed = [&](const MapGeneratedFeature &feature) {
        return !options.allowedGeneratedFeatureIds ||
               std::find(options.allowedGeneratedFeatureIds->begin(),
                         options.allowedGeneratedFeatureIds->end(),
                         feature.id) != options.allowedGeneratedFeatureIds->end();
    };
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (featureAllowed(feature) &&
            feature.kind == MapGeneratedFeatureKind::Bridge)
            rasterizePolyline(map, feature.points, network.bridges);

    if (options.includeRasterNetwork)
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                const std::size_t index = static_cast<std::size_t>(indexOf(map, x, y));
                if (cell && (cell->kind == MapTileKind::Road || roadLike(cell->wayKind)))
                    network.cells[index] = 1u;
            }
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (featureAllowed(feature) &&
            (feature.kind == MapGeneratedFeatureKind::Road ||
             feature.kind == MapGeneratedFeatureKind::Trail))
            rasterizePolyline(map, feature.points, network.cells);

    for (std::size_t index = 0; index < count; ++index)
    {
        if (network.bridges[index])
            network.cells[index] = 1u;
        const Cell coordinate = cellOf(map, static_cast<int>(index));
        if (waterLike(mapCellAt(map, coordinate.x, coordinate.y)) &&
            !network.bridges[index])
            network.cells[index] = 0u;
    }

    constexpr std::array<Cell, 8> neighbors{{
        {0, -1}, {1, 0}, {0, 1}, {-1, 0},
        {1, -1}, {1, 1}, {-1, 1}, {-1, -1}}};
    std::deque<int> queue;
    for (int start = 0; start < static_cast<int>(count); ++start)
    {
        if (!network.cells[static_cast<std::size_t>(start)] ||
            network.components[static_cast<std::size_t>(start)] >= 0)
            continue;
        const int component = network.componentCount++;
        network.components[static_cast<std::size_t>(start)] = component;
        queue.push_back(start);
        while (!queue.empty())
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
                if (!network.cells[static_cast<std::size_t>(next)] ||
                    network.components[static_cast<std::size_t>(next)] >= 0)
                    continue;
                network.components[static_cast<std::size_t>(next)] = component;
                queue.push_back(next);
            }
        }
    }
    return network;
}

struct ConnectorSearch
{
    std::vector<int> parent;
    std::vector<int> distance;
    std::vector<int> targetByComponent;
};

inline bool terrainPassable(const MapModel &map,
                            const Network &network,
                            int index)
{
    const Cell coordinate = cellOf(map, index);
    const MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
    if (blockedTerrain(cell))
        return false;
    return !waterLike(cell) || network.bridges[static_cast<std::size_t>(index)] != 0u;
}

inline ConnectorSearch findConnectors(const MapModel &map,
                                      const Network &network,
                                      int origin,
                                      int maximumSteps)
{
    const std::size_t count = network.cells.size();
    ConnectorSearch search;
    search.parent.assign(count, -1);
    search.distance.assign(count, -1);
    search.targetByComponent.assign(static_cast<std::size_t>(network.componentCount), -1);
    if (origin < 0 || origin >= static_cast<int>(count))
        return search;

    constexpr std::array<Cell, 4> neighbors{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
    std::deque<int> queue;
    search.distance[static_cast<std::size_t>(origin)] = 0;
    queue.push_back(origin);
    while (!queue.empty())
    {
        const int current = queue.front();
        queue.pop_front();
        const int distance = search.distance[static_cast<std::size_t>(current)];
        if (network.cells[static_cast<std::size_t>(current)])
        {
            const int component = network.components[static_cast<std::size_t>(current)];
            int &target = search.targetByComponent[static_cast<std::size_t>(component)];
            if (target < 0 || distance < search.distance[static_cast<std::size_t>(target)] ||
                (distance == search.distance[static_cast<std::size_t>(target)] && current < target))
                target = current;
        }
        if (distance >= maximumSteps)
            continue;
        const Cell from = cellOf(map, current);
        for (const Cell delta : neighbors)
        {
            const int nextX = from.x + delta.x;
            const int nextY = from.y + delta.y;
            if (!isMapCellInside(map, nextX, nextY))
                continue;
            const int next = indexOf(map, nextX, nextY);
            if (search.distance[static_cast<std::size_t>(next)] >= 0 ||
                !terrainPassable(map, network, next))
                continue;
            search.distance[static_cast<std::size_t>(next)] = distance + 1;
            search.parent[static_cast<std::size_t>(next)] = current;
            queue.push_back(next);
        }
    }
    return search;
}

inline std::vector<int> pathFromOrigin(int target, const ConnectorSearch &search)
{
    std::vector<int> reversed;
    for (int current = target; current >= 0;
         current = search.parent[static_cast<std::size_t>(current)])
    {
        reversed.push_back(current);
        if (search.parent[static_cast<std::size_t>(current)] < 0)
            break;
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}
} // namespace gm::map_route_presentation::detail

