#pragma once

#include "Models/Map/MapRoutePresentationPathfinding.hpp"

namespace gm::map_route_presentation
{
inline Path build(const MapModel &map, const MapRouteModel &route,
                  const BuildOptions &options = {})
{
    Path result;
    const MapFeaturePoint rawStart{static_cast<float>(route.startX) + 0.5f,
                                   static_cast<float>(route.startY) + 0.5f};
    const MapFeaturePoint rawEnd{static_cast<float>(route.endX) + 0.5f,
                                 static_cast<float>(route.endY) + 0.5f};
    if (!detail::validGrid(map))
    {
        result.points = {rawStart, rawEnd};
        return result;
    }

    const int startX = std::clamp(route.startX, 0, map.width - 1);
    const int startY = std::clamp(route.startY, 0, map.height - 1);
    const int endX = std::clamp(route.endX, 0, map.width - 1);
    const int endY = std::clamp(route.endY, 0, map.height - 1);
    const int start = detail::indexOf(map, startX, startY);
    const int goal = detail::indexOf(map, endX, endY);
    if (start == goal)
    {
        result.source = Source::TerrainFallback;
        result.points = {{static_cast<float>(startX) + 0.5f,
                          static_cast<float>(startY) + 0.5f}};
        return result;
    }

    const detail::Network network = detail::buildNetwork(map, options);
    if (network.componentCount > 0)
    {
        const int maximumConnectorSteps = std::clamp(
            std::max(map.width, map.height) / 6, 3, 12);
        const detail::ConnectorSearch fromStart = detail::findConnectors(
            map, network, start, maximumConnectorSteps);
        const detail::ConnectorSearch fromGoal = detail::findConnectors(
            map, network, goal, maximumConnectorSteps);
        int chosenComponent = -1;
        int chosenScore = (std::numeric_limits<int>::max)();
        for (int component = 0; component < network.componentCount; ++component)
        {
            const int startTarget = fromStart.targetByComponent[static_cast<std::size_t>(component)];
            const int goalTarget = fromGoal.targetByComponent[static_cast<std::size_t>(component)];
            if (startTarget < 0 || goalTarget < 0)
                continue;
            const int score = fromStart.distance[static_cast<std::size_t>(startTarget)] +
                              fromGoal.distance[static_cast<std::size_t>(goalTarget)];
            if (score < chosenScore || (score == chosenScore && component < chosenComponent))
            {
                chosenScore = score;
                chosenComponent = component;
            }
        }
        if (chosenComponent >= 0)
        {
            const int startTarget = fromStart.targetByComponent[static_cast<std::size_t>(chosenComponent)];
            const int goalTarget = fromGoal.targetByComponent[static_cast<std::size_t>(chosenComponent)];
            const std::vector<int> startConnector = detail::pathFromOrigin(startTarget, fromStart);
            const std::vector<int> onNetwork = detail::networkPath(map, network, startTarget, goalTarget);
            std::vector<int> goalConnector = detail::pathFromOrigin(goalTarget, fromGoal);
            std::reverse(goalConnector.begin(), goalConnector.end());
            if (!onNetwork.empty())
            {
                std::vector<int> combined;
                combined.reserve(startConnector.size() + onNetwork.size() + goalConnector.size());
                detail::appendUnique(combined, startConnector);
                detail::appendUnique(combined, onNetwork);
                detail::appendUnique(combined, goalConnector);
                result.source = Source::RoadNetwork;
                result.points = detail::pointsFromCells(map, combined, &network);
                return result;
            }
        }
    }

    const std::vector<int> safeTerrain = detail::terrainPath(map, network, start, goal);
    if (!safeTerrain.empty())
    {
        result.source = Source::TerrainFallback;
        result.points = detail::pointsFromCells(map, safeTerrain, &network);
        return result;
    }

    // This final line is deliberately classified as a direct fallback. Every
    // renderer draws it dashed instead of presenting an unverified crossing as
    // a real road. This case is expected only for disconnected islands or
    // malformed legacy endpoints.
    result.source = Source::DirectFallback;
    result.points = {
        {static_cast<float>(startX) + 0.5f, static_cast<float>(startY) + 0.5f},
        {static_cast<float>(endX) + 0.5f, static_cast<float>(endY) + 0.5f}};
    return result;
}

inline MapFeaturePoint pointAtFraction(const std::vector<MapFeaturePoint> &points,
                                       float fraction)
{
    if (points.empty())
        return {};
    if (points.size() == 1)
        return points.front();
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    float total = 0.0f;
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const float dx = points[index].x - points[index - 1].x;
        const float dy = points[index].y - points[index - 1].y;
        total += std::sqrt(dx * dx + dy * dy);
    }
    if (total <= 0.0001f)
        return points.front();
    const float target = total * fraction;
    float travelled = 0.0f;
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const MapFeaturePoint &a = points[index - 1];
        const MapFeaturePoint &b = points[index];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (travelled + length >= target && length > 0.0001f)
        {
            const float local = (target - travelled) / length;
            return {a.x + dx * local, a.y + dy * local};
        }
        travelled += length;
    }
    return points.back();
}

inline std::vector<MapFeaturePoint> prefixAtFraction(
    const std::vector<MapFeaturePoint> &points,
    float fraction)
{
    if (points.empty())
        return {};
    if (fraction <= 0.0f)
        return {points.front()};
    if (fraction >= 1.0f || points.size() == 1)
        return points;
    const MapFeaturePoint end = pointAtFraction(points, fraction);
    float total = 0.0f;
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const float dx = points[index].x - points[index - 1].x;
        const float dy = points[index].y - points[index - 1].y;
        total += std::sqrt(dx * dx + dy * dy);
    }
    const float target = total * std::clamp(fraction, 0.0f, 1.0f);
    float travelled = 0.0f;
    std::vector<MapFeaturePoint> prefix{points.front()};
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const float dx = points[index].x - points[index - 1].x;
        const float dy = points[index].y - points[index - 1].y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (travelled + length >= target)
        {
            if (prefix.back().x != end.x || prefix.back().y != end.y)
                prefix.push_back(end);
            break;
        }
        prefix.push_back(points[index]);
        travelled += length;
    }
    return prefix;
}

inline bool crossesUnbridgedWater(const MapModel &map,
                                  const std::vector<MapFeaturePoint> &points)
{
    if (!detail::validGrid(map) || points.size() < 2)
        return false;
    const detail::Network network = detail::buildNetwork(map, {});
    for (std::size_t index = 1; index < points.size(); ++index)
    {
        const MapFeaturePoint &a = points[index - 1];
        const MapFeaturePoint &b = points[index];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const int steps = std::max(1, static_cast<int>(std::ceil(
            std::max(std::abs(dx), std::abs(dy)) * 5.0f)));
        for (int step = 0; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const int x = static_cast<int>(std::floor(a.x + dx * t));
            const int y = static_cast<int>(std::floor(a.y + dy * t));
            if (!isMapCellInside(map, x, y))
                continue;
            const int cellIndex = detail::indexOf(map, x, y);
            if (detail::waterLike(mapCellAt(map, x, y)) &&
                !network.bridges[static_cast<std::size_t>(cellIndex)])
                return true;
        }
    }
    return false;
}
} // namespace gm::map_route_presentation
