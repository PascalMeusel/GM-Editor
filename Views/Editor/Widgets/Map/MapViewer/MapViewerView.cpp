#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

using namespace gm::map_viewer_detail;

MapViewerView::MapViewerView(MapViewerController &controller)
    : DockableWidget("Atlas", 0, ImVec2(1180.0f, 760.0f)), _controller(controller)
{
}

void MapViewerView::draw()
{
    if (_controller._currentCampaign.maps.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No maps exist yet. In World, choose New world entry, then Map, to create a world, region, site, or tactical map.").c_str());
        return;
    }

    const MapModel *map = _controller.getSelectedMap();
    if (!map)
        return;

    auto &campaign = _controller._currentCampaign;
    if (_canvasPreferenceCampaign != campaign.metadata.id)
    {
        _canvasPreferenceCampaign = campaign.metadata.id;
        const auto preference = campaign.metadata.customProperties.find("ui.atlas.canvasFocus");
        _canvasFocus = preference != campaign.metadata.customProperties.end() && preference->second == "1";
    }
    if (_openTravelWorkflowRequested) { _canvasFocus = false; _showDetails = true; }
    if (FormUi::Checkbox(uiTextForLocale("Map focus", "Kartenfokus"), &_canvasFocus))
    {
        campaign.metadata.customProperties["ui.atlas.canvasFocus"] = _canvasFocus ? "1" : "0";
        campaign.metadata.touch();
        ProjectSaveService::getInstance().markDirty("atlas layout preference");
        _controller._fitZoomRequested = true;
    }
    if (_canvasFocus)
    {
        ImGui::SameLine();
        if (drawSmallUiButton(uiTextForLocale("Maps, details and travel", "Karten, Details und Reise"), UiButtonRole::Secondary))
            ImGui::OpenPopup("MapFocusDetails");
    }
    else drawAtlasHeader(*map);
    map = _controller.getSelectedMap();
    if (!map)
        return;

    const float rowEnd = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (rowEnd - ImGui::GetItemRectMax().x > 300.0f) ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(180.0f, std::max(100.0f, ImGui::GetContentRegionAvail().x - 130.0f)));
    if (FormUi::SliderFloat(localizedUiLabel("Zoom").c_str(), &_controller._zoom, 0.02f, 3.0f, "%.2fx", ImGuiSliderFlags_Logarithmic))
        _controller._fitZoomRequested = false;
    ImGui::SameLine();
    if (drawSmallUiButton(localizedUiLabel("Fit map").c_str(), UiButtonRole::Secondary))
        _controller._fitZoomRequested = true;
    if (!_canvasFocus) FormUi::Checkbox(uiTextForLocale("Details and travel", "Details und Reise"), &_showDetails);
    if (map->atlasLevel == MapLevel::World)
    {
        TaskUi::sameLineIfFits(ImGui::CalcTextSize(uiTextForLocale("Political borders", "Politische Grenzen")).x + ImGui::GetFrameHeight() + 16.0f);
        FormUi::Checkbox(uiTextForLocale("Political borders", "Politische Grenzen"), &_controller._showPoliticalOverlay);
    }
    drawMapModeControls(*map);
    if (hasActiveTravelRoute(_controller._currentCampaign.partyTravel)) drawTravelStatus(*map);
    drawMapCanvas();
    ImGui::SetNextWindowSize(ImVec2(std::min(540.0f, ImGui::GetIO().DisplaySize.x - 48.0f),
                                  std::max(240.0f, ImGui::GetIO().DisplaySize.y - 120.0f)), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("MapFocusDetails"))
    {
        if (drawSmallUiButton(uiTextForLocale("Return to map", "Zurück zur Karte"), UiButtonRole::Primary))
            ImGui::CloseCurrentPopup();
        const auto *selected = _controller.getSelectedMap();
        if (selected) drawAtlasHeader(*selected);
        auto *current = _controller.getSelectedMap();
        if (current) drawMapSidePanel(*current);
        ImGui::EndPopup();
    }
    // Modal scope must be outside the scrolling atlas sidebar.
    drawDetailDesignReview();
}

