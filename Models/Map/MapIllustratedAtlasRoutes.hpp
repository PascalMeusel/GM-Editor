#pragma once

#include "Models/Map/MapIllustratedAtlasInk.hpp"

namespace gm::map_render
{
inline std::vector<MapFeaturePoint> atlasRouteEntrancePoints(const MapModel &map,std::vector<MapFeaturePoint> points)
{
    if(!usesIllustratedAtlas(map)) return points;
    for(const auto &city:map.generation.graph.features)
    {
        if(city.kind!=MapGeneratedFeatureKind::Settlement || city.points.empty()) continue;
        const auto c=city.points.front();
        const auto inside=[&](MapFeaturePoint p){const float x=(p.x-c.x)/.95f,y=(p.y-c.y)/.65f;return x*x+y*y<1.f;};
        for(int end=0;end<2;++end)
        {
            if(end) std::reverse(points.begin(),points.end());
            while(points.size()>1 && inside(points.front()) && inside(points[1])) points.erase(points.begin());
            if(points.size()>1 && inside(points.front()))
            {
                const auto a=points.front(),b=points[1];float lo=0,hi=1;
                for(int step=0;step<24;++step) {const float t=(lo+hi)*.5f;if(inside({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t})) lo=t;else hi=t;}
                points.front()={a.x+(b.x-a.x)*hi,a.y+(b.y-a.y)*hi};
            }
            if(end) std::reverse(points.begin(),points.end());
        }
    }
    return points;
}

inline std::vector<MapFeaturePoint> illustratedAtlasRoadInkPoints(const MapModel &map,const MapGeneratedFeature &road)
{
    auto points=overviewFinalRoadCurvePoints(map,road);
    if(!usesIllustratedAtlasV5(map) || points.size()<2)return points;
    const auto complete=points;
    const float total=detail::overviewPathLength(complete);
    const float inset=std::min(total*.2f,(illustratedAtlasV2RoadWidth(road,32.f)/32.f+.055f)*.5f);
    for(bool start:{true,false})
    {
        const auto p=start?complete.front():complete.back();
        bool joined=false;
        for(const auto &other:map.generation.graph.features)
        {
            if(other.id==road.id || (other.kind!=MapGeneratedFeatureKind::Road && other.kind!=MapGeneratedFeatureKind::Trail))continue;
            for(std::size_t i=1;i<other.points.size();++i)
                if(gm::map_geometry::roadPointSegmentDistanceSquared(p,other.points[i-1],other.points[i])<.00001f)joined=true;
        }
        if(!joined)continue;
        // Hide the rasterizer's rounded end cap inside the receiving road.
        // The separate flat junction surface reaches the exact semantic join.
        float walked=0.f;
        if(start)
        {
            std::size_t first=0;
            while(first+1<points.size() && walked+std::hypot(points[first+1].x-points[first].x,points[first+1].y-points[first].y)<inset)
            {walked+=std::hypot(points[first+1].x-points[first].x,points[first+1].y-points[first].y);++first;}
            points.erase(points.begin(),points.begin()+first);
            points.front()=detail::sampleOverviewPath(complete,inset).point;
        }
        else
        {
            std::size_t last=points.size()-1;
            while(last>0 && walked+std::hypot(points[last].x-points[last-1].x,points[last].y-points[last-1].y)<inset)
            {walked+=std::hypot(points[last].x-points[last-1].x,points[last].y-points[last-1].y);--last;}
            points.resize(last+1);
            points.back()=detail::sampleOverviewPath(complete,total-inset).point;
        }
    }
    return points;
}

// A compact walled forecourt makes a road meet a town entrance. It is painted
// over through-roads, before the settlement artwork, in every render backend.
inline std::vector<AtlasInkPrimitive> buildAtlasCityEntrances(const MapModel &map)
{
    std::vector<AtlasInkPrimitive> ink;
    if(!usesIllustratedAtlas(map)) return ink;
    constexpr float tau=6.28318530718f, rx=.95f, ry=.65f;
    auto ways=map.generation.graph.features;
    for(const auto &route:map.routes) {
        MapGeneratedFeature path;path.kind=MapGeneratedFeatureKind::Road;
        path.points=gm::map_route_presentation::build(map,route).points;
        ways.push_back(std::move(path));
    }
    for(const auto &city:map.generation.graph.features)
    {
        if(city.kind!=MapGeneratedFeatureKind::Settlement || city.points.empty()) continue;
        const auto center=city.points.front();
        if(!overviewGroundKindIsLand(overviewGroundKindForRender(map,
            static_cast<int>(std::floor(center.x)),static_cast<int>(std::floor(center.y)))))continue;
        std::vector<float> gates;
        for(const auto &road:ways)
        {
            if(road.kind!=MapGeneratedFeatureKind::Road && road.kind!=MapGeneratedFeatureKind::Trail) continue;
            for(std::size_t i=1;i<road.points.size();++i)
            {
                const auto a=road.points[i-1], b=road.points[i];
                const float x=(a.x-center.x)/rx,y=(a.y-center.y)/ry;
                const float dx=(b.x-a.x)/rx,dy=(b.y-a.y)/ry;
                const float A=dx*dx+dy*dy,B=2*(x*dx+y*dy),C=x*x+y*y-1;
                const float disc=B*B-4*A*C;
                if(A<.000001f || disc<0) continue;
                for(float t:{(-B-std::sqrt(disc))/(2*A),(-B+std::sqrt(disc))/(2*A)})
                {
                    if(t<0 || t>1) continue;
                    const float angle=std::atan2(y+t*dy,x+t*dx);
                    if(std::none_of(gates.begin(),gates.end(),[&](float old){return std::abs(std::remainder(angle-old,tau))<.25f;})) gates.push_back(angle);
                }
            }
        }
        if(gates.empty()) continue;
        const auto at=[&](float a,float scale=1.f){return MapFeaturePoint{center.x+rx*std::cos(a)*scale,center.y+ry*std::sin(a)*scale};};
        for(int i=0;i<48;++i)
        {
            const float a=tau*i/48.f,b=tau*(i+1)/48.f;
            ink.push_back({{center,at(a),at(b)},{234,218,177,255},0,true});
            const float middle=(a+b)*.5f;
            if(std::none_of(gates.begin(),gates.end(),[&](float gate){return std::abs(std::remainder(middle-gate,tau))<.23f;}))
                ink.push_back({{at(a),at(b)},{128,112,80,230},.045f,false});
        }
        for(float gate:gates)
            for(float side:{-.21f,.21f})
                ink.push_back({{at(gate+side,.88f),at(gate+side,1.13f)},{103,84,53,255},.08f,false});
    }
    return ink;
}

// Resolve crossings from the same curves that are displayed. They are derived
// artwork, so moving either way updates bridges without creating saved objects
// or leaving auto-created bridge objects behind. Call with reviewed geometry.
} // namespace gm::map_render
