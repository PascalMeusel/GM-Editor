// Internal renderPresentationPanel continuation: window setup and publication status.
    if (!_presentationPanelOpen)
        return;

    const CompactUiDensityScope uiDensity;
    // Keep the parent and its private comparison on the same GM viewport.
    // A detached parent otherwise stays above a modal hosted by the main view.
    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    setResponsiveWindowSize(ImVec2(820.0f, 680.0f), ImVec2(520.0f, 420.0f));
    if (!ImGui::Begin(localizedUiLabel("Presentation Panel").c_str(), &_presentationPanelOpen)||!_presentationPanelOpen)
    {
        _playerReviewSession.clear();
        _presentationReviewAction=PresentationReviewAction::None;
        _controller._workspace.playerReviewTarget.reset();
        _controller._workspace.playerReviewError.clear();
        ImGui::End();
        return;
    }

    // Revoke before adopting: a completed worker must not capture assets after
    // its parent was collapsed or its exact owned popup was closed.
    if(_presentationReviewAction!=PresentationReviewAction::None&&
        !ImGui::IsPopupOpen(localizedUiLabel("Presentation Arm Review").c_str())&&
        !ImGui::IsPopupOpen(localizedUiLabel("Presentation Publish Review").c_str()))
    { _playerReviewSession.clear(); _presentationReviewAction=PresentationReviewAction::None; }
    pollPresentationPlayerReview();

    if (_controller.consumeCurrentPlayerReview())
    {
        const bool arm = !_controller.currentCampaign().localPlayerDisplay.enabled;
        preparePresentationPlayerReview(arm ? PresentationReviewAction::Arm : PresentationReviewAction::Publish);
        ImGui::OpenPopup(localizedUiLabel(arm ? "Presentation Arm Review" : "Presentation Publish Review").c_str());
    }
    if (!_controller._workspace.playerReviewError.empty())
        ImGui::TextWrapped("%s", _controller._workspace.playerReviewError.c_str());

    if (_openPresentationArmReviewNextFrame)
    {
        // MapCreator only stages a persisted public selection and forces
        // blackout.  Opening this existing fail-closed review is deliberately
        // separate from its explicit Arm action below.
        preparePresentationPlayerReview(PresentationReviewAction::Arm);
        ImGui::OpenPopup(localizedUiLabel("Presentation Arm Review").c_str());
        _openPresentationArmReviewNextFrame = false;
    }

    const Campaign &campaign = _controller.currentCampaign();
    const LocalPlayerDisplayModel &display = campaign.localPlayerDisplay;
    const std::string mode = !display.enabled
        ? uiTextForLocale("Player window closed", "Spielerfenster geschlossen")
        : (localPlayerDisplayIsPublished(display)
            ? uiTextForLocale("Player window open · showing content", "Spielerfenster offen · Inhalte sichtbar")
            : uiTextForLocale("Player window open · black screen", "Spielerfenster offen · Schwarzbild"));
    const std::string selection = presentationSelectionLabel(campaign);
    const std::size_t npcCount = buildPlayerSafeNpcSummaries(campaign).size();
    const std::size_t handoutCount = buildPlayerSafeHandouts(campaign).size();
    const std::string next = !display.enabled ? uiTextForLocale("Review the selected content, then open the player window.", "Prüfe die gewählten Inhalte und öffne dann das Spielerfenster.")
                                              : (localPlayerDisplayIsPublished(display)
                                                     ? "Use Blackout to hide the player view immediately."
                                                     : "Review the staged content, then show it to players.");
    UiStatusActionStrip strip;
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = next.c_str();
    strip.help =
        "This panel controls what the optional local Player Display can show. GM notes, secrets, hidden tokens, and unrevealed links remain filtered out.";
    ImGui::TextColored(localPlayerDisplayIsPublished(display) ? WorkspaceUi::Teal : WorkspaceUi::Amber, "%s",
                       localizedUiLabel(mode.c_str()).c_str());
    ImGui::SameLine();
    if (drawSmallUiButton(uiTextForLocale("Sharing details", "Freigabedetails"), UiButtonRole::Secondary))
        ImGui::OpenPopup("PresentationSharingDetails");
    if (ImGui::BeginPopup("PresentationSharingDetails"))
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(580.0f, ImGui::GetMainViewport()->WorkSize.x * .7f));
        drawStatusActionStrip("PresentationPanelStatus", strip);

        drawInlineLabelValue(
            "Player-safe",
            localizedUiText("Scene summaries, revealed map details, visible NPCs, public handouts, and shared art."));
        drawInlineLabelValue(
            "GM-only hidden",
            localizedUiText("GM notes, secrets, hidden tokens, private stats, and unrevealed story links."));

        bool presentationMode = campaign.presentationModeActive;
        if (FormUi::Checkbox(localizedUiLabel("Presentation-safe GM preview").c_str(), &presentationMode))
            _controller.setPresentationModeActive(presentationMode);
        showHelpTooltip("Hides GM-only prep sections in live-session surfaces where supported.");

        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    auto capturePresentationRestore = [&]() {
        _presentationRestoreSnapshot = display;
        _hasPresentationRestoreSnapshot = true;
    };

    UiGuidance::drawPurpose(UiGuidance::Topic::PlayerDisplay);
    UiGuidance::drawDetails(UiGuidance::Topic::PlayerDisplay);
    const char *presentationPopupToOpen = nullptr;
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (display.enabled)
    {
        if (localPlayerDisplayIsPublished(display))
        {
            if (drawUiButton("Blackout", UiButtonRole::Destructive, ImVec2(160.0f, 0.0f),
                             "Immediately hide all player-facing content. Shortcut: B."))
                _controller.blackoutLocalPlayerDisplayOutput();
            if (_controller.localPlayerDisplayCombatStatsUpdatePending())
            {
                ImGui::SameLine();
                if (drawUiButton(uiTextForLocale("Update shared health & turn", "Geteilte TP und Zug aktualisieren"),
                                 UiButtonRole::Secondary, ImVec2(0.0f, 0.0f),
                                 uiTextForLocale(
                                     "Updates only current/temporary HP and initiative turn/round for the already-reviewed roster. All other player content stays frozen until a new review.",
                                     "Aktualisiert nur aktuelle/temporäre TP und Initiativzug/-runde der bereits geprüften Gruppe. Alle anderen Spielerinhalte bleiben bis zu einer neuen Prüfung eingefroren.")))
                    _controller.updateLocalPlayerDisplayCombatStats();
            }
            ImGui::SameLine();
            if (drawUiButton("Ping camera center", UiButtonRole::Secondary, ImVec2(180.0f, 0.0f),
                             "Show a three-second ping at the saved public camera center."))
                _controller.pingLocalPlayerDisplay();
        }
        else if (drawUiButton("Show to players", localPlayerDisplayHasPublicPayload(campaign),
                              "Choose at least one valid public scene, map, initiative source, NPC, or handout first.",
                              UiButtonRole::Primary, ImVec2(180.0f, 0.0f),
                              "Explicitly publish the reviewed player-safe state."))
        {
            preparePresentationPlayerReview(PresentationReviewAction::Publish);
            presentationPopupToOpen = "Presentation Publish Review";
        }
    }

    if (!display.enabled && drawUiButton(uiTextForLocale("Open player window…", "Spielerfenster öffnen…"),
            localPlayerDisplayHasPublicPayload(campaign),
            "Choose at least one valid public scene, map, initiative source, NPC, or handout first.",
            UiButtonRole::Primary, ImVec2(0.0f, 0.0f)))
    {
        preparePresentationPlayerReview(PresentationReviewAction::Arm);
        presentationPopupToOpen = "Presentation Arm Review";
    }
    if (display.enabled && drawUiButton(uiTextForLocale("Close player window", "Spielerfenster schließen"),
                                       UiButtonRole::Secondary, ImVec2(0.0f, 0.0f)))
    {
        capturePresentationRestore();
        _controller.disableLocalPlayerDisplay();
    }
    if (drawSmallUiButton(uiTextForLocale("More sharing actions", "Weitere Freigabeaktionen"), UiButtonRole::Secondary))
        ImGui::OpenPopup("PresentationMoreActions");
    if (ImGui::BeginPopup("PresentationMoreActions"))
    {
        if (ImGui::MenuItem(uiTextForLocale("Clear selected content…", "Gewählte Inhalte zurücksetzen…")))
            presentationPopupToOpen = "Clear Public State";
        if (ImGui::MenuItem(uiTextForLocale("Restore previous selections", "Vorherige Auswahl wiederherstellen"), nullptr,
                false, _hasPresentationRestoreSnapshot))
        {
            _controller.restoreLocalPlayerDisplayState(_presentationRestoreSnapshot);
            _hasPresentationRestoreSnapshot = false;
        }
        ImGui::EndPopup();
    }

    // Open all review dialogs in the parent scope, after any action popup closes.
    if (presentationPopupToOpen != nullptr)
        ImGui::OpenPopup(localizedUiLabel(presentationPopupToOpen).c_str());

    renderPresentationPlayerReviewModal("Presentation Arm Review");
    renderPresentationPlayerReviewModal("Presentation Publish Review");
    if(_presentationReviewAction!=PresentationReviewAction::None&&
        !ImGui::IsPopupOpen(localizedUiLabel("Presentation Arm Review").c_str())&&
        !ImGui::IsPopupOpen(localizedUiLabel("Presentation Publish Review").c_str()))
    { _playerReviewSession.clear(); _presentationReviewAction=PresentationReviewAction::None; }

    setResponsiveAutoSizeWindow(640.0f, 420.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("Clear Public State").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            localizedUiLabel(
                "Clear the current public scene, map, initiative source, NPC reveals, handouts, and shared art?")
                .c_str());
        drawInlineLabelValue("Scene", storySceneLabel(campaign, display.publicSceneId).c_str());
        drawInlineLabelValue("Map", mapLabel(campaign, display.publicMapId).c_str());
        drawInlineLabelValue("Initiative", encounterLabel(campaign, display.publicEncounterId).c_str());
        const std::string revealSummary = std::to_string(static_cast<int>(npcCount)) + " " + localizedUiLabel("NPCs") +
                                          ", " + std::to_string(static_cast<int>(handoutCount)) + " " +
                                          localizedUiLabel("Handouts");
        drawInlineLabelValue("Reveals", revealSummary.c_str());
        if (drawModalActionButtons("Clear Public State", UiButtonRole::Destructive))
        {
            capturePresentationRestore();
            _controller.clearLocalPlayerDisplayState();
        }
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(uiTextForLocale("Player layout", "Spieleransicht"));
    if (FormUi::RadioButton(uiTextForLocale("Scene, map and journal", "Szene, Karte und Journal"), !display.mapOnly))
        _controller.setLocalPlayerDisplayMapOnly(false);
    if (FormUi::RadioButton(uiTextForLocale("Map only", "Nur Karte"), display.mapOnly))
        _controller.setLocalPlayerDisplayMapOnly(true);
    ImGui::TextWrapped("%s", display.mapOnly
        ? uiTextForLocale("Show the map across the player window. Other content stays hidden.",
            "Zeige die Karte im gesamten Spielerfenster. Andere Inhalte bleiben ausgeblendet.")
        : uiTextForLocale("Share the selected scene and revealed material alongside an optional map.",
            "Zeige die gewählte Szene und freigegebene Inhalte mit einer optionalen Karte."));
    ImGui::SeparatorText(uiTextForLocale("Include in player view", "In der Spieleransicht zeigen"));
    const float presentationWidth = ImGui::GetContentRegionAvail().x;
    const int channelColumns = presentationWidth >= 720.0f ? 3 : (presentationWidth >= 420.0f ? 2 : 1);
    if (ImGui::BeginTable("PresentationChannels", channelColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        bool showMap = display.showMap;
        if (FormUi::Checkbox(localizedUiLabel("Map").c_str(), &showMap))
            _controller.setShowPublicMap(showMap);
        ImGui::TableNextColumn();
        ImGui::BeginDisabled(display.mapOnly);
        bool showInitiative = display.showInitiative;
        if (FormUi::Checkbox(localizedUiLabel("Initiative").c_str(), &showInitiative))
            _controller.setShowPublicInitiative(showInitiative);
        ImGui::TableNextColumn();
        bool showNpcs = display.showVisibleNpcs;
        if (FormUi::Checkbox(localizedUiLabel("NPCs").c_str(), &showNpcs))
            _controller.setShowPublicNpcs(showNpcs);
        ImGui::TableNextColumn();
        bool showHandouts = display.showPublicHandouts;
        if (FormUi::Checkbox(localizedUiLabel("Handouts").c_str(), &showHandouts))
            _controller.setShowPublicHandouts(showHandouts);
        ImGui::TableNextColumn();
        bool showArt = display.showSharedArt;
        if (FormUi::Checkbox(localizedUiLabel("Shared Art").c_str(), &showArt))
            _controller.setShowSharedArt(showArt);
        ImGui::TableNextColumn();
        bool showParty = display.showParty;
        if (FormUi::Checkbox(uiTextForLocale("Party", "Gruppe"), &showParty))
            _controller.setShowPublicParty(showParty);
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    if (!display.mapOnly && display.showParty)
    {
        ImGui::SeparatorText(uiTextForLocale("Choose party members", "Gruppenmitglieder auswählen"));
        ImGui::TextWrapped("%s", uiTextForLocale(
            "Share names, class, level, health, movement and conditions. Notes stay private. Inventory is shared only when enabled below.",
            "Teile Namen, Klasse, Stufe, Trefferpunkte, Bewegung und Zustände. Notizen bleiben privat. Inventar wird nur geteilt, wenn es unten aktiviert ist."));
        if (campaign.characters.empty())
            ImGui::TextWrapped("%s", uiTextForLocale("Create player characters in Cast first.", "Erstelle zuerst Spielfiguren unter Figuren."));
        for (const auto &character : campaign.characters)
        {
            bool shared = playerSafeContainsId(display.visibleCharacterIds, character.metadata.id);
            const std::string label = std::string(character.nameBuffer[0] ? character.nameBuffer : uiTextForLocale("Unnamed character", "Unbenannte Figur")) +
                "##ShareParty" + character.metadata.id;
            if (FormUi::Checkbox(label.c_str(), &shared))
                _controller.setPublicCharacterVisibility(character.metadata.id, shared);
        }
        bool showSlots = display.showPartySpellSlots;
        if (FormUi::Checkbox(uiTextForLocale("Show spell slots for selected party members", "Zauberplätze der gewählten Gruppenmitglieder zeigen"), &showSlots))
            _controller.setShowPublicPartySpellSlots(showSlots);
        showHelpTooltip(uiTextForLocale("Shares remaining and maximum slots by spell level after review. Spell names and notes stay private.",
            "Zeigt nach der Prüfung übrige und maximale Plätze je Zaubergrad. Zaubernamen und Notizen bleiben privat."));
        bool showInventory = display.showPartyInventory;
        if (FormUi::Checkbox(uiTextForLocale("Show inventory for selected party members", "Inventar der gewählten Gruppenmitglieder zeigen"), &showInventory))
            _controller.setShowPublicPartyInventory(showInventory);
        ImGui::TextWrapped("%s", uiTextForLocale(
            "This shares every resolvable item name and quantity carried by selected members, plus Party Inventory. Item descriptions, notes, values, and currency stay private.",
            "Dies teilt jeden auflösbaren Gegenstandsnamen und jede Menge der gewählten Mitglieder sowie des Gruppeninventars. Beschreibungen, Notizen, Werte und Währung bleiben privat."));
    }

    const int selectorColumns = presentationWidth >= 900.0f ? 3 : (presentationWidth >= 600.0f ? 2 : 1);
    if (ImGui::BeginTable("PresentationSelectors", selectorColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo(localizedUiLabel("Public Scene").c_str(),
                               storySceneLabel(campaign, display.publicSceneId).c_str()))
        {
            if (ImGui::Selectable(localizedUiLabel("None").c_str(), display.publicSceneId.empty()))
                _controller.setPublicScene("");
            for (const StoryThreadModel &thread : campaign.storyThreads)
            {
                for (const StoryBeatModel &beat : thread.beats)
                {
                    const std::string label =
                        (thread.title.empty() ? localizedUiLabel("Untitled story") : thread.title) + " / " +
                        (beat.title.empty() ? localizedUiLabel("Untitled scene") : beat.title);
                    if (ImGui::Selectable(label.c_str(), display.publicSceneId == beat.metadata.id))
                        _controller.setPublicScene(beat.metadata.id);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo(localizedUiLabel("Public Map").c_str(), mapLabel(campaign, display.publicMapId).c_str()))
        {
            if (ImGui::Selectable(localizedUiLabel("None").c_str(), display.publicMapId.empty()))
                _controller.setPublicMap("");
            for (const MapModel &map : campaign.maps)
            {
                const std::string label = map.name.empty() ? localizedUiLabel("Untitled map") : map.name;
                if (ImGui::Selectable(label.c_str(), display.publicMapId == map.metadata.id))
                    _controller.setPublicMap(map.metadata.id);
            }
            ImGui::EndCombo();
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo(localizedUiLabel("Initiative Source").c_str(),
                               encounterLabel(campaign, display.publicEncounterId).c_str()))
        {
            if (ImGui::Selectable(localizedUiLabel("None").c_str(), display.publicEncounterId.empty()))
                _controller.setPublicEncounter("");
            for (const EncounterModel &encounter : campaign.encounters)
            {
                const std::string label =
                    encounter.name.empty() ? localizedUiLabel("Untitled encounter") : encounter.name;
                if (ImGui::Selectable(label.c_str(), display.publicEncounterId == encounter.metadata.id))
                    _controller.setPublicEncounter(encounter.metadata.id);
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader(uiTextForLocale("Screen and camera settings", "Bildschirm und Kamera")))
    {
    ImGui::SeparatorText(localizedUiLabel("Display window").c_str());
    const ImGuiPlatformIO &platform = ImGui::GetPlatformIO();
    const int mainMonitor = mainPlatformMonitorIndex();
    const LocalPlayerDisplayWindowPlacement placement =
        resolveLocalPlayerDisplayWindowPlacement(display, platform.Monitors.Size, mainMonitor);
    std::string monitorSelection = localizedUiLabel("Automatic (prefer second display)");
    if (display.preferredMonitorIndex >= 0)
    {
        if (display.preferredMonitorIndex < platform.Monitors.Size)
        {
            const ImGuiPlatformMonitor &monitor = platform.Monitors[display.preferredMonitorIndex];
            monitorSelection =
                formatLocalizedUi("Display %d (%d x %d)", display.preferredMonitorIndex + 1,
                                  static_cast<int>(monitor.MainSize.x), static_cast<int>(monitor.MainSize.y));
        }
        else
        {
            monitorSelection = formatLocalizedUi("Unavailable display %d", display.preferredMonitorIndex + 1);
        }
    }
    ImGui::SetNextItemWidth(std::min(420.0f, ImGui::GetContentRegionAvail().x));
    if (FormUi::BeginCombo(localizedUiLabel("Player display monitor").c_str(), monitorSelection.c_str()))
    {
        if (ImGui::Selectable(localizedUiLabel("Automatic (prefer second display)").c_str(),
                              display.preferredMonitorIndex < 0))
            _controller.setLocalPlayerDisplayMonitor(-1);
        for (int monitorIndex = 0; monitorIndex < platform.Monitors.Size; ++monitorIndex)
        {
            const ImGuiPlatformMonitor &monitor = platform.Monitors[monitorIndex];
            std::string label =
                formatLocalizedUi("Display %d (%d x %d)", monitorIndex + 1, static_cast<int>(monitor.MainSize.x),
                                  static_cast<int>(monitor.MainSize.y));
            if (monitorIndex == mainMonitor)
                label += localizedUiLabel(" (main display)");
            if (ImGui::Selectable(label.c_str(), display.preferredMonitorIndex == monitorIndex))
                _controller.setLocalPlayerDisplayMonitor(monitorIndex);
        }
        ImGui::EndCombo();
