#pragma once

#include "Models/Map/MapIllustratedAtlasRiverGeometry.hpp"

namespace gm::map_render
{
inline std::vector<AtlasInkPrimitive> buildIllustratedAtlasMountainPassInk(
    const MapModel &map,const std::vector<gm::map_geometry::MapRoadCorridor> &roads)
{
    std::vector<AtlasInkPrimitive> ink;
    if(!usesIllustratedAtlasV5(map) || (map.atlasLevel!=MapLevel::World && map.atlasLevel!=MapLevel::Region))return ink;
    if(map.generation.recipe.recipeVersion>=8) return buildAtlasMountainLandformInk(map,roads);
    if(map.generation.recipe.recipeVersion>=6) {
        auto relief=buildAtlasValleyReliefInk(map,roads);
        if(map.generation.recipe.recipeVersion>=7) {
            auto crossings=buildAtlasMountainCrossingInk(map,roads);
            relief.insert(relief.end(),crossings.begin(),crossings.end());
        }
        return relief;
    }
    const auto ground=buildOverviewGroundProjection(map);
    const auto kind=[&](MapFeaturePoint p){return overviewGroundKindFromProjection(map,ground,
        static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y)));};
    const auto triangle=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,OverviewRenderColor color){
        ink.push_back({{a,b,c},color,0,true});
    };
    const auto mix=[](MapFeaturePoint a,MapFeaturePoint b,float t){
        return MapFeaturePoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
    };
    const auto hash=[](unsigned value){
        value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;
        return value^(value>>16);
    };
    const auto land=[&](MapFeaturePoint p){return p.x>=0 && p.y>=0 &&
        p.x<map.width && p.y<map.height && overviewGroundKindIsLand(kind(p));};
    auto passClearance=roads;
    for(auto &road:passClearance)
        for(const auto &feature:map.generation.graph.features)
            if(feature.id==road.featureId)
            {
                const float closeViewRadius=std::max(.14f,illustratedAtlasV2RoadWidth(feature,32.f)/64.f+.08f);
                // Pixel-minimum roads widen in cell units when zoomed out.
                // New passes must retain that widest clearance, just like the
                // surrounding mountain relief, rather than clipping the road.
                road.radiusCells=map.generation.recipe.recipeVersion>=4
                    ? std::max(road.radiusCells,closeViewRadius) : closeViewRadius;
            }
    for(const auto &road:roads)
    {
        const float length=detail::overviewPathLength(road.centerline);
        if(road.centerline.size()<2 || length<.35f)continue;
        float pathHalf=.20f;
        for(const auto &candidate:passClearance)if(candidate.featureId==road.featureId)pathHalf=candidate.radiusCells;
        // Find the mountain crossing first so both ends can dwindle into
        // foothills. The cliff is one connected contour, not a row of rocks.
        std::vector<std::pair<float,float>> crossings;
        float start=-1.f;
        for(float along=0;;along=std::min(length,along+.24f))
        {
            const bool mountain=kind(detail::sampleOverviewPath(road.centerline,along).point)==MapTileKind::Mountain;
            if(mountain && start<0)start=along;
            if(start>=0 && (!mountain || along>=length))
            {
                if(along-start>.35f)crossings.push_back({start,along});
                start=-1.f;
            }
            if(along>=length)break;
        }
        for(const auto &crossing:crossings)for(float side:{-1.f,1.f})
        {
            const auto anchor=detail::sampleOverviewPath(road.centerline,crossing.first).point;
            const unsigned salt=hash(static_cast<unsigned>(std::max(0.f,anchor.x)*17.f)*73856093u ^
                static_cast<unsigned>(std::max(0.f,anchor.y)*19.f)*19349663u ^
                map.generation.recipe.seed ^ (side<0?0x72a1u:0xb4e3u));
            const auto noise=[&](float distance,float scale,unsigned channel){
                const float position=distance/scale;
                const unsigned cell=static_cast<unsigned>(std::floor(position));
                float t=position-cell;t=t*t*(3.f-2.f*t);
                const float a=(hash(cell^salt^channel)%1024)/1023.f;
                const float b=(hash((cell+1)^salt^channel)%1024)/1023.f;
                return a+(b-a)*t;
            };
            struct CliffSection {MapFeaturePoint foot,fold,crest;float distance;};
            std::vector<CliffSection> sections;
            for(float along=crossing.first;;)
            {
                const auto p=detail::sampleOverviewPath(road.centerline,along).point;
                const auto a=detail::sampleOverviewPath(road.centerline,std::max(0.f,along-.13f)).point;
                const auto b=detail::sampleOverviewPath(road.centerline,std::min(length,along+.13f)).point;
                const float dx=b.x-a.x,dy=b.y-a.y,n=std::hypot(dx,dy);
                if(n>.001f)
                {
                    const float edge=std::min(along-crossing.first,crossing.second-along);
                    const float taper=std::min(1.f,edge/.85f);
                    // Recesses and projecting ledges break the straight road
                    // shoulder independently of the upper mountain contour.
                    const float inner=pathHalf+.085f+.31f*noise(along,.53f,93u)+
                        .10f*noise(along,.21f,317u);
                    const float cleft=std::clamp((noise(along,.69f,607u)-.16f)*4.5f,.10f,1.f);
                    const float height=(.31f+.65f*noise(along,1.13f,17u)+
                        .34f*noise(along,.29f,71u))*(.08f+.92f*taper)*cleft;
                    const auto at=[&](float offset){return MapFeaturePoint{p.x-dy/n*side*offset,p.y+dx/n*side*offset};};
                    const auto foot=at(inner),crest=at(inner+height);
                    sections.push_back({foot,mix(foot,crest,.27f+.51f*noise(along,.49f,133u)),crest,along});
                }
                if(along>=crossing.second)break;
                along=std::min(crossing.second,along+.14f+.12f*noise(along,.59f,211u));
            }
            const auto quad=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,MapFeaturePoint d,
                                OverviewRenderColor color){triangle(a,b,c,color);triangle(a,c,d,color);};
            std::vector<bool> connected(sections.size(),true);
            if(map.generation.recipe.recipeVersion>=4)
            {
                // Reject isolated slivers, then taper the ends of each valid
                // connected run. Historical recipes keep their exact ink.
                for(std::size_t i=1;i<sections.size();++i)
                {
                    const auto &a=sections[i-1],&b=sections[i];
                    const std::vector<MapFeaturePoint> outline{a.foot,a.crest,b.crest,b.foot};
                    const auto cross=[](MapFeaturePoint p,MapFeaturePoint q,MapFeaturePoint r){
                        return (q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x);
                    };
                    connected[i]=std::all_of(outline.begin(),outline.end(),land) &&
                        atlasSilhouetteClearsRoads(outline,passClearance,.026f) &&
                        cross(a.foot,a.crest,b.crest)*cross(a.foot,b.crest,b.foot)>0;
                }
                for(std::size_t first=1;first<sections.size();)
                {
                    if(!connected[first]) { ++first; continue; }
                    std::size_t end=first;
                    while(end<sections.size() && connected[end]) ++end;
                    if(end-first<3 || sections[end-1].distance-sections[first-1].distance<.65f)
                        for(std::size_t i=first;i<end;++i) connected[i]=false;
                    else
                        for(const auto index:{first-1,end-1})
                        {
                            auto &section=sections[index];
                            section.crest=mix(section.foot,section.crest,.15f);
                            section.fold=mix(section.foot,section.fold,.15f);
                        }
                    first=end;
                }
            }
            for(std::size_t i=1;i<sections.size();++i)
            {
                if(!connected[i]) continue;
                const auto &a=sections[i-1],&b=sections[i];
                const std::vector<MapFeaturePoint> outline{a.foot,a.crest,b.crest,b.foot};
                // Local clearance preserves the rest of a winding gorge when
                // an intersecting road or a tight inside bend clips one piece.
                if(!std::all_of(outline.begin(),outline.end(),land) ||
                    !atlasSilhouetteClearsRoads(outline,passClearance,.026f))continue;
                const auto cross=[](MapFeaturePoint p,MapFeaturePoint q,MapFeaturePoint r){
                    return (q.x-p.x)*(r.y-p.y)-(q.y-p.y)*(r.x-p.x);
                };
                if(cross(a.foot,a.crest,b.crest)*cross(a.foot,b.crest,b.foot)<=0)continue;
                // Broad unbroken shadow masses use the surrounding mountain
                // palette; shared sample edges disappear inside each band.
                quad(a.foot,a.crest,b.crest,b.foot,{235,220,191,255});
                const float facing=std::clamp((a.crest.x-a.foot.x)*.8f+.5f,0.f,1.f);
                const OverviewRenderColor shade{static_cast<std::uint8_t>(164-34*facing),
                    static_cast<std::uint8_t>(172-30*facing),static_cast<std::uint8_t>(166-9*facing),255};
                quad(a.foot,a.fold,b.fold,b.foot,shade);
                // Occasional exposed planes interrupt the long shadow face.
                // Their sizes follow the ledges, not a repeated rock symbol.
                const unsigned fracture=hash(static_cast<unsigned>(i)^salt);
                if(fracture%5==0)
                {
                    const auto toe=mix(a.foot,b.foot,.25f+(fracture%31)*.016f);
                    triangle(a.fold,b.fold,toe,{173,180,169,255});
                }
                else if(fracture%7==0)
                    triangle(a.fold,mix(a.crest,b.crest,.64f),b.fold,{193,192,170,255});
                ink.push_back({{a.crest,b.crest},{116,124,92,205},.042f,false});
                if(noise(a.distance,.43f,809u)>.29f)
                    ink.push_back({{a.fold,b.fold},{107,119,88,175},.027f,false});
                if(noise(a.distance,.47f,401u)>.55f)
                    ink.push_back({{a.foot,b.foot},{107,119,88,150},.027f,false});
                // Short, uneven fissures stop before the foot of the cliff;
                // never outline individual strip pieces as polygonal slabs.
                if(i>1 && hash(static_cast<unsigned>(i)^salt)%4==0)
                {
                    const auto top=mix(a.foot,a.crest,.91f);
                    const auto elbow=mix(mix(a.foot,b.foot,.28f),mix(a.crest,b.crest,.28f),.52f);
                    const auto end=mix(b.foot,b.crest,.21f);
                    ink.push_back({{top,elbow,end},{107,119,88,155},.025f,false});
                    ink.back().minimumCellPixels=18.f;
                }
            }
        }
    }
    return ink;
}

} // namespace gm::map_render
