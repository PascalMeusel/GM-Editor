#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"

namespace gm::map_render {
inline bool usesUndergroundIllustrationV6(const MapModel &map) {
    return map.generation.recipe.generatorVersion >= 12 && map.generation.recipe.styleVersion >= 6;
}
inline unsigned undergroundInkHash(unsigned seed,int x,int y,int salt=0) {
    unsigned h=seed ^ static_cast<unsigned>(x)*0x9e3779b9u ^ static_cast<unsigned>(y)*0x85ebca6bu ^ static_cast<unsigned>(salt)*0xc2b2ae35u;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
inline void undergroundInkPolygon(std::vector<AtlasInkPrimitive> &ink,const std::vector<MapFeaturePoint> &p,OverviewRenderColor c) {
    if(p.size()<3)return;
    MapFeaturePoint center{};for(auto q:p){center.x+=q.x;center.y+=q.y;}
    center.x/=static_cast<float>(p.size());center.y/=static_cast<float>(p.size());
    for(std::size_t i=0;i<p.size();++i)ink.push_back({{center,p[i],p[(i+1)%p.size()]},c,0,true});
}
inline void undergroundInkLine(std::vector<AtlasInkPrimitive> &ink,std::vector<MapFeaturePoint> p,OverviewRenderColor c,float width=.035f) {
    ink.push_back({std::move(p),c,width,false});
}
inline std::vector<MapFeaturePoint> undergroundInkOval(float x,float y,float rx,float ry,unsigned h,int n=12) {
    std::vector<MapFeaturePoint> p;
    const float phase=(h%628)*.01f;
    for(int i=0;i<n;++i) {
        const float a=i*6.2831853f/n;
        const float r=1.f+.09f*std::sin(a*3.f+phase)+.04f*std::cos(a*5.f-phase);
        p.push_back({x+std::cos(a)*rx*r,y+std::sin(a)*ry*r});
    }
    return p;
}
inline void undergroundInkClosedLine(std::vector<AtlasInkPrimitive> &ink,std::vector<MapFeaturePoint> p,OverviewRenderColor c,float width=.035f) {
    if(p.empty())return;p.push_back(p.front());undergroundInkLine(ink,std::move(p),c,width);
}
inline void undergroundInkStone(std::vector<AtlasInkPrimitive> &ink,float x,float y,float rx,float ry,unsigned h,bool crystal=false) {
    const auto rim=undergroundInkOval(x,y,rx,ry,h,crystal?6:9);
    auto shadow=rim;for(auto &p:shadow){p.x+=.07f;p.y+=.08f;}
    undergroundInkPolygon(ink,shadow,{44,46,40,110});
    undergroundInkPolygon(ink,rim,crystal?OverviewRenderColor{66,135,148,255}:OverviewRenderColor{134,133,109,255});
    const MapFeaturePoint peak{x-rx*.15f,y-ry*.2f};
    for(std::size_t i=0;i<rim.size();++i) {
        const auto a=rim[i],b=rim[(i+1)%rim.size()];
        const bool lit=(a.x+b.x)*.4f+(a.y+b.y)*.6f<x*.8f+y*1.2f;
        ink.push_back({{peak,a,b},crystal?(lit?OverviewRenderColor{156,211,207,255}:OverviewRenderColor{74,145,157,255}):(lit?OverviewRenderColor{195,187,150,255}:OverviewRenderColor{122,124,103,255}),0,true});
    }
    undergroundInkClosedLine(ink,rim,crystal?OverviewRenderColor{44,83,95,255}:OverviewRenderColor{70,76,64,255},.045f);
    undergroundInkLine(ink,{{x-rx*.62f,y-ry*.2f},{peak.x,peak.y},{x+rx*.24f,y-ry*.55f}},crystal?OverviewRenderColor{221,237,210,240}:OverviewRenderColor{221,211,174,220},.04f);
}
}
