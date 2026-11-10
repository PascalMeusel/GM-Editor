#include "Models/Map/MapUndergroundMaterialInk.hpp"
#include "Models/Map/MapIllustrationPigment.hpp"
#include <array>

namespace gm::map_render
{
namespace
{
OverviewRenderColor shifted(OverviewRenderColor base, float value)
{
    const auto channel = [&](int c, float weight) {
        return static_cast<std::uint8_t>(std::clamp(int(std::lround(c + value * weight)), 0, 255));
    };
    return {channel(base.r, 1.f), channel(base.g, .92f), channel(base.b, .72f), base.a};
}

MapFeaturePoint mix(MapFeaturePoint a, MapFeaturePoint b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

void floorTriangle(std::vector<AtlasInkPrimitive> &ink, std::array<MapFeaturePoint, 3> p,
                   OverviewRenderColor base, unsigned seed, int depth = 0)
{
    int longest = 0;
    float squaredLength = 0.f;
    for (int i = 0; i < 3; ++i)
    {
        const auto a = p[i], b = p[(i + 1) % 3];
        const float d = (b.x-a.x)*(b.x-a.x) + (b.y-a.y)*(b.y-a.y);
        if (d > squaredLength) { longest = i; squaredLength = d; }
    }
    // Subdivide only existing floor triangles. Shared midpoints remain on the
    // exact contour, including concave rooms and enclosed solid islands.
    if (squaredLength > 6.25f && depth < 12)
    {
        const auto a = p[longest], b = p[(longest+1)%3], c = p[(longest+2)%3];
        const auto middle = mix(a, b, .5f);
        floorTriangle(ink, {a,middle,c}, base, seed, depth+1);
        floorTriangle(ink, {middle,b,c}, base, seed, depth+1);
        return;
    }
    AtlasInkPrimitive piece{{p[0],p[1],p[2]},base,0.f,true};
    for (const auto point : p)
        piece.vertexColors.push_back(shifted(base,
            illustrationPigmentWash(seed,point.x,point.y)*2.2f +
            illustrationPigmentNoise(seed^0xb421u,point.x*.11f,point.y*.11f)*4.f));
    ink.push_back(std::move(piece));
}
}

void shadeCaveFloorMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                            OverviewRenderColor floor)
{
    std::vector<AtlasInkPrimitive> shaded;
    shaded.reserve(ink.size()*2);
    for (auto &piece : ink)
    {
        if (piece.filled && piece.points.size()==3 && piece.color.r==floor.r &&
            piece.color.g==floor.g && piece.color.b==floor.b && piece.color.a==floor.a)
            floorTriangle(shaded,{piece.points[0],piece.points[1],piece.points[2]},floor,map.generation.recipe.seed);
        else
            shaded.push_back(std::move(piece));
    }
    ink=std::move(shaded);
}

void appendDungeonSlabMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                              int x, int y, OverviewRenderColor floor)
{
    const float l=float(x),t=float(y);
    floorTriangle(ink,{{{l,t},{l+1,t},{l+1,t+1}}},floor,map.generation.recipe.seed);
    floorTriangle(ink,{{{l,t},{l+1,t+1},{l,t+1}}},floor,map.generation.recipe.seed);
    // Fine worn edges belong to each two-cell, staggered slab. They sit on
    // the floor and never resemble a raised obstacle or movement boundary.
    if (y%2==0)
        ink.push_back({{{l+.045f,t+.065f},{l+.96f,t+.065f}},{225,212,173,100},.027f,false,12.f});
    if ((x+(y/2)%2)%2==0)
        ink.push_back({{{l+.065f,t+.045f},{l+.065f,t+.96f}},{225,212,173,90},.025f,false,12.f});
}

void appendUndergroundWallMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                                   MapFeaturePoint outerA, MapFeaturePoint outerB,
                                   MapFeaturePoint innerA, MapFeaturePoint innerB, bool cave)
{
    const auto color = [&](MapFeaturePoint p, bool inner) {
        const OverviewRenderColor base = cave ? (inner?OverviewRenderColor{168,161,130,255}:OverviewRenderColor{75,91,79,255})
                                              : (inner?OverviewRenderColor{174,166,134,255}:OverviewRenderColor{111,118,98,255});
        return shifted(base,illustrationPigmentWash(map.generation.recipe.seed,p.x,p.y)*1.4f);
    };
    AtlasInkPrimitive a{{outerA,outerB,innerA},{130,133,108,255},0.f,true};
    a.vertexColors={color(outerA,false),color(outerB,false),color(innerA,true)};
    AtlasInkPrimitive b{{innerA,outerB,innerB},a.color,0.f,true};
    b.vertexColors={color(innerA,true),color(outerB,false),color(innerB,true)};
    ink.push_back(std::move(a)); ink.push_back(std::move(b));
    if (cave)
    {
        // Mineral bands follow the existing rock face, not a second outline.
        for (float t : {.36f,.69f})
            ink.push_back({{mix(outerA,innerA,t),mix(outerB,innerB,t)},
                           {188,178,138,80},.025f,false,12.f});
    }
}
}
