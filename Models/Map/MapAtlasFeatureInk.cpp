#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapGouacheAtlasInk.hpp"

namespace gm::map_render {
std::vector<AtlasInkPrimitive> buildIllustratedAtlasFeatureInk(const MapModel &map, bool ways, bool symbols)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesIllustratedAtlasV3(map) && !(usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map))) return ink;
    const bool fantasy = (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map));
    auto cultivationRoads = usesIllustratedAtlasV5(map) ? illustratedAtlasRoadCorridors(map) : std::vector<gm::map_geometry::MapRoadCorridor>{};
    if(map.generation.recipe.recipeVersion>=4 && usesIllustratedAtlasV5(map))
        for(const auto &feature:map.generation.graph.features)
            if(feature.kind==MapGeneratedFeatureKind::River && feature.points.size()>=2)
                cultivationRoads.push_back({feature.id,mapFeaturePresentationPoints(map,feature),
                    std::max(.22f,feature.width*.12f+.12f)});
    const OverviewRenderColor edge{70,65,51,255}, paper{242,228,187,255}, roof = fantasy ? OverviewRenderColor{122, 60,72,255} : OverviewRenderColor{141,80,58,255};
    const auto triangle=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,OverviewRenderColor color) {
        ink.push_back({{a,b,c},color,0,true});
    };
    const auto quad=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,MapFeaturePoint d,OverviewRenderColor color) {
        triangle(a,b,c,color); triangle(a,c,d,color);
    };
    const auto crossings = ways ? illustratedAtlasCrossingBridges(map) : std::vector<MapGeneratedFeature>{};
    if(ways && usesIllustratedAtlasV5(map) && !usesGouacheAtlasArtwork(map))
        for(const auto &road:map.generation.graph.features)
        {
            if((road.kind!=MapGeneratedFeatureKind::Road && road.kind!=MapGeneratedFeatureKind::Trail) || road.points.size()<2)continue;
            const auto curve=overviewFinalRoadCurvePoints(map,road);
            const float length=detail::overviewPathLength(curve);
            for(bool start:{true,false})
            {
                const auto endpoint=start?curve.front():curve.back();
                bool joins=false;
                for(const auto &other:map.generation.graph.features)
                {
                    if(other.id==road.id || (other.kind!=MapGeneratedFeatureKind::Road && other.kind!=MapGeneratedFeatureKind::Trail))continue;
                    for(std::size_t i=1;i<other.points.size();++i)
                        if(gm::map_geometry::roadPointSegmentDistanceSquared(endpoint,other.points[i-1],other.points[i])<.00001f)joins=true;
                }
                if(!joins)continue;
                const auto inside=detail::sampleOverviewPath(curve,start?std::min(.45f,length):std::max(0.f,length-.45f)).point;
                // Continue the branch surface through the receiving road's
                // outline, instead of leaving a capped stub or dark seam.
                const float dx=endpoint.x-inside.x,dy=endpoint.y-inside.y;
                const float span=std::max(.00001f,std::hypot(dx,dy));
                const float half=illustratedAtlasV2RoadWidth(road,32.f)/64.f;
                quad({inside.x-dy/span*half,inside.y+dx/span*half},
                     {endpoint.x-dy/span*half,endpoint.y+dx/span*half},
                     {endpoint.x+dy/span*half,endpoint.y-dx/span*half},
                     {inside.x+dy/span*half,inside.y-dx/span*half},
                     {kOverviewRoadSurface.r,kOverviewRoadSurface.g,kOverviewRoadSurface.b,255});
            }
        }
    for (const auto &feature:crossings)
    {
        if (!ways || feature.kind!=MapGeneratedFeatureKind::Bridge || feature.points.size()<2) continue;
        const auto a=feature.points.front(), b=feature.points.back();
        const float length=std::max(.001f,std::hypot(b.x-a.x,b.y-a.y));
        const float nx=-(b.y-a.y)/length, ny=(b.x-a.x)/length;
        const auto point=[&](float t,float side){return MapFeaturePoint{a.x+(b.x-a.x)*t+nx*side,a.y+(b.y-a.y)*t+ny*side};};
        const float half = feature.id.rfind("resolved_crossing_",0)==0 ? feature.width*.5f : .19f;
        // A small masonry arch in the same oblique illustration language as
        // the towns. The opening is empty geometry, leaving actual water
        // visible underneath; it never paints a fake blue hole on dry land.
        const OverviewRenderColor stone{177,170,139,255}, shade{128,132,114,255},
            coping{218,205,165,255}, outline{112,117,101,255}, deck{191,171,125,255};
        const float depth=std::min(.21f,length*.22f);
        // Show the lower-facing side consistently, even on reversed paths.
        const float facing=(ny<-.001f || (std::abs(ny)<=.001f && nx<0.f)) ? -1.f : 1.f;
        const auto arch=[&](float t){return half+depth*(1.f-.83f*std::sin(t*3.14159265f));};
        std::vector<MapFeaturePoint> archEdge;
        for(int i=0;i<12;++i)
        {
            const float t=i/12.f,u=(i+1)/12.f;
            quad(point(t,facing*half),point(u,facing*half),
                 point(u,facing*arch(u)),point(t,facing*arch(t)),shade);
            archEdge.push_back(point(t,facing*arch(t)));
        }
        archEdge.push_back(point(1,facing*arch(1)));
        ink.push_back({archEdge,outline,.025f,false});
        for(float t:{.14f,.29f,.43f,.57f,.71f,.86f})
        {
            ink.push_back({{point(t,facing*(half+.012f)),
                            point(t,facing*arch(t))},{103,112,99,150},.016f,false});
            ink.back().minimumCellPixels=36.f;
        }
        // Short flared stone seats lead into the deck, without a crossbar
        // across the road or four oversized square posts.
        for(float t:{0.f,1.f})
        {
            const float inward=t==0.f?.10f:-.10f;
            for(float side:{-1.f,1.f})
                quad(point(t,side*(half+.035f)),point(t+inward,side*half),
                     point(t+inward,side*half*.57f),point(t,side*half*.48f),stone);
        }
        quad(point(0,-half*.65f),point(1,-half*.65f),point(1,half*.65f),point(0,half*.65f),deck);
        for(float side:{-1.f,1.f})
        {
            quad(point(.06f,side*half),point(.94f,side*half),
                 point(.94f,side*half*.65f),point(.06f,side*half*.65f),stone);
            ink.push_back({{point(.07f,side*half*.65f),point(.93f,side*half*.65f)},
                           {129,123,98,190},.022f,false});
            ink.push_back({{point(.04f,side*half),point(.96f,side*half)},coping,.045f,false});
            // Sparse masonry joints emerge at inspection zoom only.
            const int blocks=std::clamp(static_cast<int>(length/.34f),2,10);
            for(int j=1;j<blocks;++j)
            {
                const float t=j/static_cast<float>(blocks);
                ink.push_back({{point(t,side*half*.68f),point(t,side*half)},outline,.017f,false});
                ink.back().minimumCellPixels=40.f;
            }
        }
    }
    if (!symbols) return ink;
    if (ways && !usesGouacheAtlasArtwork(map)) {
        auto entrances=buildAtlasCityEntrances(map);
        ink.insert(ink.end(),entrances.begin(),entrances.end());
    }
    for (int y=0;y<map.height;++y) for (int x=0;x<map.width;++x)
    {
        const auto *cell=mapCellAt(map,x,y);
        if (!cell) continue;
        MapTileKind symbolKind=cell->kind;
        // Named entities survive terrain painting. Their glyph must not
        // disappear merely because water/forest replaced the old cell stamp.
        if(usesIllustratedAtlasV5(map))for(const auto &site:map.generation.graph.features)
            if(!site.points.empty() && static_cast<int>(std::floor(site.points.front().x))==x &&
               static_cast<int>(std::floor(site.points.front().y))==y)
            {
                if(site.kind==MapGeneratedFeatureKind::Settlement){symbolKind=MapTileKind::Settlement;break;}
                if(site.kind==MapGeneratedFeatureKind::Landmark)symbolKind=MapTileKind::Landmark;
            }
        if (fantasy && symbolKind==MapTileKind::Landmark)
        {
            if(!usesIllustratedAtlasV5(map) && !overviewGroundKindIsLand(overviewGroundKindForRender(map,x,y)))continue;
            // A cartographic rosette marks an existing point of interest.
            // It does not invent a site type (castle, shrine, etc.) which the
            // campaign never specified. Its name remains the authoritative key.
            MapFeaturePoint centre{x+.5f,y+.5f};
            std::string regionalMaterial;
            for (const auto &feature : map.generation.graph.features)
                if (feature.kind == MapGeneratedFeatureKind::Landmark && !feature.points.empty() &&
                    static_cast<int>(std::floor(feature.points.front().x)) == x && static_cast<int>(std::floor(feature.points.front().y)) == y)
                { centre = feature.points.front(); regionalMaterial = feature.materialId; }
            if (map.atlasLevel == MapLevel::Region && map.generation.recipe.styleVersion == 6 &&
                regionalMaterial.rfind("illustrated.region.",0)==0) {
                // Typed generated sites get recognizable atlas symbols. An
                // authored generic landmark keeps its original rosette below.
                auto p=[&](float dx,float dy){return MapFeaturePoint{centre.x+dx,centre.y+dy};};
                auto line=[&](std::vector<MapFeaturePoint> v,OverviewRenderColor c,float w=.05f){ink.push_back({std::move(v),c,w,false});};
                auto box=[&](float l,float t,float r,float b,OverviewRenderColor c){quad(p(l,t),p(r,t),p(r,b),p(l,b),c);};
                const OverviewRenderColor stone{175,188,167,255}, shade{109,132,119,255},light{229,220,178,255};
                if (regionalMaterial=="illustrated.region.ruins" || regionalMaterial=="illustrated.region.gate") {
                    const bool ruin=regionalMaterial=="illustrated.region.ruins";
                    box(-.48f,-.24f,-.27f,.29f,stone);box(.26f,-.34f,.47f,.29f,stone);
                    box(-.48f,-.24f,-.40f,.29f,shade);box(.38f,-.34f,.47f,.29f,shade);
                    if(!ruin){box(-.34f,-.45f,.34f,-.22f,light);line({p(-.34f,-.22f),p(-.19f,-.38f),p(.18f,-.38f),p(.34f,-.22f)},edge,.047f);}
                    line({p(-.53f,.31f),p(-.48f,-.27f),p(-.36f,-.34f),p(-.27f,-.24f),p(-.27f,.26f)},edge);
                    line({p(.23f,.29f),p(.26f,-.34f),p(.47f,-.34f),p(.52f,.31f)},edge);
                    for(int b=0;b<3;++b)line({p(-.38f+b*.29f,.39f),p(-.20f+b*.29f,.39f)},shade,.075f);
                } else if (regionalMaterial=="illustrated.region.watch") {
                    box(-.28f,-.34f,.28f,.36f,light);box(.14f,-.34f,.28f,.36f,stone);
                    box(-.08f,.10f,.06f,.36f,edge);box(-.07f,-.21f,.03f,-.07f,shade);
                    triangle(p(-.38f,-.35f),p(0,-.66f),p(.37f,-.35f),roof);
                    line({p(-.28f,.36f),p(-.28f,-.34f),p(.28f,-.34f),p(.28f,.36f)},edge);
                    line({p(-.39f,-.35f),p(0,-.68f),p(.39f,-.35f)},edge);
                } else if (regionalMaterial=="illustrated.region.shrine") {
                    box(-.34f,-.17f,.34f,.29f,light);box(.18f,-.17f,.34f,.29f,stone);
                    triangle(p(-.45f,-.18f),p(0,-.56f),p(.44f,-.18f),{100,146,143,255});
                    line({p(-.45f,-.18f),p(0,-.56f),p(.44f,-.18f)},edge);
                    box(-.11f,-.01f,.11f,.29f,shade);line({p(-.43f,.33f),p(.43f,.33f)},edge,.065f);
                    line({p(0,-.56f),p(0,-.71f)},edge,.037f);
                } else if (map.generation.recipe.recipeVersion>=5 && regionalMaterial=="illustrated.region.monolith") {
                    // A narrow stroke disappeared against adjacent relief at
                    // play scale. A shaded obelisk and low plinth retain the
                    // site's identity inside the established symbol clearance.
                    box(-.32f,.27f,.33f,.39f,shade);
                    box(-.27f,.23f,.28f,.30f,stone);
                    quad(p(-.22f,.24f),p(-.18f,-.38f),p(.02f,-.54f),p(.04f,.24f),light);
                    quad(p(.04f,.24f),p(.02f,-.54f),p(.20f,-.39f),p(.24f,.24f),shade);
                    line({p(-.22f,.24f),p(-.18f,-.38f),p(.02f,-.54f),p(.20f,-.39f),p(.24f,.24f)},edge,.04f);
                    line({p(-.09f,-.26f),p(-.02f,-.16f),p(-.10f,-.05f),p(-.03f,.07f)},shade,.04f);
                    line({p(-.32f,.39f),p(.33f,.39f)},edge,.035f);
                } else {
                    const int count=regionalMaterial=="illustrated.region.stones"?3:1;
                    for(int k=0;k<count;++k){const float dx=count==1?0.f:(k-1)*.34f,dy=k==1?-.16f:0;
                        quad(p(dx-.14f,dy+.30f),p(dx-.11f,dy-.34f),p(dx+.05f,dy-.43f),p(dx+.17f,dy+.30f),stone);
                        triangle(p(dx+.02f,dy-.4f),p(dx+.17f,dy+.30f),p(dx+.04f,dy+.30f),shade);
                        line({p(dx-.14f,dy+.30f),p(dx-.11f,dy-.34f),p(dx+.05f,dy-.43f),p(dx+.17f,dy+.30f)},edge,.04f);
                        if(count==1)line({p(-.04f,-.20f),p(.05f,-.10f),p(-.03f,.02f),p(.04f,.13f)},light,.035f);
                    }
                }
                continue;
            }
            std::vector<MapFeaturePoint> ring;
            for (int i=0;i<=32;++i)
            {
                const float a=i*6.2831853f/32.f;
                ring.push_back({centre.x+std::cos(a)*.36f,centre.y+std::sin(a)*.36f});
                if (i>0) triangle(centre,ring[i-1],ring[i],paper);
            }
            ink.push_back({ring,edge,.034f,false});
            for (int i=0;i<8;++i)
            {
                const float a=i*6.2831853f/8.f-1.5707963f;
                const float radius=i%2==0?.43f:.27f;
                const MapFeaturePoint tip{centre.x+std::cos(a)*radius,centre.y+std::sin(a)*radius};
                const MapFeaturePoint side{centre.x+std::cos(a+.7f)*.12f,centre.y+std::sin(a+.7f)*.12f};
                const MapFeaturePoint other{centre.x+std::cos(a-.7f)*.12f,centre.y+std::sin(a-.7f)*.12f};
                triangle(centre,tip,side,{191,136,57,255});
                triangle(centre,other,tip,{233,194,105,255});
                ink.push_back({{other,tip,side},edge,.024f,false});
            }
            continue;
        }
        if (symbolKind!=MapTileKind::Settlement) continue;
        std::string material;
        float cx=x+.5f,cy=y+.5f;
        for (const auto &feature:map.generation.graph.features)
            if (feature.kind==MapGeneratedFeatureKind::Settlement && !feature.points.empty() &&
                static_cast<int>(std::floor(feature.points.front().x))==x && static_cast<int>(std::floor(feature.points.front().y))==y)
            {
                material=feature.materialId;
                cx=feature.points.front().x; cy=feature.points.front().y;
            }
        if (usesIllustratedAtlasV5(map) && !material.empty())
        {
            // Tiny cultivated parcels locate a visible settlement in its
            // countryside. They belong to released place artwork, never to
            // the gameplay raster, and cannot disclose a hidden town.
            const unsigned parcelSeed = map.generation.recipe.seed ^ static_cast<unsigned>(x*73856093u) ^ static_cast<unsigned>(y*19349663u);
            const bool regionalV6=map.atlasLevel==MapLevel::Region && map.generation.recipe.generatorVersion>=12 && map.generation.recipe.styleVersion>=6;
            std::vector<std::array<float,4>> cultivatedBounds;
            for (int parcel=0;parcel<(regionalV6?4:3);++parcel)
            {
                const float side=(parcelSeed>>(parcel+2))&1u ? 1.f : -1.f;
                const float px=cx+side*((regionalV6?1.18f:.95f)+parcel*.42f), py=cy+.56f+parcel*.28f;
                const unsigned parcelVariation=(parcelSeed>>(parcel*4)) ^ (parcelSeed*static_cast<unsigned>(parcel+3));
                const float w=regionalV6?.74f+(parcelVariation%9)*.057f:.65f+(parcelSeed%7)*.035f;
                const float h=regionalV6?.38f+((parcelVariation>>4)%7)*.055f:.35f+parcel*.035f;
                std::vector<MapFeaturePoint> patch{{px-w,py},{px+w*.70f,py-.20f},{px+w,py+h},{px-w*.65f,py+h+.13f}};
                if(regionalV6)
                {
                    const float angle=(static_cast<int>((parcelVariation>>8)%17)-8)*.073f;
                    const float ca=std::cos(angle),sa=std::sin(angle);
                    for(auto &q:patch){const float dx=q.x-px,dy=q.y-py;q={px+dx*ca-dy*sa,py+dx*sa+dy*ca};}
                }
                const bool clearLand=std::all_of(patch.begin(),patch.end(),[&](auto q){
                    const int tx=static_cast<int>(std::floor(q.x)),ty=static_cast<int>(std::floor(q.y));
                    if (!isMapCellInside(map,tx,ty)) return false;
                    const auto kind=overviewGroundKindForRender(map,tx,ty);
                    return kind==MapTileKind::Empty || kind==MapTileKind::Dirt || kind==MapTileKind::Hill;
                });
                if (!clearLand || !atlasSilhouetteClearsRoads(patch,cultivationRoads,.07f)) continue;
                if(regionalV6)
                {
                    std::array<float,4> bounds{patch[0].x,patch[0].y,patch[0].x,patch[0].y};
                    for(auto q:patch){bounds[0]=std::min(bounds[0],q.x);bounds[1]=std::min(bounds[1],q.y);bounds[2]=std::max(bounds[2],q.x);bounds[3]=std::max(bounds[3],q.y);}
                    if(std::any_of(cultivatedBounds.begin(),cultivatedBounds.end(),[&](const auto &other){
                        return bounds[0]<other[2]+.07f && bounds[2]+.07f>other[0] && bounds[1]<other[3]+.07f && bounds[3]+.07f>other[1];})) continue;
                    cultivatedBounds.push_back(bounds);
                }
                const OverviewRenderColor fill=parcel%2 ? OverviewRenderColor{191,193,128,185} : OverviewRenderColor{213,186,121,185};
                quad(patch[0],patch[1],patch[2],patch[3],fill);
                auto boundary=patch;boundary.push_back(patch.front());
                ink.push_back({boundary,{137,141,93,170},.023f,false});
                const bool orchard=regionalV6 && (parcelVariation%4==0);
                for (float t : {.25f,.50f,.75f})
                {
                    const MapFeaturePoint a{patch[0].x+(patch[1].x-patch[0].x)*t,patch[0].y+(patch[1].y-patch[0].y)*t};
                    const MapFeaturePoint b{patch[3].x+(patch[2].x-patch[3].x)*t,patch[3].y+(patch[2].y-patch[3].y)*t};
                    if(!orchard) ink.push_back({{a,b},{153,147,96,145},.018f,false});
                    else for(float row:{.30f,.67f})
                    {
                        const MapFeaturePoint center{a.x+(b.x-a.x)*row,a.y+(b.y-a.y)*row};
                        // Small orchard crowns stay within released cultivated parcels;
                        // they imply cultivation, not blocking forest or new entities.
                        const float radius=.075f;
                        const MapFeaturePoint n{center.x,center.y-radius},e{center.x+radius,center.y},
                                              south{center.x,center.y+radius},west{center.x-radius,center.y};
                        quad(n,e,south,west,{112,143,90,215});
                        ink.push_back({{n,e,south,west,n},{85,109,69,190},.018f,false});
                    }
                }
            }
        }
        const bool city=material.find("city")!=std::string::npos || material.find("capital")!=std::string::npos;
        const bool village=material.find("village")!=std::string::npos;
        if (usesGouacheAtlasArtwork(map)) {
            auto town=buildGouacheAtlasSettlementInk(map,{cx,cy},material);
            ink.insert(ink.end(),town.begin(),town.end());
            continue;
        }
        if(map.generation.recipe.recipeVersion>=6) {
            const unsigned settlementSeed=map.generation.recipe.seed ^ unsigned(x)*73856093u ^ unsigned(y)*19349663u;
            const int count=village?2+int(settlementSeed%2):city?5:4;
            const OverviewRenderColor roofColors[]={{122,60,72,255},{102,126,123,255},{160,107,70,255},{127,115,90,255}};
            for(int house=0;house<count;++house) {
                const unsigned h=(settlementSeed ^ unsigned(house+7)*2654435761u);
                const float dx=(house-(count-1)*.5f)*(city?.24f:.29f);
                const float w=.12f+(h%5)*.012f, height=.21f+((h>>5)%8)*.031f;
                const float base=cy+.16f-((h>>12)%3)*.09f, top=base-height;
                const float hx=cx+dx, side=.07f;
                quad({hx-w,base},{hx+w,base},{hx+w,top},{hx-w,top},paper);
                quad({hx+w,base},{hx+w+side,base-.06f},{hx+w+side,top-.06f},{hx+w,top},{179,170,140,255});
                const auto tint=roofColors[(h>>17)%4];
                const MapFeaturePoint peak{hx-w*.12f,top-.13f};
                triangle({hx-w-.025f,top},peak,{hx+w+.025f,top},tint);
                quad(peak,{peak.x+side,peak.y-.06f},{hx+w+side+.025f,top-.06f},{hx+w+.025f,top},
                    {std::uint8_t(tint.r*.78f),std::uint8_t(tint.g*.78f),std::uint8_t(tint.b*.78f),255});
                ink.push_back({{{hx-w,base},{hx-w,top},peak,{hx+w,top},{hx+w,base}},edge,.025f,false});
                ink.push_back({{{hx-.045f,base},{hx-.045f,base-.11f}},edge,.042f,false});
                if(height>.30f)ink.push_back({{{hx+.055f,top+.08f},{hx+.055f,top+.13f}},{95,110,99,255},.038f,false});
                if((h>>23)%2)ink.push_back({{{hx+.08f,top-.03f},{hx+.08f,top-.18f}},{124,117,99,255},.046f,false});
                if(h%3==0)ink.push_back({{{hx-w+.02f,base-.13f},{hx+w-.02f,base-.13f}},{158,125,85,200},.021f,false});
            }
            // Larger places retain a civic landmark without every settlement
            // repeating the same three identical flag towers.
            if(city) {
                quad({cx-.11f,cy-.15f},{cx+.10f,cy-.15f},{cx+.10f,cy-.79f},{cx-.11f,cy-.79f},paper);
                triangle({cx-.15f,cy-.79f},{cx-.01f,cy-1.03f},{cx+.14f,cy-.79f},roofColors[(settlementSeed>>3)%4]);
                ink.push_back({{{cx-.11f,cy-.16f},{cx-.11f,cy-.79f},{cx-.01f,cy-1.03f},{cx+.10f,cy-.79f},{cx+.10f,cy-.16f}},edge,.027f,false});
                ink.push_back({{{cx,cy-.67f},{cx,cy-.57f}},edge,.045f,false});
            }
            ink.push_back({{{cx-.59f,cy+.22f},{cx-.20f,cy+.25f},{cx+.31f,cy+.23f},{cx+.59f,cy+.20f}},{130,126,93,170},.026f,false});
            continue;
        }
        const int houses=village?1:3;
        for (int house=0;house<houses;++house)
        {
            const float dx=houses==1?0.f:(house-1)*.29f;
            const float w=city?.22f:.18f, h=city && house==1?.62f:.36f;
            const float by=cy+.16f-(house==1?.07f:0.f);
            const MapFeaturePoint a{cx+dx-w,by},b{cx+dx+w,by},c{cx+dx+w,by-h},d{cx+dx-w,by-h};
            quad(a,b,c,d,paper);
            const MapFeaturePoint peak{cx+dx,by-h-.19f};
            triangle({d.x-.055f,d.y},peak,{c.x+.055f,c.y},roof);
            ink.push_back({{a,d,peak,c,b,a},edge,.039f,false});
            ink.push_back({{{cx+dx,by-.02f},{cx+dx,by-.15f}},edge,.065f,false});
        }
        if (fantasy && !village)
        {
            // A civic spire distinguishes towns; cities add flanking turrets.
            // Symbol detail is presentation only, not a new campaign entity.
            const int towers = city ? 3 : 1;
            for (int tower=0;tower<towers;++tower)
            {
                const float tx=cx+(towers==1?0.f:(tower-1)*.38f);
                const float base=cy+.08f, top=cy-(tower==1 || towers==1?.69f:.44f);
                const float w=tower==1 || towers==1?.14f:.115f;
                const MapFeaturePoint a{tx-w,base}, b{tx+w,base}, c{tx+w,top}, d{tx-w,top};
                quad(a,b,c,d,paper);
                const MapFeaturePoint tip{tx,top-.29f};
                triangle({tx-w-.045f,top},tip,{tx+w+.045f,top},roof);
                ink.push_back({{a,d,tip,c,b},edge,.035f,false});
                ink.push_back({{{tx,top+.12f},{tx,top+.22f}},edge,.06f,false});
                if (tower==1 || towers==1)
                {
                    ink.push_back({{tip,{tip.x,tip.y-.24f}},edge,.025f,false});
                    triangle({tip.x,tip.y-.24f},{tip.x+.25f,tip.y-.16f},{tip.x,tip.y-.09f},{165,75,63,255});
                }
            }
        }
        ink.push_back({{{cx-.53f,cy+.20f},{cx+.53f,cy+.20f}},edge,.035f,false});
    }
    return ink;
}
}
