#pragma once

#include "Models/Map/MapOverviewPoliticalProjection.hpp"

namespace gm::map_render
{
namespace detail
{
struct PoliticalGridEdge
{
    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;
    int firstOwner = -1;
    int secondOwner = -1;
};

inline float politicalPointSegmentDistanceSquared(
    const MapFeaturePoint &point,
    const MapFeaturePoint &start,
    const MapFeaturePoint &end)
{
    const float deltaX = end.x - start.x;
    const float deltaY = end.y - start.y;
    const float lengthSquared = deltaX * deltaX + deltaY * deltaY;
    if (lengthSquared <= 0.000001f)
    {
        const float dx = point.x - start.x;
        const float dy = point.y - start.y;
        return dx * dx + dy * dy;
    }
    const float amount = std::clamp(
        ((point.x - start.x) * deltaX + (point.y - start.y) * deltaY) /
            lengthSquared,
        0.0f, 1.0f);
    const float dx = point.x - (start.x + deltaX * amount);
    const float dy = point.y - (start.y + deltaY * amount);
    return dx * dx + dy * dy;
}

inline void simplifyPoliticalRunRecursive(
    const std::vector<MapFeaturePoint> &source,
    std::size_t first,
    std::size_t last,
    float toleranceSquared,
    std::vector<std::uint8_t> &keep)
{
    if (last <= first + 1u)
        return;
    float maximumDistance = -1.0f;
    std::size_t farthest = first;
    for (std::size_t index = first + 1u; index < last; ++index)
    {
        const float distance = politicalPointSegmentDistanceSquared(
            source[index], source[first], source[last]);
        if (distance > maximumDistance)
        {
            maximumDistance = distance;
            farthest = index;
        }
    }
    if (maximumDistance <= toleranceSquared)
        return;
    keep[farthest] = 1u;
    simplifyPoliticalRunRecursive(source, first, farthest, toleranceSquared, keep);
    simplifyPoliticalRunRecursive(source, farthest, last, toleranceSquared, keep);
}

inline std::vector<MapFeaturePoint> simplifyAndSoftenPoliticalRun(
    const std::vector<MapFeaturePoint> &source)
{
    if (source.size() < 3u)
        return source;
    const bool closed = std::abs(source.front().x - source.back().x) < 0.001f &&
                        std::abs(source.front().y - source.back().y) < 0.001f;
    std::vector<MapFeaturePoint> simplified;
    if (closed)
    {
        simplified = source;
    }
    else
    {
        std::vector<std::uint8_t> keep(source.size(), 0u);
        keep.front() = 1u;
        keep.back() = 1u;
        simplifyPoliticalRunRecursive(source, 0u, source.size() - 1u,
                                      0.34f * 0.34f, keep);
        for (std::size_t index = 0u; index < source.size(); ++index)
            if (keep[index] != 0u)
                simplified.push_back(source[index]);
    }
    if (simplified.size() < 3u)
        return simplified;

    // One restrained Chaikin pass removes the cardinal raster staircase while
    // retaining junction endpoints and the political partition's identity.
    std::vector<MapFeaturePoint> softened;
    softened.reserve(simplified.size() * 2u);
    softened.push_back(simplified.front());
    for (std::size_t index = 1u; index < simplified.size(); ++index)
    {
        const MapFeaturePoint &a = simplified[index - 1u];
        const MapFeaturePoint &b = simplified[index];
        softened.push_back({a.x * 0.75f + b.x * 0.25f,
                            a.y * 0.75f + b.y * 0.25f});
        softened.push_back({a.x * 0.25f + b.x * 0.75f,
                            a.y * 0.25f + b.y * 0.75f});
    }
    softened.push_back(simplified.back());
    return softened;
}
} // namespace detail

inline std::vector<OverviewPoliticalBorderRun> buildOverviewPoliticalBorderRuns(
    const MapModel &map)
{
    std::vector<OverviewPoliticalBorderRun> result;
    const OverviewPoliticalOwnerProjection projection =
        buildOverviewPoliticalOwnerProjection(map);
    if (projection.territoryCount < 2 || projection.owners.empty())
        return result;

    const auto ownerAt = [&](int x, int y) {
        if (!isMapCellInside(map, x, y))
            return -1;
        return projection.owners[static_cast<std::size_t>(mapCellIndex(map, x, y))];
    };
    std::vector<detail::PoliticalGridEdge> edges;
    edges.reserve(static_cast<std::size_t>(map.width * map.height));
    const auto appendEdge = [&](int startX, int startY, int endX, int endY,
                                int firstOwner, int secondOwner) {
        if (firstOwner < 0 || secondOwner < 0 || firstOwner == secondOwner)
            return;
        if (secondOwner < firstOwner)
            std::swap(firstOwner, secondOwner);
        edges.push_back({startX, startY, endX, endY,
                         firstOwner, secondOwner});
    };
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            if (x + 1 < map.width)
                appendEdge(x + 1, y, x + 1, y + 1,
                           ownerAt(x, y), ownerAt(x + 1, y));
            if (y + 1 < map.height)
                appendEdge(x, y + 1, x + 1, y + 1,
                           ownerAt(x, y), ownerAt(x, y + 1));
        }
    }
    if (edges.empty())
        return result;

    const int endpointWidth = map.width + 1;
    const int endpointHeight = map.height + 1;
    const auto endpointKey = [endpointWidth](int x, int y) {
        return y * endpointWidth + x;
    };
    std::vector<std::vector<std::size_t>> adjacency(
        static_cast<std::size_t>(endpointWidth * endpointHeight));
    for (std::size_t edgeIndex = 0u; edgeIndex < edges.size(); ++edgeIndex)
    {
        const detail::PoliticalGridEdge &edge = edges[edgeIndex];
        adjacency[static_cast<std::size_t>(endpointKey(edge.startX, edge.startY))]
            .push_back(edgeIndex);
        adjacency[static_cast<std::size_t>(endpointKey(edge.endX, edge.endY))]
            .push_back(edgeIndex);
    }
    std::vector<std::uint8_t> used(edges.size(), 0u);
    const auto samePair = [&](const detail::PoliticalGridEdge &edge,
                              int firstOwner, int secondOwner) {
        return edge.firstOwner == firstOwner && edge.secondOwner == secondOwner;
    };
    const auto pairDegree = [&](int key, int firstOwner, int secondOwner) {
        int degree = 0;
        for (const std::size_t edgeIndex : adjacency[static_cast<std::size_t>(key)])
            if (samePair(edges[edgeIndex], firstOwner, secondOwner))
                ++degree;
        return degree;
    };
    const auto traceRun = [&](std::size_t startingEdge, int startingKey) {
        OverviewPoliticalBorderRun run;
        run.firstOwner = edges[startingEdge].firstOwner;
        run.secondOwner = edges[startingEdge].secondOwner;
        int currentKey = startingKey;
        std::size_t currentEdge = startingEdge;
        const int firstKey = startingKey;
        for (;;)
        {
            if (used[currentEdge] != 0u)
                break;
            used[currentEdge] = 1u;
            const detail::PoliticalGridEdge &edge = edges[currentEdge];
            const int startKey = endpointKey(edge.startX, edge.startY);
            const int endKey = endpointKey(edge.endX, edge.endY);
            const int nextKey = currentKey == startKey ? endKey : startKey;
            if (run.points.empty())
                run.points.push_back({static_cast<float>(currentKey % endpointWidth),
                                      static_cast<float>(currentKey / endpointWidth)});
            run.points.push_back({static_cast<float>(nextKey % endpointWidth),
                                  static_cast<float>(nextKey / endpointWidth)});
            currentKey = nextKey;
            if (currentKey == firstKey)
                break;
            if (pairDegree(currentKey, run.firstOwner, run.secondOwner) != 2)
                break;
            std::size_t nextEdge = edges.size();
            for (const std::size_t candidate :
                 adjacency[static_cast<std::size_t>(currentKey)])
            {
                if (used[candidate] == 0u &&
                    samePair(edges[candidate], run.firstOwner, run.secondOwner))
                {
                    nextEdge = candidate;
                    break;
                }
            }
            if (nextEdge == edges.size())
                break;
            currentEdge = nextEdge;
        }
        return run;
    };
    const auto appendIfCoherent = [&](OverviewPoliticalBorderRun run) {
        float length = 0.0f;
        for (std::size_t index = 1u; index < run.points.size(); ++index)
        {
            const float dx = run.points[index].x - run.points[index - 1u].x;
            const float dy = run.points[index].y - run.points[index - 1u].y;
            length += std::sqrt(dx * dx + dy * dy);
        }
        // Short isolated ownership transitions are rasterisation noise, not a
        // legible national border at continental scale.
        if (length < 2.5f)
            return;
        const bool closed = run.points.size() >= 2u &&
                            std::abs(run.points.front().x - run.points.back().x) < 0.001f &&
                            std::abs(run.points.front().y - run.points.back().y) < 0.001f;
        if (!closed &&
            (!overviewPoliticalBorderEndpointIsAnchored(
                 projection, run.points.front()) ||
             !overviewPoliticalBorderEndpointIsAnchored(
                 projection, run.points.back())))
            return;
        run.points = detail::simplifyAndSoftenPoliticalRun(run.points);
        if (run.points.size() >= 2u)
            result.push_back(std::move(run));
    };

    // Begin at non-degree-two endpoints first, so triple junctions split into
    // independent owner-pair paths instead of creating a single spaghetti run.
    for (std::size_t edgeIndex = 0u; edgeIndex < edges.size(); ++edgeIndex)
    {
        if (used[edgeIndex] != 0u)
            continue;
        const detail::PoliticalGridEdge &edge = edges[edgeIndex];
        const int startKey = endpointKey(edge.startX, edge.startY);
        const int endKey = endpointKey(edge.endX, edge.endY);
        if (pairDegree(startKey, edge.firstOwner, edge.secondOwner) != 2)
            appendIfCoherent(traceRun(edgeIndex, startKey));
        else if (pairDegree(endKey, edge.firstOwner, edge.secondOwner) != 2)
            appendIfCoherent(traceRun(edgeIndex, endKey));
    }
    // Any remaining components are closed internal borders.
    for (std::size_t edgeIndex = 0u; edgeIndex < edges.size(); ++edgeIndex)
    {
        if (used[edgeIndex] != 0u)
            continue;
        const detail::PoliticalGridEdge &edge = edges[edgeIndex];
        appendIfCoherent(traceRun(
            edgeIndex, endpointKey(edge.startX, edge.startY)));
    }
    return result;
}

