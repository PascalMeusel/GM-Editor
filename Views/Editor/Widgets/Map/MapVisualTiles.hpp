#pragma once

#include "Views/Editor/Widgets/Map/MapVisualRoutes.hpp"

inline ImU32 mapTileColor(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
        return IM_COL32(31, 38, 43, 255);
    case MapTileKind::Water:
        return IM_COL32(43, 99, 143, 255);
    case MapTileKind::Forest:
        return IM_COL32(45, 115, 70, 255);
    case MapTileKind::Mountain:
        return IM_COL32(116, 119, 123, 255);
    case MapTileKind::Hill:
        return IM_COL32(117, 139, 76, 255);
    case MapTileKind::Road:
        return IM_COL32(164, 133, 83, 255);
    case MapTileKind::Settlement:
        return IM_COL32(179, 149, 93, 255);
    case MapTileKind::Building:
        return IM_COL32(142, 106, 75, 255);
    case MapTileKind::Room:
        return IM_COL32(88, 82, 75, 255);
    case MapTileKind::Wall:
        return IM_COL32(47, 52, 58, 255);
    case MapTileKind::Door:
        return IM_COL32(150, 93, 51, 255);
    case MapTileKind::Cave:
        return IM_COL32(82, 75, 69, 255);
    case MapTileKind::Stairs:
        return IM_COL32(118, 104, 154, 255);
    case MapTileKind::Hazard:
        return IM_COL32(168, 66, 58, 255);
    case MapTileKind::Landmark:
        return IM_COL32(202, 169, 78, 255);
    case MapTileKind::Dirt:
        return IM_COL32(138, 107, 69, 255);
    case MapTileKind::Stone:
        return IM_COL32(120, 120, 112, 255);
    case MapTileKind::Snow:
        return IM_COL32(214, 232, 238, 255);
    case MapTileKind::Desert:
        return IM_COL32(211, 179, 109, 255);
    case MapTileKind::Swamp:
        return IM_COL32(92, 114, 67, 255);
    case MapTileKind::Lava:
        return IM_COL32(217, 91, 37, 255);
    case MapTileKind::Ice:
        return IM_COL32(159, 210, 226, 255);
    case MapTileKind::Pit:
        return IM_COL32(22, 24, 26, 255);
    case MapTileKind::Rubble:
        return IM_COL32(112, 108, 98, 255);
    case MapTileKind::Cover:
        return IM_COL32(125, 83, 44, 255);
    case MapTileKind::Chasm:
        return IM_COL32(30, 31, 44, 255);
    case MapTileKind::Bridge:
        return IM_COL32(126, 91, 52, 255);
    }
    return IM_COL32(31, 38, 43, 255);
}

inline std::vector<MapTileKind> allMapTileKinds()
{
    return {
        MapTileKind::Empty,
        MapTileKind::Water,
        MapTileKind::Forest,
        MapTileKind::Mountain,
        MapTileKind::Hill,
        MapTileKind::Road,
        MapTileKind::Settlement,
        MapTileKind::Building,
        MapTileKind::Room,
        MapTileKind::Wall,
        MapTileKind::Door,
        MapTileKind::Cave,
        MapTileKind::Stairs,
        MapTileKind::Hazard,
        MapTileKind::Landmark,
        MapTileKind::Dirt,
        MapTileKind::Stone,
        MapTileKind::Snow,
        MapTileKind::Desert,
        MapTileKind::Swamp,
        MapTileKind::Lava,
        MapTileKind::Ice,
        MapTileKind::Pit,
        MapTileKind::Rubble,
        MapTileKind::Cover,
        MapTileKind::Chasm,
        MapTileKind::Bridge};
}

inline void drawTileCode(ImDrawList *drawList, ImVec2 min, ImVec2 max, MapTileKind kind)
{
    const char *code = mapTileKindCode(kind);
    if (code[0] == '\0')
        return;

    const ImVec2 textSize = ImGui::CalcTextSize(code);
    const ImVec2 position(
        min.x + (max.x - min.x - textSize.x) * 0.5f,
        min.y + (max.y - min.y - textSize.y) * 0.5f);
    drawList->AddText(position, IM_COL32(235, 239, 242, 255), code);
}

inline void drawMapTileGrid(ImDrawList *drawList, ImVec2 min, ImVec2 max, float cellSize, bool force = false)
{
    if (!force && cellSize < 36.0f)
        return;
    if (force && cellSize < 10.0f)
        return;

    const int alpha = cellSize >= 52.0f ? 48 : (force ? 34 : 22);
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, alpha));
}

