#pragma once

#include "Models/Map/MapOverviewPoliticalBorders.hpp"
#include "Models/Map/MapOverviewGroundSamplingCache.hpp"

namespace gm::map_render
{
inline float overviewValueNoise(float mapX, float mapY, std::uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(mapX));
    const int y0 = static_cast<int>(std::floor(mapY));
    const float fractionX = mapX - static_cast<float>(x0);
    const float fractionY = mapY - static_cast<float>(y0);
    const float smoothX = fractionX * fractionX * (3.0f - 2.0f * fractionX);
    const float smoothY = fractionY * fractionY * (3.0f - 2.0f * fractionY);
    const auto lattice = [seed](int x, int y) {
        std::uint32_t value = seed ^
                              (static_cast<std::uint32_t>(x) * 0x9E3779B9u) ^
                              (static_cast<std::uint32_t>(y) * 0x85EBCA6Bu);
        value ^= value >> 16u;
        value *= 0x7FEB352Du;
        value ^= value >> 15u;
        value *= 0x846CA68Bu;
        value ^= value >> 16u;
        return static_cast<float>(value & 0x00FFFFFFu) /
                   static_cast<float>(0x007FFFFFu) -
               1.0f;
    };
    const float top = lattice(x0, y0) +
                      (lattice(x0 + 1, y0) - lattice(x0, y0)) * smoothX;
    const float bottom = lattice(x0, y0 + 1) +
                         (lattice(x0 + 1, y0 + 1) - lattice(x0, y0 + 1)) * smoothX;
    return top + (bottom - top) * smoothY;
}

// A low-frequency two-dimensional displacement breaks the remaining half-cell
// rhythm without introducing a visible directional wave. Both the semantic
// terrain reconstruction and the coastline contour use this exact sample.
inline OverviewSamplePoint detail::overviewOrganicTerrainSampleCached(const MapModel &map,
                                                        float mapX,
                                                        float mapY,
                                                        const OverviewGroundSamplingCache *cache)
{
    const int cellX = static_cast<int>(std::floor(mapX)), cellY = static_cast<int>(std::floor(mapY));
    if (cache ? cache->thinCore(map, cellX, cellY) : overviewHasThinTerrainCore(map, cellX, cellY))
        return {mapX,mapY};
    const std::uint32_t seed = map.generation.recipe.seed ^ 0x6D2B79F5u;
    const float broadX = overviewValueNoise(mapX / 7.5f, mapY / 7.5f, seed);
    const float broadY = overviewValueNoise(mapX / 7.5f, mapY / 7.5f,
                                            seed ^ 0x9E3779B9u);
    const float mediumX = overviewValueNoise(mapX / 2.65f, mapY / 2.65f,
                                             seed ^ 0xC2B2AE35u);
    const float mediumY = overviewValueNoise(mapX / 2.65f, mapY / 2.65f,
                                             seed ^ 0x27D4EB2Fu);
    const float fineX = overviewValueNoise(mapX / 1.25f, mapY / 1.25f,
                                           seed ^ 0x165667B1u);
    const float fineY = overviewValueNoise(mapX / 1.25f, mapY / 1.25f,
                                           seed ^ 0x85EBCA77u);
    return {mapX + broadX * 0.27f + mediumX * 0.22f + fineX * 0.08f,
            mapY + broadY * 0.27f + mediumY * 0.22f + fineY * 0.08f};
}

inline OverviewSamplePoint overviewOrganicTerrainSample(const MapModel &map,
                                                        float mapX, float mapY)
{
    return detail::overviewOrganicTerrainSampleCached(map, mapX, mapY, nullptr);
}

// The broad kernel determines a smooth organic iso-contour; this remap then
// narrows the visible land/water transition to a crisp sub-cell shoreline.
// It avoids both the old hard staircase and the later wide blurred potato edge.
inline float overviewSharpenLandCoverage(float coverage)
{
    const float normalized = std::clamp((coverage - 0.475f) / 0.05f, 0.0f, 1.0f);
    return normalized * normalized * (3.0f - 2.0f * normalized);
}

