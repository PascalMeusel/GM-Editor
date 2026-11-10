#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapCaveInk.hpp"
namespace gm::map_render {
inline bool usesSettlementSiteInk(const MapModel &map) {
    return map.atlasLevel == MapLevel::Site && map.mapKind == MapKind::Settlement && map.generation.recipe.generatorVersion >= 10;
}
inline bool usesSettlementIllustrationV6(const MapModel &map) {
    return map.atlasLevel==MapLevel::Site && map.mapKind==MapKind::Settlement &&
           map.generation.recipe.generatorVersion>=12 && map.generation.recipe.styleVersion>=6;
}
inline void settlementInkPolygon(std::vector<AtlasInkPrimitive>& out,const std::vector<MapFeaturePoint>& points,OverviewRenderColor fill) {
    if(points.size()<3)return;
    for(std::size_t i=1;i+1<points.size();++i)out.push_back({{points[0],points[i],points[i+1]},fill,.01f,true});
}
inline std::vector<AtlasInkPrimitive> buildSettlementRoofInk(const MapModel &map,bool applyFog=true) {
    std::vector<AtlasInkPrimitive> ink;if(!usesSettlementIllustrationV6(map))return ink;
    std::vector<unsigned char> used(map.cells.size(),0);
    auto available=[&](int x,int y){const auto*c=mapCellAt(map,x,y);return c && c->kind==MapTileKind::Building && (!applyFog||!c->fogged) && !used[std::size_t(y*map.width+x)];};
    auto stroke=[&](std::vector<MapFeaturePoint> p,OverviewRenderColor c,float w=.05f){ink.push_back({std::move(p),c,w,false});};
    const OverviewRenderColor outline{67,60,48,255},wall{231,213,168,255},wallShade{168,147,111,255};
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x){
        if(!available(x,y))continue;
        int w=1,h=1;while(x+w<map.width && available(x+w,y))++w;
        while(y+h<map.height){bool row=true;for(int dx=0;dx<w;++dx)row&=available(x+dx,y+h);if(!row)break;++h;}
        for(int dy=0;dy<h;++dy)for(int dx=0;dx<w;++dx)used[std::size_t((y+dy)*map.width+x+dx)]=1;
        const unsigned hash=static_cast<unsigned>(x)*313u+static_cast<unsigned>(y)*971u+map.generation.recipe.seed;
        std::string role;for(const auto&f:map.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::BuildingGroup && !f.points.empty() && int(f.points.front().x)>=x && int(f.points.front().x)<x+w && int(f.points.front().y)>=y && int(f.points.front().y)<y+h){role=f.materialId;break;}
        const bool temple=role.find("temple")!=std::string::npos,guild=role.find("guildhall")!=std::string::npos,inn=role.find("inn")!=std::string::npos;
        const unsigned palette=temple?3:guild?2:hash%6u;
        // Warm fired tiles, honey thatch and blue slate read as different
        // working buildings even at the normal city overview scale.
        const OverviewRenderColor light[]={ {203,112,70,255},{203,162,83,255},{179,76,68,255},{98,143,162,255},{174,133,76,255},{196,130,93,255} };
        const OverviewRenderColor dark[]={ {139,65,43,255},{143,103,49,255},{119,45,48,255},{57,91,115,255},{113,79,43,255},{137,77,51,255} };
        const float l=x+.09f,r=x+w-.09f,t=y+.08f,b=y+h-.08f,eave=std::min(.27f,h*.18f),base=b-eave;
        settlementInkPolygon(ink,{{l+.08f,t+.17f},{r+.10f,t+.17f},{r+.10f,b+.09f},{l+.08f,b+.09f}},{79,76,60,75});
        settlementInkPolygon(ink,{{l,base},{r,base},{r,b},{l,b}},wall);
        settlementInkPolygon(ink,{{r-.14f,t},{r,t},{r,b},{r-.14f,b}},wallShade);
        const bool horizontal=w>=h;
        if(horizontal){
            const float mid=(t+base)*.5f,inset=std::min((r-l)*.19f,(base-t)*.28f);
            const MapFeaturePoint a{l+inset,mid},z{r-inset,mid};
            settlementInkPolygon(ink,{{l,t},{r,t},z,a},light[palette]);
            settlementInkPolygon(ink,{{l,base},a,z,{r,base}},dark[palette]);
            settlementInkPolygon(ink,{{l,t},a,{l,base}},light[palette]);
            settlementInkPolygon(ink,{{r,t},{r,base},z},dark[palette]);
            stroke({a,z},{224,191,137,255},.045f);
            for(float yy=t+.24f;yy<base-.10f;yy+=.24f)stroke({{l+.17f,yy},{r-.17f,yy}},{76,65,50,90},.018f);
        }else{
            const float mid=(l+r)*.5f,inset=std::min((base-t)*.19f,(r-l)*.28f);
            const MapFeaturePoint a{mid,t+inset},z{mid,base-inset};
            settlementInkPolygon(ink,{{l,t},a,z,{l,base}},light[palette]);
            settlementInkPolygon(ink,{{r,t},{r,base},z,a},dark[palette]);
            settlementInkPolygon(ink,{{l,t},{r,t},a},light[palette]);
            settlementInkPolygon(ink,{{l,base},z,{r,base}},dark[palette]);
            stroke({a,z},{224,191,137,255},.045f);
            for(float yy=t+.25f;yy<base-.08f;yy+=.25f)stroke({{l+.10f,yy},{r-.10f,yy}},{76,65,50,90},.018f);
        }
        stroke({{l,t},{r,t},{r,b},{l,b},{l,t}},outline,.052f);
        stroke({{l,base},{r,base}},outline,.038f);
        for(float xx=l+.25f;xx<r-.15f;xx+=.52f)stroke({{xx,base+.02f},{xx,b-.02f}},outline,.04f);
        if(w>=2 && h>=2 && hash%3u!=0){
            const float px=r-.40f,py=t+.26f;
            settlementInkPolygon(ink,{{px-.10f,py-.18f},{px+.10f,py-.18f},{px+.10f,py+.20f},{px-.10f,py+.20f}},{221,203,161,255});
            stroke({{px-.10f,py-.18f},{px+.10f,py-.18f},{px+.10f,py+.20f},{px-.10f,py+.20f},{px-.10f,py-.18f}},outline,.035f);
            stroke({{px-.07f,py-.15f},{px+.07f,py-.15f}},{82,66,50,255},.075f);
        }
        if(!temple && !guild && w>=2 && h>=2 && hash%4u!=0) {
            // Dormer roofs add a little inhabited architecture while remaining
            // wholly within the existing blocked building footprint.
            const float cx=(l+r)*.5f,cy=t+(base-t)*.67f,dw=std::min(.31f,(r-l)*.19f),dh=std::min(.30f,(base-t)*.24f);
            settlementInkPolygon(ink,{{cx-dw,cy+dh},{cx,cy-dh},{cx+dw,cy+dh}},light[palette]);
            settlementInkPolygon(ink,{{cx,cy-dh},{cx+dw,cy+dh},{cx,cy+dh}},dark[palette]);
            stroke({{cx-dw,cy+dh},{cx,cy-dh},{cx+dw,cy+dh}},outline,.035f);
            stroke({{cx-dw*.66f,cy+dh},{cx+dw*.66f,cy+dh}},wall,.09f);
            stroke({{cx,cy+dh*.55f},{cx,cy+dh*1.06f}},{63,83,83,255},.08f);
        }
        if(temple||guild){
            const float cx=(l+r)*.5f,cy=(t+base)*.5f,rr=std::min(.43f,std::min(w,h)*.23f);
            settlementInkPolygon(ink,{{cx,cy-rr-.24f},{cx-rr,cy},{cx,cy+rr},{cx+rr,cy}},temple?OverviewRenderColor{78,100,128,255}:OverviewRenderColor{126,58,67,255});
            stroke({{cx,cy-rr-.24f},{cx-rr,cy},{cx,cy+rr},{cx+rr,cy},{cx,cy-rr-.24f}},outline,.047f);
            stroke({{cx,cy-rr-.24f},{cx,cy-rr-.54f}},{103,81,47,255},.045f);
            settlementInkPolygon(ink,{{cx,cy-rr-.54f},{cx+.29f,cy-rr-.45f},{cx,cy-rr-.34f}},{203,163,78,255});
        }
        if(inn){stroke({{r-.20f,base},{r-.20f,b+.17f}},{84,65,44,255},.04f);settlementInkPolygon(ink,{{r-.30f,b+.03f},{r-.04f,b+.03f},{r-.04f,b+.18f},{r-.30f,b+.18f}},{171,128,64,255});}
        if(!temple && w>=2 && h>=2 && (inn || guild || hash%3u!=2)) {
            // Shop-front fabric, a recessed doorway and shutters give the
            // roof plan a lived-in frontage. Keep everything within the
            // blocked building cells: decoration must not invent cover in a road.
            const float cx=(l+r)*.5f,aw=std::min(.68f,(r-l)*.32f);
            const float ay=std::max(t+.25f,base-.28f);
            const OverviewRenderColor cloth=hash%2u?OverviewRenderColor{49,133,132,255}:OverviewRenderColor{191,70,55,255};
            settlementInkPolygon(ink,{{cx-aw,ay},{cx+aw,ay},{cx+aw,base+.025f},{cx-aw,base+.025f}},cloth);
            for(int stripe=0;stripe<5;++stripe)if(stripe%2==0) {
                const float xx=cx-aw+(2*aw/5)*stripe,rr=xx+2*aw/5;
                settlementInkPolygon(ink,{{xx,ay},{rr,ay},{rr,base+.025f},{xx,base+.025f}},{239,215,159,255});
            }
            stroke({{cx-aw,ay},{cx+aw,ay},{cx+aw,base+.025f},{cx-aw,base+.025f},{cx-aw,ay}},outline,.035f);
            stroke({{cx,b-.025f},{cx,base+.045f}},{64,57,43,255},.12f);
            for(float side:{-.77f,.77f}) {
                const float xx=cx+side*aw;
                stroke({{xx,base+.05f},{xx,b-.03f}},{48,106,107,255},.085f);
            }
            // Skylight for a workshop, or a copper vent: visual variation is
            // deterministic and reuses the roof, never adding collision props.
            const float px=l+.36f,py=t+.34f;
            if(hash%3u==0) {
                settlementInkPolygon(ink,{{px-.14f,py-.15f},{px+.14f,py-.15f},{px+.14f,py+.14f},{px-.14f,py+.14f}},{65,118,128,255});
                stroke({{px-.14f,py},{px+.14f,py}},{227,199,142,255},.035f);
                stroke({{px,py-.15f},{px,py+.14f}},{227,199,142,255},.035f);
            } else if(hash%3u==1) {
                settlementInkPolygon(ink,{{px-.12f,py+.12f},{px,py-.21f},{px+.12f,py+.12f}},{84,130,117,255});
                stroke({{px-.12f,py+.12f},{px,py-.21f},{px+.12f,py+.12f}},outline,.035f);
            }
        }
    }
    return ink;
}
inline std::vector<AtlasInkPrimitive> buildSettlementWallInk(const MapModel& map,bool applyFog=true) {
    std::vector<AtlasInkPrimitive> ink;if(!usesSettlementIllustrationV6(map))return ink;
    const auto contours=naturalCaveContours(map,applyFog,MapTileKind::Wall);
    for(const auto&p:contours)if(naturalCaveContourArea(p)>0)fillNaturalCavePolygon(ink,p,{195,184,149,255});
    for(const auto&p:contours)if(naturalCaveContourArea(p)<0)fillNaturalCavePolygon(ink,p,{218,218,178,255});
    for(auto p:contours){if(p.size()<3)continue;p.push_back(p.front());
        ink.push_back({p,{112,104,77,255},.13f,false});
        // The parapet follows the resolved wall silhouette, including every
        // gate and painted gap. No original ellipse is regenerated here.
        float nextMark=.28f;
        for(std::size_t i=1;i<p.size();++i){auto a=p[i-1],b=p[i];const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<.08f)continue;
            const float nx=-dy/len,ny=dx/len;
            for(;nextMark<len;nextMark+=.70f){const float x=a.x+dx*nextMark/len,y=a.y+dy*nextMark/len;
                const auto*c=mapCellAt(map,int(x+nx*.18f),int(y+ny*.18f));
                if(!c||c->kind!=MapTileKind::Wall||(applyFog&&c->fogged))continue;
                ink.push_back({{{x+nx*.08f,y+ny*.08f},{x+nx*.29f,y+ny*.29f}},{215,203,163,255},.14f,false});
            }
            nextMark-=len;
        }
    }
    std::vector<MapFeaturePoint> caps;
    for(int y=1;y+1<map.height;++y)for(int x=1;x+1<map.width;++x) {
        const auto*c=mapCellAt(map,x,y);if(!c||c->kind!=MapTileKind::Wall||(applyFog&&c->fogged))continue;
        bool entrance=false;for(int direction=0;direction<4;++direction) {
            const int dx[]={-1,1,0,0},dy[]={0,0,-1,1};
            const auto*n=mapCellAt(map,x+dx[direction],y+dy[direction]);entrance|=n && n->kind!=MapTileKind::Wall && n->wayKind!=MapWayKind::None && (!applyFog||!n->fogged);
        }
        if(!entrance)continue;const MapFeaturePoint q{x+.5f,y+.5f};
        if(std::any_of(caps.begin(),caps.end(),[&](auto a){return std::hypot(a.x-q.x,a.y-q.y)<2.1f;}))continue;
        caps.push_back(q);std::vector<MapFeaturePoint> octagon;
        for(int k=0;k<8;++k){float a=k*.78539816f;octagon.push_back({q.x+std::cos(a)*.45f,q.y+std::sin(a)*.45f});}
        settlementInkPolygon(ink,octagon,{139,153,142,255});octagon.push_back(octagon.front());ink.push_back({octagon,{85,96,84,255},.06f,false});
        settlementInkPolygon(ink,{{q.x-.25f,q.y-.20f},{q.x+.25f,q.y-.20f},{q.x+.25f,q.y+.20f},{q.x-.25f,q.y+.20f}},{204,196,157,255});
    }
    return ink;
}
inline std::vector<AtlasInkPrimitive> buildSettlementGroundInk(const MapModel &map,bool applyFog=true) {
    std::vector<AtlasInkPrimitive> ink;if(!usesSettlementIllustrationV6(map))return ink;
    const OverviewRenderColor stone{195,184,149,255},edge{101,94,72,255},light{233,221,180,255};
    auto wall=[&](int x,int y){const auto*c=mapCellAt(map,x,y);return c&&c->kind==MapTileKind::Wall&&(!applyFog||!c->fogged);};
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x){
        const auto*c=mapCellAt(map,x,y);if(!c||(applyFog&&c->fogged))continue;
        if((c->kind==MapTileKind::Dirt||c->kind==MapTileKind::Empty) && c->wayKind==MapWayKind::None) {
            const auto*west=mapCellAt(map,x-1,y);const auto*north=mapCellAt(map,x,y-1);
            const unsigned hash=unsigned(x)*733u+unsigned(y)*97u+map.generation.recipe.seed;
            if(hash%3u==0 && ((west&&west->kind==MapTileKind::Building)||(north&&north->kind==MapTileKind::Building))) {
                settlementInkPolygon(ink,{{x+.14f,y+.15f},{x+.88f,y+.15f},{x+.88f,y+.87f},{x+.14f,y+.87f}},{154,160,104,155});
                for(int row=0;row<3;++row)ink.push_back({{{x+.23f,y+.28f+row*.20f},{x+.80f,y+.28f+row*.20f}},{101,122,71,155},.06f,false});
                ink.push_back({{{x+.14f,y+.15f},{x+.88f,y+.15f},{x+.88f,y+.87f}},{139,118,76,160},.035f,false});
            }
        }
    }
    auto walls=buildSettlementWallInk(map,applyFog);ink.insert(ink.end(),walls.begin(),walls.end());
    return ink;
}
inline bool settlementPoiAt(const MapModel &map,int x,int y) {
    if(!usesSettlementSiteInk(map))return false;
    return std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){return f.kind==MapGeneratedFeatureKind::Landmark && f.materialId.rfind("fantasy_neutral.settlement.poi.",0)==0 && !f.points.empty() && int(f.points.front().x)==x && int(f.points.front().y)==y;});
}
inline void settlementInkDisc(std::vector<AtlasInkPrimitive>& out,MapFeaturePoint center,float radius,OverviewRenderColor color) {
    for(int k=0;k<20;++k) {
        const float a=k*6.2831853f/20.f,b=(k+1)*6.2831853f/20.f;
        out.push_back({{center,{center.x+std::cos(a)*radius,center.y+std::sin(a)*radius},{center.x+std::cos(b)*radius,center.y+std::sin(b)*radius}},color,.03f,true});
    }
}
inline std::vector<AtlasInkPrimitive> buildSettlementCivicInk(const MapModel &map) {
    std::vector<AtlasInkPrimitive> ink;if(!usesSettlementSiteInk(map))return ink;
    if(map.generation.recipe.generatorVersion>=11 && !usesSettlementIllustrationV6(map)) {
        for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
            const auto *c=mapCellAt(map,x,y);if(!c||c->fogged||c->kind!=MapTileKind::Wall)continue;
            const float l=x+.03f,r=x+.97f,t=y+.03f,b=y+.97f;
            ink.push_back({{{l,t},{r,t},{r,b}},{124,128,107,255},.03f,true});
            ink.push_back({{{l,t},{r,b},{l,b}},{124,128,107,255},.03f,true});
            ink.push_back({{{l,t},{r,t}},{166,170,146,255},.10f,false});
            ink.push_back({{{l,b},{r,b}},{58,65,59,255},.14f,false});
            ink.push_back({{{l,y+.50f},{r,y+.50f}},{78,85,75,230},.05f,false});
            ink.push_back({{{x+.50f,t},{x+.50f,y+.50f}},{78,85,75,230},.045f,false});
        }
    }
    if(usesSettlementIllustrationV6(map))for(const auto&f:map.generation.graph.features) {
        if(f.kind!=MapGeneratedFeatureKind::Bridge || f.materialId!="fantasy_neutral.settlement.pier" || f.points.size()<2)continue;
        const auto a=f.points.front(),b=f.points.back();const float length=std::hypot(b.x-a.x,b.y-a.y);if(length<.1f)continue;
        const float ux=(b.x-a.x)/length,uy=(b.y-a.y)/length,nx=-uy,ny=ux;
        auto p=[&](float t,float side){return MapFeaturePoint{a.x+ux*t+nx*side,a.y+uy*t+ny*side};};
        bool visible=true;for(int k=0;k<=int(length*4);++k){auto q=p(k*.25f,0);auto*c=mapCellAt(map,int(q.x),int(q.y));if(!c||c->fogged){visible=false;break;}}if(!visible)continue;
        settlementInkPolygon(ink,{p(0,-.39f),p(length,-.39f),p(length,.39f),p(0,.39f)},{163,127,80,255});
        for(float t=.10f;t<length;t+=.21f)ink.push_back({{p(t,-.37f),p(t,.37f)},{91,74,49,220},.033f,false});
        ink.push_back({{p(0,-.39f),p(length,-.39f),p(length,.39f),p(0,.39f)},{83,69,48,255},.062f,false});
        for(float t=.25f;t<length;t+=1.10f)for(float side:{-.38f,.38f}) {
            auto q=p(t,side);settlementInkDisc(ink,q,.10f,{81,69,47,255});settlementInkDisc(ink,{q.x-.025f,q.y-.035f},.065f,{208,177,119,255});
        }
    }
    for(const auto &feature:map.generation.graph.features) {
        if(feature.kind!=MapGeneratedFeatureKind::Settlement || feature.points.empty())continue;
        const auto p=feature.points.front();const auto *cell=mapCellAt(map,int(p.x),int(p.y));
        if(!cell || cell->kind!=MapTileKind::Settlement)continue;
        const float radius=map.generation.recipe.generatorVersion>=11 ? (map.width<32?1.05f:map.width<40?1.4f:1.9f) : (map.width<40?1.5f:2.05f);
        if(usesSettlementIllustrationV6(map)) {
            bool visible=true;
            for(int y=int(std::floor(p.y-radius));y<=int(std::floor(p.y+radius)) && visible;++y)
                for(int x=int(std::floor(p.x-radius));x<=int(std::floor(p.x+radius));++x) {
                    const auto *part=mapCellAt(map,x,y);if(!part || part->fogged){visible=false;break;}
                }
            if(!visible)continue;
        }
        std::vector<MapFeaturePoint> square;
        if(usesSettlementIllustrationV6(map)) {
            const float r=radius*.93f,c=radius*.26f;
            square={{p.x-r+c,p.y-r},{p.x+r-c,p.y-r},{p.x+r,p.y-r+c},{p.x+r,p.y+r-c},{p.x+r-c,p.y+r},{p.x-r+c,p.y+r},{p.x-r,p.y+r-c},{p.x-r,p.y-r+c}};
        } else for(int k=0;k<8;++k) {float a=k*6.2831853f/8.f+.392699f;square.push_back({p.x+std::cos(a)*radius,p.y+std::sin(a)*radius});}
        const bool illustratedSquare=usesSettlementIllustrationV6(map);
        const OverviewRenderColor paving=illustratedSquare?OverviewRenderColor{199,184,141,255}:OverviewRenderColor{156,147,117,255};
        for(int k=0;k<8;++k)ink.push_back({{p,square[k],square[(k+1)%8]},paving,.03f,true});
        square.push_back(square.front());ink.push_back({square,illustratedSquare?OverviewRenderColor{141,126,91,255}:OverviewRenderColor{64,68,60,255},illustratedSquare?.05f:.09f,false});
        if(illustratedSquare) {
            // Staggered paving courses belong to the square footprint, rather
            // than a dark UI-like badge surrounding every town centre.
            int row=0;
            for(float y=-radius*.70f;y<radius*.75f;y+=.31f,++row)
                for(float x=-radius*.73f+(row%2)*.22f;x<radius*.70f;x+=.44f) {
                    ink.push_back({{{p.x+x,p.y+y},{p.x+x,p.y+y+.25f}},{147,136,105,130},.018f,false});
                }
        }
        for(float dy=-1.2f;dy<=1.2f;dy+=.4f) {
            if(std::abs(dy)>radius*.7f)continue;
            ink.push_back({{{p.x-radius*.72f,p.y+dy},{p.x+radius*.72f,p.y+dy}},{88,88,73,125},.022f,false});
        }
        if(usesSettlementIllustrationV6(map)) {
            const unsigned identity=map.generation.recipe.seed%4u;
            if(identity==0) {
                settlementInkDisc(ink,{p.x+.08f,p.y+.10f},.67f,{96,91,69,110});
                settlementInkDisc(ink,p,.60f,{94,90,69,255});settlementInkDisc(ink,p,.48f,{213,199,153,255});
                settlementInkDisc(ink,p,.32f,{69,130,137,255});
                ink.push_back({{{p.x-.20f,p.y-.09f},{p.x+.16f,p.y-.13f}},{177,212,198,255},.05f,false});
            } else if(identity==1) {
                settlementInkDisc(ink,p,.73f,{174,156,111,255});
                ink.push_back({{{p.x,p.y+.30f},{p.x,p.y-.38f}},{91,74,47,255},.13f,false});
                for(int k=0;k<7;++k){const float a=k*.897598f;settlementInkDisc(ink,{p.x+std::cos(a)*.30f,p.y-.16f+std::sin(a)*.25f},.36f,{91,130,89,255});}
                settlementInkDisc(ink,{p.x-.16f,p.y-.37f},.24f,{153,170,104,255});
            } else if(identity==2) {
                settlementInkPolygon(ink,{{p.x-.40f,p.y-.20f},{p.x+.40f,p.y-.20f},{p.x+.40f,p.y+.39f},{p.x-.40f,p.y+.39f}},{173,159,119,255});
                settlementInkPolygon(ink,{{p.x,p.y-.63f},{p.x-.19f,p.y+.18f},{p.x+.19f,p.y+.18f}},{114,124,89,255});
                settlementInkDisc(ink,{p.x,p.y-.63f},.12f,{156,151,101,255});
                ink.push_back({{{p.x+.15f,p.y-.22f},{p.x+.35f,p.y-.64f}},{116,103,64,255},.06f,false});
            } else {
                for(int stall=0;stall<3;++stall){float x=p.x-.75f+stall*.60f,y=p.y+(stall%2?.22f:-.24f);
                    const OverviewRenderColor cloth=stall==1?OverviewRenderColor{161,92,85,255}:OverviewRenderColor{171,145,82,255};
                    settlementInkPolygon(ink,{{x-.22f,y-.28f},{x+.22f,y-.28f},{x+.27f,y+.28f},{x-.27f,y+.28f}},cloth);
                    ink.push_back({{{x,y-.25f},{x,y+.24f}},{235,215,166,255},.08f,false});
                    ink.push_back({{{x-.27f,y+.28f},{x+.27f,y+.28f}},{96,77,50,255},.06f,false});
                }
            }
            continue;
        }
        settlementInkDisc(ink,{p.x+.08f,p.y+.11f},.82f,{44,53,49,190});
        settlementInkDisc(ink,p,.78f,{84,91,85,255});
        settlementInkDisc(ink,p,.66f,{195,181,135,255});
        settlementInkDisc(ink,p,.51f,{39,103,113,255});
        settlementInkDisc(ink,{p.x-.08f,p.y-.08f},.35f,{57,133,135,255});
        ink.push_back({{{p.x-.29f,p.y-.18f},{p.x+.12f,p.y-.22f}},{179,215,178,240},.05f,false});
        ink.push_back({{{p.x-.1f,p.y+.13f},{p.x+.26f,p.y+.09f}},{159,211,186,220},.045f,false});
        // Compass-star pedestal gives the square a restrained fantasy detail.
        for(int k=0;k<4;++k) {const float a=k*1.5707963f;ink.push_back({{p,{p.x+std::cos(a)*.31f,p.y+std::sin(a)*.31f},{p.x+std::cos(a+.8f)*.11f,p.y+std::sin(a+.8f)*.11f}},{211,194,132,255},.02f,true});}
    }
    for(const auto &f:map.generation.graph.features) {
        if(f.kind!=MapGeneratedFeatureKind::Landmark || f.materialId.rfind("fantasy_neutral.settlement.poi.",0)!=0 || f.points.empty())continue;
        const auto p=f.points.front();const auto *cell=mapCellAt(map,int(p.x),int(p.y));if(!cell || cell->kind!=MapTileKind::Landmark)continue;
        const float l=p.x-.48f,r=p.x+.48f,t=p.y-.48f,b=p.y+.48f;
        if(usesSettlementIllustrationV6(map)) {
            if(cell->fogged)continue;
            if(f.materialId.find("well")!=std::string::npos){
                settlementInkDisc(ink,{p.x+.04f,p.y+.07f},.41f,{103,95,70,115});
                settlementInkDisc(ink,p,.38f,{116,108,77,255});settlementInkDisc(ink,p,.28f,{226,208,164,255});settlementInkDisc(ink,p,.18f,{72,132,141,255});
                ink.push_back({{{p.x-.28f,p.y},{p.x+.28f,p.y}},{129,101,59,255},.065f,false});
            }else if(f.materialId.find("market")!=std::string::npos){
                settlementInkPolygon(ink,{{l+.04f,t+.07f},{r-.04f,t+.07f},{r,b-.07f},{l,b-.07f}},{159,91,87,255});
                for(int stripe=0;stripe<3;++stripe){float x=l+.16f+stripe*.26f;ink.push_back({{{x,t+.09f},{x,b-.08f}},{229,208,155,255},.10f,false});}
                ink.push_back({{{l,b-.07f},{r,b-.07f}},{95,73,47,255},.07f,false});
                for(float x:{l+.06f,r-.06f})ink.push_back({{{x,b-.10f},{x,b+.07f}},{101,76,45,255},.055f,false});
            }else {
                settlementInkDisc(ink,{p.x,p.y+.17f},.37f,{184,176,135,150});
                const bool shrine=f.materialId.find("shrine")!=std::string::npos;
                const float peak=shrine?.45f:.32f;
                settlementInkPolygon(ink,{{p.x-.20f,p.y+.29f},{p.x-.15f,p.y-peak},{p.x+.08f,p.y-peak-.07f},{p.x+.20f,p.y+.29f}},{152,159,135,255});
                settlementInkPolygon(ink,{{p.x+.02f,p.y-peak-.04f},{p.x+.08f,p.y-peak-.07f},{p.x+.20f,p.y+.29f},{p.x+.02f,p.y+.29f}},{99,115,103,255});
                ink.push_back({{{p.x-.20f,p.y+.29f},{p.x-.15f,p.y-peak},{p.x+.08f,p.y-peak-.07f},{p.x+.20f,p.y+.29f}},{89,98,81,255},.04f,false});
                if(shrine){settlementInkDisc(ink,{p.x-.01f,p.y-.12f},.115f,{216,187,107,255});settlementInkDisc(ink,{p.x+.025f,p.y-.15f},.095f,{139,152,128,255});}
            }
            continue;
        }
        ink.push_back({{{l,t},{r,t},{r,b}},{133,133,110,255},.02f,true});ink.push_back({{{l,t},{r,b},{l,b}},{133,133,110,255},.02f,true});
        ink.push_back({{{l,t},{r,t},{r,b},{l,b},{l,t}},{49,59,52,255},.055f,false});
        if(f.materialId.find("market")!=std::string::npos) {
            const MapFeaturePoint ridge{p.x,p.y-.08f};
            ink.push_back({{{l+.05f,t+.04f},{r-.05f,t+.04f},ridge},{153,66,70,255},.02f,true});
            ink.push_back({{{l+.05f,b-.05f},{r-.05f,b-.05f},ridge},{89,38,55,255},.02f,true});
            ink.push_back({{{l+.07f,t+.07f},{r-.07f,t+.07f}},{222,191,133,255},.04f,false});
            ink.push_back({{{l+.07f,b-.07f},{r-.07f,b-.07f}},{222,191,133,255},.04f,false});
        } else if(f.materialId.find("well")!=std::string::npos) {
            settlementInkDisc(ink,p,.35f,{59,70,67,255});settlementInkDisc(ink,p,.27f,{186,181,136,255});settlementInkDisc(ink,p,.19f,{38,91,98,255});
        } else {
            const MapFeaturePoint top{p.x,p.y-.37f},left{p.x-.25f,p.y+.22f},bottom{p.x,p.y+.36f},right{p.x+.25f,p.y+.22f};
            ink.push_back({{top,left,bottom},{115,129,132,255},.02f,true});ink.push_back({{top,bottom,right},{48,76,92,255},.02f,true});
            ink.push_back({{top,left,bottom,right,top},{35,47,55,255},.05f,false});
            ink.push_back({{{p.x,p.y-.20f},{p.x,p.y+.17f}},{196,191,113,255},.045f,false});
            ink.push_back({{{p.x-.1f,p.y-.03f},{p.x+.1f,p.y-.03f}},{196,191,113,255},.04f,false});
        }
    }
    return ink;
}
inline std::vector<AtlasInkPrimitive> buildSettlementWoodlandInk(const MapModel &map) {
    std::vector<AtlasInkPrimitive> ink;if(!usesSettlementSiteInk(map))return ink;
    for(int y=1;y+1<map.height;++y)for(int x=1;x+1<map.width;++x) {
        const auto *cell=mapCellAt(map,x,y);if(!cell || cell->kind!=MapTileKind::Forest)continue;
        unsigned hash=static_cast<unsigned>(x*73856093u)^static_cast<unsigned>(y*19349663u)^map.generation.recipe.seed;
        hash ^= hash >> 16; hash *= 0x7feb352du; hash ^= hash >> 15; hash *= 0x846ca68bu; hash ^= hash >> 16;
        if(hash%4u!=0u)continue;
        bool clear=true;
        for(int yy=y-1;yy<=y+1 && clear;++yy)for(int xx=x-1;xx<=x+1;++xx) {
            const auto *adjacentCell=mapCellAt(map,xx,yy);
            if(!adjacentCell || adjacentCell->wayKind!=MapWayKind::None || adjacentCell->kind==MapTileKind::Building || adjacentCell->kind==MapTileKind::Water || adjacentCell->kind==MapTileKind::Bridge) {clear=false;break;}
        }
        if(!clear)continue;
        const MapFeaturePoint p{x+.5f+float(hash%11u)*.04f-.2f,y+.5f+float((hash>>8)%11u)*.04f-.2f};
        const float radius=.52f+float((hash>>5)%8u)*.025f;
        settlementInkDisc(ink,{p.x+.10f,p.y+.13f},radius+.10f,{30,45,33,165});
        for(int k=0;k<6;++k) {
            float a=k*1.0471976f;
            MapFeaturePoint lobe{p.x+std::cos(a)*radius*.42f,p.y+std::sin(a)*radius*.42f};
            settlementInkDisc(ink,lobe,radius*.65f,{39,69,47,255});
            settlementInkDisc(ink,{lobe.x-.025f,lobe.y-.04f},radius*.57f,{static_cast<unsigned char>(70+hash%22u),static_cast<unsigned char>(103+hash%26u),static_cast<unsigned char>(59+hash%16u),255});
        }
        settlementInkDisc(ink,{p.x-.14f,p.y-.15f},radius*.32f,{139,158,82,175});
    }
    return ink;
}
}

