#include "Models/Map/MapWeatheredStoneInk.hpp"
#include "Models/Map/MapLocalIllustration.hpp"

namespace gm::map_render
{
namespace
{
OverviewRenderColor tint(OverviewRenderColor color,int amount)
{
    const auto channel=[&](int c){return static_cast<std::uint8_t>(std::clamp(c+amount,0,255));};
    return {channel(color.r),channel(color.g),channel(color.b),color.a};
}

void stone(std::vector<AtlasInkPrimitive> &ink,const std::vector<MapFeaturePoint> &outline,
           OverviewRenderColor body,float minimumPixels=0.f,float relief=1.f)
{
    if(outline.size()<3)return;
    MapFeaturePoint center{};
    for(const auto p:outline){center.x+=p.x;center.y+=p.y;}
    center.x/=outline.size();center.y/=outline.size();
    for(std::size_t i=0;i<outline.size();++i)
    {
        const auto a=outline[i],b=outline[(i+1)%outline.size()];
        const auto shade=[&](MapFeaturePoint p){return tint(body,int((p.x+p.y<center.x+center.y?9:-7)*relief));};
        AtlasInkPrimitive piece{{center,a,b},body,0.f,true,minimumPixels};
        piece.vertexColors={tint(body,int(3*relief)),shade(a),shade(b)};
        ink.push_back(std::move(piece));
    }
}

std::vector<MapFeaturePoint> chamfered(float l,float t,float r,float b,float chip)
{
    return {{l+chip,t},{r-chip*.8f,t},{r,t+chip},{r,b-chip*.7f},
            {r-chip,b},{l+chip*.8f,b},{l,b-chip},{l,t+chip*.7f}};
}
}

void appendWeatheredRuinWallInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map,
                               int x,int y,bool applyFog)
{
    const auto visibleWall=[&](int px,int py){const auto *c=mapCellAt(map,px,py);
        return c && c->kind==MapTileKind::Wall && (!applyFog || !c->fogged);};
    if(!visibleWall(x,y))return;
    const float l=float(x),t=float(y),r=l+1.f,b=t+1.f;
    const unsigned hash=localIllustrationHash(map.generation.recipe.seed,x,y,0xc391u);
    stone(ink,{{l,t},{r,t},{r,b},{l,b}},{144,145,121,255});
    const bool vertical=(visibleWall(x,y-1)||visibleWall(x,y+1)) &&
                        !(visibleWall(x-1,y)||visibleWall(x+1,y));
    const auto point=[&](float u,float v){return vertical?MapFeaturePoint{l+v,t+u}:MapFeaturePoint{l+u,t+v};};
    // A pair of staggered capstone courses gives walls a material scale. Their
    // chipped corners remain inside the original, fully blocked cell.
    for(int row=0;row<2;++row)
    {
        const float split=.35f+float((hash>>(row*4))%6u)*.055f;
        for(int block=0;block<2;++block)
        {
            const float u0=block?split+.018f:.022f,u1=block?.978f:split-.018f;
            const float v0=row*.5f+.026f,v1=(row+1)*.5f-.026f;
            auto polygon=chamfered(u0,v0,u1,v1,.045f+float(hash%3u)*.012f);
            for(auto &p:polygon)p=point(p.x,p.y);
            stone(ink,polygon,tint({181,177,148,255},int((hash>>(block*3+row))%13u)-6));
            ink.push_back({{polygon[7],polygon[0],polygon[1]}, {225,214,176,170},.024f,false,9.f});
            if((hash+row+block)%3u==0u)
                ink.push_back({{point(u0+.06f,v0+.08f),point(u0+.13f,v0+.16f),point(u0+.09f,v0+.27f)},
                               {111,122,100,145},.023f,false,14.f});
        }
    }
    constexpr int offsets[][2]={{0,-1},{1,0},{0,1},{-1,0}};
    const std::array<MapFeaturePoint,4> corners{{{l,t},{r,t},{r,b},{l,b}}};
    for(int side=0;side<4;++side)
    {
        if(visibleWall(x+offsets[side][0],y+offsets[side][1]))continue;
        const auto a=corners[side],z=corners[(side+1)%4];
        const float dx=z.x-a.x,dy=z.y-a.y;
        // Inset faces do not move the obstacle into its neighboring alley.
        const MapFeaturePoint ai{a.x-dy*.12f,a.y+dx*.12f},zi{z.x-dy*.12f,z.y+dx*.12f};
        stone(ink,{a,z,zi,ai},side==0||side==3?OverviewRenderColor{201,193,157,255}:OverviewRenderColor{115,128,108,255});
        ink.push_back({{a,z},{78,92,77,230},.065f,false});
        ink.push_back({{ai,zi},{225,214,175,175},.025f,false,8.f});
    }
}

void appendCivicPavingInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map,bool applyFog)
{
    const auto kind=map.metadata.customProperties.find("tactical.template_kind");
    if(kind==map.metadata.customProperties.end() || kind->second!="5")return;
    const auto onFloor=[&](float x,float y){const auto *c=mapCellAt(map,int(std::floor(x)),int(std::floor(y)));
        return c && c->kind==MapTileKind::Stone && (!applyFog || !c->fogged);};
    // Offset courses belong to the plaza surface, not to the gameplay grid.
    // Reject boundary pieces instead of paving over a roof, water or fog.
    for(int row=0;row*.63f<map.height;++row)
        for(int col=-1;col*1.06f<map.width;++col)
        {
            const unsigned h=localIllustrationHash(map.generation.recipe.seed,col,row,0x9cb1u);
            const float l=col*1.06f+(row%2)*.53f+.025f,t=row*.63f+.025f;
            const float r=l+1.005f,b=t+.577f;
            bool clear=true;
            for(int yy=0;yy<=3 && clear;++yy)for(int xx=0;xx<=5;++xx)
                clear=clear && onFloor(l+(r-l)*xx/5.f,t+(b-t)*yy/3.f);
            if(!clear)continue;
            const auto polygon=chamfered(l,t,r,b,.035f+(h%4u)*.012f);
            stone(ink,polygon,tint({201,194,172,255},int(h%11u)-5),9.f,.35f);
            ink.push_back({{polygon[3],polygon[4],polygon[5]}, {116,124,104,65},.022f,false,12.f});
            ink.push_back({{polygon[7],polygon[0],polygon[1]}, {228,218,184,90},.021f,false,12.f});
            if(h%11u==0u)
                ink.push_back({{{l+.17f,t+.07f},{l+.25f,t+.17f},{l+.19f,t+.29f}},
                               {127,136,110,105},.020f,false,20.f});
        }
}

void appendWeatheredAltarInk(std::vector<AtlasInkPrimitive> &ink,MapFeaturePoint center,unsigned seed)
{
    const float x=center.x,y=center.y;
    // Retain the previous 1.8 x .96-cell setpiece footprint.
    const auto base=chamfered(x-.90f,y-.48f,x+.90f,y+.48f,.11f);
    stone(ink,base,{87,101,89,255});
    auto loop=base;loop.push_back(loop.front());
    ink.push_back({loop,{45,61,54,245},.055f,false});
    const auto top=chamfered(x-.80f,y-.40f,x+.80f,y+.28f,.085f);
    stone(ink,top,{164,164,133,255});
    ink.push_back({{top[7],top[0],top[1],top[2]}, {216,205,166,240},.04f,false});
    ink.push_back({{top[3],top[4],top[5]}, {66,83,74,225},.045f,false});
    ink.push_back({{{x-.24f,y-.05f},{x,y-.27f},{x+.24f,y-.05f},{x,y+.17f},{x-.24f,y-.05f}},
                   {62,98,88,235},.049f,false});
    ink.push_back({{{x-.15f,y-.05f},{x,y-.19f},{x+.15f,y-.05f}}, {182,205,166,220},.025f,false,10.f});
    for(int i=0;i<3;++i)
    {
        const float xx=x-.53f+i*.5f;
        ink.push_back({{{xx,y+.32f},{xx+.015f,y+.42f}}, {144,149,119,165},.026f,false,12.f});
    }
    const float side=(seed%2u)?-1.f:1.f;
    ink.push_back({{{x+side*.68f,y-.31f},{x+side*.56f,y-.19f},{x+side*.63f,y-.10f}},
                   {88,108,88,155},.023f,false,16.f});
}

void appendFittedFountainInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map)
{
    const MapFeaturePoint center{map.width/2+.5f,map.height/2+.5f};
    for(const auto &contour:localIllustrationWaterContours(map,true))
    {
        if(!atlasPointInsidePolygon(contour,center))continue;
        // Only decorate an entirely revealed, compact central basin. Edits
        // joining it to a lake or hiding its edge fall back to truthful water.
        if(std::any_of(contour.begin(),contour.end(),[&](auto p){return std::hypot(p.x-center.x,p.y-center.y)>3.4f;}))continue;
        bool revealed=true;
        for(int y=map.height/2-3;y<=map.height/2+3;++y)
            for(int x=map.width/2-3;x<=map.width/2+3;++x)
                if(const auto *c=mapCellAt(map,x,y);!c || c->fogged)revealed=false;
        if(!revealed)continue;
        auto edge=contour;edge.push_back(edge.front());
        ink.push_back({edge,{72,91,80,255},.40f,false});
        ink.push_back({edge,{176,180,151,255},.31f,false});
        ink.push_back({edge,{219,209,169,210},.12f,false});
        // The coping follows the very same contour as the water mesh, so no
        // blue lobes protrude outside a disconnected circular symbol.
        float accumulated=0.f;
        for(std::size_t i=1;i<edge.size();++i)
        {
            const auto a=edge[i-1],b=edge[i];const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
            if(len<.0001f)continue;
            accumulated+=len;
            if(accumulated<.55f)continue;
            accumulated=0.f;const MapFeaturePoint p{(a.x+b.x)*.5f,(a.y+b.y)*.5f};
            ink.push_back({{{p.x-dy/len*.15f,p.y+dx/len*.15f},{p.x+dy/len*.15f,p.y-dx/len*.15f}},
                           {95,111,92,180},.027f,false,12.f});
        }
        std::vector<MapFeaturePoint> pedestal;
        for(int k=0;k<8;++k){const float a=k*6.2831853f/8.f;pedestal.push_back({center.x+std::cos(a)*.29f,center.y+std::sin(a)*.29f});}
        stone(ink,pedestal,{183,190,157,255});
        pedestal.push_back(pedestal.front());ink.push_back({pedestal,{85,115,99,225},.035f,false});
        ink.push_back({{{center.x-.1f,center.y},{center.x+.1f,center.y}}, {234,227,184,245},.07f,false});
        break;
    }
}
}