inline void drawMapSquareGridOverlay(
    ImDrawList *drawList,
    ImVec2 origin,
    float cellSize,
    int width,
    int height,
    ImU32 color = IM_COL32(225, 218, 196, 72))
{
    if (!drawList || cellSize < 10.0f || width <= 0 || height <= 0)
        return;

    const float right = origin.x + static_cast<float>(width) * cellSize;
    const float bottom = origin.y + static_cast<float>(height) * cellSize;
    for (int x = 0; x <= width; ++x)
    {
        const float px = origin.x + static_cast<float>(x) * cellSize;
        drawList->AddLine(ImVec2(px, origin.y), ImVec2(px, bottom), color, 1.0f);
    }
    for (int y = 0; y <= height; ++y)
    {
        const float py = origin.y + static_cast<float>(y) * cellSize;
        drawList->AddLine(ImVec2(origin.x, py), ImVec2(right, py), color, 1.0f);
    }
}


inline bool shouldShowMapTacticalOverlay()
{
    return ImGui::GetIO().KeyAlt;
}

inline std::string mapEdgeMaskLabel(int edgeMask)
{
    const int mask = normalizeMapEdgeMask(edgeMask);
    std::string label;
    if ((mask & 1) != 0)
        label += label.empty() ? "N" : "/N";
    if ((mask & 2) != 0)
        label += label.empty() ? "E" : "/E";
    if ((mask & 4) != 0)
        label += label.empty() ? "S" : "/S";
    if ((mask & 8) != 0)
        label += label.empty() ? "W" : "/W";
    return label.empty() ? "None" : label;
}

inline void drawMapCellTacticalOverlay(ImDrawList *drawList, ImVec2 min, ImVec2 max, const MapCellModel *cell, float cellSize)
{
    if (!cell)
        return;

    if (cell->elevation != 0 && cellSize >= 26.0f)
    {
        const std::string label = cell->elevation > 0 ? "+" + std::to_string(cell->elevation) : std::to_string(cell->elevation);
        drawList->AddRectFilled(ImVec2(min.x + 2.0f, min.y + 2.0f), ImVec2(min.x + 22.0f, min.y + 16.0f), IM_COL32(22, 26, 30, 170), 2.0f);
        drawList->AddText(ImVec2(min.x + 4.0f, min.y + 1.0f), IM_COL32(235, 224, 174, 255), label.c_str());
    }

    const int movementMask = normalizeMapEdgeMask(cell->movementBlockedMask);
    const int sightMask = normalizeMapEdgeMask(cell->lineOfSightBlockedMask);
    const int oneWayMovementMask = normalizeMapEdgeMask(cell->movementOneWayBlockedMask);
    const int oneWaySightMask = normalizeMapEdgeMask(cell->lineOfSightOneWayBlockedMask);
    auto drawEdges = [&](int mask, ImU32 color, float thickness, float inset) {
        if ((mask & 1) != 0)
            drawList->AddLine(ImVec2(min.x + inset, min.y + inset), ImVec2(max.x - inset, min.y + inset), color, thickness);
        if ((mask & 2) != 0)
            drawList->AddLine(ImVec2(max.x - inset, min.y + inset), ImVec2(max.x - inset, max.y - inset), color, thickness);
        if ((mask & 4) != 0)
            drawList->AddLine(ImVec2(min.x + inset, max.y - inset), ImVec2(max.x - inset, max.y - inset), color, thickness);
        if ((mask & 8) != 0)
            drawList->AddLine(ImVec2(min.x + inset, min.y + inset), ImVec2(min.x + inset, max.y - inset), color, thickness);
    };
    drawEdges(movementMask, IM_COL32(230, 92, 70, 235), 2.8f, 2.5f);
    drawEdges(sightMask, IM_COL32(148, 118, 255, 220), 1.8f, 6.0f);

    auto drawOneWayMarkers = [&](int mask, ImU32 color, float inset) {
        const float cx = (min.x + max.x) * 0.5f;
        const float cy = (min.y + max.y) * 0.5f;
        const float size = std::clamp(cellSize * 0.13f, 3.0f, 7.0f);
        if ((mask & 1) != 0)
            drawList->AddTriangleFilled(ImVec2(cx, min.y + inset), ImVec2(cx - size, min.y + inset + size), ImVec2(cx + size, min.y + inset + size), color);
        if ((mask & 2) != 0)
            drawList->AddTriangleFilled(ImVec2(max.x - inset, cy), ImVec2(max.x - inset - size, cy - size), ImVec2(max.x - inset - size, cy + size), color);
        if ((mask & 4) != 0)
            drawList->AddTriangleFilled(ImVec2(cx, max.y - inset), ImVec2(cx - size, max.y - inset - size), ImVec2(cx + size, max.y - inset - size), color);
        if ((mask & 8) != 0)
            drawList->AddTriangleFilled(ImVec2(min.x + inset, cy), ImVec2(min.x + inset + size, cy - size), ImVec2(min.x + inset + size, cy + size), color);
    };
    drawOneWayMarkers(oneWayMovementMask, IM_COL32(230, 92, 70, 245), 3.0f);
    drawOneWayMarkers(oneWaySightMask, IM_COL32(148, 118, 255, 235), 7.0f);
}