void MapViewerView::drawAtlasHeader(const MapModel &map)
{
    const std::vector<MapAtlasBreadcrumb> breadcrumbs = MapAtlasService::breadcrumbs(_controller._currentCampaign.maps, map.metadata.id);
    for (std::size_t index = 0; index < breadcrumbs.size(); ++index)
    {
        if (index > 0)
        {
            ImGui::SameLine(0.0f, 7.0f);
            ImGui::TextDisabled(">");
            ImGui::SameLine(0.0f, 7.0f);
        }
        const MapAtlasBreadcrumb &crumb = breadcrumbs[index];
        ImGui::PushID(crumb.mapId.c_str());
        if (index + 1 == breadcrumbs.size())
            ImGui::TextColored(ImVec4(0.91f, 0.66f, 0.22f, 1.0f), localizedUiLabel("%s").c_str(), crumb.name.empty() ? localizedUiLabel("Untitled map").c_str() : crumb.name.c_str());
        else if (ImGui::SmallButton(crumb.name.empty() ? localizedUiLabel("Untitled map").c_str() : crumb.name.c_str()))
            _controller.selectMapById(crumb.mapId);
        ImGui::PopID();
    }

    if (ImGui::GetContentRegionAvail().x > 480.0f) ImGui::SameLine(0.0f, 18.0f);
    ImGui::TextColored(ImVec4(0.35f, 0.79f, 0.72f, 1.0f), localizedUiLabel(atlasLevelLabel(map.atlasLevel)).c_str());
    ImGui::SameLine();
    const MapAtlasStaleReport stale = MapAtlasService::detectStale(map, _controller._currentCampaign.maps);
    ImGui::TextDisabled("%s", localizedUiLabel(atlasStatusLabel(stale.suggestedStatus)).c_str());
    if (!MapAtlasService::effectiveParentId(map).empty())
    {
        ImGui::SameLine();
        if (drawSmallUiButton(localizedUiLabel("One level up").c_str(), UiButtonRole::Secondary))
            _controller.selectParentMap();
    }

    std::vector<std::string> labelStorage;
    std::vector<const char *> labels;
    const MapModel *worldRoot = MapAtlasService::primaryWorldMap(_controller._currentCampaign.maps);
    labelStorage.reserve(_controller._currentCampaign.maps.size());
    labels.reserve(_controller._currentCampaign.maps.size());
    for (const MapModel &candidate : _controller._currentCampaign.maps)
    {
        const auto path = MapAtlasService::breadcrumbs(_controller._currentCampaign.maps, candidate.metadata.id);
        std::string label;
        if (path.size() == 1 && (!worldRoot || candidate.metadata.id != worldRoot->metadata.id))
            label = uiTextForLocale("Standalone: ", "Eigenständig: ");
        for (const MapAtlasBreadcrumb &part : path)
        {
            if (!label.empty() && label.back() != ' ')
                label += " / ";
            label += part.name.empty() ? localizedUiLabel("Untitled map") : part.name;
        }
        if (!MapAtlasService::effectiveParentId(candidate).empty() && candidate.atlasPlacement.parentId.empty())
            label += uiTextForLocale(" (location not set)", " (Position fehlt)");
        labelStorage.push_back(std::move(label));
    }
    for (const std::string &label : labelStorage)
        labels.push_back(label.c_str());
    ImGui::Spacing();
    ImGui::SetNextItemWidth(std::min(360.0f, std::max(80.0f, ImGui::GetContentRegionAvail().x - 100.0f)));
    int selectedMapIndex = _controller._selectedMapIndex;
    if (FormUi::Combo(localizedUiLabel("Atlas map").c_str(), &selectedMapIndex, labels.data(), static_cast<int>(labels.size())) &&
        selectedMapIndex >= 0 && selectedMapIndex < static_cast<int>(_controller._currentCampaign.maps.size()) &&
        selectedMapIndex != _controller._selectedMapIndex)
    {
        // Route every map switch through the controller so an unfinished fog
        // stroke is rolled back before its source map stops being selected.
        _controller.selectMapById(
            _controller._currentCampaign.maps[static_cast<std::size_t>(selectedMapIndex)].metadata.id);
    }
    if (stale.stale || stale.suggestedStatus == MapAtlasStatus::Unpositioned)
    {
        const std::string staleReason = stale.reasons.empty()
                                            ? localizedUiLabel("Parent update available")
                                            : gm::localization::trSystemMessage(stale.reasons.front());
        ImGui::TextColored(ImVec4(0.94f, 0.65f, 0.22f, 1.0f), localizedUiLabel("%s").c_str(),
                           staleReason.c_str());
    }
}

