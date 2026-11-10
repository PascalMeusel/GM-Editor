#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

#include "Views/Editor/Widgets/Map/MapViewer/MapViewerCanvasFrame.hpp"

void MapViewerView::drawMapCanvasSemantics(const CanvasFrame &frame)
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
    // Rectangular regions are tactical/editing metadata. Drawing every region
    // over an overview map made generated worlds look like unfinished debug
    // layouts. Keep them visible where their cell boundaries are meaningful;
    // overview regions remain available through Inspect in the side panel.
    if (map->atlasLevel == MapLevel::Tactical)
    for (const MapRegionModel &region : map->regions)
    {
        const ImVec2 min(origin.x + region.x * cellSize, origin.y + region.y * cellSize);
        const ImVec2 max(origin.x + (region.x + region.width) * cellSize, origin.y + (region.y + region.height) * cellSize);
        drawList->AddRect(min, max, region.gmOnly ? IM_COL32(171, 118, 255, 230) : IM_COL32(80, 190, 160, 230), 2.0f, 0, 2.0f);
        if (!region.name.empty())
        {
            const std::string localizedName = localizedGeneratedMapName(region.name);
            drawList->AddText(ImVec2(min.x + 4.0f, min.y + 4.0f), IM_COL32(235, 242, 245, 255), localizedName.c_str());
        }
    }

    for (const WorldPlaceModel &place : _controller._currentCampaign.worldPlaces)
    {
        if (!place.hasMapPosition || place.linkedMapId != map->metadata.id)
            continue;
        // Note-driven generation synchronizes a WorldPlace with its stable
        // generated settlement feature. The shared renderer already draws that
        // feature with the correct capital/city symbol, so a second legacy dot
        // and label would create the doubled markers seen in Preview.
        const bool renderedByGeneratedGraph =
            (!place.linkedFeatureId.empty() &&
             std::any_of(map->generation.graph.features.begin(), map->generation.graph.features.end(),
                         [&](const MapGeneratedFeature &feature) { return feature.id == place.linkedFeatureId; })) ||
            (place.linkedFeatureId.empty() &&
             std::any_of(map->generation.graph.features.begin(), map->generation.graph.features.end(),
                         [&](const MapGeneratedFeature &feature) {
                             return feature.kind == MapGeneratedFeatureKind::Settlement && feature.name == place.name;
                         }));
        if (renderedByGeneratedGraph)
            continue;
        const ImVec2 marker(origin.x + static_cast<float>(normalizedWorldCoordinate(place.mapPosition.x)) * map->width * cellSize,
                            origin.y + static_cast<float>(normalizedWorldCoordinate(place.mapPosition.y)) * map->height * cellSize);
        const bool capital = place.type == WorldPlaceType::Capital;
        const float radius = capital ? std::max(7.0f, cellSize * 0.24f) : std::max(5.0f, cellSize * 0.18f);
        drawList->AddCircleFilled(marker, radius, capital ? IM_COL32(235, 176, 68, 255) : IM_COL32(236, 224, 199, 245), 20);
        drawList->AddCircle(marker, radius + 2.0f, IM_COL32(31, 35, 36, 245), 20, 2.0f);
        if (!place.name.empty())
        {
            const ImVec2 textPosition(marker.x + radius + 5.0f, marker.y - ImGui::GetTextLineHeight() * 0.5f);
            drawList->AddText(ImVec2(textPosition.x + 1.0f, textPosition.y + 1.0f), IM_COL32(20, 22, 23, 230), place.name.c_str());
            drawList->AddText(textPosition, IM_COL32(246, 236, 213, 255), place.name.c_str());
        }
    }

    for (const MapModel &child : _controller._currentCampaign.maps)
    {
        if (MapAtlasService::effectiveParentId(child) != map->metadata.id || child.atlasPlacement.parentId.empty() ||
            !MapAtlasService::isNormalizedRectValid(child.atlasPlacement.parentRect))
            continue;
        const MapNormalizedRect &rect = child.atlasPlacement.parentRect;
        const ImVec2 minimum(origin.x + rect.x * map->width * cellSize, origin.y + rect.y * map->height * cellSize);
        const ImVec2 maximum(origin.x + (rect.x + rect.width) * map->width * cellSize,
                             origin.y + (rect.y + rect.height) * map->height * cellSize);
        const MapAtlasStaleReport childStatus = MapAtlasService::detectStale(child, _controller._currentCampaign.maps);
        const ImU32 color = childStatus.stale ? IM_COL32(238, 166, 55, 245) : IM_COL32(73, 190, 177, 245);
        drawList->AddRectFilled(minimum, maximum, IM_COL32(34, 48, 50, 34));
        drawList->AddRect(minimum, maximum, color, 4.0f, 0, 3.0f);
        const std::string label = (child.name.empty() ? localizedUiLabel("Untitled map") : child.name) + "  >";
        const float labelHeight = ImGui::GetFontSize() + 8.0f;
        drawList->AddRectFilled(minimum, ImVec2(std::min(maximum.x, minimum.x + ImGui::CalcTextSize(label.c_str()).x + 12.0f),
            std::min(maximum.y, minimum.y + labelHeight)), IM_COL32(25, 31, 33, 220), 4.0f);
        if (maximum.x - minimum.x > 16.0f && maximum.y - minimum.y > labelHeight)
        {
            const ImVec4 clip(minimum.x + 4.0f, minimum.y, maximum.x - 4.0f, maximum.y);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(minimum.x + 6.0f, minimum.y + 4.0f),
                color, label.c_str(), nullptr, 0.0f, &clip);
        }
        if (ImGui::IsMouseHoveringRect(minimum, maximum))
            ImGui::SetTooltip("%s", label.c_str());
    }

    const PartyTravelPlanModel &travel = _controller._currentCampaign.partyTravel;
    std::shared_ptr<const MapRenderPlan> routePresentationPlan;
    for (const MapRouteModel &route : map->routes)
    {
        const bool activeRoute = travel.currentMapId == map->metadata.id && travel.routeId == route.metadata.id && hasActiveTravelRoute(travel);
        if (!activeRoute)
            continue;

        if (!routePresentationPlan)
        {
            MapRenderPlanRequest routeRequest;
            routeRequest.audience = MapRenderAudience::GameMaster;
            routeRequest.locale = renderLocale;
            routeRequest.layers = MapRenderPlanRoutes | MapRenderPlanSymbols;
            routeRequest.applyFog = false;
            routePresentationPlan = _renderPlanCache.getOrCompile(
                *map, routeRequest,
                gm::map_route_presentation::fingerprint(*map),
                cellSize);
        }

        const gm::map_route_presentation::Path presentationPath =
            mapRoutePresentationForPlan(*map, route,
                                        routePresentationPlan.get());
        drawMapRoutePresentationPath(drawList, origin, cellSize,
                                     presentationPath,
                                     IM_COL32(235, 210, 83, 220), 5.0f);
        const float progress = travelProgressFraction(travel);
        gm::map_route_presentation::Path travelledPath = presentationPath;
        travelledPath.points = gm::map_route_presentation::prefixAtFraction(
            presentationPath.points, progress);
        drawMapRoutePresentationPath(drawList, origin, cellSize,
                                     travelledPath,
                                     IM_COL32(94, 211, 156, 255), 5.0f);
        const MapFeaturePoint markerPoint =
            gm::map_route_presentation::pointAtFraction(
                presentationPath.points, progress);
        const ImVec2 marker = mapRoutePresentationScreenPoint(
            origin, cellSize, markerPoint);
        drawList->AddCircleFilled(marker, std::max(5.0f, cellSize * 0.16f), IM_COL32(94, 211, 156, 255), 24);
        drawList->AddCircle(marker, std::max(7.0f, cellSize * 0.20f), IM_COL32(255, 255, 255, 255), 24, 2.0f);
    }

    for (const MapFeatureModel &feature : map->features)
    {
        const bool semanticLocation = feature.kind == MapFeatureKind::NamedLocation || feature.kind == MapFeatureKind::Pin;
        if (map->atlasLevel != MapLevel::Tactical && !semanticLocation)
            continue;
        // Empty technical pins are useful while authoring, but a generic cyan
        // exclamation mark has no readable meaning in an atlas view.
        if (map->atlasLevel != MapLevel::Tactical && feature.name.empty())
            continue;
        const float centerX = origin.x + (feature.x + 0.5f) * cellSize;
        const float centerY = origin.y + (feature.y + 0.5f) * cellSize;
        const float markerRadius = std::clamp(cellSize * 0.20f, 4.0f, 10.0f);
        const ImVec2 center(centerX, centerY);
        const ImU32 markerColor = feature.gmOnly ? IM_COL32(183, 129, 213, 245) : IM_COL32(222, 174, 76, 245);
        const char *marker = "P";
        switch (feature.kind)
        {
        case MapFeatureKind::NamedLocation: marker = "N"; break;
        case MapFeatureKind::Door: marker = "D"; break;
        case MapFeatureKind::SecretDoor: marker = "S"; break;
        case MapFeatureKind::Trap: marker = "T"; break;
        case MapFeatureKind::Hazard: marker = "!"; break;
        case MapFeatureKind::Pin: break;
        }
        if (feature.kind == MapFeatureKind::NamedLocation)
            drawList->AddQuadFilled(ImVec2(centerX, centerY - markerRadius), ImVec2(centerX + markerRadius, centerY),
                                    ImVec2(centerX, centerY + markerRadius), ImVec2(centerX - markerRadius, centerY), markerColor);
        else
            drawList->AddCircleFilled(center, markerRadius, markerColor, 16);
        const ImVec2 markerSize = ImGui::CalcTextSize(marker);
        drawList->AddText(ImVec2(centerX - markerSize.x * 0.5f, centerY - markerSize.y * 0.5f),
                          IM_COL32(18, 22, 25, 255), marker);
        if (!feature.name.empty() && (map->atlasLevel != MapLevel::Tactical || cellSize >= 12.0f))
        {
            const std::string name = localizedGeneratedMapName(feature.name);
            const ImVec2 labelPosition(centerX + markerRadius + 4.0f, centerY - ImGui::GetTextLineHeight() * 0.5f);
            drawList->AddText(ImVec2(labelPosition.x + 1.0f, labelPosition.y + 1.0f), IM_COL32(20, 22, 23, 220), name.c_str());
            drawList->AddText(labelPosition, IM_COL32(240, 236, 219, 245), name.c_str());
        }
    }

    for (const MapTokenModel &token : map->tokens)
    {
        const float centerX = origin.x + (token.x + 0.5f) * cellSize;
        const float centerY = origin.y + (token.y + 0.5f) * cellSize;
        drawList->AddCircleFilled(ImVec2(centerX, centerY), cellSize * 0.30f * std::max(0.5f, token.size), token.hiddenFromPlayers ? IM_COL32(120, 126, 132, 255) : IM_COL32(92, 192, 235, 255), 24);
        drawList->AddCircle(ImVec2(centerX, centerY), cellSize * 0.33f * std::max(0.5f, token.size), IM_COL32(245, 250, 255, 255), 24, 2.0f);
        const std::string label = token.name.empty() ? "T" : token.name.substr(0, 1);
        drawList->AddText(ImVec2(centerX - 4.0f, centerY - 9.0f), IM_COL32(18, 22, 25, 255), label.c_str());
    }

    if (_controller._currentCampaign.partyLocation.mapId == map->metadata.id)
    {
        const float centerX = origin.x + (_controller._currentCampaign.partyLocation.x + 0.5f) * cellSize;
        const float centerY = origin.y + (_controller._currentCampaign.partyLocation.y + 0.5f) * cellSize;
        drawList->AddCircleFilled(ImVec2(centerX, centerY), cellSize * 0.34f, IM_COL32(235, 210, 83, 255), 24);
        drawList->AddCircle(ImVec2(centerX, centerY), cellSize * 0.37f, IM_COL32(255, 255, 255, 255), 24, 2.0f);
        drawList->AddText(ImVec2(centerX - 4.0f, centerY - 9.0f), IM_COL32(20, 24, 28, 255), "P");
    }

    if (isMapCellInside(*map, _controller._selectedCellX, _controller._selectedCellY) &&
        _controller._interactionMode == MapViewerInteractionMode::InspectSelect)
    {
        const ImVec2 min(origin.x + _controller._selectedCellX * cellSize, origin.y + _controller._selectedCellY * cellSize);
        const ImVec2 max(min.x + cellSize, min.y + cellSize);
        drawList->AddRect(min, max, IM_COL32(255, 255, 255, 245), 0.0f, 0, 3.0f);
        drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, 24));
    }

    if (_controller._hasPendingPartyMove && isMapCellInside(*map, _controller._pendingPartyMoveX, _controller._pendingPartyMoveY))
    {
        const ImVec2 min(origin.x + _controller._pendingPartyMoveX * cellSize, origin.y + _controller._pendingPartyMoveY * cellSize);
        const ImVec2 max(min.x + cellSize, min.y + cellSize);
        const ImVec2 destination((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
        drawList->AddRectFilled(min, max, IM_COL32(235, 210, 83, 50));
        drawList->AddRect(min, max, IM_COL32(235, 210, 83, 255), 0.0f, 0, 3.0f);
        drawList->AddCircleFilled(destination, cellSize * 0.22f, IM_COL32(235, 210, 83, 190), 24);
        const std::string moveLabel = localizedUiLabel("Move");
        drawList->AddText(ImVec2(destination.x - 18.0f, destination.y - 8.0f), IM_COL32(20, 24, 28, 255), moveLabel.c_str());
        if (_controller._currentCampaign.partyLocation.mapId == map->metadata.id)
        {
            const ImVec2 start(origin.x + (_controller._currentCampaign.partyLocation.x + 0.5f) * cellSize,
                               origin.y + (_controller._currentCampaign.partyLocation.y + 0.5f) * cellSize);
            drawList->AddLine(start, destination, IM_COL32(235, 210, 83, 220), 2.0f);
        }
    }


    if (_controller.hasDetailSelection())
    {
        const int minimumX = std::min(_controller._detailStartX, _controller._detailEndX);
        const int minimumY = std::min(_controller._detailStartY, _controller._detailEndY);
        const int maximumX = std::max(_controller._detailStartX, _controller._detailEndX) + 1;
        const int maximumY = std::max(_controller._detailStartY, _controller._detailEndY) + 1;
        const ImVec2 minimum(origin.x + minimumX * cellSize, origin.y + minimumY * cellSize);
        const ImVec2 maximum(origin.x + maximumX * cellSize, origin.y + maximumY * cellSize);
        drawList->AddRectFilled(minimum, maximum, IM_COL32(235, 171, 57, 40));
        drawList->AddRect(minimum, maximum, IM_COL32(244, 184, 72, 255), 3.0f, 0, 3.0f);
    }
}
