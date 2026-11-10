#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawMapCanvasToolPreview(const CanvasFrame &frame, const ImVec2 &mouse, int x, int y)
{
    MapModel &map = *frame.map;
    ImDrawList *drawList = frame.drawList;
    const ImVec2 origin = frame.origin;
    const float cellSize = frame.cellSize;
        if (isMapCellInside(map, x, y))
        {
            const ImVec2 cellMin(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 cellMax(cellMin.x + cellSize, cellMin.y + cellSize);

            if (_imageTraceActive)
            {
                const MapImageTraceAuthoring::Mode traceMode = imageTraceMode(_imageTraceModeIndex);
                const MapFeaturePoint previewPoint = MapImageTraceAuthoring::constrainPoint(
                    {(mouse.x - origin.x) / cellSize, (mouse.y - origin.y) / cellSize}, map.width, map.height,
                    _imageTraceSnap);
                const ImVec2 center(origin.x + previewPoint.x * cellSize, origin.y + previewPoint.y * cellSize);
                const ImU32 color = traceMode == MapImageTraceAuthoring::Mode::River ||
                                            traceMode == MapImageTraceAuthoring::Mode::Coastline
                                        ? IM_COL32(60, 196, 220, 245)
                                        : IM_COL32(245, 190, 76, 245);
                drawList->AddCircle(center, 7.5f, color, 18, 2.0f);
                if (!_imageTraceControlPoints.empty() && _imageTraceDraggedPoint < 0 &&
                    (MapImageTraceAuthoring::isLine(traceMode) || MapImageTraceAuthoring::isClosed(traceMode)))
                {
                    const MapFeaturePoint &last = _imageTraceControlPoints.back();
                    drawList->AddLine(ImVec2(origin.x + last.x * cellSize, origin.y + last.y * cellSize), center,
                                      withAlpha(color, 150), 1.5f);
                }
            }
            else if (_controller.activeToolUsesBrush())
            {
                // The footprint is the cursor. Hovering never mutates the map or
                // overlays legacy tile textures on the illustrated renderer.
                ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                drawList->AddLine(ImVec2(mouse.x - 6, mouse.y), ImVec2(mouse.x + 6, mouse.y), IM_COL32(20, 25, 24, 255),
                                  3.0f);
                drawList->AddLine(ImVec2(mouse.x, mouse.y - 6), ImVec2(mouse.x, mouse.y + 6), IM_COL32(20, 25, 24, 255),
                                  3.0f);
                drawList->AddLine(ImVec2(mouse.x - 6, mouse.y), ImVec2(mouse.x + 6, mouse.y),
                                  IM_COL32(255, 249, 213, 255), 1.0f);
                drawList->AddLine(ImVec2(mouse.x, mouse.y - 6), ImVec2(mouse.x, mouse.y + 6),
                                  IM_COL32(255, 249, 213, 255), 1.0f);
                const ImU32 previewFill =
                    _controller._editing.activeTool == MapCreatorController::Tool::PaintTile
                        ? withAlpha(mapTileColor(_controller.activeTileKind()), 55)
                    : _controller._editing.activeTool == MapCreatorController::Tool::PaintWay
                        ? withAlpha(mapWayColor(_controller.activeWayKind(), false), 70)
                    : _controller._editing.activeTool == MapCreatorController::Tool::PaintElevation ? IM_COL32(245, 184, 72, 70)
                    : _controller._editing.activeTool == MapCreatorController::Tool::PaintTacticalEdge
                        ? IM_COL32(214, 105, 87, 65)
                    : _controller._editing.activeTool == MapCreatorController::Tool::Fog
                        ? (_controller._editing.revealFog ? IM_COL32(65, 215, 165, 45) : IM_COL32(120, 100, 165, 65))
                        : IM_COL32(235, 95, 90, 70);
                const ImU32 previewLine =
                    _controller._editing.activeTool == MapCreatorController::Tool::EraseTile  ? IM_COL32(235, 95, 90, 255)
                    : _controller._editing.activeTool == MapCreatorController::Tool::EraseWay ? IM_COL32(235, 95, 90, 255)
                    : _controller._editing.activeTool == MapCreatorController::Tool::PaintWay
                        ? mapWayColor(_controller.activeWayKind(), false)
                    : _controller._editing.activeTool == MapCreatorController::Tool::PaintElevation
                        ? IM_COL32(245, 184, 72, 255)
                    : _controller._editing.activeTool == MapCreatorController::Tool::PaintTacticalEdge
                        ? IM_COL32(230, 92, 70, 255)
                    : _controller._editing.activeTool == MapCreatorController::Tool::Fog
                        ? (_controller._editing.revealFog ? IM_COL32(85, 240, 180, 255) : IM_COL32(200, 180, 240, 255))
                        : IM_COL32(250, 220, 110, 255);
                if (_controller._editing.activeTool == MapCreatorController::Tool::PaintWay &&
                    map.generation.continuousRendering)
                {
                    const float radius = gm::map_render::manualWayStrokeDisplayWidth(
                                             map, _controller.activeWayKind(), _controller.activeBrushSize(), cellSize)
                                             .outerPixels *
                                         .5f;
                    drawList->AddCircleFilled(mouse, radius, previewFill, 32);
                    drawList->AddCircle(mouse, radius, previewLine, 32, 1.0f);
                }
                else
                {
                    const auto footprint =
                        mapBrushFootprint(map, x, y, _controller._editing.brushSize, _controller._editing.roundBrush);
                    for (const auto &cell : footprint)
                    {
                        const ImVec2 min(origin.x + cell.x * cellSize, origin.y + cell.y * cellSize);
                        const ImVec2 max(min.x + cellSize, min.y + cellSize);
                        drawList->AddRectFilled(min, max, previewFill);
                        // Outline only the outside boundary; internal grid lines obscure the brush shape.
                        const auto contains = [&](int px, int py) {
                            return std::binary_search(footprint.begin(), footprint.end(), MapCellCoordinate{px, py});
                        };
                        if (!contains(cell.x - 1, cell.y))
                            drawList->AddLine(min, ImVec2(min.x, max.y), previewLine, 1.5f);
                        if (!contains(cell.x + 1, cell.y))
                            drawList->AddLine(ImVec2(max.x, min.y), max, previewLine, 1.5f);
                        if (!contains(cell.x, cell.y - 1))
                            drawList->AddLine(min, ImVec2(max.x, min.y), previewLine, 1.5f);
                        if (!contains(cell.x, cell.y + 1))
                            drawList->AddLine(ImVec2(min.x, max.y), max, previewLine, 1.5f);
                    }
                }
            }
            else if (_controller._editing.activeTool == MapCreatorController::Tool::PlaceToken)
            {
                const ImVec2 center((cellMin.x + cellMax.x) * 0.5f, (cellMin.y + cellMax.y) * 0.5f);
                drawList->AddCircleFilled(center, cellSize * 0.30f, IM_COL32(92, 192, 235, 125), 24);
                drawList->AddCircle(center, cellSize * 0.36f, IM_COL32(245, 250, 255, 230), 24, 2.0f);
                drawList->AddText(ImVec2(center.x - 4.0f, center.y - 9.0f), IM_COL32(18, 22, 25, 255), "T");
            }
            else if (_controller._editing.activeTool == MapCreatorController::Tool::AddFeature)
            {
                const ImVec2 center((cellMin.x + cellMax.x) * 0.5f, (cellMin.y + cellMax.y) * 0.5f);
                drawList->AddCircleFilled(center, cellSize * 0.24f, IM_COL32(103, 211, 156, 135), 18);
                drawList->AddCircle(center, cellSize * 0.31f, IM_COL32(250, 220, 110, 255), 18, 2.0f);
                drawList->AddText(ImVec2(center.x - 4.0f, center.y - 9.0f), IM_COL32(18, 22, 25, 255), "!");
            }
            else if (_controller._editing.activeTool == MapCreatorController::Tool::AddRegion)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
            else if (_controller._editing.activeTool == MapCreatorController::Tool::AddRoute)
            {
                int endX = std::clamp(x + 4, 0, map.width - 1);
                if (endX == x && map.width > 1)
                    endX = std::max(0, x - 4);
                const ImVec2 start((cellMin.x + cellMax.x) * 0.5f, (cellMin.y + cellMax.y) * 0.5f);
                const ImVec2 end(origin.x + (endX + 0.5f) * cellSize, start.y);
                drawList->AddLine(start, end, IM_COL32(250, 220, 110, 255), 4.0f);
                drawList->AddCircleFilled(start, 4.0f, IM_COL32(250, 220, 110, 255), 12);
                drawList->AddCircleFilled(end, 4.0f, IM_COL32(250, 220, 110, 255), 12);
            }
            else
            {
                drawList->AddRect(cellMin, cellMax, IM_COL32(250, 220, 110, 220), 2.0f, 0, 2.0f);
            }
        }

}
