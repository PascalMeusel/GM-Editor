#pragma once

#include "Models/Map/MapIllustratedAtlas.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gm::map_render
{
// A river is one continuous colored surface, not a sequence of separately
// antialiased line segments. The bank and water interpolate across shared
// cross sections; only the outer fringe and open-water outlet fade away.
inline bool usesIllustratedAtlasRiverMesh(const MapModel &map)
{
    return usesIllustratedAtlasV5(map) && map.generation.recipe.generatorVersion>=11 &&
        (map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region);
}

enum class AtlasRiverEndpointKind { Continuation, Confluence, Water, Mountain, OpenGround };

inline AtlasRiverEndpointKind atlasRiverEndpointKind(const MapModel &map,
    const MapGeneratedFeature &river, MapFeaturePoint p)
{
    if(p.x<.3f || p.y<.3f || p.x>map.width-.3f || p.y>map.height-.3f)
        return AtlasRiverEndpointKind::Continuation;
    for(const auto &other:map.generation.graph.features)
        if(other.id!=river.id && other.kind==MapGeneratedFeatureKind::River)
            for(std::size_t i=1;i<other.points.size();++i)
                if(gm::map_geometry::roadPointSegmentDistanceSquared(p,other.points[i-1],other.points[i])<.0324f)
                    return AtlasRiverEndpointKind::Confluence;
    const int x=static_cast<int>(std::floor(p.x)),y=static_cast<int>(std::floor(p.y));
    if(!overviewGroundKindIsLand(overviewGroundKindForRender(map,x,y))) return AtlasRiverEndpointKind::Water;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
        if(overviewGroundKindForRender(map,x+dx,y+dy)==MapTileKind::Mountain)
            return AtlasRiverEndpointKind::Mountain;
    return AtlasRiverEndpointKind::OpenGround;
}

inline std::vector<AtlasInkPrimitive> buildAtlasRiverIllustrationDetails(const MapModel &map,
    const MapGeneratedFeature &feature,const std::vector<MapFeaturePoint> &points,
    const AtlasRiverMouth &mouth,float halfWidth)
{
    std::vector<AtlasInkPrimitive> ink;
    if(points.size()<2) return ink;
    const OverviewRenderColor bank{161,158,110,180},reed{107,132,102,220},
        rock{143,154,137,255},rockLight{176,183,157,255},
        shallow{76,145,150,255},glint{137,184,174,125};
    const auto roads=illustratedAtlasRoadCorridors(map);
    const auto clear=[&](MapFeaturePoint p,float radius) {
        for(const auto &road:roads)for(std::size_t i=1;i<road.centerline.size();++i)
            if(gm::map_geometry::roadPointSegmentDistanceSquared(p,road.centerline[i-1],road.centerline[i])<
               (radius+.3f)*(radius+.3f))return false;
        for(const auto &site:map.generation.graph.features)
            if((site.kind==MapGeneratedFeatureKind::Settlement || site.kind==MapGeneratedFeatureKind::Landmark) &&
               !site.points.empty() && std::hypot(p.x-site.points.front().x,p.y-site.points.front().y)<radius+.7f)return false;
        return true;
    };
    const auto stroke=[&](std::vector<MapFeaturePoint> p,OverviewRenderColor color,float width,float minimum=0.f) {
        ink.push_back({std::move(p),color,width,false});ink.back().minimumCellPixels=minimum;
    };
    const auto polygon=[&](const std::vector<MapFeaturePoint> &p,OverviewRenderColor color) {
        for(std::size_t i=2;i<p.size();++i)ink.push_back({{p[0],p[i-1],p[i]},color,0,true});
    };
    // Sparse longitudinal highlights and broken banks, sampled by distance
    // rather than control-point count. These are not cross-channel tick marks.
    const float total=detail::overviewPathLength(points);
    for(float distance=1.1f;distance<total-.9f;distance+=2.3f)
    {
        const auto sample=detail::sampleOverviewPath(points,distance);
        const auto a=detail::sampleOverviewPath(points,std::max(0.f,distance-.26f)).point;
        const auto b=detail::sampleOverviewPath(points,std::min(total,distance+.35f)).point;
        const float dx=b.x-a.x,dy=b.y-a.y,len=std::max(.001f,std::hypot(dx,dy));
        const auto p=sample.point;
        if(!clear(p,.3f))continue;
        if(!overviewGroundKindIsLand(overviewGroundKindForRender(map,static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y)))))continue;
        const float side=static_cast<int>(distance/2.3f)%2 ? -1.f : 1.f;
        const auto offset=[&](MapFeaturePoint q,float v){return MapFeaturePoint{q.x-dy/len*v,q.y+dx/len*v};};
        stroke({offset(a,halfWidth*.16f),offset(p,halfWidth*.25f),offset(b,halfWidth*.12f)},
               glint,std::min(.028f,halfWidth*.16f),8.f);
        const float bankOffset=side*(halfWidth*1.1f+.065f);
        const auto dry=offset(p,bankOffset);
        if(overviewGroundKindIsLand(overviewGroundKindForRender(map,static_cast<int>(dry.x),static_cast<int>(dry.y))))
            stroke({offset(a,bankOffset),dry,offset(b,bankOffset)},bank,.04f,10.f);
    }
    for(bool start:{true,false})
    {
        const auto p=start?points.front():points.back();
        const auto inner=start?points[1]:points[points.size()-2];
        auto kind=(!start && mouth.joinsWater)?AtlasRiverEndpointKind::Water:atlasRiverEndpointKind(map,feature,p);
        if(kind==AtlasRiverEndpointKind::Continuation || kind==AtlasRiverEndpointKind::Confluence)continue;
        const float dx=inner.x-p.x,dy=inner.y-p.y,len=std::max(.001f,std::hypot(dx,dy));
        const float tx=dx/len,ty=dy/len;
        const auto at=[&](float along,float side){return MapFeaturePoint{p.x+tx*along-ty*side,p.y+ty*along+tx*side};};
        if(!clear(p,.55f))continue;
        if(kind==AtlasRiverEndpointKind::Water)
        {
            // Broken currents follow the outflow. Crosswise concentric arcs
            // looked like additional shoreline bands across the mouth.
            for(float side:{-.16f,.19f})
            {
                std::vector<MapFeaturePoint> current;
                for(int i=0;i<=5;++i)
                {
                    const float t=i/5.f;
                    const auto q=at(-.62f-.46f*t,side*(.7f+.18f*std::sin(t*3.14159265f)));
                    if(!overviewGroundKindIsLand(overviewGroundKindForRender(map,static_cast<int>(std::floor(q.x)),static_cast<int>(std::floor(q.y)))))current.push_back(q);
                    else {current.clear();break;}
                }
                if(current.size()>1)stroke(current,{153,192,175,55},.018f,18.f);
            }
            continue;
        }
        const auto stone=[&](float along,float side,float size) {
            // Low, rounded bank stones: no bright polygon facets or detached
            // black outlines competing with the atlas mountain silhouettes.
            std::vector<MapFeaturePoint> shape;
            for(int i=0;i<10;++i)
            {
                const float angle=i*6.2831853f/10;
                shape.push_back(at(along+std::cos(angle)*size*.55f,
                    side+std::sin(angle)*size*.38f));
            }
            polygon(shape,rock);
            stroke({at(along-size*.30f,side-size*.10f),at(along,side-size*.18f),
                at(along+size*.23f,side-size*.10f)},rockLight,.025f,12.f);
        };
        const auto tuft=[&](float along,float side) {
            const auto root=at(along,side);
            for(float lean:{-.09f,.015f,.11f})
                stroke({root,{root.x+lean*.35f,root.y-.10f},
                    {root.x+lean,root.y-.16f-std::abs(lean)*.3f}},reed,.023f,10.f);
        };
        if(kind==AtlasRiverEndpointKind::Mountain)
        {
            // A compact rock cleft surrounds the actual narrow headwater;
            // the stream surface is always the continuous river mesh.
            if(map.generation.recipe.recipeVersion<6) {
                stone(-.045f,-.10f,.26f);stone(.035f,.085f,.18f);
            } else {
                // Broken earth contours read as a spring cleft; detached
                // pebble circles looked like loose symbols at the river tip.
                stroke({at(-.09f,-.09f),at(.08f,-.13f),at(.32f,-.16f)},rock,.025f,10.f);
                stroke({at(-.04f,.085f),at(.12f,.12f),at(.29f,.11f)},rockLight,.02f,12.f);
            }
            stroke({at(-.10f,-.035f),at(-.01f,0),at(.12f,.025f)},
                   {63,110,116,210},.025f,10.f);
            stroke({at(.17f,-.12f),at(.34f,-.17f),at(.48f,-.15f)},bank,.027f,12.f);
        }
        else if(start)
        {
            // The channel itself narrows into a seep. No separate polygon
            // pool or highlight rectangle is pasted across its first reach.
            stone(-.045f,-.09f,.12f);
            tuft(.10f,.12f);
            stroke({at(.12f,-.12f),at(.33f,-.15f),at(.48f,-.13f)},bank,.022f,12.f);
        }
        else
        {
            // A soft damp margin around the channel, not an opaque angular
            // pond stamp. The ending still belongs to the authored river.
            const auto centre=at(-.08f,0);
            for(int i=0;i<20;++i)
            {
                const float a=i*6.2831853f/20,b=(i+1)*6.2831853f/20;
                AtlasInkPrimitive patch{{centre,at(-.08f+std::cos(a)*.38f,std::sin(a)*.29f),
                    at(-.08f+std::cos(b)*.38f,std::sin(b)*.29f)},shallow,0,true};
                patch.vertexColors={{102,144,129,100},{102,144,129,0},{102,144,129,0}};
                ink.push_back(std::move(patch));
            }
            tuft(-.16f,-.25f);tuft(.06f,.25f);
            stroke({at(-.26f,-.08f),at(-.39f,-.03f)},bank,.023f,12.f);
        }
    }
    return ink;
}

} // namespace gm::map_render
