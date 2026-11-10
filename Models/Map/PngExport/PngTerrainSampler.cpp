#include "Models/Map/PngExport/PngTerrainSampler.hpp"
#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/PngExport/PngTerrainAssets.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
Pixel continuousTerrainPixel(
    const MapModel &map,
    float cellX,
    float cellY,
    int pixelX,
    int pixelY,
    int pixelsPerCell,
    const std::vector<MapTileKind> *overviewGroundProjection)
{
    if (gm::map_render::usesIllustratedLocalV6(map)) {
        const auto c=gm::map_render::illustratedLocalTerrainColor(map,cellX,cellY);
        return {c.r,c.g,c.b,c.a};
    }
    if (gm::map_render::usesIllustratedAtlas(map))
    {
        const auto c = gm::map_render::illustratedAtlasTerrainColor(map, cellX, cellY, overviewGroundProjection);
        return {c.r, c.g, c.b, c.a};
    }
    const bool world = gm::map_render::overviewUsesAtlasTerrain(map);
    const gm::map_render::OverviewSamplePoint organicSample = world
        ? gm::map_render::overviewOrganicTerrainSample(map, cellX, cellY)
        : gm::map_render::OverviewSamplePoint{cellX, cellY};
    const float terrainCellX = organicSample.x;
    const float terrainCellY = organicSample.y;
    const int x0 = static_cast<int>(std::floor(terrainCellX));
    const int y0 = static_cast<int>(std::floor(terrainCellY));
    const float txRaw = terrainCellX - static_cast<float>(x0);
    const float tyRaw = terrainCellY - static_cast<float>(y0);
    const float tx = txRaw * txRaw * (3.0f - 2.0f * txRaw);
    const float ty = tyRaw * tyRaw * (3.0f - 2.0f * tyRaw);
    const auto sampleGroundKind = [&](int x, int y) {
        x = std::clamp(x, 0, map.width - 1);
        y = std::clamp(y, 0, map.height - 1);
        return gm::map_render::overviewUsesAtlasTerrain(map)
                   ? (overviewGroundProjection
                          ? gm::map_render::overviewGroundKindFromProjection(
                                map, *overviewGroundProjection, x, y)
                          : gm::map_render::overviewGroundKindForRender(map, x, y))
                   : mapVisualGroundKind(map, x, y);
    };
    auto sample = [&](int x, int y) {
        const MapTileKind kind = sampleGroundKind(x, y);
        if (map.scope == MapScope::Dungeon && (kind == MapTileKind::Empty || kind == MapTileKind::Wall))
            return Pixel{29, 31, 33, 255};
        const MapTileKind renderedKind = kind == MapTileKind::Empty ? MapTileKind::Empty : kind;
        // At continental scale forest cells describe a broad biome.  The
        // close-range forest base was so dark that a large woodland read as a
        // shadow or an unexplored void in preview and export.
        Pixel base = terrainColor(renderedKind);
        if (gm::map_render::overviewUsesAtlasTerrain(map))
        {
            if (renderedKind == MapTileKind::Water || renderedKind == MapTileKind::Ice)
                base = {gm::map_render::kOverviewOceanBase.r,
                        gm::map_render::kOverviewOceanBase.g,
                        gm::map_render::kOverviewOceanBase.b, 255};
            else if (renderedKind == MapTileKind::Empty)
                base = {gm::map_render::kOverviewPlainBase.r,
                        gm::map_render::kOverviewPlainBase.g,
                        gm::map_render::kOverviewPlainBase.b, 255};
            else if (renderedKind == MapTileKind::Forest)
                base = {gm::map_render::kOverviewForestBase.r,
                        gm::map_render::kOverviewForestBase.g,
                        gm::map_render::kOverviewForestBase.b, 255};
            else if (renderedKind == MapTileKind::Mountain || renderedKind == MapTileKind::Hill)
                base = {gm::map_render::kOverviewMountainGround.r,
                        gm::map_render::kOverviewMountainGround.g,
                        gm::map_render::kOverviewMountainGround.b, 255};
        }
        if (map.generation.recipe.styleVersion < 3 ||
            !mapRenderUsesDetailedTerrainMaterials(map.atlasLevel,
                                                    static_cast<float>(pixelsPerCell)) ||
            (map.scope == MapScope::Dungeon && kind == MapTileKind::Empty))
            return base;
        // World terrain is rendered from a single global watercolor surface
        // below. Local terrain paintings contain tactical detail and visible
        // repeat structure at continental scale, regardless of biome.
        if (gm::map_render::overviewUsesAtlasTerrain(map))
            return base;
        const Pixel texture = sampleTerrainAsset(map.generation.recipe.styleSnapshotId, map.atlasLevel, renderedKind,
                                                 pixelX, pixelY, pixelsPerCell, map.generation.recipe.seed);
        const float textureWeight = renderedKind == MapTileKind::Water ? 0.52f :
                                    (map.scope == MapScope::Dungeon ? 0.48f : 0.38f);
        return blended(base, texture, textureWeight);
    };
    Pixel result;
    float overviewLandCoverageForTone = 1.0f;
    if (gm::map_render::overviewUsesAtlasTerrain(map))
    {
        // A four-cell soft reconstruction turns the semantic coastline into a
        // cartographic landmass instead of a blurred staircase. It is still a
        // deterministic projection of the same cells and therefore changes no
        // gameplay or atlas data.
        float landRed = 0.0f;
        float landGreen = 0.0f;
        float landBlue = 0.0f;
        float landWeight = 0.0f;
        float waterRed = 0.0f;
        float waterGreen = 0.0f;
        float waterBlue = 0.0f;
        float waterWeight = 0.0f;
        float domainLandWeight = 0.0f;
        float domainTotalWeight = 0.0f;
        const auto axisWeight = [](float distance) {
            const float normalized = std::clamp(
                1.0f - std::abs(distance) / gm::map_render::kOverviewTerrainBlendRadiusCells,
                0.0f, 1.0f);
            return normalized * normalized * (3.0f - 2.0f * normalized);
        };
        const auto domainAxisWeight = [](float distance) {
            const float normalized = std::clamp(
                1.0f - std::abs(distance) / gm::map_render::kOverviewCoastBlendRadiusCells,
                0.0f, 1.0f);
            return normalized * normalized * (3.0f - 2.0f * normalized);
        };
        for (int sampleY = y0 - 2; sampleY <= y0 + 2; ++sampleY)
            for (int sampleX = x0 - 2; sampleX <= x0 + 2; ++sampleX)
            {
                const float distanceX = static_cast<float>(sampleX) + 0.5f - terrainCellX;
                const float distanceY = static_cast<float>(sampleY) + 0.5f - terrainCellY;
                const float weight = axisWeight(distanceX) * axisWeight(distanceY);
                const float domainWeight = domainAxisWeight(distanceX) * domainAxisWeight(distanceY);
                if (weight <= 0.0001f && domainWeight <= 0.0001f)
                    continue;
                const bool land = gm::map_render::overviewGroundKindIsLand(
                    sampleGroundKind(sampleX, sampleY));
                if (domainWeight > 0.0001f)
                {
                    domainTotalWeight += domainWeight;
                    if (land)
                        domainLandWeight += domainWeight;
                }
                if (weight <= 0.0001f)
                    continue;
                const Pixel color = sample(sampleX, sampleY);
                if (land)
                {
                    landRed += static_cast<float>(color.r) * weight;
                    landGreen += static_cast<float>(color.g) * weight;
                    landBlue += static_cast<float>(color.b) * weight;
                    landWeight += weight;
                }
                else
                {
                    waterRed += static_cast<float>(color.r) * weight;
                    waterGreen += static_cast<float>(color.g) * weight;
                    waterBlue += static_cast<float>(color.b) * weight;
                    waterWeight += weight;
                }
            }
        const Pixel fallbackLand = {gm::map_render::kOverviewPlainBase.r,
                                    gm::map_render::kOverviewPlainBase.g,
                                    gm::map_render::kOverviewPlainBase.b, 255};
        const Pixel fallbackWater = {gm::map_render::kOverviewOceanBase.r,
                                     gm::map_render::kOverviewOceanBase.g,
                                     gm::map_render::kOverviewOceanBase.b, 255};
        const float averageLandRed = landWeight > 0.0001f ? landRed / landWeight : fallbackLand.r;
        const float averageLandGreen = landWeight > 0.0001f ? landGreen / landWeight : fallbackLand.g;
        const float averageLandBlue = landWeight > 0.0001f ? landBlue / landWeight : fallbackLand.b;
        const float averageWaterRed = waterWeight > 0.0001f ? waterRed / waterWeight : fallbackWater.r;
        const float averageWaterGreen = waterWeight > 0.0001f ? waterGreen / waterWeight : fallbackWater.g;
        const float averageWaterBlue = waterWeight > 0.0001f ? waterBlue / waterWeight : fallbackWater.b;
        const float rawLandCoverage = domainTotalWeight > 0.0001f
                                          ? std::clamp(domainLandWeight / domainTotalWeight, 0.0f, 1.0f)
                                          : (landWeight >= waterWeight ? 1.0f : 0.0f);
        const float landCoverage = gm::map_render::overviewCartographicLandCoverage(
            map, terrainCellX, terrainCellY, rawLandCoverage);
        overviewLandCoverageForTone = landCoverage;
        result = {
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(
                averageWaterRed + (averageLandRed - averageWaterRed) * landCoverage)), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(
                averageWaterGreen + (averageLandGreen - averageWaterGreen) * landCoverage)), 0, 255)),
            static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(
                averageWaterBlue + (averageLandBlue - averageWaterBlue) * landCoverage)), 0, 255)),
            255};
    }
    else if (gm::map_render::usesTacticalSoftTerrainBlend(map))
    {
        float r=0.f,g=0.f,b=0.f,total=0.f;
        for (int y=y0-1;y<=y0+2;++y) for (int x=x0-1;x<=x0+2;++x)
        {
            const float w=gm::map_render::tacticalTerrainBlendWeight(x-terrainCellX,y-terrainCellY);
            if (w<=.00001f) continue;
            const auto c=sample(x,y);
            r+=c.r*w;g+=c.g*w;b+=c.b*w;total+=w;
        }
        result={static_cast<std::uint8_t>(std::clamp(std::lround(r/total),0l,255l)),
                static_cast<std::uint8_t>(std::clamp(std::lround(g/total),0l,255l)),
                static_cast<std::uint8_t>(std::clamp(std::lround(b/total),0l,255l)),255};
    }
    else
    {
        const Pixel top = blended(sample(x0, y0), sample(x0 + 1, y0), tx);
        const Pixel bottom = blended(sample(x0, y0 + 1), sample(x0 + 1, y0 + 1), tx);
        result = blended(top, bottom, ty);
    }
    const MapCellModel *centerCell = mapCellAt(
        map, std::clamp(static_cast<int>(std::floor(cellX + 0.5f)), 0, map.width - 1),
        std::clamp(static_cast<int>(std::floor(cellY + 0.5f)), 0, map.height - 1));
    if (map.scope == MapScope::Dungeon && centerCell &&
        (centerCell->kind == MapTileKind::Empty || centerCell->kind == MapTileKind::Wall))
        return result;
    if (gm::map_render::overviewUsesAtlasTerrain(map))
    {
        const float tonalAmplitude = 2.2f + overviewLandCoverageForTone * 9.3f;
        const int tonalOffset = static_cast<int>(std::lround(
            gm::map_render::overviewTerrainTone(map, terrainCellX, terrainCellY) * tonalAmplitude));
        result.r = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(result.r) + tonalOffset, 0, 255));
        result.g = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(result.g) + tonalOffset, 0, 255));
        result.b = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(result.b) + tonalOffset / 2, 0, 255));
    }
    const float grain = gm::map_render::overviewTerrainGrain(
        map, terrainCellX, terrainCellY);
    const float grainAmplitude = gm::map_render::overviewUsesAtlasTerrain(map)
                                     ? 1.2f + overviewLandCoverageForTone * 2.3f
                                     : 6.0f;
    const int variation = static_cast<int>(std::lround(grain * grainAmplitude));
    result.r = static_cast<std::uint8_t>(std::clamp(static_cast<int>(result.r) + variation, 0, 255));
    result.g = static_cast<std::uint8_t>(std::clamp(static_cast<int>(result.g) + variation, 0, 255));
    result.b = static_cast<std::uint8_t>(std::clamp(static_cast<int>(result.b) + variation / 2, 0, 255));
    return result;
}


}
