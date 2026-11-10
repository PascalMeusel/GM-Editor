#pragma once

#include "Models/Map/MapOverviewRidges.hpp"

namespace gm::map_render
{
inline std::vector<OverviewForestGlyph> buildOverviewForestGlyphs(
    const MapModel &map)
{
    std::vector<OverviewForestGlyph> result;
    if (!overviewUsesAtlasTerrain(map) || map.width <= 0 || map.height <= 0)
        return result;
    const std::uint32_t seed = map.generation.recipe.seed ^ 0xDB4F0B91u;
    const bool composedForest=map.generation.recipe.styleId=="illustrated_atlas" &&
        ((map.generation.recipe.styleVersion==5 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v5") ||
         (map.generation.recipe.styleVersion==6 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v6"));
    constexpr int neighborOffsets[][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            if (overviewGroundKindForRender(map, x, y) != MapTileKind::Forest)
                continue;
            int forestNeighbors = 0;
            for (const auto &offset : neighborOffsets)
                if (isMapCellInside(map, x + offset[0], y + offset[1]) &&
                    overviewGroundKindForRender(map, x + offset[0], y + offset[1]) ==
                        MapTileKind::Forest)
                    ++forestNeighbors;
            // Isolated dark cells remain part of the biome blend but do not
            // receive a tree mark that could be mistaken for a stray bush.
            if (forestNeighbors < 5)
                continue;
            std::uint32_t hash = seed ^
                                 (static_cast<std::uint32_t>(x) * 0x9E3779B9u) ^
                                 (static_cast<std::uint32_t>(y) * 0x85EBCA6Bu);
            hash ^= hash >> 16u;
            hash *= 0x7FEB352Du;
            hash ^= hash >> 15u;
            // Three low-ranked crowns per neighbourhood form overlapping
            // woodland groups while deterministic jitter avoids a tile grid.
            const std::uint32_t centerScore = hash & 0x0000FFFFu;
            int lowerNeighbors = 0;
            for (int neighborY = y - 1; neighborY <= y + 1;
                 ++neighborY)
                for (int neighborX = x - 1; neighborX <= x + 1; ++neighborX)
                {
                    if ((neighborX == x && neighborY == y) ||
                        !isMapCellInside(map, neighborX, neighborY) ||
                        overviewGroundKindForRender(map, neighborX, neighborY) !=
                            MapTileKind::Forest)
                        continue;
                    std::uint32_t neighborHash = seed ^
                        (static_cast<std::uint32_t>(neighborX) * 0x9E3779B9u) ^
                        (static_cast<std::uint32_t>(neighborY) * 0x85EBCA6Bu);
                    neighborHash ^= neighborHash >> 16u;
                    neighborHash *= 0x7FEB352Du;
                    neighborHash ^= neighborHash >> 15u;
                    const std::uint32_t neighborScore =
                        neighborHash & 0x0000FFFFu;
                    if (neighborScore < centerScore ||
                        (neighborScore == centerScore &&
                         (neighborY < y ||
                          (neighborY == y && neighborX < x))))
                    {
                        ++lowerNeighbors;
                    }
                }
            if (lowerNeighbors >= 3)
                continue;
            const float jitterX =
                (static_cast<float>((hash >> 8u) & 255u) / 255.0f - 0.5f) * (composedForest?.82f:.28f);
            const float jitterY =
                (static_cast<float>((hash >> 16u) & 255u) / 255.0f - 0.5f) * (composedForest?.78f:.22f);
            OverviewForestGlyph glyph;
            glyph.center = {static_cast<float>(x) + 0.5f + jitterX,
                            static_cast<float>(y) + 0.5f + jitterY};
            if (detail::overviewForestPointNeedsClearance(map, glyph.center))
                continue;
            glyph.variation = static_cast<float>((hash >> 24u) & 15u) / 15.0f;
            glyph.sizeCells = composedForest ? .72f+glyph.variation*.42f : .84f+glyph.variation*.24f;
            result.push_back(glyph);
        }
    }
    return result;
}

inline std::vector<OverviewForestHachure> buildOverviewForestHachures(
    const MapModel &map)
{
    std::vector<OverviewForestHachure> result;
    const std::vector<OverviewForestGlyph> glyphs = buildOverviewForestGlyphs(map);
    result.reserve(glyphs.size() * 2u);
    for (const OverviewForestGlyph &glyph : glyphs)
    {
        const float angle = -0.24f + glyph.variation * 0.48f;
        const float tangentX = std::cos(angle);
        const float tangentY = std::sin(angle);
        const float normalX = -tangentY;
        const float normalY = tangentX;
        for (int stroke = 0; stroke < 2; ++stroke)
        {
            const float scale = stroke == 0 ? 1.0f : 0.72f;
            const float offset = stroke == 0 ? -0.10f : 0.20f;
            const float halfLength = glyph.sizeCells * 0.43f * scale;
            // Parallel dry-brush dashes read as a woodland wash. A peaked
            // hachure looked like rows of tiny birds at overview scale.
            const float arch = 0.0f;
            const float centerX = glyph.center.x + normalX * glyph.sizeCells * offset;
            const float centerY = glyph.center.y + normalY * glyph.sizeCells * offset;
            OverviewForestHachure hachure;
            hachure.start = {centerX - tangentX * halfLength,
                             centerY - tangentY * halfLength};
            hachure.crest = {centerX - normalX * arch,
                             centerY - normalY * arch};
            hachure.end = {centerX + tangentX * halfLength,
                           centerY + tangentY * halfLength};
            hachure.emphasis = (0.82f + glyph.variation * 0.18f) * scale;
            result.push_back(hachure);
        }
    }
    return result;
}

// Straight graph edges remain authoritative, but a continental road should
// look hand-routed rather than ruler-drawn. Insert stable sub-controls with a
// very small perpendicular drift; endpoints and all original controls remain
// fixed, so connectivity and atlas lineage are unchanged.
} // namespace gm::map_render