inline std::vector<OverviewPoliticalStrokeSegment> buildOverviewPoliticalDashes(
    const OverviewPoliticalBorderRun &run,
    float dashLengthCells = kOverviewPoliticalDashLengthCells,
    float gapLengthCells = kOverviewPoliticalGapLengthCells)
{
    std::vector<OverviewPoliticalStrokeSegment> result;
    if (run.points.size() < 2u || dashLengthCells <= 0.0f ||
        gapLengthCells < 0.0f)
        return result;
    std::vector<MapFeaturePoint> points = run.points;
    // Ownership is cell based, but a displayed national boundary should not
    // expose that implementation detail. Two endpoint-preserving Chaikin
    // passes retain junctions/coast anchors while removing the staircase.
    for (int pass = 0; pass < 2 && points.size() >= 3u; ++pass)
    {
        std::vector<MapFeaturePoint> softened;
        softened.reserve(points.size() * 2u);
        softened.push_back(points.front());
        for (std::size_t index = 1u; index < points.size(); ++index)
        {
            const MapFeaturePoint &a = points[index - 1u];
            const MapFeaturePoint &b = points[index];
            softened.push_back({a.x * 0.72f + b.x * 0.28f,
                                a.y * 0.72f + b.y * 0.28f});
            softened.push_back({a.x * 0.28f + b.x * 0.72f,
                                a.y * 0.28f + b.y * 0.72f});
        }
        softened.push_back(points.back());
        points = std::move(softened);
    }
    const float period = dashLengthCells + gapLengthCells;
    float pathDistance = 0.0f;
    for (std::size_t index = 1u; index < points.size(); ++index)
    {
        const MapFeaturePoint &start = points[index - 1u];
        const MapFeaturePoint &end = points[index];
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        if (length <= 0.0001f)
            continue;
        const float segmentStart = pathDistance;
        const float segmentEnd = pathDistance + length;
        float dashStart = std::floor(segmentStart / period) * period;
        while (dashStart + dashLengthCells <= segmentStart + 0.00001f)
            dashStart += period;
        for (; dashStart < segmentEnd - 0.00001f; dashStart += period)
        {
            const float fromDistance = std::max(segmentStart, dashStart);
            const float toDistance = std::min(segmentEnd,
                                              dashStart + dashLengthCells);
            if (toDistance > fromDistance + 0.0001f)
            {
                const float from = (fromDistance - segmentStart) / length;
                const float to = (toDistance - segmentStart) / length;
                result.push_back({{start.x + deltaX * from,
                                   start.y + deltaY * from},
                                  {start.x + deltaX * to,
                                   start.y + deltaY * to}});
            }
        }
        pathDistance = segmentEnd;
    }
    return result;
}

// Small deterministic 2-D value noise. Axis-independent lattice values avoid
// the conspicuous diagonal wave bands produced by the former sum of sines.
} // namespace gm::map_render
