#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

using namespace gm::map_viewer_detail;

void MapViewerView::drawTravelStatus(const MapModel &map)
{
    const PartyTravelPlanModel &travel = _controller._currentCampaign.partyTravel;
    const bool hasPendingMove =
        _controller._hasPendingPartyMove &&
        isMapCellInside(map, _controller._pendingPartyMoveX, _controller._pendingPartyMoveY);

    std::string route = travelRouteNameForMap(travel, map);
    if (hasActiveTravelRoute(travel))
    {
        const std::string progress = travelProgressText(travel);
        route = formatLocalizedUi("%s - %s", route.c_str(), progress.c_str());
    }

    const std::string partyLocation = partyLocationText(_controller._currentCampaign, map);
    std::string detail = formatLocalizedUi(uiTextForLocale("Party location: %s", "Gruppenposition: %s"), partyLocation.c_str());
    if (hasActiveTravelRoute(travel) || !travel.travelPace.empty())
    {
        const std::string resources = travelResourcesText(travel);
        detail = formatLocalizedUi("%s - %s", detail.c_str(), resources.c_str());
    }
    const std::string notes = compactTravelNotes(travel.notes);
    if (!notes.empty())
        detail = formatLocalizedUi("%s - Note: %s", detail.c_str(), notes.c_str());

    const std::string nextAction = nextTravelActionText(
        travel,
        map,
        _controller._interactionMode == MapViewerInteractionMode::MoveParty,
        hasPendingMove);

    const std::string travelMode = localizedUiLabel("Travel");
    UiStatusActionStrip strip;
    strip.mode = travelMode.c_str();
    strip.selection = route.c_str();
    strip.nextAction = nextAction.c_str();
    strip.help = UiGuidance::text(UiGuidance::Topic::TravelWatch).purpose;
    drawStatusActionStrip("MapViewerTravelStatusStrip", strip);

    drawDisabledWrapped(detail);
}

