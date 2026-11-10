#pragma once

#include "Views/Editor/Widgets/Map/MapVisualPolitical.hpp"

inline void drawMapContinuousBridges(
    ImDrawList *drawList,
    ImVec2 origin,
    float cellSize,
    const MapModel &map,
    const std::function<bool(const MapGeneratedFeature &)> &geometryIsVisible)
{
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (gm::map_render::usesIllustratedAtlasV3(map) ||
            gm::map_render::usesIllustratedAtlasV4(map) ||
            gm::map_render::usesIllustratedAtlasV5(map) ||
            feature.kind != MapGeneratedFeatureKind::Bridge ||
            feature.points.empty() || !geometryIsVisible(feature))
            continue;

        const MapFeaturePoint &startPoint = feature.points.front();
        const MapFeaturePoint &endPoint = feature.points.size() > 1 ? feature.points.back() : startPoint;
        const ImVec2 start(origin.x + startPoint.x * cellSize, origin.y + startPoint.y * cellSize);
        const ImVec2 end(origin.x + endPoint.x * cellSize, origin.y + endPoint.y * cellSize);
        const ImVec2 center((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        dx /= length;
        dy /= length;
        const float nx = -dy * cellSize * 0.20f;
        const float ny = dx * cellSize * 0.20f;
        const ImVec2 a(start.x + nx, start.y + ny);
        const ImVec2 b(end.x + nx, end.y + ny);
        const ImVec2 c(end.x - nx, end.y - ny);
        const ImVec2 d(start.x - nx, start.y - ny);
        const int mapX = static_cast<int>(std::floor((center.x - origin.x) / cellSize));
        const int mapY = static_cast<int>(std::floor((center.y - origin.y) / cellSize));
        if (drawProductionAssetRoleRotated(
                drawList, center,
                cellSize * gm::map_render::tacticalBridgeStampWidth(map, feature),
                std::max(cellSize * 2.0f, length + cellSize * 0.85f),
                std::atan2(dy, dx) - 1.57079632679f,
                map, "bridge_stamp", mapBridgeStampSemanticRole(feature), mapX, mapY, 1901,
                IM_COL32_WHITE, mapProductionAssetZoom(cellSize)))
            continue;

        drawList->AddQuadFilled(a, b, c, d, IM_COL32(91, 62, 36, 255));
        const float inset = 2.0f;
        drawList->AddQuadFilled(
            ImVec2(a.x - nx / (cellSize * 0.20f) * inset, a.y - ny / (cellSize * 0.20f) * inset),
            ImVec2(b.x - nx / (cellSize * 0.20f) * inset, b.y - ny / (cellSize * 0.20f) * inset),
            ImVec2(c.x + nx / (cellSize * 0.20f) * inset, c.y + ny / (cellSize * 0.20f) * inset),
            ImVec2(d.x + nx / (cellSize * 0.20f) * inset, d.y + ny / (cellSize * 0.20f) * inset),
            IM_COL32(174, 127, 70, 255));
        for (int plank = -3; plank <= 3; ++plank)
        {
            const float along = static_cast<float>(plank) / 7.0f;
            const ImVec2 plankCenter(center.x + dx * length * along, center.y + dy * length * along);
            drawList->AddLine(
                ImVec2(plankCenter.x + nx * 0.82f, plankCenter.y + ny * 0.82f),
                ImVec2(plankCenter.x - nx * 0.82f, plankCenter.y - ny * 0.82f),
                IM_COL32(91, 62, 36, 190), 1.0f);
        }
    }
}
