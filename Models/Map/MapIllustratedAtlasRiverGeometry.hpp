#pragma once

#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
inline float illustratedAtlasV2RoadWidth(const MapGeneratedFeature &feature, float cellPixels)
{
    const float minimum = feature.kind == MapGeneratedFeatureKind::Trail ? 1.1f :
                          feature.width < .60f ? 1.2f : feature.width < .88f ? 1.6f : 2.3f;
    return std::max(minimum, feature.width*cellPixels*mapRenderScaleProfile(MapLevel::World).roadWidthCells*kOverviewRoadWidthScale);
}

inline std::vector<MapFeaturePoint> illustratedAtlasUntrimmedRiverPoints(const MapModel &map,
    const MapGeneratedFeature &feature)
{
    if ((usesIllustratedAtlasV3(map) || (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map))) && feature.points.size() >= 2)
    {
        // Smooth each reach independently. An incoming river's endpoint is a
        // fixed junction on the receiving channel, not a control to round off.
        std::vector<MapFeaturePoint> result, reach{feature.points.front()};
        const auto finishReach = [&]() {
            auto reachFeature = feature;
            // Uniform sampling gives hand-drawn and generated rivers the
            // same bend quality even after many closely spaced drag samples.
            reachFeature.points={reach.front()};
            for(std::size_t i=1;i<reach.size();++i)
            {
                const auto a=reach[i-1],b=reach[i];
                const int count=std::clamp(static_cast<int>(std::ceil(std::hypot(b.x-a.x,b.y-a.y)/.4f)),1,256);
                for(int j=1;j<=count;++j)reachFeature.points.push_back({a.x+(b.x-a.x)*j/count,a.y+(b.y-a.y)*j/count});
            }
            auto points = overviewCartographicCurvePoints(map,reachFeature);
            for (int pass = 0; pass < 3; ++pass)
            {
                std::vector<MapFeaturePoint> next{points.front()};
                for (std::size_t i = 1; i < points.size(); ++i)
                {
                    const auto a = points[i-1], b = points[i];
                    next.push_back({a.x*.75f+b.x*.25f,a.y*.75f+b.y*.25f});
                    next.push_back({a.x*.25f+b.x*.75f,a.y*.25f+b.y*.75f});
                }
                next.push_back(points.back()); points.swap(next);
            }
            result.insert(result.end(), points.begin()+(result.empty()?0:1), points.end());
            reach = {reach.back()};
        };
        for (std::size_t i = 1; i < feature.points.size(); ++i)
        {
            const auto a = feature.points[i-1], b = feature.points[i];
            const float dx = b.x-a.x, dy = b.y-a.y, length2 = dx*dx+dy*dy;
            std::vector<std::pair<float,MapFeaturePoint>> joins;
            for (const auto &other : map.generation.graph.features)
            {
                if (other.id == feature.id || other.kind != MapGeneratedFeatureKind::River || other.points.empty()) continue;
                const auto p = other.points.back();
                const float t = length2 > .000001f ? ((p.x-a.x)*dx+(p.y-a.y)*dy)/length2 : -1.f;
                const float ex = p.x-a.x-dx*t, ey = p.y-a.y-dy*t;
                if (t > .0001f && t <= 1.0001f && ex*ex+ey*ey < .00001f) joins.push_back({t,p});
            }
            std::sort(joins.begin(),joins.end(),[](const auto &a,const auto &b){return a.first<b.first;});
            for (const auto &join : joins) { reach.push_back(join.second); finishReach(); }
            if (std::hypot(reach.back().x-b.x,reach.back().y-b.y) > .0001f) reach.push_back(b);
        }
        if (reach.size() >= 2) finishReach();
        return result;
    }
    auto points = overviewCartographicCurvePoints(map,feature);
    if (points.size() < 3) return points;
    for (int pass = 0; pass < 3; ++pass)
    {
        std::vector<MapFeaturePoint> next;
        next.reserve(points.size()*2); next.push_back(points.front());
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            const auto a=points[i-1], b=points[i];
            next.push_back({a.x*.75f+b.x*.25f,a.y*.75f+b.y*.25f});
            next.push_back({a.x*.25f+b.x*.75f,a.y*.25f+b.y*.75f});
        }
        next.push_back(points.back()); points.swap(next);
    }
    return points;
}

struct AtlasRiverMouth
{
    bool joinsWater = false;
    MapFeaturePoint shore{}, direction{};
};