bool MapViewerView::drawTravelWorkflowPanel(MapModel &map)
{
    PartyTravelPlanModel &travel = _controller._currentCampaign.partyTravel;
    const bool activeRoute = hasActiveTravelRoute(travel);
    const bool activeRouteOnThisMap = activeRoute && travel.currentMapId == map.metadata.id;
    const MapModel *activeMap = activeRoute && !travel.currentMapId.empty() ? _controller.findMapById(travel.currentMapId) : nullptr;

    ImGui::TextDisabled(localizedUiLabel("Current Route").c_str());
    ImGui::SameLine();
    const std::string activeRouteName = travelRouteNameForMap(travel, map);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), activeRouteName.c_str());
    if (activeRoute && !activeRouteOnThisMap)
    {
        const std::string activeMapName = activeMap ? mapDisplayName(*activeMap) : localizedUiLabel("another map");
        ImGui::TextDisabled(localizedUiLabel("Active on: %s").c_str(), activeMapName.c_str());
    }

    if (activeRoute)
    {
        const std::string progress = travelProgressText(travel);
        ImGui::ProgressBar(travelProgressFraction(travel), ImVec2(-1.0f, 0.0f), progress.c_str());
        showHelpTooltip("Green progress on the map marks the active route when that route belongs to the selected map.");
    }
    else
    {
        ImGui::TextDisabled(localizedUiLabel("No route is in progress.").c_str());
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s", uiTextForLocale("Party location", "Gruppenposition"));
    ImGui::SameLine();
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), partyLocationText(_controller._currentCampaign, map).c_str());

    _controller._selectedTravelPaceIndex = travel.travelPace == "Slow" ? 0 : (travel.travelPace == "Fast" ? 2 : 1);
    const std::string localizedPaces = localizedComboItems(travelPaceItems);
    if (FormUi::Combo(localizedUiLabel("Pace##TravelPace").c_str(), &_controller._selectedTravelPaceIndex, localizedPaces.c_str(), -1))
    {
        travel.travelPace = _controller._selectedTravelPaceIndex == 0 ? "Slow" : (_controller._selectedTravelPaceIndex == 2 ? "Fast" : "Normal");
        _controller.saveCampaign();
    }
    showHelpTooltip(uiTextForLocale("Travel watches add up to 6 / 8 / 10 miles at slow / normal / fast pace.",
        "Reiseabschnitte ergänzen bis zu 6 / 8 / 10 Meilen bei langsamem / normalem / schnellem Tempo."));

    drawDisabledWrapped(travelResourcesText(travel));
    UiGuidance::drawPurpose(UiGuidance::Topic::TravelWatch);
    UiGuidance::drawDetails(UiGuidance::Topic::TravelWatch);
    const std::string notesPreview = compactTravelNotes(travel.notes);
    if (!notesPreview.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("Note").c_str());
        ImGui::SameLine();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), notesPreview.c_str());
        showHelpTooltip(travel.notes.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();

    const MapRouteModel *selectedRoute = nullptr;
    if (!map.routes.empty())
    {
        std::vector<std::string> routeLabelStorage;
        std::vector<const char *> routeLabels;
        for (const MapRouteModel &route : map.routes)
            routeLabelStorage.push_back(route.name.empty() ? localizedUiLabel("<Route>") : route.name);
        for (const std::string &label : routeLabelStorage)
            routeLabels.push_back(label.c_str());
        _controller._selectedRouteIndex = std::clamp(_controller._selectedRouteIndex, 0, static_cast<int>(routeLabels.size()) - 1);
        selectedRoute = &map.routes[static_cast<std::size_t>(_controller._selectedRouteIndex)];
        FormUi::Combo(localizedUiLabel("Next Route##TravelRoute").c_str(), &_controller._selectedRouteIndex, routeLabels.data(), static_cast<int>(routeLabels.size()));
        showHelpTooltip("Choose the route to begin or restart. Starting a route resets its travel progress.");
        selectedRoute = &map.routes[static_cast<std::size_t>(_controller._selectedRouteIndex)];
    }
    else
    {
        ImGui::TextDisabled(localizedUiLabel("No routes on this map.").c_str());
        ImGui::TextWrapped("%s", uiTextForLocale(
            "To track a journey, open this map in the map editor, choose Add Route, and set its distance. Illustrated roads alone are not travel routes. You can still generate a local scene below.",
            "Öffne diese Karte im Karteneditor, wähle Route hinzufügen und lege die Entfernung fest. Gezeichnete Straßen sind noch keine Reiserouten. Eine lokale Szene kannst du unten trotzdem erzeugen."));
    }

    const bool canStartRoute = selectedRoute && selectedRoute->distanceMiles > 0.0f;
    const char *startRouteReason = selectedRoute ? "The selected route needs a distance before travel can start." : "No routes exist on this map yet.";
    const bool selectedRouteIsActive = selectedRoute && activeRouteOnThisMap && travel.routeId == selectedRoute->metadata.id;
    if (drawUiButton(selectedRouteIsActive ? "Restart Route" : "Start Route",
                     canStartRoute,
                     startRouteReason,
                     UiButtonRole::Primary,
                     ImVec2(-1.0f, 0.0f),
                     "Start travel on the selected route and reset route progress."))
    {
        _controller.startTravelOnSelectedRoute();
    }

    const bool canAdvance = hasActiveTravelRoute(travel) && !isTravelRouteComplete(travel);
    if (canAdvance)
    {
        const float miles = travel.travelPace == "Slow" ? 6.0f : travel.travelPace == "Fast" ? 10.0f : 8.0f;
        const float after = std::min(travel.distanceMiles, travel.milesTraveled + miles);
        ImGui::TextWrapped(uiTextForLocale("Next watch — %s: %s → %s / %s mi; watches +1, rations +%d, encounter checks +1.",
            "Nächster Abschnitt — %s: %s → %s / %s mi; Abschnitte +1, Rationen +%d, Begegnungsprüfungen +1."),
            travelRouteNameForMap(travel, map).c_str(), formatMiles(travel.milesTraveled).c_str(), formatMiles(after).c_str(),
            formatMiles(travel.distanceMiles).c_str(), std::max(1, static_cast<int>(_controller._currentCampaign.characters.size())));
    }
    const char *advanceReason = !hasActiveTravelRoute(travel) ? "Start a route before advancing travel watches." : "The active route is already complete.";
    if (drawUiButton(uiTextForLocale("Advance travel watch", "Reiseabschnitt fortschreiben"),
                     canAdvance,
                     advanceReason,
                     UiButtonRole::Primary,
                     ImVec2(-1.0f, 0.0f),
                     "Advance route progress, watches, rations, and encounter checks."))
    {
        _controller.advanceTravelWatch();
    }

    if (FormUi::InputTextMultiline(localizedUiLabel("Notes##TravelNotes").c_str(), &travel.notes, ImVec2(-1.0f, 58.0f)))
        _controller.saveCampaign();
    showHelpTooltip("Optional travel notes. A short preview appears in the travel status when notes are present.");

    ImGui::Spacing();
    ImGui::SeparatorText(localizedUiLabel("Next travel scene").c_str());
    ImGui::TextWrapped(localizedUiLabel("Create a small playable scene from the active route. Add only the details that must or must not appear; the generator fills the rest.").c_str());
    const char *purposeItems = "Quiet\0Discovery\0Social\0Obstacle\0Danger\0";
    FormUi::Combo(localizedUiLabel("Focus").c_str(), &_controller._travelScenePurposeIndex, localizedComboItems(purposeItems).c_str());
    FormUi::InputText(localizedUiLabel("Must include##TravelSceneRequired").c_str(), &_controller._travelRequiredElements);
    showHelpTooltip("Separate multiple elements with commas, for example: abandoned tower, river crossing.");
    FormUi::InputText(localizedUiLabel("Must avoid##TravelSceneExcluded").c_str(), &_controller._travelExcludedElements);
    showHelpTooltip("Separate multiple exclusions with commas, for example: combat, settlement.");
    if (ImGui::TreeNode(localizedUiLabel("Options##TravelSceneOptions").c_str()))
    {
        FormUi::InputScalar(localizedUiLabel("Seed##TravelSceneSeed").c_str(), ImGuiDataType_U32,
                           &_controller._travelSceneSeed);
        FormUi::SliderInt(localizedUiLabel("Danger level").c_str(), &_controller._travelDangerLevel, 0, 5);
        FormUi::Checkbox(localizedUiLabel("Prepare for possible combat").c_str(), &_controller._travelSceneCombatReady);
        ImGui::TreePop();
    }
    if (drawUiButton("Generate 3 travel scenes", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                     "Generate three deterministic local scenes without changing the current map."))
        _controller.generateTravelSceneCandidates();

    for (int index = 0; index < static_cast<int>(_controller._travelSceneCandidates.size()); ++index)
    {
        const TravelSceneCandidate &candidate = _controller._travelSceneCandidates[static_cast<std::size_t>(index)];
        ImGui::PushID(4100 + index);
        const bool selected = _controller._selectedTravelSceneCandidate == index;
        const std::string label = localizedGeneratedMapName(candidate.title) + "  ·  " + std::to_string(candidate.quality.score) + "/100";
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, 34.0f)))
            _controller._selectedTravelSceneCandidate = index;
        if (selected)
        {
            ImGui::TextWrapped("%s", localizedUiLabel(candidate.summary.c_str()).c_str());
            ImGui::TextDisabled("%s", localizedUiLabel("Discovery").c_str());
            ImGui::SameLine();
            ImGui::TextWrapped("%s", localizedUiLabel(candidate.discovery.c_str()).c_str());
            ImGui::TextDisabled("%s", localizedUiLabel("Complication").c_str());
            ImGui::SameLine();
            ImGui::TextWrapped("%s", localizedUiLabel(candidate.complication.c_str()).c_str());
            ImGui::TextDisabled("%s", localizedUiLabel("GM secret").c_str());
            ImGui::SameLine();
            ImGui::TextWrapped("%s", localizedUiLabel(candidate.gmSecret.c_str()).c_str());
        }
        ImGui::PopID();
    }
    if (!_controller._travelSceneCandidates.empty())
    {
        if (drawUiButton("Add selected scene", _controller._selectedTravelSceneCandidate >= 0,
                         "Choose a scene first.", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                         "Add the selected editable scene below this map in the atlas."))
        {
            _controller.acceptSelectedTravelScene(false);
            return true;
        }
        const TravelSceneCandidate *selected = _controller._selectedTravelSceneCandidate >= 0
                                                   ? &_controller._travelSceneCandidates[static_cast<std::size_t>(_controller._selectedTravelSceneCandidate)]
                                                   : nullptr;
        if (drawUiButton("Prepare as encounter", selected && selected->canPrepareEncounter,
                         "Choose Danger or Obstacle, or enable combat readiness in Options.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                         "Create a linked 5-foot square tactical map and a prepared encounter."))
        {
            _controller.acceptSelectedTravelScene(true);
            return true;
        }
    }
    if (!_controller._travelSceneStatus.empty())
        ImGui::TextWrapped("%s", localizedUiLabel(_controller._travelSceneStatus.c_str()).c_str());
    return false;
}
