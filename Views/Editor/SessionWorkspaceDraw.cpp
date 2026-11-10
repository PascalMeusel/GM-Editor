#include "Views/Editor/SessionWorkspaceSupport.hpp"

void SessionWorkspace::draw(Campaign &campaign, const Actions &actions)
{
    _outcomes.tickDrafts(campaign);
    if (campaign.presentationModeActive)
    {
        checkpointRecovery(campaign, true);
        ImGui::TextWrapped("%s", st("Private session preparation is hidden. Return to GM mode in Live to continue. Drafts are retained.",
            "Die private Sitzungsvorbereitung ist ausgeblendet. Wechsle in Live zurück zur SL-Ansicht, um fortzufahren. Entwürfe bleiben erhalten."));
        return;
    }
    if (_campaignId != campaign.metadata.id)
    {
        *this = SessionWorkspace{};
        _campaignId = campaign.metadata.id;
    }
    if (drawRecovery(campaign)) return;
    refreshRecoveryBase(campaign);
    checkpointRecovery(campaign);
    const auto archived = [](const EntityMetadata &metadata) {
        return std::find(metadata.tags.begin(), metadata.tags.end(), "archived") != metadata.tags.end();
    };
    const auto *selectedSession = SessionPlan::findSession(campaign, _sessionId);
    if (!_sessionId.empty() && (!selectedSession || archived(selectedSession->metadata)))
        _selectionUnavailable = true;
    if (selectedSession && !_entryId.empty() && !SessionPlan::findEntry(*selectedSession, _entryId))
        _selectionUnavailable = true;
    if (!_sceneId.empty())
    {
        const auto *selectedScene = SessionPlan::findScene(campaign, _sceneId);
        if (!selectedScene || !gm::ux::resolveNavigationEntity(campaign, _sceneId).available) _selectionUnavailable = true;
    }
    if (!_selectionUnavailable && !selectedSession)
        if (const auto *next = SessionPlan::nextSession(campaign)) load(campaign, next->metadata.id);
    const auto *current = _selectionUnavailable ? nullptr : SessionPlan::findSession(campaign, _sessionId);
    ImGui::PushStyleColor(ImGuiCol_Text, WorkspaceUi::Muted);
    ImGui::TextUnformatted(campaign.name.c_str());
    ImGui::PopStyleColor();
    if (!current)
    {
        if (_selectionUnavailable)
        {
            ImGui::TextWrapped("%s", st("The selected session or scene is unavailable. Your draft has been retained.",
                "Die gewählte Sitzung oder Szene ist nicht mehr verfügbar. Dein Entwurf bleibt erhalten."));
            if (dirty())
            {
                if (action("Copy retained draft", "Erhaltenen Entwurf kopieren"))
                {
                    const std::string text = _title + "\n\n" + _notes + "\n\n" + _scene.title + "\n\n" + _scene.summary +
                        "\n\n" + _scene.clues + "\n\n" + _scene.consequences + "\n\n" + _scene.readAloudText +
                        "\n\n" + _scene.playerSummary + "\n\n" + _scene.gmNotes + "\n\n" + _entryNote;
                    ImGui::SetClipboardText(text.c_str());
                }
                if (action("Discard unavailable draft", "Nicht mehr zugeordneten Entwurf verwerfen"))
                {
                    _sessionDirty = _sceneDirty = _entryDirty = false;
                    _error = _draftConflict = false;
                }
            }
            if (!dirty() && FormUi::BeginCombo("##recover-session-selection", st("Choose a session", "Sitzung wählen")))
            {
                EntityId selectedId;
                for (const auto &candidate : campaign.sessions)
                    if (!archived(candidate.metadata) && ImGui::Selectable((candidate.title + "##" + candidate.metadata.id).c_str()))
                        selectedId = candidate.metadata.id;
                ImGui::EndCombo();
                if (!selectedId.empty()) load(campaign, selectedId);
            }
        }
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextUnformatted(st("Prepare your next session", "Bereite deine nächste Sitzung vor"));
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextWrapped("%s", st("Collect your notes, arrange scenes and link the people, places and encounters you need at the table.",
                                  "Sammle Notizen, ordne Szenen und verknüpfe Figuren, Orte und Begegnungen für den Spieltisch."));
        ImGui::BeginDisabled(dirty());
        if (action("Create a session", "Sitzung vorbereiten", true))
        {
            const auto result = SessionPlan::createSession(campaign, st("Next session", "Nächste Sitzung"), actions.persist);
            if (report(result)) load(campaign, result.sessionId);
        }
        ImGui::EndDisabled();
        if (!dirty() && std::any_of(campaign.sessions.begin(), campaign.sessions.end(),
                                   [&](const auto &session) { return !archived(session.metadata); }))
        {
            ImGui::Spacing();
            ImGui::TextUnformatted(st("Session history", "Vergangene Sitzungen"));
            ImGui::TextWrapped("%s", st("Completed sessions keep their scenes and notes. Choose one to review it.",
                "Abgeschlossene Sitzungen behalten ihre Szenen und Notizen. Wähle eine zur Ansicht."));
            ImGui::SetNextItemWidth(std::min(520.0f, ImGui::GetContentRegionAvail().x));
            if (FormUi::BeginCombo("##session-history", st("Review a previous session", "Vergangene Sitzung ansehen")))
            {
                EntityId selectedId;
                for (const auto &candidate : campaign.sessions)
                    if (!archived(candidate.metadata) && ImGui::Selectable((candidate.title + "##" + candidate.metadata.id).c_str()))
                        selectedId = candidate.metadata.id;
                ImGui::EndCombo();
                if (!selectedId.empty()) load(campaign, selectedId);
            }
        }
        if (_error) ImGui::TextWrapped("%s", statusText());
        return;
    }
    // Snapshot: service calls can replace the campaign and invalidate its vectors.
    const SessionModel session = *current;
    const auto *activeSession = SessionControl::activeSession(campaign);
    const bool anotherSessionActive = activeSession && activeSession->metadata.id != session.metadata.id;
    const std::string activeSessionTitle = anotherSessionActive ? activeSession->title : std::string{};
    if (!_sessionDirty) { _title = session.title; _notes = session.roughNotes; _scheduled = session.scheduledAtUtc; }
    if (!_sceneDirty) if (const auto *scene = SessionPlan::findScene(campaign, _sceneId)) _scene = _sceneBase = *scene;
    bool selectedAnotherSession = false;
    ImGui::SetNextItemWidth(std::min(380.0f, ImGui::GetContentRegionAvail().x));
    if (FormUi::BeginCombo("##session-picker", session.title.c_str()))
    {
        const auto sessions = campaign.sessions;
        for (const auto &other : sessions)
            if (!archived(other.metadata) && ImGui::Selectable((other.title + "##" + other.metadata.id).c_str(), other.metadata.id == _sessionId))
                if (flush(campaign, actions.persist)) { load(campaign, other.metadata.id); selectedAnotherSession = true; }
        ImGui::EndCombo();
    }
    if (selectedAnotherSession) return;
    sameLineIfFits(st("New session", "Neue Sitzung"));
    if (action("New session", "Neue Sitzung")) ImGui::OpenPopup("CreateNamedSession");
    bool createdSession=false;
    const float popupWidth = std::min(420.0f * (ImGui::GetFontSize() / 16.0f), ImGui::GetMainViewport()->WorkSize.x - 32.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(popupWidth, 0.0f), ImVec2(popupWidth, ImGui::GetMainViewport()->WorkSize.y - 32.0f));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopup("CreateNamedSession"))
    {
        ImGui::TextUnformatted(st("Session title","Sitzungstitel"));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputText("##NewSessionTitle",&_newSessionTitle);
        ImGui::BeginDisabled(_newSessionTitle.find_first_not_of(" \t\r\n")==std::string::npos);
        if(action("Create session","Sitzung erstellen",true) && flush(campaign,actions.persist)) {
            const auto result=SessionPlan::createSession(campaign,_newSessionTitle,actions.persist);
            if(report(result)){load(campaign,result.sessionId);_newSessionTitle.clear();createdSession=true;ImGui::CloseCurrentPopup();}
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (action("Cancel", "Abbrechen")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(createdSession)return;
    _outcomes.drawLauncher(campaign, session.metadata.id, actions.persist, _sceneId, actions.prepareTarget);
    sameLineIfFits(anotherSessionActive ? st("Resume active session", "Laufende Sitzung fortsetzen")
        : session.liveModeActive ? st("Resume live", "Live fortsetzen") : st("Run session", "Sitzung starten"));
    ImGui::BeginDisabled(!anotherSessionActive && !session.endedAtUtc.empty());
    if (action(anotherSessionActive ? "Resume active session" : session.liveModeActive ? "Resume live" : "Run session",
               anotherSessionActive ? "Laufende Sitzung fortsetzen" : session.liveModeActive ? "Live fortsetzen" : "Sitzung starten", true)
        && flush(campaign, actions.persist))
    {
        // Opening Live preserves this prepared session and its selected scene.
        // Starting another session remains unavailable until Live is finished.
        bool ready = anotherSessionActive || session.liveModeActive;
        if (!ready)
        {
            const auto result = SessionPlan::startWithEntry(campaign, _sessionId, _entryId, actions.persist);
            ready = report(result);
            if (ready && actions.started) actions.started(result.sessionId, result.sceneId);
        }
        if (ready && actions.run) actions.run();
    }
    ImGui::EndDisabled();
    if (dirty())
    {
        sameLineIfFits(_error ? st("Retry save", "Erneut speichern") : st("Save", "Speichern"));
        if (action(_error ? "Retry save" : "Save", _error ? "Erneut speichern" : "Speichern")) flush(campaign, actions.persist);
    }
    else if (!_error)
    {
        sameLineIfFits(st("Saved", "Gespeichert"));
        ImGui::TextDisabled("%s", st("Saved", "Gespeichert"));
    }
    if (anotherSessionActive)
        ImGui::TextWrapped("%s", (std::string(st("Already running: ", "Bereits aktiv: ")) + activeSessionTitle +
            st(". Finish it in Live before starting another session. This plan stays selected here.",
               ". Beende diese Sitzung in Live, bevor du eine weitere startest. Dieser Plan bleibt hier ausgewählt.")).c_str());
    if (_error || _lastStatus != SessionPlan::Status::Applied)
        ImGui::TextWrapped("%s", statusText());
    if (_draftConflict)
    {
        if (action("Copy scene draft", "Szenenentwurf kopieren"))
        {
            const std::string text = _scene.title + "\n\n" + _scene.summary + "\n\n" + _scene.clues +
                "\n\n" + _scene.consequences + "\n\n" + _scene.readAloudText + "\n\n" + _scene.playerSummary + "\n\n" + _scene.gmNotes;
            ImGui::SetClipboardText(text.c_str());
        }
        if (action("Discard scene draft and reload", "Szenenentwurf verwerfen und neu laden"))
            if (const auto *saved = SessionPlan::findScene(campaign, _sceneId))
            {
                _scene = _sceneBase = *saved;
                _sceneDirty = _draftConflict = _error = false;
                _lastStatus = SessionPlan::Status::Applied;
            }
    }
    ImGui::Separator();

    const float width = ImGui::GetContentRegionAvail().x;
    const float unit = ImGui::GetFontSize() / 16.0f;
    const bool notesLayout = _showRoughNotes;
    if (width < 980.0f * unit)
    {
        // At compact widths these are navigation tabs, not column headings.
        // Keep the selected index so switching panes retains each child pane's
        // independent scroll position and keyboard focus behavior.
        const char *panes[] = {st("Plan & notes", "Plan & Notizen"), st("Scene", "Szene"), st("Linked material", "Verknüpfungen")};
        if (ImGui::BeginTabBar("SessionCompactPanes"))
        {
            const int requestedPane = _compactPane;
            const bool programmaticSelection = requestedPane != _lastCompactPane;
            for (int i = 0; i < 3; ++i)
            {
                const ImGuiTabItemFlags flags = programmaticSelection && requestedPane == i
                    ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(panes[i], nullptr, flags))
                {
                    _compactPane = i;
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        // Each compact pane owns its scroll state. A newly selected pane or
        // scene starts at its title instead of inheriting the notes' scroll.
        const char *paneIds[] = {"SessionSequencePane", "SessionScenePane", "SessionLinksPane"};
        if (_resetCompactScroll || _lastCompactPane != _compactPane)
            ImGui::SetNextWindowScroll(ImVec2(0, 0));
        _resetCompactScroll = false;
        _lastCompactPane = _compactPane;
        ImGui::BeginChild(paneIds[_compactPane], ImVec2(0, 0));
        if (_compactPane == 0) drawSequence(campaign, session, actions);
        else if (_compactPane == 1) drawScene(campaign, session, actions);
        else drawLinks(campaign, actions);
        ImGui::EndChild();
    }
    else if (ImGui::BeginTable(notesLayout ? "SessionNotesPreparation" : "SessionPreparation",
                              notesLayout ? 2 : 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        _lastCompactPane = -1;
        ImGui::TableSetupColumn("sequence", ImGuiTableColumnFlags_WidthFixed, 260 * unit);
        ImGui::TableSetupColumn("scene", ImGuiTableColumnFlags_WidthStretch);
        if (!notesLayout) ImGui::TableSetupColumn("links", ImGuiTableColumnFlags_WidthFixed, 245 * unit);
        ImGui::TableNextColumn(); ImGui::BeginChild("SequencePane", ImVec2(0, 0)); drawSequence(campaign, session, actions); ImGui::EndChild();
        ImGui::TableNextColumn(); ImGui::BeginChild("ScenePane", ImVec2(0, 0)); drawScene(campaign, session, actions); ImGui::EndChild();
        if (!notesLayout)
        {
            ImGui::TableNextColumn(); ImGui::BeginChild("LinksPane", ImVec2(0, 0)); drawLinks(campaign, actions); ImGui::EndChild();
        }
        ImGui::EndTable();
    }
    if (dirty() && !_error && !_recoveryReview && ImGui::GetTime() - _lastEdit >= 10.0) flush(campaign, actions.persist);
    checkpointRecovery(campaign);
}
