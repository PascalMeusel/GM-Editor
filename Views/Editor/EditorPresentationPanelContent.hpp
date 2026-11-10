// Internal renderPresentationPanel continuation: player-safe source and reveal controls.
    }

    bool fullscreen = display.fullscreenMapOnly;
    if (FormUi::Checkbox(localizedUiLabel("Full-screen map on selected display").c_str(), &fullscreen))
        _controller.setLocalPlayerDisplayFullscreen(fullscreen);
    showHelpTooltip(
        "Full-screen output is available only for the map-only layout. Changing the output display activates Blackout first.");
    if (placement.fallbackToMainWindow)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kWarmAccent);
        ImGui::TextWrapped(
            "%s", localizedUiLabel(
                      "No dedicated selected display is available. Player Display stays in a safe window on the main display.")
                      .c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText(localizedUiLabel("Public camera").c_str());
    bool followCamera = display.publicCamera.followGmCamera;
    if (FormUi::Checkbox(localizedUiLabel("Follow GM map camera").c_str(), &followCamera))
        _controller.setLocalPlayerDisplayCameraFollow(followCamera);
    showHelpTooltip(
        "While the selected public map is open, pan and zoom from its GM map view are mirrored. Adjusting the controls below detaches the public camera.");

    const auto wholeMapCamera = LocalPlayerDisplayCameraPresets::wholeMapForPublicDisplay(campaign);
    const auto revealedAreaCamera = LocalPlayerDisplayCameraPresets::revealedAreaForPublicDisplay(campaign);
    const auto applyCameraPreset = [&](const std::optional<LocalPlayerDisplayCameraState> &camera) {
        if (camera)
            _controller.setLocalPlayerDisplayCamera(camera->normalizedCenterX, camera->normalizedCenterY, camera->zoom);
    };
    if (drawUiButton(uiTextForLocale("Fit whole map", "Ganze Karte einpassen"), wholeMapCamera.has_value(),
                     uiTextForLocale("Choose a valid player-visible map first.", "Wähle zuerst eine gültige, für Spieler sichtbare Karte."),
                     UiButtonRole::Secondary, ImVec2(0.0f, 0.0f),
                     uiTextForLocale("Center the complete public map and detach it from the GM camera.",
                                     "Zentriert die gesamte öffentliche Karte und löst sie von der GM-Kamera.")))
        applyCameraPreset(wholeMapCamera);
    TaskUi::sameLineIfFits();
    if (drawUiButton(uiTextForLocale("Frame revealed area", "Aufgedeckten Bereich einrahmen"), revealedAreaCamera.has_value(),
                     uiTextForLocale("Reveal at least one cell on a valid player-visible map first.",
                                     "Decke zuerst mindestens eine Zelle auf einer gültigen, für Spieler sichtbaren Karte auf."),
                     UiButtonRole::Secondary, ImVec2(0.0f, 0.0f),
                     uiTextForLocale("Center the revealed cells with a small margin and detach from the GM camera.",
                                     "Zentriert die aufgedeckten Zellen mit einem kleinen Rand und löst sie von der GM-Kamera.")))
        applyCameraPreset(revealedAreaCamera);

    float cameraX = display.publicCamera.normalizedCenterX;
    float cameraY = display.publicCamera.normalizedCenterY;
    float cameraZoom = display.publicCamera.zoom;
    bool cameraChanged = false;
    cameraChanged |= FormUi::SliderFloat(localizedUiLabel("Zoom").c_str(), &cameraZoom, 0.5f, 8.0f, "%.1fx");
    cameraChanged |= FormUi::SliderFloat(localizedUiLabel("Horizontal center").c_str(), &cameraX, 0.0f, 1.0f, "%.2f");
    cameraChanged |= FormUi::SliderFloat(localizedUiLabel("Vertical center").c_str(), &cameraY, 0.0f, 1.0f, "%.2f");
    if (cameraChanged)
        _controller.setLocalPlayerDisplayCamera(cameraX, cameraY, cameraZoom);
    if (!followCamera)
        ImGui::TextDisabled(
            "%s", localizedUiLabel("Public camera detached; player framing can differ from the GM view.").c_str());

    } // Screen and camera settings

    if (!display.mapOnly)
    {
        float textScale = display.playerTextScale;
        ImGui::SetNextItemWidth(std::min(320.0f, ImGui::GetContentRegionAvail().x));
        if (FormUi::SliderFloat(uiTextForLocale("Player text size", "Textgröße für Spieler"), &textScale, 1.0f, 1.8f, "%.2fx"))
            _controller.setPlayerTextScale(textScale);
        showHelpTooltip(uiTextForLocale("Changes only the player window. Shared content stays the same.",
                                        "Ändert nur das Spielerfenster. Die geteilten Inhalte bleiben gleich."));
    }

    const MapModel *publicMap = findPlayerSafePublishedMap(campaign, display.publicMapId);
    if (publicMap != nullptr)
    {
        ImGui::SeparatorText(localizedUiLabel("Player map content").c_str());
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "Terrain, coasts, water, approved background art, and fog remain part of the base player map. Optional campaign knowledge stays hidden until enabled here.")
                .c_str());
        const int layerColumns =
            ImGui::GetContentRegionAvail().x >= 760.0f ? 3 : (ImGui::GetContentRegionAvail().x >= 440.0f ? 2 : 1);
        if (ImGui::BeginTable("PresentationPlayerMapCategories", layerColumns,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const auto category = [&](const char *label, MapPlayerReleaseCategory releaseCategory) {
                ImGui::TableNextColumn();
                bool visible = mapPlayerReleaseCategoryEnabled(publicMap->playerRelease, releaseCategory);
                const std::string id = localizedUiLabel(label) + "##PresentationPlayerMapCategory" +
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
        ImGui::TextDisabled(
            "%s", localizedUiLabel("Changing a category activates Blackout and requires a new player review.").c_str());

        ImGui::TextDisabled(
            "%s",
            localizedUiLabel(
                "Whole-map fog changes require confirmation and return the player display to Blackout for review.")
                .c_str());
        const bool fogButtonsInline = ImGui::GetContentRegionAvail().x >= 430.0f;
        if (drawUiButton("Reveal entire public map", UiButtonRole::Secondary, ImVec2(200.0f, 0.0f),
                         "Reveal every cell on the selected public map after confirmation."))
            ImGui::OpenPopup(localizedUiLabel("Reveal entire public map").c_str());
        if (fogButtonsInline)
            ImGui::SameLine();
        if (drawUiButton("Hide entire public map", UiButtonRole::Secondary, ImVec2(200.0f, 0.0f),
                         "Hide every cell on the selected public map after confirmation."))
            ImGui::OpenPopup(localizedUiLabel("Hide entire public map").c_str());
    }

    setResponsiveAutoSizeWindow(640.0f, 420.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("Reveal entire public map").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "Reveal every cell on this public map? Players will not see the change until you review and choose Show to players again.")
                .c_str());
        drawInlineLabelValue("Map", publicMap != nullptr && !publicMap->name.empty() ? publicMap->name.c_str()
                                                                                     : localizedUiText("Untitled map"));
        if (drawModalActionButtons("Reveal entire map", UiButtonRole::Advanced))
            _controller.revealEntirePublicMap();
        ImGui::EndPopup();
    }

    setResponsiveAutoSizeWindow(640.0f, 420.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("Hide entire public map").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "Hide every cell on this public map? Players will not see the change until you review and choose Show to players again.")
                .c_str());
        drawInlineLabelValue("Map", publicMap != nullptr && !publicMap->name.empty() ? publicMap->name.c_str()
                                                                                     : localizedUiText("Untitled map"));
        if (drawModalActionButtons("Hide entire map", UiButtonRole::Advanced))
            _controller.hideEntirePublicMap();
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(localizedUiLabel("Preview Summary").c_str());
    const PlayerSafeStoryScene publicScene = buildPlayerSafeStoryScene(campaign);
    ImGui::TextWrapped(localizedUiLabel("Scene: %s").c_str(),
                       publicScene.available ? publicScene.title.c_str() : localizedUiText("No public scene"));
    ImGui::TextWrapped(localizedUiLabel("Map: %s").c_str(),
                       display.showMap ? mapLabel(campaign, display.publicMapId).c_str() : localizedUiText("Hidden"));
    ImGui::TextWrapped(localizedUiLabel("Initiative: %s").c_str(),
                       display.showInitiative ? encounterLabel(campaign, display.publicEncounterId).c_str()
                                              : localizedUiText("Hidden"));
    ImGui::TextWrapped(localizedUiLabel("Visible NPCs: %d | Public handouts: %d").c_str(), static_cast<int>(npcCount),
                       static_cast<int>(handoutCount));

    const int revealColumns = presentationWidth >= 760.0f ? 2 : 1;
    if (ImGui::BeginTable("PresentationRevealLists", revealColumns,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame |
                              ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SeparatorText(localizedUiLabel("NPC Reveals").c_str());
        if (campaign.npcs.empty())
            ImGui::TextDisabled(localizedUiLabel("No NPCs available.").c_str());
        for (const NPCModel &npc : campaign.npcs)
        {
            bool visible = playerSafeContainsId(display.visibleNpcIds, npc.metadata.id);
            const std::string label = (npc.name.empty() ? localizedUiLabel("Untitled NPC") : npc.name) +
                                      "##PresentationNpc" + npc.metadata.id;
            if (FormUi::Checkbox(label.c_str(), &visible))
                _controller.setPublicNpcVisibility(npc.metadata.id, visible);
            if (!npc.role.empty())
                ImGui::TextDisabled(localizedUiLabel("%s").c_str(), npc.role.c_str());
        }

        ImGui::TableNextColumn();
        ImGui::SeparatorText(localizedUiLabel("Handout Reveals").c_str());
        if (campaign.handouts.empty())
            ImGui::TextDisabled(localizedUiLabel("No handouts available.").c_str());
        for (const HandoutModel &handout : campaign.handouts)
        {
            bool visible = playerSafeContainsId(display.publicHandoutIds, handout.metadata.id);
            const std::string label = (handout.title.empty() ? localizedUiLabel("Untitled handout") : handout.title) +
                                      "##PresentationHandout" + handout.metadata.id;
            if (FormUi::Checkbox(label.c_str(), &visible))
                _controller.setPublicHandoutVisibility(handout.metadata.id, visible);
            if (!handout.body.empty())
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), handout.body.substr(0, 120).c_str());
        }
        ImGui::EndTable();
    }

    ImGui::End();
