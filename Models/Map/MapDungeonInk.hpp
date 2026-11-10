#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapUndergroundIllustrationV6.hpp"
#include "Models/Map/MapUndergroundMaterialInk.hpp"

namespace gm::map_render {
inline bool usesDungeonMasonryInk(const MapModel &map)
{
    const auto type=map.metadata.customProperties.find("tactical.template_kind");
    const bool tacticalDungeon=map.creationIntent==MapCreationIntent::TacticalEncounter &&
        type!=map.metadata.customProperties.end() && type->second=="7";
    return map.scope == MapScope::Dungeon && (map.creationIntent == MapCreationIntent::Dungeon || tacticalDungeon) &&
           map.generation.recipe.generatorVersion >= 11;
}

inline void appendDungeonIllustrationV6(std::vector<AtlasInkPrimitive> &ink,const MapModel &map,bool applyFog) {
    const auto floor=[&](int x,int y) {
        const auto*c=mapCellAt(map,x,y);
        return c&&c->kind!=MapTileKind::Wall&&c->kind!=MapTileKind::Empty&&(!applyFog||!c->fogged);
    };
    for(const auto &f:map.generation.graph.features) {
        if(f.materialId!="fantasy_neutral.dungeon.room"||f.points.empty())continue;
        float l=f.points[0].x,r=l,t=f.points[0].y,b=t;
        for(auto p:f.points){l=std::min(l,p.x);r=std::max(r,p.x);t=std::min(t,p.y);b=std::max(b,p.y);}
        if(r-l<5||b-t<5)continue;
        const float cx=(l+r)*.5f,cy=(t+b)*.5f;
        bool clear=true;
        for(int y=static_cast<int>(cy-2);y<=static_cast<int>(cy+2);++y)for(int x=static_cast<int>(cx-2);x<=static_cast<int>(cx+2);++x) {
            const auto*c=mapCellAt(map,x,y);
            if(!floor(x,y)||!c||(c->kind!=MapTileKind::Room&&c->kind!=MapTileKind::Cover&&c->kind!=MapTileKind::Rubble&&c->kind!=MapTileKind::Hazard))clear=false;
        }
        if(!clear)continue;
        const unsigned h=undergroundInkHash(map.generation.recipe.seed,static_cast<int>(cx),static_cast<int>(cy),101);
        if(h%5==0) {
            const auto circle=[&](float radius,int count) {
                std::vector<MapFeaturePoint> p;
                for(int i=0;i<count;++i){const float a=i*6.2831853f/count;p.push_back({cx+std::cos(a)*radius,cy+std::sin(a)*radius});}
                return p;
            };
            auto medallion=circle(1.65f,32);
            undergroundInkPolygon(ink,medallion,{111,139,130,38});
            undergroundInkClosedLine(ink,medallion,{108,129,116,200},.10f);
            undergroundInkClosedLine(ink,circle(1.45f,32),{220,203,156,220},.07f);
            for(int i=0;i<8;++i) {
                const float a=i*.785398f;
                const MapFeaturePoint tip{cx+std::cos(a)*1.25f,cy+std::sin(a)*1.25f};
                undergroundInkPolygon(ink,{{cx,cy},{cx+std::cos(a-.22f)*.48f,cy+std::sin(a-.22f)*.48f},tip},{113,135,122,90});
                undergroundInkLine(ink,{{cx+std::cos(a)*.45f,cy+std::sin(a)*.45f},tip},{124,134,115,180},.045f);
            }
            undergroundInkClosedLine(ink,circle(.4f,16),{124,132,114,230},.08f);
        } else if(h%5==1) {
            undergroundInkClosedLine(ink,{{cx-1.5f,cy-1.15f},{cx+1.5f,cy-1.15f},{cx+1.5f,cy+1.15f},{cx-1.5f,cy+1.15f}},{121,131,114,150},.10f);
            for(int row=-1;row<=1;++row)for(int col=-2;col<=2;++col) {
                const float xx=cx+col*.5f,yy=cy+row*.55f;
                undergroundInkPolygon(ink,{{xx,yy-.19f},{xx+.19f,yy},{xx,yy+.19f},{xx-.19f,yy}},{129,147,130,50});
            }
        } else if(h%5==2||h%5==3) {
            const OverviewRenderColor rug=h%5==2?OverviewRenderColor{105,131,126,190}:OverviewRenderColor{150,102,82,175};
            const std::vector<MapFeaturePoint> cloth{{cx-1.65f,cy-1.13f},{cx+1.62f,cy-1.10f},{cx+1.68f,cy+1.12f},{cx-1.60f,cy+1.15f}};
            undergroundInkPolygon(ink,cloth,rug);undergroundInkClosedLine(ink,cloth,{99,89,66,180},.04f);
            undergroundInkClosedLine(ink,{{cx-1.45f,cy-.92f},{cx+1.44f,cy-.92f},{cx+1.45f,cy+.93f},{cx-1.43f,cy+.93f}},{203,182,131,220},.09f);
            for(int i=-2;i<=2;++i) {
                const float xx=cx+i*.5f;
                undergroundInkClosedLine(ink,{{xx,cy-.42f},{xx+.25f,cy},{xx,cy+.42f},{xx-.25f,cy}},{217,193,143,190},.055f);
            }
            for(int i=0;i<11;++i) {
                const float xx=cx-1.5f+i*.3f;
                undergroundInkLine(ink,{{xx,cy-1.13f},{xx+.025f,cy-1.27f}},{202,181,135,180},.035f);
                undergroundInkLine(ink,{{xx,cy+1.14f},{xx-.035f,cy+1.26f}},{202,181,135,180},.035f);
            }
        } else {
            undergroundInkClosedLine(ink,{{cx-1.5f,cy-1.15f},{cx+1.5f,cy-1.15f},{cx+1.5f,cy+1.15f},{cx-1.5f,cy+1.15f}},{119,137,119,125},.065f);
            for(int sy:{-1,1})for(int sx:{-1,1}) {
                const float xx=cx+sx*1.32f,yy=cy+sy*.98f;
                undergroundInkPolygon(ink,{{xx,yy},{xx-sx*.48f,yy},{xx,yy-sy*.45f}},{110,135,120,55});
                undergroundInkLine(ink,{{xx-sx*.45f,yy-sy*.12f},{xx-sx*.12f,yy-sy*.45f}},{217,202,154,180},.065f);
            }
        }
    }
    constexpr int offsets[][2]={{0,-1},{1,0},{0,1},{-1,0}};
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        if(!floor(x,y))continue;
        for(int corner=0;corner<4;++corner) {
            const int sx=corner==0||corner==3?-1:1,sy=corner<2?-1:1;
            const auto solid=[&](int xx,int yy){const auto*c=mapCellAt(map,xx,yy);return !c||c->kind==MapTileKind::Wall||c->kind==MapTileKind::Empty;};
            if(!solid(x+sx,y)||!solid(x,y+sy)||!solid(x+sx,y+sy))continue;
            const float xx=x+(sx>0?1.f:0.f),yy=y+(sy>0?1.f:0.f);
            undergroundInkPolygon(ink,{{xx,yy},{xx+sx*.48f,yy},{xx+sx*.48f,yy+sy*.48f},{xx,yy+sy*.48f}},{134,133,109,255});
            undergroundInkLine(ink,{{xx+sx*.48f,yy},{xx+sx*.48f,yy+sy*.48f},{xx,yy+sy*.48f}},{57,63,55,255},.09f);
        }
        for(int side=0;side<4;++side) {
            const auto*n=mapCellAt(map,x+offsets[side][0],y+offsets[side][1]);
            // Fog is not a physical wall: its final mask remains authoritative.
            if(n&&n->kind!=MapTileKind::Wall&&n->kind!=MapTileKind::Empty)continue;
            MapFeaturePoint a{static_cast<float>(x),static_cast<float>(y)},b{a.x+1,a.y};
            if(side==1){a={x+1.f,static_cast<float>(y)};b={x+1.f,y+1.f};}
            else if(side==2){a={x+1.f,y+1.f};b={static_cast<float>(x),y+1.f};}
            else if(side==3){a={static_cast<float>(x),y+1.f};b={static_cast<float>(x),static_cast<float>(y)};}
            const float nx=static_cast<float>(offsets[side][0]),ny=static_cast<float>(offsets[side][1]);
            const MapFeaturePoint ao{a.x+nx*.48f,a.y+ny*.48f},bo{b.x+nx*.48f,b.y+ny*.48f};
            const unsigned h=undergroundInkHash(map.generation.recipe.seed,x,y,side);
            const unsigned char shade=static_cast<unsigned char>(h%12);
            undergroundInkPolygon(ink,{a,b,{b.x-nx*.15f,b.y-ny*.15f},{a.x-nx*.15f,a.y-ny*.15f}},{55,56,47,110});
            if(map.generation.recipe.recipeVersion>=10)
                appendUndergroundWallMaterial(ink,map,ao,bo,a,b,false);
            else
                undergroundInkPolygon(ink,{a,b,bo,ao},{static_cast<unsigned char>(130+shade),static_cast<unsigned char>(129+shade),static_cast<unsigned char>(105+shade),255});
            undergroundInkLine(ink,{ao,bo},{57,63,55,255},.09f);
            undergroundInkLine(ink,{a,b},{65,69,57,255},.09f);
            undergroundInkLine(ink,{{a.x+nx*.09f,a.y+ny*.09f},{b.x+nx*.09f,b.y+ny*.09f}},{202,190,150,230},.055f);
            undergroundInkLine(ink,{a,ao},{77,83,68,210},.045f);
            if(h%5==0)undergroundInkLine(ink,{{a.x+(b.x-a.x)*.6f+nx*.13f,a.y+(b.y-a.y)*.6f+ny*.13f},{a.x+(b.x-a.x)*.7f+nx*.4f,a.y+(b.y-a.y)*.7f+ny*.4f}},{93,95,77,170},.028f);
        }
    }
}
inline std::vector<AtlasInkPrimitive> buildDungeonFloorInk(const MapModel &map, bool applyFog)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesDungeonMasonryInk(map)) return ink;
    const bool v6=usesUndergroundIllustrationV6(map);
    struct StoneRoom {float l,r,t,b;unsigned theme;};
    std::vector<StoneRoom> stoneRooms;
    if(v6)for(const auto &f:map.generation.graph.features) {
        if(f.materialId!="fantasy_neutral.dungeon.room"||f.points.empty())continue;
        StoneRoom room{f.points[0].x,f.points[0].x,f.points[0].y,f.points[0].y,0};
        for(auto p:f.points){room.l=std::min(room.l,p.x);room.r=std::max(room.r,p.x);room.t=std::min(room.t,p.y);room.b=std::max(room.b,p.y);}
        room.theme=undergroundInkHash(map.generation.recipe.seed,static_cast<int>((room.l+room.r)*.5f),static_cast<int>((room.t+room.b)*.5f),101)%5;
        stoneRooms.push_back(room);
    }
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x)
    {
        const auto *cell=mapCellAt(map,x,y);
        if(!cell || cell->kind==MapTileKind::Wall || cell->kind==MapTileKind::Empty || (applyFog && cell->fogged)) continue;
        const int slabColumn=v6 && map.generation.recipe.recipeVersion>=10 ? (x+(y/2)%2)/2 : x/2;
        const unsigned h=static_cast<unsigned>(slabColumn)*73856093u ^ static_cast<unsigned>(y/2)*19349663u ^ map.generation.recipe.seed;
        const int shade=static_cast<int>((h>>8)%9);
        OverviewRenderColor floor=v6?OverviewRenderColor{static_cast<std::uint8_t>(184+shade),static_cast<std::uint8_t>(176+shade),static_cast<std::uint8_t>(146+shade),255}:OverviewRenderColor{static_cast<std::uint8_t>(157+shade),static_cast<std::uint8_t>(151+shade),static_cast<std::uint8_t>(131+shade),255};
        for(const auto &room:stoneRooms)if(x+.5f>=room.l&&x+.5f<=room.r&&y+.5f>=room.t&&y+.5f<=room.b) {
            // Room-wide stone families create hierarchy without changing cover,
            // terrain or the recognizable warm corridor material.
            if(room.theme==0)floor={static_cast<std::uint8_t>(169+shade),static_cast<std::uint8_t>(177+shade),static_cast<std::uint8_t>(158+shade),255};
            else if(room.theme==3)floor={static_cast<std::uint8_t>(192+shade),static_cast<std::uint8_t>(169+shade),static_cast<std::uint8_t>(139+shade),255};
            break;
        }
        const float l=static_cast<float>(x),t=static_cast<float>(y);
        if(v6 && map.generation.recipe.recipeVersion>=10)
            appendDungeonSlabMaterial(ink,map,x,y,floor);
        else {
            ink.push_back({{{l,t},{l+1,t},{l+1,t+1}},floor,.01f,true});
            ink.push_back({{{l,t},{l+1,t+1},{l,t+1}},floor,.01f,true});
        }
        if(v6 && cell->kind==MapTileKind::Room) {
            const unsigned wear=undergroundInkHash(map.generation.recipe.seed,x,y,51);
            if(wear%17==0) {
                undergroundInkPolygon(ink,{{l+.05f,t+.08f},{l+.23f,t+.06f},{l+.12f,t+.19f}},{115,119,98,85});
                undergroundInkLine(ink,{{l+.14f,t+.2f},{l+.3f,t+.14f}},{218,207,166,120},.025f);
            }
            if(wear%7==0) undergroundInkLine(ink,{{l+.36f,t+.64f},{l+.57f,t+.66f},{l+.7f,t+.60f}},{146,143,117,55},.025f);
        }
        // Large staggered slabs read at fit-to-window without a noisy grid.
        if(y%2==0) ink.push_back({{{l,t+.025f},{l+1,t+.025f}},{133,130,116,255},.022f,false});
        if((x+(y/2)%2)%2==0) ink.push_back({{{l+.025f,t},{l+.025f,t+1}},{133,130,116,255},.022f,false});
        if(cell->kind==MapTileKind::Room && cell->wayKind==MapWayKind::None && ((static_cast<unsigned>(x)*83492791u ^ static_cast<unsigned>(y)*2971215073u ^ h)%59u)==0)
            ink.push_back({{{l+.28f,t+.23f},{l+.43f,t+.39f},{l+.39f,t+.53f}},{121,120,108,255},.025f,false});
    }
    if(v6)appendDungeonIllustrationV6(ink,map,applyFog);
    return ink;
}

