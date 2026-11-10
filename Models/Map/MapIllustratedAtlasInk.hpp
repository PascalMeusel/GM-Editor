#pragma once

#include "Models/Map/MapIllustratedAtlasV2Ink.hpp"

namespace gm::map_render
{
inline std::vector<AtlasInkPrimitive> buildIllustratedAtlasInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &roads)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesIllustratedAtlas(map)) return ink;
    if (usesIllustratedAtlasDetailed(map)) return buildIllustratedAtlasV2Ink(map,roads);
    const OverviewRenderColor outline{73, 78, 61, 235}, mountainLight{230, 220, 184, 255},
        mountainShade{166, 166, 136, 255}, mountainDeep{131, 143, 120, 255};
    const auto triangle = [&](MapFeaturePoint a, MapFeaturePoint b, MapFeaturePoint c, OverviewRenderColor color) {
        ink.push_back({{a, b, c}, color, 0, true});
    };
    const auto stroke = [&](std::vector<MapFeaturePoint> points, OverviewRenderColor color, float width = .045f) {
        ink.push_back({std::move(points), color, width, false});
    };
    // Low, broken hill contours build a foothill zone without rectangular row
    // hatching. Wetlands and dunes have distinct marks in the same ink family.
    for (int y = 1; y < map.height - 1; ++y)
        for (int x = 1; x < map.width - 1; ++x)
        {
            const auto kind = overviewGroundKindForRender(map, x, y);
            const unsigned h = (x * 73856093u) ^ (y * 19349663u) ^ map.generation.recipe.seed;
            if (h % 7 != 0 || (kind != MapTileKind::Hill && kind != MapTileKind::Swamp && kind != MapTileKind::Desert)) continue;
            const float cx = x + .5f, cy = y + .5f;
            const std::vector<MapFeaturePoint> box{{cx-.65f,cy-.35f},{cx+.65f,cy-.35f},{cx+.65f,cy+.3f},{cx-.65f,cy+.3f}};
            if (!atlasSilhouetteClearsRoads(box, roads)) continue;
            if (kind == MapTileKind::Swamp)
            {
                stroke({{cx-.4f,cy+.15f},{cx+.4f,cy+.15f}}, {87,130,116,145}, .04f);
                stroke({{cx-.15f,cy+.1f},{cx-.18f,cy-.17f},{cx-.28f,cy-.24f}}, {87,130,116,165}, .04f);
                stroke({{cx+.08f,cy+.1f},{cx+.14f,cy-.24f}}, {87,130,116,165}, .04f);
            }
            else
                stroke({{cx-.6f,cy+.18f},{cx-.36f,cy-.02f},{cx-.05f,cy-.15f},{cx+.23f,cy-.1f},{cx+.52f,cy+.13f}},
                    kind == MapTileKind::Hill ? OverviewRenderColor{125,133,95,125} : OverviewRenderColor{163,141,91,140}, .04f);
        }
    for (const auto &glyph : buildOverviewForestGlyphs(map))
    {
        // Small overlapping groves, with stepped boughs rather than repeated
        // triangle markers. Back trees precede the foreground tree.
        for (int tree = 0; tree < 3; ++tree)
        {
            const float s = glyph.sizeCells * (tree == 2 ? .72f : .53f);
            const float x = glyph.center.x + (tree == 0 ? -.34f : tree == 1 ? .31f : .03f);
            const float y = glyph.center.y + (tree == 2 ? .24f : -.18f);
            const std::vector<MapFeaturePoint> footprint{{x-s*.48f,y-s*.95f},{x+s*.48f,y-s*.95f},{x+s*.48f,y+s*.5f},{x-s*.48f,y+s*.5f}};
            if (!atlasSilhouetteClearsRoads(footprint, roads)) continue;
            stroke({{x,y+s*.16f},{x,y+s*.50f}}, {82,99,70,210}, .045f);
            if (glyph.variation > .54f)
            {
                const MapFeaturePoint center{x,y-s*.22f};
                std::vector<MapFeaturePoint> crown;
                for (int i = 0; i < 12; ++i)
                {
                    const float angle = i*6.2831853f/12.f;
                    const float lobe = i%3 == 0 ? .91f : 1.f;
                    crown.push_back({center.x+std::cos(angle)*s*.45f*lobe,
                                     center.y+std::sin(angle)*s*.54f*lobe});
                }
                for (int i = 0; i < 12; ++i)
                    triangle(center,crown[i],crown[(i+1)%12],i < 3 || i > 8
                        ? OverviewRenderColor{133,160,108,255} : OverviewRenderColor{87,125,89,255});
                crown.push_back(crown.front());
                stroke(crown,{66,105,76,235},.037f);
                stroke({{x-s*.15f,y+s*.12f},{x,y+s*.27f},{x+s*.14f,y+s*.03f}}, {76,113,80,190},.03f);
                continue;
            }
            for (int bough = 2; bough >= 0; --bough)
            {
                const float tip = -.95f + bough*.29f, base = -.31f + bough*.31f;
                const float half = .27f + bough*.10f;
                const MapFeaturePoint top{x,y+s*tip}, left{x-s*half,y+s*base}, right{x+s*half,y+s*base};
                triangle(top,left,right,{65,105,80,255});
                triangle(top,{x,y+s*base},right,{129,156,105,255});
                stroke({left,top,right},{53,91,69,220},.033f);
            }
        }
    }
    for (const auto &ridge : buildOverviewRidgePaths(map))
        for (const auto &m : ridge.marks)
        {
            // Offset lower summits connect the high spine into a range. Each
            // silhouette has its own complete clearance test, so a pass can
            // trim a flank without removing an entire mountain component.
            const float baseWidth = m.rightBase.x - m.leftBase.x;
            for (int flank = 0; flank < 2; ++flank)
            {
                const float side = flank == 0 ? -1.f : 1.f;
                const float cx = (m.leftBase.x+m.rightBase.x)*.5f + side*baseWidth*.46f;
                const float by = m.leftBase.y + baseWidth*.10f;
                const float h = (m.leftBase.y-m.peak.y)*(.52f + flank*.09f);
                const MapFeaturePoint a{cx-baseWidth*.43f,by}, b{cx+side*.12f,by-h}, c{cx+baseWidth*.43f,by};
                const MapFeaturePoint mid{cx+side*.1f,by};
                const std::vector<MapFeaturePoint> flankOutline{a,b,c};
                bool flankLand = true;
                for (const auto p : flankOutline)
                    if (!isMapCellInside(map,static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y))) ||
                        !overviewGroundKindIsLand(overviewGroundKindForRender(map,static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y))))) flankLand = false;
                if (!flankLand || !atlasSilhouetteClearsRoads(flankOutline,roads)) continue;
                triangle(a,b,mid,{172,174,141,255});
                triangle(b,c,mid,{222,212,172,255});
                stroke(flankOutline,{108,121,94,205},.047f);
                stroke({b,{cx-side*.1f,by-h*.43f},mid},{124,140,109,175},.035f);
            }
            const std::vector<MapFeaturePoint> silhouette{m.leftBase,m.leftShoulder,m.peak,m.rightShoulder,m.rightBase};
            // Reject the entire mark, not just its anchor. A road therefore
            // remains an open pass, including short perpendicular crossings.
            if (!atlasSilhouetteClearsRoads(silhouette, roads)) continue;
            bool land = true;
            for (const auto p : silhouette)
                if (!isMapCellInside(map, static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))) ||
                    !overviewGroundKindIsLand(overviewGroundKindForRender(map, static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))))) land = false;
            if (!land) continue;
            triangle(m.leftBase,m.leftShoulder,m.shadeEnd,mountainShade);
            triangle(m.leftShoulder,m.peak,m.shadeEnd,mountainDeep);
            triangle(m.peak,m.rightShoulder,m.shadeEnd,mountainLight);
            triangle(m.rightShoulder,m.rightBase,m.shadeEnd,mountainLight);
            stroke(silhouette, outline, .06f);
            const MapFeaturePoint fold{m.peak.x*.55f+m.shadeEnd.x*.45f-.12f,m.peak.y*.55f+m.shadeEnd.y*.45f};
            stroke({m.peak,fold,{m.shadeEnd.x+.09f,m.shadeEnd.y-.18f},m.shadeEnd}, {104,121,101,220}, .045f);
            const auto mix = [](MapFeaturePoint a, MapFeaturePoint b, float t) { return MapFeaturePoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; };
            for (float t : {.38f,.60f,.79f})
                stroke({mix(m.leftShoulder,m.peak,t),mix(m.leftBase,m.shadeEnd,t*.88f)}, {104,122,102,125}, .027f);
            const auto capL = mix(m.peak,m.leftShoulder,.27f), capR = mix(m.peak,m.rightShoulder,.29f);
            triangle(m.peak,capL,capR,{247,239,211,255});
        }
    return ink;
}
// Resolved symbols use the same cell-space ink contract as terrain. The plan
// supplies an audience-filtered map and explicit category visibility.
} // namespace gm::map_render
