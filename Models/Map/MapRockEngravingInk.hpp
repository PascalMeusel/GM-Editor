#pragma once

#include "Models/Map/MapIllustratedAtlasRiverGeometry.hpp"

namespace gm::map_render
{
inline void appendAtlasRockEngravingInk(std::vector<AtlasInkPrimitive> &ink,
                                      const std::vector<MapFeaturePoint> &silhouette,
                                      MapFeaturePoint base, float width, float height,
                                      unsigned hash, bool main)
{
    if(width<.08f || height<.08f) return;
    // Rock strata fan out downhill. Every sample is constrained to the
    // existing mountain silhouette, whose road/water clearance was checked
    // before this function. This adds material, never new blocked terrain.
    const int count=main?15:8;
    for(int ridge=0;ridge<count;++ridge)
    {
        const float side=-.88f+1.76f*(ridge+.5f)/count;
        const float start=.48f+static_cast<float>((hash>>(ridge%8*3))&15u)*.027f;
        const float finish=.09f+static_cast<float>((hash>>(ridge%7*4))&7u)*.022f;
        std::vector<MapFeaturePoint> line;
        const auto flush=[&]() {
            if(line.size()>1)
                ink.push_back({line,side<.12f?OverviewRenderColor{73,91,91,116}
                                                  :OverviewRenderColor{139,131,107,108},
                               main?.022f:.019f,false,9.f});
            line.clear();
        };
        for(int point=0;point<=10;++point)
        {
            const float t=start+(finish-start)*point/10.f;
            const float bend=.029f*std::sin(t*15.f+ridge*1.9f+hash%17u)*(1.f-t);
            const MapFeaturePoint p{base.x+width*(side*(1.f-t*.74f)+bend),base.y-height*t};
            // Leave a small material margin at the silhouette so the fine
            // hatch cannot produce dark spikes beyond the outer contour.
            const float inset=.045f*std::min(width,height);
            if(atlasPointInsidePolygon(silhouette,p) &&
               atlasPointInsidePolygon(silhouette,{p.x-inset,p.y}) &&
               atlasPointInsidePolygon(silhouette,{p.x+inset,p.y})) line.push_back(p);
            else flush();
        }
        flush();
    }
    // Broken, low-contrast sediment lines soften large blank foothill faces.
    for(int band=0;band<(main?3:2);++band)
    {
        std::vector<MapFeaturePoint> line;
        for(int point=0;point<15;++point)
        {
            const float x=-.78f+point*.11f;
            const float t=.13f+band*.13f+.035f*std::sin(x*7.f+hash%13u);
            const MapFeaturePoint p{base.x+x*width,base.y-t*height};
            if(atlasPointInsidePolygon(silhouette,p))line.push_back(p);
            else if(line.size()>1)break;
        }
        if(line.size()>1)ink.push_back({line,{129,130,105,72},.021f,false,13.f});
    }
}
} // namespace gm::map_render
