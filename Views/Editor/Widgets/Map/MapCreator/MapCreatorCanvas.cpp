#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::cancelPendingInspectorReveal()
{
    // A reset may precede the next palette draw. Consume the selected summary
    // so its automatic reveal cannot resurrect this cancelled request.
    if (_pendingCanvasInspectorReveal) _lastSelectionSummary = localizedSelectedObjectSummary();
    _pendingCanvasInspectorReveal = false;
}

void MapCreatorView::resetGeneratedFeatureGeometryInteraction()
{
    cancelPendingInspectorReveal();
    _terrainSelection = {};
    _terrainDragArmed = _terrainDragging = false;
    if (_generatedFeatureGeometryEditActive)
        finishGeneratedFeatureGeometryEdit();
    _selectedGeneratedFeaturePoint = -1;
    _draggedGeneratedFeaturePoint = -1;
    _draggingWholeObject = false;
    _objectEditMode = gm::mapui::MapObjectEditMode::Move;
    _generatedFeatureGeometryEditId.clear();
    _generatedFeatureGeometryEditActive = false;
}

void MapCreatorView::finishGeneratedFeatureGeometryEdit()
{
    if (!_generatedFeatureGeometryEditActive)
        return;

    const bool applied =
        finishGeneratedMapFeatureGeometryEdit(_controller._draftMap, _generatedFeatureGeometryBeforeEdit);
    _controller._integration.statusMessage =
        applied
            ? gm::localization::trUi("The feature geometry was updated.")
            : (_generatedFeatureGeometryBeforeEdit.kind == MapGeneratedFeatureKind::Settlement
                   ? uiTextForLocale(
                         "Choose clear land with space around the city. The previous position was kept.",
                         "Wähle freies Land mit Platz um die Stadt. Die bisherige Position wurde beibehalten.")
                   : gm::localization::trUi(
                         "The building needs a separate, dry footprint clear of routes. Its previous position was kept."));
    _controller.markDraftEdited();
    _geometryPreviewPending = true;
    _generatedFeatureGeometryEditActive = false;
    _generatedFeatureGeometryEditId.clear();
}

bool MapCreatorView::removeSelectedGeneratedFeaturePoint()
{
    MapModel &map = _controller._draftMap;
    const int featureIndex = _controller._editing.selectedGeneratedFeatureIndex;
    if (featureIndex < 0 || featureIndex >= static_cast<int>(map.generation.graph.features.size()))
        return false;

    MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
    if (_selectedGeneratedFeaturePoint < 0 ||
        _selectedGeneratedFeaturePoint >= static_cast<int>(feature.points.size()) || feature.locked)
        return false;
    const std::size_t minimumPoints = feature.closed ? 3u : 2u;
    if (feature.points.size() <= minimumPoints)
        return false;

    const MapGeneratedFeature beforeEdit = feature;
    _controller.finishPropertyEditUndo();
    _controller.captureUndoSnapshot("Remove Generated Control Point");
    if (!removeGeneratedMapFeaturePoint(feature, _selectedGeneratedFeaturePoint))
        return false;
    reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map, beforeEdit);
    _controller.markDraftEdited();_geometryPreviewPending=true;
    _selectedGeneratedFeaturePoint =
        std::min(_selectedGeneratedFeaturePoint, static_cast<int>(feature.points.size()) - 1);
    _draggedGeneratedFeaturePoint = -1;
    _controller._integration.statusMessage = gm::localization::trUi("The control point was removed.");
    return true;
}

