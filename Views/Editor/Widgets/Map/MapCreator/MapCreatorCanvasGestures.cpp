#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::finishMapCanvasGestures(const CanvasFrame &frame)
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
    // Cancelling an interaction also cancels its delayed inspector reveal.
    // Keep the existing token/feature mutation and undo semantics below.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
        _controller._editing.activeTool != MapCreatorController::Tool::InspectSelect)
        cancelPendingInspectorReveal();
    if (_tokenDragArmed &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
         _controller._editing.activeTool != MapCreatorController::Tool::InspectSelect ||
         _draggedTokenIndex < 0 || _draggedTokenIndex >= static_cast<int>(map.tokens.size())))
    {
        _tokenDragArmed = false;
        _draggedTokenIndex = -1;
        cancelPendingInspectorReveal();
        _controller._integration.statusMessage = uiTextForLocale("Token move cancelled.", "Tokenverschiebung abgebrochen.");
    }
    if (_tokenDragArmed && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (gm::mapui::mapCreatorShouldUpdateGeneratedFeatureDrag(false))
        {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const float dragDeltaX = (mouse.x - origin.x) / cellSize - _tokenDragStart.x;
            const float dragDeltaY = (mouse.y - origin.y) / cellSize - _tokenDragStart.y;
            const float targetX = std::clamp(_tokenDragStartX + dragDeltaX, 0.0f,
                                             static_cast<float>(std::max(0, map.width - 1)));
            const float targetY = std::clamp(_tokenDragStartY + dragDeltaY, 0.0f,
                                             static_cast<float>(std::max(0, map.height - 1)));
            MapTokenModel &token = map.tokens[static_cast<std::size_t>(_draggedTokenIndex)];
            if (std::fabs(token.x - targetX) > 0.0001f || std::fabs(token.y - targetY) > 0.0001f)
            {
                _controller.finishPropertyEditUndo();
                _controller.captureUndoSnapshot("Move Token");
                token.x = targetX;
                token.y = targetY;
                _controller.markDraftEdited();
                _controller._integration.statusMessage = uiTextForLocale("Token moved. Undo restores its previous position.",
                                                                           "Token verschoben. Rückgängig stellt die vorherige Position wieder her.");
            }
        }
        _tokenDragArmed = false;
        _draggedTokenIndex = -1;
    }
    if (_boxArmed)
    {
        if (_controller._editing.activeTool != MapCreatorController::Tool::InspectSelect ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
            _boxArmed = _boxDragging = false;
        else
        {
            const auto mouse = ImGui::GetIO().MousePos;
            if (gm::mapui::mapCreatorShouldUpdateGeneratedFeatureDrag(_boxDragging))
            {
                _boxDragging = true;
                _boxEnd = {std::clamp((mouse.x - origin.x) / cellSize, 0.f, static_cast<float>(map.width)),
                           std::clamp((mouse.y - origin.y) / cellSize, 0.f, static_cast<float>(map.height))};
            }
            if (_boxDragging)
            {
                const auto box = gm::mapui::mapSelectionBox(_boxStart, _boxEnd);
                const ImVec2 a(origin.x + box.left * cellSize, origin.y + box.top * cellSize),
                    b(origin.x + box.right * cellSize, origin.y + box.bottom * cellSize);
                drawList->AddRectFilled(a, b, IM_COL32(45, 185, 200, 35));
                drawList->AddRect(a, b, IM_COL32(70, 210, 220, 255), 0, 0, 2);
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if (_boxDragging)
                {
                    _areaCandidates =
                        gm::mapui::collectMapAreaSelection(map, gm::mapui::mapSelectionBox(_boxStart, _boxEnd));
                    _areaSelectionSerial = _controller._editing.authoringSerial;
                    _areaFilter = 0;
                    _areaSearch[0] = 0;
                    resetGeneratedFeatureGeometryInteraction();
                    _controller.clearObjectSelection();
                    _showCompactInspector=true;_resetInspectorScroll=true;
                    _boxSelectMode=false;
                    _controller._integration.statusMessage=uiTextForLocale(
                        "Area selected. Uncheck items in the sidebar; drag the selection to move it.",
                        "Bereich ausgewählt. Elemente in der Seitenleiste abwählen; Auswahl zum Verschieben ziehen.");
                }
                _boxArmed = _boxDragging = false;
            }
        }
    }
    if(_areaDragArmed && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
        _controller._editing.activeTool!=MapCreatorController::Tool::InspectSelect || _areaSelectionSerial!=_controller._editing.authoringSerial)) {
        _areaDragArmed=_areaDragging=false;_areaDeltaX=_areaDeltaY=0;
    }
    if(_areaDragArmed && gm::mapui::mapCreatorShouldUpdateGeneratedFeatureDrag(_areaDragging)) {
        _areaDragging=true;
        const auto p=ImGui::GetIO().MousePos;
        _areaDeltaX=int(std::round((p.x-origin.x)/cellSize-_areaDragStart.x));
        _areaDeltaY=int(std::round((p.y-origin.y)/cellSize-_areaDragStart.y));
    }
    if(_areaDragArmed && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if(_areaDragging && (_areaDeltaX || _areaDeltaY) && _areaSelectionSerial==_controller._editing.authoringSerial) {
            auto candidate=map;auto members=_areaCandidates;
            if(gm::mapui::moveAreaSelection(candidate,members,_areaDeltaX,_areaDeltaY)) {
                _controller.finishPropertyEditUndo();_controller.captureUndoSnapshot("Move Area Selection");
                map=std::move(candidate);_areaCandidates=std::move(members);_controller.markDraftEdited();
                _areaSelectionSerial=_controller._editing.authoringSerial;_geometryPreviewPending=true;
            } else _controller._integration.statusMessage=uiTextForLocale("The selection cannot move there. Nothing was changed.",
                "Die Auswahl kann dort nicht platziert werden. Es wurde nichts geändert.");
        }
        _areaDragArmed=_areaDragging=false;_areaDeltaX=_areaDeltaY=0;
    }
    if (_terrainDragArmed && gm::mapui::mapCreatorShouldUpdateGeneratedFeatureDrag(_terrainDragging))
    {
        _terrainDragging = true;
        const auto mouse = ImGui::GetIO().MousePos;
        _terrainDeltaX = static_cast<int>(std::round((mouse.x - origin.x) / cellSize - _terrainDragStart.x));
        _terrainDeltaY = static_cast<int>(std::round((mouse.y - origin.y) / cellSize - _terrainDragStart.y));
    }
    if (_terrainDragArmed && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (_terrainDragging && (_terrainDeltaX || _terrainDeltaY))
        {
            if (gm::mapui::terrainGroupCanMove(map, _terrainSelection, _terrainDeltaX, _terrainDeltaY))
            {
                _controller.finishPropertyEditUndo();
                _controller.captureUndoSnapshot("Move Terrain Area");
                gm::mapui::moveTerrainGroup(map, _terrainSelection, _terrainDeltaX, _terrainDeltaY);
                _controller.markDraftEdited();
                _terrainSelection =
                    gm::mapui::translatedTerrainSelection(map, _terrainSelection, _terrainDeltaX, _terrainDeltaY);
                _terrainSelectionSerial = _controller._editing.authoringSerial;
                _controller._integration.statusMessage = uiTextForLocale("Terrain area moved.", "Geländegebiet verschoben.");
            }
            else
                _controller._integration.statusMessage = uiTextForLocale(
                    "Choose clear land inside the map, away from water, places, roads and locked features.",
                    "Wähle freies Land innerhalb der Karte, ohne Wasser, Orte, Wege oder gesperrte Objekte.");
        }
        _terrainDragArmed = _terrainDragging = false;
        _terrainDeltaX = _terrainDeltaY = 0;
    }
    if ((_draggedGeneratedFeaturePoint >= 0 || _draggingWholeObject) &&
        gm::mapui::mapCreatorShouldUpdateGeneratedFeatureDrag(_generatedFeatureGeometryEditActive) &&
        _controller._editing.selectedGeneratedFeatureIndex >= 0 &&
        _controller._editing.selectedGeneratedFeatureIndex < static_cast<int>(map.generation.graph.features.size()))
    {
        MapGeneratedFeature &feature =
            map.generation.graph.features[static_cast<std::size_t>(_controller._editing.selectedGeneratedFeatureIndex)];
        if (mapGeneratedFeatureIsLocked(map, feature) ||
            (!_draggingWholeObject && _draggedGeneratedFeaturePoint >= static_cast<int>(feature.points.size())))
        {
            _draggedGeneratedFeaturePoint = -1;
            _draggingWholeObject = false;
        }
        else
        {
            if (!_generatedFeatureGeometryEditActive)
            {
                _controller.finishPropertyEditUndo();
                _controller.captureUndoSnapshot(_draggingWholeObject ? "Move Map Object"
                                                                     : "Move Generated Control Point");
                _generatedFeatureGeometryBeforeEdit = feature;
                _generatedFeatureGeometryEditId = feature.id;
                _generatedFeatureGeometryRenderSerial = _controller._editing.authoringSerial;
                _generatedFeatureGeometryEditActive = true;
            }
            if (_generatedFeatureGeometryEditId == feature.id)
            {
                const ImVec2 mousePosition = ImGui::GetIO().MousePos;
                const MapFeaturePoint target{
                    std::clamp((mousePosition.x - origin.x) / cellSize, 0.0f, static_cast<float>(map.width)),
                    std::clamp((mousePosition.y - origin.y) / cellSize, 0.0f, static_cast<float>(map.height))};
                if (_draggingWholeObject)
                {
                    if (gm::mapui::moveMapObject(map, _controller._editing.selectedGeneratedFeatureIndex,
                                                 _generatedFeatureGeometryBeforeEdit,
                                                 {target.x - _objectDragStart.x, target.y - _objectDragStart.y}))
                        _controller.markDraftEdited();
                }
                else if (mapFeaturePointDistanceSquared(feature.points[_draggedGeneratedFeaturePoint], target) >
                         0.000001f)
                {
                    moveGeneratedMapFeatureSection(map, _controller._editing.selectedGeneratedFeatureIndex,
                                                   _draggedGeneratedFeaturePoint, target,
                                                   _generatedFeatureGeometryBeforeEdit);
                    _controller.markDraftEdited();
                }
            }
        }
    }
    if ((_draggedGeneratedFeaturePoint >= 0 || _draggingWholeObject || _generatedFeatureGeometryEditActive) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (_generatedFeatureGeometryEditActive)
        {
            finishGeneratedFeatureGeometryEdit();
        }
        _draggedGeneratedFeaturePoint = -1;
        _draggingWholeObject = false;
    }
    if (_imageTraceActive && ImGui::IsKeyPressed(ImGuiKey_Escape))
        cancelImageTrace();
    if (_imageTraceDraggedPoint >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _imageTraceDraggedPoint = -1;
    if (_controller._editing.strokeInProgress && !ImGui::IsItemHovered())
    {
        _controller._editing.lastBrushX = -1;
        _controller._editing.lastBrushY = -1;
    }
    if (_controller._editing.strokeInProgress && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _controller.endBrushStroke();

    // All final coordinates and the original gesture's commit have now used
    // the unchanged canvas frame. The inspector may reflow the next frame.
    if (_pendingCanvasInspectorReveal && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        _pendingCanvasInspectorReveal = false;
        if (_controller.hasSelectedObject())
        {
            _showCompactInspector = true;
            _resetInspectorScroll = true;
        }
    }

    drawAreaSelectionOverlay(drawList, origin, cellSize);
}