inline float detail::overviewCartographicLandCoverageCached(const MapModel &map,
                                               float mapX,
                                               float mapY,
                                               float rawCoverage, bool sharpen,
                                               const OverviewGroundSamplingCache *cache)
{
    // Perturb only the implicit half-coverage contour. Fully wet and fully dry
    // areas stay untouched, while long raster-aligned coast plateaus gain a
    // deterministic sub-cell shoreline in both renderers.
    const std::uint32_t seed = map.generation.recipe.seed ^ 0x94D049BBu;
    const float shapeNoise =
        overviewValueNoise(mapX / 2.35f, mapY / 2.35f, seed) * 0.095f +
        overviewValueNoise(mapX / 0.92f, mapY / 0.92f,
                           seed ^ 0x369DEA0Fu) * 0.035f;
    const float edgeInfluence = std::clamp(
        1.0f - std::abs(rawCoverage - 0.5f) * 2.0f, 0.0f, 1.0f);
    float coverage = std::clamp(rawCoverage + shapeNoise * edgeInfluence, 0.0f, 1.0f);
    const int cellX=static_cast<int>(std::floor(mapX)),cellY=static_cast<int>(std::floor(mapY));
    if (cache ? cache->thinCore(map,cellX,cellY) : overviewHasThinTerrainCore(map,cellX,cellY))
    {
        // Smooth compact support vanishes before the cell edge, retaining a
        // rounded connected shoreline rather than exposing square paint tiles.
        float dx=(mapX-cellX-.5f)/.48f,dy=(mapY-cellY-.5f)/.48f;
        const bool centreLand=overviewGroundKindIsLand(cache ? cache->ground(map,cellX,cellY) : overviewGroundKindForRender(map,cellX,cellY));
        const auto same=[&](int x,int y) {return isMapCellInside(map,x,y) &&
            overviewGroundKindIsLand(cache ? cache->ground(map,x,y) : overviewGroundKindForRender(map,x,y))==centreLand;};
        // Adjacent strokes join continuously; only exposed ends are rounded.
        if ((dx<0.f && same(cellX-1,cellY)) || (dx>=0.f && same(cellX+1,cellY))) dx=0.f;
        if ((dy<0.f && same(cellX,cellY-1)) || (dy>=0.f && same(cellX,cellY+1))) dy=0.f;
        const float weight=std::max(0.f,1.f-dx*dx-dy*dy);
        const float core=weight*weight*(3.f-2.f*weight);
        const float target=centreLand?1.f:0.f;
        coverage+= (target-coverage)*core;
    }
    return sharpen ? overviewSharpenLandCoverage(coverage) : coverage;
}

inline float overviewCartographicLandCoverage(const MapModel &map,
                                               float mapX, float mapY,
                                               float rawCoverage, bool sharpen)
{
    return detail::overviewCartographicLandCoverageCached(map, mapX, mapY, rawCoverage, sharpen, nullptr);
}

// Low-frequency fBm gives the atlas base quiet painterly variation without a
// repeated texture tile. Three octaves keep land readable while the renderer
// applies a lower amplitude to ocean pixels.
inline float overviewTerrainTone(const MapModel &map, float mapX, float mapY)
{
    const std::uint32_t seed = map.generation.recipe.seed ^ 0xA341316Cu;
    const float broad = overviewValueNoise(mapX / 10.5f, mapY / 10.5f, seed);
    const float medium = overviewValueNoise(mapX / 5.25f, mapY / 5.25f,
                                            seed ^ 0xC8013EA4u);
    const float fine = overviewValueNoise(mapX / 2.625f, mapY / 2.625f,
                                          seed ^ 0xAD90777Du);
    return std::clamp(broad * 0.46f + medium * 0.31f + fine * 0.23f,
                      -1.0f, 1.0f);
}

inline float overviewTerrainGrain(const MapModel &map, float mapX, float mapY)
{
    const std::uint32_t seed = map.generation.recipe.seed ^ 0x51ED270Bu;
    const float medium = overviewValueNoise(mapX * 1.65f, mapY * 1.65f, seed);
    const float fine = overviewValueNoise(mapX * 3.9f, mapY * 3.9f,
                                          seed ^ 0xD1B54A35u);
    return std::clamp(medium * 0.64f + fine * 0.36f, -1.0f, 1.0f);
}

// V5 river points are ordered source-to-mouth. A mild cartographic taper is
// enough to communicate flow without turning the waterway into a railway-like
// constant-width band. It is renderer-only and leaves graph geometry intact.
inline float overviewRiverWidthFactor(std::size_t pointIndex,
                                      std::size_t pointCount)
{
    if (pointCount <= 1u)
        return 1.0f;
    const float amount = std::clamp(
        static_cast<float>(pointIndex) / static_cast<float>(pointCount - 1u),
        0.0f, 1.0f);
    const float smooth = amount * amount * (3.0f - 2.0f * amount);
    // Keep the source at a readable two-pixel minimum in normal world-map
    // exports. The previous 0.58 factor rounded creeks and tributaries down to
    // a one-pixel hairline even though their semantic width was valid.
    return 0.78f + smooth * 0.22f;
}

inline OverviewSettlementGlyphKind overviewSettlementGlyphKind(
    std::string_view semanticRole)
{
    if (semanticRole == "capital")
        return OverviewSettlementGlyphKind::Capital;
    if (semanticRole == "city")
        return OverviewSettlementGlyphKind::City;
    return OverviewSettlementGlyphKind::Town;
}

inline OverviewSettlementGlyphStyle overviewSettlementGlyphStyle(
    OverviewSettlementGlyphKind kind)
{
    switch (kind)
    {
    case OverviewSettlementGlyphKind::Capital:
        return {0.72f, 0.50f};
    case OverviewSettlementGlyphKind::City:
        return {0.54f, 0.35f};
    case OverviewSettlementGlyphKind::Town:
    default:
        return {0.40f, 0.25f};
    }
}

inline std::uint32_t overviewStableFeatureHash(const MapGeneratedFeature &feature,
                                               std::uint32_t seed)
{
    std::uint32_t hash = 2166136261u ^ seed;
    for (const unsigned char character : feature.id)
    {
        hash ^= character;
        hash *= 16777619u;
    }
    for (const unsigned char character : feature.materialId)
    {
        hash ^= character;
        hash *= 16777619u;
    }
    return hash;
}

} // namespace gm::map_render
