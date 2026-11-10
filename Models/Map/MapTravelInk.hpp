#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"

namespace gm::map_render {
inline bool travelPurposeAt(const MapModel &map,int x,int y) {
    if(map.creationIntent!=MapCreationIntent::TravelScene || map.generation.recipe.generatorVersion<11) return false;
    return std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){
        return f.materialId.rfind("fantasy_neutral.travel.",0)==0 && !f.points.empty() &&
            static_cast<int>(f.points.front().x)==x && static_cast<int>(f.points.front().y)==y;
    });
}
// Purpose art is resolved from visible semantic anchors in the shared plan.
// Historical scene recipes retain their original generic symbol rendering.
inline std::vector<AtlasInkPrimitive> buildTravelPurposeInk(const MapModel &map, bool applyFog)
{
    std::vector<AtlasInkPrimitive> ink;
    if (map.creationIntent != MapCreationIntent::TravelScene || map.generation.recipe.generatorVersion < 11) return ink;
    const OverviewRenderColor dark{49,45,35,255}, stone{168,172,151,255}, shade{95,111,99,255}, canvas{216,184,120,255}, wood{133,93,57,255}, flame{230,134,48,255};
    auto stroke=[&](std::vector<MapFeaturePoint> p, OverviewRenderColor c,float w=.07f){ink.push_back({std::move(p),c,w,false});};
    auto triangle=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,OverviewRenderColor color){ink.push_back({{a,b,c},color,.02f,true});};
    auto quad=[&](float x,float y,float w,float h,OverviewRenderColor c){triangle({x,y},{x+w,y},{x+w,y+h},c);triangle({x,y},{x+w,y+h},{x,y+h},c);};
    const auto wagon=[&](float x,float y,bool broken) {
        // Overhead cart matches the map view. Visible rails, cargo and shafts
        // give it a readable silhouette without an unrelated side-view icon.
        quad(x-.78f,y-.50f,1.55f,1.f,dark);
        quad(x-.67f,y-.40f,1.33f,.8f,wood);
        for(float plank:{-.24f,0.f,.24f})stroke({{x-.62f,y+plank},{x+.62f,y+plank}},canvas,.055f);
        for(float axle:{-.45f,.43f}) {
            stroke({{x+axle,y-.7f},{x+axle,y+.7f}},dark,.08f);
            for(float side:{-.68f,.53f})if(!(broken && axle>.0f && side>.0f)){
                quad(x+axle-.15f,y+side,.3f,.16f,dark);
                stroke({{x+axle-.12f,y+side+.05f},{x+axle+.12f,y+side+.05f}},wood,.06f);
            }
        }
        stroke({{x+.67f,y-.25f},{x+1.24f,y-.32f}},wood,.10f);
        stroke({{x+.67f,y+.25f},{x+(broken?.94f:1.24f),y+.32f}},wood,.10f);
        if(broken) {
            stroke({{x-.60f,y-.47f},{x+.31f,y+.38f}},dark,.11f);
            stroke({{x-.30f,y-.45f},{x+.54f,y+.35f}},canvas,.055f);
            std::vector<MapFeaturePoint> rim;
            for(int i=0;i<=16;++i){const float a=i*6.2831853f/16.f;rim.push_back({x+.92f+.25f*std::cos(a),y+.69f+.25f*std::sin(a)});}
            stroke(rim,dark,.08f);stroke({{x+.7f,y+.69f},{x+1.14f,y+.69f}},wood,.055f);
            // Torn wine-red canvas and a split cargo chest make the broken
            // wagon read as an interrupted journey, not a generic brown box.
            triangle({x-.61f,y-.36f},{x+.42f,y-.34f},{x-.29f,y+.17f},{171,67,53,255});
            triangle({x-.61f,y-.36f},{x-.29f,y+.17f},{x-.58f,y+.31f},{210,119,75,255});
            stroke({{x-.48f,y-.29f},{x-.22f,y-.10f}},{240,206,139,255},.055f);
            quad(x+.05f,y-.24f,.42f,.37f,canvas);
            stroke({{x+.10f,y-.20f},{x+.43f,y+.08f}},dark,.05f);
        }else {
            quad(x-.55f,y-.29f,.48f,.55f,canvas);
            stroke({{x-.55f,y-.29f},{x-.07f,y+.26f}},wood,.06f);
            quad(x+.10f,y-.23f,.40f,.46f,shade);
            stroke({{x+.30f,y-.23f},{x+.30f,y+.23f}},canvas,.05f);
            // Rolled trade cloth on the loaded wagon adds a recognisable
            // purpose without changing the existing semantic cargo footprint.
            quad(x-.55f,y-.29f,.15f,.55f,{176,64,57,255});
            quad(x-.37f,y-.29f,.15f,.55f,{54,127,137,255});
            stroke({{x-.58f,y-.02f},{x-.07f,y-.02f}},{239,213,158,255},.045f);
        }
    };
    for(const auto &f:map.generation.graph.features) {
        if(f.materialId.rfind("fantasy_neutral.travel.",0)!=0 || f.points.empty())continue;
        const float x=f.points.front().x,y=f.points.front().y;
        const auto *cell=mapCellAt(map,static_cast<int>(x),static_cast<int>(y));
        if(!cell || (applyFog && cell->fogged))continue;
        if(f.materialId=="fantasy_neutral.travel.rest") {
            triangle({x-.9f,y+.35f},{x-.15f,y-.65f},{x+.6f,y+.35f},canvas);
            triangle({x-.15f,y-.65f},{x+.6f,y+.35f},{x+.28f,y+.13f},wood);
            stroke({{x-.9f,y+.35f},{x-.15f,y-.65f},{x+.6f,y+.35f},{x-.9f,y+.35f}},dark);
            stroke({{x-.15f,y-.65f},{x-.15f,y+.31f}},dark,.045f);
            stroke({{x+.64f,y+.65f},{x+1.15f,y+.88f}},wood,.15f);
            stroke({{x+.66f,y+.9f},{x+1.13f,y+.65f}},dark,.12f);
            triangle({x+.65f,y+.73f},{x+.91f,y+.21f},{x+1.15f,y+.75f},flame);
            triangle({x+.84f,y+.72f},{x+.94f,y+.43f},{x+1.03f,y+.76f},canvas);
        } else if(f.materialId=="fantasy_neutral.travel.social") {
            wagon(x,y,false);
        } else if(f.materialId=="fantasy_neutral.travel.discovery") {
            triangle({x-.46f,y+.68f},{x-.23f,y-.92f},{x+.41f,y+.63f},stone);
            triangle({x-.23f,y-.92f},{x+.32f,y-.77f},{x+.41f,y+.63f},shade);
            stroke({{x-.46f,y+.68f},{x-.23f,y-.92f},{x+.32f,y-.77f},{x+.41f,y+.63f},{x-.46f,y+.68f}},dark);
            stroke({{x-.07f,y-.50f},{x+.12f,y-.25f},{x-.1f,y},{x+.14f,y+.23f}}, {140,214,186,255},.065f);
        } else if(f.materialId=="fantasy_neutral.travel.obstacle") {
            const auto p=[&](float a,float b){return cell->rotationQuarterTurns%2?MapFeaturePoint{x-b,y+a}:MapFeaturePoint{x+a,y+b};};
            stroke({p(-.9f,-.15f),p(.84f,.37f)},dark,.42f);
            stroke({p(-.9f,-.15f),p(.84f,.37f)},wood,.29f);
            stroke({p(-.30f,.02f),p(-.5f,-.63f)},wood,.15f);
            stroke({p(.24f,.19f),p(.46f,-.32f)},wood,.11f);
        } else if(f.materialId=="fantasy_neutral.travel.danger") {
            wagon(x,y,true);
        } else if(map.generation.recipe.generatorVersion>=12 && map.generation.recipe.styleVersion==6 &&
                  map.generation.recipe.styleId=="illustrated_atlas" && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v6") {
            if(f.materialId=="fantasy_neutral.travel.bridge" && f.points.size()>1) {
                const float end=f.points.back().x;
                quad(x,y-.46f,end-x,.92f,wood);
                stroke({{x,y-.46f},{end,y-.46f},{end,y+.46f},{x,y+.46f},{x,y-.46f}},dark,.065f);
                for(float xx=x+.22f;xx<end;xx+=.25f)stroke({{xx,y-.42f},{xx,y+.42f}},canvas,.04f);
                for(float side:{-.48f,.48f})stroke({{x-.1f,y+side},{end+.1f,y+side}},dark,.07f);
            } else if(f.materialId=="fantasy_neutral.travel.discovery_site") {
                const auto onStone=[&](MapFeaturePoint p){const auto *c=mapCellAt(map,int(p.x),int(p.y));return c && c->kind==MapTileKind::Stone && c->wayKind==MapWayKind::None && (!applyFog || !c->fogged);};
                for(float radius:{f.width*.82f,f.width})for(int i=0;i<72;++i) {
                    if(i%18==0 || i%18==1)continue;
                    const float a=i*6.2831853f/72.f,b=(i+1)*6.2831853f/72.f;
                    const MapFeaturePoint p{x+radius*std::cos(a),y+radius*std::sin(a)},q{x+radius*std::cos(b),y+radius*std::sin(b)};
                    if(onStone(p)&&onStone(q))stroke({p,q},{119,128,105,210},.055f);
                }
                for(int i=0;i<8;++i) {
                    const float a=i*6.2831853f/8.f;
                    const MapFeaturePoint p{x+f.width*.82f*std::cos(a),y+f.width*.82f*std::sin(a)},q{x+f.width*std::cos(a),y+f.width*std::sin(a)};
                    if(onStone(p)&&onStone(q))stroke({p,q},shade,.05f);
                }
            } else if(f.materialId=="fantasy_neutral.travel.shelter") {
                if(f.points.size()!=4 || std::any_of(f.points.begin(),f.points.end(),[&](auto p){const auto *c=mapCellAt(map,int(p.x),int(p.y));return !c || c->kind!=MapTileKind::Cover || (applyFog && c->fogged);}))continue;
                quad(x-.42f,y-.42f,1.84f,1.84f,dark);
                quad(x-.34f,y-.34f,1.68f,1.68f,canvas);
                for(float stripe:{-.32f,.24f,.80f})quad(x+stripe,y-.32f,.27f,1.64f,{151,77,62,255});
                stroke({{x+.50f,y-.30f},{x+.50f,y+1.30f}},dark,.065f);
                stroke({{x-.30f,y+.50f},{x+1.30f,y+.50f}},{244,217,157,255},.08f);
                for(float px:{-.39f,1.39f})for(float py:{-.39f,1.39f})quad(x+px-.055f,y+py-.055f,.11f,.11f,wood);
            } else if(f.materialId=="fantasy_neutral.travel.supply_crate") {
                const unsigned variation=(unsigned(std::max(0.f,x))*197u+unsigned(std::max(0.f,y))*397u+map.generation.recipe.seed)%3u;
                quad(x-.40f,y-.36f,.80f,.72f,dark);
                quad(x-.34f,y-.30f,.68f,.60f,wood);
                for(float plank:{-.18f,.0f,.18f})stroke({{x-.31f,y+plank},{x+.31f,y+plank}},canvas,.025f);
                if(variation==0) {
                    // An open produce crate: visible fruit in a timber tray.
                    quad(x-.27f,y-.23f,.54f,.46f,{78,75,45,255});
                    for(int i=0;i<6;++i) {
                        const float px=x-.17f+(i%3)*.17f,py=y-.12f+(i/3)*.23f;
                        triangle({px-.065f,py+.06f},{px,py-.07f},{px+.07f,py+.045f},i%2?OverviewRenderColor{204,103,57,255}:OverviewRenderColor{195,166,69,255});
                    }
                } else if(variation==1) {
                    // Folded dyed textiles secured by a pale carrying strap.
                    quad(x-.29f,y-.24f,.58f,.48f,{62,125,139,255});
                    quad(x-.29f,y-.06f,.58f,.14f,{187,77,63,255});
                    stroke({{x+.04f,y-.24f},{x+.04f,y+.24f}},{239,214,166,255},.055f);
                } else {
                    stroke({{x-.29f,y-.25f},{x+.29f,y+.25f}},canvas,.065f);
                    stroke({{x+.29f,y-.25f},{x-.29f,y+.25f}},canvas,.065f);
                    quad(x-.10f,y-.09f,.20f,.18f,{180,65,49,255});
                }
            } else if(f.materialId=="fantasy_neutral.travel.bedroll") {
                quad(x-.35f,y-.40f,.70f,.80f,{92,117,100,255});
                quad(x-.35f,y-.40f,.70f,.22f,canvas);
                stroke({{x-.35f,y-.4f},{x+.35f,y-.4f},{x+.35f,y+.4f},{x-.35f,y+.4f},{x-.35f,y-.4f}},dark,.055f);
                stroke({{x-.35f,y-.14f},{x+.35f,y-.14f}},dark,.035f);
            } else if(f.materialId=="fantasy_neutral.travel.stone_seat") {
                quad(x-.42f,y-.24f,.84f,.48f,shade);
                triangle({x-.42f,y-.24f},{x+.29f,y-.32f},{x+.42f,y+.02f},stone);
                stroke({{x-.42f,y-.24f},{x+.29f,y-.32f},{x+.42f,y+.02f},{x+.37f,y+.24f},{x-.39f,y+.24f},{x-.42f,y-.24f}},dark,.055f);
            }
        }
    }
    return ink;
}

