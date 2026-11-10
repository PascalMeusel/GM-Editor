#pragma once

#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapTileSemantics.hpp"

namespace gm::map_render {

// Compatibility cells remain the authored data. This presentation replaces
// photographic atlas tiles and green-backed road prototypes with the same
// restrained palette and ink vocabulary used by current illustrated maps.
inline OverviewRenderColor classicGroundColor(MapTileKind kind) {
    switch(kind) {
    case MapTileKind::Water: case MapTileKind::Bridge:return {67,132,145,255};
    case MapTileKind::Forest:return {185,197,151,255};
    case MapTileKind::Mountain:case MapTileKind::Stone:return {205,199,174,255};
    case MapTileKind::Hill:return {214,207,165,255};
    case MapTileKind::Room:case MapTileKind::Cave:case MapTileKind::Door:case MapTileKind::Stairs:return {217,207,175,255};
    case MapTileKind::Wall:return {111,117,103,255};
    case MapTileKind::Dirt:return {218,199,157,255};
    case MapTileKind::Snow:return {232,235,219,255};
    case MapTileKind::Desert:return {237,218,172,255};
    case MapTileKind::Swamp:return {167,189,159,255};
    case MapTileKind::Lava:return {173,91,52,255};
    case MapTileKind::Ice:return {172,209,206,255};
    case MapTileKind::Pit:case MapTileKind::Chasm:return {49,61,57,255};
    default:return {220,217,178,255};
    }
}

inline bool classicCellIsSymbol(MapTileKind kind) {
    return kind==MapTileKind::Building||kind==MapTileKind::Settlement||kind==MapTileKind::Landmark||
        kind==MapTileKind::Cover||kind==MapTileKind::Rubble||kind==MapTileKind::Hazard||
        kind==MapTileKind::Door||kind==MapTileKind::Stairs;
}

inline std::vector<AtlasInkPrimitive> classicCellInk(const MapTileRenderContext& context,
                                                   bool ground=true,bool symbols=true) {
    std::vector<AtlasInkPrimitive> ink;
    const auto p=[&](float x,float y){return MapFeaturePoint{context.tileX+x,context.tileY+y};};
    const auto line=[&](std::vector<MapFeaturePoint> points,OverviewRenderColor color,float width=.04f){
        ink.push_back({std::move(points),color,width,false});};
    const auto poly=[&](std::vector<MapFeaturePoint> points,OverviewRenderColor color){
        for(std::size_t i=1;i+1<points.size();++i)ink.push_back({{points[0],points[i],points[i+1]},color,0,true});};
    const auto rect=[&](float x,float y,float w,float h,OverviewRenderColor color){poly({p(x,y),p(x+w,y),p(x+w,y+h),p(x,y+h)},color);};
    const OverviewRenderColor edge{64,76,61,255},pale{234,216,163,255},wood{153,108,62,255};
    if(ground)rect(0,0,1,1,classicGroundColor(context.kind));
    if(classicCellIsSymbol(context.kind)?!symbols:!ground)return ink;
    const auto kind=context.kind;
    if(kind==MapTileKind::Forest) {
        const float phase=(unsigned(context.tileX)*197u+unsigned(context.tileY)*397u)%17u*.15f;
        for(int tree=0;tree<3;++tree) {
            const float cx=tree==0?.31f:tree==1?.69f:.49f,cy=tree==0?.38f:tree==1?.48f:.72f;
            std::vector<MapFeaturePoint> crown;
            for(int i=0;i<=10;++i){const float a=i*6.2831853f/10,r=.22f+.025f*std::sin(a*3+phase+tree);crown.push_back(p(cx+r*std::cos(a),cy+r*std::sin(a)));}
            poly(crown,tree==1?OverviewRenderColor{101,151,111,255}:OverviewRenderColor{129,166,116,255});line(crown,edge,.028f);
            line({p(cx-.06f,cy-.11f),p(cx+.07f,cy-.13f),p(cx+.12f,cy-.06f)},{169,190,131,255},.04f);
        }
    } else if(kind==MapTileKind::Mountain) {
        poly({p(.08f,.85f),p(.40f,.14f),p(.65f,.78f)},{145,162,154,255});
        poly({p(.40f,.14f),p(.65f,.78f),p(.91f,.87f)},{113,139,139,255});
        poly({p(.28f,.41f),p(.40f,.14f),p(.54f,.48f),p(.42f,.39f),p(.37f,.45f)},pale);
        line({p(.08f,.85f),p(.40f,.14f),p(.91f,.87f)},edge,.036f);
        line({p(.40f,.44f),p(.36f,.69f),p(.24f,.82f)},{105,124,114,255},.025f);
    } else if(kind==MapTileKind::Hill) {
        line({p(.10f,.65f),p(.29f,.43f),p(.49f,.36f),p(.68f,.43f),p(.88f,.65f)},{157,150,108,255},.035f);
        line({p(.26f,.79f),p(.43f,.65f),p(.62f,.61f),p(.79f,.72f)},{171,158,114,255},.025f);
    } else if(kind==MapTileKind::Water||kind==MapTileKind::Ice||kind==MapTileKind::Swamp) {
        for(float y:{.32f,.69f})line({p(.19f,y),p(.37f,y-.025f),p(.53f,y+.02f),p(.73f,y)},kind==MapTileKind::Swamp?OverviewRenderColor{94,137,116,170}:OverviewRenderColor{127,178,179,180},.022f);
        if(kind==MapTileKind::Swamp)for(float x:{.25f,.67f})line({p(x,.57f),p(x-.05f,.32f),p(x,.57f),p(x+.08f,.36f)},edge,.025f);
    } else if(kind==MapTileKind::Building||kind==MapTileKind::Settlement) {
        const bool blue=(unsigned(context.tileX)*37u+unsigned(context.tileY)*91u)%3u==0;
        rect(.16f,.20f,.72f,.67f,{113,111,86,70});rect(.13f,.16f,.72f,.67f,{225,206,157,255});
        poly({p(.13f,.16f),p(.85f,.16f),p(.76f,.45f),p(.22f,.45f)},blue?OverviewRenderColor{95,146,164,255}:OverviewRenderColor{194,111,73,255});
        poly({p(.22f,.45f),p(.76f,.45f),p(.85f,.70f),p(.13f,.70f)},blue?OverviewRenderColor{57,101,120,255}:OverviewRenderColor{135,72,48,255});
        line({p(.13f,.16f),p(.85f,.16f),p(.85f,.83f),p(.13f,.83f),p(.13f,.16f)},edge,.032f);
        line({p(.22f,.45f),p(.76f,.45f)},pale,.035f);line({p(.49f,.72f),p(.49f,.82f)},edge,.075f);
        rect(.66f,.24f,.09f,.14f,pale);
    } else if(kind==MapTileKind::Room||kind==MapTileKind::Stone) {
        line({p(.08f,.51f),p(.93f,.51f)},{166,160,134,130},.018f);
        line({p(.49f,.08f),p(.49f,.50f),p(.72f,.51f),p(.72f,.94f)},{166,160,134,130},.018f);
    } else if(kind==MapTileKind::Wall) {
        rect(.07f,.08f,.86f,.84f,{151,151,125,255});
        line({p(.08f,.10f),p(.92f,.10f)},pale,.035f);
        line({p(.07f,.50f),p(.93f,.50f)},edge,.03f);
        line({p(.38f,.08f),p(.38f,.49f)},edge,.025f);line({p(.70f,.51f),p(.70f,.92f)},edge,.025f);
    } else if(kind==MapTileKind::Stairs||kind==MapTileKind::Door) {
        const int turn=automaticMapTileRotation(context);
        const auto r=[&](float x,float y){for(int i=0;i<turn;++i){const float old=x;x=1-y;y=old;}return p(x,y);};
        if(kind==MapTileKind::Stairs) {
            poly({r(.14f,.10f),r(.86f,.10f),r(.86f,.88f),r(.14f,.88f)},edge);
            for(int step=0;step<5;++step){const float y=.22f+step*.13f,h=.18f+step*.037f;line({r(.5f-h,y),r(.5f+h,y)},pale,.065f);}
        } else {
            poly({r(.12f,.36f),r(.88f,.36f),r(.88f,.63f),r(.12f,.63f)},wood);
            line({r(.12f,.36f),r(.88f,.36f),r(.88f,.63f),r(.12f,.63f),r(.12f,.36f)},edge,.04f);
            line({r(.26f,.39f),r(.26f,.60f)},pale,.055f);line({r(.74f,.39f),r(.74f,.60f)},pale,.055f);
        }
    } else if(kind==MapTileKind::Cover) {
        rect(.16f,.19f,.70f,.65f,edge);rect(.20f,.23f,.62f,.57f,wood);
        line({p(.25f,.28f),p(.76f,.73f)},pale,.06f);line({p(.76f,.28f),p(.25f,.73f)},pale,.06f);
    } else if(kind==MapTileKind::Rubble) {
        for(int i=0;i<3;++i){const float x=.18f+i*.24f,y=i==1?.27f:.58f;
            poly({p(x,y),p(x+.16f,y-.04f),p(x+.22f,y+.12f),p(x+.06f,y+.19f)},{145,152,126,255});
            line({p(x,y),p(x+.16f,y-.04f),p(x+.22f,y+.12f)},edge,.025f);}
    } else if(kind==MapTileKind::Hazard||kind==MapTileKind::Landmark) {
        const OverviewRenderColor color=kind==MapTileKind::Hazard?OverviewRenderColor{142,96,143,255}:OverviewRenderColor{207,161,77,255};
        poly({p(.50f,.12f),p(.86f,.51f),p(.50f,.89f),p(.14f,.51f)},color);
        line({p(.50f,.12f),p(.86f,.51f),p(.50f,.89f),p(.14f,.51f),p(.50f,.12f)},edge,.04f);
        line({p(.5f,.30f),p(.5f,.57f)},pale,.055f);
    } else if(kind==MapTileKind::Lava) {
        line({p(.06f,.68f),p(.34f,.45f),p(.48f,.48f),p(.65f,.17f),p(.95f,.08f)},{241,177,67,255},.08f);
    } else if(kind==MapTileKind::Pit||kind==MapTileKind::Chasm) {
        line({p(.1f,.23f),p(.32f,.13f),p(.64f,.16f),p(.86f,.35f)},{158,162,130,255},.06f);
    }
    return ink;
}

inline std::vector<AtlasInkPrimitive> classicWayInk(MapWayRenderContext context,
                                                  const MapTileRenderContext& terrain) {
    std::vector<AtlasInkPrimitive> ink;
    // Older files may encode a road/bridge entirely in terrain cells.
    if(context.wayKind==MapWayKind::None && (terrain.kind==MapTileKind::Road||terrain.kind==MapTileKind::Bridge)) {
        context.wayKind=MapWayKind::DirtRoad;
        for(std::size_t i=0;i<context.neighbors.size();++i) {
            const auto& source=terrain.neighbors[i];
            if(source.exists&&(source.kind==MapTileKind::Road||source.kind==MapTileKind::Bridge)) {
                context.neighbors[i].exists=true;context.neighbors[i].wayKind=MapWayKind::DirtRoad;
            }
        }
    }
    if(context.wayKind==MapWayKind::None)return ink;
    const auto p=[&](float x,float y){return MapFeaturePoint{context.tileX+x,context.tileY+y};};
    const auto stroke=[&](std::vector<MapFeaturePoint> points,OverviewRenderColor color,float width){ink.push_back({std::move(points),color,width,false});};
    const bool river=mapWayKindIsWaterLike(context.wayKind),bridge=mapWayRendersAsBridge(context);
    const float width=river?.28f:bridge?.32f:context.wayKind==MapWayKind::Trail?.13f:.20f;
    const OverviewRenderColor edge=river?OverviewRenderColor{50,102,116,255}:OverviewRenderColor{152,132,90,255};
    const OverviewRenderColor color=river?OverviewRenderColor{69,139,151,255}:bridge?OverviewRenderColor{167,124,70,255}:OverviewRenderColor{195,173,120,255};
    const int mask=mapWayEightNeighborMask(context);
    constexpr float dx[]={0,1,1,1,0,-1,-1,-1},dy[]={-1,-1,0,1,1,1,0,-1};
    std::vector<int> directions;
    for(int d=0;d<8;++d)if(mask&(1<<d)) {
        // A filled 2x2 bend already joins through its cardinal neighbours.
        // Adding both diagonal spokes produces the old star-shaped crossings.
        if(d%2 && ((mask&(1<<((d+7)%8)))||(mask&(1<<((d+1)%8)))))continue;
        directions.push_back(d);
    }
    if(directions.empty()) {
        const bool horizontal=terrain.rotationQuarterTurns%2;
        stroke({p(horizontal?.18f:.5f,horizontal?.5f:.18f),p(horizontal?.82f:.5f,horizontal?.5f:.82f)},edge,width+.045f);
        stroke({p(horizontal?.18f:.5f,horizontal?.5f:.18f),p(horizontal?.82f:.5f,horizontal?.5f:.82f)},color,width);
    } else {
        // Connected cell edges have no terminal cap. A full outlined capsule
        // per spoke paints little dark rings over the next cell's road fill.
        for(int d:directions) {
            const float length=std::hypot(dx[d],dy[d]);
            const float nx=-dy[d]/length*(width*.5f+.012f),ny=dx[d]/length*(width*.5f+.012f);
            for(float side:{-1.f,1.f})
                stroke({p(.5f+nx*side,.5f+ny*side),p(.5f+dx[d]*.5f+nx*side,.5f+dy[d]*.5f+ny*side)},edge,.026f);
        }
        for(int d:directions)stroke({p(.5f,.5f),p(.5f+dx[d]*.5f,.5f+dy[d]*.5f)},color,width);
    }
    if(bridge) {
        const bool horizontal=(mask&(4|64))!=0;
        for(float along:{.20f,.38f,.56f,.74f})
            stroke(horizontal?std::vector<MapFeaturePoint>{p(along,.33f),p(along,.67f)}:
                std::vector<MapFeaturePoint>{p(.33f,along),p(.67f,along)},{227,207,157,255},.024f);
    }
    return ink;
}
}
