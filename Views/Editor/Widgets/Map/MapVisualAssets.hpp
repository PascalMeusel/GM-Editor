#pragma once

#include "Views/Editor/Widgets/Map/MapVisualTiles.hpp"

inline int automaticDirectionalRotation(const MapTileRenderContext &context)
{
    return automaticMapTileRotation(context);
}

inline void drawAtlasLayer(ImDrawList *drawList, ImVec2 min, ImVec2 max, ImTextureID textureId, ImVec2 uv0, ImVec2 uv1, int rotation, ImU32 tint = IM_COL32_WHITE)
{
    if (!textureId)
        return;

    const ImVec2 corners[4] = {
        ImVec2(min.x, min.y),
        ImVec2(max.x, min.y),
        ImVec2(max.x, max.y),
        ImVec2(min.x, max.y)};
    const ImVec2 uvs[4] = {
        ImVec2(uv0.x, uv0.y),
        ImVec2(uv1.x, uv0.y),
        ImVec2(uv1.x, uv1.y),
        ImVec2(uv0.x, uv1.y)};
    drawList->AddImageQuad(
        textureId,
        corners[0],
        corners[1],
        corners[2],
        corners[3],
        uvs[(0 + rotation) % 4],
        uvs[(1 + rotation) % 4],
        uvs[(2 + rotation) % 4],
        uvs[(3 + rotation) % 4],
        tint);
}

inline void drawMapTile(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapTileRenderContext &context, bool showCode);
inline void drawMapTile(ImDrawList *drawList, ImVec2 min, ImVec2 max, MapTileKind kind, int rotationQuarterTurns, bool showCode, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY);

inline void drawMapTile(ImDrawList *drawList, ImVec2 min, ImVec2 max, MapTileKind kind, int rotationQuarterTurns, bool showCode)
{
    drawMapTile(drawList, min, max, makeMapTileRenderContext(kind, rotationQuarterTurns, 0, 0), showCode);
}

inline void drawMapTile(ImDrawList *drawList, ImVec2 min, ImVec2 max, MapTileKind kind, int rotationQuarterTurns, bool showCode, bool sameNorth, bool sameEast, bool sameSouth, bool sameWest, int tileX, int tileY)
{
    MapTileRenderContext context = makeMapTileRenderContext(kind, rotationQuarterTurns, tileX, tileY);
    auto setSame = [&context, kind](MapTileNeighborSlot slot, bool same) {
        MapTileNeighborInfo &neighbor = context.neighbors[static_cast<size_t>(slot)];
        neighbor.exists = same;
        neighbor.kind = kind;
    };
    setSame(MapTileNeighborSlot::North, sameNorth);
    setSame(MapTileNeighborSlot::East, sameEast);
    setSame(MapTileNeighborSlot::South, sameSouth);
    setSame(MapTileNeighborSlot::West, sameWest);
    drawMapTile(drawList, min, max, context, showCode);
}

inline void drawMapTile(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapTileRenderContext &context, bool showCode)
{
    if(!drawList)return;
    (void)showCode; // Names belong in the palette/tooltip, not on every empty cell.
    const float size=std::min(max.x-min.x,max.y-min.y);
    const ImVec2 origin(min.x-context.tileX*size,min.y-context.tileY*size);
    drawMapIllustratedAtlasInk(drawList,origin,size,gm::map_render::classicCellInk(context));
    if(context.kind==MapTileKind::Road||context.kind==MapTileKind::Bridge) {
        MapWayRenderContext way;way.terrainKind=context.kind;way.tileX=context.tileX;way.tileY=context.tileY;
        drawMapIllustratedAtlasInk(drawList,origin,size,gm::map_render::classicWayInk(way,context));
    }
}

inline ImU32 mapWayColor(MapWayKind kind, bool bridge)
{
    if (bridge)
        return IM_COL32(132, 91, 51, 245);

    switch (kind)
    {
    case MapWayKind::Trail:
        return IM_COL32(174, 139, 89, 220);
    case MapWayKind::DirtRoad:
        return IM_COL32(190, 151, 88, 235);
    case MapWayKind::StoneRoad:
        return IM_COL32(156, 154, 143, 235);
    case MapWayKind::River:
        return IM_COL32(50, 135, 169, 225);
    case MapWayKind::Canal:
        return IM_COL32(66, 128, 158, 230);
    case MapWayKind::DungeonCorridor:
        return IM_COL32(102, 96, 88, 230);
    case MapWayKind::CaveTunnel:
        return IM_COL32(92, 82, 70, 225);
    case MapWayKind::None:
        break;
    }
    return IM_COL32(0, 0, 0, 0);
}

