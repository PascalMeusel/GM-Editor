#pragma once
#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
// Ear clipping retains inward notches of a broken ridge silhouette.
inline void appendAtlasPolygonInk(std::vector<AtlasInkPrimitive> &ink,
                                  const std::vector<MapFeaturePoint> &p,OverviewRenderColor color)
{
    if (p.size() < 3) return;
    const auto cross = [](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c) {
        return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    };
    float area = 0;
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < p.size(); ++i)
    {
        area += p[i].x*p[(i+1)%p.size()].y-p[(i+1)%p.size()].x*p[i].y;
        indices.push_back(i);
    }
    if (area < 0) std::reverse(indices.begin(),indices.end());
    while (indices.size() >= 3)
    {
        bool clipped = false;
        for (std::size_t i = 0; i < indices.size(); ++i)
        {
            const auto a = p[indices[(i+indices.size()-1)%indices.size()]], b = p[indices[i]],
                       c = p[indices[(i+1)%indices.size()]];
            if (cross(a,b,c) <= .000001f) continue;
            bool occupied = false;
            for (std::size_t j = 0; j < indices.size(); ++j)
            {
                if (j == i || j == (i+1)%indices.size() || j == (i+indices.size()-1)%indices.size()) continue;
                const auto q = p[indices[j]];
                if (cross(a,b,q) >= -.000001f && cross(b,c,q) >= -.000001f && cross(c,a,q) >= -.000001f)
                { occupied = true; break; }
            }
            if (occupied) continue;
            ink.push_back({{a,b,c},color,0.f,true});
            indices.erase(indices.begin()+static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) break;
    }
}
} // namespace gm::map_render