inline std::vector<AtlasInkPrimitive> buildDungeonPropInk(const MapModel &map, bool applyFog)
{
    std::vector<AtlasInkPrimitive> ink;
    if(!usesDungeonMasonryInk(map)) return ink;
    auto line=[&](std::vector<MapFeaturePoint> p,OverviewRenderColor c,float w=.04f){ink.push_back({std::move(p),c,w,false});};
    auto rect=[&](float l,float t,float r,float b,OverviewRenderColor c){
        ink.push_back({{{l,t},{r,t},{r,b}},c,.01f,true});ink.push_back({{{l,t},{r,b},{l,b}},c,.01f,true});};
    const OverviewRenderColor edge{55,48,39,255},wood{128,87,49,255},light{193,155,95,255},stone{177,174,151,255};
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x)
    {
        const auto *c=mapCellAt(map,x,y);if(!c || (applyFog && c->fogged))continue;
        float cx=x+.5f,cy=y+.5f;
        unsigned h=static_cast<unsigned>(x)*73856093u ^ static_cast<unsigned>(y)*19349663u ^ map.generation.recipe.seed;
        if(usesUndergroundIllustrationV6(map) && c->kind==MapTileKind::Cover) {
            unsigned form=h%5u;
            for(const auto &room:map.generation.graph.features) {
                if(room.materialId!="fantasy_neutral.dungeon.room"||room.points.empty())continue;
                float l=room.points.front().x,r=l,t=room.points.front().y,b=t;
                for(auto p:room.points){l=std::min(l,p.x);r=std::max(r,p.x);t=std::min(t,p.y);b=std::max(b,p.y);}
                if(cx>=l&&cx<=r&&cy>=t&&cy<=b) {form=undergroundInkHash(map.generation.recipe.seed,static_cast<int>((l+r)*.5f),static_cast<int>((t+b)*.5f),101)%5u;break;}
            }
            rect(cx-.43f+.055f,cy-.40f+.075f,cx+.43f+.055f,cy+.40f+.075f,{49,48,40,140});
            if(form==4) {
                for(int i=0;i<2;++i) {
                    const float xx=cx+(i? .19f:-.19f),yy=cy+(i?.12f:-.12f);
                    auto oval=undergroundInkOval(xx,yy,.24f,.26f,h+i,12);
                    undergroundInkPolygon(ink,oval,{143,99,55,255});undergroundInkClosedLine(ink,oval,edge,.055f);
                    line({{xx-.15f,yy-.17f},{xx-.15f,yy+.15f}},light,.04f);line({{xx+.10f,yy-.17f},{xx+.10f,yy+.17f}},light,.035f);
                    line({{xx-.20f,yy-.11f},{xx+.20f,yy-.11f}},{66,72,65,255},.045f);line({{xx-.20f,yy+.12f},{xx+.20f,yy+.12f}},{66,72,65,255},.045f);
                }
            } else {
                const float rx=form==2?.33f:.41f,ry=form==2?.43f:.35f;
                rect(cx-rx-.04f,cy-ry-.04f,cx+rx+.04f,cy+ry+.04f,edge);
                rect(cx-rx,cy-ry,cx+rx,cy+ry,form==1?OverviewRenderColor{170,162,130,255}:OverviewRenderColor{153,110,62,255});
                line({{cx-rx+.035f,cy+ry-.025f},{cx-rx+.035f,cy-ry+.025f},{cx+rx-.025f,cy-ry+.025f}},light,.05f);
                if(form==0) {
                    line({{cx-rx+.08f,cy-ry+.07f},{cx+rx-.08f,cy+ry-.07f}},light,.09f);line({{cx+rx-.08f,cy-ry+.07f},{cx-rx+.08f,cy+ry-.07f}},light,.09f);
                    for(int i=0;i<3;++i)line({{cx-.22f+i*.22f,cy-.26f},{cx-.22f+i*.22f,cy+.26f}},{99,73,45,180},.025f);
                } else if(form==1) {
                    rect(cx-.25f,cy-.21f,cx+.25f,cy+.21f,{116,128,118,255});
                    undergroundInkClosedLine(ink,{{cx,cy-.17f},{cx+.19f,cy},{cx,cy+.17f},{cx-.19f,cy}},{210,193,143,255},.045f);
                    rect(cx-.05f,cy-.08f,cx+.05f,cy+.08f,{165,85,77,255});
                    // A ceremonial runner and paired candles distinguish the
                    // altar from the ordinary storage/work tables nearby.
                    rect(cx-.075f,cy-.26f,cx+.075f,cy+.26f,{156,57,72,255});
                    line({{cx,cy-.19f},{cx+.045f,cy},{cx,cy+.17f},{cx-.045f,cy},{cx,cy-.19f}},{218,186,114,255},.028f);
                    for(float side:{-.24f,.24f}) {
                        rect(cx+side-.035f,cy-.09f,cx+side+.035f,cy+.05f,{234,221,172,255});
                        ink.push_back({{{cx+side-.045f,cy-.10f},{cx+side,cy-.18f},{cx+side+.045f,cy-.10f}},{236,150,61,255},0,true});
                    }
                } else if(form==2) {
                    for(int shelf=0;shelf<3;++shelf) {
                        float yy=cy-.34f+shelf*.27f;line({{cx-.29f,yy},{cx+.29f,yy}},light,.045f);
                        for(int book=0;book<4;++book) {float xx=cx-.25f+book*.14f;rect(xx,yy+.025f,xx+.095f,yy+.19f,book%3==0?OverviewRenderColor{91,131,128,255}:book%3==1?OverviewRenderColor{164,83,66,255}:OverviewRenderColor{180,157,96,255});line({{xx+.02f,yy+.05f},{xx+.07f,yy+.05f}},{224,205,154,230},.018f);}
                    }
                } else {
                    for(int i=0;i<3;++i)line({{cx-.28f+i*.28f,cy-.28f},{cx-.28f+i*.28f,cy+.28f}},{106,79,47,160},.025f);
                    rect(cx-.19f,cy-.15f,cx+.09f,cy+.13f,{201,189,148,255});
                    line({{cx-.13f,cy-.07f},{cx+.03f,cy-.07f}},edge,.02f);line({{cx-.13f,cy+.015f},{cx+.015f,cy+.015f}},edge,.018f);
                    undergroundInkPolygon(ink,undergroundInkOval(cx+.25f,cy+.16f,.075f,.075f,h,8),{64,81,74,255});
                }
            }
            continue;
        }
        if(usesUndergroundIllustrationV6(map) && c->kind==MapTileKind::Rubble) {
            for(int i=0;i<4;++i) undergroundInkStone(ink,cx+(i%2? .18f:-.19f),cy+(i/2?.18f:-.14f),.17f,.13f,h+i*37);
            continue;
        }
        if(c->kind==MapTileKind::Cover)
        {
            const unsigned form=h%3u;
            const float rx=form==0?.39f:.30f,ry=form==0?.32f:.43f;
            rect(cx-rx-.05f,cy-ry-.05f,cx+rx+.05f,cy+ry+.05f,edge);
            rect(cx-rx,cy-ry,cx+rx,cy+ry,form==1?stone:wood);
            if(form==0){line({{cx-rx,cy-ry},{cx+rx,cy+ry}},light,.06f);line({{cx+rx,cy-ry},{cx-rx,cy+ry}},light,.06f);}
            else if(form==1){rect(cx-.17f,cy-.28f,cx+.17f,cy+.28f,{128,128,114,255});line({{cx,cy-.23f},{cx,cy+.23f}},stone,.08f);}
            else {for(int shelf=0;shelf<3;++shelf){float yy=cy-.30f+shelf*.29f;line({{cx-.25f,yy},{cx+.25f,yy}},light,.035f);for(int book=0;book<4;++book){float xx=cx-.20f+book*.12f;rect(xx,yy+.03f,xx+.075f,yy+.21f,book%2?OverviewRenderColor{92,115,113,255}:OverviewRenderColor{148,81,68,255});}}}
        }
        else if(c->kind==MapTileKind::Rubble)
        {
            for(int k=0;k<3;++k){float xx=cx+(k-1)*.23f,yy=cy+(k%2?-.12f:.12f);rect(xx-.14f,yy-.10f,xx+.14f,yy+.11f,edge);rect(xx-.11f,yy-.08f,xx+.10f,yy+.06f,stone);}
        }
        else if(usesUndergroundIllustrationV6(map) && (c->kind==MapTileKind::Hazard || c->kind==MapTileKind::Pit)) {
            const bool pit=c->kind==MapTileKind::Pit;
            if(pit) {
                auto outer=undergroundInkOval(cx,cy,.44f,.40f,h,14);
                undergroundInkPolygon(ink,outer,{115,118,98,255});undergroundInkClosedLine(ink,outer,edge,.04f);
                auto inner=undergroundInkOval(cx+.015f,cy+.02f,.33f,.29f,h,14);
                undergroundInkPolygon(ink,inner,{37,44,41,255});
                undergroundInkLine(ink,{{cx-.3f,cy-.19f},{cx-.1f,cy-.29f},{cx+.14f,cy-.25f},{cx+.31f,cy-.14f}},{197,184,142,220},.045f);
                for(int i=0;i<5;++i){float a=i*1.2566f;line({{cx+std::cos(a)*.34f,cy+std::sin(a)*.3f},{cx+std::cos(a)*.42f,cy+std::sin(a)*.38f}},{70,77,63,220},.025f);}
            } else {
                undergroundInkPolygon(ink,undergroundInkOval(cx,cy,.43f,.43f,h,20),{128,97,145,35});
                undergroundInkClosedLine(ink,undergroundInkOval(cx,cy,.4f,.4f,h,20),{117,85,136,230},.045f);
                std::vector<MapFeaturePoint> star;
                for(int i=0;i<=5;++i){float a=-1.5708f+i*2.51327f;star.push_back({cx+std::cos(a)*.29f,cy+std::sin(a)*.29f});}
                line(star,{145,102,161,230},.037f);
                for(int i=0;i<5;++i){float a=-1.5708f+i*1.256637f;line({{cx+std::cos(a)*.36f,cy+std::sin(a)*.36f},{cx+std::cos(a)*.45f,cy+std::sin(a)*.45f}},{107,89,123,255},.035f);}
            }
        }
        else if(c->kind==MapTileKind::Hazard || c->kind==MapTileKind::Pit)
        {
            const bool pit=c->kind==MapTileKind::Pit;std::vector<MapFeaturePoint> ring;
            for(int k=0;k<=8;++k){float a=k*6.2831853f/8.f;ring.push_back({cx+std::cos(a)*.40f,cy+std::sin(a)*.35f});}
            if(pit){for(int k=0;k<8;++k)ink.push_back({{{cx,cy},ring[k],ring[k+1]},{39,42,39,255},.01f,true});line(ring,edge,.09f);}
            else {line(ring,{107,76,133,255},.055f);line({{cx,cy-.25f},{cx+.24f,cy+.17f},{cx-.24f,cy+.17f},{cx,cy-.25f}},{139,93,166,255},.045f);line({{cx-.28f,cy},{cx+.28f,cy}},{107,76,133,255},.03f);}
        }
    }
    return ink;
}

