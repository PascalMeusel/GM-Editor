#pragma once

#include "Models/Map/MapOverviewForests.hpp"

namespace gm::map_render
{
inline std::vector<MapFeaturePoint> overviewCartographicCurvePoints(
    const MapModel &map,
    const MapGeneratedFeature &feature)
{
    if ((map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region) ||
        (feature.kind != MapGeneratedFeatureKind::Road &&
         feature.kind != MapGeneratedFeatureKind::Trail &&
         feature.kind != MapGeneratedFeatureKind::River) ||
        feature.points.size() < 2 || feature.locked)
        return feature.points;

    const bool river = feature.kind == MapGeneratedFeatureKind::River;
    const auto nearAnchor = [&](const MapFeaturePoint &point) {
        for (const auto &other : map.generation.graph.features)
        {
            if (other.id == feature.id || other.points.empty())
                continue;
            if (other.kind != MapGeneratedFeatureKind::Bridge &&
                other.kind != MapGeneratedFeatureKind::Settlement &&
                other.kind != MapGeneratedFeatureKind::River)
                continue;
            const float clearance = other.kind == MapGeneratedFeatureKind::Bridge ? 1.4f : 0.8f;
            for (const auto &anchor : {other.points.front(), other.points.back()})
                if (std::abs(point.x - anchor.x) < clearance &&
                    std::abs(point.y - anchor.y) < clearance)
                    return true;
        }
        return false;
    };
    std::vector<MapFeaturePoint> controls;
    controls.reserve(feature.points.size());
    for (std::size_t index = 0; index < feature.points.size(); ++index)
    {
        const auto &point = feature.points[index];
        if (river && index > 0 && index + 1 < feature.points.size() && !nearAnchor(point))
        {
            const auto &a = feature.points[index - 1];
            const auto &b = feature.points[index + 1];
            const float cross = (point.x - a.x) * (b.y - point.y) - (point.y - a.y) * (b.x - point.x);
            const float dot = (point.x - a.x) * (b.x - point.x) + (point.y - a.y) * (b.y - point.y);
            if (std::abs(cross) < 0.0001f && dot > 0.0f)
                continue;
        }
        controls.push_back(point);
    }
    std::vector<MapFeaturePoint> result;
    result.reserve(controls.size() * 4u);
    result.push_back(controls.front());
    const std::uint32_t stableHash = overviewStableFeatureHash(
        feature, map.generation.recipe.seed);
    const float phase = static_cast<float>(stableHash % 8191u) * 0.0037f;
    float travelled = 0.0f;
    for (std::size_t index = 1; index < controls.size(); ++index)
    {
        const MapFeaturePoint &start = controls[index - 1];
        const MapFeaturePoint &end = controls[index];
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        if (length <= 0.0001f)
            continue;
        const int pieces = std::max(1, static_cast<int>(std::ceil(length / 1.8f)));
        const float normalX = -deltaY / length;
        const float normalY = deltaX / length;
        for (int piece = 1; piece < pieces; ++piece)
        {
            const float amount = static_cast<float>(piece) / static_cast<float>(pieces);
            const float distance = travelled + length * amount;
            const float endpointFade = std::sin(amount * 3.14159265359f);
            const float drift =
                (std::sin(distance * 0.42f + phase) * (river ? 0.48f : 0.32f) +
                 std::sin(distance * 1.13f - phase * 0.47f) * 0.08f) * endpointFade;
            const MapFeaturePoint straight{start.x + deltaX * amount, start.y + deltaY * amount};
            const MapFeaturePoint curved{straight.x + normalX * drift, straight.y + normalY * drift};
            const int cx = static_cast<int>(std::floor(curved.x));
            const int cy = static_cast<int>(std::floor(curved.y));
            const MapCellModel *cell = mapCellAt(map, cx, cy);
            // Presentation must not create a crossing that the gameplay grid
            // does not possess. Keep bridge/junction approaches and authored
            // locked geometry exact, and stay within the same terrain domain.
            const bool dry = cell && cell->kind != MapTileKind::Water &&
                             cell->kind != MapTileKind::Ice && cell->kind != MapTileKind::Mountain &&
                             cell->wayKind != MapWayKind::River && cell->wayKind != MapWayKind::Canal;
            const bool riverDomain = cell && (overviewGroundKindIsLand(
                overviewGroundKindForRender(map, cx, cy)) || cell->wayKind == MapWayKind::River);
            result.push_back(!nearAnchor(straight) && (river ? riverDomain : dry) ? curved : straight);
        }
        result.push_back(end);
        travelled += length;
    }
    return result;
}

// Complete overview road geometry. Both backends consume these points
// directly; smoothing them again could cut through a pass or bridge approach.
// Locked and manually drawn roads keep their exact authored control geometry.
inline std::vector<MapFeaturePoint> overviewFinalRoadCurvePoints(
    const MapModel &map, const MapGeneratedFeature &feature)
{
    if((map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region) &&
       (feature.kind==MapGeneratedFeatureKind::Road || feature.kind==MapGeneratedFeatureKind::Trail) &&
       feature.id.rfind("manual_stroke_",0)==0 && !feature.locked && feature.points.size()>1 &&
       std::find(map.generation.recipe.lockedFeatureIds.begin(),map.generation.recipe.lockedFeatureIds.end(),feature.id)==map.generation.recipe.lockedFeatureIds.end())
    {
        // Round hand-drawn sampling corners within a fraction of one cell.
        // Split at incoming junctions so smoothing never pulls a road away
        // from the exact point to which another road was attached.
        std::vector<MapFeaturePoint> result,reach{feature.points.front()};
        const auto append=[&](MapFeaturePoint b) {
            const auto a=reach.back();
            const int steps=std::clamp(static_cast<int>(std::ceil(std::hypot(b.x-a.x,b.y-a.y)/1.f)),1,256);
            for(int s=1;s<=steps;++s)reach.push_back({a.x+(b.x-a.x)*s/steps,a.y+(b.y-a.y)*s/steps});
        };
        const auto finish=[&]() {
            auto smooth=detail::softenOverviewPath(reach,2);
            result.insert(result.end(),smooth.begin()+(result.empty()?0:1),smooth.end());
            reach={reach.back()};
        };
        for(std::size_t i=1;i<feature.points.size();++i)
        {
            const auto a=feature.points[i-1],b=feature.points[i];
            const float dx=b.x-a.x,dy=b.y-a.y,len2=dx*dx+dy*dy;
            std::vector<std::pair<float,MapFeaturePoint>> joins;
            for(const auto &other:map.generation.graph.features)
            {
                if(other.id==feature.id || other.points.empty() ||
                   (other.kind!=MapGeneratedFeatureKind::Road && other.kind!=MapGeneratedFeatureKind::Trail))continue;
                for(const auto p:{other.points.front(),other.points.back()})
                {
                    const float t=len2>.000001f?((p.x-a.x)*dx+(p.y-a.y)*dy)/len2:-1.f;
                    if(t>.0001f && t<=1.f && detail::politicalPointSegmentDistanceSquared(p,a,b)<.00001f)
                        joins.push_back({t,p});
                }
            }
            std::sort(joins.begin(),joins.end(),[](const auto &a,const auto &b){return a.first<b.first;});
            for(const auto &join:joins){append(join.second);finish();}
            if(std::hypot(reach.back().x-b.x,reach.back().y-b.y)>.0001f)append(b);
        }
        if(reach.size()>1)finish();
        return result;
    }
    if ((map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region) ||
        (feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) ||
        feature.points.size() < 2 || feature.locked || feature.id.rfind("manual_way_", 0) == 0 ||
        map.generation.recipe.generatorVersion >= 7)
        return feature.points;
    const auto nearFixedAnchor = [&](const MapFeaturePoint &point) {
        for (const auto &other : map.generation.graph.features)
            if (other.kind == MapGeneratedFeatureKind::Bridge || other.kind == MapGeneratedFeatureKind::Settlement)
                for (const auto &anchor : other.points)
                    if ((point.x - anchor.x) * (point.x - anchor.x) + (point.y - anchor.y) * (point.y - anchor.y) < 1.96f)
                        return true;
        return false;
    };
    const auto safeSegment = [&](const MapFeaturePoint &a, const MapFeaturePoint &b) {
        const int steps = std::max(1, static_cast<int>(std::ceil(std::hypot(b.x - a.x, b.y - a.y) * 8.0f)));
        for (int step = 0; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / steps;
            const MapFeaturePoint point{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
            const auto *cell = mapCellAt(map, static_cast<int>(std::floor(point.x)), static_cast<int>(std::floor(point.y)));
            if (!cell || cell->kind == MapTileKind::Mountain || cell->kind == MapTileKind::Water ||
                cell->kind == MapTileKind::Wall || cell->kind == MapTileKind::Building ||
                cell->kind == MapTileKind::Lava || cell->kind == MapTileKind::Pit || cell->kind == MapTileKind::Chasm)
                return false;
            float nearest = std::numeric_limits<float>::max();
            for (std::size_t index = 1; index < feature.points.size(); ++index)
                nearest = std::min(nearest, gm::map_geometry::roadPointSegmentDistanceSquared(
                    point, feature.points[index - 1], feature.points[index]));
            if (nearest > 0.35f * 0.35f)
                return false;
        }
        return true;
    };
    const auto safePath = [&](const std::vector<MapFeaturePoint> &points) {
        for (std::size_t index = 1; index < points.size(); ++index)
            if (!safeSegment(points[index - 1], points[index]))
                return false;
        return true;
    };
    auto points = overviewCartographicCurvePoints(map, feature);
    if (!safePath(points))
        points = feature.points;
    for (int pass = 0; pass < 3 && points.size() >= 3; ++pass)
    {
        auto next = points;
        for (std::size_t index = 1; index + 1 < points.size(); ++index)
        {
            if (nearFixedAnchor(points[index]))
                continue;
            const MapFeaturePoint candidate{
                points[index - 1].x * 0.22f + points[index].x * 0.56f + points[index + 1].x * 0.22f,
                points[index - 1].y * 0.22f + points[index].y * 0.56f + points[index + 1].y * 0.22f};
            if (safeSegment(points[index - 1], candidate) && safeSegment(candidate, points[index + 1]))
                next[index] = candidate;
        }
        if (safePath(next))
            points.swap(next);
    }
    return points;
}

// Continuous land coverage used to derive a contour from the same semantic
// projection that feeds the soft World terrain. It is public for deterministic
// policy tests and for renderer diagnostics; normal rendering should call the
// batched buildOverviewCoastSegments() below.
} // namespace gm::map_render
