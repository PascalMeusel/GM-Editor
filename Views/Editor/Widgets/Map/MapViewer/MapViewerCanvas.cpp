#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

#include "Views/Editor/Widgets/Map/MapViewer/MapViewerCanvasFrame.hpp"

void MapViewerView::drawMapCanvas()
{
    MapModel *map = _controller.getSelectedMap();
    if (!map)
        return;

    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const float detailWidth = std::min(std::max(ImGui::GetFontSize() * 19.0f, 300.0f), std::max(300.0f, contentWidth * 0.46f));
    const bool detailsBesideMap = _showDetails && !_canvasFocus && contentWidth >= 760.0f;
    if (!ImGui::BeginTable("MapViewerCanvasWithLegendResponsive", detailsBesideMap ? 2 : 1, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Map").c_str(), ImGuiTableColumnFlags_WidthStretch);
    if (detailsBesideMap)
        ImGui::TableSetupColumn(localizedUiLabel("Legend").c_str(), ImGuiTableColumnFlags_WidthFixed, detailWidth);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    // Keep the map viewport inside the visible editor height. Filling the
    // table's remaining content height can place a centered map below the
    // screen on a 1280x720 window, leaving an apparently empty canvas.
    const float viewportHeight = std::max(120.0f, ImGui::GetContentRegionAvail().y - 8.0f);
    ImGui::BeginChild("MapViewerCanvas", ImVec2(0.0f, viewportHeight), true,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    constexpr float baseCellSize = 28.0f;
    constexpr float canvasPadding = 20.0f;
    const ImGuiStyle &style = ImGui::GetStyle();
    const ImVec2 childWindowSize = ImGui::GetWindowSize();
    const ImVec2 availableCanvas(
        std::max(1.0f, childWindowSize.x - style.WindowPadding.x * 2.0f - style.ScrollbarSize),
        std::max(1.0f, childWindowSize.y - style.WindowPadding.y * 2.0f - style.ScrollbarSize));
    // A newly docked ImGui child can report its one-frame bootstrap size here.
    // Keep the request alive until the canvas has reached a useful layout size;
    // otherwise a small map is incorrectly clamped to the minimum zoom forever.
    if (_controller._fitZoomRequested && availableCanvas.x >= 160.0f && availableCanvas.y >= 80.0f)
    {
        const float fitWidth = std::max(1.0f, availableCanvas.x - canvasPadding * 2.0f) /
                               (baseCellSize * static_cast<float>(std::max(1, map->width)));
        const float fitHeight = std::max(1.0f, availableCanvas.y - canvasPadding * 2.0f) /
                                (baseCellSize * static_cast<float>(std::max(1, map->height)));
        _controller._zoom = std::clamp(std::min(fitWidth, fitHeight), 0.02f, 3.0f);
        _controller._fitZoomRequested = false;
        ImGui::SetScrollX(0.0f);
        ImGui::SetScrollY(0.0f);
    }
    _controller._zoom = std::clamp(_controller._zoom, 0.02f, 3.0f);
    if (ImGui::GetIO().KeyCtrl)
        applyMapCanvasWheelZoom(_controller._zoom, 0.02f, 3.0f);
    const bool showTacticalOverlay = shouldShowMapTacticalOverlay();
    const float cellSize = baseCellSize * _controller._zoom;
    const ImVec2 cursorOrigin = ImGui::GetCursorScreenPos();
    const float mapPixelWidth = cellSize * static_cast<float>(map->width);
    const float mapPixelHeight = cellSize * static_cast<float>(map->height);
    const ImVec2 canvasSize(
        std::max(availableCanvas.x, mapPixelWidth + canvasPadding * 2.0f),
        std::max(availableCanvas.y, mapPixelHeight + canvasPadding * 2.0f));
    const ImVec2 origin(
        cursorOrigin.x + std::max(canvasPadding, (availableCanvas.x - mapPixelWidth) * 0.5f),
        cursorOrigin.y + std::max(canvasPadding, (availableCanvas.y - mapPixelHeight) * 0.5f));
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 visibleMinimum = drawList->GetClipRectMin();
    const ImVec2 visibleMaximum = drawList->GetClipRectMax();
    LocalPlayerDisplayCanvasCameraInput cameraInput;
    cameraInput.mapOriginX = origin.x;
    cameraInput.mapOriginY = origin.y;
    cameraInput.mapPixelWidth = mapPixelWidth;
    cameraInput.mapPixelHeight = mapPixelHeight;
    cameraInput.visibleMinimumX = visibleMinimum.x;
    cameraInput.visibleMinimumY = visibleMinimum.y;
    cameraInput.visibleMaximumX = visibleMaximum.x;
    cameraInput.visibleMaximumY = visibleMaximum.y;
    cameraInput.viewportWidth = availableCanvas.x;
    cameraInput.viewportHeight = availableCanvas.y;
    cameraInput.cellSize = cellSize;
    cameraInput.mapWidth = map->width;
    cameraInput.mapHeight = map->height;
    LocalPlayerDisplayCameraState canvasCamera;
    if (localPlayerDisplayCameraFromCanvas(cameraInput, canvasCamera))
    {
        _controller.updateViewCameraSnapshot(map->metadata.id,
                                             canvasCamera.normalizedCenterX,
                                             canvasCamera.normalizedCenterY,
                                             canvasCamera.zoom);
    }
    drawList->AddRectFilled(cursorOrigin, ImVec2(cursorOrigin.x + canvasSize.x, cursorOrigin.y + canvasSize.y), IM_COL32(32, 36, 40, 255));
    const bool continuousRendering = map->generation.continuousRendering;
    const bool forceTacticalGrid = map->atlasLevel == MapLevel::Tactical && map->gridType == GridType::Square;
    const std::string renderLocale(gm::localization::localeCode(
        gm::localization::LocalizationService::instance().locale()));
    bool importedBackgroundDrawn = false;
    MapRenderPlanRequest backgroundRequest;
    backgroundRequest.audience = MapRenderAudience::GameMaster;
    backgroundRequest.locale = renderLocale;
    backgroundRequest.layers = MapRenderPlanBackground;
    backgroundRequest.applyFog = false;
    const std::shared_ptr<const MapRenderPlan> cachedBackgroundPlan =
        _renderPlanCache.getOrCompile(*map, backgroundRequest,
                                      _controller._mapRenderContentVersion,
                                      cellSize);
    const MapRenderPlan &backgroundPlan = *cachedBackgroundPlan;
    if (backgroundPlan.backgroundVisible)
    {
        const CachedImageTexture &texture = getCachedImageTexture(
            ProjectAssets::resolveProjectPath(
                backgroundPlan.backgroundInteractiveProjectRelativePath),
            backgroundPlan.backgroundInteractiveContentHash);
        if (texture.loaded)
        {
            const MapNormalizedRect crop = map->importedReference.enabled
                                               ? map->importedReference.crop
                                               : MapNormalizedRect{0.0f, 0.0f, 1.0f, 1.0f};
            const float rotation = map->importedReference.enabled ? map->importedReference.rotationDegrees : 0.0f;
            const float radians = -rotation * 0.01745329251994329577f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            const auto uv = [&](float x, float y) {
                const float centeredX = x - 0.5f;
                const float centeredY = y - 0.5f;
                const float rotatedX = cosine * centeredX - sine * centeredY + 0.5f;
                const float rotatedY = sine * centeredX + cosine * centeredY + 0.5f;
                return ImVec2(crop.x + rotatedX * crop.width, crop.y + rotatedY * crop.height);
            };
            const float opacity = map->importedReference.enabled ? 1.0f : std::clamp(map->backgroundImageOpacity, 0.0f, 1.0f);
            drawList->AddImageQuad(texture.textureId,
                                   origin, ImVec2(origin.x + mapPixelWidth, origin.y),
                                   ImVec2(origin.x + mapPixelWidth, origin.y + mapPixelHeight),
                                   ImVec2(origin.x, origin.y + mapPixelHeight),
                                   uv(0.0f, 0.0f), uv(1.0f, 0.0f), uv(1.0f, 1.0f), uv(0.0f, 1.0f),
                                   ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, opacity)));
            importedBackgroundDrawn = true;
        }
    }
    if (continuousRendering && !importedBackgroundDrawn)
    {
        MapRenderPlanRequest terrainRequest;
        terrainRequest.audience = MapRenderAudience::GameMaster;
        terrainRequest.locale = renderLocale;
        terrainRequest.layers = MapRenderPlanTerrain | MapRenderPlanWater;
        terrainRequest.applyFog = false;
        const std::shared_ptr<const MapRenderPlan> terrainPlan =
            _renderPlanCache.getOrCompile(*map, terrainRequest,
                                          _controller._mapRenderContentVersion,
                                          cellSize);
        renderMap(drawList, origin, *map, *terrainPlan, cellSize);
    }
    const MapVisibleCellBounds visibleCells = mapVisibleCellBounds(drawList, origin, cellSize, *map, 2);

    CanvasFrame frame;
    frame.map = map;
    frame.drawList = drawList;
    frame.origin = origin;
    frame.cellSize = cellSize;
    frame.mapPixelWidth = mapPixelWidth;
    frame.mapPixelHeight = mapPixelHeight;
    frame.visibleCells = visibleCells;
    frame.showTacticalOverlay = showTacticalOverlay;
    frame.continuousRendering = continuousRendering;
    frame.forceTacticalGrid = forceTacticalGrid;
    frame.importedBackgroundDrawn = importedBackgroundDrawn;
    frame.renderLocale = renderLocale;
    drawMapCanvasCells(frame);
    drawMapCanvasSemantics(frame);
    handleMapCanvasInput(frame);
    ImGui::EndChild();

    if (detailsBesideMap)
    {
        ImGui::TableNextColumn();
        ImGui::BeginChild("MapViewerDetails", ImVec2(0.0f, viewportHeight), false);
        drawMapSidePanel(*map);
        ImGui::EndChild();
    }
    ImGui::EndTable();
    if (_showDetails && !_canvasFocus && !detailsBesideMap) drawMapSidePanel(*map);
}