void MapViewerView::drawMapModeControls(const MapModel &map)
{
    const bool partyOnMap = _controller._currentCampaign.partyLocation.mapId == map.metadata.id;
    const bool hasPendingMove =
        _controller._hasPendingPartyMove &&
        isMapCellInside(map, _controller._pendingPartyMoveX, _controller._pendingPartyMoveY);
    const bool pendingIsCurrentPartyCell =
        hasPendingMove &&
        partyOnMap &&
        _controller._currentCampaign.partyLocation.x == _controller._pendingPartyMoveX &&
        _controller._currentCampaign.partyLocation.y == _controller._pendingPartyMoveY;

    std::string selection;
    if (_controller._interactionMode == MapViewerInteractionMode::MoveParty)
    {
        selection = partyOnMap
                        ? localizedUiLabel("Moving: Party at ") + coordinateLabel(_controller._currentCampaign.partyLocation.x, _controller._currentCampaign.partyLocation.y)
                        : localizedUiLabel("Moving: Party onto ") + mapDisplayName(map);
        if (hasPendingMove)
            selection += " -> " + coordinateLabel(_controller._pendingPartyMoveX, _controller._pendingPartyMoveY);
    }
    else if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea)
    {
        if (_controller.hasDetailSelection())
        {
            selection = localizedUiLabel("Detail area selected");
        }
        else
        {
            selection = localizedUiLabel("No detail area selected");
        }
    }
    else if (_controller._interactionMode == MapViewerInteractionMode::RevealFog ||
             _controller._interactionMode == MapViewerInteractionMode::HideFog)
    {
        selection = _controller._fogStroke.active()
                        ? localizedUiLabel("Fog stroke in progress")
                        : localizedUiLabel("Fog brush ready");
    }
    else if (_controller._interactionMode == MapViewerInteractionMode::PingPlayers)
    {
        selection = _controller.playerPingModeAvailable()
                        ? localizedUiLabel("This map is live for players")
                        : localizedUiLabel("Player display is not ready for this map");
    }
    else if (isMapCellInside(map, _controller._selectedCellX, _controller._selectedCellY))
    {
        selection = localizedUiLabel("Selected cell ") + coordinateLabel(_controller._selectedCellX, _controller._selectedCellY);
    }
    else
    {
        selection = partyOnMap
                        ? localizedUiLabel("Party at ") + coordinateLabel(_controller._currentCampaign.partyLocation.x, _controller._currentCampaign.partyLocation.y)
                        : localizedUiLabel("No cell selected");
    }

    std::string nextAction;
    if (_controller._interactionMode == MapViewerInteractionMode::MoveParty)
        nextAction = UiGuidance::text(UiGuidance::Topic::PartyMarker).purpose;
    else if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea)
        nextAction = localizedUiLabel(_controller._detailGenerationRunning
                                          ? "Detail designs are being checked in the background."
                                          : (_controller.hasDetailSelection()
                                                 ? "Review the inherited anchors, then generate three detail designs."
                                                 : "Drag across the map to select a detail area."));
    else if (_controller._interactionMode == MapViewerInteractionMode::RevealFog)
        nextAction = localizedUiLabel("Click or drag to reveal cells. Release to save one fog change.");
    else if (_controller._interactionMode == MapViewerInteractionMode::HideFog)
        nextAction = localizedUiLabel("Click or drag to hide cells. Release to save one fog change.");
    else if (_controller._interactionMode == MapViewerInteractionMode::PingPlayers)
        nextAction = localizedUiLabel(_controller.playerPingModeAvailable()
                                          ? "Click a revealed cell to show a three-second player ping."
                                          : "Publish this map in Player Display before sending a ping.");
    else
        nextAction = uiTextForLocale("Select a cell to inspect it.", "Wähle eine Zelle für Details.");

    ImGui::TextWrapped("%s", nextAction.c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", selection.c_str());
    if (_controller._interactionMode == MapViewerInteractionMode::MoveParty)
        UiGuidance::drawDetails(UiGuidance::Topic::PartyMarker);

    const bool inspectActive = _controller._interactionMode == MapViewerInteractionMode::InspectSelect;
    const bool moveActive = _controller._interactionMode == MapViewerInteractionMode::MoveParty;
    const bool detailActive = _controller._interactionMode == MapViewerInteractionMode::SelectDetailArea;
    const bool revealFogActive = _controller._interactionMode == MapViewerInteractionMode::RevealFog;
    const bool hideFogActive = _controller._interactionMode == MapViewerInteractionMode::HideFog;
    const bool pingActive = _controller._interactionMode == MapViewerInteractionMode::PingPlayers;
    const bool pingAvailable = _controller.playerPingModeAvailable();

    const float toolbarWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    float usedWidth = 0.0f;
    const auto beginResponsiveTool = [&](const char *label, float minimumWidth) {
        const std::string localizedLabel = localizedUiLabel(label);
        const float width = std::min(
            toolbarWidth,
            std::max(minimumWidth,
                     ImGui::CalcTextSize(localizedLabel.c_str()).x +
                         ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f));
        if (usedWidth > 0.0f && usedWidth + itemSpacing + width <= toolbarWidth)
        {
            ImGui::SameLine();
            usedWidth += itemSpacing;
        }
        else if (usedWidth > 0.0f)
        {
            usedWidth = 0.0f;
        }
        usedWidth += width;
        return width;
    };

    const char *inspectLabel = uiTextForLocale("Inspect", "Untersuchen");
    float toolWidth = beginResponsiveTool(inspectLabel, 100.0f);
    if (drawUiButton(inspectLabel, inspectActive ? UiButtonRole::Primary : UiButtonRole::Secondary,
                     ImVec2(toolWidth, 0.0f),
                     "Plain map clicks select cells and show details."))
    {
        _controller.setInteractionMode(MapViewerInteractionMode::InspectSelect);
    }
    const char *moveLabel = uiTextForLocale("Place party marker", "Gruppenmarker setzen");
    toolWidth = beginResponsiveTool(moveLabel, 120.0f);
    if (drawUiButton(moveLabel, moveActive ? UiButtonRole::Primary : UiButtonRole::Secondary,
                     ImVec2(toolWidth, 0.0f),
                     uiTextForLocale("Choose a map cell, then save this campaign location. Character and combat tokens stay in place.",
                         "Wähle ein Kartenfeld und speichere diesen Kampagnenstandort. Figuren und Kampfmarker bleiben an ihrem Platz.")))
    {
        _controller.setInteractionMode(MapViewerInteractionMode::MoveParty);
        _showDetails = true;
    }
    if (map.atlasLevel != MapLevel::Tactical)
    {
        toolWidth = beginResponsiveTool("Create Detail Map", 158.0f);
        if (drawUiButton("Create Detail Map", detailActive ? UiButtonRole::Primary : UiButtonRole::Secondary,
                         ImVec2(toolWidth, 0.0f),
                         "Drag an area on this map. The atlas keeps the parent map stable and generates a linked child only after review."))
        {
            _controller.setInteractionMode(MapViewerInteractionMode::SelectDetailArea);
            _showDetails = true;
        }
    }
    const std::string extraTool = localizedUiLabel(revealFogActive ? "Reveal Fog" : hideFogActive ? "Hide with Fog" : pingActive ? "Ping Players" :
        uiTextForLocale("Fog and pings", "Nebel und Pings"));
    toolWidth = beginResponsiveTool(extraTool.c_str(), ImGui::CalcTextSize(extraTool.c_str()).x + ImGui::GetFrameHeight() + 24.0f);
    ImGui::SetNextItemWidth(toolWidth);
    if (FormUi::BeginCombo("##FogAndPingTools", extraTool.c_str()))
    {
        if (ImGui::Selectable(localizedUiText("Reveal Fog"), revealFogActive))
            _controller.setInteractionMode(MapViewerInteractionMode::RevealFog);
        if (ImGui::Selectable(localizedUiText("Hide with Fog"), hideFogActive))
            _controller.setInteractionMode(MapViewerInteractionMode::HideFog);
        ImGui::BeginDisabled(!pingAvailable);
        if (ImGui::Selectable(localizedUiText("Ping Players"), pingActive))
            _controller.setInteractionMode(MapViewerInteractionMode::PingPlayers);
        ImGui::EndDisabled();
        if (!pingAvailable)
            ImGui::TextWrapped("%s", uiTextForLocale("Show this map in the player display to enable pings.",
                                                    "Zeige diese Karte in der Spieleranzeige, um Pings zu aktivieren."));
        ImGui::EndCombo();
    }

    if (!_controller._mapInteractionStatusMessage.empty())
        ImGui::TextWrapped("%s", _controller._mapInteractionStatusMessage.c_str());

    if (_controller._interactionMode == MapViewerInteractionMode::MoveParty)
    {
        ImGui::Spacing();
        drawPartyMarkerContext(_controller._currentCampaign, map, hasPendingMove,
            _controller._pendingPartyMoveX, _controller._pendingPartyMoveY);
        const bool canApply = hasPendingMove && !pendingIsCurrentPartyCell;
        const char *applyReason = !hasPendingMove ? "Choose a destination cell first." : "The pending destination is already the party's current cell.";
        const char *saveLabel = uiTextForLocale("Save party location", "Gruppenposition speichern");
        const char *cancelLabel = localizedUiText("Cancel");
        const int columns = TaskUi::equalColumnButtonsFit(saveLabel, cancelLabel, ImGui::GetStyle().FramePadding.x) ? 2 : 1;
        if (ImGui::BeginTable("PartyMarkerActions", columns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton(saveLabel, canApply, applyReason, UiButtonRole::Primary, ImVec2(-1, 0),
                uiTextForLocale("Save the location and run matching region audio triggers. Travel progress and time do not advance.",
                    "Speichert den Standort und startet passende Regions-Audioauslöser. Reisefortschritt und Zeit werden nicht erhöht.")))
                _controller.applyPendingPartyMove();
            ImGui::TableNextColumn();
            if (drawUiButton(cancelLabel, hasPendingMove, "There is no pending party move to cancel.", UiButtonRole::Secondary, ImVec2(-1, 0),
                "Clear the pending party destination. Right click the map does the same thing.")) _controller.clearPendingPartyMove();
            ImGui::EndTable();
        }
    }
    else if (_controller._interactionMode == MapViewerInteractionMode::SelectDetailArea)
    {
        ImGui::Spacing();
        // Reserve this row before the drag starts so the map does not move
        // underneath the pointer when the first selected cell appears.
        if (drawUiButton("Clear Area", _controller.hasDetailSelection(), "Select an area first.",
                         UiButtonRole::Secondary, ImVec2(110.0f, 0.0f), "Clear the selected detail area without changing the map."))
            _controller.clearDetailSelection();
    }
}
