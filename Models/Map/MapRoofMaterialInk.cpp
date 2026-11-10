#include "Models/Map/MapRoofMaterialInk.hpp"
#include "Models/Map/MapIllustrationPigment.hpp"
#include <array>

namespace gm::map_render
{
namespace
{
float cross(MapFeaturePoint a, MapFeaturePoint b, MapFeaturePoint p)
{ return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x); }

std::vector<MapFeaturePoint> clipToFace(std::vector<MapFeaturePoint> polygon,
                                     const std::vector<MapFeaturePoint> &face)
{
    float area=0.f;
    for(std::size_t i=0;i<face.size();++i)
        area+=face[i].x*face[(i+1)%face.size()].y-face[i].y*face[(i+1)%face.size()].x;
    const float sign=area>=0?1.f:-1.f;
    for(std::size_t edge=0;edge<face.size() && !polygon.empty();++edge)
    {
        std::vector<MapFeaturePoint> clipped;
        const auto a=face[edge],b=face[(edge+1)%face.size()];
        auto previous=polygon.back();float previousSide=cross(a,b,previous)*sign;
        for(const auto current:polygon)
        {
            const float side=cross(a,b,current)*sign;
            if((side>=0)!=(previousSide>=0))
            {
                const float t=previousSide/(previousSide-side);
                clipped.push_back({previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t});
            }
            if(side>=0)clipped.push_back(current);
            previous=current;previousSide=side;
        }
        polygon=std::move(clipped);
    }
    return polygon;
}

OverviewRenderColor tint(OverviewRenderColor base, float value)
{
    const auto channel=[&](int c){return static_cast<std::uint8_t>(std::clamp(int(std::lround(c+value)),0,255));};
    return {channel(base.r),channel(base.g),channel(base.b),255};
}

OverviewRenderColor blend(OverviewRenderColor a, OverviewRenderColor b,float t)
{
    const auto channel=[&](int x,int y){return static_cast<std::uint8_t>(std::lround(x+(y-x)*t));};
    return {channel(a.r,b.r),channel(a.g,b.g),channel(a.b,b.b),255};
}
}

void appendWeatheredRoofMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                                 float left, float top, float right, float bottom,
                                 OverviewRenderColor light, OverviewRenderColor dark)
{
    if(right<=left || bottom<=top)return;
    const bool horizontal=right-left>=bottom-top;
    const float u0=horizontal?left:top,u1=horizontal?right:bottom;
    const float v0=horizontal?top:left,v1=horizontal?bottom:right;
    const float middle=(v0+v1)*.5f,half=(v1-v0)*.5f;
    const float inset=std::min((u1-u0)*.27f,(v1-v0)*.38f);
    const MapFeaturePoint ridgeA{u0+inset,middle},ridgeB{u1-inset,middle};
    const auto world=[&](MapFeaturePoint p){return horizontal?p:MapFeaturePoint{p.y,p.x};};
    const auto surface=[&](const std::vector<MapFeaturePoint> &polygon,OverviewRenderColor base,float minimum)
    {
        for(std::size_t i=1;i+1<polygon.size();++i)
        {
            if(std::abs(cross(polygon[0],polygon[i],polygon[i+1]))<.0000001f)continue;
            AtlasInkPrimitive triangle{{world(polygon[0]),world(polygon[i]),world(polygon[i+1])},base,0.f,true,minimum};
            for(const auto p:{polygon[0],polygon[i],polygon[i+1]})
            {
                const float towardRidge=1.f-std::abs(p.y-middle)/std::max(.01f,half);
                const auto w=world(p);
                triangle.vertexColors.push_back(tint(base,-5.f+towardRidge*9.f+
                    illustrationPigmentNoise(map.generation.recipe.seed^0x4917u,w.x*.9f,w.y*.9f)*3.f));
            }
            ink.push_back(std::move(triangle));
        }
    };
    const auto face=[&](std::vector<MapFeaturePoint> polygon,OverviewRenderColor base,bool hip)
    {
        surface(polygon,base,0.f);
        // Roof ends have their own downhill courses. Clip each tile to its
        // actual face, so narrow/edited roofs never gain floating shingles.
        std::vector<MapFeaturePoint> aligned=polygon;
        if(hip)for(auto &p:aligned)std::swap(p.x,p.y);
        float minU=aligned[0].x,maxU=minU,minV=aligned[0].y,maxV=minV;
        for(const auto p:aligned){minU=std::min(minU,p.x);maxU=std::max(maxU,p.x);minV=std::min(minV,p.y);maxV=std::max(maxV,p.y);}
        // Bound detail even for a large authored warehouse. Ordinary roofs
        // keep the same tile scale; very large faces use larger courses.
        const float course=std::max(.24f,(maxV-minV)/48.f);
        const float tile=std::max(.39f,(maxU-minU)/96.f);
        int row=0;
        for(float v=minV;v<maxV;v+=course,++row)
            for(float u=minU-tile+(row%2)*tile*.5f;u<maxU;u+=tile)
            {
                auto piece=clipToFace({{u+.012f,v+.015f},{u+tile-.012f,v+.015f},
                                      {u+tile-.012f,v+course-.018f},{u+.012f,v+course-.018f}},aligned);
                if(piece.size()<3)continue;
                if(hip)for(auto &p:piece)std::swap(p.x,p.y);
                const auto sample=world(piece.front());
                const float variation=illustrationPigmentNoise(map.generation.recipe.seed^0x85a9u,sample.x*17.f,sample.y*17.f)*8.f;
                surface(piece,tint(base,variation+2.f),8.f);
                // One restrained worn edge per tile; no high-contrast grid.
                if(piece.size()==4)
                    ink.push_back({{world(piece[2]),world(piece[3])},{219,209,180,48},.016f,false,12.f});
            }
    };
    face({{u0,v0},{u1,v0},ridgeB,ridgeA},light,false);
    face({{u0,v1},ridgeA,ridgeB,{u1,v1}},blend(dark,light,.34f),false);
    face({{u0,v0},ridgeA,{u0,v1}},blend(light,dark,.26f),true);
    face({{u1,v0},{u1,v1},ridgeB},blend(dark,light,.18f),true);
    const auto seam=[&](std::initializer_list<MapFeaturePoint> points,OverviewRenderColor color,float width)
    {
        std::vector<MapFeaturePoint> line;for(const auto p:points)line.push_back(world(p));
        ink.push_back({std::move(line),color,width,false});
    };
    seam({ridgeA,ridgeB},blend(light,{223,207,163,255},.50f),.046f);
    seam({{u0,v0},ridgeA,{u0,v1}},{69,62,50,125},.025f);
    seam({{u1,v0},ridgeB,{u1,v1}},{58,57,48,140},.025f);
}
}