inline float mapWayThickness(MapWayKind kind, float cellSize)
{
    switch (kind)
    {
    case MapWayKind::Trail:
        return std::max(2.0f, cellSize * 0.12f);
    case MapWayKind::River:
    case MapWayKind::Canal:
        return std::max(3.0f, cellSize * 0.24f);
    case MapWayKind::DungeonCorridor:
    case MapWayKind::CaveTunnel:
        return std::max(3.0f, cellSize * 0.22f);
    case MapWayKind::DirtRoad:
    case MapWayKind::StoneRoad:
        return std::max(3.0f, cellSize * 0.18f);
    case MapWayKind::None:
        break;
    }
    return std::max(2.0f, cellSize * 0.14f);
}

inline void drawMapWayOverlay(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapWayRenderContext &context)
{
    if (context.wayKind == MapWayKind::None)
        return;

    const float cellSize = std::min(max.x - min.x, max.y - min.y);
    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const bool bridge = mapWayRendersAsBridge(context);
    const ImU32 color = mapWayColor(context.wayKind, bridge);
    const float thickness = mapWayThickness(context.wayKind, cellSize);
    const int mask = mapWayEightNeighborMask(context);

    const ImU32 edgeColor = bridge ? IM_COL32(68, 45, 28, 235) : IM_COL32(20, 24, 26, 85);
    auto drawSegment = [&](float dx, float dy) {
        const ImVec2 end(center.x + dx * cellSize * 0.52f, center.y + dy * cellSize * 0.52f);
        drawList->AddLine(center, end, edgeColor, thickness + 2.0f);
        drawList->AddLine(center, end, color, thickness);
    };

    if (mask == 0)
    {
        drawList->AddCircleFilled(center, thickness * 0.75f, edgeColor, 18);
        drawList->AddCircleFilled(center, thickness * 0.55f, color, 18);
        return;
    }

    if ((mask & 1) != 0)
        drawSegment(0.0f, -1.0f);
    if ((mask & 2) != 0)
        drawSegment(1.0f, -1.0f);
    if ((mask & 4) != 0)
        drawSegment(1.0f, 0.0f);
    if ((mask & 8) != 0)
        drawSegment(1.0f, 1.0f);
    if ((mask & 16) != 0)
        drawSegment(0.0f, 1.0f);
    if ((mask & 32) != 0)
        drawSegment(-1.0f, 1.0f);
    if ((mask & 64) != 0)
        drawSegment(-1.0f, 0.0f);
    if ((mask & 128) != 0)
        drawSegment(-1.0f, -1.0f);

    drawList->AddCircleFilled(center, thickness * 0.62f, color, 18);
    if (bridge)
    {
        drawList->AddLine(ImVec2(center.x - cellSize * 0.16f, center.y - cellSize * 0.16f), ImVec2(center.x + cellSize * 0.16f, center.y + cellSize * 0.16f), IM_COL32(226, 174, 96, 200), 1.2f);
        drawList->AddLine(ImVec2(center.x - cellSize * 0.16f, center.y + cellSize * 0.16f), ImVec2(center.x + cellSize * 0.16f, center.y - cellSize * 0.16f), IM_COL32(226, 174, 96, 200), 1.2f);
    }
}

inline bool drawMapIntegratedWayPrototype(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapWayRenderContext &context)
{
    if (!hasIntegratedWayPrototypeTile(context))
        return false;

    ImVec2 uv0;
    ImVec2 uv1;
    getIntegratedWayPrototypeUV(context, uv0, uv1);
    drawAtlasLayer(drawList, min, max, getIntegratedWayPrototypeTextureId(), uv0, uv1, 0);
    return true;
}

inline bool drawMapRuntimeAtlasAsset(ImDrawList *drawList, ImVec2 min, ImVec2 max, const std::string &assetId, bool allowCandidatePreview = false, int rotationQuarterTurns = 0, ImU32 tint = IM_COL32_WHITE)
{
    if (!drawList || assetId.empty())
        return false;

    MapRuntimeAtlasAssetDrawData data;
    if (!getRuntimeAtlasAssetDrawData(assetId, data, allowCandidatePreview))
        return false;
    drawAtlasLayer(drawList, min, max, data.textureId, data.uv0, data.uv1, normalizeMapRotation(rotationQuarterTurns), tint);
    return true;
}

