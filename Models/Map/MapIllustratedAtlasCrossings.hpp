#pragma once

#include "Models/Map/MapIllustratedAtlasRoutes.hpp"

namespace gm::map_render
{
inline std::vector<MapGeneratedFeature> illustratedAtlasCrossingBridges(const MapModel &map)
{
    std::vector<MapGeneratedFeature> bridges;
    for (const auto &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::Bridge && feature.points.size() >= 2 &&
            (feature.locked || feature.id.rfind("manual_bridge_",0)!=0 ||
             std::find(map.generation.recipe.lockedFeatureIds.begin(),map.generation.recipe.lockedFeatureIds.end(),feature.id)!=map.generation.recipe.lockedFeatureIds.end()))
            bridges.push_back(feature);
    struct WaterCurve { std::vector<MapFeaturePoint> points; float halfWidth; };
    std::vector<WaterCurve> rivers;
    for (const auto &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::River && feature.points.size() >= 2)
            rivers.push_back({illustratedAtlasFinalRiverPoints(map, feature),
                std::max(.065f, std::max(1.f,feature.width)*atlasPresentationProfile(map).riverWidthCells*.5f)});
    for (const auto &road : map.generation.graph.features)
    {
        if (road.kind != MapGeneratedFeatureKind::Road && road.kind != MapGeneratedFeatureKind::Trail) continue;
        const auto points = overviewFinalRoadCurvePoints(map, road);
        const float deckHalf = std::max(.16f, illustratedAtlasV2RoadWidth(road, 32.f)/64.f+.045f);
        for (const auto &river : rivers)
            for (std::size_t r = 1; r < points.size(); ++r)
                for (std::size_t w = 1; w < river.points.size(); ++w)
                {
                    const auto a = points[r-1], b = points[r], c = river.points[w-1], d = river.points[w];
                    const float dx=b.x-a.x, dy=b.y-a.y, wx=d.x-c.x, wy=d.y-c.y;
                    const float denominator=dx*wy-dy*wx;
                    if (std::abs(denominator)<.000001f) continue;
                    const float t=((c.x-a.x)*wy-(c.y-a.y)*wx)/denominator;
                    const float u=((c.x-a.x)*dy-(c.y-a.y)*dx)/denominator;
                    if (t<0.f || t>1.f || u<0.f || u>1.f) continue;
                    const float length=std::hypot(dx,dy), waterLength=std::hypot(wx,wy);
                    const float sine=std::abs(denominator)/(length*waterLength);
                    if (sine<.12f) continue; // A shared channel is not a local crossing.
                    const MapFeaturePoint centre{a.x+t*dx,a.y+t*dy};
                    bool covered=false;
                    for (const auto &bridge : bridges)
                        if (gm::map_geometry::roadPointSegmentDistanceSquared(centre,
                                bridge.points.front(),bridge.points.back())<.25f)
                        { covered=true; break; }
                    if (covered) continue;
                    float along=length*t;
                    for(std::size_t i=1;i<r;++i)along+=std::hypot(points[i].x-points[i-1].x,points[i].y-points[i-1].y);
                    const float roadLength=detail::overviewPathLength(points);
                    const auto endOnBank=[&](float direction) {
                        MapFeaturePoint end=centre;
                        for(float distance=.15f;distance<=8.f;distance+=.10f)
                        {
                            const float at=std::clamp(along+direction*distance,0.f,roadLength);
                            end=detail::sampleOverviewPath(points,at).point;
                            float nearest=std::numeric_limits<float>::max();
                            for(std::size_t i=1;i<river.points.size();++i)
                                nearest=std::min(nearest,gm::map_geometry::roadPointSegmentDistanceSquared(end,river.points[i-1],river.points[i]));
                            const float clearance=river.halfWidth+.16f+deckHalf*.2f;
                            if(nearest>clearance*clearance || at==0.f || at==roadLength)break;
                        }
                        return end;
                    };
                    MapGeneratedFeature bridge;
                    bridge.kind=MapGeneratedFeatureKind::Bridge;
                    bridge.id="resolved_crossing_"+std::to_string(bridges.size());
                    bridge.width=deckHalf*2.f;
                    bridge.points={endOnBank(-1.f),endOnBank(1.f)};
                    bridges.push_back(std::move(bridge));
                }
    }
    return bridges;
}

std::vector<AtlasInkPrimitive> buildIllustratedAtlasFeatureInk(const MapModel &map, bool ways, bool symbols);
} // namespace gm::map_render
