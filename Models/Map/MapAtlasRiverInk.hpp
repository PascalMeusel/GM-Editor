#pragma once

#include "Models/Map/MapAtlasRiverDetails.hpp"

namespace gm::map_render
{
inline std::vector<AtlasInkPrimitive> buildIllustratedAtlasRiverInk(
    const MapModel &map,const MapGeneratedFeature &feature,bool mouthOnly=false)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesIllustratedAtlasRiverMesh(map) || feature.points.size()<2) return ink;
    auto points=illustratedAtlasUntrimmedRiverPoints(map,feature);
    const auto mouth=trimIllustratedAtlasRiverMouth(map,points,feature.id.rfind("manual_stroke_",0)==0);
    if (mouthOnly && !mouth.joinsWater) return ink;
    const float halfWidth=std::max(.065f,std::max(1.f,feature.width)*atlasPresentationProfile(map).riverWidthCells*.5f);
    bool joinsChannel=false;
    if (!mouth.joinsWater)
        for(const auto &other:map.generation.graph.features)
        {
            if(other.id==feature.id || other.kind!=MapGeneratedFeatureKind::River) continue;
            for(std::size_t i=1;i<other.points.size();++i)
            {
                const auto a=other.points[i-1],b=other.points[i],p=feature.points.back();
                const float dx=b.x-a.x,dy=b.y-a.y,length2=dx*dx+dy*dy;
                const float t=length2>.000001f?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length2,0.f,1.f):0.f;
                if(std::hypot(p.x-a.x-dx*t,p.y-a.y-dy*t)<.035f) joinsChannel=true;
            }
        }
    struct Station { MapFeaturePoint point; float along; float width; };
    std::vector<Station> stations;
    const auto sourceKind=atlasRiverEndpointKind(map,feature,points.front());
    const bool taperedSource=sourceKind==AtlasRiverEndpointKind::OpenGround || sourceKind==AtlasRiverEndpointKind::Mountain;
    const bool inlandSink=!mouth.joinsWater && !joinsChannel &&
        atlasRiverEndpointKind(map,feature,points.back())==AtlasRiverEndpointKind::OpenGround;
    float sourceDistance=0.f;
    for (std::size_t i=0;i<points.size();++i)
    {
        if(i)sourceDistance+=std::hypot(points[i].x-points[i-1].x,points[i].y-points[i-1].y);
        if (!stations.empty() && std::hypot(points[i].x-stations.back().point.x,points[i].y-stations.back().point.y)<.002f) continue;
        const auto terminal=mouth.joinsWater?mouth.shore:feature.points.back();
        const float distance=mouth.joinsWater||joinsChannel?-std::hypot(points[i].x-terminal.x,points[i].y-terminal.y):-10.f;
        if (mouthOnly && distance<-.40f) continue;
        const float opening=mouth.joinsWater?1.f+.45f*std::clamp((distance+.8f)/.8f,0.f,1.f):1.f;
        const bool naturalEnds=map.generation.recipe.recipeVersion>=6;
        const float growth=std::clamp(sourceDistance/(naturalEnds?2.2f:.95f),0.f,1.f);
        const float sourceFloor=naturalEnds?.035f:.20f;
        const float sourceWidth=taperedSource?sourceFloor+(1.f-sourceFloor)*growth*growth*(3.f-2.f*growth):1.f;
        const float sinkDistance=std::hypot(points[i].x-points.back().x,points[i].y-points.back().y);
        const float sinkGrowth=std::clamp(sinkDistance/.8f,0.f,1.f);
        const float sinkFloor=naturalEnds?.06f:.45f;
        const float sinkWidth=inlandSink?sinkFloor+(1.f-sinkFloor)*sinkGrowth*sinkGrowth*(3.f-2.f*sinkGrowth):1.f;
        stations.push_back({points[i],distance,halfWidth*overviewRiverWidthFactor(i,points.size())*opening*sourceWidth*sinkWidth});
    }
    if (stations.empty()) return ink;
    if (!mouthOnly && stations.size()>=2 && sourceKind!=AtlasRiverEndpointKind::Confluence)
    {
        const auto first=stations.front(),inner=stations[1];
        const float dx=first.point.x-inner.point.x,dy=first.point.y-inner.point.y,len=std::max(.0001f,std::hypot(dx,dy));
        std::vector<Station> cap;
        for(const float t:{1.f,.8660254f,.5f})
            cap.push_back({{first.point.x+dx/len*first.width*t,first.point.y+dy/len*first.width*t},
                first.along-first.width*t,first.width*std::sqrt(std::max(0.f,1.f-t*t))});
        stations.insert(stations.begin(),cap.begin(),cap.end());
    }
    const auto ground=buildOverviewGroundProjection(map);
    std::vector<std::uint8_t> land(ground.size());
    for (std::size_t i=0;i<ground.size();++i) land[i]=overviewGroundKindIsLand(ground[i])?1:0;
    const auto inWater=[&](MapFeaturePoint p) {
        return p.x>=0.f && p.y>=0.f && p.x<map.width && p.y<map.height &&
            detail::overviewLandCoverageFromMask(map,land,p.x,p.y)<.5f;
    };
    if (mouth.joinsWater)
    {
        const float blendReach=map.generation.recipe.recipeVersion>=6 ? std::clamp(.65f+halfWidth*1.5f,.8f,1.25f) : std::clamp(1.1f+halfWidth*2.f,1.15f,1.8f);
        for (int i=1;i<=16;++i)
        {
            const float along=blendReach*i/16.f;
            const auto center=MapFeaturePoint{mouth.shore.x+mouth.direction.x*along,mouth.shore.y+mouth.direction.y*along};
            float width=halfWidth*(1.45f+(map.generation.recipe.recipeVersion>=6?.55f:1.25f)*along/blendReach);
            // Keep a continuous fan. Repeated width reductions at an oblique
            // coast produced a pinched, flat-ended tube. Fade individual
            // vertices against the receiving shore below instead.
            if(along>.12f && !inWater(center))break;
            stations.push_back({center,along,width});
        }
    }
    else if (!mouthOnly && stations.size()>=2)
    {
        const auto last=stations.back(),inner=stations[stations.size()-2];
        const float dx=last.point.x-inner.point.x,dy=last.point.y-inner.point.y,len=std::max(.0001f,std::hypot(dx,dy));
        for(const float t:{.5f,.8660254f,1.f})
            stations.push_back({{last.point.x+dx/len*last.width*t,last.point.y+dy/len*last.width*t},joinsChannel?last.width*t:-10.f,
                last.width*std::sqrt(std::max(0.f,1.f-t*t))});
    }
    if (stations.size()<2) return ink;
    const float mouthLength=mouth.joinsWater?std::max(.055f,stations.back().along):.88f;
    const auto smooth=[](float v) {v=std::clamp(v,0.f,1.f);return v*v*(3.f-2.f*v);};
    struct Vertex { MapFeaturePoint point; OverviewRenderColor color; };
    std::vector<std::array<Vertex,7>> rows;
    rows.reserve(stations.size());
    // A painted river may enter and leave the same lake. Dissolve every
    // submerged reach, not just the final endpoint, so it cannot sit like a
    // blue pipe on top of the lake. Distances are in map units, independent
    // of the number of authored controls.
    std::vector<float> submerged(stations.size(),1000.f);
    for(std::size_t i=0;i<stations.size();++i)
    {
        if(!inWater(stations[i].point))submerged[i]=0.f;
        else if(i)submerged[i]=submerged[i-1]+std::hypot(stations[i].point.x-stations[i-1].point.x,stations[i].point.y-stations[i-1].point.y);
    }
    for(std::size_t i=stations.size()-1;i>0;--i)
        submerged[i-1]=std::min(submerged[i-1],submerged[i]+std::hypot(stations[i].point.x-stations[i-1].point.x,stations[i].point.y-stations[i-1].point.y));
    constexpr float offsets[7]={-1.12f,-1.f,-.84f,0.f,.84f,1.f,1.12f};
    for (std::size_t i=0;i<stations.size();++i)
    {
        const auto prev=stations[i?i-1:i].point,next=stations[std::min(i+1,stations.size()-1)].point;
        const float dx=next.x-prev.x,dy=next.y-prev.y,length=std::max(.0001f,std::hypot(dx,dy));
        const auto &station=stations[i];
        const float fadeStart=std::min(.12f,mouthLength*.15f);
        const float fade=mouth.joinsWater?smooth((station.along-fadeStart)/(mouthLength-fadeStart)):0.f;
        const float bankFade=mouth.joinsWater?smooth((station.along+.48f)/(.48f+std::min(.12f,mouthLength))):
            joinsChannel?smooth((station.along+halfWidth*1.5f)/(halfWidth*1.5f)):0.f;
        const auto water=illustratedAtlasTerrainColor(map,station.point.x,station.point.y,&ground);
        std::array<Vertex,7> row;
        for (int side=0;side<7;++side)
        {
            const bool bank=side<=1||side>=5;
            const bool fringe=side==0||side==6;
            const float blend=mouth.joinsWater?smooth(std::max(0.f,station.along)/mouthLength):0.f;
            OverviewRenderColor c=bank?OverviewRenderColor{36,82,88,255}:OverviewRenderColor{51,119,129,255};
            if (side==3) c={58,126,134,255};
            if(mouth.joinsWater && !bank)
            {
                const float flat=smooth((station.along+.75f)/.75f);
                c.r=static_cast<std::uint8_t>(std::lround(c.r+(61-c.r)*flat));
                c.g=static_cast<std::uint8_t>(std::lround(c.g+(125-c.g)*flat));
                c.b=static_cast<std::uint8_t>(std::lround(c.b+(134-c.b)*flat));
            }
            const auto channel=[&](int a,int b) {return static_cast<std::uint8_t>(std::lround(a+(b-a)*blend));};
            c={channel(c.r,water.r),channel(c.g,water.g),channel(c.b,water.b),
                static_cast<std::uint8_t>(std::lround(255.f*(1.f-fade)*(bank?1.f-bankFade:1.f)))};
            // Dissolve the rounded end inside the receiving channel instead
            // of leaving a visible capsule on top of its continuous water.
            if(joinsChannel)
                c.a=static_cast<std::uint8_t>(std::lround(c.a*(1.f-smooth((station.along+.10f)/(halfWidth+.10f)))));
            if(sourceKind==AtlasRiverEndpointKind::Confluence)
            {
                const float distance=std::hypot(station.point.x-points.front().x,station.point.y-points.front().y);
                c.a=static_cast<std::uint8_t>(std::lround(c.a*smooth(distance/std::max(.15f,halfWidth))));
            }
            if (fringe) c.a=0;
            if(!mouth.joinsWater || station.along<-.5f)
                c.a=static_cast<std::uint8_t>(std::lround(c.a*(1.f-smooth(submerged[i]/.85f))));
            const float spread=mouth.joinsWater?smooth((station.along+.25f)/(.25f+mouthLength)):0.f;
            const float lateral=offsets[side]*((side==2 || side==4)?1.f-.55f*spread:1.f);
            if(side==2 || side==4)c.a=static_cast<std::uint8_t>(std::lround(c.a*(1.f-.4f*spread)));
            const MapFeaturePoint position{station.point.x-dy/length*station.width*lateral,
                                          station.point.y+dx/length*station.width*lateral};
            if(mouth.joinsWater && station.along>.12f)
            {
                const float wet=1.f-detail::overviewLandCoverageFromMask(map,land,position.x,position.y);
                c.a=static_cast<std::uint8_t>(std::lround(c.a*smooth(wet*2.f)));
            }
            row[side]={position,c};
        }
        rows.push_back(row);
    }
    const auto triangle=[&](const Vertex &a,const Vertex &b,const Vertex &c) {
        AtlasInkPrimitive primitive{{a.point,b.point,c.point},a.color,0.f,true};
        primitive.vertexColors={a.color,b.color,c.color}; ink.push_back(std::move(primitive));
    };
    for (std::size_t i=1;i<rows.size();++i) for (int side=0;side<6;++side)
    {
        triangle(rows[i-1][side],rows[i-1][side+1],rows[i][side+1]);
        triangle(rows[i-1][side],rows[i][side+1],rows[i][side]);
    }
    if(!mouthOnly)
    {
        auto details=buildAtlasRiverIllustrationDetails(map,feature,points,mouth,halfWidth);
        ink.insert(ink.end(),details.begin(),details.end());
    }
    return ink;
}

} // namespace gm::map_render
