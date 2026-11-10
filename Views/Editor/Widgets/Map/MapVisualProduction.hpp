#pragma once

#include "Views/Editor/Widgets/Map/MapVisualSemantics.hpp"

inline const char *mapProductionTerrainSemanticRole(MapLevel level, MapTileKind kind)
{
    // Forest is a continental biome on a World map. Reuse the accepted macro
    // grass material as subdued paper/ground grain; the close-range
    // forest_floor paintings contain visible leaves and ferns.
    if (level == MapLevel::World && kind == MapTileKind::Forest)
        return "grass_temperate";
    switch (kind)
    {
    case MapTileKind::Water: return "water_shallow";
    case MapTileKind::Dirt: return "earth";
    case MapTileKind::Stone:
    case MapTileKind::Room: return "dressed_stone_floor";
    case MapTileKind::Cave: return "cave_floor";
    case MapTileKind::Forest: return "forest_floor";
    case MapTileKind::Swamp: return "mud";
    case MapTileKind::Road: return "gravel";
    default: return "grass_temperate";
    }
}

// Compatibility for scale-specific preview callers that already exclude
// World forest materials before resolving their semantic role.
inline const char *mapProductionTerrainSemanticRole(MapTileKind kind)
{
    return mapProductionTerrainSemanticRole(MapLevel::Region, kind);
}

inline int mapProductionClusterModulo(MapLevel level, MapTileKind kind)
{
    const MapRenderScaleProfile &profile = mapRenderScaleProfile(level);
    const float spacing = kind == MapTileKind::Forest ? profile.vegetationClusterDistance : profile.mountainClusterDistance;
    // The earlier 0.28 multiplier placed a site-scale tree group on roughly
    // every third forest cell. Multi-cell paintings then overlapped and read as
    // repeated stickers. Treat the declared distance as an actual visual
    // spacing contract; tactical maps remain denser through their own profile.
    return std::max(3, static_cast<int>(std::round(spacing * spacing * 0.82f)));
}

inline float mapProductionAssetZoom(float cellSize)
{
    // Runtime manifests express zoom relative to the editor's 24 px reference
    // cell.  Passing the real screen scale lets world/region overview art hand
    // off to the base atlas when the GM zooms in, as the manifest intended.
    return std::clamp(cellSize / 24.0f, 0.1f, 4.5f);
}

inline bool mapCellIsVisibleDungeonBoundary(const MapModel &map, int x, int y)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell || cell->kind != MapTileKind::Wall)
        return false;
    constexpr int offsets[][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto &offset : offsets)
    {
        const MapCellModel *neighbor = mapCellAt(map, x + offset[0], y + offset[1]);
        if (neighbor && neighbor->kind != MapTileKind::Wall)
            return true;
    }
    return false;
}

inline bool drawProductionAssetRole(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapModel &map,
                                    const std::string &role, const std::string &semanticRole,
                                    int x, int y, int salt = 0, ImU32 tint = IM_COL32_WHITE,
                                    float zoom = 1.0f)
{
    const std::string &snapshotId = map.generation.recipe.styleSnapshotId;
    if (map.generation.recipe.styleVersion < 3 || !mapStyleProductionSnapshotAvailable(snapshotId))
        return false;
    const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
        snapshotId, map.atlasLevel, role, semanticRole, x, y, salt, zoom);
    return asset && drawMapRuntimeAtlasAsset(drawList, min, max, asset->id, false, 0, tint);
}