// The graph may end inside a water cell. Resolve its display endpoint against
// the same continuous land field as the coast, not that cell's square edge.
// Only a source-to-mouth terminal reach entering standing water is changed.
inline AtlasRiverMouth trimIllustratedAtlasRiverMouth(const MapModel &map,
    std::vector<MapFeaturePoint> &points, bool firstStandingWaterCrossing=false)
{
    AtlasRiverMouth mouth;
    if (!usesIllustratedAtlasV5(map) || map.generation.recipe.generatorVersion<11 ||
        points.size()<2 || (map.atlasLevel!=MapLevel::World && map.atlasLevel!=MapLevel::Region)) return mouth;
    const auto ground=buildOverviewGroundProjection(map);
    std::vector<std::uint8_t> land(ground.size());
    for (int y=0;y<map.height;++y) for (int x=0;x<map.width;++x)
        land[static_cast<std::size_t>(y)*map.width+x]=overviewGroundKindIsLand(overviewGroundKindFromProjection(map,ground,x,y))?1:0;
    const auto coverage=[&](MapFeaturePoint p) {
        return detail::overviewLandCoverageFromMask(map,land,p.x,p.y);
    };
    // A hand-drawn river has no prescribed mouse direction. Resolve the
    // visual reach source-to-mouth even when the user starts in the sea/lake.
    // The authored control points and their edit handles remain untouched.
    if (firstStandingWaterCrossing && coverage(points.front()) < .5f && coverage(points.back()) >= .5f)
        std::reverse(points.begin(), points.end());
    auto endpoint=points.back();
    if (endpoint.x<=0.f || endpoint.y<=0.f || endpoint.x>=map.width || endpoint.y>=map.height) return mouth;
    if (coverage(endpoint)>=.5f)
    {
        // A real lake cell at a convex corner may be covered by the smooth
        // coast reconstruction. Continue only through contiguous standing
        // water to that displayed shore; never bridge an inland land gap.
        const auto standingWater=[&](MapFeaturePoint p) {
            return p.x>=0.f && p.y>=0.f && p.x<map.width && p.y<map.height &&
                overviewGroundKindFromProjection(map,ground,static_cast<int>(p.x),static_cast<int>(p.y))==MapTileKind::Water;
        };
        if (!standingWater(endpoint)) return mouth;
        std::size_t before=points.size()-2;
        while (before>0 && std::hypot(endpoint.x-points[before].x,endpoint.y-points[before].y)<.12f) --before;
        const float dx=endpoint.x-points[before].x,dy=endpoint.y-points[before].y,length=std::hypot(dx,dy);
        if (length<.00001f) return mouth;
        bool found=false;
        for (int step=1;step<=30;++step)
        {
            const float distance=step*.08f;
            const MapFeaturePoint candidate{endpoint.x+dx/length*distance,endpoint.y+dy/length*distance};
            if (!standingWater(candidate)) break;
            if (coverage(candidate)<.5f) {endpoint=candidate;found=true;break;}
        }
        if (!found) return mouth;
        points.push_back(endpoint);
    }
    if (firstStandingWaterCrossing)
    {
        // A hand-drawn source-to-mouth stroke may finish far offshore. Its
        // saved path remains editable, but the illustrated river ends at the
        // first actual coast or lake it enters, not at the mouse-up position.
        bool hasDry=false;
        MapFeaturePoint dry{};
        std::size_t drySegment=1;
        for (std::size_t i=1;i<points.size();++i)
        {
            const auto a=points[i-1],b=points[i];
            const int steps=std::max(1,static_cast<int>(std::ceil(std::hypot(b.x-a.x,b.y-a.y)/.08f)));
            for (int step=0;step<=steps;++step)
            {
                const float t=static_cast<float>(step)/steps;
                auto wet=MapFeaturePoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
                if (coverage(wet)>=.5f) {hasDry=true;dry=wet;drySegment=i;continue;}
                if (!hasDry || wet.x<0.f || wet.y<0.f || wet.x>=map.width || wet.y>=map.height ||
                    overviewGroundKindFromProjection(map,ground,static_cast<int>(wet.x),static_cast<int>(wet.y))!=MapTileKind::Water) continue;
                const float dx=wet.x-dry.x,dy=wet.y-dry.y,len=std::hypot(dx,dy);
                if (len<.00001f) continue;
                mouth.direction={dx/len,dy/len};
                for (int n=0;n<12;++n) {
                    const MapFeaturePoint mid{(dry.x+wet.x)*.5f,(dry.y+wet.y)*.5f};
                    if (coverage(mid)>=.5f) dry=mid; else wet=mid;
                }
                mouth.joinsWater=true;mouth.shore={(dry.x+wet.x)*.5f,(dry.y+wet.y)*.5f};
                points.resize(drySegment);points.push_back(mouth.shore);
                return mouth;
            }
        }
        return mouth;
    }
    float distance=0.f;
    MapFeaturePoint wet=endpoint;
    for (std::size_t i=points.size()-1;i>0;--i)
    {
        const auto a=points[i],b=points[i-1];
        const float length=std::hypot(b.x-a.x,b.y-a.y);
        const int steps=std::max(1,static_cast<int>(std::ceil(length/.08f)));
        for (int step=1;step<=steps;++step)
        {
            if (distance+length*step/steps>4.f) return mouth;
            const float t=static_cast<float>(step)/steps;
            auto dry=MapFeaturePoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
            if (coverage(dry)>=.5f)
            {
                const float dx=wet.x-dry.x,dy=wet.y-dry.y,len=std::hypot(dx,dy);
                if (len<.00001f) return mouth;
                mouth.direction={dx/len,dy/len};
                for (int n=0;n<12;++n) {
                    const MapFeaturePoint mid{(dry.x+wet.x)*.5f,(dry.y+wet.y)*.5f};
                    if (coverage(mid)>=.5f) dry=mid; else wet=mid;
                }
                mouth.joinsWater=true; mouth.shore={(dry.x+wet.x)*.5f,(dry.y+wet.y)*.5f};
                points.resize(i); points.push_back(mouth.shore);
                return mouth;
            }
            wet=dry;
        }
        distance+=length;
    }
    return mouth;
}

