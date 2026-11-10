#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

#include "Views/Editor/Widgets/Map/MapViewer/MapViewerCanvasFrame.hpp"

void MapViewerView::drawMapCanvasCells(const CanvasFrame &frame)
{
    MapModel *map = frame.map;
    ImDrawList *drawList = frame.drawList;
    const ImVec2 origin = frame.origin;
    const float cellSize = frame.cellSize;
    const float mapPixelWidth = frame.mapPixelWidth;
    const float mapPixelHeight = frame.mapPixelHeight;
    const MapVisibleCellBounds visibleCells = frame.visibleCells;
    const bool showTacticalOverlay = frame.showTacticalOverlay;
    const bool continuousRendering = frame.continuousRendering;
    const bool forceTacticalGrid = frame.forceTacticalGrid;
    const bool importedBackgroundDrawn = frame.importedBackgroundDrawn;
    const std::string &renderLocale = frame.renderLocale;
    for (int y = visibleCells.minY; y <= visibleCells.maxY; ++y)
    {
        for (int x = visibleCells.minX; x <= visibleCells.maxX; ++x)
        {
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            const MapCellModel *cell = mapCellAt(*map, x, y);
            if (continuousRendering || importedBackgroundDrawn)
                continue;
            drawMapCellSemanticLayers(drawList, min, max, *map, x, y, true, _controller._previewCandidateStyleAssets);
            if (showTacticalOverlay && cell && cell->difficultTerrain)
                drawList->AddCircle(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), cellSize * 0.24f, IM_COL32(245, 184, 72, 210), 16, 1.5f);
            if (showTacticalOverlay)
                drawMapCellTacticalOverlay(drawList, min, max, cell, cellSize);
            // Atlas View mode stays illustration-first. The square grid is a
            // tactical aid and must not reappear merely because the user zooms in.
            if (forceTacticalGrid)
                drawMapTileGrid(drawList, min, max, cellSize, true);
        }
    }

    if (continuousRendering || importedBackgroundDrawn)
    {
        MapRenderPlanRequest featureRequest;
        featureRequest.audience = MapRenderAudience::GameMaster;
        featureRequest.locale = renderLocale;
        featureRequest.layers = MapRenderPlanWays | MapRenderPlanSymbols |
                                MapRenderPlanLabels;
        featureRequest.applyFog = false;
        featureRequest.showPoliticalOverlay = _controller._showPoliticalOverlay;
        if (_controller._showPoliticalOverlay)
            featureRequest.layers |= MapRenderPlanPolitical;
        featureRequest.showGrid = forceTacticalGrid;
        if (forceTacticalGrid)
            featureRequest.layers |= MapRenderPlanGrid;
        const std::shared_ptr<const MapRenderPlan> featurePlan =
            _renderPlanCache.getOrCompile(*map, featureRequest,
                                          _controller._mapRenderContentVersion,
                                          cellSize);
        renderMap(drawList, origin, *map, *featurePlan, cellSize);
        for (int y = visibleCells.minY; y <= visibleCells.maxY; ++y)
        {
            for (int x = visibleCells.minX; x <= visibleCells.maxX; ++x)
            {
                const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
                const ImVec2 max(min.x + cellSize, min.y + cellSize);
                const MapCellModel *cell = mapCellAt(*map, x, y);
                if (showTacticalOverlay && cell && cell->difficultTerrain)
                    drawList->AddCircle(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), cellSize * 0.24f, IM_COL32(245, 184, 72, 210), 16, 1.5f);
                if (showTacticalOverlay)
                    drawMapCellTacticalOverlay(drawList, min, max, cell, cellSize);
            }
        }
    }

    const auto mistTexture=getMapFogTexture(true).textureId;
    gm::mapui::visitEditorFog(*map,visibleCells.minX,visibleCells.minY,visibleCells.maxX,visibleCells.maxY,
        [&](int x0,int y0,int x1,int y1){drawMapFogSpan(drawList,mistTexture,origin,cellSize,x0,y0,x1,y1,true,.28f);},
        [](int,int,int,int){},false);
}
