#pragma once

#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
// Fine foliage stays inside the already-cleared crown. No new terrain,
// collision footprint, fog visibility or asset dependency is introduced.
inline void appendBotanicalCanopyInk(std::vector<AtlasInkPrimitive> &ink,
                                    const std::vector<MapFeaturePoint> &outline,
                                    MapFeaturePoint center, float radiusX, float radiusY,
                                    unsigned hash, bool pine, bool topDown)
{
    if (outline.size() < 3) return;
    const bool golden = hash % 11u == 0;
    const OverviewRenderColor light = golden ? OverviewRenderColor{180,185,116,255}
                                             : OverviewRenderColor{143,174,118,255};
    const OverviewRenderColor dark = pine ? OverviewRenderColor{56,103,83,255}
                                          : OverviewRenderColor{65,106,73,255};
    const OverviewRenderColor middle = golden ? OverviewRenderColor{137,155,89,255}
                                               : OverviewRenderColor{104,143,96,255};
    const auto mix = [](OverviewRenderColor a, OverviewRenderColor b, float t) {
        const auto channel = [t](unsigned x, unsigned y) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(x * (1.f-t) + y * t), 0l, 255l));
        };
        return OverviewRenderColor{channel(a.r,b.r),channel(a.g,b.g),channel(a.b,b.b),255};
    };
    const auto edgeColor = [&](MapFeaturePoint p) {
        const float nx = (p.x-center.x) / std::max(.01f,radiusX);
        const float ny = (p.y-center.y) / std::max(.01f,radiusY);
        const float facing = std::clamp(.45f - nx*.23f - ny*.24f, .05f, .95f);
        return mix(dark, light, facing);
    };
    for (std::size_t i=0; i<outline.size(); ++i)
    {
        const auto a=outline[i], b=outline[(i+1)%outline.size()];
        // Local maps already supply an eight-point distant silhouette. Do
        // not draw the full crown beneath it at overview zoom.
        AtlasInkPrimitive triangle{{center,a,b},middle,0.f,true,topDown?4.f:0.f};
        triangle.vertexColors={mix(middle,light,.34f),edgeColor(a),edgeColor(b)};
        ink.push_back(std::move(triangle));
    }
    auto border=outline; border.push_back(border.front());
    ink.push_back({border,{59,86,62,215},topDown?.036f:.028f,false,topDown?4.f:0.f});

    if (pine)
    {
        // Interrupted branch tiers avoid a solid half-green triangular token.
        for(int tier=0;tier<4;++tier)
        {
            const float y=center.y+radiusY*(-.55f+tier*.36f);
            const float half=radiusX*(.16f+tier*.18f);
            ink.push_back({{{center.x-half,y+.04f},{center.x-.03f,y-.07f},
                            {center.x+half*.62f,y+.01f}},
                           {155,183,136,145},.025f,false,8.f});
        }
        return;
    }

    // Asymmetric crownlets use the same lighting as the outer crown. Their
    // radii fit in its central 70%, leaving the tested silhouette untouched.
    const int lobes=4+static_cast<int>((hash>>5)%3u);
    for(int lobe=0;lobe<lobes;++lobe)
    {
        const float angle=lobe*6.2831853f/lobes+static_cast<float>(hash%31u)*.11f;
        const float distance=.29f+static_cast<float>((hash>>(lobe*3))&3u)*.025f;
        const MapFeaturePoint c{center.x+std::cos(angle)*radiusX*distance,
                                center.y+std::sin(angle)*radiusY*distance};
        const float size=.25f+static_cast<float>((hash>>(lobe*4))&3u)*.016f;
        std::vector<MapFeaturePoint> ring;
        constexpr int crownletSides=8;
        for(int p=0;p<crownletSides;++p)
        {
            const float a=p*6.2831853f/crownletSides;
            const float r=size*(.91f+.09f*std::cos(a*5.f+lobe));
            ring.push_back({c.x+std::cos(a)*radiusX*r,c.y+std::sin(a)*radiusY*r});
        }
        const auto body=mix(middle,light,.28f+.11f*(lobe%3));
        for(std::size_t p=0;p<ring.size();++p)
        {
            AtlasInkPrimitive triangle{{c,ring[p],ring[(p+1)%ring.size()]},body,0.f,true,9.f};
            triangle.vertexColors={mix(body,light,.45f),body,body};
            ink.push_back(std::move(triangle));
        }
        ink.push_back({{ring[0],ring[1],ring[2],ring[3],ring[4]},
                       {64,101,69,140},.022f,false,10.f});
    }
    // A few leaf/branch breaks are readable when zoomed in, not visual noise
    // at fit-to-window scale. Stable positions prevent shimmer during a drag.
    for(int mark=0;mark<4;++mark)
    {
        const float a=(mark+.3f)*1.48f+static_cast<float>((hash>>8)%29u)*.1f;
        const MapFeaturePoint p{center.x+std::cos(a)*radiusX*.57f,
                                center.y+std::sin(a)*radiusY*.54f};
        ink.push_back({{{p.x-radiusX*.07f,p.y},{p.x,p.y-radiusY*.04f},
                        {p.x+radiusX*.08f,p.y+radiusY*.03f}},
                       {187,199,145,170},.024f,false,12.f});
    }
}
} // namespace gm::map_render
