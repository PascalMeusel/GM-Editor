#pragma once

#include "Models/Map/MapAuthoring.hpp"

// A brush gesture owns one continuous centreline. Its cell projection remains
// gameplay data; it is deliberately never converted into adjacent-cell edges.
inline bool mapGeneratedFeatureIsManualStroke(const MapGeneratedFeature &feature)
{
    return feature.id.rfind("manual_stroke_", 0) == 0 &&
           (feature.kind == MapGeneratedFeatureKind::Road ||
            feature.kind == MapGeneratedFeatureKind::Trail ||
            feature.kind == MapGeneratedFeatureKind::River);
}

inline bool appendContinuousMapWayStroke(MapModel &map,
                                         const std::vector<MapFeaturePoint> &rawPoints,
                                         MapWayKind wayKind, int brushSize)
{
    if (!map.generation.continuousRendering || map.width <= 0 || map.height <= 0 ||
        wayKind == MapWayKind::None) return false;
    MapGeneratedFeature feature;
    for (const auto &raw : rawPoints)
    {
        if (!std::isfinite(raw.x) || !std::isfinite(raw.y)) continue;
        MapFeaturePoint point{std::clamp(raw.x, 0.001f, map.width - 0.001f),
                              std::clamp(raw.y, 0.001f, map.height - 0.001f)};
        if (!feature.points.empty())
        {
            const auto last = feature.points.back();
            if (std::hypot(point.x - last.x, point.y - last.y) < 0.02f) continue;
        }
        feature.points.push_back(point);
    }
    if (feature.points.empty()) return false;
    if (feature.points.size() == 1)
    {
        const auto point = feature.points.front();
        feature.points.front().x = std::max(0.001f, point.x - 0.15f);
        feature.points.push_back({std::min(map.width - 0.001f, point.x + 0.15f), point.y});
    }
    for (unsigned serial = 1;; ++serial)
    {
        feature.id = "manual_stroke_" + std::to_string(serial);
        if (std::none_of(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                         [&](const auto &existing) { return existing.id == feature.id; })) break;
    }
    const bool river = wayKind == MapWayKind::River || wayKind == MapWayKind::Canal;
    feature.kind = river ? MapGeneratedFeatureKind::River :
                   wayKind == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road;
    feature.materialId = wayKind == MapWayKind::Canal ? "fantasy_neutral.water.canal" :
                         river ? "fantasy_neutral.water.river" :
                         wayKind == MapWayKind::Trail ? "fantasy_neutral.road.trail" :
                         wayKind == MapWayKind::StoneRoad ? "fantasy_neutral.road.stone" : "fantasy_neutral.road.dirt";
    feature.name = river ? "Drawn waterway" : "Drawn route";
    feature.width = std::clamp(brushSize, 1, 32) * (river ? 1.0f : wayKind == MapWayKind::Trail ? 0.55f : 0.82f);
    map.generation.graph.features.push_back(std::move(feature));
    if(!river)
    {
        const auto before=map.generation.graph.features.back();
        connectEditedMapRoad(map,static_cast<int>(map.generation.graph.features.size())-1,before,1.f);
        reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map,before);
    }
    return true;
}

namespace map_stroke_authoring_detail
{
// Intersect a segment with one erased cell. Keeping the complement gives exact
// cuts even when two cursor samples lie on opposite sides of the eraser.
inline bool cellInterval(MapFeaturePoint a, MapFeaturePoint b, MapCellCoordinate cell,
                         float &first, float &last)
{
    first = 0.0f; last = 1.0f;
    const auto axis = [&](float start, float delta, float lower, float upper) {
        if (std::abs(delta) < 1e-7f) return start >= lower && start <= upper;
        float lo = (lower - start) / delta, hi = (upper - start) / delta;
        if (lo > hi) std::swap(lo, hi);
        first = std::max(first, lo); last = std::min(last, hi);
        return first <= last;
    };
    return axis(a.x, b.x - a.x, static_cast<float>(cell.x), cell.x + 1.0f) &&
           axis(a.y, b.y - a.y, static_cast<float>(cell.y), cell.y + 1.0f) && first <= last;
}
inline bool samePoint(MapFeaturePoint a, MapFeaturePoint b)
{
    return std::hypot(a.x - b.x, a.y - b.y) < 1e-5f;
}
}

