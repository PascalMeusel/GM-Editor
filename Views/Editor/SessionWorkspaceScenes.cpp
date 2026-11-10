#include "Views/Editor/SessionWorkspaceSupport.hpp"
#include "Views/Editor/SessionWorkspaceCommandPopup.hpp"
#include "Models/Project/CampaignEditHistory.hpp"

void SessionWorkspace::drawSequence(Campaign &campaign, const SessionModel &session, const Actions &actions)
{
    const bool readOnly = !session.endedAtUtc.empty();
    const ImGuiInputTextFlags textFlags = readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
    if (ImGui::TreeNode(st("Session details", "Sitzungsdetails")))
    {
        label("Session title", "Sitzungstitel");
        if (FormUi::InputText("##session-title", &_title, textFlags)) changed(_sessionDirty);
        ImGui::TreePop();
    }
    if (readOnly) ImGui::TextWrapped("%s", st("Completed session · browse your prepared material here.", "Abgeschlossene Sitzung · hier kannst du deine vorbereiteten Inhalte nachlesen."));
    ImGui::Spacing();
    ImGui::TextUnformatted(st("Scenes", "Szenen"));
    for (std::size_t i = 0; i < session.planEntries.size(); ++i)
    {
        const auto &entry = session.planEntries[i];
        const auto *scene = SessionPlan::findScene(campaign, entry.storyBeatId);
        const std::string state = entry.state == SessionEntryState::Completed ? "✓ " : entry.state == SessionEntryState::Skipped ? "– " : "";
        const std::string title = std::to_string(i + 1) + ". " + state + (scene ? scene->title : st("Missing scene", "Fehlende Szene"));
        if (ImGui::Selectable((title + "##" + entry.metadata.id).c_str(), !_showRoughNotes && _entryId == entry.metadata.id, 0, ImVec2(0, ImGui::GetFrameHeight())))
            if (flush(campaign, actions.persist)) { load(campaign, _sessionId, entry.metadata.id); _compactPane = 1; }
    }
    ImGui::BeginDisabled(readOnly);
    if (action("+ New scene", "+ Neue Szene")) ImGui::OpenPopup("CreateNamedScene");
    bool createdScene=false;
    const float scenePopupWidth = std::min(420.0f * ImGui::GetFontSize() / 16.0f, ImGui::GetMainViewport()->WorkSize.x - 32.0f);
    ImGui::SetNextWindowSizeConstraints(ImVec2(scenePopupWidth, 0.0f), ImVec2(scenePopupWidth, ImGui::GetMainViewport()->WorkSize.y));
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if(ImGui::BeginPopup("CreateNamedScene"))
    {
        ImGui::TextUnformatted(st("Scene title","Szenentitel"));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputText("##NewSceneTitle",&_newSceneTitle);
        ImGui::BeginDisabled(_newSceneTitle.find_first_not_of(" \t\r\n")==std::string::npos);
        if(action("Add scene","Szene hinzufügen",true) && flush(campaign,actions.persist)) {
            const auto result=SessionPlan::createScene(campaign,_sessionId,_newSceneTitle,"",actions.persist);
            if(report(result)){load(campaign,_sessionId,result.entryId);_newSceneTitle.clear();_compactPane=1;createdScene=true;ImGui::CloseCurrentPopup();}
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (action("Cancel", "Abbrechen")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if(createdScene){ImGui::EndDisabled();return;}
    if (action("Add existing scene", "Vorhandene Szene hinzufügen")) ImGui::OpenPopup("AddExistingScene");
    if (ImGui::BeginPopup("AddExistingScene"))
    {
        const auto threads = campaign.storyThreads;
        for (const auto &thread : threads)
            for (const auto &scene : thread.beats)
                if (ImGui::Selectable((scene.title + "##" + scene.metadata.id).c_str()) && flush(campaign, actions.persist))
                {
                    const auto result = CampaignEditHistory::editPlan(campaign, _sessionId, actions.persist, "Add scene to session", [&] {
                        return SessionPlan::addScene(campaign, _sessionId, scene.metadata.id, actions.persist);
                    });
                    if (report(result)) { load(campaign, _sessionId, result.entryId); _compactPane = 1; }
                }
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
    if (ImGui::Selectable(st("Rough notes", "Ideen und Notizen"), _showRoughNotes, 0, ImVec2(0, ImGui::GetFrameHeight())))
    {
        _showRoughNotes = true;
        _compactPane = 1;
        _resetCompactScroll = true;
    }
}

void SessionWorkspace::drawRoughNotes(Campaign &campaign, const SessionModel &session, const Actions &actions)
{
    const bool readOnly = !session.endedAtUtc.empty();
    const ImGuiInputTextFlags textFlags = readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
    const float unit = ImGui::GetFontSize() / 16.0f;
    const float width = std::min(ImGui::GetContentRegionAvail().x, 1040.0f * unit);
    ImGui::BeginChild("SessionNotesWriting", ImVec2(width, 0));
    ImGui::TextUnformatted(st("Rough notes", "Ideen und Notizen"));
    ImGui::TextWrapped("%s", st("Write what you have in mind. Select a passage, then create a scene from it.",
        "Schreibe auf, was du planst. Markiere einen Abschnitt und erstelle daraus eine Szene."));
    const float height = std::clamp(ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.0f,
                                   200.0f * unit, 540.0f * unit);
    if (FormUi::InputTextMultiline("##rough-notes", &_notes, ImVec2(-1, height), textFlags | ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackAlways, captureSelection, this)) changed(_sessionDirty);
    ImGui::BeginDisabled(readOnly || _selectedNotes.empty());
    if (action("Create scene from selection", "Szene aus Auswahl erstellen", true) && flush(campaign, actions.persist))
    {
        std::string title = PreparationText::sceneTitle(_selectedNotes);
        const auto result = SessionPlan::createScene(campaign, _sessionId, title, _selectedNotes, actions.persist);
        if (report(result)) { load(campaign, _sessionId, result.entryId); _compactPane = 1; }
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
}

void SessionWorkspace::drawScene(Campaign &campaign, const SessionModel &session, const Actions &actions)
{
    const bool readOnly = !session.endedAtUtc.empty();
    const ImGuiInputTextFlags textFlags = readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
    const auto *entry = SessionPlan::findEntry(session, _entryId);
    if (_showRoughNotes || !entry)
    {
        drawRoughNotes(campaign, session, actions);
        return;
    }
    if (!SessionPlan::findScene(campaign, entry->storyBeatId))
    {
        ImGui::TextWrapped("%s", st("This scene is missing. Remove this entry and add an existing scene to repair the plan.", "Diese Szene fehlt. Entferne den Eintrag und füge eine vorhandene Szene hinzu."));
        ImGui::BeginDisabled(readOnly);
        if (action("Remove from session", "Aus Sitzung entfernen"))
            if (report(CampaignEditHistory::editPlan(campaign, _sessionId, actions.persist, "Remove scene from session", [&] {
                return SessionPlan::removeEntry(campaign, _sessionId, _entryId, actions.persist);
            }))) load(campaign, _sessionId);
        ImGui::EndDisabled();
        return;
    }
    label("Scene title", "Szenentitel");
    if (FormUi::InputText("##scene-title", &_scene.title, textFlags)) changed(_sceneDirty);
    ImGui::Spacing();
    if (ImGui::BeginTabBar("SceneContent"))
    {
        if (ImGui::BeginTabItem(st("Notes", "Notizen")))
        {
            label("What happens in this scene?", "Was passiert in dieser Szene?");
            const float notesHeight = std::clamp(ImGui::GetContentRegionAvail().y -
                ImGui::GetFrameHeightWithSpacing() * 3.0f - (_showClues ? 140.0f : 0.0f) -
                (_showConsequences ? 140.0f : 0.0f) - (_showEntryNote ? 120.0f : 0.0f), 170.0f, 420.0f);
            if (FormUi::InputTextMultiline("##scene-summary", &_scene.summary, ImVec2(-1, notesHeight), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_sceneDirty);
            if (_showClues)
            {
                label("Clues", "Hinweise");
                if (FormUi::InputTextMultiline("##scene-clues", &_scene.clues, ImVec2(-1, 90), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_sceneDirty);
            }
            if (_showConsequences)
            {
                label("Consequences", "Folgen");
                if (FormUi::InputTextMultiline("##scene-consequences", &_scene.consequences, ImVec2(-1, 90), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_sceneDirty);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(st("Read aloud", "Vorlesen")))
        {
            ImGui::TextWrapped("%s", st("Player-facing text. It stays private until you publish it from the separate player preview.", "Text für Spieler. Er bleibt privat, bis du ihn über die separate Spielervorschau veröffentlichst."));
            label("Read-aloud text", "Vorlesetext");
            const float editorAvailable = ImGui::GetContentRegionAvail().y;
            const float footerReserve = ImGui::GetFrameHeightWithSpacing() * 6.0f;
            const float playerSummaryHeight = _showPlayerSummary
                ? std::clamp((editorAvailable - footerReserve) * 0.32f, 64.0f, 90.0f)
                : 0.0f;
            const float readAloudHeight = std::clamp(editorAvailable - footerReserve - playerSummaryHeight, 110.0f, 190.0f);
            if (FormUi::InputTextMultiline("##read-aloud", &_scene.readAloudText, ImVec2(-1, readAloudHeight), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_sceneDirty);
            if (_showPlayerSummary)
            {
                label("Player summary", "Zusammenfassung für Spieler");
                if (FormUi::InputTextMultiline("##player-summary", &_scene.playerSummary, ImVec2(-1, playerSummaryHeight), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_sceneDirty);
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(st("GM secrets", "SL-Geheimnisse")))
        {
            ImGui::TextWrapped("%s", st("Private notes stay on the GM screen.", "Private Notizen bleiben auf dem SL-Bildschirm."));
            if (FormUi::InputTextMultiline("##gm-notes", &_scene.gmNotes, ImVec2(-1, 280), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_sceneDirty);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (_showEntryNote)
    {
        label("Note for this session only", "Notiz nur für diese Sitzung");
        if (FormUi::InputTextMultiline("##entry-note", &_entryNote, ImVec2(-1, 70), textFlags | ImGuiInputTextFlags_WordWrap)) changed(_entryDirty);
    }
    ImGui::Spacing();
    if (action("More fields", "Weitere Felder")) ImGui::OpenPopup("SceneOptionalFields");
    showHelpTooltip(st("Show only the fields you need. Hiding a field keeps its text.", "Zeige nur benötigte Felder. Ausblenden erhält den Text."));
    if (ImGui::BeginPopup("SceneOptionalFields"))
    {
        ImGui::MenuItem(st("Clues", "Hinweise"), nullptr, &_showClues);
        ImGui::MenuItem(st("Consequences", "Folgen"), nullptr, &_showConsequences);
        ImGui::MenuItem(st("Player summary (Read aloud)", "Spielerzusammenfassung (Vorlesen)"), nullptr, &_showPlayerSummary);
        ImGui::MenuItem(st("Note for this session only", "Notiz nur für diese Sitzung"), nullptr, &_showEntryNote);
        ImGui::EndPopup();
    }
    sameLineIfFits(st("Arrange scene", "Szene anordnen"));
    if (action("Arrange scene", "Szene anordnen")) ImGui::OpenPopup("ScenePlanActions");
    prepareSessionCommandPopup("ScenePlanActions");
    if (ImGui::BeginPopup("ScenePlanActions"))
    {
    const auto position = std::find_if(session.planEntries.begin(), session.planEntries.end(), [&](const auto &item) { return item.metadata.id == _entryId; });
    ImGui::BeginDisabled(readOnly || position == session.planEntries.begin());
    if (action("Move up", "Nach oben") && flush(campaign, actions.persist))
        report(CampaignEditHistory::editPlan(campaign, _sessionId, actions.persist, "Move session scene up", [&] {
            return SessionPlan::moveEntry(campaign, _sessionId, _entryId, -1, actions.persist);
        }));
    ImGui::EndDisabled();
    ImGui::BeginDisabled(readOnly || position == session.planEntries.end() || std::next(position) == session.planEntries.end());
    if (action("Move down", "Nach unten") && flush(campaign, actions.persist))
        report(CampaignEditHistory::editPlan(campaign, _sessionId, actions.persist, "Move session scene down", [&] {
            return SessionPlan::moveEntry(campaign, _sessionId, _entryId, 1, actions.persist);
        }));
    ImGui::EndDisabled();
    ImGui::BeginDisabled(readOnly);
    if (action("Remove from session", "Aus Sitzung entfernen") && flush(campaign, actions.persist))
    {
        if (report(CampaignEditHistory::editPlan(campaign, _sessionId, actions.persist, "Remove scene from session", [&] {
            return SessionPlan::removeEntry(campaign, _sessionId, _entryId, actions.persist);
        }))) { load(campaign, _sessionId); ImGui::CloseCurrentPopup(); }
    }
    ImGui::EndDisabled();
    if (_error || _lastStatus != SessionPlan::Status::Applied) ImGui::TextWrapped("%s", statusText());
    ImGui::EndPopup();
    }
}
