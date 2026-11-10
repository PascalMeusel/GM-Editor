#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawMapCanvasTerrain(const CanvasFrame &frame)
{
    MapModel &map = *frame.map;
    const MapModel &canvasSource = *frame.canvasSource;
    ImDrawList *drawList = frame.drawList;
    const ImVec2 origin = frame.origin;
    const ImVec2 canvasOrigin = frame.canvasOrigin;
    const float cellSize = frame.cellSize;
    const MapVisibleCellBounds visibleCells = frame.visibleCells;
    const bool showTacticalOverlay = frame.showTacticalOverlay;
    const bool showEditorGrid = frame.showEditorGrid;
    const bool continuousRendering = frame.continuousRendering;
    const bool importedBackgroundDrawn = frame.importedBackgroundDrawn;
    const std::string &renderLocale = frame.renderLocale;
    const std::uint64_t canvasContentVersion = frame.canvasContentVersion;
    const auto pointIsVisible = [&](float x, float y, float padding) {
        return frame.pointIsVisible(x, y, padding);
    };
    for (int y = visibleCells.minY; y <= visibleCells.maxY; ++y)
    {
        for (int x = visibleCells.minX; x <= visibleCells.maxX; ++x)
        {
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (continuousRendering || importedBackgroundDrawn)
                continue;
            drawMapCellSemanticLayers(drawList, min, max, map, x, y, true, _controller._generation.previewCandidateStyleAssets);
            if (showTacticalOverlay && cell && cell->difficultTerrain)
                drawList->AddCircle(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), cellSize * 0.25f,
                                    IM_COL32(245, 184, 72, 210), 16, 1.5f);
            if (showTacticalOverlay)
                drawMapCellTacticalOverlay(drawList, min, max, cell, cellSize);
            if (showEditorGrid)
                drawMapTileGrid(drawList, min, max, cellSize);
        }
    }

    if (continuousRendering || importedBackgroundDrawn)
    {
        MapRenderPlanRequest featureRequest;
        featureRequest.audience = MapRenderAudience::GameMaster;
        featureRequest.locale = renderLocale;
        // Resolve relief and its visible road/river corridors together, just
        // as export does. A terrain-only plan loses the required pass clearances.
        featureRequest.layers = MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays | MapRenderPlanSymbols |
                                MapRenderPlanPolitical | MapRenderPlanLabels | MapRenderPlanRoutes;
        if (importedBackgroundDrawn)
            featureRequest.layers |= MapRenderPlanTerrain | MapRenderPlanWater;
        featureRequest.showPoliticalOverlay = map.atlasLevel == MapLevel::World && _showWorldPoliticalOverlay;
        featureRequest.applyFog = false;
        _canvasPreviewCompiler.request(canvasSource, featureRequest, canvasContentVersion);
        const auto preview = _canvasPreviewCompiler.state();
        if (preview.plan)
            renderMap(drawList, origin, *preview.source, *preview.plan, cellSize, importedBackgroundDrawn);
        // Show current semantic changes immediately while the immutable art
        // catches up. Reuse a previous plan only with its own source snapshot.
        if(preview.current) _geometryPreviewPending=false;
        // A way move changes coarse occupancy cells, not the illustrated ground.
        // Overlaying those cells painted black/blue blocks until the async plan
        // finished. Keep its coherent snapshot and the live route highlight.
        if (!preview.current && !importedBackgroundDrawn && !_geometryPreviewPending)
        {
            for (int y = visibleCells.minY; y <= visibleCells.maxY; ++y)
                for (int x = visibleCells.minX; x <= visibleCells.maxX; ++x)
                {
                    const auto *current = mapCellAt(map, x, y);
                    const auto *previous = preview.source ? mapCellAt(*preview.source, x, y) : nullptr;
                    if (current && previous && current->kind == previous->kind && current->wayKind == previous->wayKind &&
                        current->elevation == previous->elevation)
                        continue;
                    const ImVec2 minimum(origin.x + x * cellSize, origin.y + y * cellSize);
                    if (!current) continue;
                    // This is an immediate semantic preview, not another
                    // textured tile build on the input path.
                    drawList->AddRectFilled(minimum, ImVec2(minimum.x + cellSize, minimum.y + cellSize),
                                           withAlpha(mapTileColor(current->kind), 220));
                    if (current->wayKind != MapWayKind::None)
                        drawList->AddCircleFilled(ImVec2(minimum.x + cellSize * .5f, minimum.y + cellSize * .5f),
                            std::max(1.f, cellSize * .14f), current->wayKind == MapWayKind::River
                                ? IM_COL32(98,151,161,255) : IM_COL32(122,96,65,255), 6);
                }
        }
        if (!preview.current)
        {
            const std::string message = preview.error.empty()
                ? uiTextForLocale("Updating map illustration…", "Kartendarstellung wird aktualisiert…")
                : uiTextForLocale("Map illustration failed; retrying…", "Kartendarstellung fehlgeschlagen; neuer Versuch…");
            const ImVec2 label(canvasOrigin.x + 12, canvasOrigin.y + 12);
            const auto size = ImGui::CalcTextSize(message.c_str());
            drawList->AddRectFilled(ImVec2(label.x - 5, label.y - 3), ImVec2(label.x + size.x + 5, label.y + size.y + 3),
                                    IM_COL32(32, 36, 40, 235), 3);
            drawList->AddText(label, IM_COL32(238, 224, 185, 255), message.c_str());
            if (!preview.error.empty() && ImGui::GetTime() >= _nextCanvasPreviewRetry)
            {
                _nextCanvasPreviewRetry = ImGui::GetTime() + 2;
                _canvasPreviewCompiler.retry();
            }
        }
        for (int y = visibleCells.minY; y <= visibleCells.maxY; ++y)
        {
            for (int x = visibleCells.minX; x <= visibleCells.maxX; ++x)
            {
                const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
                const ImVec2 max(min.x + cellSize, min.y + cellSize);
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (showTacticalOverlay && cell && cell->difficultTerrain)
                    drawList->AddCircle(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), cellSize * 0.25f,
                                        IM_COL32(245, 184, 72, 210), 16, 1.5f);
                if (showTacticalOverlay)
                    drawMapCellTacticalOverlay(drawList, min, max, cell, cellSize);
                if (showEditorGrid)
                    drawMapTileGrid(drawList, min, max, cellSize);
            }
        }
    }

    if (_controller._editing.strokeInProgress && (continuousRendering || importedBackgroundDrawn))
    {
        using Tool = MapCreatorController::Tool;
        const auto tool = _controller._editing.activeTool;
        if (tool == Tool::PaintTile || tool == Tool::EraseTile || tool == Tool::EraseWay ||
            tool == Tool::PaintElevation)
        {
            const ImU32 color = tool == Tool::PaintTile ? withAlpha(mapTileColor(_controller.activeTileKind()), 185)
                                : tool == Tool::PaintElevation ? IM_COL32(235, 181, 88, 115)
                                                               : IM_COL32(238, 205, 148, 155);
            _controller._editing.brushStrokePreview.visitSpans(
                visibleCells.minX, visibleCells.minY, visibleCells.maxX, visibleCells.maxY,
                [&](int x0, int y0, int x1, int y1) {
                    const ImVec2 a(origin.x + x0 * cellSize, origin.y + y0 * cellSize);
                    const ImVec2 b(origin.x + x1 * cellSize, origin.y + y1 * cellSize);
                    drawList->AddRectFilled(a, b, color);
                });
        }
    }

    gm::mapui::visitEditorFog(
        map, visibleCells.minX, visibleCells.minY, visibleCells.maxX, visibleCells.maxY,
        [&](int x0, int y0, int x1, int y1) {
            drawMapFogSpan(drawList, getMapFogTexture(true).textureId, origin, cellSize, x0, y0, x1, y1, true,
                           _editorFogOpacity);
        },
        [&](int x0, int y0, int x1, int y1) {
            drawList->AddLine(ImVec2(origin.x + x0 * cellSize, origin.y + y0 * cellSize),
                              ImVec2(origin.x + x1 * cellSize, origin.y + y1 * cellSize), IM_COL32(175, 162, 207, 200),
                              1.5f);
        },
        _controller._editing.activeTool == MapCreatorController::Tool::Fog);
}