// The caller clears the ordinary brush footprint first. Retain generated and
// locked geometry, cut only drawn strokes, then restore surviving crossings.
inline bool eraseContinuousMapWayStrokes(MapModel &map,
                                         const std::vector<MapCellCoordinate> &rawFootprint)
{
    if (!map.generation.continuousRendering || rawFootprint.empty()) return false;
    const auto footprint = normalizeMapCellCoordinates(rawFootprint);
    std::vector<MapCellCoordinate> affected = footprint;
    std::vector<std::pair<MapCellCoordinate, MapWayKind>> obsoleteProjection;
    std::vector<MapGeneratedFeature> retained;
    std::vector<std::string> reservedIds;
    for (const auto &feature : map.generation.graph.features) reservedIds.push_back(feature.id);
    bool changed = false;
    for (const auto &feature : map.generation.graph.features)
    {
        if (!mapGeneratedFeatureIsManualStroke(feature) || mapGeneratedFeatureIsLocked(map, feature))
        {
            retained.push_back(feature); continue;
        }
        std::vector<std::vector<MapFeaturePoint>> fragments;
        bool cut = false;
        for (std::size_t index = 1; index < feature.points.size(); ++index)
        {
            const auto a = feature.points[index - 1], b = feature.points[index];
            std::vector<std::pair<float, float>> erased;
            for (const auto cell : footprint)
            {
                float lo, hi;
                if (map_stroke_authoring_detail::cellInterval(a, b, cell, lo, hi)) erased.push_back({lo, hi});
            }
            std::sort(erased.begin(), erased.end());
            std::vector<std::pair<float, float>> merged;
            for (const auto interval : erased)
            {
                if (!merged.empty() && interval.first <= merged.back().second + 1e-6f)
                    merged.back().second = std::max(merged.back().second, interval.second);
                else merged.push_back(interval);
            }
            cut |= !merged.empty();
            const auto appendInterval = [&](float lo, float hi) {
                if (hi - lo < 1e-6f) return;
                const MapFeaturePoint start{a.x + (b.x - a.x) * lo, a.y + (b.y - a.y) * lo};
                const MapFeaturePoint end{a.x + (b.x - a.x) * hi, a.y + (b.y - a.y) * hi};
                if (!fragments.empty() && map_stroke_authoring_detail::samePoint(fragments.back().back(), start))
                    fragments.back().push_back(end);
                else fragments.push_back({start, end});
            };
            const float margin = 0.0001f / std::max(0.0001f, std::hypot(b.x - a.x, b.y - a.y));
            float cursor = 0.0f;
            for (const auto interval : merged)
            {
                appendInterval(cursor, std::max(cursor, interval.first - margin));
                cursor = std::min(1.0f, interval.second + margin);
            }
            appendInterval(cursor, 1.0f);
        }
        if (!cut) { retained.push_back(feature); continue; }
        changed = true;
        std::vector<MapCellCoordinate> survivingProjection;
        for (std::size_t index = 0; index < fragments.size(); ++index)
        {
            auto fragment = feature; // Preserve name, material, visibility and lineage.
            fragment.points = std::move(fragments[index]);
            if (index != 0)
                for (unsigned suffix = 2;; ++suffix)
                {
                    fragment.id = feature.id + "_part_" + std::to_string(suffix);
                    if (std::find(reservedIds.begin(), reservedIds.end(), fragment.id) == reservedIds.end())
                    { reservedIds.push_back(fragment.id); break; }
                }
            const auto cells = mapGeneratedFeatureRasterCells(map, fragment);
            survivingProjection.insert(survivingProjection.end(), cells.begin(), cells.end());
            retained.push_back(std::move(fragment));
        }
        survivingProjection = normalizeMapCellCoordinates(std::move(survivingProjection));
        for (const auto cell : mapGeneratedFeatureRasterCells(map, feature))
            if (!std::binary_search(survivingProjection.begin(), survivingProjection.end(), cell))
            {
                obsoleteProjection.push_back({cell, mapGeneratedFeatureWayKind(feature)});
                affected.push_back(cell);
            }
    }
    if (changed) map.generation.graph.features = std::move(retained);
    // Water first, roads second: a surviving road across a surviving river
    // restores a bridge instead of replacing the crossing with plain water.
    const auto restore = [&](const MapGeneratedFeature &feature) {
        // Grid-derived edges are reconciled from the erased cells afterwards;
        // replaying them here would undo the ordinary legacy eraser operation.
        return !mapGeneratedFeatureIsGridDerivedManualWay(feature) || mapGeneratedFeatureIsLocked(map, feature);
    };
    std::vector<std::pair<MapWayKind, std::vector<MapCellCoordinate>>> owners;
    for (const auto &feature : map.generation.graph.features)
        if (restore(feature) && mapGeneratedFeatureIsWayOrBridge(feature))
            owners.push_back({mapGeneratedFeatureWayKind(feature), mapGeneratedFeatureRasterCells(map, feature)});
    for (const auto &projection : obsoleteProjection)
    {
        const auto cell = projection.first;
        auto *target = mapCellAt(map, cell.x, cell.y);
        if (!target || (target->wayKind != MapWayKind::None && target->wayKind != projection.second)) continue;
        if (mapAuthoringPreservesStandingWater(map,*target,projection.second)) continue;
        const bool stillOwned = std::any_of(owners.begin(), owners.end(), [&](const auto &owner) {
            return owner.first == projection.second &&
                   std::binary_search(owner.second.begin(), owner.second.end(), cell);
        });
        if (stillOwned) continue;
        // A narrow eraser removes the full cut section's old brush projection,
        // otherwise its side cells become grid roads at the next rebuild.
        if ((target->kind == MapTileKind::Water && (projection.second == MapWayKind::River || projection.second == MapWayKind::Canal)) ||
            target->kind == MapTileKind::Bridge)
        {
            target->wayKind = target->kind == MapTileKind::Water ? MapWayKind::River : projection.second;
            const auto ground = mapVisualGroundKind(map, cell.x, cell.y);
            MapCellModel defaults = *target;
            applyMapCellTerrainDefaults(defaults, ground);
            target->kind = defaults.kind;
            target->movementCost = defaults.movementCost;
            target->difficultTerrain = defaults.difficultTerrain;
            target->blocksLineOfSight = defaults.blocksLineOfSight;
        }
        target->wayKind = MapWayKind::None;
    }
    affected = normalizeMapCellCoordinates(std::move(affected));
    for (const auto &feature : map.generation.graph.features)
        if (restore(feature) && feature.kind == MapGeneratedFeatureKind::River)
            projectGeneratedWayFeatureToCells(map, feature, false, &affected);
    for (const auto &feature : map.generation.graph.features)
        if (restore(feature) && (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail))
            projectGeneratedWayFeatureToCells(map, feature, false, &affected);
    return changed;
}