void MapCreatorView::drawMapCanvas(bool compactInspector)
{
    MapModel &map = _controller._draftMap;
    const std::size_t expectedCellCount =
        static_cast<std::size_t>(std::max(4, map.width)) * static_cast<std::size_t>(std::max(4, map.height));
    if (map.width < 4 || map.height < 4 || map.cells.size() != expectedCellCount)
        resizeMapCells(map);

    const bool generatedSelectionValid =
        _controller._editing.selectedGeneratedFeatureIndex >= 0 &&
        _controller._editing.selectedGeneratedFeatureIndex < static_cast<int>(map.generation.graph.features.size());
    if (!generatedSelectionValid && (_selectedGeneratedFeaturePoint >= 0 || _draggedGeneratedFeaturePoint >= 0 ||
                                     _draggingWholeObject || _generatedFeatureGeometryEditActive))
        resetGeneratedFeatureGeometryInteraction();

    const std::string mode =
        _imageTraceActive ? localizedUiLabel("Trace image") : localizedUiLabel(_controller.activeToolLabel());
    const std::string selection = _imageTraceActive
                                      ? localizedUiLabel(imageTraceDefaultName(imageTraceMode(_imageTraceModeIndex)))
                                      : localizedActiveSelectionSummary();
    const std::string nextAction = _imageTraceActive
                                       ? localizedUiLabel("Click to add points; drag existing points to adjust them.")
                                       : localizedUiLabel(_controller.nextActionHint());
    // One contextual row above the canvas. The active tool is already visible
    // in the palette; detailed instructions/status belong in its help popup.
    const float controlsRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const auto beside = [&](float width) {
        if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width <= controlsRight)
            ImGui::SameLine();
    };
    const auto buttonWidth = [](const char *label) {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
    };
    if (drawSmallUiButton("Fit map", UiButtonRole::Secondary))
    {
        _fitCanvasOnNextDraw = true;
        _fitCanvasDelayFrames = 0;
    }
    const float zoomWidth = std::min(150.f, ImGui::GetContentRegionAvail().x * .35f);
    beside(zoomWidth);
    ImGui::SetNextItemWidth(zoomWidth);
    int zoomPercent = static_cast<int>(std::lround(_controller._editing.zoom * 100));
    if (ImGui::SliderInt("##CanvasZoom", &zoomPercent, static_cast<int>(mapCreatorMinimumZoom(map.atlasLevel) * 100),
                         300, "%d%%"))
        _controller._editing.zoom = zoomPercent / 100.f;
    showHelpTooltip(uiTextForLocale("Mouse wheel to zoom. Middle-drag to pan.",
                                    "Mausrad zum Zoomen. Mittlere Maustaste zum Verschieben."));
    if (compactInspector)
    {
        beside(buttonWidth(uiTextForLocale("Tools", "Werkzeuge")) + ImGui::GetFrameHeight());
        if (FormUi::Checkbox(uiTextForLocale("Tools", "Werkzeuge"), &_showCompactInspector))
        {
            cancelPendingInspectorReveal();
            if (_showCompactInspector) _resetInspectorScroll = true;
        }
    }
    beside(buttonWidth(uiTextForLocale("View", "Ansicht")));
    if (drawSmallUiButton(uiTextForLocale("View", "Ansicht"), UiButtonRole::Secondary))
        ImGui::OpenPopup("MapCreatorCanvasInformation");
    ImGui::SetNextWindowSize(ImVec2(std::min(440.0f, ImGui::GetMainViewport()->WorkSize.x - 40.0f), 0.0f));
    if (ImGui::BeginPopup("MapCreatorCanvasInformation"))
    {
        ImGui::TextWrapped("%s", map.name.c_str());
        if (map.atlasLevel == MapLevel::World)
            FormUi::Checkbox(uiTextForLocale("Political borders", "Politische Grenzen"), &_showWorldPoliticalOverlay);
        FormUi::Checkbox(uiTextForLocale("Editing grid", "Bearbeitungsraster"), &_controller._editing.showEditorGrid);
        if (map.generation.continuousRendering && ImGui::TreeNode(uiTextForLocale("Map key", "Legende")))
        {
            drawGenerationPreviewLegend();
            ImGui::TreePop();
        }
        if (ImGui::TreeNode(uiTextForLocale("Help with this tool", "Hilfe zum Werkzeug")))
        {
            ImGui::TextWrapped("%s · %s", mode.c_str(), selection.c_str());
            ImGui::TextWrapped("%s", nextAction.c_str());
            ImGui::TextWrapped("%s", localizedUiLabel(_controller.toolHelpText()).c_str());
            if (!_controller._integration.statusMessage.empty())
                ImGui::TextWrapped("%s", localizedUiLabel(_controller._integration.statusMessage.c_str()).c_str());
            ImGui::TreePop();
        }
        ImGui::EndPopup();
    }
    if (!map.generation.continuousRendering)
    {
        ImGui::TextWrapped(
            "%s",
            uiTextForLocale("Grid map. Choose another design in the Map menu for illustrated creation.",
                            "Rasterkarte. Wähle im Kartenmenü einen anderen Entwurf für die illustrierte Erstellung."));
        showHelpTooltip(uiTextForLocale(
            "Your current draft is retained. Return to current map resumes editing it.",
            "Der aktuelle Entwurf bleibt erhalten. Zurück zur aktuellen Karte setzt die Bearbeitung fort."));
    }
    const bool showTechnicalFeatureLegend = map.atlasLevel == MapLevel::Tactical ||
                                            _controller._editing.activeTool == MapCreatorController::Tool::AddFeature ||
                                            _controller._editing.activeTool == MapCreatorController::Tool::PaintTacticalEdge ||
                                            _controller._editing.selectedFeatureIndex >= 0;
    if (!map.features.empty() && showTechnicalFeatureLegend && _showTileLegend)
    {
        const std::string markerLegend = formatLocalizedUi(
            "D %s   |   T %s   |   ! %s   |   P %s", localizedUiLabel("Door").c_str(), localizedUiLabel("Trap").c_str(),
            localizedUiLabel("Hazard").c_str(), localizedUiLabel("Map Pin").c_str());
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), markerLegend.c_str());
    }

    // Keep a useful canvas width in narrow docks. The element combo in the
    // controls remains available when there is not enough room for the legend.
    const bool showLegend = _showTileLegend && _controller._editing.activeTool == MapCreatorController::Tool::PaintTile &&
                            ImGui::GetContentRegionAvail().x >= 620.0f;
    if (!ImGui::BeginTable("MapCreatorCanvasWithLegend", showLegend ? 2 : 1,
                           ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Map").c_str(), ImGuiTableColumnFlags_WidthStretch);
    if (showLegend)
        ImGui::TableSetupColumn(localizedUiLabel("Legend").c_str(), ImGuiTableColumnFlags_WidthFixed, 270.0f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::BeginChild("MapCreatorCanvas", ImGui::GetContentRegionAvail(), true,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 availableCanvas(std::max(1.0f, ImGui::GetContentRegionAvail().x),
                                 std::max(1.0f, ImGui::GetContentRegionAvail().y));
    const float minimumZoom = mapCreatorMinimumZoom(map.atlasLevel);
    _controller._editing.zoom = std::clamp(_controller._editing.zoom, minimumZoom, 3.0f);
    if (_fitCanvasOnNextDraw && _fitCanvasDelayFrames > 0)
        --_fitCanvasDelayFrames;
    else if (_fitCanvasOnNextDraw && availableCanvas.x >= 120.0f && availableCanvas.y >= 120.0f)
    {
        const float fitCellSize =
            std::min(std::max(1.0f, availableCanvas.x - 24.0f) / static_cast<float>(std::max(1, map.width)),
                     std::max(1.0f, availableCanvas.y - 24.0f) / static_cast<float>(std::max(1, map.height)));
        _controller._editing.zoom = std::clamp(fitCellSize / 26.0f, minimumZoom, 3.0f);
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(0.0f);
        _fitCanvasOnNextDraw = false;
    }
    if (ImGui::GetIO().KeyCtrl)
        applyMapCanvasWheelZoom(_controller._editing.zoom, minimumZoom, 3.0f);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
    {
        ImGui::SetScrollX(ImGui::GetScrollX() - ImGui::GetIO().MouseDelta.x);
        ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseDelta.y);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    const bool showTacticalOverlay = shouldShowMapTacticalOverlay() ||
                                     _controller._editing.activeTool == MapCreatorController::Tool::PaintElevation ||
                                     _controller._editing.activeTool == MapCreatorController::Tool::PaintTacticalEdge;
    const bool showEditorGrid = _controller._editing.showEditorGrid || map.atlasLevel == MapLevel::Tactical;
    const float cellSize = 26.0f * _controller._editing.zoom;
    const ImVec2 canvasSize(std::max(availableCanvas.x, cellSize * static_cast<float>(map.width) + 20.0f),
                            std::max(availableCanvas.y, cellSize * static_cast<float>(map.height) + 20.0f));
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 origin(canvasOrigin.x + std::max(0.0f, (availableCanvas.x - cellSize * map.width) * 0.5f),
                        canvasOrigin.y + std::max(0.0f, (availableCanvas.y - cellSize * map.height) * 0.5f));
    _interactionCanvasOrigin=origin;_interactionCanvasCellSize=cellSize;
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 visibleMinimum = drawList->GetClipRectMin();
    const ImVec2 visibleMaximum = drawList->GetClipRectMax();
    LocalPlayerDisplayCanvasCameraInput cameraInput;
    cameraInput.mapOriginX = origin.x;
    cameraInput.mapOriginY = origin.y;
    cameraInput.mapPixelWidth = cellSize * static_cast<float>(map.width);
    cameraInput.mapPixelHeight = cellSize * static_cast<float>(map.height);
    cameraInput.visibleMinimumX = visibleMinimum.x;
    cameraInput.visibleMinimumY = visibleMinimum.y;
    cameraInput.visibleMaximumX = visibleMaximum.x;
    cameraInput.visibleMaximumY = visibleMaximum.y;
    cameraInput.viewportWidth = availableCanvas.x;
    cameraInput.viewportHeight = availableCanvas.y;
    cameraInput.cellSize = cellSize;
    cameraInput.mapWidth = map.width;
    cameraInput.mapHeight = map.height;
    LocalPlayerDisplayCameraState canvasCamera;
    if (localPlayerDisplayCameraFromCanvas(cameraInput, canvasCamera))
        _controller.updateViewCameraSnapshot(map.metadata.id, canvasCamera);
    drawList->AddRectFilled(canvasOrigin, ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y),
                            IM_COL32(32, 36, 40, 255));
    const bool continuousRendering = map.generation.continuousRendering;
    const std::string renderLocale(
        gm::localization::localeCode(gm::localization::LocalizationService::instance().locale()));
    bool importedBackgroundDrawn = false;
    MapRenderPlanRequest backgroundRequest;
    backgroundRequest.audience = MapRenderAudience::GameMaster;
    backgroundRequest.locale = renderLocale;
    backgroundRequest.layers = MapRenderPlanBackground;
    backgroundRequest.applyFog = false;
    const auto objectCanvasVersion = gm::mapui::mapObjectCanvasVersion(
        _controller._editing.authoringSerial, _generatedFeatureGeometryEditActive, _generatedFeatureGeometryRenderSerial);
    // Only GM canvas plans reuse pre-stroke art. Live brush overlays show the
    // edit immediately; player review/exports still use current authoring data.
    const auto canvasContentVersion = gm::mapui::mapObjectCanvasVersion(
        objectCanvasVersion, _controller._editing.strokeInProgress, _controller._editing.brushStrokeRenderSerial);
    const MapModel &canvasSource = _controller._editing.strokeInProgress ? _controller._editing.brushRenderSnapshot.source(map) : map;
    const std::shared_ptr<const MapRenderPlan> cachedBackgroundPlan =
        (canvasSource.importedReference.enabled || !canvasSource.backgroundImagePath.empty())
            ? _renderPlanCache.getOrCompile(canvasSource, backgroundRequest, canvasContentVersion, cellSize)
            : nullptr;
    if (cachedBackgroundPlan && cachedBackgroundPlan->backgroundVisible)
    {
        const MapRenderPlan &backgroundPlan = *cachedBackgroundPlan;
        const CachedImageTexture &texture = getCachedImageTexture(
            ProjectAssets::resolveProjectPath(backgroundPlan.backgroundInteractiveProjectRelativePath),
            backgroundPlan.backgroundInteractiveContentHash);
        if (texture.loaded)
        {
            const MapNormalizedRect crop =
                map.importedReference.enabled ? map.importedReference.crop : MapNormalizedRect{0.0f, 0.0f, 1.0f, 1.0f};
            const float radians = -(map.importedReference.enabled ? map.importedReference.rotationDegrees : 0.0f) *
                                  0.01745329251994329577f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            const auto uv = [&](float x, float y) {
                const float centeredX = x - 0.5f;
                const float centeredY = y - 0.5f;
                const float rotatedX = cosine * centeredX - sine * centeredY + 0.5f;
                const float rotatedY = sine * centeredX + cosine * centeredY + 0.5f;
                return ImVec2(crop.x + rotatedX * crop.width, crop.y + rotatedY * crop.height);
            };
            const ImVec2 maximum(origin.x + cellSize * map.width, origin.y + cellSize * map.height);
            drawList->AddImageQuad(texture.textureId, origin, ImVec2(maximum.x, origin.y), maximum,
                                   ImVec2(origin.x, maximum.y), uv(0.0f, 0.0f), uv(1.0f, 0.0f), uv(1.0f, 1.0f),
                                   uv(0.0f, 1.0f), IM_COL32_WHITE);
            importedBackgroundDrawn = true;
        }
    }
    const MapVisibleCellBounds visibleCells = mapVisibleCellBounds(drawList, origin, cellSize, map, 2);
    const float visibleMapLeft = std::max(visibleMinimum.x, origin.x);
    const float visibleMapTop = std::max(visibleMinimum.y, origin.y);
    const float visibleMapRight = std::min(visibleMaximum.x, origin.x + cellSize * static_cast<float>(map.width));
    const float visibleMapBottom = std::min(visibleMaximum.y, origin.y + cellSize * static_cast<float>(map.height));
    if (cellSize > 0.0f && visibleMapRight > visibleMapLeft && visibleMapBottom > visibleMapTop)
    {
        const int firstVisibleX =
            std::clamp(static_cast<int>(std::floor((visibleMapLeft - origin.x) / cellSize)), 0, map.width);
        const int firstVisibleY =
            std::clamp(static_cast<int>(std::floor((visibleMapTop - origin.y) / cellSize)), 0, map.height);
        const int endVisibleX =
            std::clamp(static_cast<int>(std::ceil((visibleMapRight - origin.x) / cellSize)), 0, map.width);
        const int endVisibleY =
            std::clamp(static_cast<int>(std::ceil((visibleMapBottom - origin.y) / cellSize)), 0, map.height);
        _controller._transfer.exportViewportRect = {firstVisibleX, firstVisibleY, std::max(0, endVisibleX - firstVisibleX),
                                           std::max(0, endVisibleY - firstVisibleY)};
        _controller._transfer.exportViewportValid =
            _controller._transfer.exportViewportRect.width > 0 && _controller._transfer.exportViewportRect.height > 0;
    }
    else
    {
        _controller._transfer.exportViewportRect = {};
        _controller._transfer.exportViewportValid = false;
        _controller._transfer.exportCurrentViewport = false;
    }
    gm::map_creator_view_detail::CanvasFrame frame;
    frame.map = &map;
    frame.canvasSource = &canvasSource;
    frame.drawList = drawList;
    frame.origin = origin;
    frame.canvasOrigin = canvasOrigin;
    frame.cellSize = cellSize;
    frame.visibleCells = visibleCells;
    frame.showTacticalOverlay = showTacticalOverlay;
    frame.showEditorGrid = showEditorGrid;
    frame.continuousRendering = continuousRendering;
    frame.importedBackgroundDrawn = importedBackgroundDrawn;
    frame.renderLocale = renderLocale;
    frame.canvasContentVersion = canvasContentVersion;
    drawMapCanvasTerrain(frame);
    drawMapCanvasObjects(frame);
    drawMapCanvasSelection(frame);
    handleMapCanvasPrimaryInput(frame);
    finishMapCanvasGestures(frame);
    ImGui::EndChild();

    if (showLegend)
    {
        ImGui::TableNextColumn();
        drawSelectableTileLegend(_controller.getPalette());
    }
    ImGui::EndTable();
}
