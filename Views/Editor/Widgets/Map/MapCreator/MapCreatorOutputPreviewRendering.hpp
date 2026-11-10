#pragma once

#include "Views/Editor/Widgets/Map/MapTileVisuals.hpp"
#include "Views/Editor/Widgets/Map/MapFogRendering.hpp"
#include "Views/Editor/Widgets/Map/MapTokenLegend.hpp"

namespace gm::mapui
{
// The caller supplies filterMapForRenderPlan's source after campaign consent,
// and establishes the whole/current-view clip. This pass never changes source
// dimensions or feature coordinates and contains no editor chrome.
inline void drawMapCreatorOutputPreview(
    ImDrawList *drawList, ImVec2 origin, const MapModel &filtered,
    const MapRenderPlan &plan, float cellSize, bool authoritativeBackgroundDrawn, ImTextureID fogTexture=0,
    bool numberTokens=false)
{
    if (!drawList || cellSize <= 0.0f || !mapRenderPlanMatchesMap(plan, filtered))
        return;
    const auto visible = mapVisibleCellBounds(drawList, origin, cellSize, filtered);
    if (!authoritativeBackgroundDrawn && !filtered.generation.continuousRendering &&
        filtered.generation.graph.features.empty() &&
        (plan.layers & (MapRenderPlanTerrain | MapRenderPlanWater)) != 0)
    {
        // Existing raster maps remain valid exports. Reuse the editor's cell
        // renderer, with its codes/candidate-art previews disabled and only
        // the already-filtered player/GM semantics available to it.
        for (int y = visible.minY; y <= visible.maxY; ++y)
            for (int x = visible.minX; x <= visible.maxX; ++x)
            {
                const ImVec2 minimum(origin.x + x * cellSize, origin.y + y * cellSize);
                drawMapCellSemanticLayers(drawList, minimum,
                    ImVec2(minimum.x + cellSize, minimum.y + cellSize), filtered, x, y,
                    false, false, (plan.layers & MapRenderPlanWays) == 0);
            }
    }
    MapRenderPlan contentPlan = plan;
    contentPlan.layers &= ~static_cast<std::uint32_t>(MapRenderPlanGrid);
    contentPlan.showGrid = false;
    renderMap(drawList, origin, filtered, contentPlan, cellSize, authoritativeBackgroundDrawn);
    const auto legendTokens = numberTokens ? orderedLegendTokens(filtered) : std::vector<const MapTokenModel *>{};
    for (const MapRenderPrimitive &primitive : plan.primitives)
    {
        const bool region = primitive.kind == MapRenderPrimitiveKind::AnnotationRegion;
        const bool route = primitive.kind == MapRenderPrimitiveKind::Route;
        const bool point = primitive.kind == MapRenderPrimitiveKind::AnnotationPoint;
        const bool token = primitive.kind == MapRenderPrimitiveKind::Token;
        if ((!region && !route && !point && !token) || primitive.points.empty())
            continue;
        std::vector<ImVec2> points;
        for (const MapFeaturePoint &position : primitive.points)
            points.emplace_back(origin.x + position.x * cellSize, origin.y + position.y * cellSize);
        const ImU32 color = token ? IM_COL32(92, 192, 235, 255) : IM_COL32(228, 185, 102, 245);
        if (region || route)
            drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                                  region ? ImDrawFlags_Closed : ImDrawFlags_None, 2.0f);
        else
            drawList->AddCircleFilled(points.front(), std::max(3.0f, cellSize * 0.3f), color, 16);
        if (token && numberTokens && (plan.layers & MapRenderPlanLabels) != 0)
        {
            const auto tokenEntry = std::find_if(legendTokens.begin(), legendTokens.end(),
                [&](const auto *entry) { return entry->metadata.id == primitive.sourceId; });
            if (tokenEntry == legendTokens.end()) continue;
            const std::string label = std::to_string(std::distance(legendTokens.begin(), tokenEntry) + 1);
            drawList->AddText(ImVec2(points.front().x + 4.0f, points.front().y - ImGui::GetFontSize()),
                              color, label.c_str());
        }
        else if (!primitive.label.empty())
            drawList->AddText(ImVec2(points.front().x + 5.0f, points.front().y + 5.0f), color, primitive.label.c_str());
    }
    // Match PNG ordering: opaque fog covers all semantic content; the requested
    // grid then lies above it. GM fog is optional and Player fog is mandatory.
    if (plan.applyFog && (plan.layers & MapRenderPlanFog) != 0)
        drawOpaqueMapFog(drawList,fogTexture,origin,cellSize,filtered,
            visible.minX,visible.minY,visible.maxX,visible.maxY);
    if (plan.showGrid && (plan.layers & MapRenderPlanGrid) != 0)
    {
        // Export options must remain visible at overview scale too; the
        // editor's automatic small-cell grid suppression does not apply.
        constexpr ImU32 color = IM_COL32(48, 54, 48, 120);
        for (int x = visible.minX; x <= visible.maxX + 1; ++x)
            drawList->AddLine(ImVec2(origin.x + x * cellSize, origin.y),
                              ImVec2(origin.x + x * cellSize, origin.y + filtered.height * cellSize), color, 1.0f);
        for (int y = visible.minY; y <= visible.maxY + 1; ++y)
            drawList->AddLine(ImVec2(origin.x, origin.y + y * cellSize),
                              ImVec2(origin.x + filtered.width * cellSize, origin.y + y * cellSize), color, 1.0f);
    }
}
}
