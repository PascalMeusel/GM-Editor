#pragma once

#include "Models/Map/MapOverviewPathSampling.hpp"

namespace gm::map_render
{
inline bool overviewFeaturePolygonContains(const MapGeneratedFeature &feature,
                                           float x,
                                           float y)
{
    if (!feature.closed || feature.points.size() < 3u)
        return false;
    bool inside = false;
    for (std::size_t current = 0u, previous = feature.points.size() - 1u;
         current < feature.points.size(); previous = current++)
    {
        const MapFeaturePoint &a = feature.points[current];
        const MapFeaturePoint &b = feature.points[previous];
        if ((a.y > y) == (b.y > y))
            continue;
        const float denominator = b.y - a.y;
        const float crossingX = a.x + (b.x - a.x) * (y - a.y) /
            (std::abs(denominator) < 0.00001f ? 0.00001f : denominator);
        if (x < crossingX)
            inside = !inside;
    }
    return inside;
}

// Choose a quiet label position inside a nation rather than blindly using the
// polygon centroid. The semantic geometry is untouched; only the cartographic
// annotation moves away from mountain spines, waterways, roads and towns.
inline bool overviewTerritoryLabelPoint(const MapModel &map,
                                        const MapGeneratedFeature &territory,
                                        MapFeaturePoint &result)
{
    if (map.atlasLevel != MapLevel::World || territory.points.size() < 3u)
        return false;
    float minimumX = territory.points.front().x;
    float maximumX = minimumX;
    float minimumY = territory.points.front().y;
    float maximumY = minimumY;
    MapFeaturePoint centroid{};
    for (const MapFeaturePoint &point : territory.points)
    {
        centroid.x += point.x;
        centroid.y += point.y;
        minimumX = std::min(minimumX, point.x);
        maximumX = std::max(maximumX, point.x);
        minimumY = std::min(minimumY, point.y);
        maximumY = std::max(maximumY, point.y);
    }
    centroid.x /= static_cast<float>(territory.points.size());
    centroid.y /= static_cast<float>(territory.points.size());

    float bestScore = -100000.0f;
    bool found = false;
    // Nation names are deliberately wider than settlement labels. Keep their
    // semantic anchor inside a fixed cartographic safe area so both ImGui and
    // PNG placement start away from clipping, without coupling the shared
    // policy to a particular font or translated text width.
    const float horizontalSafeMargin = std::min(4.5f, map.width * 0.18f);
    const float verticalSafeMargin = std::min(2.75f, map.height * 0.16f);
    for (int y = std::max(0, static_cast<int>(std::floor(minimumY)));
         y <= std::min(map.height - 1, static_cast<int>(std::ceil(maximumY))); ++y)
        for (int x = std::max(0, static_cast<int>(std::floor(minimumX)));
             x <= std::min(map.width - 1, static_cast<int>(std::ceil(maximumX))); ++x)
        {
            const MapFeaturePoint candidate{static_cast<float>(x) + 0.5f,
                                            static_cast<float>(y) + 0.5f};
            if (candidate.x < horizontalSafeMargin ||
                candidate.x > static_cast<float>(map.width) - horizontalSafeMargin ||
                candidate.y < verticalSafeMargin ||
                candidate.y > static_cast<float>(map.height) - verticalSafeMargin)
                continue;
            if (!overviewFeaturePolygonContains(territory, candidate.x, candidate.y) ||
                !detail::isOverviewLandKind(overviewGroundKindForRender(map, x, y)))
                continue;

            float nearestSettlement = 12.0f;
            float nearestRoute = 8.0f;
            float nearestMountain = 10.0f;
            for (const MapGeneratedFeature &feature : map.generation.graph.features)
            {
                if (feature.points.empty() || &feature == &territory)
                    continue;
                float *nearest = nullptr;
                if (feature.kind == MapGeneratedFeatureKind::Settlement ||
                    feature.kind == MapGeneratedFeatureKind::Landmark)
                    nearest = &nearestSettlement;
                else if (feature.kind == MapGeneratedFeatureKind::River ||
                         feature.kind == MapGeneratedFeatureKind::Road ||
                         feature.kind == MapGeneratedFeatureKind::Trail)
                    nearest = &nearestRoute;
                else if (feature.kind == MapGeneratedFeatureKind::MountainRange)
                    nearest = &nearestMountain;
                if (!nearest)
                    continue;
                if (feature.points.size() == 1u)
                {
                    const float dx = candidate.x - feature.points.front().x;
                    const float dy = candidate.y - feature.points.front().y;
                    *nearest = std::min(*nearest, std::sqrt(dx * dx + dy * dy));
                }
                else
                {
                    for (std::size_t index = 1u; index < feature.points.size(); ++index)
                        *nearest = std::min(*nearest, std::sqrt(
                            detail::politicalPointSegmentDistanceSquared(
                                candidate, feature.points[index - 1u], feature.points[index])));
                }
            }

            float borderClearance = 8.0f;
            for (std::size_t index = 0u; index < territory.points.size(); ++index)
                borderClearance = std::min(borderClearance, std::sqrt(
                    detail::politicalPointSegmentDistanceSquared(
                        candidate, territory.points[index],
                        territory.points[(index + 1u) % territory.points.size()])));
            const float dx = candidate.x - centroid.x;
            const float dy = candidate.y - centroid.y;
            const MapTileKind ground = overviewGroundKindForRender(map, x, y);
            const float terrainPenalty = ground == MapTileKind::Mountain ? 22.0f :
                                         (ground == MapTileKind::Forest ? 1.4f : 0.0f);
            const float score = std::min(borderClearance, 4.0f) * 1.8f +
                                std::min(nearestSettlement, 7.0f) * 1.35f +
                                // A national label laid directly over a river
                                // or road destroys both labels' hierarchy. At
                                // world scale route clearance is more valuable
                                // than staying close to the polygon centroid.
                                std::min(nearestRoute, 4.5f) * 3.20f +
                                std::min(nearestMountain, 7.0f) * 4.0f -
                                std::sqrt(dx * dx + dy * dy) * 0.16f - terrainPenalty;
            if (!found || score > bestScore)
            {
                bestScore = score;
                result = candidate;
                found = true;
            }
        }
    return found;
}

} // namespace gm::map_render