inline bool drawProductionAssetRolePreservingAspect(ImDrawList *drawList,
                                                     ImVec2 center,
                                                     float nominalExtent,
                                                     const MapModel &map,
                                                     const std::string &role,
                                                     const std::string &semanticRole,
                                                     int x,
                                                     int y,
                                                     int salt = 0,
                                                     ImU32 tint = IM_COL32_WHITE,
                                                     float zoom = 1.0f)
{
    const std::string &snapshotId = map.generation.recipe.styleSnapshotId;
    if (!drawList || nominalExtent <= 0.0f || map.generation.recipe.styleVersion < 3 ||
        !mapStyleProductionSnapshotAvailable(snapshotId))
        return false;
    const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
        snapshotId, map.atlasLevel, role, semanticRole, x, y, salt, zoom);
    if (!asset)
        return false;

    // Stamps are authored as multi-cell paintings.  Fitting every source into
    // a square made wide ridges look like narrow, clipped mountain shards.
    // Preserve the packed image ratio while keeping the previous screen area,
    // so this is a presentation-only change and does not move map semantics.
    float aspect = 1.0f;
    if (asset->width > 0 && asset->height > 0)
        aspect = static_cast<float>(asset->width) / static_cast<float>(asset->height);
    else if (asset->footprintWidth > 0 && asset->footprintHeight > 0)
        aspect = static_cast<float>(asset->footprintWidth) / static_cast<float>(asset->footprintHeight);
    aspect = std::clamp(aspect, 0.5f, 2.5f);
    const float ratioRoot = std::sqrt(aspect);
    const float targetWidth = nominalExtent * ratioRoot;
    const float targetHeight = nominalExtent / ratioRoot;
    return drawMapRuntimeAtlasAsset(
        drawList,
        ImVec2(center.x - targetWidth * 0.5f, center.y - targetHeight * 0.5f),
        ImVec2(center.x + targetWidth * 0.5f, center.y + targetHeight * 0.5f),
        asset->id, false, 0, tint);
}

inline bool drawProductionAssetRoleRotated(ImDrawList *drawList,
                                           ImVec2 center,
                                           float targetSizeX,
                                           float targetSizeY,
                                           float angleRadians,
                                           const MapModel &map,
                                           const std::string &role,
                                           const std::string &semanticRole,
                                           int x,
                                           int y,
                                           int salt = 0,
                                           ImU32 tint = IM_COL32_WHITE,
                                           float zoom = 1.0f)
{
    const std::string &snapshotId = map.generation.recipe.styleSnapshotId;
    if (map.generation.recipe.styleVersion < 3 || !mapStyleProductionSnapshotAvailable(snapshotId))
        return false;
    const MapStylePackedRuntimeAsset *asset = resolveMapStyleProductionAssetForSnapshot(
        snapshotId, map.atlasLevel, role, semanticRole, x, y, salt, zoom);
    return asset && drawMapRuntimeAtlasAssetRotated(
                        drawList, center, targetSizeX, targetSizeY,
                        angleRadians - asset->sourceAxisRadians, asset->id, tint);
}

inline bool mapVisualKindIsLocalStampAnchor(const MapModel &map,
                                            int x,
                                            int y,
                                            MapTileKind kind,
                                            int radius,
                                            int salt)
{
    if (mapVisualGroundKind(map, x, y) != kind)
        return false;
    constexpr int scoreRange = 104729;
    const int centerScore = deterministicMapTileIndex(x, y, salt, scoreRange);
    for (int neighborY = std::max(0, y - radius); neighborY <= std::min(map.height - 1, y + radius); ++neighborY)
        for (int neighborX = std::max(0, x - radius); neighborX <= std::min(map.width - 1, x + radius); ++neighborX)
        {
            if ((neighborX == x && neighborY == y) || mapVisualGroundKind(map, neighborX, neighborY) != kind)
                continue;
            const int neighborScore = deterministicMapTileIndex(neighborX, neighborY, salt, scoreRange);
            if (neighborScore < centerScore ||
                (neighborScore == centerScore && (neighborY < y || (neighborY == y && neighborX < x))))
                return false;
        }
    return true;
}

