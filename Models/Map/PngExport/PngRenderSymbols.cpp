#include "Models/Map/PngExport/PngRenderSession.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapClassicIllustration.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapTravelInk.hpp"
#include "Models/Map/PngExport/PngAtlasInk.hpp"
#include "Models/Map/PngExport/PngLabelLayout.hpp"
#include "Models/Map/PngExport/PngRasterAssets.hpp"
#include "Models/Map/PngExport/PngSemanticOverlays.hpp"
#include "Models/Map/PngExport/PngVisualPolicy.hpp"
#include "Models/Map/PngExport/PngWorldSymbols.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
void PngRenderSession::drawSymbolLayers()
{
        // Dungeon void is intentionally untextured.  Only the boundary of a
        // room/corridor receives wall art, eliminating the old wallpaper of a
        // stamp on every blocked cell.
        if (showTerrain && map.generation.continuousRendering && map.scope == MapScope::Dungeon && !gm::map_render::usesNaturalCaveInk(map) && !gm::map_render::usesIllustratedLocalV6(map))
        {
            constexpr int wallOffsets[][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
            for (int y = 0; y < map.height; ++y)
                for (int x = 0; x < map.width; ++x)
                {
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell || (cell->kind != MapTileKind::Room && cell->kind != MapTileKind::Cave &&
                                  cell->kind != MapTileKind::Door && cell->kind != MapTileKind::Stairs))
                        continue;
                    for (int side = 0; side < 4; ++side)
                    {
                        const int *offset = wallOffsets[side];
                        const MapCellModel *neighbor = mapCellAt(map, x + offset[0], y + offset[1]);
                        if (neighbor && neighbor->kind != MapTileKind::Wall && neighbor->kind != MapTileKind::Empty)
                            continue;
                        const int left = x * pixelsPerCell;
                        const int top = y * pixelsPerCell;
                        const int right = (x + 1) * pixelsPerCell;
                        const int bottom = (y + 1) * pixelsPerCell;
                        int ax = left, ay = top, bx = right, by = top;
                        if (side == 1) { ax = right; ay = top; bx = right; by = bottom; }
                        else if (side == 2) { ax = left; ay = bottom; bx = right; by = bottom; }
                        else if (side == 3) { ax = left; ay = top; bx = left; by = bottom; }
                        drawLine(pixels, width, height, ax, ay, bx, by, std::max(2, pixelsPerCell / 3), {48, 44, 38, 255});
                        drawLine(pixels, width, height, ax, ay, bx, by, std::max(1, pixelsPerCell / 7), {130, 118, 94, 255});
                    }
                    if (!gm::map_render::usesDungeonArchitectureInk(map) && map.generation.recipe.styleVersion >= 3 && (cell->kind == MapTileKind::Door || cell->kind == MapTileKind::Stairs))
                    {
                        const int cx = x * pixelsPerCell + pixelsPerCell / 2;
                        const int cy = y * pixelsPerCell + pixelsPerCell / 2;
                        const int extent = static_cast<int>(pixelsPerCell * 1.35f);
                        const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
                            map.generation.recipe.styleSnapshotId, map.atlasLevel, "dungeon_stamp",
                            "dungeon_architecture", x, y, static_cast<int>(cell->kind) * 43);
                        drawRasterAsset(pixels, width, height, cx - extent / 2, cy - extent / 2, cx + extent / 2, cy + extent / 2, asset);
                    }
                }
        }
        else if (showTerrain && map.generation.continuousRendering && map.atlasLevel == MapLevel::Tactical && !gm::map_render::usesNaturalCaveInk(map) && !gm::map_render::usesIllustratedLocalV6(map))
        {
            // Outdoor ruins use the same boundary-first principle as dungeons:
            // contiguous walls read as one structure instead of a row of
            // unrelated square stamps.
            constexpr int wallOffsets[][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
            for (int y = 0; y < map.height; ++y)
                for (int x = 0; x < map.width; ++x)
                {
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell || cell->kind != MapTileKind::Wall)
                        continue;
                    const int left = x * pixelsPerCell;
                    const int top = y * pixelsPerCell;
                    const int right = (x + 1) * pixelsPerCell;
                    const int bottom = (y + 1) * pixelsPerCell;
                    for (int side = 0; side < 4; ++side)
                    {
                        const int *offset = wallOffsets[side];
                        const MapCellModel *neighbor = mapCellAt(map, x + offset[0], y + offset[1]);
                        if (neighbor && neighbor->kind == MapTileKind::Wall)
                            continue;
                        int ax = left, ay = top, bx = right, by = top;
                        if (side == 1) { ax = right; ay = top; bx = right; by = bottom; }
                        else if (side == 2) { ax = left; ay = bottom; bx = right; by = bottom; }
                        else if (side == 3) { ax = left; ay = top; bx = left; by = bottom; }
                        drawLine(pixels, width, height, ax, ay, bx, by, std::max(2, pixelsPerCell / 4), {49, 43, 37, 255});
                        drawLine(pixels, width, height, ax, ay, bx, by, std::max(1, pixelsPerCell / 12), {151, 132, 101, 255});
                    }
                }
        }

        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (playerSafe && (feature.gmOnly || !feature.discovered))
                continue;
            if ((gm::map_render::usesIllustratedAtlasV3(map) || (gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map))) || !showWays || feature.kind != MapGeneratedFeatureKind::Bridge ||
                feature.points.size() < 2)
                continue;
            const MapFeaturePoint &a = feature.points.front();
            const MapFeaturePoint &b = feature.points.back();
            const float dx = (b.x - a.x) * pixelsPerCell;
            const float dy = (b.y - a.y) * pixelsPerCell;
            const float bridgeLength = std::sqrt(dx * dx + dy * dy);
            const float centerX = (a.x + b.x) * pixelsPerCell * 0.5f;
            const float centerY = (a.y + b.y) * pixelsPerCell * 0.5f;
            const std::string semantic = feature.materialId.find("stone") != std::string::npos
                ? "bridge_stone_rough"
                : "bridge_wood_light";
            const MapStylePackedRuntimeAsset *bridgeAsset = map.generation.recipe.styleVersion >= 3
                ? resolveMapStyleProductionAssetForSnapshot(map.generation.recipe.styleSnapshotId,
                      map.atlasLevel, "bridge_stamp", semantic,
                      static_cast<int>(std::floor(centerX / pixelsPerCell)),
                      static_cast<int>(std::floor(centerY / pixelsPerCell)), 1901)
                : nullptr;
            if (bridgeAsset)
            {
                // Bridge source art is authored vertically, so source Y is
                // rotated onto the feature tangent and given the long axis.
                drawRotatedRasterAsset(pixels, width, height, centerX, centerY,
                                       pixelsPerCell * gm::map_render::tacticalBridgeStampWidth(map,feature),
                                       std::max(pixelsPerCell * 2.0f, bridgeLength + pixelsPerCell * 0.85f),
                                       std::atan2(dy, dx) - 1.57079632679f, bridgeAsset);
            }
            else
            {
                drawLine(pixels, width, height, static_cast<int>(a.x * pixelsPerCell), static_cast<int>(a.y * pixelsPerCell),
                         static_cast<int>(b.x * pixelsPerCell), static_cast<int>(b.y * pixelsPerCell), pixelsPerCell / 2, {91, 62, 36, 255});
                drawLine(pixels, width, height, static_cast<int>(a.x * pixelsPerCell), static_cast<int>(a.y * pixelsPerCell),
                         static_cast<int>(b.x * pixelsPerCell), static_cast<int>(b.y * pixelsPerCell), pixelsPerCell * 2 / 5, {174, 127, 70, 255});
            }
        }

        drawAtlasInk(pixels,width,height,plan.atlasFeatureInk,pixelsPerCell,mapOffsetX,mapOffsetY);

        // Tactical overlays are part of the map's semantic output. GM-only
        // deployment zones are deliberately omitted from player-safe exports;
        // public objectives remain visible and are later covered by fog like
        // every other unrevealed map detail.
        if (showAnnotations)
            drawTacticalRegionOverlays(pixels, width, height, map, pixelsPerCell, playerSafe);

        if (showSymbols)
        {
                for (int y = 0; y < map.height; ++y)
                {
                    for (int x = 0; x < map.width; ++x)
                    {
                        const MapCellModel *cell = mapCellAt(map, x, y);
                        if (!cell)
                            continue;
                        const int cx = x * pixelsPerCell + pixelsPerCell / 2;
                        const int cy = y * pixelsPerCell + pixelsPerCell / 2;
                        if(gm::map_render::travelPurposeAt(map,x,y)) continue;
                        if(gm::map_render::tacticalCoverInkAt(map,x,y)) continue;
                        if(cell->kind==MapTileKind::Hazard && gm::map_render::tacticalShrineAt(map,x,y)) continue;
                        if(cell->kind==MapTileKind::Cover && gm::map_render::tacticalWaystoneAt(map,x,y)) continue;
                        if ((cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building ||
                             cell->kind == MapTileKind::Landmark || cell->kind == MapTileKind::Cover ||
                             cell->kind == MapTileKind::Rubble || cell->kind == MapTileKind::Hazard) &&
                            !generatedPointSymbolVisible(map, x, y, playerSafe))
                            continue;
                        if (cell->kind == MapTileKind::Landmark && gm::map_render::settlementPoiAt(map,x,y)) continue;
                        if(!map.generation.continuousRendering) {
                            if(gm::map_render::classicCellIsSymbol(cell->kind))
                                drawAtlasInk(pixels,width,height,gm::map_render::classicCellInk(classicTerrain(x,y),false,true),pixelsPerCell,float(classicOffsetX),float(classicOffsetY));
                            continue;
                        }
                        if (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building)
                        {
                if (cell->kind == MapTileKind::Building && gm::map_render::supportsFootprintRoofInk(map)) continue;
                if (cell->kind == MapTileKind::Settlement && gm::map_render::usesSettlementSiteInk(map)) continue;
                    const MapRenderScaleProfile &scaleProfile = mapRenderScaleProfile(map.atlasLevel);
                    const bool buildingVisibleAtScale = cell->kind != MapTileKind::Building || scaleProfile.allowIndividualBuildings;
                    const bool productionSnapshot = map.generation.recipe.styleVersion >= 3 &&
                                                    mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId);
                    if (productionSnapshot &&
                        !isVisualStampAnchor(map, x, y, cell->kind, 1,
                                             cell->kind == MapTileKind::Building ? 3203 : 3101))
                        continue;
                    if (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map))
                    {
                        if (cell->kind == MapTileKind::Settlement && !(gm::map_render::usesIllustratedAtlasV3(map) || (gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map))))
                            drawWorldSettlementGlyph(
                                pixels, width, height, cx, cy, pixelsPerCell,
                                overviewSettlementSemanticRole(map, x, y));
                        // Individual buildings are intentionally absent from
                        // the continental atlas scale.
                        continue;
                    }
                    const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
                    const char *role = overview ? "map_symbol" : "settlement_stamp";
                    const char *semantic = overview ? (cell->kind == MapTileKind::Settlement
                                                            ? overviewSettlementSemanticRole(map, x, y)
                                                            : "town")
                                                    : mapSettlementVisualSemanticRole(map, cell->kind);
                    const MapStylePackedRuntimeAsset *asset = buildingVisibleAtScale && productionSnapshot
                        ? resolveMapStyleProductionAssetForSnapshot(map.generation.recipe.styleSnapshotId,
                              map.atlasLevel, role, semantic, x, y, static_cast<int>(cell->kind))
                        : nullptr;
                    if (asset)
                    {
                        float extentCells = cell->kind == MapTileKind::Settlement
                            ? scaleProfile.settlementStampCells
                            : scaleProfile.buildingStampCells;
                        if (map.atlasLevel == MapLevel::World && cell->kind == MapTileKind::Settlement)
                        {
                            const std::string_view semanticRole(semantic);
                            if (semanticRole == "capital")
                                extentCells *= 1.22f;
                            else if (semanticRole == "city")
                                extentCells *= 1.10f;
                        }
                        const int extent = static_cast<int>(pixelsPerCell * extentCells);
                        if (cell->kind == MapTileKind::Building)
                            drawRotatedRasterAsset(pixels, width, height, static_cast<float>(cx), static_cast<float>(cy),
                                                   static_cast<float>(extent), static_cast<float>(extent),
                                                   visualBuildingStreetRotation(map, x, y), asset);
                        else
                        {
                            drawRasterAsset(pixels, width, height, cx - extent / 2, cy - extent / 2,
                                            cx + extent / 2, cy + extent / 2, asset,
                                            255,
                                            map.atlasLevel == MapLevel::World
                                                ? Pixel{244, 235, 210, 230}
                                                : Pixel{255, 255, 255, 255});
                            if (map.atlasLevel == MapLevel::World)
                            {
                                const std::string_view semanticRole(semantic);
                                if (semanticRole == "capital" || semanticRole == "city")
                                {
                                    const int radius = std::max(3, static_cast<int>(std::lround(
                                        pixelsPerCell * (semanticRole == "capital" ? 1.04f : 0.91f))));
                                    drawCircleOutline(pixels, width, height, cx, cy, radius,
                                                      std::max(1, pixelsPerCell / 12),
                                                      {230, 198, 118, 255});
                                    if (semanticRole == "capital")
                                        drawCircleOutline(pixels, width, height, cx, cy,
                                                          radius + std::max(2, pixelsPerCell / 6),
                                                          std::max(1, pixelsPerCell / 14),
                                                          {68, 51, 31, 255});
                                }
                            }
                        }
                    }
                    else if (buildingVisibleAtScale)
                    {
                        const int half = cell->kind == MapTileKind::Settlement ? pixelsPerCell / 4 : pixelsPerCell / 6;
                        fillRect(pixels, width, height, cx - half, cy - half / 2, cx + half, cy + half, {210, 187, 137, 255});
                        drawLine(pixels, width, height, cx - half, cy - half / 2, cx, cy - half, 3, {113, 65, 42, 255});
                        drawLine(pixels, width, height, cx, cy - half, cx + half, cy - half / 2, 3, {113, 65, 42, 255});
                    }
                }
                else if (cell->kind == MapTileKind::Landmark)
                {
                if ((gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map))) continue;
                    if (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map))
                    {
                        if (!pngMapPointIsVisibleLand(map, static_cast<float>(x) + 0.5f,
                                                      static_cast<float>(y) + 0.5f, playerSafe))
                            continue;
                        // Match the editor's scale-safe cartographic landmark
                        // mark. The generic discovery PNG depicts a planted
                        // bridge and reads as stray bushes at world scale.
                        const int radius = std::max(3, pixelsPerCell * 3 / 10);
                        fillTriangle(pixels, width, height,
                                     cx, cy - radius, cx + radius, cy, cx, cy + radius,
                                     {222, 174, 76, 245});
                        fillTriangle(pixels, width, height,
                                     cx, cy - radius, cx, cy + radius, cx - radius, cy,
                                     {222, 174, 76, 245});
                        drawLine(pixels, width, height, cx, cy - radius, cx + radius, cy, 1, {62, 43, 24, 235});
                        drawLine(pixels, width, height, cx + radius, cy, cx, cy + radius, 1, {62, 43, 24, 235});
                        drawLine(pixels, width, height, cx, cy + radius, cx - radius, cy, 1, {62, 43, 24, 235});
                        drawLine(pixels, width, height, cx - radius, cy, cx, cy - radius, 1, {62, 43, 24, 235});
                        continue;
                    }
                    const MapStylePackedRuntimeAsset *asset = map.generation.recipe.styleVersion >= 3
                        ? resolveMapStyleProductionAssetForSnapshot(map.generation.recipe.styleSnapshotId,
                              map.atlasLevel, "map_symbol", "discovery", x, y, 1701)
                        : nullptr;
                    if (asset)
                    {
                        const int extent = static_cast<int>(pixelsPerCell * 1.4f);
                        drawRasterAsset(pixels, width, height, cx - extent / 2, cy - extent / 2, cx + extent / 2, cy + extent / 2, asset);
                    }
                    else
                        fillCircle(pixels, width, height, cx, cy, pixelsPerCell / 4, {210, 169, 71, 255});
                }
                else if (map.atlasLevel == MapLevel::Tactical && !gm::map_render::usesNaturalCaveInk(map) && !gm::map_render::usesDungeonMasonryInk(map) &&
                         (cell->kind == MapTileKind::Cover || cell->kind == MapTileKind::Rubble || cell->kind == MapTileKind::Hazard))
                {
                    const char *semantic = cell->kind == MapTileKind::Rubble ? "rubble" : (cell->kind == MapTileKind::Hazard ? "barricade" : "well_log_cover");
                    const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
                        map.generation.recipe.styleSnapshotId, map.atlasLevel, "tactical_prop", semantic, x, y, 1801);
                    if (asset)
                    {
                        const int extent = static_cast<int>(pixelsPerCell * 1.45f);
                        drawRasterAsset(pixels, width, height, cx - extent / 2, cy - extent / 2, cx + extent / 2, cy + extent / 2, asset);
                    }
                }
            }
        }
        }

}
}
