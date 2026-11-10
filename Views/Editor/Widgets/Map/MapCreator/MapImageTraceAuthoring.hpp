#pragma once

#include "Models/Map/MapImageImportService.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace MapImageTraceAuthoring
{
enum class Mode
{
    Coastline,
    TerrainArea,
    Road,
    River,
    Settlement,
    PointOfInterest
};

inline MapGeneratedFeatureKind featureKind(Mode mode)
{
    switch (mode)
    {
    case Mode::Coastline: return MapGeneratedFeatureKind::Coastline;
    case Mode::TerrainArea: return MapGeneratedFeatureKind::TerrainRegion;
    case Mode::Road: return MapGeneratedFeatureKind::Road;
    case Mode::River: return MapGeneratedFeatureKind::River;
    case Mode::Settlement: return MapGeneratedFeatureKind::Settlement;
    case Mode::PointOfInterest: return MapGeneratedFeatureKind::Landmark;
    }
    return MapGeneratedFeatureKind::Landmark;
}

inline bool isClosed(Mode mode)
{
    return mode == Mode::Coastline || mode == Mode::TerrainArea;
}

inline bool isLine(Mode mode)
{
    return mode == Mode::Road || mode == Mode::River;
}

inline std::size_t minimumPointCount(Mode mode)
{
    return isClosed(mode) ? 3u : (isLine(mode) ? 2u : 1u);
}

inline const char *materialId(Mode mode)
{
    switch (mode)
    {
    case Mode::Coastline: return "trace.coastline";
    case Mode::TerrainArea: return "trace.terrain_area";
    case Mode::Road: return "trace.road";
    case Mode::River: return "trace.river";
    case Mode::Settlement: return "trace.settlement";
    case Mode::PointOfInterest: return "trace.poi";
    }
    return "trace.feature";
}

inline MapFeaturePoint constrainPoint(MapFeaturePoint point, int width, int height, bool snapToHalfCell)
{
    const float maximumX = static_cast<float>(std::max(0, width));
    const float maximumY = static_cast<float>(std::max(0, height));
    point.x = std::clamp(point.x, 0.0f, maximumX);
    point.y = std::clamp(point.y, 0.0f, maximumY);
    if (snapToHalfCell)
    {
        point.x = std::round(point.x * 2.0f) * 0.5f;
        point.y = std::round(point.y * 2.0f) * 0.5f;
        point.x = std::clamp(point.x, 0.0f, maximumX);
        point.y = std::clamp(point.y, 0.0f, maximumY);
    }
    return point;
}

// One deterministic Chaikin pass removes harsh mouse-click corners while
// retaining the authored endpoints. Closed outlines remain closed through the
// feature flag; the repeated first point is deliberately not stored.
inline std::vector<MapFeaturePoint> smoothedPoints(const std::vector<MapFeaturePoint> &points,
                                                   bool closed,
                                                   bool smoothingEnabled)
{
    if (!smoothingEnabled || points.size() < 3u)
        return points;

    std::vector<MapFeaturePoint> result;
    if (closed)
    {
        result.reserve(points.size() * 2u);
        for (std::size_t index = 0; index < points.size(); ++index)
        {
            const MapFeaturePoint &a = points[index];
            const MapFeaturePoint &b = points[(index + 1u) % points.size()];
            result.push_back({a.x * 0.75f + b.x * 0.25f, a.y * 0.75f + b.y * 0.25f});
            result.push_back({a.x * 0.25f + b.x * 0.75f, a.y * 0.25f + b.y * 0.75f});
        }
        return result;
    }

    result.reserve(points.size() * 2u);
    result.push_back(points.front());
    for (std::size_t index = 0; index + 1u < points.size(); ++index)
    {
        const MapFeaturePoint &a = points[index];
        const MapFeaturePoint &b = points[index + 1u];
        result.push_back({a.x * 0.75f + b.x * 0.25f, a.y * 0.75f + b.y * 0.25f});
        result.push_back({a.x * 0.25f + b.x * 0.75f, a.y * 0.25f + b.y * 0.75f});
    }
    result.push_back(points.back());
    return result;
}

inline MapImageImport::TraceRequest makeRequest(Mode mode,
                                                const std::string &name,
                                                const std::vector<MapFeaturePoint> &controlPoints,
                                                bool smoothingEnabled,
                                                bool gmOnly)
{
    MapImageImport::TraceRequest request;
    request.kind = featureKind(mode);
    request.name = name;
    request.materialId = materialId(mode);
    request.points = smoothedPoints(controlPoints, isClosed(mode), smoothingEnabled);
    request.closed = isClosed(mode);
    request.gmOnly = gmOnly;
    return request;
}
}
