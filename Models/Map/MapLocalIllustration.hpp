#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapBotanicalInk.hpp"
#include "Models/Map/MapWeatheredStoneInk.hpp"

namespace gm::map_render {
// Explicit immutable recipe gate; opening a previous map does not restyle it.
inline bool usesIllustratedLocalV6(const MapModel &map) {
    return !overviewUsesAtlasTerrain(map) && map.generation.recipe.generatorVersion >= 12 &&
        map.generation.recipe.styleId == "illustrated_atlas" &&
        map.generation.recipe.styleVersion == 6 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6";
}
inline unsigned localIllustrationHash(unsigned seed,int x,int y,unsigned salt=0) {
    unsigned h=seed ^ unsigned(x)*0x9e3779b9u ^ unsigned(y)*0x85ebca6bu ^ salt;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
inline bool usesNaturalLocalRoadSurface(const MapModel &map) {
    return usesIllustratedLocalV6(map) && map.generation.recipe.recipeVersion>=10 &&
        map.scope!=MapScope::Dungeon;
}
std::vector<AtlasInkPrimitive> buildIllustratedLocalRoadDetailInk(
    const MapModel &map, const MapGeneratedFeature &feature, const std::vector<MapFeaturePoint> &points);

inline OverviewRenderColor localIllustrationGround(const MapModel &map,MapTileKind kind) {
    if(map.scope==MapScope::Dungeon) return kind==MapTileKind::Empty || kind==MapTileKind::Wall
        ? OverviewRenderColor{48,54,50,255} : OverviewRenderColor{217,207,175,255};
    switch(kind) {
    case MapTileKind::Water: case MapTileKind::Ice:return {77,139,147,255};
    case MapTileKind::Forest:return {167,184,139,255};
    case MapTileKind::Mountain:return {190,188,165,255};
    case MapTileKind::Stone:return {205,198,178,255};
    case MapTileKind::Hill:return {209,207,162,255};
    case MapTileKind::Dirt: case MapTileKind::Road:return {216,199,157,255};
    case MapTileKind::Building: case MapTileKind::Settlement:return {221,211,173,255};
    case MapTileKind::Swamp:return {166,188,157,255};
    case MapTileKind::Desert:return {233,213,167,255};
    case MapTileKind::Snow:return {234,235,218,255};
    case MapTileKind::Lava:return {180,106,65,255};
    case MapTileKind::Pit: case MapTileKind::Chasm:return {62,75,65,255};
    default:return {218,218,178,255};
    }
}
// Coordinates are cell corners in BOTH backends. No pixel-dependent noise or
// external photo material: a subdued wash leaves the ink and play surface clear.
inline OverviewRenderColor illustratedLocalTerrainColor(const MapModel &map,float x,float y) {
    const float sx=x-.5f,sy=y-.5f;const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
    float r=0,g=0,b=0,total=0;
    const bool underground=map.scope==MapScope::Dungeon;
    float tx=sx-ix,ty=sy-iy;tx=tx*tx*(3-2*tx);ty=ty*ty*(3-2*ty);
    // A compact bicubic wash avoids the square dirt/grass patch boundaries
    // seen in settlement R12. Exact obstacles and water get their own ink.
    // Masonry already has an explicit wall silhouette. A broad outdoor wash
    // around it creates a bright halo in the supposedly solid underground.
    for(int dy=underground?0:-1;dy<=(underground?1:2);++dy)for(int dx=underground?0:-1;dx<=(underground?1:2);++dx) {
        const auto *cell=mapCellAt(map,std::clamp(ix+dx,0,map.width-1),std::clamp(iy+dy,0,map.height-1));
        auto kind=cell?cell->kind:MapTileKind::Empty;
        // Route-only cells must not leave a secret route-shaped terrain wash.
        if(kind==MapTileKind::Road || kind==MapTileKind::Bridge)kind=MapTileKind::Empty;
        const auto c=localIllustrationGround(map,kind);
        const float ax=std::max(0.f,1.f-std::abs(float(ix+dx)+.5f-x)/2.f),ay=std::max(0.f,1.f-std::abs(float(iy+dy)+.5f-y)/2.f);
        const float w=underground?(dx?tx:1-tx)*(dy?ty:1-ty):ax*ax*(3.f-2.f*ax)*ay*ay*(3.f-2.f*ay);total+=w;
        r+=c.r*w;g+=c.g*w;b+=c.b*w;
    }
    if(!underground && total>.0001f){r/=total;g/=total;b/=total;}
    const float phase=(map.generation.recipe.seed%127)*.043f;
    const float wash=2.2f*std::sin(x*.18f+y*.11f+phase)+1.4f*std::cos(y*.27f-x*.09f-phase)+
        (!underground && map.generation.recipe.recipeVersion>=10
            ? illustrationPigmentWash(map.generation.recipe.seed,x,y)*1.35f : 0.f);
    auto v=[&](float c){return static_cast<std::uint8_t>(std::clamp(std::lround(c+wash),0l,255l));};
    return {v(r),v(g),v(b),255};
}
inline std::vector<AtlasInkPrimitive> buildLocalIllustrationInk(const MapModel &map,bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;
    if(!usesIllustratedLocalV6(map) || map.scope==MapScope::Dungeon)return ink;
    if(map.generation.recipe.recipeVersion>=10)appendCivicPavingInk(ink,map,applyFog);
    auto stroke=[&](std::vector<MapFeaturePoint> p,OverviewRenderColor c,float w){ink.push_back({std::move(p),c,w,false});};
    auto fill=[&](const std::vector<MapFeaturePoint>&p,MapFeaturePoint center,OverviewRenderColor c,float minPixels=0.f,float maxPixels=0.f){
        for(size_t i=0;i<p.size();++i)ink.push_back({{center,p[i],p[(i+1)%p.size()]},c,.01f,true,minPixels,maxPixels});};
    auto crown=[&](float x,float y,float radius,unsigned h) {
        const OverviewRenderColor edge{64,96,76,255};
        std::vector<MapFeaturePoint> p;
        for(int i=0;i<28;++i){const float a=i*6.2831853f/28.f;const float r=radius*(.91f+.09f*std::cos(a*7.f+float(h%13)));
            p.push_back({x+std::cos(a)*r,y+std::sin(a)*r});}
        const auto body=h%5==0?OverviewRenderColor{147,164,104,255}:OverviewRenderColor{117,155,114,255};
        // At fit-to-window scale each crown is only a few pixels wide. Keep
        // its silhouette while avoiding dozens of invisible leaf triangles.
        std::vector<MapFeaturePoint> distant;
        for(int i=0;i<8;++i){const float a=i*6.2831853f/8.f;
            distant.push_back({x+std::cos(a)*radius*.96f,y+std::sin(a)*radius*.96f});}
        fill(distant,{x,y},body,0.f,4.f);
        auto shadow=p;for(auto &v:shadow){v.x+=.10f;v.y+=.14f;}fill(shadow,{x+.10f,y+.14f},{98,120,88,135},4.f);
        if(map.generation.recipe.recipeVersion>=10) {
            appendBotanicalCanopyInk(ink,p,{x,y},radius,radius,h,false,true);
            return;
        }
        fill(p,{x,y},body,4.f);
        auto loop=p;loop.push_back(loop.front());stroke(loop,edge,.047f);
        const int lobes=3+int(h%3);
        for(int l=0;l<lobes;++l){const float a=(l*6.2831853f/lobes)+float(h%23)*.27f;float lx=x+std::cos(a)*radius*.34f,ly=y+std::sin(a)*radius*.30f;
            std::vector<MapFeaturePoint> leaf;
            for(int j=0;j<9;++j){const float t=j*6.2831853f/9;leaf.push_back({lx+std::cos(t)*radius*.41f,ly+std::sin(t)*radius*.34f});}
            fill(leaf,{lx,ly},l%2?OverviewRenderColor{145,177,123,255}:OverviewRenderColor{156,184,132,255},6.f);
            stroke({leaf[5],leaf[6],leaf[7],leaf[8]}, {91,129,95,215},.029f);
        }
        stroke({{x-.05f,y+radius*.32f},{x+.10f,y+.04f},{x+.30f,y-.15f}},edge,.025f);
    };
    const auto visible=[&](int x,int y){const auto*c=mapCellAt(map,x,y);return c&&(!applyFog||!c->fogged);};
    // A staggered, jittered display lattice is independent of the collision
    // grid. Crowns and their shadows must fit revealed forest ground, so this
    // cannot add trees to a road, clearing, or fogged neighboring cell.
    for(int row=0;row*.82f<map.height;++row)for(int col=0;col*.89f<map.width;++col){
        const unsigned h=localIllustrationHash(map.generation.recipe.seed,col,row,197u);
        const float cx=col*.89f+(row%2)*.445f+.34f+float(int((h>>5)%101)-50)*.0047f;
        const float cy=row*.82f+.36f+float(int((h>>13)%101)-50)*.0043f;
        const auto forest=[&](float x,float y){const int ix=int(std::floor(x)),iy=int(std::floor(y));const auto*c=mapCellAt(map,ix,iy);
            return c&&c->kind==MapTileKind::Forest&&c->wayKind==MapWayKind::None&&visible(ix,iy);};
        if(!forest(cx,cy))continue;
        const auto fits=[&](float radius){for(int k=0;k<16;++k){const float a=k*6.2831853f/16;
            const float x=cx+std::cos(a)*radius,y=cy+std::sin(a)*radius;
            if(!forest(x,y)||!forest(x+.10f,y+.14f))return false;}return true;};
        float radius=.56f+float(h%9)*.023f;
        while(radius>=.25f&&!fits(radius))radius-=.09f;
        if(radius>=.25f)crown(cx,cy,radius,h);
    }
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        const auto*c=mapCellAt(map,x,y);if(!c||!visible(x,y))continue;
        const unsigned h=localIllustrationHash(map.generation.recipe.seed,x,y);
        if(c->kind==MapTileKind::Empty && c->wayKind==MapWayKind::None && h%19==0) {
            const float px=x+.3f,py=y+.5f;
            stroke({{px-.10f,py},{px-.18f,py-.13f}}, {150,161,109,160},.035f);
            stroke({{px,py+.02f},{px+.05f,py-.16f}}, {150,161,109,160},.035f);
        } else if(c->kind==MapTileKind::Wall && map.mapKind!=MapKind::Settlement) {
            if(map.generation.recipe.recipeVersion>=10) {
                appendWeatheredRuinWallInk(ink,map,x,y,applyFog);
                continue;
            }
            // Tactical ruin walls remain semantic blocked cells. Render their
            // shared footprint as masonry instead of suppressing the old art
            // without providing an equivalent obstacle silhouette.
            const float l=float(x),t=float(y),r=l+1,b=t+1;
            fill({{l,t},{r,t},{r,b},{l,b}},{l+.5f,t+.5f},{177,171,143,255});
            constexpr int off[][2]={{0,-1},{1,0},{0,1},{-1,0}};
            for(int side=0;side<4;++side){const auto*n=mapCellAt(map,x+off[side][0],y+off[side][1]);if(n&&n->kind==MapTileKind::Wall&&visible(x+off[side][0],y+off[side][1]))continue;
                const MapFeaturePoint a=side==0?MapFeaturePoint{l,t}:side==1?MapFeaturePoint{r,t}:side==2?MapFeaturePoint{r,b}:MapFeaturePoint{l,b};
                const MapFeaturePoint z=side==0?MapFeaturePoint{r,t}:side==1?MapFeaturePoint{r,b}:side==2?MapFeaturePoint{l,b}:MapFeaturePoint{l,t};
                stroke({a,z},{88,101,89,255},.17f);
                const float dx=z.x-a.x,dy=z.y-a.y;
                stroke({{a.x-dy*.16f,a.y+dx*.16f},{z.x-dy*.16f,z.y+dx*.16f}},side<2?OverviewRenderColor{222,211,175,255}:OverviewRenderColor{138,143,117,255},.07f);
            }
            if(h%3==0)stroke({{l+.24f,t+.12f},{l+.35f,t+.43f},{l+.28f,t+.71f}},{135,144,121,190},.032f);
        }
    }
    // Layered cliff edges describe continuous blocked areas, without tiling a
    // mountain icon or filling the entire mass with repetitive chevrons.
    for(auto p:naturalCaveContours(map,applyFog,MapTileKind::Mountain)) {
        if(p.empty())continue;p.push_back(p.front());
        // Directional rock faces, not three concentric circuit-board outlines.
        // Facets sit inside the blocked contour and leave the dry path readable.
        for(size_t i=1;i<p.size();++i){const auto a=p[i-1],b=p[i];float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<.03f)continue;
            const float depth=.34f+.16f*std::sin(a.x*2.1f+a.y*1.3f);
            const MapFeaturePoint ai{a.x-dy/len*depth,a.y+dx/len*depth},bi{b.x-dy/len*depth,b.y+dx/len*depth};
            const auto face=dx-dy>0?OverviewRenderColor{103,124,103,255}:OverviewRenderColor{231,220,183,255};
            ink.push_back({{a,b,bi},face,.01f,true});ink.push_back({{a,bi,ai},face,.01f,true});
            // Short fractures describe the cliff face instead of a uniform
            // bevel around a flat UI-like panel. They follow the rock side.
            for(float d=.27f;d<len;d+=.67f){const float t=d/len;
                const float x=a.x+dx*t,y=a.y+dy*t;
                stroke({{x-dy/len*.06f,y+dx/len*.06f},
                        {x-dy/len*depth*.7f+dx/len*.05f,y+dx/len*depth*.7f+dy/len*.05f}},
                        {78,102,83,205},.035f);
            }
        }
        stroke(p,{79,102,86,245},.075f);
    }
    for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x){
        const auto*c=mapCellAt(map,x,y);const unsigned h=localIllustrationHash(map.generation.recipe.seed,x,y,41);
        if(c->kind!=MapTileKind::Mountain||!visible(x,y)||h%13)continue;
        stroke({{x+.13f,y+.74f},{x+.43f,y+.45f},{x+.69f,y+.49f},{x+.87f,y+.24f}},{125,139,122,160},.035f);
        stroke({{x+.43f,y+.45f},{x+.38f,y+.25f}},{125,139,122,130},.025f);
    }
    // Broad irregular stones break up a plateau into a readable rock formation.
    // Every vertex stays on blocked cells; no decorative rock obstructs play.
    for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x){
        const auto*c=mapCellAt(map,x,y);const unsigned h=localIllustrationHash(map.generation.recipe.seed,x,y,771);
        if(c->kind!=MapTileKind::Mountain||!visible(x,y)||h%5)continue;
        std::vector<MapFeaturePoint> stone;const float cx=x+.5f,cy=y+.5f;
        bool allowed=true;
        for(int k=0;k<7;++k){const float a=k*6.2831853f/7;const float r=.49f+float((h>>(k*3))&7)*.058f;
            const MapFeaturePoint v{cx+std::cos(a)*r,cy+std::sin(a)*r*.68f};stone.push_back(v);
            const auto*n=mapCellAt(map,int(std::floor(v.x)),int(std::floor(v.y)));allowed&=n&&n->kind==MapTileKind::Mountain&&visible(int(std::floor(v.x)),int(std::floor(v.y)));}
        if(!allowed)continue;
        fill(stone,{cx-.11f,cy-.10f},{200,200,177,255});
        ink.push_back({{{cx-.11f,cy-.10f},stone[0],stone[1]},{153,165,145,255},.01f,true});
        ink.push_back({{{cx-.11f,cy-.10f},stone[1],stone[2]},{171,179,157,255},.01f,true});
        ink.push_back({{{cx-.11f,cy-.10f},stone[4],stone[5]},{225,218,189,255},.01f,true});
        auto border=stone;border.push_back(border.front());stroke(border,{123,143,122,220},.032f);
    }
    return ink;
}
inline std::vector<std::vector<MapFeaturePoint>> localIllustrationWaterContours(const MapModel &map,bool applyFog) {
    auto contours=naturalCaveContours(map,applyFog,MapTileKind::Water);
    for(auto &p:contours) {
        const auto original=p;
        for(int pass=0;pass<5;++pass) {
            const auto before=p;
            for(size_t i=0;i<p.size();++i){
                const auto a=before[(i+p.size()-1)%p.size()],b=before[(i+1)%p.size()];
                const auto o=original[i];
                if(o.x<=.001f||o.y<=.001f||o.x>=map.width-.001f||o.y>=map.height-.001f)continue;
                MapFeaturePoint q{before[i].x*.5f+(a.x+b.x)*.25f,before[i].y*.5f+(a.y+b.y)*.25f};
                const float dx=q.x-o.x,dy=q.y-o.y,d=std::hypot(dx,dy);
                if(d>.22f){q.x=o.x+dx*.22f/d;q.y=o.y+dy*.22f/d;}
                p[i]=q;
            }
        }
    }
    return contours;
}
inline std::vector<AtlasInkPrimitive> buildLocalIllustrationWaterInk(const MapModel &map,bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;
    if(!usesIllustratedLocalV6(map)||map.scope==MapScope::Dungeon)return ink;
    const auto contours=localIllustrationWaterContours(map,applyFog);
    // Even/odd scan conversion preserves dry islands. Painting a land-coloured
    // polygon over a filled lake erased island trees and masonry underneath.
    // Resolve a triangle mesh once, shared by the editor and exported image.
    std::vector<float> rows;
    for(const auto&p:contours)for(const auto&v:p)rows.push_back(v.y);
    std::sort(rows.begin(),rows.end());
    rows.erase(std::unique(rows.begin(),rows.end(),[](float a,float b){return std::abs(a-b)<.00001f;}),rows.end());
    struct Crossing {float middle,top,bottom;};
    for(size_t row=1;row<rows.size();++row){
        const float top=rows[row-1],bottom=rows[row],middle=(top+bottom)*.5f;
        if(bottom-top<.00001f)continue;
        std::vector<Crossing> crossings;
        for(const auto&p:contours)for(size_t i=0;i<p.size();++i){
            const auto a=p[i],b=p[(i+1)%p.size()];
            if(middle<=std::min(a.y,b.y)||middle>=std::max(a.y,b.y))continue;
            const auto at=[&](float y){return a.x+(b.x-a.x)*(y-a.y)/(b.y-a.y);};
            crossings.push_back({at(middle),at(top),at(bottom)});
        }
        std::sort(crossings.begin(),crossings.end(),[](const auto&a,const auto&b){return a.middle<b.middle;});
        for(size_t i=1;i<crossings.size();i+=2){const auto a=crossings[i-1],b=crossings[i];
            ink.push_back({{{a.top,top},{b.top,top},{b.bottom,bottom}},{77,139,147,255},.01f,true});
            ink.push_back({{{a.top,top},{b.bottom,bottom},{a.bottom,bottom}},{77,139,147,255},.01f,true});
        }
    }
    for(auto p:contours){if(p.empty())continue;p.push_back(p.front());
        ink.push_back({p,{144,180,157,255},.19f,false});
        ink.push_back({p,{63,116,123,255},.045f,false});}
    for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x){
        auto*c=mapCellAt(map,x,y);if(c->kind!=MapTileKind::Water||(applyFog&&c->fogged)||localIllustrationHash(map.generation.recipe.seed,x,y,45)%11)continue;
        ink.push_back({{{x+.19f,y+.49f},{x+.35f,y+.46f},{x+.52f,y+.49f},{x+.69f,y+.47f}},{173,206,188,130},.033f,false});
    }
    return ink;
}
}
