#pragma once

#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/MapFeaturePresentationGeometry.hpp"
#include "Models/Map/MapIllustrationPigment.hpp"

// Versioned, renderer-independent atlas artwork. Coordinates and stroke widths
// are in map cells, so editor, detached display and export share silhouettes.
// This style does not replace or repin an existing production asset snapshot.
namespace gm::map_render
{
inline constexpr const char *kIllustratedAtlasStyleId = "illustrated_atlas";
inline constexpr const char *kIllustratedAtlasSnapshotId = "illustrated_atlas_v1";
inline constexpr const char *kIllustratedAtlasV2SnapshotId = "illustrated_atlas_v2";

inline bool usesIllustratedAtlasV5(const MapModel &map)
{
    return overviewUsesAtlasTerrain(map) && map.generation.recipe.styleId == kIllustratedAtlasStyleId &&
           ((map.generation.recipe.styleVersion == 5 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v5") ||
            (map.generation.recipe.styleVersion == 6 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6"));
}
inline bool usesIllustratedAtlasV4(const MapModel &map)
{
    return overviewUsesAtlasTerrain(map) && map.generation.recipe.styleId == kIllustratedAtlasStyleId &&
           map.generation.recipe.styleVersion == 4 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v4";
}
inline bool usesIllustratedAtlasV3(const MapModel &map)
{
    return overviewUsesAtlasTerrain(map) && map.generation.recipe.styleId == kIllustratedAtlasStyleId &&
           map.generation.recipe.styleVersion == 3 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v3";
}
inline bool usesIllustratedAtlasV2(const MapModel &map)
{
    return overviewUsesAtlasTerrain(map) && map.generation.recipe.styleId == kIllustratedAtlasStyleId &&
           map.generation.recipe.styleVersion == 2 &&
           map.generation.recipe.styleSnapshotId == kIllustratedAtlasV2SnapshotId;
}

inline bool usesIllustratedAtlasDetailed(const MapModel &map)
{
    return usesIllustratedAtlasV2(map) || usesIllustratedAtlasV3(map) || (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map));
}

inline bool usesIllustratedAtlas(const MapModel &map)
{
    return overviewUsesAtlasTerrain(map) &&
           map.generation.recipe.styleId == kIllustratedAtlasStyleId &&
           ((map.generation.recipe.styleVersion == 1 &&
             map.generation.recipe.styleSnapshotId == kIllustratedAtlasSnapshotId) || usesIllustratedAtlasDetailed(map));
}

// Shared cell-space preference for label alternatives. A label may still
// cross terrain when all alternatives are occupied; this is a ranking, never
// permission to move a label into unrevealed player cells.
inline std::array<float,4> illustratedAtlasSymbolLabelBounds(MapFeaturePoint anchor,bool settlement)
{
    // Candidate clamping can move a side label back onto its own symbol.
    // The castle rises above its anchor; the rosette is almost symmetric.
    const float halfWidth=settlement?.80f:.62f;
    const float above=settlement?1.38f:.70f,below=settlement?.38f:.66f;
    return {anchor.x-halfWidth,anchor.y-above,anchor.x+halfWidth,anchor.y+below};
}
inline bool illustratedAtlasLabelClearsOwnSymbol(MapFeaturePoint anchor, float left, float top,
    float right, float bottom, bool settlement)
{
    const auto box=illustratedAtlasSymbolLabelBounds(anchor,settlement);
    return right<=box[0] || left>=box[2] || bottom<=box[1] || top>=box[3];
}

float illustratedAtlasLabelTerrainCost(const MapModel &map, float left, float top, float right, float bottom);

struct AtlasLabelInkMask {
    int width=0,height=0;
    float samplesPerCell=4.f;
    std::vector<unsigned char> covered;
};
float illustratedAtlasLabelInkCost(const AtlasLabelInkMask &mask,float left,float top,float right,float bottom);

inline float illustratedAtlasLabelPlacementCost(const MapModel &map, MapFeaturePoint anchor,
                                                float left, float top, float right, float bottom,
    const std::vector<gm::map_geometry::MapRoadCorridor> *cachedCorridors = nullptr,
    const AtlasLabelInkMask *inkMask = nullptr)
{
    const MapFeaturePoint center{(left+right)*.5f,(top+bottom)*.5f};
    float cost=illustratedAtlasLabelTerrainCost(map,left,top,right,bottom)*.10f+
               std::hypot(center.x-anchor.x,center.y-anchor.y)*1.4f;
    if(map.generation.recipe.recipeVersion>=5 && inkMask)
        cost+=illustratedAtlasLabelInkCost(*inkMask,left,top,right,bottom);
    // Crossing a waterway or another route makes a displaced name appear to
    // belong to the far bank. Keep the relationship to its symbol dominant.
    const float rx=center.x-anchor.x,ry=center.y-anchor.y;
    const auto ownedCorridors = map.generation.recipe.recipeVersion>=4 && !cachedCorridors
        ? mapPresentationCorridors(map) : std::vector<gm::map_geometry::MapRoadCorridor>{};
    const auto &corridors=cachedCorridors?*cachedCorridors:ownedCorridors;
    for (const auto &feature : map.generation.graph.features)
    {
        if (feature.kind!=MapGeneratedFeatureKind::River && feature.kind!=MapGeneratedFeatureKind::Road &&
            feature.kind!=MapGeneratedFeatureKind::Trail) continue;
        if (map.generation.recipe.recipeVersion >= 4)
        {
            const auto found=std::find_if(corridors.begin(),corridors.end(),[&](const auto &c){return c.featureId==feature.id;});
            if(found==corridors.end()) continue; // A hidden way cannot move a public label.
            const auto &corridor=*found;
            const std::array<MapFeaturePoint,4> box{{{left,top},{right,top},{right,bottom},{left,bottom}}};
            bool overlap=std::any_of(corridor.centerline.begin(),corridor.centerline.end(),[&](auto p){
                return p.x>=left && p.x<=right && p.y>=top && p.y<=bottom;
            });
            for(std::size_t edge=0;edge<box.size() && !overlap;++edge)
                overlap=gm::map_geometry::roadCorridorIntersectsSegment(corridor,box[edge],box[(edge+1)%4],.05f);
            if(overlap) cost+=feature.kind==MapGeneratedFeatureKind::River?24.f:18.f;
        }
        for (std::size_t i=1;i<feature.points.size();++i)
        {
            const auto a=feature.points[i-1],b=feature.points[i];
            const float sx=b.x-a.x,sy=b.y-a.y,denominator=rx*sy-ry*sx;
            if (std::abs(denominator)<.000001f) continue;
            const float t=((a.x-anchor.x)*sy-(a.y-anchor.y)*sx)/denominator;
            const float u=((a.x-anchor.x)*ry-(a.y-anchor.y)*rx)/denominator;
            if (t>.15f && t<.99f && u>=0.f && u<=1.f)
            { cost+=feature.kind==MapGeneratedFeatureKind::River?8.f:3.f; break; }
        }
    }
    return cost;
}

inline std::vector<MapFeaturePoint> illustratedAtlasLabelLeader(MapFeaturePoint anchor,
    float left,float top,float right,float bottom)
{
    const MapFeaturePoint nearest{std::clamp(anchor.x,left,right),std::clamp(anchor.y,top,bottom)};
    const float dx=nearest.x-anchor.x,dy=nearest.y-anchor.y,length=std::hypot(dx,dy);
    const bool conventionalAbove=anchor.x>=left && anchor.x<=right && bottom<anchor.y;
    if (length<.9f || (conventionalAbove && length<2.2f)) return {};
    const float start=std::min(length*.72f,dy<0.f?.95f:.65f);
    return {{anchor.x+dx/length*start,anchor.y+dy/length*start},nearest};
}

inline const MapRenderScaleProfile &atlasPresentationProfile(const MapModel &map)
{
    return mapRenderScaleProfile(usesIllustratedAtlas(map) ? MapLevel::World : map.atlasLevel);
}

inline OverviewRenderColor illustratedAtlasGroundColor(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Water: case MapTileKind::Ice: return {98, 151, 161, 255};
    case MapTileKind::Forest: return {164, 183, 128, 255};
    case MapTileKind::Mountain: case MapTileKind::Stone: return {203, 191, 153, 255};
    case MapTileKind::Hill: return {211, 199, 154, 255};
    case MapTileKind::Swamp: return {165, 186, 155, 255};
    case MapTileKind::Desert: return {231, 208, 154, 255};
    case MapTileKind::Snow: return {236, 234, 210, 255};
    case MapTileKind::Dirt: return {216, 196, 156, 255};
    default: return {225, 218, 178, 255};
    }
}

// Original broad generated relief, before roads and settlements flatten their
// gameplay cells. Sampling those modified heights would disclose private ways
// through the public terrain wash. Keep this small sampler independent of the
// generator and sample in full-map coordinates in every backend.
inline float illustratedAtlasElevation(const MapModel &map, float x, float y)
{
    const auto noise = [](unsigned seed,float x,float y,int scale) {
        x /= static_cast<float>(std::max(1,scale)); y /= static_cast<float>(std::max(1,scale));
        const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
        x -= ix; y -= iy; x = x*x*(3.f-2.f*x); y = y*y*(3.f-2.f*y);
        const auto lattice = [seed](int x,int y) {
            unsigned h = seed ^ static_cast<unsigned>(x)*0x9e3779b9u ^ static_cast<unsigned>(y)*0x85ebca6bu;
            h ^= h>>16; h *= 0x7feb352du; h ^= h>>15; h *= 0x846ca68bu; h ^= h>>16;
            return static_cast<float>(h&0x00ffffffu)/static_cast<float>(0x01000000u);
        };
        const float a=lattice(ix,iy), b=lattice(ix+1,iy), c=lattice(ix,iy+1), d=lattice(ix+1,iy+1);
        const float top=a+(b-a)*x, bottom=c+(d-c)*x;
        return top+(bottom-top)*y;
    };
    const unsigned seed=map.generation.recipe.seed+17u;
    const int scale=std::max(8,std::min(map.width,map.height)/2);
    const float offset = map.generation.recipe.landscape == MapLandscapeProfile::MountainPass ||
                         map.generation.recipe.landscape == MapLandscapeProfile::Highlands ? .12f : 0.f;
    return noise(seed,x,y,scale)*.56f+noise(seed+1013u,x,y,std::max(2,scale/2))*.29f+
           noise(seed+7919u,x,y,std::max(2,scale/4))*.15f+offset+
           (1.f-y/std::max(1.f,static_cast<float>(map.height-1)))*.18f;
}

inline OverviewRenderColor illustratedAtlasTerrainColor(
    const MapModel &map, float x, float y,
    const std::vector<MapTileKind> *projection = nullptr)
{
    const auto at = [&](int sx, int sy) {
        sx = std::clamp(sx, 0, map.width - 1);
        sy = std::clamp(sy, 0, map.height - 1);
        return projection ? overviewGroundKindFromProjection(map, *projection, sx, sy)
                          : overviewGroundKindForRender(map, sx, sy);
    };
    const auto sample = overviewOrganicTerrainSample(map, x, y);
    const bool revised = usesIllustratedAtlasDetailed(map);
    const bool fantasy = (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map));
    const int cx = static_cast<int>(std::floor(sample.x));
    const int cy = static_cast<int>(std::floor(sample.y));
    float land = 0, total = 0, weight = 0, r = 0, g = 0, b = 0;
    for (int sy = cy - 2; sy <= cy + 2; ++sy)
        for (int sx = cx - 2; sx <= cx + 2; ++sx)
        {
            const float dx = sx + .5f - sample.x, dy = sy + .5f - sample.y;
            const float domain = detail::smoothOverviewAxisWeight(dx) *
                                 detail::smoothOverviewAxisWeight(dy);
            const auto kind = at(sx, sy);
            total += domain;
            if (!overviewGroundKindIsLand(kind)) continue;
            land += domain;
            const float w = revised ? domain : std::max(0.f, 1.f - std::abs(dx) / 1.05f) *
                            std::max(0.f, 1.f - std::abs(dy) / 1.05f);
            auto c = illustratedAtlasGroundColor(kind);
            if (revised)
            {
                switch (kind)
                {
                case MapTileKind::Forest: c = {183,195,151,255}; break;
                case MapTileKind::Mountain: case MapTileKind::Stone: c = {219,210,178,255}; break;
                case MapTileKind::Hill: c = {221,214,179,255}; break;
                default: break;
                }
            }
            if (fantasy)
            {
                switch (kind)
                {
                case MapTileKind::Forest: c = {170,194,144,255}; break;
                case MapTileKind::Swamp: c = {153,182,155,255}; break;
                case MapTileKind::Mountain: case MapTileKind::Stone: c = {229,216,187,255}; break;
                case MapTileKind::Hill: c = {231,221,183,255}; break;
                case MapTileKind::Desert: c = {237,208,151,255}; break;
                case MapTileKind::Dirt: c = {226,204,161,255}; break;
                case MapTileKind::Snow: c = {242,237,219,255}; break;
                default: c = {239,227,187,255}; break;
                }
            }
            r += c.r * w; g += c.g * w; b += c.b * w; weight += w;
        }
    const float coverage = overviewCartographicLandCoverage(map, sample.x, sample.y,
        total > .0001f ? land / total : 0.f);
    const auto paper = fantasy ? OverviewRenderColor{239,227,187,255} : illustratedAtlasGroundColor(MapTileKind::Empty);
    const auto sea = fantasy ? OverviewRenderColor{62,129,141,255} : illustratedAtlasGroundColor(MapTileKind::Water);
    if (weight > .0001f) { r /= weight; g /= weight; b /= weight; }
    else { r = paper.r; g = paper.g; b = paper.b; }
    if (revised)
    {
        const float relief = illustratedAtlasElevation(map,x,y);
        const float upland = std::clamp((relief-.44f)/.40f,0.f,1.f);
        const float light = std::clamp((illustratedAtlasElevation(map,x+.7f,y-.7f)-
                                       illustratedAtlasElevation(map,x-.7f,y+.7f))*95.f,-6.f,6.f);
        r += -11.f+upland*12.f+light;
        g += 2.f-upland*16.f+light;
        b += -5.f-upland*8.f+light;
    }
    // A narrow light shelf defines water depth; restrained paper tone avoids
    // the blurred low-contrast texture of the former photographic paintings.
    const float shelf = (1.f - coverage) * std::clamp((land / std::max(total, .0001f) - .16f) * 1.6f, 0.f, .28f);
    float pigment = map.generation.recipe.recipeVersion >= 10
        ? illustrationPigmentWash(map.generation.recipe.seed,x,y) : 0.f;
    if (map.generation.recipe.recipeVersion >= 11) {
        pigment = pigment * 1.65f + illustrationPigmentNoise(map.generation.recipe.seed^0x4137u,x*5.3f,y*5.3f)*1.7f;
        r += 3.f; g -= 1.5f; b -= 2.f;
    }
    const auto channel = [&](float water, float ground) {
        return static_cast<std::uint8_t>(std::clamp(std::lround(
            water + (ground - water) * coverage + shelf * 28.f + pigment), 0l, 255l));
    };
    return {channel(sea.r, r), channel(sea.g, g), channel(sea.b, b), 255};
}

struct AtlasInkPrimitive
{
    std::vector<MapFeaturePoint> points;
    OverviewRenderColor color;
    float strokeCells = .045f;
    bool filled = false; // Filled primitives are triangles; strokes are open.
    float minimumCellPixels = 0.f;
    float maximumCellPixels = 0.f; // Zero means no upper limit.
    // Optional triangle vertex colors. Shared mesh edges use identical
    // positions/colors; renderers interpolate once without internal AA.
    std::vector<OverviewRenderColor> vertexColors;
};

AtlasLabelInkMask buildAtlasLabelInkMask(const MapModel &map,const std::vector<AtlasInkPrimitive> &ink);

} // namespace gm::map_render
