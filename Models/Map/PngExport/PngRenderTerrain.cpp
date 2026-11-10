#include "Models/Map/PngExport/PngRenderSession.hpp"
#include "Models/Map/MapClassicIllustration.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/PngExport/PngAtlasInk.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"
#include "Models/Map/PngExport/PngTerrainAssets.hpp"
#include "Models/Map/PngExport/PngTerrainSampler.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"
#include "Models/Map/PngExport/PngWorldSurface.hpp"
#include "Models/Map/PngExport/PngWorldSymbols.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
void PngRenderSession::drawTerrainLayers()
{
        if (!importedBackgroundDrawn && showTerrain && showWater &&
            map.generation.continuousRendering)
        {
            // V2's regional land wash and organic coast sampling depend on
            // full-map coordinates and dimensions. A crop changes only the
            // output rectangle, never the reviewed terrain source.
            const bool globalTerrain = (gm::map_render::usesIllustratedAtlasDetailed(map) || gm::map_render::usesIllustratedLocalV6(map)) && fullTerrainSource;
            const MapModel &terrainSource = globalTerrain ? *fullTerrainSource : map;
            const int terrainOffsetX = globalTerrain ? static_cast<int>(std::lround(mapOffsetX * pixelsPerCell)) : 0;
            const int terrainOffsetY = globalTerrain ? static_cast<int>(std::lround(mapOffsetY * pixelsPerCell)) : 0;
            const std::vector<MapTileKind> overviewGroundProjection =
                gm::map_render::overviewUsesAtlasTerrain(terrainSource)
                    ? gm::map_render::buildOverviewGroundProjection(terrainSource)
                    : std::vector<MapTileKind>{};
            for (int pixelY = 0; pixelY < height; ++pixelY)
            {
                const float rawCellY = (gm::map_render::overviewUsesAtlasTerrain(map) || gm::map_render::usesIllustratedLocalV6(map))
                    ? (static_cast<float>(pixelY + terrainOffsetY) + 0.5f) / pixelsPerCell
                    : static_cast<float>(pixelY) / pixelsPerCell - 0.5f;
                for (int pixelX = 0; pixelX < width; ++pixelX)
                {
                    const float rawCellX = (gm::map_render::overviewUsesAtlasTerrain(map) || gm::map_render::usesIllustratedLocalV6(map))
                        ? (static_cast<float>(pixelX + terrainOffsetX) + 0.5f) / pixelsPerCell
                        : static_cast<float>(pixelX) / pixelsPerCell - 0.5f;
                    const float phase = static_cast<float>(map.generation.recipe.seed % 997u) * 0.011f;
                    // World terrain already applies the shared deterministic
                    // 2-D organic sample inside continuousTerrainPixel(). The
                    // older sine displacement here warped it a second time,
                    // reintroduced directional bands and made PNG previews
                    // substantially mushier than the ImGui canvas.
                    const float warpStrength = (gm::map_render::overviewUsesAtlasTerrain(map) || gm::map_render::usesIllustratedLocalV6(map)) ? 0.0f :
                                               (map.atlasLevel == MapLevel::Region ? 0.22f : 0.10f);
                    const float warpX = (std::sin(rawCellY * 0.73f + phase) + std::sin(rawCellY * 1.91f - phase * 0.7f)) * warpStrength * 0.5f;
                    const float warpY = (std::sin(rawCellX * 0.67f - phase * 0.8f) + std::sin(rawCellX * 1.47f + phase * 0.5f)) * warpStrength * 0.5f;
                    pixels[static_cast<std::size_t>(pixelY * width + pixelX)] = continuousTerrainPixel(
                        terrainSource, rawCellX + warpX, rawCellY + warpY, pixelX + terrainOffsetX, pixelY + terrainOffsetY,
                        pixelsPerCell, &overviewGroundProjection);
                }
            }
        }
        else if (!importedBackgroundDrawn && (showTerrain || showWater))
        {
            for (int y = 0; y < map.height; ++y)
                for (int x = 0; x < map.width; ++x)
                {
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    const MapTileKind kind = cell ? cell->kind : MapTileKind::Empty;
                    if(!map.generation.continuousRendering) {
                        auto terrain=classicTerrain(x,y);
                        const auto way=classicWay(x,y);
                        if(mapWayRendersAsBridge(way))terrain.kind=MapTileKind::Water;
                        const bool waterGround=terrain.kind==MapTileKind::Water||terrain.kind==MapTileKind::Bridge;
                        if(waterGround?showWater:showTerrain)
                            drawAtlasInk(pixels,width,height,gm::map_render::classicCellInk(terrain,true,false),pixelsPerCell,float(classicOffsetX),float(classicOffsetY));
                        continue;
                    }
                    const bool waterCell = kind == MapTileKind::Water ||
                                           (cell && (cell->wayKind == MapWayKind::River ||
                                                     cell->wayKind == MapWayKind::Canal));
                    const bool layerVisible = waterCell ? showWater : showTerrain;
                    const Pixel color = !layerVisible
                        ? Pixel{74, 96, 53, 255}
                        : (map.scope == MapScope::Dungeon &&
                                   (kind == MapTileKind::Empty || kind == MapTileKind::Wall)
                               ? Pixel{29, 31, 33, 255}
                               : terrainColor(kind));
                    fillRect(pixels, width, height, x * pixelsPerCell, y * pixelsPerCell, (x + 1) * pixelsPerCell, (y + 1) * pixelsPerCell, color);
                }
        }

        if(!map.generation.continuousRendering && (showWays||showWater)) {
            // Neighbouring line caps can overlap a cropped edge. Include the
            // full-map halo just as a crop of the uncropped render would.
            for(int y=-1;y<=map.height;++y)for(int x=-1;x<=map.width;++x) {
                if(!isMapCellInside(classicSource,x+classicOffsetX,y+classicOffsetY))continue;
                const auto terrain=classicTerrain(x,y);
                const auto way=classicWay(x,y);
                const bool waterWay=mapWayKindIsWaterLike(way.wayKind);
                if(waterWay?showWater:showWays)
                    drawAtlasInk(pixels,width,height,gm::map_render::classicWayInk(way,terrain),pixelsPerCell,float(classicOffsetX),float(classicOffsetY));
            }
        }

        if (!importedBackgroundDrawn && (showTerrain || showWater) &&
            map.generation.continuousRendering)
            applyWorldPainterlySurface(pixels, width, height, map);

        // Legacy image underlays retain their configured opacity. New managed
        // imports use the full-resolution base path above.
        if (!importedBackgroundDrawn && showBackground &&
            !map.importedReference.enabled &&
            !plan.backgroundExportProjectRelativePath.empty())
        {
            const std::uint8_t opacity = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(std::lround(map.backgroundImageOpacity * 255.0f)), 0, 255));
            const MapImageImport::NormalizedSamplingTransform legacyTransform =
                MapImageImport::makeNormalizedSamplingTransform(
                    {0.0f, 0.0f, 1.0f, 1.0f}, 0.0f, mapViewport);
            drawBackgroundReference(pixels, width, height,
                                    resolveBackgroundPath(plan.backgroundExportProjectRelativePath),
                                    legacyTransform, opacity);
        }

        if (!importedBackgroundDrawn && showTerrain &&
            map.generation.continuousRendering)
        {
            // Continuous terrain interpolation already provides the organic
            // biome transition.  The current blend assets are baked colour
            // swatches rather than neutral masks, so drawing them as decals
            // created a conspicuous repeated field of pale circles.

            for (int y = 0; y < map.height; ++y)
            {
                for (int x = 0; x < map.width; ++x)
                {
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell)
                        continue;
                    if (gm::map_render::usesIllustratedLocalV6(map)) continue;
                    const int cx = x * pixelsPerCell + pixelsPerCell / 2;
                    const int cy = y * pixelsPerCell + pixelsPerCell / 2;
                    const int variant = (x * 37 + y * 61 + static_cast<int>(map.generation.recipe.seed & 255u)) & 7;
                    const bool productionAssets = map.generation.recipe.styleVersion >= 3 &&
                                                  mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId);
                    if (!gm::map_render::usesSettlementSiteInk(map) && productionAssets && !gm::map_render::overviewUsesAtlasTerrain(map) &&
                        cell->kind == MapTileKind::Forest &&
                        stableMapVariant(x, y, 1501, scaleClusterModulo(map.atlasLevel, cell->kind)) == 0)
                    {
                        const float extentCells = 2.1f;
                        const int extent = static_cast<int>(extentCells * pixelsPerCell);
                        const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
                            map.generation.recipe.styleSnapshotId, map.atlasLevel,
                            "vegetation_stamp", "forest_edge",
                            x, y, variant);
                        drawRasterAssetPreservingAspect(pixels, width, height, cx, cy, extent, asset);
                    }
                    else if (!productionAssets && !gm::map_render::overviewUsesAtlasTerrain(map) &&
                             cell->kind == MapTileKind::Forest && variant < 5)
                    {
                        const int crown = std::max(3, pixelsPerCell / 7);
                        fillCircle(pixels, width, height, cx - crown, cy + crown / 3, crown, {39, 73, 42, 255});
                        fillCircle(pixels, width, height, cx + crown, cy, crown + 1, {48, 92, 47, 255});
                        fillCircle(pixels, width, height, cx, cy - crown, crown + 2, {57, 105, 53, 255});
                    }
                    else if (!productionAssets && !gm::map_render::usesIllustratedAtlas(map) && cell->kind == MapTileKind::Mountain && variant < 6)
                    {
                        const int half = pixelsPerCell * 2 / 5;
                        fillTriangle(pixels, width, height, cx, cy - half, cx - half, cy + half, cx + half, cy + half, {83, 78, 69, 255});
                        fillTriangle(pixels, width, height, cx, cy - half, cx, cy + half, cx + half, cy + half, {121, 112, 91, 255});
                        fillTriangle(pixels, width, height, cx, cy - half, cx - pixelsPerCell / 9, cy - pixelsPerCell / 8,
                                     cx + pixelsPerCell / 8, cy - pixelsPerCell / 10, {208, 205, 185, 255});
                    }
                }
            }

            // The editor uses one connected, scale-specific macro stamp per
            // mountain mass. Mirror that contract in PNG/player-safe exports;
            // never fall back to one tactical peak (or tree) per world cell.
            if (gm::map_render::usesIllustratedLocalV6(map) || gm::map_render::usesIllustratedAtlas(map) || gm::map_render::usesSettlementSiteInk(map) || gm::map_render::usesNaturalCaveInk(map) || gm::map_render::usesDungeonMasonryInk(map))
            {
                drawAtlasInk(pixels, width, height, plan.atlasInk, pixelsPerCell, mapOffsetX, mapOffsetY);
            }
            else if (gm::map_render::overviewUsesAtlasTerrain(map))
            {
                drawWorldForestPattern(pixels, width, height, map, pixelsPerCell);
                const std::string ridgeSnapshot = map.generation.recipe.styleSnapshotId.empty()
                    ? mapStyleCurrentProductionSnapshotId() : map.generation.recipe.styleSnapshotId;
                if (!mapStyleProductionSnapshotAvailable(ridgeSnapshot) &&
                    !gm::map_render::buildOverviewRidgePaths(map).empty())
                    drawWorldOverviewRidges(pixels, width, height, map, pixelsPerCell);
                else
                    drawOverviewMountainRanges(pixels, width, height, map, pixelsPerCell);
            }
            else
                drawOverviewMountainRanges(pixels, width, height, map, pixelsPerCell);
            if (showWater && !gm::map_render::usesIllustratedAtlasDetailed(map))
                drawOverviewCoastTreatment(pixels, width, height, map, pixelsPerCell);
        }

        if (!importedBackgroundDrawn && showWater && (gm::map_render::usesIllustratedAtlasDetailed(map) || gm::map_render::usesIllustratedLocalV6(map)))
            drawAtlasInk(pixels, width, height, plan.atlasWaterInk, pixelsPerCell, mapOffsetX, mapOffsetY);

}
}
