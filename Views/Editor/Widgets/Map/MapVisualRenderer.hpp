#pragma once

#include "Views/Editor/Widgets/Map/MapVisualFeatures.hpp"

enum MapRenderLayer : unsigned int
{
    MapRenderLayerTerrain = 1u << 0,
    MapRenderLayerFeatures = 1u << 1,
    MapRenderLayerSymbols = 1u << 2,
    MapRenderLayerGrid = 1u << 3,
    MapRenderLayerAll = MapRenderLayerTerrain | MapRenderLayerFeatures | MapRenderLayerSymbols | MapRenderLayerGrid
};

struct MapRenderContext
{
    const MapRenderPlan *resolvedPlan = nullptr;
    const std::vector<gm::map_render::AtlasInkPrimitive> *atlasInk = nullptr;
    const std::vector<gm::map_render::AtlasInkPrimitive> *atlasWaterInk = nullptr;
    float cellSize = 24.0f;
    unsigned int layers = MapRenderLayerAll;
    bool showGrid = false;
    bool playerSafe = false;
    MapGeneratedFeatureVisibilityFilter generatedFeatureVisible;
    MapGeneratedFeatureVisibilityFilter generatedFeatureGeometryVisible;
    bool showPoliticalOverlay = false;
    bool showSymbols = true;
    bool showLabels = true;
    // Empty means the current application locale. Immutable output plans set
    // this explicitly so preview, editor, player display, and PNG agree even
    // when they are rendered outside the active UI locale.
    std::string_view locale;
};

inline void renderMap(ImDrawList *drawList, ImVec2 origin, const MapModel &map, const MapRenderContext &context)
{
    if (!drawList || context.cellSize <= 0.0f)
        return;
    std::vector<ImVec4> labelBounds;
    if ((context.layers & MapRenderLayerTerrain) != 0)
        measureMapRenderPhase(&MapRenderTimings::terrain,[&] {
            drawMapContinuousTerrainBase(drawList, origin, context.cellSize, map, context.atlasInk, context.atlasWaterInk);
        });
    if ((context.layers & MapRenderLayerFeatures) != 0)
        measureMapRenderPhase(&MapRenderTimings::features,[&] {
        drawMapContinuousFeatureGraph(drawList, origin, context.cellSize, map, context.playerSafe,
                                      context.generatedFeatureVisible, context.showPoliticalOverlay,
                                      context.showLabels, context.locale, context.generatedFeatureGeometryVisible, context.resolvedPlan, &labelBounds);
        });
    if (context.resolvedPlan)
        drawMapIllustratedAtlasInk(drawList,origin,context.cellSize,context.resolvedPlan->atlasFeatureInk);
    else if ((gm::map_render::usesIllustratedAtlasV3(map) || (gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map))))
    {
        MapRenderPlanRequest request;
        request.audience = context.playerSafe ? MapRenderAudience::Player : MapRenderAudience::GameMaster;
        request.layers = ((context.layers & MapRenderLayerFeatures) ? MapRenderPlanWays : 0u) |
            ((context.layers & MapRenderLayerSymbols) && context.showSymbols ? MapRenderPlanSymbols : 0u);
        request.additionalGeneratedFeatureVisibility = [&](const MapGeneratedFeature &feature) {
            return (!context.generatedFeatureVisible || context.generatedFeatureVisible(feature)) &&
                (!context.generatedFeatureGeometryVisible || context.generatedFeatureGeometryVisible(feature));
        };
        const auto plan = compileMapRenderPlan(map,request);
        drawMapIllustratedAtlasInk(drawList,origin,context.cellSize,plan.atlasFeatureInk);
    }
    if ((context.layers & MapRenderLayerSymbols) != 0)
        measureMapRenderPhase(&MapRenderTimings::symbols,[&] {
        drawMapContinuousSymbols(drawList, origin, context.cellSize, map, context.playerSafe,
                                 context.showSymbols, context.showLabels,
                                 context.generatedFeatureVisible, context.locale, &labelBounds, context.resolvedPlan);
        });
    if ((context.layers & MapRenderLayerGrid) != 0 && context.showGrid)
        drawMapSquareGridOverlay(drawList, origin, context.cellSize, map.width, map.height);
}

inline void renderMap(ImDrawList *drawList, ImVec2 origin, const MapModel &map, const MapRenderPlan &plan,
                      float cellSize, bool authoritativeBackgroundDrawn = false)
{
    if (!mapRenderPlanMatchesMap(plan, map))
        return;
    MapRenderContext context;
    context.resolvedPlan = &plan;
    context.atlasInk = &plan.atlasInk;
    context.atlasWaterInk = &plan.atlasWaterInk;
    context.cellSize = cellSize;
    context.layers = 0u;
    // Imported artwork is an authoritative visual base.  It is still part of
    // this immutable plan, but the semantic terrain must not be painted over
    // it. Editor, preview and player display use the same rule.
    if (!authoritativeBackgroundDrawn &&
        (plan.layers & (MapRenderPlanTerrain | MapRenderPlanWater)) != 0)
        context.layers |= MapRenderLayerTerrain;
    if ((plan.layers & (MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays |
                        MapRenderPlanPolitical | MapRenderPlanSymbols | MapRenderPlanLabels)) != 0)
        context.layers |= MapRenderLayerFeatures;
    if ((plan.layers & (MapRenderPlanSymbols | MapRenderPlanLabels)) != 0)
        context.layers |= MapRenderLayerSymbols;
    if ((plan.layers & MapRenderPlanGrid) != 0)
        context.layers |= MapRenderLayerGrid;
    context.showGrid = plan.showGrid;
    context.playerSafe = plan.audience == MapRenderAudience::Player;
    context.showPoliticalOverlay = plan.showPoliticalOverlay;
    context.showSymbols = (plan.layers & MapRenderPlanSymbols) != 0;
    context.showLabels = (plan.layers & MapRenderPlanLabels) != 0;
    context.locale = plan.locale;
    context.generatedFeatureVisible = [&plan](const MapGeneratedFeature &feature) {
        return mapRenderPlanContainsGeneratedFeature(plan, feature.id);
    };
    context.generatedFeatureGeometryVisible = [&plan](const MapGeneratedFeature &feature) {
        return mapRenderPlanContainsGeneratedGeometry(plan, feature.id);
    };
    renderMap(drawList, origin, map, context);
}

inline void renderTileLegend(const std::vector<MapTileKind> &tiles, const char *emptyText)
{
    ImGui::BeginChild("MapLegend", ImVec2(250.0f, 0.0f), true);
    ImGui::TextUnformatted(localizedUiLabel("Legend").c_str());
    ImGui::Separator();

    if (tiles.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), emptyText);
        ImGui::EndChild();
        return;
    }

    for (MapTileKind kind : tiles)
    {
        ImGui::PushID(static_cast<int>(kind));
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 swatchMin(cursor.x, cursor.y + 2.0f);
        const ImVec2 swatchMax(cursor.x + 24.0f, cursor.y + 26.0f);
        drawMapTile(ImGui::GetWindowDrawList(), swatchMin, swatchMax, kind, 0, false);
        ImGui::GetWindowDrawList()->AddRect(swatchMin, swatchMax, IM_COL32(215, 224, 230, 190), 3.0f);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 32.0f);
        ImGui::Text(localizedUiLabel("%s  %s").c_str(), mapTileKindCode(kind), mapTileKindLabel(kind));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", mapTileKindPurpose(kind));
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), mapTileKindPurpose(kind));
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::PopID();
    }

    ImGui::EndChild();
}