// Draw one cell-sized window from a larger material painting.  Reusing the
// same source across an entire scale-profile repeat keeps the texture visible
// without shrinking a complete 512 px painting into every semantic cell.
inline bool drawMapRuntimeAtlasAssetSection(ImDrawList *drawList,
                                            ImVec2 min,
                                            ImVec2 max,
                                            const std::string &assetId,
                                            float sectionU0,
                                            float sectionV0,
                                            float sectionU1,
                                            float sectionV1,
                                            ImU32 tint = IM_COL32_WHITE)
{
    if (!drawList || assetId.empty())
        return false;

    MapRuntimeAtlasAssetDrawData data;
    if (!getRuntimeAtlasAssetDrawData(assetId, data, false))
        return false;

    sectionU0 = std::clamp(sectionU0, 0.0f, 1.0f);
    sectionV0 = std::clamp(sectionV0, 0.0f, 1.0f);
    sectionU1 = std::clamp(sectionU1, sectionU0, 1.0f);
    sectionV1 = std::clamp(sectionV1, sectionV0, 1.0f);
    // Accepted material paintings retain a small presentation frame. The CPU
    // renderer already samples an inset of 1/24; sampling the full packed UV
    // here made the same frame reappear as dark block seams in the Editor.
    // Apply the inset in asset-local coordinates before mapping into the atlas.
    constexpr float safeInset = 1.0f / 24.0f;
    const auto insetSection = [safeInset](float amount) {
        return safeInset + amount * (1.0f - safeInset * 2.0f);
    };
    sectionU0 = insetSection(sectionU0);
    sectionV0 = insetSection(sectionV0);
    sectionU1 = insetSection(sectionU1);
    sectionV1 = insetSection(sectionV1);
    const auto interpolate = [](float first, float second, float amount) {
        return first + (second - first) * amount;
    };
    drawList->AddImage(
        data.textureId, min, max,
        ImVec2(interpolate(data.uv0.x, data.uv1.x, sectionU0),
               interpolate(data.uv0.y, data.uv1.y, sectionV0)),
        ImVec2(interpolate(data.uv0.x, data.uv1.x, sectionU1),
               interpolate(data.uv0.y, data.uv1.y, sectionV1)),
        tint);
    return true;
}

// Draw a packed runtime asset at an arbitrary angle without creating a
// separate rotated texture.  Returning true means that a production asset was
// available, even when its quad was outside the current clip rectangle.  This
// distinction keeps callers from drawing a procedural fallback for culled
// assets.
inline bool drawMapRuntimeAtlasAssetRotated(ImDrawList *drawList,
                                            ImVec2 center,
                                            float targetSizeX,
                                            float targetSizeY,
                                            float angleRadians,
                                            const std::string &assetId,
                                            ImU32 tint = IM_COL32_WHITE)
{
    if (!drawList || assetId.empty() || targetSizeX <= 0.0f || targetSizeY <= 0.0f)
        return false;

    MapRuntimeAtlasAssetDrawData data;
    if (!getRuntimeAtlasAssetDrawData(assetId, data, false))
        return false;

    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    const ImVec2 axisX(cosine * targetSizeX * 0.5f, sine * targetSizeX * 0.5f);
    const ImVec2 axisY(-sine * targetSizeY * 0.5f, cosine * targetSizeY * 0.5f);
    const ImVec2 corners[4] = {
        ImVec2(center.x - axisX.x - axisY.x, center.y - axisX.y - axisY.y),
        ImVec2(center.x + axisX.x - axisY.x, center.y + axisX.y - axisY.y),
        ImVec2(center.x + axisX.x + axisY.x, center.y + axisX.y + axisY.y),
        ImVec2(center.x - axisX.x + axisY.x, center.y - axisX.y + axisY.y)};

    float minimumX = corners[0].x;
    float maximumX = corners[0].x;
    float minimumY = corners[0].y;
    float maximumY = corners[0].y;
    for (int corner = 1; corner < 4; ++corner)
    {
        minimumX = std::min(minimumX, corners[corner].x);
        maximumX = std::max(maximumX, corners[corner].x);
        minimumY = std::min(minimumY, corners[corner].y);
        maximumY = std::max(maximumY, corners[corner].y);
    }
    const ImVec2 clipMin = drawList->GetClipRectMin();
    const ImVec2 clipMax = drawList->GetClipRectMax();
    if (maximumX < clipMin.x || minimumX > clipMax.x || maximumY < clipMin.y || minimumY > clipMax.y)
        return true;

    drawList->AddImageQuad(
        data.textureId,
        corners[0], corners[1], corners[2], corners[3],
        ImVec2(data.uv0.x, data.uv0.y), ImVec2(data.uv1.x, data.uv0.y),
        ImVec2(data.uv1.x, data.uv1.y), ImVec2(data.uv0.x, data.uv1.y),
        tint);
    return true;
}

