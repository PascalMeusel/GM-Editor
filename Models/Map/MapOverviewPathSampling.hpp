#pragma once

#include "Models/Map/MapOverviewTerrainSampling.hpp"

namespace gm::map_render
{
namespace detail
{
inline std::vector<MapFeaturePoint> softenOverviewPath(
    const std::vector<MapFeaturePoint> &source,
    int passes)
{
    if (source.size() < 3u || passes <= 0)
        return source;
    std::vector<MapFeaturePoint> current = source;
    for (int pass = 0; pass < passes; ++pass)
    {
        std::vector<MapFeaturePoint> softened;
        softened.reserve(current.size() * 2u);
        softened.push_back(current.front());
        for (std::size_t index = 1u; index < current.size(); ++index)
        {
            const MapFeaturePoint &a = current[index - 1u];
            const MapFeaturePoint &b = current[index];
            softened.push_back({a.x * 0.72f + b.x * 0.28f,
                                a.y * 0.72f + b.y * 0.28f});
            softened.push_back({a.x * 0.28f + b.x * 0.72f,
                                a.y * 0.28f + b.y * 0.72f});
        }
        softened.push_back(current.back());
        current = std::move(softened);
    }
    return current;
}

inline float overviewPathLength(const std::vector<MapFeaturePoint> &points)
{
    float length = 0.0f;
    for (std::size_t index = 1u; index < points.size(); ++index)
    {
        const float dx = points[index].x - points[index - 1u].x;
        const float dy = points[index].y - points[index - 1u].y;
        length += std::sqrt(dx * dx + dy * dy);
    }
    return length;
}

struct OverviewPathSample
{
    MapFeaturePoint point;
    float tangentX = 1.0f;
    float tangentY = 0.0f;
};

inline OverviewPathSample sampleOverviewPath(
    const std::vector<MapFeaturePoint> &points,
    float requestedDistance)
{
    OverviewPathSample result;
    if (points.empty())
        return result;
    if (points.size() == 1u)
    {
        result.point = points.front();
        return result;
    }
    const float totalLength = overviewPathLength(points);
    const float distance = std::clamp(requestedDistance, 0.0f, totalLength);
    float traversed = 0.0f;
    for (std::size_t index = 1u; index < points.size(); ++index)
    {
        const MapFeaturePoint &start = points[index - 1u];
        const MapFeaturePoint &end = points[index];
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0001f)
            continue;
        if (traversed + length >= distance || index + 1u == points.size())
        {
            const float amount = std::clamp((distance - traversed) / length,
                                            0.0f, 1.0f);
            result.point = {start.x + dx * amount, start.y + dy * amount};
            result.tangentX = dx / length;
            result.tangentY = dy / length;
            return result;
        }
        traversed += length;
    }
    result.point = points.back();
    return result;
}

inline bool overviewRidgePointNeedsClearance(const MapModel &map,
                                             const MapFeaturePoint &point)
{
    for (const MapGeneratedFeature &other : map.generation.graph.features)
    {
        if (other.points.empty())
            continue;
        float clearance = 0.0f;
        if (other.kind == MapGeneratedFeatureKind::Settlement ||
            other.kind == MapGeneratedFeatureKind::Landmark)
            clearance = 1.45f;
        else if (other.kind == MapGeneratedFeatureKind::River)
            clearance = 0.92f;
        else if (other.kind == MapGeneratedFeatureKind::Road ||
                 other.kind == MapGeneratedFeatureKind::Trail)
            clearance = 0.62f;
        else
            continue;

        const float clearanceSquared = clearance * clearance;
        if (other.points.size() == 1u)
        {
            const float dx = point.x - other.points.front().x;
            const float dy = point.y - other.points.front().y;
            if (dx * dx + dy * dy <= clearanceSquared)
                return true;
            continue;
        }
        for (std::size_t index = 1u; index < other.points.size(); ++index)
            if (politicalPointSegmentDistanceSquared(
                    point, other.points[index - 1u], other.points[index]) <=
                clearanceSquared)
                return true;
    }
    return false;
}

inline bool overviewForestPointNeedsClearance(const MapModel &map,
                                              const MapFeaturePoint &point,
                                              bool includePointSymbols = true)
{
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.points.empty() || (!includePointSymbols &&
            (feature.kind == MapGeneratedFeatureKind::Settlement || feature.kind == MapGeneratedFeatureKind::Landmark)))
            continue;
        float clearance = 0.0f;
        if (feature.kind == MapGeneratedFeatureKind::Settlement ||
            feature.kind == MapGeneratedFeatureKind::Landmark)
            clearance = ((map.generation.recipe.styleVersion == 5 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v5") || (map.generation.recipe.styleVersion == 6 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6")) ? 1.70f : 1.05f;
        else if (feature.kind == MapGeneratedFeatureKind::River)
            clearance = 0.62f;
        else if (feature.kind == MapGeneratedFeatureKind::Road ||
                 feature.kind == MapGeneratedFeatureKind::Trail)
            clearance = 0.48f;
        else
            continue;
        if (map.atlasLevel == MapLevel::Region && map.generation.recipe.generatorVersion >= 12 &&
            map.generation.recipe.styleVersion == 6 && feature.kind == MapGeneratedFeatureKind::Settlement)
            clearance = 2.35f;
        const float clearanceSquared = clearance * clearance;
        if (feature.points.size() == 1u)
        {
            const float dx = point.x - feature.points.front().x;
            const float dy = point.y - feature.points.front().y;
            if (dx * dx + dy * dy <= clearanceSquared)
                return true;
            continue;
        }
        for (std::size_t index = 1u; index < feature.points.size(); ++index)
            if (politicalPointSegmentDistanceSquared(
                    point, feature.points[index - 1u], feature.points[index]) <=
                clearanceSquared)
                return true;
    }
    return false;
}
} // namespace detail

} // namespace gm::map_render
