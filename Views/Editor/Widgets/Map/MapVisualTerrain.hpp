#pragma once

#include "Views/Editor/Widgets/Map/MapVisualBiomeBlend.hpp"

inline void drawMapContinuousTerrainBase(ImDrawList *drawList, ImVec2 origin, float cellSize, const MapModel &map,
    const std::vector<gm::map_render::AtlasInkPrimitive> *resolvedAtlasInk = nullptr,
    const std::vector<gm::map_render::AtlasInkPrimitive> *resolvedAtlasWaterInk = nullptr)
{
    if (!map.generation.continuousRendering && map.generation.graph.features.empty())
        return;
    const ImVec2 mapMax(origin.x + map.width * cellSize, origin.y + map.height * cellSize);
    drawList->AddRectFilled(origin, mapMax, IM_COL32(74, 96, 53, 255));
    const MapVisibleCellBounds visible = mapVisibleCellBounds(drawList, origin, cellSize, map);
    const bool illustrated = gm::map_render::usesIllustratedAtlas(map);
    const bool productionAssets = !illustrated && map.generation.recipe.styleVersion >= 3 &&
                                  mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId);
    const bool detailedTerrainMaterials = productionAssets &&
                                          mapRenderUsesDetailedTerrainMaterials(map.atlasLevel, cellSize);
    MapAtlasTerrainCache *terrainCache = gm::map_render::overviewUsesAtlasTerrain(map)
        ? &mapAtlasTerrainCache(map) : nullptr;
    const std::vector<MapTileKind> emptyGround;
    const auto &overviewGroundProjection = terrainCache ? terrainCache->ground : emptyGround;
    const auto renderedGroundKind = [&](int x, int y) {
        return gm::map_render::overviewUsesAtlasTerrain(map)
                   ? gm::map_render::overviewGroundKindFromProjection(
                         map, overviewGroundProjection, x, y)
                   : mapVisualGroundKind(map, x, y);
    };
    const MapRenderScaleProfile &scaleProfile = mapRenderScaleProfile(map.atlasLevel);
    const int materialBlockSize = std::max(1, static_cast<int>(std::round(scaleProfile.materialCellsPerRepeat)));
    const float assetZoom = mapProductionAssetZoom(cellSize);
    const int circleSegments = cellSize < 18.0f ? 8 : 12;
    // Candidate strips used to submit one gradient primitive per semantic cell
    // for all three thumbnails on every frame. At overview zoom that work is
    // invisible and made 80x80 design selection feel much heavier than the
    // actual map. Keep the same renderer and RenderPlan, but rasterize its
    // terrain field at a deterministic LOD. Lines, labels, symbols and exact
    // cells remain untouched and the full-resolution editor automatically
    // returns to stride one when zoomed in.
    const int terrainLodStride = map.scope == MapScope::Dungeon || map.atlasLevel == MapLevel::Tactical
                                     ? 1
                                     : (cellSize < 2.75f ? 3 : (cellSize < 6.5f ? 2 : 1));
    const int firstTerrainX = std::max(0, (visible.minX / terrainLodStride) * terrainLodStride);
    const int firstTerrainY = std::max(0, (visible.minY / terrainLodStride) * terrainLodStride);
    // Coast cells need sub-cell samples: interpolating only four integer
    // corners blurred a sharp coastline across an entire gameplay cell.
    // Cache shared vertices for this draw and keep inland terrain coarse.
    constexpr int cachedCoarseSubdivisions = 4;
    const int coastSubdivisions = illustrated ? mapAtlasCoastSubdivisions(cellSize) : cachedCoarseSubdivisions;
    // Refine only visible coast cells. A dense full-map32x cache can consume
    // hundreds of megabytes; sparse colors keep panning/zoom bounded.
    if (terrainCache && (terrainCache->fineSubdivisions!=coastSubdivisions || terrainCache->fineColors.size()>250000)) {
        terrainCache->fineSubdivisions=coastSubdivisions;terrainCache->fineColors.clear();
    }
    const int sampleWidth = terrainCache && map.width <= 512 && map.height <= 512
        ? map.width * cachedCoarseSubdivisions + 1 : 0;
    std::vector<ImU32> emptyColors;
    auto &worldVertexColors = terrainCache ? terrainCache->colors : emptyColors;
    const std::size_t colorCount = static_cast<std::size_t>(sampleWidth) *
        (terrainCache ? map.height * cachedCoarseSubdivisions + 1 : 0);
    if (worldVertexColors.size() != colorCount)
        worldVertexColors.assign(colorCount, 0u);
    const auto terrainColorAt = [&](float x, float y) {
        if (terrainCache && coastSubdivisions>cachedCoarseSubdivisions) {
            const auto sx=static_cast<std::uint32_t>(std::lround(x*coastSubdivisions));
            const auto sy=static_cast<std::uint32_t>(std::lround(y*coastSubdivisions));
            const auto key=(static_cast<std::uint64_t>(sx)<<32)|sy;
            const auto [entry,inserted]=terrainCache->fineColors.try_emplace(key,0u);
            if(inserted) entry->second=mapAverageContinuousTerrainVertexColor(map,x,y,&overviewGroundProjection);
            return entry->second;
        }
        if (sampleWidth == 0)
            return mapAverageContinuousTerrainVertexColor(map, x, y, &overviewGroundProjection);
        const int sx = std::clamp(static_cast<int>(std::lround(x * cachedCoarseSubdivisions)), 0, map.width * cachedCoarseSubdivisions);
        const int sy = std::clamp(static_cast<int>(std::lround(y * cachedCoarseSubdivisions)), 0, map.height * cachedCoarseSubdivisions);
        ImU32 &cached = worldVertexColors[static_cast<std::size_t>(sy * sampleWidth + sx)];
        if (cached == 0u)
            cached = mapAverageContinuousTerrainVertexColor(map, x, y, &overviewGroundProjection);
        return cached;
    };
    for (int y = firstTerrainY; y <= visible.maxY; y += terrainLodStride)
    {
        for (int x = firstTerrainX; x <= visible.maxX; x += terrainLodStride)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;
            const MapTileKind visualKind = renderedGroundKind(x, y);
            const int variation = deterministicMapTileIndex(x / 3, y / 3,
                static_cast<int>(visualKind) * 37 + static_cast<int>(map.generation.recipe.seed & 0x7fffu), 5);
            const ImVec2 tileMin(origin.x + x * cellSize, origin.y + y * cellSize);
            const int endX = std::min(map.width, x + terrainLodStride);
            const int endY = std::min(map.height, y + terrainLodStride);
            const ImVec2 tileMax(origin.x + endX * cellSize + 0.5f,
                                 origin.y + endY * cellSize + 0.5f);
            if (map.scope == MapScope::Dungeon)
            {
                // Version 11 dungeon void matches the untextured export ground.
                // Per-cell variation here made large checkerboards around rooms.
                const bool masonryVoid = gm::map_render::usesDungeonMasonryInk(map) &&
                    (visualKind == MapTileKind::Wall || visualKind == MapTileKind::Empty);
                drawList->AddRectFilled(tileMin, tileMax, masonryVoid
                    ? IM_COL32(29, 31, 33, 255) : mapContinuousTerrainColor(visualKind, variation));
            }
            else
            {
                bool coast = false;
                if (gm::map_render::overviewUsesAtlasTerrain(map) && cellSize >= 6.5f)
                {
                    const bool land = gm::map_render::overviewGroundKindIsLand(visualKind);
                    for (int ny = std::max(0, y - 2); ny <= std::min(map.height - 1, y + 2) && !coast; ++ny)
                        for (int nx = std::max(0, x - 2); nx <= std::min(map.width - 1, x + 2); ++nx)
                            if (gm::map_render::overviewGroundKindIsLand(renderedGroundKind(nx, ny)) != land)
                            {
                                coast = true;
                                break;
                            }
                }
                const int divisions = coast ? coastSubdivisions : 1;
                for (int sy = 0; sy < divisions; ++sy)
                    for (int sx = 0; sx < divisions; ++sx)
                    {
                        const float ax = x + static_cast<float>(endX - x) * sx / divisions;
                        const float ay = y + static_cast<float>(endY - y) * sy / divisions;
                        const float bx = x + static_cast<float>(endX - x) * (sx + 1) / divisions;
                        const float by = y + static_cast<float>(endY - y) * (sy + 1) / divisions;
                        drawList->AddRectFilledMultiColor(
                            ImVec2(origin.x + ax * cellSize, origin.y + ay * cellSize),
                            ImVec2(origin.x + bx * cellSize, origin.y + by * cellSize),
                            terrainColorAt(ax, ay), terrainColorAt(bx, ay),
                            terrainColorAt(bx, by), terrainColorAt(ax, by));
                    }
            }
            if (gm::map_render::usesIllustratedLocalV6(map)) continue;
            if (terrainLodStride == 1 && detailedTerrainMaterials &&
                visualKind != MapTileKind::Wall && visualKind != MapTileKind::Mountain &&
                !(gm::map_render::usesDungeonMasonryInk(map) && visualKind == MapTileKind::Empty))
            {
                const int repeatX = x / materialBlockSize;
                const int repeatY = y / materialBlockSize;
                const int localX = x % materialBlockSize;
                const int localY = y % materialBlockSize;
                const bool mirrorX = (repeatX & 1) != 0;
                const bool mirrorY = (repeatY & 1) != 0;
                const float inverseRepeat = 1.0f / static_cast<float>(materialBlockSize);
                const float rawU0 = static_cast<float>(localX) * inverseRepeat;
                const float rawV0 = static_cast<float>(localY) * inverseRepeat;
                const float rawU1 = static_cast<float>(localX + 1) * inverseRepeat;
                const float rawV1 = static_cast<float>(localY + 1) * inverseRepeat;
                const float sectionU0 = mirrorX ? 1.0f - rawU1 : rawU0;
                const float sectionU1 = mirrorX ? 1.0f - rawU0 : rawU1;
                const float sectionV0 = mirrorY ? 1.0f - rawV1 : rawV0;
                const float sectionV1 = mirrorY ? 1.0f - rawV0 : rawV1;
                const int sectorSpan = materialBlockSize * 3;
                const int sectorX = x / sectorSpan;
                const int sectorY = y / sectorSpan;
                const int materialSalt = static_cast<int>(visualKind) * 97 +
                                         static_cast<int>(map.generation.recipe.seed & 0x7fffu);
                // Every local terrain painting carries tactical-scale detail.
                // World maps receive one global painterly surface below, so
                // no repeated material (water included) is sampled here.
                const bool worldSurface = gm::map_render::overviewUsesAtlasTerrain(map);
                const MapStylePackedRuntimeAsset *material = worldSurface
                                                                 ? nullptr
                                                                 : resolveMapStyleProductionAssetForSnapshot(
                                                                       map.generation.recipe.styleSnapshotId, map.atlasLevel, "terrain_material",
                                                                       mapProductionTerrainSemanticRole(map.atlasLevel, visualKind), sectorX, sectorY, materialSalt, assetZoom);
                if (material)
                {
                    // Match the CPU export's 38% material mix (52% for water)
                    // instead of the old barely visible 30/255 debug wash.
                    const int alpha = visualKind == MapTileKind::Water ? 133 :
                                      (map.scope == MapScope::Dungeon ? 122 : 97);
                    const ImU32 materialTint = IM_COL32(255, 255, 255, alpha);
                    drawMapRuntimeAtlasAssetSection(
                        drawList, tileMin, tileMax, material->id,
                        sectionU0, sectionV0, sectionU1, sectionV1,
                        materialTint);
                }
            }
            if (cell->kind == MapTileKind::Wall && !mapCellIsVisibleDungeonBoundary(map, x, y))
                continue;
            const float jitterX = static_cast<float>((variation % 3) - 1) * cellSize * 0.09f;
            const float jitterY = static_cast<float>(((variation / 3) % 3) - 1) * cellSize * 0.09f;
            const ImVec2 center(origin.x + (x + 0.5f) * cellSize + jitterX, origin.y + (y + 0.5f) * cellSize + jitterY);
            const ImU32 color = mapContinuousTerrainColor(visualKind, variation);
            if (!productionAssets && !gm::map_render::overviewUsesAtlasTerrain(map) &&
                !gm::map_render::usesDungeonMasonryInk(map))
                drawList->AddCircleFilled(center, cellSize * 0.72f, color, circleSegments);
            if (cell->kind == MapTileKind::Forest)
            {
                // World forest is always a continuous cartographic biome,
                // including legacy/no-snapshot maps. Never fall back to the
                // old per-cell canopy circles at continental scale.
                if (gm::map_render::overviewUsesAtlasTerrain(map))
                    continue;
                if (productionAssets)
                {
                    const int modulo = mapProductionClusterModulo(map.atlasLevel, cell->kind);
                    if (!gm::map_render::usesSettlementSiteInk(map) && deterministicMapTileIndex(x, y, 1501, modulo) == 0)
                    {
                        const float extent = cellSize * 2.1f;
                        const bool drawn=drawProductionAssetRolePreservingAspect(
                            drawList, center, extent, map, "vegetation_stamp",
                            "forest_edge",
                            x, y, variation, IM_COL32_WHITE, assetZoom);
                        // Packed vegetation has no low-zoom atlas entry. A
                        // fit-to-window tactical map must not lose all tree
                        // symbols while the PNG keeps its default-zoom art.
                        if(!drawn && map.generation.recipe.generatorVersion>=11 &&
                           map.generation.recipe.creationIntent==MapCreationIntent::TacticalEncounter)
                            drawProductionAssetRolePreservingAspect(drawList,center,extent,map,
                                "vegetation_stamp","forest_edge",x,y,variation,IM_COL32_WHITE,1.f);
                    }
                    continue;
                }
                const ImU32 canopy = variation % 2 == 0 ? IM_COL32(43, 91, 49, 225) : IM_COL32(55, 105, 54, 225);
                drawList->AddCircleFilled(ImVec2(center.x - cellSize * 0.18f, center.y + cellSize * 0.05f), cellSize * 0.28f, canopy, circleSegments);
                drawList->AddCircleFilled(ImVec2(center.x + cellSize * 0.16f, center.y - cellSize * 0.10f), cellSize * 0.33f, canopy, circleSegments);
                drawList->AddCircleFilled(ImVec2(center.x + cellSize * 0.03f, center.y + cellSize * 0.18f), cellSize * 0.25f, IM_COL32(36, 72, 42, 230), circleSegments);
            }
            else if (cell->kind == MapTileKind::Mountain)
            {
                if (productionAssets || illustrated)
                {
                    // World/region range stamps are drawn once per connected
                    // visual cluster after the terrain pass. Never fall back to
                    // one tactical-looking mountain illustration per cell.
                    continue;
                }
                const ImVec2 top(center.x + cellSize * 0.08f, center.y - cellSize * 0.48f);
                const ImVec2 left(center.x - cellSize * 0.43f, center.y + cellSize * 0.38f);
                const ImVec2 right(center.x + cellSize * 0.48f, center.y + cellSize * 0.38f);
                drawList->AddTriangleFilled(top, left, right, IM_COL32(89, 83, 73, 255));
                drawList->AddTriangleFilled(top, ImVec2(center.x + cellSize * 0.06f, center.y + cellSize * 0.38f), right, IM_COL32(119, 111, 91, 255));
                drawList->AddTriangleFilled(top, ImVec2(center.x - cellSize * 0.08f, center.y - cellSize * 0.16f), ImVec2(center.x + cellSize * 0.17f, center.y - cellSize * 0.12f), IM_COL32(207, 205, 187, 215));
            }
            else if (cell->kind == MapTileKind::Hill)
            {
                if (map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical)
                    drawList->AddBezierCubic(ImVec2(center.x - cellSize * 0.42f, center.y + cellSize * 0.20f),
                                             ImVec2(center.x - cellSize * 0.12f, center.y - cellSize * 0.23f),
                                             ImVec2(center.x + cellSize * 0.18f, center.y - cellSize * 0.18f),
                                             ImVec2(center.x + cellSize * 0.43f, center.y + cellSize * 0.20f), IM_COL32(153, 145, 83, 180), 1.4f, 10);
            }
            else if (productionAssets && cell->kind == MapTileKind::Wall &&
                     !gm::map_render::usesNaturalCaveInk(map) && !gm::map_render::usesDungeonMasonryInk(map))
            {
                const float extent = cellSize * 1.15f;
                drawProductionAssetRole(drawList, ImVec2(center.x - extent * 0.5f, center.y - extent * 0.5f),
                                        ImVec2(center.x + extent * 0.5f, center.y + extent * 0.5f), map,
                                        "dungeon_stamp", "dungeon_architecture", x, y, variation,
                                        IM_COL32_WHITE, assetZoom);
            }
            }
        }
    if (gm::map_render::usesIllustratedLocalV6(map)) {
        const auto clipMin=drawList->GetClipRectMin(),clipMax=drawList->GetClipRectMax();
        const bool clipped=origin.x<clipMin.x || origin.y<clipMin.y ||
            origin.x+map.width*cellSize>clipMax.x || origin.y+map.height*cellSize>clipMax.y;
        if (resolvedAtlasInk) drawMapIllustratedAtlasInk(drawList,origin,cellSize,*resolvedAtlasInk,clipped);
        else drawMapIllustratedAtlasInk(drawList,origin,cellSize,gm::map_render::buildLocalIllustrationInk(map,false),clipped);
        if (resolvedAtlasWaterInk) drawMapIllustratedAtlasInk(drawList,origin,cellSize,*resolvedAtlasWaterInk);
        else drawMapIllustratedAtlasInk(drawList,origin,cellSize,gm::map_render::buildLocalIllustrationWaterInk(map,false));
        return;
    }
    if (illustrated)
    {
        if (resolvedAtlasInk)
            drawMapIllustratedAtlasInk(drawList, origin, cellSize, *resolvedAtlasInk);
        else
            drawMapIllustratedAtlasInk(drawList, origin, cellSize,
                gm::map_render::buildIllustratedAtlasInk(map, gm::map_render::illustratedAtlasRoadCorridors(map)));
        if (gm::map_render::usesIllustratedAtlasDetailed(map))
        {
            if (resolvedAtlasWaterInk)
                drawMapIllustratedAtlasInk(drawList, origin, cellSize, *resolvedAtlasWaterInk);
            else
                drawMapIllustratedAtlasInk(drawList, origin, cellSize,
                    gm::map_render::buildIllustratedAtlasWaterInk(map));
        }
        else
            drawMapOverviewCoastTreatment(drawList, origin, cellSize, map);
        return;
    }
    if (gm::map_render::usesDungeonMasonryInk(map)) {
        if (resolvedAtlasInk) drawMapIllustratedAtlasInk(drawList, origin, cellSize, *resolvedAtlasInk);
        else drawMapIllustratedAtlasInk(drawList, origin, cellSize, gm::map_render::buildDungeonFloorInk(map, false));
        drawMapDungeonMasonryBoundary(drawList, origin, cellSize, map);
    }
    if (gm::map_render::usesNaturalCaveInk(map)) {
        if (resolvedAtlasInk) drawMapIllustratedAtlasInk(drawList, origin, cellSize, *resolvedAtlasInk);
        else drawMapIllustratedAtlasInk(drawList, origin, cellSize, gm::map_render::buildNaturalCaveFloorInk(map, false));
    }
    if (gm::map_render::usesSettlementSiteInk(map)) {
        if (resolvedAtlasInk) drawMapIllustratedAtlasInk(drawList, origin, cellSize, *resolvedAtlasInk);
        else drawMapIllustratedAtlasInk(drawList, origin, cellSize, gm::map_render::buildSettlementWoodlandInk(map));
    }
    drawWorldPainterlySurface(drawList, origin, cellSize, map);
    // Production mountain paintings already carry their own relief. Drawing a
    // Bezier contour for every raster row underneath them created the large
    // horizontal hatched rectangles visible in World previews.
    if (!productionAssets)
        drawMapOverviewRelief(drawList, origin, cellSize, map, visible);
    drawMapProductionOverviewMountainRanges(drawList, origin, cellSize, map, visible);
    drawMapOverviewCoastTreatment(drawList, origin, cellSize, map);
    // Continuous terrain interpolation already owns biome transitions.  The
    // current "organic_blend_mask" sources are baked colour swatches, not
    // neutral alpha masks; stamping them here produced the repeated pale
    // circles visible in both editor and export.
}
