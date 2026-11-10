#pragma once

#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapFeaturePresentationGeometry.hpp"

namespace gm::mapui
{
enum class MapObjectEditMode { Move, Reshape };

// Only the editor canvas may show the pre-drag illustration under a live
// selection outline. Dirty tracking and player/export review use the current
// authoring serial; they must never reuse this deliberately frozen preview.
inline std::uint64_t mapObjectCanvasVersion(std::uint64_t current, bool dragging,
                                          std::uint64_t beforeDrag)
{
    return dragging ? beforeDrag : current;
}

inline bool mapObjectCanMove(const MapGeneratedFeature &feature)
{
    switch (feature.kind)
    {
    case MapGeneratedFeatureKind::Road: case MapGeneratedFeatureKind::Trail:
    case MapGeneratedFeatureKind::River: case MapGeneratedFeatureKind::Bridge:
    case MapGeneratedFeatureKind::Settlement: case MapGeneratedFeatureKind::Landmark:
    case MapGeneratedFeatureKind::BuildingGroup: case MapGeneratedFeatureKind::TacticalSetpiece:
        return !feature.points.empty();
    default: return false;
    }
}

inline bool mapObjectCanReshape(const MapGeneratedFeature &feature)
{
    return feature.points.size() > 1 && (feature.kind == MapGeneratedFeatureKind::Road ||
        feature.kind == MapGeneratedFeatureKind::Trail || feature.kind == MapGeneratedFeatureKind::River);
}

inline bool mapObjectContainsPoint(const MapGeneratedFeature &feature, MapFeaturePoint p)
{
    if (!feature.closed || feature.points.size() < 3) return false;
    bool inside = false;
    for (std::size_t i = 0, j = feature.points.size() - 1; i < feature.points.size(); j = i++)
    {
        const auto a = feature.points[i], b = feature.points[j];
        if ((a.y > p.y) != (b.y > p.y) &&
            p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

// A roof is selected by its interior, not just an invisible polygon corner.
// In Reshape, visible handles of the selected route beat overlapping objects.
inline MapGeneratedFeatureHit hitTestMapObject(const MapModel &map, MapFeaturePoint p,
    float radius, int selected, MapObjectEditMode mode, const MapRenderPlan *presentation = nullptr)
{
    if (mode == MapObjectEditMode::Reshape && selected >= 0 &&
        selected < static_cast<int>(map.generation.graph.features.size()))
    {
        const auto &feature = map.generation.graph.features[selected];
        if (mapObjectCanReshape(feature))
        {
            MapGeneratedFeatureHit hit;
            float distance = radius * radius;
            for (int point : generatedMapFeatureEditHandles(feature))
            {
                const float candidate = mapFeaturePointDistanceSquared(p, feature.points[point]);
                if (candidate <= distance)
                { distance = candidate; hit = {selected, point, -1, 0, std::sqrt(candidate)}; }
            }
            if (hit.featureIndex >= 0) return hit;
        }
    }
    for (int index = 0; index < static_cast<int>(map.generation.graph.features.size()); ++index)
    {
        const auto &feature = map.generation.graph.features[index];
        if (feature.kind == MapGeneratedFeatureKind::BuildingGroup && mapObjectContainsPoint(feature, p))
            return {index, -1, -1, 0, 0};
    }
    // Some tactical buildings store one anchor and an irregular raster roof.
    // Follow that roof rather than treating its anchor as the entire building.
    const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
    const auto *cell = mapCellAt(map, x, y);
    if (cell && cell->kind == MapTileKind::Building)
    {
        std::vector<bool> visited(map.cells.size(), false);
        std::vector<MapCellCoordinate> roof{{x,y}};
        visited[y * map.width + x] = true;
        for (std::size_t next = 0; next < roof.size(); ++next)
            for (const auto delta : {MapCellCoordinate{1,0}, {-1,0}, {0,1}, {0,-1}})
            {
                const int xx=roof[next].x+delta.x, yy=roof[next].y+delta.y;
                const auto *neighbor=mapCellAt(map,xx,yy);
                if (neighbor && neighbor->kind==MapTileKind::Building && !visited[yy*map.width+xx])
                { visited[yy*map.width+xx]=true; roof.push_back({xx,yy}); }
            }
        int owner=-1;
        for (int index=0;index<static_cast<int>(map.generation.graph.features.size());++index)
        {
            const auto &feature=map.generation.graph.features[index];
            if (feature.kind!=MapGeneratedFeatureKind::BuildingGroup || feature.points.empty()) continue;
            const int xx=static_cast<int>(std::floor(feature.points.front().x));
            const int yy=static_cast<int>(std::floor(feature.points.front().y));
            if (isMapCellInside(map,xx,yy) && visited[yy*map.width+xx])
            { if (owner>=0) { owner=-1; break; } owner=index; }
        }
        if (owner>=0) return {owner,-1,-1,0,0};
    }
    return mode == MapObjectEditMode::Move ? hitTestPresentedMapFeature(map, p, radius, presentation)
                                           : hitTestGeneratedMapFeature(map, p, radius);
}

// Always derive the entire translation from the original feature. Clamping
// individual vertices would squash buildings or deform a route at map edges.
inline bool moveMapObject(MapModel &map, int index, const MapGeneratedFeature &before,
    MapFeaturePoint delta)
{
    if (index < 0 || index >= static_cast<int>(map.generation.graph.features.size()) ||
        !mapObjectCanMove(before) || mapGeneratedFeatureIsLocked(map, before)) return false;
    auto &feature = map.generation.graph.features[index];
    if (feature.id != before.id || mapGeneratedFeatureIsLocked(map, feature)) return false;
    float left = before.points.front().x, right = left, top = before.points.front().y, bottom = top;
    for (auto point : before.points)
    { left = std::min(left, point.x); right = std::max(right, point.x);
      top = std::min(top, point.y); bottom = std::max(bottom, point.y); }
    if (left < 0 || top < 0 || right > map.width || bottom > map.height) return false;
    delta.x = std::clamp(delta.x, -left, map.width - right);
    delta.y = std::clamp(delta.y, -top, map.height - bottom);
    if (before.kind == MapGeneratedFeatureKind::BuildingGroup)
    {
        delta.x = std::clamp(std::round(delta.x), std::ceil(-left), std::floor(map.width - right));
        delta.y = std::clamp(std::round(delta.y), std::ceil(-top), std::floor(map.height - bottom));
    }
    feature.points = before.points;
    for (auto &point : feature.points) { point.x += delta.x; point.y += delta.y; }
    return true;
}
}
