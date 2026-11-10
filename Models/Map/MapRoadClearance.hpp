#pragma once

#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gm::map_geometry
{
struct MapRoadCorridor
{
    EntityId featureId;
    std::vector<MapFeaturePoint> centerline;
    float radiusCells = 0.60f;
};

// Include the actual rendered shoulder (including the pixel minimum), then
// leave visible paper beside it. Relief must respect this capsule union, not
// just a road's control points or the tangent of a mountain spine.
inline float roadClearanceRadiusCells(float outerStrokeWidthCells)
{
    return std::max(0.30f, outerStrokeWidthCells * 0.5f) + 0.30f;
}

inline float roadPointSegmentDistanceSquared(const MapFeaturePoint &point,
                                              const MapFeaturePoint &a,
                                              const MapFeaturePoint &b)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float t = lengthSquared > 0.000001f
        ? std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    const float px = point.x - a.x - t * dx, py = point.y - a.y - t * dy;
    return px * px + py * py;
}

inline float roadSegmentDistanceSquared(const MapFeaturePoint &a, const MapFeaturePoint &b,
                                         const MapFeaturePoint &c, const MapFeaturePoint &d)
{
    const float rx = b.x - a.x, ry = b.y - a.y;
    const float sx = d.x - c.x, sy = d.y - c.y;
    const float cross = rx * sy - ry * sx;
    if (std::abs(cross) > 0.000001f)
    {
        const float t = ((c.x - a.x) * sy - (c.y - a.y) * sx) / cross;
        const float u = ((c.x - a.x) * ry - (c.y - a.y) * rx) / cross;
        if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f)
            return 0.0f;
    }
    return std::min({roadPointSegmentDistanceSquared(a, c, d),
                     roadPointSegmentDistanceSquared(b, c, d),
                     roadPointSegmentDistanceSquared(c, a, b),
                     roadPointSegmentDistanceSquared(d, a, b)});
}

inline bool roadCorridorContainsPoint(const MapRoadCorridor &corridor,
                                       const MapFeaturePoint &point, float extraRadiusCells = 0.0f)
{
    const float radius = corridor.radiusCells + std::max(0.0f, extraRadiusCells);
    for (std::size_t index = 1; index < corridor.centerline.size(); ++index)
        if (roadPointSegmentDistanceSquared(point, corridor.centerline[index - 1], corridor.centerline[index]) <= radius * radius)
            return true;
    return false;
}

inline bool roadCorridorIntersectsSegment(const MapRoadCorridor &corridor,
                                          const MapFeaturePoint &a, const MapFeaturePoint &b,
                                          float extraRadiusCells = 0.0f)
{
    const float radius = corridor.radiusCells + std::max(0.0f, extraRadiusCells);
    // Conservative broad phase. Most label edges and relief faces are far
    // from almost every route segment; avoid four projections for each pair.
    // Padding keeps floating-point boundary cases on the original exact path.
    const float padding = std::abs(radius) + .0001f *
        (1.f + std::max({std::abs(a.x),std::abs(a.y),std::abs(b.x),std::abs(b.y)}));
    const float left=std::min(a.x,b.x)-padding, right=std::max(a.x,b.x)+padding;
    const float top=std::min(a.y,b.y)-padding, bottom=std::max(a.y,b.y)+padding;
    for (std::size_t index = 1; index < corridor.centerline.size(); ++index)
    {
        const auto &c=corridor.centerline[index-1],&d=corridor.centerline[index];
        if(std::max(c.x,d.x)<left || std::min(c.x,d.x)>right ||
           std::max(c.y,d.y)<top || std::min(c.y,d.y)>bottom) continue;
        if (roadSegmentDistanceSquared(a, b, corridor.centerline[index - 1], corridor.centerline[index]) <= radius * radius)
            return true;
    }
    return false;
}
}
