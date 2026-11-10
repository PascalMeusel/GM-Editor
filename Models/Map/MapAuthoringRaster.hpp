#pragma once

#include "Models/Combat/EncounterModel.hpp"
#include "Models/Map/MapModels.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

struct MapCellCoordinate
{
    int x = 0;
    int y = 0;
};

inline bool operator==(const MapCellCoordinate &left, const MapCellCoordinate &right)
{
    return left.x == right.x && left.y == right.y;
}

inline bool operator<(const MapCellCoordinate &left, const MapCellCoordinate &right)
{
    if (left.y != right.y)
        return left.y < right.y;
    return left.x < right.x;
}

inline void addUniqueMapCellCoordinate(std::vector<MapCellCoordinate> &cells, const MapModel &map, int x, int y)
{
    if (!isMapCellInside(map, x, y))
        return;
    cells.push_back({x, y});
}

inline std::vector<MapCellCoordinate> normalizeMapCellCoordinates(std::vector<MapCellCoordinate> cells)
{
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return cells;
}

inline MapWayKind mapGeneratedFeatureWayKind(const MapGeneratedFeature &feature)
{
    if (feature.kind == MapGeneratedFeatureKind::River)
        return feature.materialId.find("canal") != std::string::npos
                   ? MapWayKind::Canal
                   : MapWayKind::River;
    if (feature.kind == MapGeneratedFeatureKind::Trail)
        return MapWayKind::Trail;
    if (feature.materialId.find("stone") != std::string::npos)
        return MapWayKind::StoneRoad;
    return MapWayKind::DirtRoad;
}

inline bool mapGeneratedFeatureIsWayOrBridge(const MapGeneratedFeature &feature)
{
    return feature.kind == MapGeneratedFeatureKind::Road ||
           feature.kind == MapGeneratedFeatureKind::Trail ||
           feature.kind == MapGeneratedFeatureKind::River ||
           feature.kind == MapGeneratedFeatureKind::Bridge;
}

inline bool mapGeneratedFeatureIsGridDerivedManualWay(const MapGeneratedFeature &feature)
{
    return feature.id.rfind("manual_way_", 0) == 0 ||
           feature.id.rfind("manual_bridge_", 0) == 0;
}

inline bool mapGeneratedFeatureIsLocked(const MapModel &map, const MapGeneratedFeature &feature)
{
    return feature.locked ||
           std::find(map.generation.recipe.lockedFeatureIds.begin(),
                     map.generation.recipe.lockedFeatureIds.end(), feature.id) !=
               map.generation.recipe.lockedFeatureIds.end();
}

