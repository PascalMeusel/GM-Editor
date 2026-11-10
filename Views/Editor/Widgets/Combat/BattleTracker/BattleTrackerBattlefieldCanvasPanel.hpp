
    bool fitRequested = _battleCanvasEncounterId != encounter.metadata.id ||
        _battleCanvasWidth != encounter.battlefieldWidth || _battleCanvasHeight != encounter.battlefieldHeight;
    if (fitRequested) _battleCanvasAutoFit = true;
    if (drawSmallUiButton("Fit map", UiButtonRole::Secondary))
    {
        fitRequested = true;
        _battleCanvasAutoFit = true;
    }
    ImGui::SameLine();
    float zoomFactor = 1.0f;
    if (ImGui::SmallButton("-##BattleZoomOut")) zoomFactor = 1.0f / 1.25f;
    showHelpTooltip("Zoom out");
    ImGui::SameLine();
    if (ImGui::SmallButton("+##BattleZoomIn")) zoomFactor = 1.25f;
    showHelpTooltip("Zoom in");
    ImGui::SameLine();
    ImGui::TextDisabled(localizedUiText("Zoom: %d%%"), static_cast<int>(std::lround(_controller._zoom * 100.0f)));
    showHelpTooltip(uiTextForLocale("Ctrl + mouse wheel zooms the map; wheel scrolls the page.",
                                    "Strg + Mausrad zoomt die Karte; das Mausrad scrollt die Seite."));

    if (!ImGui::BeginChild("BattlefieldCanvas", ImGui::GetContentRegionAvail(), true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::EndChild();
        return;
    }
    const ImVec2 viewport(std::max(1.0f, ImGui::GetWindowSize().x - 2.0f * ImGui::GetStyle().WindowPadding.x - ImGui::GetStyle().ScrollbarSize),
                          std::max(1.0f, ImGui::GetWindowSize().y - 2.0f * ImGui::GetStyle().WindowPadding.y - ImGui::GetStyle().ScrollbarSize));
    // Dear ImGui can expose a provisional one-pixel child during docking.
    // Keep initial fit pending until there is a real visible viewport.
    if (viewport.x < 40.0f || viewport.y < 40.0f)
    {
        ImGui::EndChild();
        return;
    }
    fitRequested |= _battleCanvasAutoFit &&
        (std::abs(viewport.x - _battleCanvasViewport.x) > 1.0f || std::abs(viewport.y - _battleCanvasViewport.y) > 1.0f);
    _battleCanvasEncounterId = encounter.metadata.id;
    _battleCanvasWidth = encounter.battlefieldWidth;
    _battleCanvasHeight = encounter.battlefieldHeight;
    _battleCanvasViewport = viewport;
    _controller._zoom = std::clamp(_controller._zoom, encounterCanvasMinimumZoom, encounterCanvasMaximumZoom);
    if (fitRequested)
    {
        _controller._zoom = encounterCanvasFitZoom(encounter.battlefieldWidth, encounter.battlefieldHeight, viewport.x, viewport.y);
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(0.0f);
    }
    else if (zoomFactor != 1.0f)
    {
        _battleCanvasAutoFit = false;
        const float previous = _controller._zoom;
        _controller._zoom = std::clamp(previous * zoomFactor, encounterCanvasMinimumZoom, encounterCanvasMaximumZoom);
        ImGui::SetScrollX(encounterCanvasZoomScroll(ImGui::GetScrollX(), viewport.x, previous, _controller._zoom));
        ImGui::SetScrollY(encounterCanvasZoomScroll(ImGui::GetScrollY(), viewport.y, previous, _controller._zoom));
    }
    const float zoomBeforeWheel = _controller._zoom;
    if (ImGui::GetIO().KeyCtrl)
        applyMapCanvasWheelZoom(_controller._zoom, encounterCanvasMinimumZoom, encounterCanvasMaximumZoom);
    if (_controller._zoom != zoomBeforeWheel) _battleCanvasAutoFit = false;
    const bool showTacticalOverlay = shouldShowMapTacticalOverlay();
    const float cellSize = 32.0f * _controller._zoom;
    const ImVec2 canvasSize(
        std::max(ImGui::GetContentRegionAvail().x, cellSize * static_cast<float>(encounter.battlefieldWidth) + 20.0f),
        std::max(ImGui::GetContentRegionAvail().y, cellSize * static_cast<float>(encounter.battlefieldHeight) + 20.0f));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(30, 34, 38, 255));

    const MapModel *illustratedMap = encounterMapRenderSource(encounter, _controller._currentCampaign.maps);
    const bool sharedAtlas = illustratedMap && illustratedMap->generation.continuousRendering &&
        !illustratedMap->importedReference.enabled && illustratedMap->backgroundImagePath.empty();
    if (sharedAtlas)
    {
        const auto request = encounterGameMasterArtRequest(std::string(gm::localization::localeCode(
            gm::localization::LocalizationService::instance().locale())));
        // This is the GM combat canvas. Fog remains a visible GM overlay;
        // player publication uses its separate reviewed, fail-closed plan.
        const auto plan = _battleRenderPlanCache.getOrCompile(*illustratedMap, request, 0, cellSize);
        drawList->PushClipRect(origin,
            ImVec2(origin.x + encounter.battlefieldWidth * cellSize,
                   origin.y + encounter.battlefieldHeight * cellSize), true);
        renderMap(drawList, origin, *illustratedMap, *plan, cellSize);
        drawList->PopClipRect();
    }

    for (int y = 0; y < encounter.battlefieldHeight; ++y)
    {
        for (int x = 0; x < encounter.battlefieldWidth; ++x)
        {
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            const MapCellModel *cell = encounterCellAt(encounter, x, y);
            if (!sharedAtlas)
                drawMapCellResolvedSemanticLayers(
                drawList,
                min,
                max,
                cell,
                x,
                y,
                encounterCellAt(encounter, x, y - 1),
                encounterCellAt(encounter, x + 1, y),
                encounterCellAt(encounter, x, y + 1),
                encounterCellAt(encounter, x - 1, y),
                encounterCellAt(encounter, x + 1, y - 1),
                encounterCellAt(encounter, x + 1, y + 1),
                encounterCellAt(encounter, x - 1, y + 1),
                encounterCellAt(encounter, x - 1, y - 1),
                true);
            if (sharedAtlas && cell && cell->fogged)
                drawList->AddRectFilled(min, max, IM_COL32(42, 48, 56, 150));
            if (showTacticalOverlay)
                drawMapCellTacticalOverlay(drawList, min, max, cell, cellSize);
            drawMapTileGrid(drawList, min, max, cellSize);
        }
    }
