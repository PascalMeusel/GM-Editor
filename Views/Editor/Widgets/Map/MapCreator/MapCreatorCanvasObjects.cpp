#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorCanvasFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawMapCanvasObjects(const CanvasFrame &frame)
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
    for (int regionIndex = 0; regionIndex < static_cast<int>(map.regions.size()); ++regionIndex)
    {
        if (_regionHandle && _regionDragging && regionIndex == _regionDragIndex)
            continue; // The lightweight drag preview replaces the committed bounds.
        const MapRegionModel &region = map.regions[regionIndex];
        const bool selected = regionIndex == _controller._editing.selectedRegionIndex;
        const bool regionEditing = _controller._editing.activeTool == MapCreatorController::Tool::AddRegion || selected;
        if (map.atlasLevel != MapLevel::Tactical && !regionEditing)
            continue;
        if (region.x + region.width < visibleCells.minX || region.y + region.height < visibleCells.minY ||
            region.x > visibleCells.maxX + 1 || region.y > visibleCells.maxY + 1)
            continue;
        const ImVec2 min(origin.x + region.x * cellSize, origin.y + region.y * cellSize);
        const ImVec2 max(origin.x + (region.x + region.width) * cellSize,
                         origin.y + (region.y + region.height) * cellSize);
        const ImU32 regionColor = selected
                                      ? IM_COL32(250, 220, 110, 255)
                                      : (region.gmOnly ? IM_COL32(204, 103, 111, 220) : IM_COL32(80, 190, 160, 220));
        drawList->AddRectFilled(min, max, withAlpha(regionColor, selected ? 28 : 14), 3.0f);
        drawList->AddRect(min, max, regionColor, 3.0f, 0, selected ? 3.0f : 1.5f);
        if (!region.name.empty())
        {
            const std::string localizedName = localizedGeneratedMapName(region.name);
            const ImVec2 textSize = ImGui::CalcTextSize(localizedName.c_str());
            drawList->AddRectFilled(ImVec2(min.x + 3.0f, min.y + 3.0f),
                                    ImVec2(min.x + textSize.x + 9.0f, min.y + textSize.y + 7.0f),
                                    IM_COL32(24, 27, 26, 190), 3.0f);
            drawList->AddText(ImVec2(min.x + 6.0f, min.y + 5.0f), IM_COL32(235, 242, 245, 255), localizedName.c_str());
        }
    }

    std::shared_ptr<const MapRenderPlan> routePresentationPlan;
    for (int routeIndex = 0; routeIndex < static_cast<int>(map.routes.size()); ++routeIndex)
    {
        const MapRouteModel &route = map.routes[routeIndex];
        const bool selected = routeIndex == _controller._editing.selectedRouteIndex;
        if (!selected)
            continue;

        if (!routePresentationPlan)
        {
            MapRenderPlanRequest routeRequest;
            routeRequest.audience = MapRenderAudience::GameMaster;
            routeRequest.locale = renderLocale;
            routeRequest.layers = MapRenderPlanRoutes | MapRenderPlanSymbols;
            routeRequest.applyFog = false;
            routePresentationPlan = _renderPlanCache.getOrCompile(map, routeRequest, canvasContentVersion, cellSize);
        }

        const gm::map_route_presentation::Path presentationPath =
            mapRoutePresentationForPlan(map, route, routePresentationPlan.get());
        drawMapRoutePresentationPath(drawList, origin, cellSize, presentationPath, IM_COL32(250, 220, 110, 255), 5.0f);
    }

    for (int featureIndex = 0; featureIndex < static_cast<int>(map.features.size()); ++featureIndex)
    {
        const MapFeatureModel &feature = map.features[featureIndex];
        const bool selected = featureIndex == _controller._editing.selectedFeatureIndex;
        const bool semanticLocation =
            feature.kind == MapFeatureKind::NamedLocation || feature.kind == MapFeatureKind::Pin;
        // Doors, traps and hazards are tactical authoring data. On overview maps
        // they only appear while selected; otherwise the canvas stays a readable
        // map instead of a layer of lettered debug markers.
        if (map.atlasLevel != MapLevel::Tactical && !semanticLocation && !selected)
            continue;
        if (!pointIsVisible(static_cast<float>(feature.x) + 0.5f, static_cast<float>(feature.y) + 0.5f, 0.5f))
            continue;
        const float centerX = origin.x + (feature.x + 0.5f) * cellSize;
        const float centerY = origin.y + (feature.y + 0.5f) * cellSize;
        const char *marker = "P";
        ImU32 color = IM_COL32(92, 172, 214, 255);
        switch (feature.kind)
        {
        case MapFeatureKind::NamedLocation:
            marker = "N";
            color = IM_COL32(222, 174, 76, 255);
            break;
        case MapFeatureKind::Door:
            marker = "D";
            color = IM_COL32(88, 192, 166, 255);
            break;
        case MapFeatureKind::SecretDoor:
            marker = "S";
            color = IM_COL32(169, 126, 211, 255);
            break;
        case MapFeatureKind::Trap:
            marker = "T";
            color = IM_COL32(219, 142, 70, 255);
            break;
        case MapFeatureKind::Hazard:
            marker = "!";
            color = IM_COL32(214, 92, 82, 255);
            break;
        case MapFeatureKind::Pin:
            break;
        }
        const ImVec2 center(centerX, centerY);
        const float markerRadius = std::clamp(cellSize * 0.20f, 5.0f, 10.0f);
        const bool worldNamedLocation =
            map.atlasLevel == MapLevel::World && feature.kind == MapFeatureKind::NamedLocation;
        if (feature.kind == MapFeatureKind::NamedLocation)
            drawList->AddQuadFilled(ImVec2(centerX, centerY - markerRadius), ImVec2(centerX + markerRadius, centerY),
                                    ImVec2(centerX, centerY + markerRadius), ImVec2(centerX - markerRadius, centerY),
                                    color);
        else if (feature.kind == MapFeatureKind::Door || feature.kind == MapFeatureKind::SecretDoor)
            drawList->AddRectFilled(ImVec2(centerX - markerRadius, centerY - markerRadius),
                                    ImVec2(centerX + markerRadius, centerY + markerRadius), color, 2.0f);
        else
            drawList->AddCircleFilled(center, markerRadius, color, 16);
        if (feature.gmOnly)
            drawList->AddCircle(center, markerRadius + 2.0f, IM_COL32(211, 154, 241, 235), 18, 1.5f);
        if (selected)
            drawList->AddCircle(center, markerRadius + 4.0f, IM_COL32(250, 220, 110, 255), 18, 2.5f);
        if (!worldNamedLocation)
        {
            const ImVec2 markerSize = ImGui::CalcTextSize(marker);
            drawList->AddText(ImVec2(centerX - markerSize.x * 0.5f, centerY - markerSize.y * 0.5f),
                              IM_COL32(22, 24, 22, 255), marker);
        }
        const bool showPersistentWorldLabel = map.atlasLevel == MapLevel::World && semanticLocation;
        if ((selected || showPersistentWorldLabel) && !feature.name.empty())
        {
            const std::string name = localizedGeneratedMapName(feature.name);
            const ImVec2 nameSize = ImGui::CalcTextSize(name.c_str());
            const float mapRight = origin.x + map.width * cellSize;
            const float mapBottom = origin.y + map.height * cellSize;
            const float requestedX = centerX + markerRadius + 5.0f;
            const ImVec2 labelMin(
                std::clamp(requestedX, origin.x + 3.0f, std::max(origin.x + 3.0f, mapRight - nameSize.x - 11.0f)),
                std::clamp(centerY - nameSize.y * 0.5f - 3.0f, origin.y + 3.0f,
                           std::max(origin.y + 3.0f, mapBottom - nameSize.y - 9.0f)));
            drawList->AddRectFilled(labelMin, ImVec2(labelMin.x + nameSize.x + 8.0f, labelMin.y + nameSize.y + 6.0f),
                                    IM_COL32(25, 28, 27, selected ? 225 : 188), 3.0f);
            drawList->AddText(ImVec2(labelMin.x + 4.0f, labelMin.y + 3.0f), IM_COL32(240, 236, 219, 255), name.c_str());
        }
    }

    for (int tokenIndex = 0; tokenIndex < static_cast<int>(map.tokens.size()); ++tokenIndex)
    {
        const MapTokenModel &token = map.tokens[tokenIndex];
        const float tokenRadiusInCells = 0.5f + 0.30f * std::max(0.5f, token.size);
        if (!pointIsVisible(token.x + 0.5f, token.y + 0.5f, tokenRadiusInCells))
            continue;
        const float centerX = origin.x + (token.x + 0.5f) * cellSize;
        const float centerY = origin.y + (token.y + 0.5f) * cellSize;
        drawList->AddCircleFilled(ImVec2(centerX, centerY), cellSize * 0.30f * std::max(0.5f, token.size),
                                  token.hiddenFromPlayers ? IM_COL32(120, 126, 132, 255) : IM_COL32(92, 192, 235, 255),
                                  24);
        drawList->AddCircle(ImVec2(centerX, centerY), cellSize * 0.33f * std::max(0.5f, token.size),
                            IM_COL32(245, 250, 255, 255), 24, 2.0f);
        if (tokenIndex == _controller._editing.selectedTokenIndex)
            drawList->AddCircle(ImVec2(centerX, centerY), cellSize * 0.43f * std::max(0.5f, token.size),
                                IM_COL32(250, 220, 110, 255), 28, 2.5f);
        const std::string label = token.name.empty() ? "T" : token.name.substr(0, 1);
        drawList->AddText(ImVec2(centerX - 4.0f, centerY - 9.0f), IM_COL32(18, 22, 25, 255), label.c_str());
    }

    if (!_terrainSelection.cells.empty())
    {
        if (_controller._editing.activeTool != MapCreatorController::Tool::InspectSelect ||
            (!_terrainDragging && _terrainSelectionSerial != _controller._editing.authoringSerial))
        {
            _terrainSelection = {};
            _terrainDragArmed = _terrainDragging = false;
        }
        else
        {
            const bool valid = !_terrainDragging ||
                               gm::mapui::terrainGroupCanMove(map, _terrainSelection, _terrainDeltaX, _terrainDeltaY);
            const ImU32 edge = valid ? IM_COL32(250, 194, 70, 255) : IM_COL32(245, 90, 90, 255);
            drawTerrainSelectionCells(drawList, origin, cellSize, _terrainSelection, _terrainDeltaX, _terrainDeltaY,
                                      edge, valid ? IM_COL32(250, 194, 70, 60) : IM_COL32(245, 90, 90, 65));
            const std::string title = terrainSelectionSummary(_terrainSelection);
            const std::string detail =
                std::string(valid ? uiTextForLocale("Drag the marked cells to move this terrain.",
                                                    "Ziehe die markierten Zellen, um dieses Gelände zu verschieben.")
                                  : uiTextForLocale("Destination blocked. Release to cancel this move.",
                                                    "Ziel blockiert. Loslassen bricht diese Verschiebung ab.")) +
                "\n" +
                uiTextForLocale("Roads and places stay in position; select them separately.",
                                "Wege und Orte bleiben an ihrer Position; wähle sie separat aus.");
            const auto clip = drawList->GetClipRectMin();
            const ImVec2 at(clip.x + 12, clip.y + 12);
            const float wrap = std::max(80.f, std::min(480.f, drawList->GetClipRectMax().x - at.x - 24.f));
            const auto titleSize = ImGui::CalcTextSize(title.c_str(), nullptr, false, wrap);
            const auto detailSize = ImGui::CalcTextSize(detail.c_str(), nullptr, false, wrap);
            drawList->AddRectFilled(
                at, ImVec2(at.x + std::max(titleSize.x, detailSize.x) + 16, at.y + titleSize.y + detailSize.y + 18),
                IM_COL32(26, 30, 30, 240), 4);
            drawList->AddText(nullptr, 0, ImVec2(at.x + 8, at.y + 6), edge, title.c_str(), nullptr, wrap);
            drawList->AddText(nullptr, 0, ImVec2(at.x + 8, at.y + titleSize.y + 12), IM_COL32(225, 230, 230, 255),
                              detail.c_str(), nullptr, wrap);
        }
    }
}