inline void drawMapProductionOverviewMountainRanges(ImDrawList *drawList,
                                                     ImVec2 origin,
                                                     float cellSize,
                                                     const MapModel &map,
                                                     const MapVisibleCellBounds &visible)
{
    if (!drawList || visible.maxX < visible.minX || visible.maxY < visible.minY)
        return;
    if (gm::map_render::overviewUsesAtlasTerrain(map))
    {
        const auto ink = [](gm::map_render::OverviewRenderColor color) {
            return IM_COL32(color.r, color.g, color.b, color.a);
        };
        if (cellSize >= 5.0f)
        for (const auto &glyph : gm::map_render::buildOverviewForestGlyphs(map))
        {
            const ImVec2 center(origin.x + glyph.center.x * cellSize,
                                origin.y + glyph.center.y * cellSize);
            const float size = glyph.sizeCells * cellSize;
            const float line = std::max(0.7f, cellSize * 0.045f);
            const ImVec2 crowns[] = {{center.x - size * 0.38f, center.y},
                                     {center.x + size * 0.35f, center.y + size * 0.06f},
                                     {center.x, center.y - size * 0.29f}};
            drawList->AddLine(ImVec2(center.x, center.y + size * 0.2f),
                              ImVec2(center.x, center.y + size * 0.64f),
                              ink(gm::map_render::kOverviewForestInk), line);
            for (const ImVec2 &crown : crowns)
                drawList->AddCircleFilled(crown, size * 0.45f + line,
                                          ink(gm::map_render::kOverviewForestInk), 10);
            for (const ImVec2 &crown : crowns)
                drawList->AddCircleFilled(crown, size * 0.45f,
                                          IM_COL32(112, 148, 96, 245), 10);
            drawList->AddCircleFilled(ImVec2(center.x - size * 0.1f, center.y - size * 0.37f),
                                      size * 0.21f, ink(gm::map_render::kOverviewForestHighlight), 10);
        }
        const auto point = [&](const MapFeaturePoint &p) {
            return ImVec2(origin.x + p.x * cellSize, origin.y + p.y * cellSize);
        };
        const std::string ridgeSnapshot = map.generation.recipe.styleSnapshotId.empty()
            ? mapStyleCurrentProductionSnapshotId() : map.generation.recipe.styleSnapshotId;
        const auto ridges = mapStyleProductionSnapshotAvailable(ridgeSnapshot)
            ? std::vector<gm::map_render::OverviewRidgePath>{}
            : gm::map_render::buildOverviewRidgePaths(map);
        for (const auto &ridge : ridges)
        for (const auto &mark : ridge.marks)
        {
            const float line = std::max(0.75f, cellSize * (0.045f + mark.emphasis * 0.018f));
            const auto peak = [&](const MapFeaturePoint &left, const MapFeaturePoint &top,
                                   const MapFeaturePoint &right, const MapFeaturePoint &division,
                                   float scale) {
                drawList->AddTriangleFilled(point(left), point(top), point(division),
                                            IM_COL32(99, 91, 70, 205));
                drawList->AddTriangleFilled(point(division), point(top), point(right),
                                            IM_COL32(196, 183, 144, 230));
                drawList->AddLine(point(left), point(top), ink(gm::map_render::kOverviewRidgeInk), line * scale);
                drawList->AddLine(point(top), point(right), ink(gm::map_render::kOverviewRidgeHighlight), line * scale);
                drawList->AddLine(point(top), point(division), ink(gm::map_render::kOverviewRidgeShadow), line * scale);
            };
            peak(mark.secondaryLeftBase, mark.secondaryPeak, mark.secondaryRightBase, mark.secondaryShadeEnd, 0.82f);
            drawList->AddTriangleFilled(point(mark.leftBase), point(mark.leftShoulder), point(mark.shadeEnd), IM_COL32(99, 91, 70, 205));
            drawList->AddTriangleFilled(point(mark.leftShoulder), point(mark.peak), point(mark.shadeEnd), IM_COL32(99, 91, 70, 205));
            drawList->AddTriangleFilled(point(mark.shadeEnd), point(mark.peak), point(mark.rightShoulder), IM_COL32(196, 183, 144, 230));
            drawList->AddTriangleFilled(point(mark.shadeEnd), point(mark.rightShoulder), point(mark.rightBase), IM_COL32(196, 183, 144, 230));
            drawList->AddLine(point(mark.leftBase), point(mark.leftShoulder), ink(gm::map_render::kOverviewRidgeInk), line);
            drawList->AddLine(point(mark.leftShoulder), point(mark.peak), ink(gm::map_render::kOverviewRidgeInk), line);
            drawList->AddLine(point(mark.peak), point(mark.rightShoulder), ink(gm::map_render::kOverviewRidgeHighlight), line);
            drawList->AddLine(point(mark.rightShoulder), point(mark.rightBase), ink(gm::map_render::kOverviewRidgeHighlight), line);
            drawList->AddLine(point(mark.peak), point(mark.shadeEnd), ink(gm::map_render::kOverviewRidgeShadow), line);
            drawList->AddLine(point(mark.leftShoulder), point(mark.shadeEnd), ink(gm::map_render::kOverviewRidgeShadow), line * 0.65f);
        }
        // Cartographic world relief uses the geographic spine. Region views
        // retain their scale-specific paintings; saved map cells never change.
        if (!ridges.empty())
            return;
    }

    const std::string &v32SnapshotId = mapStyleV32ProductionSnapshotId();
    const std::string &v33SnapshotId = mapStyleV33ProductionSnapshotId();
    const std::string &requestedSnapshotId = map.generation.recipe.styleSnapshotId;
    const std::string resolvedSnapshotId = requestedSnapshotId.empty()
                                               ? mapStyleCurrentProductionSnapshotId()
                                               : requestedSnapshotId;
    const bool usesV32Snapshot = !v32SnapshotId.empty() &&
                                 resolvedSnapshotId == v32SnapshotId;
    const bool usesV33Snapshot = !v33SnapshotId.empty() &&
                                 resolvedSnapshotId == v33SnapshotId;
    const bool overviewLevel = map.atlasLevel == MapLevel::World ||
                               map.atlasLevel == MapLevel::Region;
    if ((!usesV32Snapshot && !usesV33Snapshot) || !overviewLevel ||
        map.generation.recipe.styleVersion < 3 ||
        !mapStyleProductionSnapshotAvailable(resolvedSnapshotId) ||
        visible.maxX < visible.minX || visible.maxY < visible.minY)
        return;

    // The old local-minimum rule stamped one detailed painting for every few
    // mountain cells.  At fit-to-window zoom that became a pile of short,
    // disconnected "worms" over horizontal relief hatching.  Derive a small,
    // deterministic set from connected range sections instead.  This keeps the
    // accepted art, but uses it at the macro scale it was authored for.
    const std::vector<gm::map_render::OverviewMountainStamp> stamps =
        gm::map_render::buildOverviewMountainStamps(map);
    for (const gm::map_render::OverviewMountainStamp &stamp : stamps)
    {
        const float cullRadius = std::max(stamp.lengthCells, stamp.widthCells) * cellSize * 0.65f;
        const ImVec2 center(origin.x + stamp.centerX * cellSize,
                            origin.y + stamp.centerY * cellSize);
        const ImVec2 clipMin = drawList->GetClipRectMin();
        const ImVec2 clipMax = drawList->GetClipRectMax();
        if (center.x + cullRadius < clipMin.x || center.x - cullRadius > clipMax.x ||
            center.y + cullRadius < clipMin.y || center.y - cullRadius > clipMax.y)
            continue;
        drawProductionAssetRoleRotated(
            drawList, center,
            cellSize * stamp.lengthCells * gm::map_render::kOverviewMountainLengthScale,
            cellSize * stamp.widthCells *
                (usesV33Snapshot
                     ? gm::map_render::kOverviewV33MountainWidthScale
                     : gm::map_render::kOverviewMountainWidthScale),
            stamp.angleRadians,
            map, "landscape_stamp", "connected_ridge",
            stamp.anchorX, stamp.anchorY, 2421 + stamp.sectionIndex,
            usesV33Snapshot
                ? IM_COL32(gm::map_render::kOverviewV33MountainTint.r,
                           gm::map_render::kOverviewV33MountainTint.g,
                           gm::map_render::kOverviewV33MountainTint.b,
                           gm::map_render::kOverviewV33MountainTint.a)
                : IM_COL32(gm::map_render::kOverviewMountainTint.r,
                           gm::map_render::kOverviewMountainTint.g,
                           gm::map_render::kOverviewMountainTint.b,
                           gm::map_render::kOverviewMountainTint.a),
            mapProductionAssetZoom(cellSize));
    }
}