inline std::vector<AtlasInkPrimitive> buildTravelReliefInk(const MapModel &map, bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;
    if(map.creationIntent!=MapCreationIntent::TravelScene || map.generation.recipe.generatorVersion<11)return ink;
    std::vector<std::uint8_t> mask(map.cells.size(),0);
    for(std::size_t i=0;i<map.cells.size();++i)mask[i]=map.cells[i].kind==MapTileKind::Mountain && (!applyFog || !map.cells[i].fogged);
    if(std::none_of(mask.begin(),mask.end(),[](auto v){return v!=0;}))return ink;
    // Resolve the actual blocked terrain as a continuous level set. Triangles
    // are clipped at the contour; no grid outlines or checkerboard faces.
    struct Sample {MapFeaturePoint p;float v;};
    constexpr int subdivisions=3;
    const int fw=map.width*subdivisions+1,fh=map.height*subdivisions+1;
    std::vector<float> field(static_cast<std::size_t>(fw)*fh);
    for(int y=0;y<fh;++y)for(int x=0;x<fw;++x)field[y*fw+x]=detail::overviewLandCoverageFromMask(map,mask,x/3.f,y/3.f,true);
    auto clipped=[&](std::array<Sample,3> a) {
        for(float threshold:{.5f,.75f,.92f}) {
        std::vector<MapFeaturePoint> polygon,crossings;
        for(int i=0;i<3;++i) {
            const auto u=a[i],v=a[(i+1)%3];const bool inside=u.v>=threshold,other=v.v>=threshold;
            if(inside)polygon.push_back(u.p);
            if(inside!=other){const float q=(threshold-u.v)/(v.v-u.v);MapFeaturePoint p{u.p.x+(v.p.x-u.p.x)*q,u.p.y+(v.p.y-u.p.y)*q};polygon.push_back(p);crossings.push_back(p);}
        }
        const float cx=(a[0].p.x+a[1].p.x+a[2].p.x)/3.f,cy=(a[0].p.y+a[1].p.y+a[2].p.y)/3.f;
        const int tone=static_cast<int>(8.f*std::sin(cx*.8f+cy*.4f)+5.f*std::cos(cy*1.2f-cx*.5f));
        const OverviewRenderColor surface{static_cast<std::uint8_t>(106+tone),static_cast<std::uint8_t>(116+tone),static_cast<std::uint8_t>(103+tone),255};
        if(threshold==.5f)for(std::size_t i=2;i<polygon.size();++i)ink.push_back({{polygon[0],polygon[i-1],polygon[i]},surface,.02f,true});
        if(crossings.size()==2)ink.push_back({crossings,threshold==.5f?OverviewRenderColor{66,76,64,255}:OverviewRenderColor{151,153,125,190},threshold==.5f?.1f:.045f,false});
        }
    };
    for(int y=0;y<fh-1;++y)for(int x=0;x<fw-1;++x){
        const Sample a{{x/3.f,y/3.f},field[y*fw+x]},b{{(x+1)/3.f,y/3.f},field[y*fw+x+1]},c{{(x+1)/3.f,(y+1)/3.f},field[(y+1)*fw+x+1]},d{{x/3.f,(y+1)/3.f},field[(y+1)*fw+x]};
        clipped({a,b,c});clipped({a,c,d});
    }
    for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x)if(mask[y*map.width+x] && mask[y*map.width+x+1] && mask[(y+1)*map.width+x] && (x*11+y*7)%5==0) {
        ink.push_back({{{x+.05f,y+.82f},{x+.51f,y+.17f},{x+.96f,y+.48f},{x+1.2f,y+.2f}},{63,77,66,230},.075f,false});
        ink.push_back({{{x+.16f,y+.84f},{x+.55f,y+.3f},{x+.94f,y+.58f}},{158,163,136,210},.05f,false});
    }
    return ink;
}
}
