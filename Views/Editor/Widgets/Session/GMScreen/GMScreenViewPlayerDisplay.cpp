#include "Views/Editor/Widgets/Session/GMScreen/GMScreenViewSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

void GMScreenView::drawLocalPlayerDisplayControls()
{
    _playerReviewDrawFrame=ImGui::GetFrameCount();
    if (_controller._currentCampaign.presentationModeActive || !isVisible()) { resetPlayerReview(); return; }
    if(_publicDisplayReviewAction!=PublicDisplayReviewAction::None&&
        !ImGui::IsPopupOpen(localizedUiLabel("GM Arm Player Display").c_str())&&
        !ImGui::IsPopupOpen(localizedUiLabel("GM Publish Player Display").c_str()))resetPlayerReview();
    pollPlayerReview();
    LocalPlayerDisplayModel &display = _controller._currentCampaign.localPlayerDisplay;

    ImGui::SeparatorText(gm::localization::trUi("Public Table Display"));
    drawPublicDisplayStatusStrip("GMScreenPublicDisplayStatus");
    UiGuidance::drawPurpose(UiGuidance::Topic::PlayerDisplay);
    UiGuidance::drawDetails(UiGuidance::Topic::PlayerDisplay);

    auto capturePublicDisplayRestore = [&]() {
        _publicDisplayRestoreSnapshot = display;
        _hasPublicDisplayRestoreSnapshot = true;
    };

    if (display.enabled)
    {
        if (localPlayerDisplayIsPublished(display))
        {
            if (drawUiButton("Blackout", UiButtonRole::Destructive, ImVec2(120.0f, 0.0f), "Immediately hide all player-facing content. Shortcut: B."))
                _controller.blackoutLocalPlayerDisplayOutput();
        }
        else if (drawUiButton("Show to players",
                              localPlayerDisplayHasPublicPayload(_controller._currentCampaign),
                              "Choose at least one valid public scene, map, initiative source, NPC, or handout first.",
                              UiButtonRole::Primary,
                              ImVec2(150.0f, 0.0f),
                              "Explicitly publish the reviewed player-safe state."))
        {
            preparePublicDisplayReview(PublicDisplayReviewAction::Publish);
            ImGui::OpenPopup(localizedUiLabel("GM Publish Player Display").c_str());
        }
        ImGui::SameLine();
        if (drawUiButton(uiTextForLocale("Close player window", "Spielerfenster schließen"), UiButtonRole::Secondary))
        {
            capturePublicDisplayRestore();
            disableLocalPlayerDisplay(display);
            _controller.touchLocalPlayerDisplay();
        }
    }
    else if (drawUiButton(uiTextForLocale("Open player window…", "Spielerfenster öffnen…"),
                          localPlayerDisplayHasPublicPayload(_controller._currentCampaign),
                          "Choose at least one valid public scene, map, initiative source, NPC, or handout first.",
                          UiButtonRole::Primary,
                          ImVec2(0.0f, 0.0f),
                          "Review the public selections before opening the local player-facing display."))
    {
        preparePublicDisplayReview(PublicDisplayReviewAction::Arm);
        ImGui::OpenPopup(localizedUiLabel("GM Arm Player Display").c_str());
    }
    ImGui::SameLine();
    if (drawUiButton("Clear Public Display", hasPublicDisplayPayload(display), "No public display selections are staged.", UiButtonRole::Destructive, ImVec2(0.0f, 0.0f), "Disables the local player window and removes current public scene, map, initiative, NPC, handout, and art selections."))
        ImGui::OpenPopup(localizedUiLabel("GM Clear Public Display").c_str());
    ImGui::SameLine();
    if (drawUiButton("Restore Last",
                     _hasPublicDisplayRestoreSnapshot,
                     "No previous public display state has been captured in this GM Screen session.",
                     UiButtonRole::Secondary,
                     ImVec2(104.0f, 0.0f),
                     "Restore the last public state captured before arming, disabling, or clearing."))
    {
        const bool windowWasOpen = display.enabled;
        display = _publicDisplayRestoreSnapshot;
        display.enabled = windowWasOpen;
        _hasPublicDisplayRestoreSnapshot = false;
        _controller.touchLocalPlayerDisplay();
    }
    ImGui::TextDisabled(localizedUiLabel("%s: %s").c_str(), gm::localization::trUi("Panels"), publicVisibilitySummary(display).c_str());

    drawPublicDisplayReviewModal("GM Arm Player Display");
    drawPublicDisplayReviewModal("GM Publish Player Display");
    if(_publicDisplayReviewAction!=PublicDisplayReviewAction::None&&
        !ImGui::IsPopupOpen(localizedUiLabel("GM Arm Player Display").c_str())&&
        !ImGui::IsPopupOpen(localizedUiLabel("GM Publish Player Display").c_str()))resetPlayerReview();

    setResponsiveAutoSizeWindow(640.0f, 420.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("GM Clear Public Display").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Clear the current public scene, map, initiative source, NPC reveals, handouts, and shared art?"));
        drawInlineLabelValue("Scene", publicSceneLabel(_controller._currentCampaign, display.publicSceneId).c_str());
        drawInlineLabelValue("Map", mapLabel(_controller._currentCampaign, display.publicMapId).c_str());
        drawInlineLabelValue("Initiative", encounterLabel(_controller._currentCampaign, display.publicEncounterId).c_str());
        const std::string revealSummary = std::to_string(static_cast<int>(display.visibleNpcIds.size())) + " " + localizedUiLabel("NPCs") + ", " +
                                          std::to_string(static_cast<int>(display.publicHandoutIds.size())) + " " + localizedUiLabel("Handouts");
        drawInlineLabelValue("Reveals", revealSummary.c_str());
        if (drawModalActionButtons("Clear Public Display", UiButtonRole::Destructive))
        {
            capturePublicDisplayRestore();
            _controller.clearLocalPlayerDisplay();
        }
        ImGui::EndPopup();
    }

    if (beginAdvancedHeader("Display Panels", "Choose which player-facing panels are enabled. Kept closed after arming so the public-state summary remains primary."))
    {
        bool mapOnly = display.mapOnly;
        if (FormUi::Checkbox(gm::localization::trUi("Map-only display"), &mapOnly))
        {
            display.mapOnly = mapOnly;
            ::blackoutLocalPlayerDisplay(display);
            _controller.touchLocalPlayerDisplay();
        }
        showHelpTooltip("Map-only display uses the full detached window for the published map.");

        if (ImGui::BeginTable("PublicDisplayFlags", 5, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (FormUi::Checkbox(gm::localization::trUi("Show map"), &display.showMap))
            {
                ::blackoutLocalPlayerDisplay(display);
                _controller.touchLocalPlayerDisplay();
            }
            ImGui::TableNextColumn();
            if (FormUi::Checkbox(gm::localization::trUi("Show initiative"), &display.showInitiative))
            {
                ::blackoutLocalPlayerDisplay(display);
                _controller.touchLocalPlayerDisplay();
            }
            ImGui::TableNextColumn();
            if (FormUi::Checkbox(gm::localization::trUi("Show NPCs"), &display.showVisibleNpcs))
            {
                ::blackoutLocalPlayerDisplay(display);
                _controller.touchLocalPlayerDisplay();
            }
            ImGui::TableNextColumn();
            if (FormUi::Checkbox(gm::localization::trUi("Show handouts"), &display.showPublicHandouts))
            {
                ::blackoutLocalPlayerDisplay(display);
                _controller.touchLocalPlayerDisplay();
            }
            ImGui::TableNextColumn();
            if (FormUi::Checkbox(gm::localization::trUi("Show shared art"), &display.showSharedArt))
            {
                ::blackoutLocalPlayerDisplay(display);
                _controller.touchLocalPlayerDisplay();
            }
            ImGui::EndTable();
        }

        ImGui::SeparatorText(gm::localization::trUi("Public camera"));
        bool followCamera = display.publicCamera.followGmCamera;
        if (FormUi::Checkbox(gm::localization::trUi("Follow GM camera"), &followCamera))
        {
            display.publicCamera.followGmCamera = followCamera;
            _controller.touchLocalPlayerDisplay();
        }
        bool cameraChanged = false;
        cameraChanged |= FormUi::SliderFloat(gm::localization::trUi("Zoom"), &display.publicCamera.zoom, 0.5f, 8.0f, "%.1fx");
        cameraChanged |= FormUi::SliderFloat(gm::localization::trUi("Horizontal center"), &display.publicCamera.normalizedCenterX, 0.0f, 1.0f, "%.2f");
        cameraChanged |= FormUi::SliderFloat(gm::localization::trUi("Vertical center"), &display.publicCamera.normalizedCenterY, 0.0f, 1.0f, "%.2f");
        if (cameraChanged)
        {
            normalizeLocalPlayerDisplayCamera(display.publicCamera);
            _controller.touchLocalPlayerDisplay();
        }
        if (drawUiButton("Ping camera center",
                         localPlayerDisplayIsPublished(display) && display.showMap,
                         "Publish a visible map before sending a ping.",
                         UiButtonRole::Secondary,
                         ImVec2(170.0f, 0.0f),
                         "Show a three-second player-safe ping at the public camera center."))
            _controller.pingLocalPlayerDisplay();
    }

    if (beginAdvancedHeader("Public Sources", "Choose exact scene, map, and initiative sources for the player display."))
    {
        if (ImGui::BeginTable("PublicDisplaySelectors", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (FormUi::BeginCombo(gm::localization::trUi("Public Scene"), publicSceneLabel(_controller._currentCampaign, display.publicSceneId).c_str()))
            {
                if (ImGui::Selectable(localizedUiLabel("None").c_str(), display.publicSceneId.empty()))
                    _controller.setPublicScene("");
                for (const StoryThreadModel &thread : _controller._currentCampaign.storyThreads)
                {
                    for (const StoryBeatModel &beat : thread.beats)
                    {
                        const std::string label = thread.title + " / " + beat.title;
                        if (ImGui::Selectable(label.c_str(), display.publicSceneId == beat.metadata.id))
                            _controller.setPublicScene(beat.metadata.id);
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextColumn();
            if (FormUi::BeginCombo(gm::localization::trUi("Public Map"), mapLabel(_controller._currentCampaign, display.publicMapId).c_str()))
            {
                if (ImGui::Selectable(localizedUiLabel("None").c_str(), display.publicMapId.empty()))
                    _controller.setPublicMap("");
                for (const MapModel &map : _controller._currentCampaign.maps)
                {
                    const std::string label = map.name.empty() ? localizedUiLabel("<Unnamed map>") : map.name;
                    if (ImGui::Selectable(label.c_str(), display.publicMapId == map.metadata.id))
                        _controller.setPublicMap(map.metadata.id);
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextColumn();
            if (FormUi::BeginCombo(gm::localization::trUi("Initiative Source"), encounterLabel(_controller._currentCampaign, display.publicEncounterId).c_str()))
            {
                if (ImGui::Selectable(localizedUiLabel("None").c_str(), display.publicEncounterId.empty()))
                    _controller.setPublicEncounter("");
                for (const EncounterModel &encounter : _controller._currentCampaign.encounters)
                {
                    const std::string label = encounter.name.empty() ? localizedUiLabel("<Unnamed encounter>") : encounter.name;
                    if (ImGui::Selectable(label.c_str(), display.publicEncounterId == encounter.metadata.id))
                        _controller.setPublicEncounter(encounter.metadata.id);
                }
                ImGui::EndCombo();
            }
            ImGui::EndTable();
        }

        const MapModel *publicMap = findPlayerSafePublishedMap(
            _controller._currentCampaign, display.publicMapId);
        if (publicMap != nullptr)
        {
            ImGui::SeparatorText(localizedUiLabel("Player map content").c_str());
            ImGui::TextWrapped("%s", localizedUiLabel(
                "Terrain, coasts, water, approved background art, and fog remain part of the base player map. Optional campaign knowledge stays hidden until enabled here.").c_str());
            const int layerColumns = ImGui::GetContentRegionAvail().x >= 760.0f ? 3 :
                                     (ImGui::GetContentRegionAvail().x >= 440.0f ? 2 : 1);
            if (ImGui::BeginTable("GMScreenPlayerMapCategories", layerColumns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
            {
                const auto category = [&](const char *label, MapPlayerReleaseCategory releaseCategory) {
                    ImGui::TableNextColumn();
                    bool visible = mapPlayerReleaseCategoryEnabled(publicMap->playerRelease, releaseCategory);
                    const std::string id = localizedUiLabel(label) + "##GMScreenPlayerMapCategory" +
                                           std::to_string(static_cast<int>(releaseCategory));
                    if (FormUi::Checkbox(id.c_str(), &visible))
                        _controller.setPublicMapReleaseCategory(releaseCategory, visible);
                };
                category("Political borders", MapPlayerReleaseCategory::PoliticalBorders);
                category("Roads and routes", MapPlayerReleaseCategory::WaysAndRoutes);
                category("Places and markers", MapPlayerReleaseCategory::PlacesAndMarkers);
                category("Names and labels", MapPlayerReleaseCategory::Labels);
                category("Tokens and annotations", MapPlayerReleaseCategory::TokensAndAnnotations);
                ImGui::EndTable();
            }
            ImGui::TextDisabled("%s", localizedUiLabel(
                "Changing a category activates Blackout and requires a new player review.").c_str());
        }
    }

    const std::string visibleNpcHeader = localizedUiLabel("NPC Reveals") + " (" +
                                         std::to_string(static_cast<int>(display.visibleNpcIds.size())) + " " +
                                         localizedUiLabel("Public") + ")";
    if (beginContextHeader(visibleNpcHeader.c_str(), !display.visibleNpcIds.empty(), "Reveal individual NPCs only when the public display needs NPC names or portraits."))
    {
        if (_controller._currentCampaign.npcs.empty())
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No NPCs available."));
        for (const NPCModel &npc : _controller._currentCampaign.npcs)
        {
            bool visible = containsId(display.visibleNpcIds, npc.metadata.id);
            const std::string label = (npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name) + "##PublicNpc" + npc.metadata.id;
            if (FormUi::Checkbox(label.c_str(), &visible))
                _controller.setNpcPublicVisibility(npc.metadata.id, visible);
        }
    }
}