inline bool usesDungeonArchitectureInk(const MapModel &map)
{
    return map.scope == MapScope::Dungeon && map.creationIntent == MapCreationIntent::Dungeon &&
           map.generation.recipe.generatorVersion >= 10;
}

// Door and stair geometry comes from the semantic cell, never a random prop
// family. All coordinates are shared by editor, player plan, and PNG export.
inline std::vector<AtlasInkPrimitive> buildDungeonArchitectureInk(const MapModel &map, bool applyFog)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesDungeonArchitectureInk(map)) return ink;
    const OverviewRenderColor dark{42,35,29,255}, brass{198,166,101,255}, wood{124,83,48,255}, stone{197,183,145,255};
    auto stroke = [&](std::vector<MapFeaturePoint> points, OverviewRenderColor color, float width) {
        ink.push_back({std::move(points),color,width,false});
    };
    for (int y=0;y<map.height;++y) for(int x=0;x<map.width;++x)
    {
        const auto *cell=mapCellAt(map,x,y);
        if (!cell || (applyFog && cell->fogged)) continue;
        const float cx=x+.5f,cy=y+.5f;
        if (cell->kind == MapTileKind::Door)
        {
            const auto wall=[&](int dx,int dy) { const auto *n=mapCellAt(map,x+dx,y+dy);return !n || n->kind==MapTileKind::Wall || n->kind==MapTileKind::Empty; };
            const auto route=[&](int dx,int dy) { const auto *n=mapCellAt(map,x+dx,y+dy);return n && n->wayKind==MapWayKind::DungeonCorridor; };
            const int horizontal=route(-1,0)+route(1,0), vertical=route(0,-1)+route(0,1);
            const bool eastWest = horizontal == vertical ? wall(0,-1)+wall(0,1) >= wall(-1,0)+wall(1,0) : horizontal > vertical;
            const auto point=[&](float across,float depth) { return eastWest ? MapFeaturePoint{cx+depth,cy+across}:MapFeaturePoint{cx+across,cy+depth}; };
            const std::vector<MapFeaturePoint> border{point(-.43f,-.12f),point(.43f,-.12f),point(.43f,.12f),point(-.43f,.12f),point(-.43f,-.12f)};
            ink.push_back({{border[0],border[1],border[2]},wood,.02f,true});
            ink.push_back({{border[0],border[2],border[3]},wood,.02f,true});
            stroke(border,dark,.07f);
            stroke({point(-.41f,-.05f),point(.41f,-.05f)},brass,.032f);
            stroke({point(-.28f,-.11f),point(-.28f,.11f)},brass,.055f);
            stroke({point(.28f,-.11f),point(.28f,.11f)},brass,.055f);
        }
        else if (cell->kind == MapTileKind::Stairs)
        {
            stroke({{cx-.40f,cy-.42f},{cx-.40f,cy+.42f},{cx+.40f,cy+.42f},{cx+.40f,cy-.42f}},dark,.095f);
            for(int step=0;step<6;++step)
            {
                const float yy=cy-.35f+step*.14f;
                const float half=.20f+step*.033f;
                stroke({{cx-half,yy+.02f},{cx+half,yy+.02f}},dark,.105f);
                stroke({{cx-half,yy},{cx+half,yy}},stone,.06f);
            }
        }
    }
    return ink;
}
}