inline std::vector<MapFeaturePoint> illustratedAtlasFinalRiverPoints(const MapModel &map,
    const MapGeneratedFeature &feature)
{
    auto points=illustratedAtlasUntrimmedRiverPoints(map,feature);
    trimIllustratedAtlasRiverMouth(map,points,feature.id.rfind("manual_stroke_",0)==0);
    return points;
}

inline std::vector<gm::map_geometry::MapRoadCorridor> illustratedAtlasRoadCorridors(const MapModel &map)
{
    std::vector<gm::map_geometry::MapRoadCorridor> roads;
    // Artwork is omitted below four pixels/cell. Resolve once for that widest
    // relative minimum stroke, so zoom never makes mountains appear/disappear.
    constexpr float minimumInkCellPixels = 4.f;
    for (const auto &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) continue;
        const bool world = map.atlasLevel == MapLevel::World || usesIllustratedAtlas(map);
        const float surface = usesIllustratedAtlasDetailed(map) ? illustratedAtlasV2RoadWidth(feature,minimumInkCellPixels) :
            std::max(feature.kind == MapGeneratedFeatureKind::Trail ? 2.f : 3.f,
            feature.width * minimumInkCellPixels * atlasPresentationProfile(map).roadWidthCells *
                (world ? kOverviewRoadWidthScale : 1.f));
        const float shoulder = std::max(1.f, minimumInkCellPixels * (world ? .055f : .20f));
        roads.push_back({feature.id, overviewFinalRoadCurvePoints(map, feature),
            gm::map_geometry::roadClearanceRadiusCells((surface + shoulder) / minimumInkCellPixels)});
    }
    for (const auto &route : map.routes)
        roads.push_back({route.metadata.id, gm::map_route_presentation::build(map, route).points, .85f});
    return roads;
}

inline bool atlasPointInsidePolygon(const std::vector<MapFeaturePoint> &polygon, MapFeaturePoint p)
{
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        const auto a = polygon[i], b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y) &&
            p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}

inline bool atlasSilhouetteClearsRoads(const std::vector<MapFeaturePoint> &polygon,
    const std::vector<gm::map_geometry::MapRoadCorridor> &roads, float inkHalfWidth = .03f)
{
    if (polygon.size() < 3) return false;
    for (const auto &road : roads)
    {
        for (const auto p : road.centerline)
            if (atlasPointInsidePolygon(polygon, p)) return false;
        for (std::size_t i = 0; i < polygon.size(); ++i)
            if (gm::map_geometry::roadCorridorIntersectsSegment(road, polygon[i],
                    polygon[(i + 1) % polygon.size()], inkHalfWidth)) return false;
    }
    return true;
}

// V2 uses overlapping, irregular relief and continuous woodland canopies.
// Keep the V1 renderer below intact: saved style pins are appearance contracts.
// Relief along an authored road is a pass, not missing mountains. This ink is
// derived from the reviewed road corridors and terrain, so road moves/erases,
// undo and audience filtering cannot leave independent pass objects behind.
std::vector<AtlasInkPrimitive> buildAtlasMountainCrossingInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &corridors);
std::vector<AtlasInkPrimitive> buildAtlasMountainLandformInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &corridors);
struct AtlasMountainFootprint {
    std::vector<MapFeaturePoint> outline;
    MapFeaturePoint base;
    float width, height;
    unsigned variant;
    bool main;
};
std::vector<AtlasInkPrimitive> buildAtlasMountainButtressInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &corridors,
    const std::vector<AtlasMountainFootprint> &mountains);
std::vector<AtlasInkPrimitive> buildAtlasValleyReliefInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &corridors);
std::vector<AtlasInkPrimitive> buildAtlasCountrysideInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &corridors);
} // namespace gm::map_render
