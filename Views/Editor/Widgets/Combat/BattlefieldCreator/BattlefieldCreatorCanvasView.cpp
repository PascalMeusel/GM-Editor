#include "BattlefieldCreatorViewSupport.hpp"

void BattlefieldCreatorView::drawCanvas()
{
    EncounterModel &battlefield = _controller._draftBattlefield;
    normalizeEncounter(battlefield);

    ImGui::BeginChild("BattlefieldCreatorCanvas", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    _controller._zoom = std::clamp(_controller._zoom, 0.4f, 2.4f);
    if (ImGui::GetIO().KeyCtrl)
        applyMapCanvasWheelZoom(_controller._zoom, 0.4f, 2.4f);
    const bool showTacticalOverlay = shouldShowMapTacticalOverlay();
    const float cellSize = 28.0f * _controller._zoom;
    const ImVec2 canvasSize(
        std::max(ImGui::GetContentRegionAvail().x, cellSize * static_cast<float>(battlefield.battlefieldWidth) + 20.0f),
        std::max(ImGui::GetContentRegionAvail().y, cellSize * static_cast<float>(battlefield.battlefieldHeight) + 20.0f));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(30, 34, 38, 255));

    for (int y = 0; y < battlefield.battlefieldHeight; ++y)
    {
        for (int x = 0; x < battlefield.battlefieldWidth; ++x)
        {
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            const MapCellModel *cell = encounterCellAt(battlefield, x, y);
            drawMapCellResolvedSemanticLayers(
                drawList,
                min,
                max,
                cell,
                x,
                y,
                encounterCellAt(battlefield, x, y - 1),
                encounterCellAt(battlefield, x + 1, y),
                encounterCellAt(battlefield, x, y + 1),
                encounterCellAt(battlefield, x - 1, y),
                encounterCellAt(battlefield, x + 1, y - 1),
                encounterCellAt(battlefield, x + 1, y + 1),
                encounterCellAt(battlefield, x - 1, y + 1),
                encounterCellAt(battlefield, x - 1, y - 1),
                true);
            if (showTacticalOverlay)
                drawMapCellTacticalOverlay(drawList, min, max, cell, cellSize);
            drawMapTileGrid(drawList, min, max, cellSize);
        }
    }

    ImGui::InvisibleButton("BattlefieldCreatorHitbox", ImVec2(cellSize * battlefield.battlefieldWidth, cellSize * battlefield.battlefieldHeight), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    if (ImGui::IsItemHovered())
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int x = static_cast<int>((mouse.x - origin.x) / cellSize);
        const int y = static_cast<int>((mouse.y - origin.y) / cellSize);
        if (isEncounterCellInside(battlefield, x, y))
        {
            const int brushSize = std::clamp(_controller._brushSize, 1, 8);
            const int start = -(brushSize / 2);
            const int end = start + brushSize - 1;
            const bool editingTerrain = _controller._terrainTool != BattlefieldTerrainTool::Inspect;
            const MapTileKind previewTerrain = _controller._terrainTool == BattlefieldTerrainTool::Erase ? MapTileKind::Empty : _controller.selectedTerrain();
            if (editingTerrain)
            {
                const std::vector<MapCellCoordinate> footprint = battlefieldBrushFootprint(battlefield, x, y, brushSize);
                const EncounterModel previewBattlefield = previewBattlefieldTerrainBrush(battlefield, footprint, previewTerrain);
                const std::vector<MapCellCoordinate> dirtyCells = battlefieldTerrainDirtyCellsForFootprint(previewBattlefield, footprint);
                for (const MapCellCoordinate &coordinate : dirtyCells)
                {
                    const ImVec2 previewCellMin(origin.x + coordinate.x * cellSize, origin.y + coordinate.y * cellSize);
                    const ImVec2 previewCellMax(previewCellMin.x + cellSize, previewCellMin.y + cellSize);
                    const MapCellModel *previewCell = encounterCellAt(previewBattlefield, coordinate.x, coordinate.y);
                    drawMapCellResolvedSemanticLayers(
                        drawList,
                        previewCellMin,
                        previewCellMax,
                        previewCell,
                        coordinate.x,
                        coordinate.y,
                        encounterCellAt(previewBattlefield, coordinate.x, coordinate.y - 1),
                        encounterCellAt(previewBattlefield, coordinate.x + 1, coordinate.y),
                        encounterCellAt(previewBattlefield, coordinate.x, coordinate.y + 1),
                        encounterCellAt(previewBattlefield, coordinate.x - 1, coordinate.y),
                        encounterCellAt(previewBattlefield, coordinate.x + 1, coordinate.y - 1),
                        encounterCellAt(previewBattlefield, coordinate.x + 1, coordinate.y + 1),
                        encounterCellAt(previewBattlefield, coordinate.x - 1, coordinate.y + 1),
                        encounterCellAt(previewBattlefield, coordinate.x - 1, coordinate.y - 1),
                        true);
                    if (showTacticalOverlay)
                        drawMapCellTacticalOverlay(drawList, previewCellMin, previewCellMax, previewCell, cellSize);
                    drawMapTileGrid(drawList, previewCellMin, previewCellMax, cellSize);
                }
            }
            for (int dy = start; dy <= end; ++dy)
            {
                for (int dx = start; dx <= end; ++dx)
                {
                    if (!isEncounterCellInside(battlefield, x + dx, y + dy))
                        continue;
                    const ImVec2 min(origin.x + (x + dx) * cellSize, origin.y + (y + dy) * cellSize);
                    const ImVec2 max(min.x + cellSize, min.y + cellSize);
                    if (editingTerrain)
                        drawList->AddRectFilled(min, max, colorWithAlpha(mapTileColor(previewTerrain), _controller._terrainTool == BattlefieldTerrainTool::Erase ? 0.38f : 0.42f));
                    drawList->AddRect(min, max, editingTerrain ? IM_COL32(255, 216, 112, 220) : IM_COL32(255, 255, 255, 180), 0.0f, 0, 2.0f);
                }
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                _controller.cancelTerrainTool();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                _controller.paintCell(x, y);
            const MapCellModel *hoveredCell = encounterCellAt(battlefield, x, y);
            if (_controller._terrainTool == BattlefieldTerrainTool::Inspect && hoveredCell)
            {
                const std::string toolLabel = localizedUiLabel(battlefieldTerrainToolLabel(_controller._terrainTool));
                const std::string hoveredTerrainLabel = localizedUiLabel(mapTileKindLabel(hoveredCell->kind));
                const std::string movementEdges = localizedEdgeMaskLabel(hoveredCell->movementBlockedMask);
                const std::string sightEdges = localizedEdgeMaskLabel(hoveredCell->lineOfSightBlockedMask);
                ImGui::SetTooltip(localizedUiLabel("%s at %d, %d\n%s\nElevation %+d\nMove blocked %s\nSight blocked %s").c_str(),
                                  toolLabel.c_str(),
                                  x,
                                  y,
                                  hoveredTerrainLabel.c_str(),
                                  hoveredCell->elevation,
                                  movementEdges.c_str(),
                                  sightEdges.c_str());
            }
            else
            {
                const std::string toolLabel = localizedUiLabel(battlefieldTerrainToolLabel(_controller._terrainTool));
                const std::string cancelHint = localizedUiLabel("Right click cancels");
                ImGui::SetTooltip(localizedUiLabel("%s at %d, %d\n%s").c_str(),
                                  toolLabel.c_str(),
                                  x,
                                  y,
                                  cancelHint.c_str());
            }
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _controller.endTerrainStroke();
    ImGui::EndChild();
}
