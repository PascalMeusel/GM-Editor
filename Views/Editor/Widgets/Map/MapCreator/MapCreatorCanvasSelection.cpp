#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawMapCanvasSelection(const CanvasFrame &frame)
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
    if (_controller._editing.selectedGeneratedFeatureIndex >= 0 &&
        _controller._editing.selectedGeneratedFeatureIndex < static_cast<int>(map.generation.graph.features.size()))
    {
        const MapGeneratedFeature &feature =
            map.generation.graph.features[static_cast<std::size_t>(_controller._editing.selectedGeneratedFeatureIndex)];
        if (_selectedGeneratedFeaturePoint >= static_cast<int>(feature.points.size()))
            _selectedGeneratedFeaturePoint = -1;

        std::vector<ImVec2> selectedScreenPoints;
        selectedScreenPoints.reserve(feature.points.size());
        // Move highlights follow the same curve as the artwork; raw handles
        // belong only to Reshape. A thin coloured stroke avoids black spikes.
        const auto selectedPoints = _objectEditMode == gm::mapui::MapObjectEditMode::Reshape
            ? feature.points : mapFeaturePresentationPoints(map, feature);
        for (const MapFeaturePoint &point : selectedPoints)
            selectedScreenPoints.emplace_back(origin.x + point.x * cellSize, origin.y + point.y * cellSize);

        const bool terrainReference = feature.kind == MapGeneratedFeatureKind::TerrainRegion ||
                                      feature.kind == MapGeneratedFeatureKind::Vegetation ||
                                      feature.kind == MapGeneratedFeatureKind::MountainRange;
        const ImU32 selectionColor = feature.locked     ? IM_COL32(186, 194, 199, 255)
                                     : terrainReference ? IM_COL32(185, 165, 245, 255)
                                                        : IM_COL32(250, 194, 70, 255);
        if (terrainReference && _terrainSelection.cells.empty())
        {
            const char *hint = uiTextForLocale(
                "Terrain reference outline — no terrain cells selected.\nClick terrain or draw a selection box to choose cells to move.",
                "Gelände-Referenzumriss — keine Geländezellen ausgewählt.\nKlicke auf Gelände oder ziehe einen Auswahlrahmen, um Zellen zum Verschieben zu wählen.");
            const auto clip = drawList->GetClipRectMin();
            const ImVec2 at(clip.x + 12, clip.y + 12);
            const float wrap = std::max(80.f, std::min(480.f, drawList->GetClipRectMax().x - at.x - 24.f));
            const auto size = ImGui::CalcTextSize(hint, nullptr, false, wrap);
            drawList->AddRectFilled(at, ImVec2(at.x + size.x + 16, at.y + size.y + 12), IM_COL32(26, 30, 30, 240), 4);
            drawList->AddText(nullptr, 0, ImVec2(at.x + 8, at.y + 6), selectionColor, hint, nullptr, wrap);
        }
        if (selectedScreenPoints.size() >= 2u)
        {
            const ImDrawFlags flags = feature.closed ? ImDrawFlags_Closed : ImDrawFlags_None;
            drawList->AddPolyline(selectedScreenPoints.data(), static_cast<int>(selectedScreenPoints.size()),
                                  selectionColor, flags, 2.0f);
        }
        else if (selectedScreenPoints.size() == 1u)
        {
            drawList->AddCircleFilled(selectedScreenPoints.front(), 8.0f, IM_COL32(17, 21, 22, 230), 20);
            drawList->AddCircle(selectedScreenPoints.front(), 8.0f, selectionColor, 20, 2.5f);
        }

        if (_objectEditMode == gm::mapui::MapObjectEditMode::Reshape && !feature.locked)
            for (int pointIndex : generatedMapFeatureEditHandles(feature))
            {
                const bool selectedPoint = pointIndex == _selectedGeneratedFeaturePoint;
                const bool draggedPoint = pointIndex == _draggedGeneratedFeaturePoint;
                const float radius = draggedPoint ? 7.5f : (selectedPoint ? 6.5f : 5.0f);
                drawList->AddCircleFilled(selectedScreenPoints[static_cast<std::size_t>(pointIndex)], radius,
                                          selectedPoint ? selectionColor : IM_COL32(25, 30, 30, 245), 18);
                drawList->AddCircle(selectedScreenPoints[static_cast<std::size_t>(pointIndex)], radius,
                                    selectedPoint ? IM_COL32(255, 245, 205, 255) : selectionColor, 18,
                                    selectedPoint ? 2.5f : 1.7f);
            }

        if (_generatedFeatureGeometryEditActive && !selectedScreenPoints.empty())
        {
            const char *hint = uiTextForLocale("Preview - release to apply", "Vorschau - loslassen zum Anwenden");
            const ImVec2 size = ImGui::CalcTextSize(hint);
            const ImVec2 at(selectedScreenPoints.front().x + 12.0f, selectedScreenPoints.front().y + 12.0f);
            drawList->AddRectFilled(at, ImVec2(at.x + size.x + 12.0f, at.y + size.y + 8.0f), IM_COL32(26, 30, 30, 235),
                                    3.0f);
            drawList->AddText(ImVec2(at.x + 6.0f, at.y + 4.0f), selectionColor, hint);
        }
        if (feature.locked && !selectedScreenPoints.empty())
        {
            const std::string lockedLabel = localizedUiLabel("Locked");
            const ImVec2 labelSize = ImGui::CalcTextSize(lockedLabel.c_str());
            const ImVec2 labelMin(selectedScreenPoints.front().x + 9.0f,
                                  selectedScreenPoints.front().y - labelSize.y - 9.0f);
            drawList->AddRectFilled(labelMin, ImVec2(labelMin.x + labelSize.x + 10.0f, labelMin.y + labelSize.y + 6.0f),
                                    IM_COL32(26, 30, 30, 225), 3.0f);
            drawList->AddText(ImVec2(labelMin.x + 5.0f, labelMin.y + 3.0f), selectionColor, lockedLabel.c_str());
        }
    }

    if (_regenerationUseArea)
    {
        const int minimumX = std::clamp(std::min(_regenerationAreaStartX, _regenerationAreaEndX), 0, map.width - 1);
        const int minimumY = std::clamp(std::min(_regenerationAreaStartY, _regenerationAreaEndY), 0, map.height - 1);
        const int maximumX = std::clamp(std::max(_regenerationAreaStartX, _regenerationAreaEndX), 0, map.width - 1);
        const int maximumY = std::clamp(std::max(_regenerationAreaStartY, _regenerationAreaEndY), 0, map.height - 1);
        const ImVec2 selectionMinimum(origin.x + minimumX * cellSize, origin.y + minimumY * cellSize);
        const ImVec2 selectionMaximum(origin.x + (maximumX + 1) * cellSize, origin.y + (maximumY + 1) * cellSize);
        drawList->AddRectFilled(selectionMinimum, selectionMaximum,
                                _regenerationPickingArea ? IM_COL32(40, 170, 190, 42) : IM_COL32(245, 184, 72, 28));
        drawList->AddRect(selectionMinimum, selectionMaximum,
                          _regenerationPickingArea ? IM_COL32(53, 195, 210, 255) : IM_COL32(245, 184, 72, 225), 3.0f, 0,
                          _regenerationPickingArea ? 3.0f : 2.0f);
    }

    if (_imageTraceActive)
    {
        const MapImageTraceAuthoring::Mode traceMode = imageTraceMode(_imageTraceModeIndex);
        const std::vector<MapFeaturePoint> previewPoints = MapImageTraceAuthoring::smoothedPoints(
            _imageTraceControlPoints, MapImageTraceAuthoring::isClosed(traceMode), _imageTraceSmooth);
        std::vector<ImVec2> screenPoints;
        screenPoints.reserve(previewPoints.size());
        for (const MapFeaturePoint &point : previewPoints)
            screenPoints.emplace_back(origin.x + point.x * cellSize, origin.y + point.y * cellSize);

        ImU32 traceColor = IM_COL32(245, 190, 76, 245);
        if (traceMode == MapImageTraceAuthoring::Mode::River || traceMode == MapImageTraceAuthoring::Mode::Coastline)
            traceColor = IM_COL32(60, 196, 220, 245);
        if (screenPoints.size() >= 2u)
        {
            const bool closedPreview =
                MapImageTraceAuthoring::isClosed(traceMode) && _imageTraceControlPoints.size() >= 3u;
            if (closedPreview)
                drawList->AddConvexPolyFilled(screenPoints.data(), static_cast<int>(screenPoints.size()),
                                              withAlpha(traceColor, 34));
            drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()), traceColor,
                                  closedPreview ? ImDrawFlags_Closed : ImDrawFlags_None,
                                  std::max(2.0f, cellSize * 0.09f));
        }

        for (int pointIndex = 0; pointIndex < static_cast<int>(_imageTraceControlPoints.size()); ++pointIndex)
        {
            const MapFeaturePoint &point = _imageTraceControlPoints[static_cast<std::size_t>(pointIndex)];
            const ImVec2 center(origin.x + point.x * cellSize, origin.y + point.y * cellSize);
            const bool dragged = pointIndex == _imageTraceDraggedPoint;
            const float radius = dragged ? 7.0f : 5.5f;
            drawList->AddCircleFilled(center, radius,
                                      dragged ? IM_COL32(255, 232, 130, 255) : IM_COL32(24, 29, 28, 245), 16);
            drawList->AddCircle(center, radius, traceColor, 16, 2.0f);
        }
    }

    if (_controller._editing.strokeInProgress && _controller._editing.activeTool == MapCreatorController::Tool::PaintWay &&
        map.generation.continuousRendering)
    {
        const auto way = _controller.activeWayKind();
        const float width =
            gm::map_render::manualWayStrokeDisplayWidth(map, way, _controller.activeBrushSize(), cellSize).outerPixels;
        const ImU32 color = mapWayColor(way, false);
        for (const auto &part : _controller._editing.wayStrokeParts)
        {
            std::vector<ImVec2> points;
            for (const auto &p : part)
                points.emplace_back(origin.x + p.x * cellSize, origin.y + p.y * cellSize);
            if (points.size() > 1)
                drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color, 0, width);
            if (!points.empty())
            {
                drawList->AddCircleFilled(points.front(), width * .5f, color);
                drawList->AddCircleFilled(points.back(), width * .5f, color);
            }
        }
    }

    if (_controller._editing.strokeInProgress && _controller._editing.activeTool != MapCreatorController::Tool::Fog &&
        (continuousRendering || importedBackgroundDrawn))
    {
        const char *hint = uiTextForLocale("Stroke preview - release to finish artwork",
                                           "Pinselvorschau - loslassen für fertige Darstellung");
        const ImVec2 p(canvasOrigin.x + 12, canvasOrigin.y + 12);
        const ImVec2 size = ImGui::CalcTextSize(hint);
        drawList->AddRectFilled(ImVec2(p.x - 6, p.y - 4), ImVec2(p.x + size.x + 6, p.y + size.y + 4),
                                IM_COL32(28, 32, 33, 230), 4);
        drawList->AddText(p, IM_COL32(245, 231, 192, 255), hint);
    }
}
