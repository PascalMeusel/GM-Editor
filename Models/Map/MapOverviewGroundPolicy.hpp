#pragma once

#include "Models/Map/MapOverviewRenderTypes.hpp"

namespace gm::map_render
{
namespace detail
{
struct MountainCell
{
    int x = 0;
    int y = 0;
    float along = 0.0f;
};

inline bool isMountainCell(const MapModel &map, int x, int y)
{
    return isMapCellInside(map, x, y) &&
           mapVisualGroundKind(map, x, y) == MapTileKind::Mountain;
}

inline bool isOverviewLandKind(MapTileKind kind)
{
    // River cells use inferred visual ground and therefore stay land here;
    // only actual water bodies form a coastline. Ice is treated as water at
    // continental scale so a frozen sea does not become a false landmass.
    return kind != MapTileKind::Water && kind != MapTileKind::Ice;
}

inline float smoothOverviewAxisWeight(float distance)
{
    const float normalized = std::clamp(
        1.0f - std::abs(distance) / kOverviewCoastBlendRadiusCells,
        0.0f, 1.0f);
    return normalized * normalized * (3.0f - 2.0f * normalized);
}

inline float overviewLandCoverageFromMaskCached(const MapModel &map,
                                          const std::vector<std::uint8_t> &landMask,
                                          float mapX,
                                          float mapY, bool sharpen,
                                          const OverviewGroundSamplingCache *cache)
{
    if (map.width <= 0 || map.height <= 0 ||
        landMask.size() < static_cast<std::size_t>(map.width) *
                              static_cast<std::size_t>(map.height))
        return 0.0f;
    const OverviewSamplePoint sample = overviewOrganicTerrainSampleCached(map, mapX, mapY, cache);
    const float sampleX = sample.x;
    const float sampleY = sample.y;
    const int centerX = static_cast<int>(std::floor(sampleX));
    const int centerY = static_cast<int>(std::floor(sampleY));
    float landWeight = 0.0f;
    float totalWeight = 0.0f;
    for (int y = centerY - 2; y <= centerY + 2; ++y)
    {
        for (int x = centerX - 2; x <= centerX + 2; ++x)
        {
            if (!isMapCellInside(map, x, y))
                continue;
            const float weight = smoothOverviewAxisWeight(
                                     static_cast<float>(x) + 0.5f - sampleX) *
                                 smoothOverviewAxisWeight(
                                     static_cast<float>(y) + 0.5f - sampleY);
            if (weight <= 0.0001f)
                continue;
            totalWeight += weight;
            if (landMask[static_cast<std::size_t>(mapCellIndex(map, x, y))] != 0u)
                landWeight += weight;
        }
    }
    // The outline and filled land must use the identical displaced field.
    // Previously the contour traced the raw mask while the fill perturbed it
    // a second time, leaving a second shoreline visibly offshore.
    return totalWeight > 0.0001f
               ? overviewCartographicLandCoverageCached(map, sampleX, sampleY,
                                                   landWeight / totalWeight, sharpen, cache)
               : 0.0f;
}

inline float overviewLandCoverageFromMask(const MapModel &map,
                                          const std::vector<std::uint8_t> &landMask,
                                          float mapX,
                                          float mapY, bool sharpen = true)
{
    return overviewLandCoverageFromMaskCached(map, landMask, mapX, mapY, sharpen, nullptr);
}
} // namespace detail

inline bool overviewGroundKindIsLand(MapTileKind kind)
{
    return detail::isOverviewLandKind(kind);
}

inline bool overviewUsesAtlasTerrain(const MapModel &map)
{
    return map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
}

inline bool overviewUsesCurveProfileAccents(const MapModel &)
{
    // Current profile images are isolated oval accents. Their edges form
    // false tributaries beside the continuous water stroke at every scale.
    return false;
}

inline bool usesIntegratedTacticalRoadInk(const MapModel &map)
{
    return map.atlasLevel == MapLevel::Tactical && map.generation.recipe.generatorVersion >= 7;
}

inline bool usesTacticalSoftTerrainBlend(const MapModel &map)
{
    return map.atlasLevel == MapLevel::Tactical && map.scope != MapScope::Dungeon &&
           map.generation.recipe.generatorVersion >= 9;
}

inline float tacticalTerrainBlendWeight(float dx, float dy)
{
    // Radial support avoids axis-aligned ledges in diagonal dirt shoulders.
    // This is a display reconstruction; collision/cover cells stay exact.
    const float weight=std::max(0.f,1.f-(dx*dx+dy*dy)/2.56f);
    return weight*weight*weight;
}

inline std::uint64_t overviewTerrainContentHash(const MapModel &map)
{
    std::uint64_t hash = 14695981039346656037ull;
    const auto add = [&](std::uint64_t value) { hash ^= value; hash *= 1099511628211ull; };
    add(map.width); add(map.height); add(static_cast<int>(map.atlasLevel));
    add(static_cast<int>(map.scope)); add(map.generation.recipe.seed);
    for (const unsigned char c : map.generation.recipe.styleId) add(c);
    for (const unsigned char c : map.generation.recipe.styleSnapshotId) add(c);
    add(map.generation.recipe.styleVersion);
    if (map.generation.recipe.styleId == "illustrated_atlas" &&
        ((map.generation.recipe.styleVersion == 2 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v2") ||
         (map.generation.recipe.styleVersion == 3 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v3") ||
         ((map.generation.recipe.styleVersion == 4 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v4") ||
          (((map.generation.recipe.styleVersion == 5 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v5") || (map.generation.recipe.styleVersion == 6 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6"))))))
        add(static_cast<int>(map.generation.recipe.landscape));
    add(map.cells.size());
    for (const auto &cell : map.cells)
    {
        add(static_cast<int>(cell.kind));
        add(static_cast<int>(cell.wayKind));
    }
    // Manual river geometry can preserve standing water at diagonal shore
    // corners even when a control-point/width edit leaves every cell equal.
    // Include only the geometry used by that terrain rule; labels and other
    // generated artwork still do not invalidate the terrain cache.
    for(const auto &feature:map.generation.graph.features)
    {
        if(feature.kind!=MapGeneratedFeatureKind::River || feature.id.rfind("manual_stroke_",0)!=0) continue;
        add(0x72697665725f756cull);add(feature.id.size());
        for(const unsigned char c:feature.id)add(c);
        const auto addFloat=[&](float value) {std::uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));add(bits);};
        addFloat(feature.width);add(feature.points.size());
        for(const auto p:feature.points){addFloat(p.x);addFloat(p.y);}
    }
    return hash;
}

// The semantic raster stores river occupancy as Water for movement and
// hydrology. On a World rendering the smooth feature-graph curve is the visible
// river; painting those coarse projection cells as lakes creates the blue
// zig-zag loops seen beside the curve. Recover a stable nearby land biome for
// the terrain underneath while leaving actual seas and lakes untouched.
inline MapTileKind overviewGroundKindForRender(const MapModel &map, int x, int y)
{
    const MapTileKind visualKind = mapVisualGroundKind(map, x, y);
    if (!overviewUsesAtlasTerrain(map))
        return visualKind;
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell)
        return visualKind;
    bool projectedRiverWater = (map.generation.recipe.generatorVersion >= 8 && cell->kind == MapTileKind::Bridge) ||
                               cell->wayKind == MapWayKind::River ||
                               cell->wayKind == MapWayKind::Canal;
    // Diagonal river rasterization fills the two orthogonal corner cells with
    // Water/None. Rendering these helpers beside the authoritative spline
    // creates cyan diamonds. Detect that exact topology instead of masking a
    // broad river neighborhood, which would turn real sea at a mouth to land.
    if (!projectedRiverWater && visualKind == MapTileKind::Water)
    {
        // Manual strokes preserve standing-water underlay. Water/None beside
        // those strokes is real sea/lake, not the generator's diagonal helper
        // raster; inferring land here would still grow small coastal wedges.
        for(const auto &feature:map.generation.graph.features)
        {
            if(feature.kind!=MapGeneratedFeatureKind::River || feature.id.rfind("manual_stroke_",0)!=0) continue;
            const float radius=std::max(1.f,feature.width*.5f)+.75f;
            for(std::size_t i=1;i<feature.points.size();++i)
            {
                const auto a=feature.points[i-1],b=feature.points[i];
                const float dx=b.x-a.x,dy=b.y-a.y,length2=dx*dx+dy*dy;
                const float px=x+.5f-a.x,py=y+.5f-a.y;
                const float t=length2>.000001f?std::clamp((px*dx+py*dy)/length2,0.f,1.f):0.f;
                if((px-dx*t)*(px-dx*t)+(py-dy*t)*(py-dy*t)<=radius*radius) return visualKind;
            }
        }
        const auto riverLike = [&](int neighborX, int neighborY) {
            const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
            return neighbor &&
                   (neighbor->kind == MapTileKind::Bridge ||
                    (neighbor->kind == MapTileKind::Water &&
                     (neighbor->wayKind == MapWayKind::River ||
                      neighbor->wayKind == MapWayKind::Canal)));
        };
        const bool north = riverLike(x, y - 1);
        const bool east = riverLike(x + 1, y);
        const bool south = riverLike(x, y + 1);
        const bool west = riverLike(x - 1, y);
        projectedRiverWater = (north && east) || (east && south) ||
                              (south && west) || (west && north);
    }
    if (!projectedRiverWater)
        return visualKind;

    constexpr int kindCount = static_cast<int>(MapTileKind::Bridge) + 1;
    int scores[kindCount]{};
    for (int offsetY = -3; offsetY <= 3; ++offsetY)
    {
        for (int offsetX = -3; offsetX <= 3; ++offsetX)
        {
            if (offsetX == 0 && offsetY == 0)
                continue;
            const MapCellModel *neighbor = mapCellAt(map, x + offsetX, y + offsetY);
            if (!neighbor || neighbor->wayKind == MapWayKind::River ||
                neighbor->wayKind == MapWayKind::Canal)
                continue;
            const MapTileKind neighborKind = mapVisualGroundKind(
                map, x + offsetX, y + offsetY);
            if (!detail::isOverviewLandKind(neighborKind))
                continue;
            const int kindIndex = static_cast<int>(neighborKind);
            if (kindIndex < 0 || kindIndex >= kindCount)
                continue;
            const int distance = std::max(std::abs(offsetX), std::abs(offsetY));
            const int cardinalBonus = (offsetX == 0 || offsetY == 0) ? 1 : 0;
            scores[kindIndex] += (4 - distance) * 2 + cardinalBonus;
        }
    }
    MapTileKind bestKind = MapTileKind::Empty;
    int bestScore = -1;
    for (int kindIndex = 0; kindIndex < kindCount; ++kindIndex)
    {
        if (scores[kindIndex] > bestScore)
        {
            bestScore = scores[kindIndex];
            bestKind = static_cast<MapTileKind>(kindIndex);
        }
    }
    return bestScore > 0 ? bestKind : visualKind;
}

// Renderer-local projection cache. Building it once avoids repeating the
// river-shoulder neighborhood search for every PNG pixel and for every shared
// ImGui terrain vertex. It is derived data and never enters campaign storage.
} // namespace gm::map_render