// Rasterize only the semantic footprint of a graph feature. The helper is used
// by authoring reconciliation and partial regeneration protection, so both paths
// agree on which cells a preserved feature owns.
inline std::vector<MapCellCoordinate> mapGeneratedFeatureRasterCells(
    const MapModel &map,
    const MapGeneratedFeature &feature)
{
    std::vector<MapCellCoordinate> cells;
    if (map.width <= 0 || map.height <= 0 || feature.points.empty())
        return cells;

    const auto appendPoint = [&](const MapFeaturePoint &point) {
        const int x = std::clamp(static_cast<int>(std::floor(point.x)), 0, map.width - 1);
        const int y = std::clamp(static_cast<int>(std::floor(point.y)), 0, map.height - 1);
        if (feature.id.rfind("manual_stroke_", 0) == 0 &&
            (feature.kind == MapGeneratedFeatureKind::Road ||
             feature.kind == MapGeneratedFeatureKind::Trail ||
             feature.kind == MapGeneratedFeatureKind::River))
        {
            const float unitWidth = feature.kind == MapGeneratedFeatureKind::River ? 1.0f :
                                    feature.kind == MapGeneratedFeatureKind::Trail ? 0.55f : 0.82f;
            const int brushSize = std::clamp(static_cast<int>(std::lround(feature.width / unitWidth)), 1, 32);
            const int first = -(brushSize / 2);
            // The square envelope also owns circular-brush cells. Ownership
            // never paints cells; it prevents their conversion to a road mesh.
            for (int dy = first; dy < first + brushSize; ++dy)
                for (int dx = first; dx < first + brushSize; ++dx)
                    addUniqueMapCellCoordinate(cells, map, x + dx, y + dy);
        }
        else addUniqueMapCellCoordinate(cells, map, x, y);
    };
    const auto appendSegment = [&](const MapFeaturePoint &start, const MapFeaturePoint &end) {
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const int steps = std::max(
            1, static_cast<int>(std::ceil(std::max(std::abs(deltaX), std::abs(deltaY)) * 2.0f)));
        for (int step = 0; step <= steps; ++step)
        {
            const float amount = static_cast<float>(step) / static_cast<float>(steps);
            appendPoint({start.x + deltaX * amount, start.y + deltaY * amount});
        }
        if (feature.id.rfind("manual_stroke_", 0) == 0)
        {
            // The controller stamps Bresenham cells between cursor samples.
            // Its snapped centres can differ from the raw subcell line by one
            // cell. Own both projections so the shoulder cannot become an
            // unrelated grid-derived road/river spur during reconciliation.
            int x = std::clamp(static_cast<int>(std::floor(start.x)), 0, map.width - 1);
            int y = std::clamp(static_cast<int>(std::floor(start.y)), 0, map.height - 1);
            const int endX = std::clamp(static_cast<int>(std::floor(end.x)), 0, map.width - 1);
            const int endY = std::clamp(static_cast<int>(std::floor(end.y)), 0, map.height - 1);
            const int dx = std::abs(endX - x), sx = x < endX ? 1 : -1;
            const int dy = -std::abs(endY - y), sy = y < endY ? 1 : -1;
            int error = dx + dy;
            for (;;)
            {
                appendPoint({x + 0.5f, y + 0.5f});
                if (x == endX && y == endY) break;
                const int doubledError = error * 2;
                if (doubledError >= dy) { error += dy; x += sx; }
                if (doubledError <= dx) { error += dx; y += sy; }
            }
        }
    };
    const bool linear = feature.kind == MapGeneratedFeatureKind::River ||
                        feature.kind == MapGeneratedFeatureKind::Road ||
                        feature.kind == MapGeneratedFeatureKind::Trail ||
                        feature.kind == MapGeneratedFeatureKind::Bridge ||
                        feature.kind == MapGeneratedFeatureKind::Coastline ||
                        feature.kind == MapGeneratedFeatureKind::MountainRange;
    if (linear || feature.closed)
    {
        appendPoint(feature.points.front());
        for (std::size_t index = 1; index < feature.points.size(); ++index)
            appendSegment(feature.points[index - 1], feature.points[index]);
        if (feature.closed && feature.points.size() >= 3)
            appendSegment(feature.points.back(), feature.points.front());
    }
    else
    {
        for (const MapFeaturePoint &point : feature.points)
            appendPoint(point);
    }

    if (feature.closed && feature.points.size() >= 3)
    {
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                bool inside = false;
                for (std::size_t current = 0, previous = feature.points.size() - 1;
                     current < feature.points.size(); previous = current++)
                {
                    const MapFeaturePoint &a = feature.points[current];
                    const MapFeaturePoint &b = feature.points[previous];
                    const bool crosses = ((a.y > py) != (b.y > py)) &&
                        (px < (b.x - a.x) * (py - a.y) /
                                      ((b.y - a.y) == 0.0f ? 0.000001f : (b.y - a.y)) +
                                  a.x);
                    if (crosses)
                        inside = !inside;
                }
                if (inside)
                    addUniqueMapCellCoordinate(cells, map, x, y);
            }
        }
    }
    return normalizeMapCellCoordinates(std::move(cells));
}

