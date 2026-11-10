// Internal renderWorkspaceLanding continuation: workspace identity, live context, and summaries.
    if (DockableWidget::defaultDockId() != 0)
        ImGui::SetNextWindowDockID(DockableWidget::defaultDockId(), ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    const bool audioWorkspace = _controller._navigation.current().editorRoute == "show:audio_manager";
    const std::string workspaceWindowLabel = std::string(audioWorkspace ? "Audio" : editorWorkspaceLabel(_activeWorkspace)) + "###Workspace";
    if (!ImGui::Begin(workspaceWindowLabel.c_str(), nullptr, flags))
    {
        ImGui::End();
        return;
    }

    const Campaign &campaign = _controller.currentCampaign();
    const EditorWorkspaceDefinition &definition = editorWorkspaceDefinition(_activeWorkspace);
    const ProjectSaveState &saveState = ProjectSaveService::getInstance().state();
    const bool hasPendingChanges = _controller.hasUnsavedChanges();
    const bool retainedDraft = hasPendingChanges && !saveState.dirty;
    const std::string breadcrumb = std::string(campaign.name.empty() ? localizedUiText("Campaign") : campaign.name) +
                                   " / " + editorWorkspaceLabel(definition.workspace);
    const bool germanHeader =
        gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const char *stateLabel = retainedDraft ? (germanHeader ? "Entwurf noch nicht gespeichert" : "Draft not yet saved")
                                           : saveState.dirty ? gm::localization::tr(gm::localization::TextId::CommonUnsaved)
                                                             : gm::localization::tr(gm::localization::TextId::CommonSaved);
    // Embedded editors need their own compact chrome so the working form starts
    // near the top of a 720px window. The controller renders the tool name,
    // save state and its contextual return actions before the editor itself.
    // Keep the fuller browse header below for workspace landing pages.
    if (_activeWorkspace != EditorWorkspace::Live)
    {
        renderHandoutCreation();
        if (_controller.renderEmbeddedWorkspaceTool())
        {
            ImGui::End();
            return;
        }
    }

    if (_activeWorkspace == EditorWorkspace::Live)
    {
        const auto *session = _controller.activeSession();
        ImGui::TextWrapped("%s", session ? session->title.c_str() : breadcrumb.c_str());
        const bool german =
            gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
        if (drawUiButton(german ? "Spieleranzeige prüfen" : "Review player display", UiButtonRole::Primary))
            _controller.showPresentationPanel();
        if (_controller.localPlayerDisplayCombatStatsUpdatePending())
        {
            ImGui::SameLine();
            if (drawUiButton(german ? "Geteilte TP und Zug aktualisieren" : "Update shared health & turn",
                             UiButtonRole::Secondary,
                             ImVec2(0.0f, 0.0f),
                             german ? "Aktualisiert nur aktuelle/temporäre TP und Zug/Runde der bereits geprüften Gruppe. Neue Namen, Zustände oder Inhalte bleiben gesperrt."
                                    : "Updates only current/temporary HP and turn/round for the already-reviewed roster. New names, conditions, or content remain blocked."))
                _controller.updateLocalPlayerDisplayCombatStats();
        }
        ImGui::SameLine();
        if (drawUiButton(german ? "Schwarzbild" : "Blackout", campaign.localPlayerDisplay.enabled,
                         german ? "Das Spielerfenster ist geschlossen." : "The player window is closed.", UiButtonRole::Secondary))
            _controller.blackoutLocalPlayerDisplayOutput();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", stateLabel);
        ImGui::TextUnformatted(!campaign.localPlayerDisplay.enabled
            ? (german ? "Spielerfenster geschlossen" : "Player window closed")
            : (localPlayerDisplayIsPublished(campaign.localPlayerDisplay)
                ? (german ? "Spielerfenster offen · Inhalte sichtbar" : "Player window open · showing content")
                : (german ? "Spielerfenster offen · Schwarzbild" : "Player window open · black screen")));
        if (campaign.localPlayerDisplay.enabled)
        {
            ImGui::SameLine();
            if (drawSmallUiButton(german ? "Spielerfenster schließen" : "Close player window", UiButtonRole::Secondary))
                _controller.disableLocalPlayerDisplay();
        }
    }
    else
    {
        pushBoldFont();
        ImGui::TextWrapped("%s", editorWorkspaceLabel(definition.workspace));
        popBoldFont();
        showHelpTooltip(editorWorkspaceDescription(definition.workspace));
        const bool stableWorldHeader=_activeWorkspace==EditorWorkspace::World;
        if(stableWorldHeader) {
            const char *states[]={germanHeader?"Entwurf noch nicht gespeichert":"Draft not yet saved",
                gm::localization::tr(gm::localization::TextId::CommonUnsaved),gm::localization::tr(gm::localization::TextId::CommonSaved)};
            float width=0;for(const char *state:states)width=std::max(width,ImGui::CalcTextSize(state).x);
            TaskUi::sameLineIfFits(width);
            width=std::max(1.0f,std::min(width,ImGui::GetContentRegionAvail().x));
            float height=0;for(const char *state:states)height=std::max(height,ImGui::CalcTextSize(state,nullptr,false,width).y);
            ImGui::BeginGroup();const auto start=ImGui::GetCursorPos();
            ImGui::PushStyleColor(ImGuiCol_Text,hasPendingChanges?WorkspaceUi::Amber:WorkspaceUi::Teal);
            ImGui::PushTextWrapPos(start.x+width);ImGui::TextUnformatted(stateLabel);ImGui::PopTextWrapPos();ImGui::PopStyleColor();
            ImGui::SetCursorPos(ImVec2(start.x,start.y+height));ImGui::Dummy(ImVec2(width,0));ImGui::EndGroup();
        } else {
            TaskUi::sameLineIfFits(ImGui::CalcTextSize(stateLabel).x);
            ImGui::TextColored(hasPendingChanges ? WorkspaceUi::Amber : WorkspaceUi::Teal, "%s", stateLabel);
        }
        TaskUi::sameLineForButton(germanHeader ? "Spieleranzeige prüfen" : "Review player display");
        if (drawUiButton(germanHeader ? "Spieleranzeige prüfen" : "Review player display", UiButtonRole::Secondary,
                         ImVec2(170.0f, 0.0f),
                         germanHeader ? "Prüfe die spielersichere Ausgabe, bevor du sie für die Tischanzeige veröffentlichst." :
                             "Review the player-safe output before publishing it to the table display."))
            _controller.showPresentationPanel();
        if (hasPendingChanges || stableWorldHeader)
        {
            TaskUi::sameLineForButton(germanHeader ? "Änderungen speichern" : "Save changes");
            if (drawUiButton(germanHeader ? "Änderungen speichern" : "Save changes",hasPendingChanges,
                             germanHeader?"Keine ausstehenden Änderungen zum Speichern.":"There are no pending changes to save.",
                             UiButtonRole::Primary, ImVec2(170.0f, 0.0f),
                             "Save all retained campaign and editor drafts."))
                _controller.saveCurrentCampaign("workspace header save");
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
    }

    // Live retains its established header. It normally has no embedded editor,
    // but retain the existing route behavior if a restored navigation state has
    // one.
    if (_activeWorkspace == EditorWorkspace::Live)
    {
        renderHandoutCreation();
        if (_controller.renderEmbeddedWorkspaceTool())
        {
            ImGui::End();
            return;
        }
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const int columnCount = availableWidth >= 920.0f ? 3 : (availableWidth >= 600.0f ? 2 : 1);

    if (_activeWorkspace == EditorWorkspace::Live)
    {
        const char *kPhases[] = {gm::localization::tr(gm::localization::TextId::LiveSetup),
                                 gm::localization::tr(gm::localization::TextId::LiveScene),
                                 gm::localization::tr(gm::localization::TextId::LiveExploration),
                                 gm::localization::tr(gm::localization::TextId::LiveCombat),
                                 gm::localization::tr(gm::localization::TextId::LiveBreak),
                                 gm::localization::tr(gm::localization::TextId::LiveWrapUp)};
        static constexpr LiveSessionDashboardPhase kLivePhases[] = {LiveSessionDashboardPhase::Prep,
                                                                    LiveSessionDashboardPhase::RunningScene,
                                                                    LiveSessionDashboardPhase::ExplorationTravel,
                                                                    LiveSessionDashboardPhase::Combat,
                                                                    LiveSessionDashboardPhase::Break,
                                                                    LiveSessionDashboardPhase::WrapUp};
        const LiveSessionDashboardPhase selectedPhase = _controller.liveSessionPhase();
        int selectedPhaseIndex = 0;
        for (int index = 0; index < 6; ++index)
        {
            if (kLivePhases[index] == selectedPhase)
                selectedPhaseIndex = index;
        }
        // Keep the phase selector on the action row when there is room. On a
        // laptop-height window this gives the active tool another full row.
        const float remainingHeaderWidth = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x -
            ImGui::GetItemRectMax().x - ImGui::GetStyle().ItemSpacing.x;
        if (remainingHeaderWidth >= 240.0f) ImGui::SameLine();
        ImGui::SetNextItemWidth(std::min(240.0f, ImGui::GetContentRegionAvail().x));
        if (FormUi::BeginCombo("##LivePhaseFocus", kPhases[selectedPhaseIndex]))
        {
            for (int index = 0; index < 6; ++index)
            {
                if (ImGui::Selectable(kPhases[index], selectedPhaseIndex == index))
                    _controller.setLiveSessionPhase(kLivePhases[index]);
                if (selectedPhaseIndex == index)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        showHelpTooltip(uiTextForLocale("Choose which part of the session to focus on.",
                                        "Wähle den Bereich der Sitzung, auf den du dich konzentrierst."));
        _controller.renderLiveWorkspace();
        ImGui::End();
        return;
    }

#include "Views/Editor/EditorWorkspaceLandingDirectoryEntries.hpp"
