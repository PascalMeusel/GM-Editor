#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapNaturalMaterialInk.hpp"
#include "Models/Map/MapRoofMaterialInk.hpp"

namespace gm::map_render
{
float illustratedAtlasLabelTerrainCost(const MapModel &map, float left, float top, float right, float bottom)
{
    // Recipe 5 scores the full label plate, including the outer glyph/halo
    // edges missed by interior-only samples. The compiled ink mask separately
    // scores actual overhanging relief without guessing from neighboring cells.
    const bool edgeAware = map.generation.recipe.recipeVersion >= 5;
    float cost = 0.f;
    for (int iy=0;iy<3;++iy) for (int ix=0;ix<5;++ix)
    {
        const float x=left+(right-left)*(edgeAware ? ix/4.f : (ix+.5f)/5.f),
                    y=top+(bottom-top)*(edgeAware ? iy/2.f : (iy+.5f)/3.f);
        const int tx=static_cast<int>(std::floor(x)), ty=static_cast<int>(std::floor(y));
        if (!isMapCellInside(map,tx,ty)) { cost+=5.f; continue; }
        const auto kind=overviewGroundKindForRender(map,tx,ty);
        cost += kind==MapTileKind::Mountain || kind==MapTileKind::Stone ? 5.f :
                kind==MapTileKind::Forest ? (edgeAware ? 6.f : 3.f) : kind==MapTileKind::Water ? .4f : 0.f;
        // Illustrated summits rise north of their gameplay anchor.
        if (isMapCellInside(map,tx,ty+1) && (!edgeAware || !mapCellAt(map,tx,ty+1)->fogged) &&
            overviewGroundKindForRender(map,tx,ty+1)==MapTileKind::Mountain)
            cost+=2.f;
    }
    return cost;
}

AtlasLabelInkMask buildAtlasLabelInkMask(const MapModel &map,const std::vector<AtlasInkPrimitive> &ink) {
    AtlasLabelInkMask mask;
    if(map.generation.recipe.recipeVersion<5 || !usesIllustratedAtlas(map) || map.width<=0 || map.height<=0 || ink.empty())return mask;
    // Compiled once with the reviewed render plan, never rebuilt for each
    // label or input frame. Cap storage even for unusually large authored maps.
    mask.samplesPerCell=std::min(4.f,2048.f/static_cast<float>(std::max(map.width,map.height)));
    mask.width=static_cast<int>(std::ceil(map.width*mask.samplesPerCell));
    mask.height=static_cast<int>(std::ceil(map.height*mask.samplesPerCell));
    mask.covered.resize(static_cast<std::size_t>(mask.width)*mask.height,0);
    for(const auto& primitive:ink) {
        if(!primitive.filled || primitive.points.size()<3 || primitive.color.a==0)continue;
        float left=primitive.points[0].x,right=left,top=primitive.points[0].y,bottom=top;
        for(const auto p:primitive.points){left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);}
        const int x0=std::clamp(static_cast<int>(std::floor(left*mask.samplesPerCell)),0,mask.width-1);
        const int x1=std::clamp(static_cast<int>(std::ceil(right*mask.samplesPerCell)),0,mask.width-1);
        const int y0=std::clamp(static_cast<int>(std::floor(top*mask.samplesPerCell)),0,mask.height-1);
        const int y1=std::clamp(static_cast<int>(std::ceil(bottom*mask.samplesPerCell)),0,mask.height-1);
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
            auto& occupied=mask.covered[static_cast<std::size_t>(y)*mask.width+x];
            if(!occupied && atlasPointInsidePolygon(primitive.points,{(x+.5f)/mask.samplesPerCell,(y+.5f)/mask.samplesPerCell}))occupied=1;
        }
    }
    return mask;
}

float illustratedAtlasLabelInkCost(const AtlasLabelInkMask &mask,float left,float top,float right,float bottom) {
    if(mask.covered.empty())return 0.f;
    float cost=0.f;
    for(int iy=0;iy<3;++iy)for(int ix=0;ix<7;++ix) {
        const int x=static_cast<int>(std::floor((left+(right-left)*ix/6.f)*mask.samplesPerCell));
        const int y=static_cast<int>(std::floor((top+(bottom-top)*iy/2.f)*mask.samplesPerCell));
        if(x>=0 && y>=0 && x<mask.width && y<mask.height && mask.covered[static_cast<std::size_t>(y)*mask.width+x])cost+=1.5f;
    }
    return cost;
}

