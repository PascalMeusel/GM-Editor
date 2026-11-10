#pragma once

#include "Models/Map/MapAtlasRiverInk.hpp"

namespace gm::map_render
{
inline std::vector<AtlasInkPrimitive> buildIllustratedAtlasRiverMouthInk(
    const MapModel &map,const MapGeneratedFeature &feature)
{
    return buildIllustratedAtlasRiverInk(map,feature,true);
}

inline bool usesTacticalRiverDetail(const MapModel &map)
{
    return (map.atlasLevel == MapLevel::Tactical && map.generation.recipe.generatorVersion >= 8) ||
           (map.creationIntent == MapCreationIntent::TravelScene && map.generation.recipe.generatorVersion >= 11);
}

inline std::vector<MapFeaturePoint> tacticalRiverDetailPoints(const MapModel &map, const MapGeneratedFeature &feature)
{
    auto points = overviewCartographicCurvePoints(map,feature);
    if (points.size()<2 || (points.size()<3 && map.generation.recipe.generatorVersion<9)) return points;
    for (int pass=0;pass<3;++pass)
    {
        std::vector<MapFeaturePoint> next{points.front()};
        for (std::size_t i=1;i<points.size();++i)
        {
            const auto a=points[i-1],b=points[i];
            next.push_back({a.x*.75f+b.x*.25f,a.y*.75f+b.y*.25f});
            next.push_back({a.x*.25f+b.x*.75f,a.y*.25f+b.y*.75f});
        }
        next.push_back(points.back()); points.swap(next);
    }
    if (map.generation.recipe.generatorVersion >= 9)
    {
        // A channel whose authored endpoint reaches an edge continues out of
        // the viewport. Extend only its display ribbon, retaining the saved
        // endpoint and leaving inland sources/mouths exactly where authored.
        const auto continuation = [&](MapFeaturePoint p, MapFeaturePoint inner) {
            const float margin=std::max(1.f,feature.width)+.5f;
            float dx=p.x-inner.x,dy=p.y-inner.y;
            const float length=std::hypot(dx,dy);
            if (length<.00001f) return p;
            dx/=length; dy/=length;
            if (p.y<=.501f && dy<-.1f) { const float t=(-margin-p.y)/dy; return MapFeaturePoint{p.x+dx*t,-margin}; }
            if (p.y>=map.height-.501f && dy>.1f) { const float t=(map.height+margin-p.y)/dy; return MapFeaturePoint{p.x+dx*t,map.height+margin}; }
            if (p.x<=.501f && dx<-.1f) { const float t=(-margin-p.x)/dx; return MapFeaturePoint{-margin,p.y+dy*t}; }
            if (p.x>=map.width-.501f && dx>.1f) { const float t=(map.width+margin-p.x)/dx; return MapFeaturePoint{map.width+margin,p.y+dy*t}; }
            return p;
        };
        const auto first=continuation(points.front(),points[1]);
        const auto last=continuation(points.back(),points[points.size()-2]);
        if (std::hypot(first.x-points.front().x,first.y-points.front().y)>.001f) points.insert(points.begin(),first);
        if (std::hypot(last.x-points.back().x,last.y-points.back().y)>.001f) points.push_back(last);
    }
    return points;
}

// All water marks stay within the resolved river ribbon. Distances and
// placement use full-map coordinates, so crops and zoom cannot reroll them.
inline std::vector<AtlasInkPrimitive> buildTacticalRiverDetailInk(const std::vector<MapFeaturePoint> &points, float width, bool fantasy = false)
{
    std::vector<AtlasInkPrimitive> ink;
    if (points.size()<2 || width<=0.f) return ink;
    if (fantasy)
    {
        for (int band=0;band<8;++band)
            ink.push_back({points,{static_cast<std::uint8_t>(52-band),static_cast<std::uint8_t>(113-band*2),
                                  static_cast<std::uint8_t>(121-band),255},width*(.94f-band*.085f),false});
        float traversed=0.f,next=.8f; unsigned mark=0;
        for (std::size_t i=1;i<points.size();++i)
        {
            const auto a=points[i-1],b=points[i];
            const float length=std::hypot(b.x-a.x,b.y-a.y);
            if (length<.00001f) continue;
            const float tx=(b.x-a.x)/length,ty=(b.y-a.y)/length;
            while (next<traversed+length)
            {
                unsigned h=(++mark)*0x9e3779b9u; h^=h>>16; h*=0x7feb352du; h^=h>>15;
                const float t=(next-traversed)/length;
                const float side=(static_cast<float>(h%101)/100.f-.5f)*width*.53f;
                const MapFeaturePoint p{a.x+(b.x-a.x)*t-ty*side,a.y+(b.y-a.y)*t+tx*side};
                const float extent=.13f+((h>>8)%100)*.0016f;
                ink.push_back({{{p.x-tx*extent,p.y-ty*extent},{p.x-ty*.055f,p.y+tx*.055f},
                               {p.x+tx*extent-ty*.02f,p.y+ty*extent+tx*.02f}},
                               {81,137,144,255},.022f,false});
                next+=.74f+((h>>16)%100)*.008f;
            }
            traversed+=length;
        }
        return ink;
    }
    ink.push_back({points,{41,91,104,255},width*.64f,false});
    ink.push_back({points,{44,96,108,255},width*.38f,false});
    float traversed=0.f, nextMark=.7f;
    int mark=0;
    for (std::size_t i=1;i<points.size();++i)
    {
        const auto a=points[i-1],b=points[i];
        const float length=std::hypot(b.x-a.x,b.y-a.y);
        if (length<.00001f) continue;
        const float tx=(b.x-a.x)/length,ty=(b.y-a.y)/length;
        while (nextMark<traversed+length)
        {
            const float t=(nextMark-traversed)/length;
            const float side=(mark%3-1)*width*.23f;
            const MapFeaturePoint p{a.x+(b.x-a.x)*t-ty*side,a.y+(b.y-a.y)*t+tx*side};
            const float extent=.13f+(mark%4)*.035f;
            ink.push_back({{{p.x-tx*extent,p.y-ty*extent},{p.x,p.y},{p.x+tx*extent-ty*.025f,p.y+ty*extent+tx*.025f}},
                {80,135,143,255},.027f,false});
            nextMark+=.69f+(mark%3)*.19f; ++mark;
        }
        traversed+=length;
    }
    return ink;
}

// Decorative ink has no depth, distance or name semantics. Resolve it once
// from the reviewed full-map ground projection, never from a cropped raster.
} // namespace gm::map_render
