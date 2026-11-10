#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::handleMapCanvasPrimaryInput(const CanvasFrame &frame)
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
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("MapCreatorCanvasHitbox", ImVec2(cellSize * map.width, cellSize * map.height),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool regionConsumedInput = handleRegionCanvasInteraction(origin, cellSize, ImGui::IsItemHovered());
    if (ImGui::IsItemHovered() && !regionConsumedInput)
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const int x = static_cast<int>((mouse.x - origin.x) / cellSize);
        const int y = static_cast<int>((mouse.y - origin.y) / cellSize);
        drawMapCanvasToolPreview(frame, mouse, x, y);

        const ImGuiIO &io = ImGui::GetIO();
        if (_imageTraceActive)
        {
            const MapFeaturePoint mousePoint = MapImageTraceAuthoring::constrainPoint(
                {(mouse.x - origin.x) / cellSize, (mouse.y - origin.y) / cellSize}, map.width, map.height,
                _imageTraceSnap);
            int nearestPoint = -1;
            float nearestDistanceSquared = 100.0f;
            for (int pointIndex = 0; pointIndex < static_cast<int>(_imageTraceControlPoints.size()); ++pointIndex)
            {
                const MapFeaturePoint &point = _imageTraceControlPoints[static_cast<std::size_t>(pointIndex)];
                const float screenDx = (point.x - mousePoint.x) * cellSize;
                const float screenDy = (point.y - mousePoint.y) * cellSize;
                const float distanceSquared = screenDx * screenDx + screenDy * screenDy;
                if (distanceSquared < nearestDistanceSquared)
                {
                    nearestDistanceSquared = distanceSquared;
                    nearestPoint = pointIndex;
                }
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                if (!_imageTraceControlPoints.empty())
                    _imageTraceControlPoints.pop_back();
                _imageTraceDraggedPoint = -1;
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (nearestPoint >= 0)
                {
                    _imageTraceDraggedPoint = nearestPoint;
                }
                else
                {
                    const MapImageTraceAuthoring::Mode traceMode = imageTraceMode(_imageTraceModeIndex);
                    if (!MapImageTraceAuthoring::isLine(traceMode) && !MapImageTraceAuthoring::isClosed(traceMode))
                        _imageTraceControlPoints.clear();
                    _imageTraceControlPoints.push_back(mousePoint);
                    _imageTraceDraggedPoint = static_cast<int>(_imageTraceControlPoints.size()) - 1;
                }
            }
            if (_imageTraceDraggedPoint >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                _imageTraceDraggedPoint < static_cast<int>(_imageTraceControlPoints.size()))
                _imageTraceControlPoints[static_cast<std::size_t>(_imageTraceDraggedPoint)] = mousePoint;
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                _imageTraceDraggedPoint = -1;
        }
        else if (_regenerationPickingArea)
        {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                _regenerationPickingArea = false;
                _regenerationAreaDragging = false;
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                _regenerationAreaStartX = x;
                _regenerationAreaStartY = y;
                _regenerationAreaEndX = x;
                _regenerationAreaEndY = y;
                _regenerationAreaDragging = true;
            }
            else if (_regenerationAreaDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                _regenerationAreaEndX = x;
                _regenerationAreaEndY = y;
            }
            else if (_regenerationAreaDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                _regenerationAreaEndX = x;
                _regenerationAreaEndY = y;
                _regenerationAreaDragging = false;
                _regenerationPickingArea = false;
            }
        }
        else if (!_controller._generation.inProgress &&
                 _controller._editing.activeTool == MapCreatorController::Tool::InspectSelect &&
                 ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt)
        {
            const MapFeaturePoint mapPoint{
                std::clamp((mouse.x - origin.x) / cellSize, 0.0f, static_cast<float>(map.width)),
                std::clamp((mouse.y - origin.y) / cellSize, 0.0f, static_cast<float>(map.height))};
            const float hitRadiusCells = std::clamp(11.0f / std::max(1.0f, cellSize), 0.18f, 1.15f);
            const auto displayedPreview = _canvasPreviewCompiler.state();
            const MapGeneratedFeatureHit hit = gm::mapui::hitTestMapObject(
                map, mapPoint, hitRadiusCells, _controller._editing.selectedGeneratedFeatureIndex, _objectEditMode,
                displayedPreview.plan.get());

            // Tokens render over generated terrain and are the most direct
            // authoring target at their visible center. Pick the topmost token
            // first so a road, forest, or landmark underneath cannot steal a
            // select-drag gesture.
            int tokenHit = -1;
            for (int index = static_cast<int>(map.tokens.size()) - 1; index >= 0; --index)
            {
                const MapTokenModel &token = map.tokens[static_cast<std::size_t>(index)];
                const float radius = 0.43f * std::max(0.5f, token.size);
                const float deltaX = mapPoint.x - (token.x + 0.5f);
                const float deltaY = mapPoint.y - (token.y + 0.5f);
                if (deltaX * deltaX + deltaY * deltaY <= radius * radius)
                {
                    tokenHit = index;
                    break;
                }
            }

            const bool terrainHit =
                hit.featureIndex < 0 ||
                map.generation.graph.features[hit.featureIndex].kind == MapGeneratedFeatureKind::Vegetation ||
                map.generation.graph.features[hit.featureIndex].kind == MapGeneratedFeatureKind::MountainRange ||
                map.generation.graph.features[hit.featureIndex].kind == MapGeneratedFeatureKind::TerrainRegion;
            auto terrainGroup = _terrainSelection.contains(x, y) ? _terrainSelection
                                : terrainHit                     ? gm::mapui::selectTerrainGroup(map, x, y)
                                                                 : gm::mapui::TerrainGroupSelection{};
            if (tokenHit >= 0)
            {
                _areaCandidates.clear();
                resetGeneratedFeatureGeometryInteraction();
                _terrainDragArmed = _terrainDragging = false;
                _controller.selectTokenObject(tokenHit);
                _pendingCanvasInspectorReveal = true;
                _resetInspectorScroll = true;
                _draggedTokenIndex = tokenHit;
                _tokenDragStart = mapPoint;
                _tokenDragStartX = map.tokens[static_cast<std::size_t>(tokenHit)].x;
                _tokenDragStartY = map.tokens[static_cast<std::size_t>(tokenHit)].y;
                _tokenDragArmed = true;
                _controller._integration.statusMessage = uiTextForLocale(
                    "Token selected. Drag it to move; release to place it, or press Escape to cancel.",
                    "Token ausgewählt. Ziehe ihn zum Verschieben, lasse zum Platzieren los oder drücke Escape zum Abbrechen.");
            }
            else if (!_boxSelectMode && _areaSelectionSerial==_controller._editing.authoringSerial &&
                     gm::mapui::areaSelectionContains(map,_areaCandidates,mapPoint,hitRadiusCells))
            {
                _areaDragArmed=true;_areaDragging=false;_areaDragStart=mapPoint;_areaDeltaX=_areaDeltaY=0;
            }
            else if (_boxSelectMode)
            {
                resetGeneratedFeatureGeometryInteraction();
                _controller.clearObjectSelection();
                _boxStart = _boxEnd = mapPoint;
                _boxArmed = true;
                _boxDragging = false;
            }
            else if (!terrainGroup.cells.empty())
            {
                _areaCandidates.clear();
                resetGeneratedFeatureGeometryInteraction();
                _controller.clearObjectSelection();
                _terrainSelection = std::move(terrainGroup);
                _terrainSelectionSerial = _controller._editing.authoringSerial;
                _terrainDragStart = mapPoint;
                _terrainDeltaX = _terrainDeltaY = 0;
                _terrainDragArmed = true;
                _controller._integration.statusMessage =
                    uiTextForLocale("Terrain area selected. Drag the highlighted area as a group.",
                                    "Geländegebiet ausgewählt. Ziehe die markierte Fläche als Gruppe.");
            }
            else if (hit.featureIndex >= 0)
            {
                _areaCandidates.clear();
                if (_controller._editing.selectedGeneratedFeatureIndex != hit.featureIndex)
                    resetGeneratedFeatureGeometryInteraction();
                _controller.selectGeneratedFeatureObject(hit.featureIndex);
                _pendingCanvasInspectorReveal = true;
                _selectedGeneratedFeaturePoint =
                    _objectEditMode == gm::mapui::MapObjectEditMode::Reshape ? hit.pointIndex : -1;

                MapGeneratedFeature &feature =
                    map.generation.graph.features[static_cast<std::size_t>(hit.featureIndex)];
                if (mapGeneratedFeatureIsLocked(map, feature))
                {
                    _draggedGeneratedFeaturePoint = -1;
                    _draggingWholeObject = false;
                    _controller._integration.statusMessage =
                        feature.lineage.sourceMapId.empty()
                            ? gm::localization::trUi("This feature is locked. Unlock it before changing its geometry.")
                            : gm::localization::trUi(
                                  "This inherited atlas feature is locked. Detach or unlock it before changing its geometry.");
                }
                else if (_objectEditMode == gm::mapui::MapObjectEditMode::Move)
                {
                    _draggedGeneratedFeaturePoint = -1;
                    _draggingWholeObject = gm::mapui::mapObjectCanMove(feature);
                    _objectDragStart = mapPoint;
                    _controller._integration.statusMessage =
                        _draggingWholeObject
                            ? uiTextForLocale(
                                  "Drag the selected object to move it. Use Reshape to change a route.",
                                  "Ziehe das ausgewählte Objekt zum Verschieben. Mit Umformen änderst du den Verlauf.")
                            : uiTextForLocale(
                                  "This outline does not select terrain cells. Click terrain or draw a selection box to choose cells to move.",
                                  "Dieser Umriss wählt keine Geländezellen aus. Klicke auf Gelände oder ziehe einen Auswahlrahmen, um Zellen zum Verschieben zu wählen.");
                }
                else if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && hit.pointIndex < 0 &&
                         hit.segmentStartIndex >= 0)
                {
                    const MapGeneratedFeature beforeEdit = feature;
                    _controller.finishPropertyEditUndo();
                    _controller.captureUndoSnapshot("Insert Generated Control Point");
                    const int insertedPoint =
                        insertGeneratedMapFeaturePoint(feature, hit.segmentStartIndex, hit.segmentAmount);
                    if (insertedPoint >= 0)
                    {
                        _selectedGeneratedFeaturePoint = insertedPoint;
                        reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map, beforeEdit);
                        _controller._integration.statusMessage =
                            gm::localization::trUi("A control point was added. Drag it to reshape the feature.");
                    }
                    _draggedGeneratedFeaturePoint = -1;
                }
                else
                {
                    _draggedGeneratedFeaturePoint = hit.pointIndex;
                    if (hit.pointIndex >= 0)
                        _controller._integration.statusMessage =
                            gm::localization::trUi("Drag this control point to reshape the feature.");
                    else
                        _controller._integration.statusMessage =
                            gm::localization::trUi("Double-click this segment to add a control point.");
                }
            }
            else
            {
                resetGeneratedFeatureGeometryInteraction();
                _controller.applyActiveToolAt(x, y);
                if (!_controller.hasSelectedObject())
                {
                    _boxStart = _boxEnd = mapPoint;
                    _boxArmed = true;
                    _boxDragging = false;
                }
            }
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            _controller.cancelActiveTool();
        }
        else if (_controller._editing.activeTool == MapCreatorController::Tool::InspectSelect &&
                 ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyCtrl)
            _controller.addTokenAt(x, y);
        else if (_controller._editing.activeTool == MapCreatorController::Tool::InspectSelect &&
                 ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyShift)
            _controller.addFeatureAt(x, y);
        else if (_controller._editing.activeTool == MapCreatorController::Tool::InspectSelect &&
                 ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt)
            _controller.addRegionAt(x, y);
        else if (!io.KeyCtrl && !io.KeyShift && !io.KeyAlt)
        {
            if (_controller.activeToolUsesBrush())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    _controller.beginBrushStroke();
                if ((ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) &&
                    _controller._editing.strokeInProgress)
                {
                    const ImVec2 mouse = ImGui::GetIO().MousePos;
                    const MapFeaturePoint point{(mouse.x - origin.x) / cellSize, (mouse.y - origin.y) / cellSize};
                    _controller.applyActiveToolAt(x, y, &point);
                }
            }
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                _controller.applyActiveToolAt(x, y);
            }
        }
    }
}