std::vector<AtlasInkPrimitive> buildTacticalStructureInk(const MapModel &map) {
    std::vector<AtlasInkPrimitive> ink;
    if (!supportsFootprintRoofInk(map) || map.width <= 0 || map.height <= 0) return ink;
    std::vector<bool> used(static_cast<std::size_t>(map.width * map.height), false);
    auto available = [&](int x, int y) {
        const auto *cell = mapCellAt(map,x,y);
        return cell && cell->kind == MapTileKind::Building && !used[static_cast<std::size_t>(y*map.width+x)];
    };
    auto triangle = [&](MapFeaturePoint a, MapFeaturePoint b, MapFeaturePoint c, OverviewRenderColor color) {
        ink.push_back({{a,b,c},color,.04f,true});
    };
    auto stroke = [&](std::vector<MapFeaturePoint> p, OverviewRenderColor color, float width) {
        ink.push_back({std::move(p),color,width,false});
    };
    if(usesIllustratedTacticalV12(map) && map.generation.recipe.recipeVersion>=10)
        appendSanctuaryPavingInk(ink,map);
    for(const auto &feature:map.generation.graph.features) {
        if(feature.materialId!="fantasy_neutral.tactical.waystone" || feature.points.empty()) continue;
        const auto centre=feature.points.front();
        const auto *cell=mapCellAt(map,static_cast<int>(std::floor(centre.x)),static_cast<int>(std::floor(centre.y)));
        if(!cell || cell->kind!=MapTileKind::Cover) continue;
        const float x=centre.x,y=centre.y;
        const std::vector<MapFeaturePoint> outline{{x-.31f,y+.35f},{x-.34f,y-.19f},{x-.11f,y-.46f},
                                                  {x+.23f,y-.38f},{x+.34f,y+.23f},{x+.17f,y+.39f},{x-.31f,y+.35f}};
        for(int i=0;i<6;++i) triangle({x,y},outline[i],outline[i+1],{110,123,107,255});
        triangle({x-.11f,y-.46f},{x+.04f,y+.29f},{x-.31f,y+.35f},{158,164,136,255});
        triangle({x+.23f,y-.38f},{x+.34f,y+.23f},{x+.04f,y+.29f},{69,82,77,255});
        stroke(outline,{40,50,46,255},.065f);
        stroke({{x-.09f,y-.24f},{x+.05f,y-.10f},{x-.07f,y+.02f},{x+.04f,y+.13f}},{116,184,164,235},.05f);
        stroke({{x-.09f,y-.24f},{x-.11f,y+.16f}},{116,184,164,235},.045f);
    }
    if(usesIllustratedTacticalV12(map) && map.generation.recipe.recipeVersion<10) {
        for(const auto &f:map.generation.graph.features) {
            if(f.materialId!="fantasy_neutral.tactical.sanctuary" || f.points.empty())continue;
            const auto centre=f.points.front();
            const auto onStone=[&](MapFeaturePoint p){const auto *c=mapCellAt(map,int(p.x),int(p.y));return c && c->kind==MapTileKind::Stone && c->wayKind==MapWayKind::None;};
            // Broken concentric paving survives fit-to-window view while each
            // segment remains on the actual walkable stone apron.
            for(float scale:{.43f,.77f,.96f}) {
                std::vector<MapFeaturePoint> arc;
                for(int i=0;i<=96;++i) {
                    const float angle=i*6.2831853f/96.f;
                    const MapFeaturePoint p{centre.x+std::cos(angle)*f.width*scale,centre.y+std::sin(angle)*f.width*scale};
                    if(onStone(p))arc.push_back(p);else{if(arc.size()>1)stroke(arc,{132,126,101,175},.045f);arc.clear();}
                }
                if(arc.size()>1)stroke(arc,{132,126,101,175},.045f);
            }
            for(int i=0;i<12;++i) {
                const float angle=i*6.2831853f/12.f;
                for(float radius=f.width*.43f;radius<f.width*.96f;radius+=.15f) {
                    const MapFeaturePoint a{centre.x+std::cos(angle)*radius,centre.y+std::sin(angle)*radius};
                    const MapFeaturePoint z{centre.x+std::cos(angle)*(radius+.15f),centre.y+std::sin(angle)*(radius+.15f)};
                    if(onStone(a)&&onStone(z))stroke({a,z},{146,139,113,160},.035f);
                }
            }
        }
    }
    if(usesIllustratedTacticalV12(map)) {
        for(const auto &f:map.generation.graph.features) {
            if(f.materialId!="fantasy_neutral.tactical.market_stall" || f.points.size()<2)continue;
            const float x=f.points.front().x-.4f,y=f.points.front().y;
            const auto *a=mapCellAt(map,int(f.points.front().x),int(y)),*z=mapCellAt(map,int(f.points.back().x),int(y));
            if(!a || !z || a->kind!=MapTileKind::Cover || z->kind!=MapTileKind::Cover)continue;
            for(int i=0;i<6;++i) {
                const float l=x+i*.30f,r=l+.30f;
                const OverviewRenderColor c=i%2?OverviewRenderColor{219,203,155,255}:OverviewRenderColor{147,86,72,255};
                triangle({l,y-.34f},{r,y-.34f},{r,y+.28f},c);triangle({l,y-.34f},{r,y+.28f},{l,y+.28f},c);
            }
            stroke({{x,y-.34f},{x+1.8f,y-.34f},{x+1.8f,y+.28f},{x,y+.28f},{x,y-.34f}},{75,61,45,255},.055f);
            stroke({{x,y+.40f},{x+1.8f,y+.40f}},{107,80,48,255},.13f);
            for(float xx:{x+.10f,x+1.70f})stroke({{xx,y-.44f},{xx,y+.44f}},{91,69,46,255},.055f);
        }
    }
    const auto templateKind=map.metadata.customProperties.find("tactical.template_kind");
    if(templateKind!=map.metadata.customProperties.end() && templateKind->second=="0") {
        const float cx=map.width/2+.5f,cy=map.height/2+.5f;
        const auto *centre=mapCellAt(map,map.width/2,map.height/2);
        if(centre && centre->kind==MapTileKind::Hazard) {
            if(map.generation.recipe.recipeVersion>=10 && usesIllustratedTacticalV12(map))
                appendWeatheredAltarInk(ink,{cx,cy},map.generation.recipe.seed);
            else {
            const std::vector<MapFeaturePoint> edge{{cx-.9f,cy-.48f},{cx+.9f,cy-.48f},{cx+.9f,cy+.48f},{cx-.9f,cy+.48f},{cx-.9f,cy-.48f}};
            triangle(edge[0],edge[1],edge[2],{100,108,95,255});triangle(edge[0],edge[2],edge[3],{100,108,95,255});
            stroke(edge,{45,51,44,255},.10f);
            stroke({{cx-.76f,cy-.34f},{cx+.76f,cy-.34f},{cx+.76f,cy+.28f}},{190,177,132,245},.06f);
            stroke({{cx-.23f,cy},{cx,cy-.23f},{cx+.23f,cy},{cx,cy+.23f},{cx-.23f,cy}},{139,193,165,245},.055f);
            }
        }
    }
    if(templateKind!=map.metadata.customProperties.end() && templateKind->second=="5") {
        const float cx=map.width/2+.5f,cy=map.height/2+.5f;
        const auto *centre=mapCellAt(map,map.width/2,map.height/2);
        if(centre && centre->kind==MapTileKind::Water) {
            if(map.generation.recipe.recipeVersion>=10 && usesIllustratedTacticalV12(map))
                appendFittedFountainInk(ink,map);
            else {
            std::vector<MapFeaturePoint> outside,inside;
            for(int i=0;i<=32;++i) {
                const float angle=i*6.28318530718f/32.f;
                outside.push_back({cx+std::cos(angle)*2.25f,cy+std::sin(angle)*2.25f});
                inside.push_back({cx+std::cos(angle)*1.92f,cy+std::sin(angle)*1.92f});
            }
            for(int i=0;i<32;++i) {
                triangle(outside[i],outside[i+1],inside[i+1],{124,124,102,255});
                triangle(outside[i],inside[i+1],inside[i],{124,124,102,255});
                if(i%2==0) stroke({outside[i],inside[i]},{62,64,58,220},.045f);
            }
            stroke(outside,{53,60,56,255},.075f);stroke(inside,{188,181,140,245},.055f);
            stroke({{cx-.35f,cy},{cx+.35f,cy}},{196,217,187,230},.05f);
            stroke({{cx,cy-.35f},{cx,cy+.35f}},{196,217,187,230},.05f);
            }
        }
    }
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x) {
        if (!available(x,y)) continue;
        int w=1,h=1;
        while(x+w<map.width && available(x+w,y)) ++w;
        while(y+h<map.height) {
            bool row=true;for(int dx=0;dx<w;++dx) row = row && available(x+dx,y+h);
            if(!row) break;
            ++h;
        }
        for(int dy=0;dy<h;++dy) for(int dx=0;dx<w;++dx) used[static_cast<std::size_t>((y+dy)*map.width+x+dx)]=true;
        const float l=x+.06f,r=x+w-.06f,t=y+.06f,b=y+h-.06f;
        const bool slate=(static_cast<unsigned>(x*31+y*17)+map.generation.recipe.seed)%3u==0;
        OverviewRenderColor light=slate?OverviewRenderColor{111,131,135,255}:OverviewRenderColor{160,117,70,255};
        OverviewRenderColor dark=slate?OverviewRenderColor{64,81,90,255}:OverviewRenderColor{104,69,45,255};
        const bool settlement11=map.atlasLevel==MapLevel::Site && map.mapKind==MapKind::Settlement && map.generation.recipe.generatorVersion>=11;
        bool temple=false,guildhall=false;
        if(settlement11) {
            for(const auto &f:map.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::BuildingGroup && !f.points.empty() && int(f.points.front().x)==x && int(f.points.front().y)==y) {
                temple=f.materialId=="fantasy_neutral.settlement.building.temple";
                guildhall=f.materialId=="fantasy_neutral.settlement.building.guildhall";
            }
            const unsigned palette=(static_cast<unsigned>(x)*313u+static_cast<unsigned>(y)*971u+map.generation.recipe.seed)%7u;
            if(temple){light={119,137,169,255};dark={57,69,107,255};}
            else if(guildhall || palette==0){light={162,101,99,255};dark={103,54,66,255};}
            else if(palette==1){light={182,140,90,255};dark={118,82,48,255};}
        }
        // Hipped ends and staggered shingle courses read as a roof rather
        // than the former cross-strapped box. All eaves remain in the exact
        // building footprint, including rectangles split from edited L-shapes.
        const bool weatheredRoof=map.generation.recipe.recipeVersion>=10 && usesIllustratedLocalV6(map);
        if(weatheredRoof)appendWeatheredRoofMaterial(ink,map,l,t,r,b,light,dark);
        else if(w>=h) {
            const float mid=(t+b)*.5f,inset=std::min((r-l)*.27f,(b-t)*.38f);
            const MapFeaturePoint a{l+inset,mid},z{r-inset,mid};
            triangle({l,t},{r,t},z,light);triangle({l,t},z,a,light);
            triangle({l,b},a,z,dark);triangle({l,b},z,{r,b},dark);
            triangle({l,t},a,{l,b},settlement11?OverviewRenderColor{static_cast<unsigned char>(light.r*.83f),static_cast<unsigned char>(light.g*.83f),static_cast<unsigned char>(light.b*.83f),255}:(slate?OverviewRenderColor{91,115,122,255}:OverviewRenderColor{127,97,65,255}));triangle({r,t},{r,b},z,settlement11?OverviewRenderColor{static_cast<unsigned char>(dark.r*.83f),static_cast<unsigned char>(dark.g*.83f),static_cast<unsigned char>(dark.b*.83f),255}:(slate?OverviewRenderColor{52,67,77,255}:OverviewRenderColor{77,65,55,255}));
            stroke({a,z},{194,161,108,255},.065f);
            stroke({{l,t},a,{l,b}},{48,39,32,220},.035f);
            stroke({{r,t},z,{r,b}},{48,39,32,220},.035f);
            for(float yy=t+.23f;yy<b;yy+=.24f) {
                float taper=1.f-std::abs(yy-mid)/std::max(.01f,(b-t)*.5f);
                stroke({{l+inset*taper,yy},{r-inset*taper,yy}},{43,34,28,105},.022f);
            }
        } else {
            const float mid=(l+r)*.5f,inset=std::min((b-t)*.27f,(r-l)*.38f);
            const MapFeaturePoint a{mid,t+inset},z{mid,b-inset};
            triangle({l,t},a,z,light);triangle({l,t},z,{l,b},light);
            triangle({r,t},{r,b},z,dark);triangle({r,t},z,a,dark);
            triangle({l,t},{r,t},a,settlement11?OverviewRenderColor{static_cast<unsigned char>(light.r*.83f),static_cast<unsigned char>(light.g*.83f),static_cast<unsigned char>(light.b*.83f),255}:(slate?OverviewRenderColor{91,115,122,255}:OverviewRenderColor{127,97,65,255}));triangle({l,b},z,{r,b},settlement11?OverviewRenderColor{static_cast<unsigned char>(dark.r*.83f),static_cast<unsigned char>(dark.g*.83f),static_cast<unsigned char>(dark.b*.83f),255}:(slate?OverviewRenderColor{52,67,77,255}:OverviewRenderColor{77,65,55,255}));
            stroke({a,z},{194,161,108,255},.065f);
            stroke({{l,t},a,{r,t}},{48,39,32,220},.035f);
            stroke({{l,b},z,{r,b}},{48,39,32,220},.035f);
            for(float xx=l+.23f;xx<r;xx+=.24f) {
                float taper=1.f-std::abs(xx-mid)/std::max(.01f,(r-l)*.5f);
                stroke({{xx,t+inset*taper},{xx,b-inset*taper}},{43,34,28,105},.022f);
            }
        }
        if(!weatheredRoof && map.generation.recipe.recipeVersion>=5 && usesIllustratedLocalV6(map)) {
            // Staggered shingle joints replace the uniform plank-like surface.
            // Clip each course to both hip boundaries, including narrow edited
            // footprints. Decoration never changes the building's collision cells.
            const bool horizontal=w>=h;
            const float u0=horizontal?l:t,u1=horizontal?r:b;
            const float v0=horizontal?t:l,v1=horizontal?b:r;
            const float mid=(v0+v1)*.5f;
            const float inset=std::min((u1-u0)*.27f,(v1-v0)*.38f);
            const auto p=[&](float u,float v){return horizontal?MapFeaturePoint{u,v}:MapFeaturePoint{v,u};};
            const auto hip=[&](float v){return inset*(1.f-std::abs(v-mid)/std::max(.01f,(v1-v0)*.5f));};
            int row=0;
            for(float v=v0+.025f;v<v1-.045f;v+=.24f,++row) {
                const float end=std::min(v+.20f,v1-.045f);
                const float clip=std::max(hip(v),hip(end))+.035f;
                // The course crossing the ridge must also respect its peak inset.
                const float edge=(v<mid && end>mid)?inset+.035f:clip;
                for(float u=u0+.16f+(row%2)*.18f;u<u1-edge;u+=.36f) {
                    if(u<u0+edge) continue;
                    stroke({p(u,v),p(u,end)},{46,39,34,84},.018f);
                    const float tileEnd=std::min(u+.28f,u1-edge);
                    if(tileEnd-u>.10f)
                        stroke({p(u+.04f,end),p(tileEnd,end)},{225,203,163,65},.018f);
                }
            }
        }
        if(settlement11 && w>=2 && h>=2) {
            // A contained dormer breaks the repeated roof silhouette without
            // extending past the actual blocked building footprint.
            const float mx=(l+r)*.5f,my=(t+b)*.5f;
            if(temple) {
                const float q=std::min(r-l,b-t)*.25f;
                triangle({mx,my-q},{mx-q*.68f,my+q*.50f},{mx,my+q*.80f},{179,188,198,255});
                triangle({mx,my-q},{mx,my+q*.80f},{mx+q*.68f,my+q*.50f},{65,88,129,255});
                stroke({{mx,my-q},{mx-q*.68f,my+q*.50f},{mx,my+q*.80f},{mx+q*.68f,my+q*.50f},{mx,my-q}},{38,46,71,255},.05f);
                stroke({{mx-.10f,my+.17f},{mx,my-.02f},{mx+.10f,my+.17f}},{221,199,123,255},.06f);
            } else if((x+y)%3==0 || guildhall) {
                const float q=.23f;
                triangle({mx-q,my+.28f},{mx+q,my+.28f},{mx,my-.04f},{221,193,139,255});
                stroke({{mx-q,my+.28f},{mx,my-.04f},{mx+q,my+.28f}},{74,55,44,255},.07f);
                stroke({{mx,my+.13f},{mx,my+.25f}},{47,55,52,255},.09f);
            }
        }
        stroke({{l,t},{r,t},{r,b},{l,b},{l,t}},weatheredRoof?OverviewRenderColor{58,58,49,245}:OverviewRenderColor{43,35,30,255},weatheredRoof?.065f:.12f);
        stroke({{l+.1f,t+.1f},{r-.1f,t+.1f}},{226,199,143,210},.035f);
        if(w*h>=6) {
            const float chimneyX=l+.35f,chimneyY=t+.3f;
            triangle({chimneyX,chimneyY},{chimneyX+.32f,chimneyY},{chimneyX+.32f,chimneyY+.38f},{110,105,90,255});
            triangle({chimneyX,chimneyY},{chimneyX+.32f,chimneyY+.38f},{chimneyX,chimneyY+.38f},{110,105,90,255});
            stroke({{chimneyX,chimneyY},{chimneyX+.32f,chimneyY},{chimneyX+.32f,chimneyY+.38f},{chimneyX,chimneyY+.38f},{chimneyX,chimneyY}},{36,38,35,255},.05f);
            stroke({{chimneyX+.04f,chimneyY+.05f},{chimneyX+.28f,chimneyY+.05f}},{38,39,35,255},.09f);
        }

    }
    if(usesIllustratedTacticalV12(map)) {
        for(const auto &feature:map.generation.graph.features) {
            if(feature.kind!=MapGeneratedFeatureKind::Bridge || feature.points.size()<2)continue;
            const auto a=feature.points.front(),z=feature.points.back();
            const float length=std::hypot(z.x-a.x,z.y-a.y);if(length<.2f)continue;
            const float nx=-(z.y-a.y)/length,ny=(z.x-a.x)/length;
            const float half=std::clamp(feature.width*.46f,.28f,.65f);
            const auto p=[&](float t,float side){return MapFeaturePoint{a.x+(z.x-a.x)*t+nx*side,a.y+(z.y-a.y)*t+ny*side};};
            const bool stone=feature.materialId.find("stone")!=std::string::npos;
            const OverviewRenderColor deck=stone?OverviewRenderColor{197,190,162,255}:OverviewRenderColor{177,134,82,255};
            triangle(p(0,-half),p(1,-half),p(1,half),deck);triangle(p(0,-half),p(1,half),p(0,half),deck);
            stroke({p(0,-half),p(1,-half),p(1,half),p(0,half),p(0,-half)},{69,57,41,255},.08f);
            const int boards=std::max(3,static_cast<int>(length*(stone?1.5f:3.f)));
            for(int i=1;i<boards;++i)stroke({p(float(i)/boards,-half+.05f),p(float(i)/boards,half-.05f)},{94,78,52,220},.03f);
            for(float side:{-1.f,1.f}) {
                stroke({p(0,side*(half+.04f)),p(1,side*(half+.04f))},{98,79,50,255},.085f);
                stroke({p(.03f,side*(half-.03f)),p(.97f,side*(half-.03f))},{226,207,156,255},.035f);
                for(float t:{.06f,.5f,.94f})stroke({p(t,side*(half-.11f)),p(t,side*(half+.15f))},{64,57,43,255},.10f);
            }
        }
    }
    return ink;
}
}