inline bool drawMapContextualTileAsset(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapTileRenderContext &context, bool allowCandidateAsset)
{
    const MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForTileVisual(context);
    if (request.owner.empty())
        return false;

    const MapStyleContextualAssetResolution resolution = resolveMapStyleContextualAsset(request);

    const bool allowCandidatePreview = allowCandidateAsset && mapStyleRuntimeCandidatePreviewAllowed(request.assetClass);
    const std::string atlasAssetId = mapStyleContextualDrawableAtlasAssetId(resolution, allowCandidatePreview);
    return !atlasAssetId.empty() && drawMapRuntimeAtlasAsset(drawList, min, max, atlasAssetId, allowCandidatePreview);
}

inline bool drawMapContextualWayAsset(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapWayRenderContext &context, bool allowCandidateAsset)
{
    const MapStyleContextualAssetRequest request = mapStyleContextualAssetRequestForWayVisual(context);
    if (request.owner.empty())
        return false;

    const MapStyleContextualAssetResolution resolution = resolveMapStyleContextualAsset(request);
    const bool allowCandidatePreview = allowCandidateAsset && mapStyleRuntimeCandidatePreviewAllowed(request.assetClass);
    const std::string atlasAssetId = mapStyleContextualDrawableAtlasAssetId(resolution, allowCandidatePreview);
    const int rotation = request.assetClass == MapStyleContextualAssetClass::BridgeSequence
                             ? mapStyleBridgeSequenceRotationQuarterTurns(context)
                             : 0;
    return !atlasAssetId.empty() && drawMapRuntimeAtlasAsset(drawList, min, max, atlasAssetId, allowCandidatePreview, rotation);
}

inline bool mapWayAlreadyRepresentedByTerrain(const MapWayRenderContext &context)
{
    if (mapWayKindIsRoadLike(context.wayKind))
        return context.terrainKind == MapTileKind::Road || context.terrainKind == MapTileKind::Bridge;
    if (mapWayKindIsWaterLike(context.wayKind))
        return context.terrainKind == MapTileKind::Water || context.terrainKind == MapTileKind::Swamp;
    return false;
}

inline std::string mapCandidateTwoDigit(int value)
{
    value = std::clamp(value, 0, 99);
    return value < 10 ? "0" + std::to_string(value) : std::to_string(value);
}

inline std::string mapCandidateThreeDigit(int value)
{
    value = std::clamp(value, 0, 999);
    if (value < 10)
        return "00" + std::to_string(value);
    if (value < 100)
        return "0" + std::to_string(value);
    return std::to_string(value);
}

inline std::string mapCandidateSameAutoAssetId(const MapTileRenderContext &context)
{
    if (context.kind != MapTileKind::Empty)
        return {};

    const int mask = connectedMapTileMask(context);
    const int variant = 5 + deterministicMapTileIndex(context.tileX, context.tileY, 331 + mask, 8);
    return "fantasy_neutral.world.same_auto.grass_open.mask" +
           mapCandidateTwoDigit(mask) +
           ".v" +
           mapCandidateThreeDigit(variant);
}

inline bool drawMapCandidateSameAutoTerrainAsset(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapTileRenderContext &context, bool allowCandidateAsset)
{
    if (!allowCandidateAsset)
        return false;

    const std::string assetId = mapCandidateSameAutoAssetId(context);
    if (!drawMapRuntimeAtlasAsset(drawList, min, max, assetId, true))
        return false;

    const int overlayCount = getTerrainTransitionOverlayCount(context);
    for (int overlayIndex = 0; overlayIndex < overlayCount; ++overlayIndex)
    {
        ImVec2 overlayUv0;
        ImVec2 overlayUv1;
        getTerrainTransitionOverlayUV(context, overlayIndex, overlayUv0, overlayUv1);
        drawAtlasLayer(drawList, min, max, getTerrainTransitionOverlayTextureId(context, overlayIndex), overlayUv0, overlayUv1, 0);
    }
    return true;
}
