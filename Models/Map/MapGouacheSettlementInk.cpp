#include "Models/Map/MapGouacheAtlasInk.hpp"
#include <array>

namespace gm::map_render
{
namespace
{
struct House
{
    float x, base, half, height, depth;
    unsigned variation;
};
}

std::vector<AtlasInkPrimitive> buildGouacheAtlasSettlementInk(
    const MapModel &map, MapFeaturePoint center, const std::string &material)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesGouacheAtlasArtwork(map)) return ink;
    const unsigned seed = map.generation.recipe.seed ^
        static_cast<unsigned>(std::floor(center.x)) * 73856093u ^
        static_cast<unsigned>(std::floor(center.y)) * 19349663u;
    const bool village = material.find("village") != std::string::npos;
    const bool city = material.find("city") != std::string::npos || material.find("capital") != std::string::npos;
    const auto p = [&](float x, float y) { return MapFeaturePoint{center.x+x,center.y+y}; };
    const auto tri = [&](MapFeaturePoint a, MapFeaturePoint b, MapFeaturePoint c, OverviewRenderColor color) {
        ink.push_back({{a,b,c},color,0.f,true});
    };
    const auto quad = [&](MapFeaturePoint a, MapFeaturePoint b, MapFeaturePoint c, MapFeaturePoint d, OverviewRenderColor color) {
        tri(a,b,c,color); tri(a,c,d,color);
    };
    const auto line = [&](std::vector<MapFeaturePoint> points, OverviewRenderColor color, float width, float detail=0.f) {
        ink.push_back({std::move(points),color,width,false,detail});
    };
    const auto wash = [&](float x, float y, float rx, float ry, OverviewRenderColor color) {
        for (int i=0;i<18;++i) {
            const float a=i*6.2831853f/18.f,b=(i+1)*6.2831853f/18.f;
            tri(p(x,y),p(x+std::cos(a)*rx,y+std::sin(a)*ry),p(x+std::cos(b)*rx,y+std::sin(b)*ry),color);
        }
    };
    // A quiet painted footing, not the hard oval badge used by old symbols.
    wash(.025f,.12f,village?.48f:.70f,.16f,{153,133,91,35});
    wash(-.02f,.08f,village?.39f:.62f,.10f,{230,205,150,100});
    std::vector<House> houses;
    const int backCount=village?2:city?5:4, frontCount=village?2:city?4:3;
    for (int row=0;row<2;++row) {
        const int count=row?frontCount:backCount;
        for (int i=0;i<count;++i) {
            unsigned h=seed ^ static_cast<unsigned>(i+7+row*13)*2654435761u;
            h^=h>>16; h*=0x7feb352du; h^=h>>15;
            const float spread=village?.48f:row?.93f:.96f;
            const float x=(count==1?0.f:float(i)/(count-1)-.5f)*spread+
                (int((h>>4)%5)-2)*.012f;
            houses.push_back({x,(row?.18f:-.06f)-(h%4)*.025f,
                .105f+((h>>8)%5)*.009f,.19f+((h>>12)%7)*.018f,.07f+((h>>17)%3)*.018f,h});
        }
    }
    const OverviewRenderColor edge{86,76,55,220};
    // Civic silhouettes are cartographic notation, not new campaign entities.
    if (!village) {
        const float tx=(seed%3-1.f)*.10f, top=city?-.91f:-.72f;
        quad(p(tx-.105f,-.05f),p(tx+.10f,-.05f),p(tx+.10f,top),p(tx-.105f,top),{228,214,175,255});
        quad(p(tx+.10f,-.05f),p(tx+.18f,-.12f),p(tx+.18f,top-.02f),p(tx+.10f,top),{162,157,126,255});
        if (seed%3==0) {
            quad(p(tx-.14f,top),p(tx+.13f,top),p(tx+.17f,top-.08f),p(tx-.10f,top-.08f),{114,130,120,255});
            for (int i=0;i<3;++i)
                quad(p(tx-.12f+i*.09f,top-.02f),p(tx-.075f+i*.09f,top-.02f),
                     p(tx-.075f+i*.09f,top-.10f),p(tx-.12f+i*.09f,top-.10f),{202,190,151,255});
        } else {
            tri(p(tx-.15f,top),p(tx-.01f,top-.24f),p(tx+.14f,top),{149,89,62,255});
            tri(p(tx-.01f,top-.24f),p(tx+.22f,top-.035f),p(tx+.14f,top),{108,76,57,255});
            line({p(tx-.15f,top),p(tx-.01f,top-.24f),p(tx+.22f,top-.035f)},edge,.018f);
        }
        line({p(tx-.105f,-.05f),p(tx-.105f,top),p(tx+.10f,top),p(tx+.10f,-.05f)},edge,.017f);
        line({p(tx-.025f,top+.14f),p(tx-.025f,top+.23f)},{64,79,70,230},.047f);
        line({p(tx-.07f,top+.32f),p(tx+.065f,top+.32f)},{177,153,112,205},.025f,18.f);
    }
    std::stable_sort(houses.begin(),houses.end(),[](const House &a,const House &b){return a.base<b.base;});
    constexpr OverviewRenderColor roofs[]={{160,102,70,255},{133,84,65,255},{108,132,124,255},{151,135,94,255}};
    for (const auto &h:houses) {
        const float l=h.x-h.half,r=h.x+h.half,b=h.base,t=b-h.height,d=h.depth;
        const float peak=t-.09f-(h.variation%4)*.017f;
        const auto roof=roofs[(h.variation>>20)%std::size(roofs)];
        const OverviewRenderColor shade{std::uint8_t(roof.r*.78f),std::uint8_t(roof.g*.81f),std::uint8_t(roof.b*.81f),255};
        quad(p(l+.025f,b+.027f),p(r+d+.035f,b-.035f),p(r+d+.035f,b+.01f),p(l+.11f,b+.06f),{91,96,74,43});
        quad(p(l,b),p(r,b),p(r,t),p(l,t),{237,224,184,255});
        quad(p(r,b),p(r+d,b-.065f),p(r+d,t-.065f),p(r,t),{180,173,138,255});
        const float ridge=h.x-h.half*.12f;
        tri(p(l-.025f,t),p(ridge,peak),p(r+.025f,t),roof);
        quad(p(ridge,peak),p(ridge+d,peak-.055f),p(r+d+.025f,t-.055f),p(r+.025f,t),shade);
        // Thin broken eaves and warm roof highlights keep the paint readable
        // at overview zoom without turning every cottage into a black box.
        line({p(l,b),p(l,t),p(ridge,peak),p(r+d+.025f,t-.055f),p(r+d,b-.065f)},edge,.016f);
        line({p(l-.018f,t+.006f),p(r+.02f,t+.006f)},{101,88,62,190},.018f);
        line({p(l+.01f,t-.018f),p(ridge,peak+.018f)},{220,179,119,190},.017f);
        line({p(h.x-.02f,b-.015f),p(h.x-.02f,b-.10f)},{74,86,73,255},.045f);
        if (h.height>.23f) {
            line({p(h.x+.065f,t+.075f),p(h.x+.065f,t+.12f)},{75,92,83,240},.033f);
            line({p(l+.018f,b-.12f),p(r-.018f,b-.12f)},{153,128,90,170},.014f,17.f);
        }
        if ((h.variation>>23)%3==0) {
            line({p(r-.025f,t-.025f),p(r-.025f,t-.13f)},{120,109,84,245},.039f);
            line({p(r-.045f,t-.135f),p(r-.006f,t-.135f)},{223,199,146,240},.015f,20.f);
        }
        for (int course=1;course<3;++course) {
            const float u=course/3.f;
            line({p(l+(ridge-l)*u,t+(peak-t)*u),p(r+(ridge-r)*u,t+(peak-t)*u)},
                {99,84,65,85},.012f,23.f);
        }
    }
    // The footprint stays inside the existing label/woodland reserve.
    line({p(-.53f,.24f),p(-.21f,.26f),p(.09f,.24f)},{143,139,96,105},.021f);
    line({p(.23f,.24f),p(.54f,.20f)},{132,140,96,120},.020f);
    return ink;
}
}
