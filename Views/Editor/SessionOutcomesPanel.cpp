#include "Views/Editor/SessionOutcomesPanelSupport.hpp"
#include "Views/Editor/Widgets/UiPopupPlacement.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Models/Project/ApplicationPaths.hpp"

bool SessionOutcomesPanel::report(const SessionOutcomes::Result &result)
{
    _message = result.success ? ot("Saved", "Gespeichert") : result.error;
    return result.success;
}
void SessionOutcomesPanel::drawLauncher(Campaign &campaign, EntityId id,
                                       const SessionPlan::Persist &persist, EntityId scene, const PreparationTargetSink &sink)
{
    if (_drawnSession != id) clearPreparationTarget();
    tickDrafts(campaign);
    if (campaign.presentationModeActive)
    {
        ImGui::TextWrapped("%s", ot("Private outcomes are hidden. Return to GM mode in Live to edit them. Drafts are retained.",
            "Private Ergebnisse sind ausgeblendet. Wechsle in Live zurück zur SL-Ansicht, um sie zu bearbeiten. Entwürfe bleiben erhalten."));
        return;
    }
    if (ob("Outcomes and next session", "Ergebnisse und nächste Sitzung")) ImGui::OpenPopup("SessionOutcomes");
    if (!_preparationNavigationError.empty()) ImGui::TextWrapped("%s", _preparationNavigationError.c_str());
    PopupUi::prepareFixed("SessionOutcomes", ImVec2(820.0f, 700.0f));
    if (ImGui::BeginPopup("SessionOutcomes"))
    {
        _preparationOpenEmitted = false;
        const char *closeLabel = ot("Return to session", "Zurück zur Sitzung");
        const auto &style = ImGui::GetStyle();
        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float footerHeight = ImGui::CalcTextSize(closeLabel, nullptr, false,
            std::max(1.0f, width - style.FramePadding.x * 2)).y + style.FramePadding.y * 2;
        const float bodyHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y - footerHeight - style.ItemSpacing.y);
        if (ImGui::BeginChild("SessionOutcomesBody", ImVec2(0, bodyHeight), ImGuiChildFlags_NavFlattened))
            draw(campaign, id, persist, scene, sink);
        ImGui::EndChild();
        _launcherWasOpen = true;
        // Only close this launcher after its body has unwound. The sink queues
        // navigation; it does not mutate the workspace during this draw.
        if (_preparationOpenEmitted)
        { _preparationOpenEmitted = false; _launcherWasOpen = false; ImGui::CloseCurrentPopup(); }
        else if (drawUiButton(closeLabel, UiButtonRole::Secondary, ImVec2(-1, footerHeight)))
        { checkpointDrafts(campaign); _launcherWasOpen = false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    else if (_launcherWasOpen) { checkpointDrafts(campaign); _launcherWasOpen = false; }
}
void SessionOutcomesPanel::draw(Campaign &campaign, EntityId sessionId,
                               const SessionPlan::Persist &persist, EntityId sceneId, const PreparationTargetSink &sink)
{
    tickDrafts(campaign);
    if (campaign.presentationModeActive)
    {
        ImGui::TextWrapped("%s", ot("Private outcomes are hidden. Return to GM mode in Live to edit them. Drafts are retained.",
            "Private Ergebnisse sind ausgeblendet. Wechsle in Live zurück zur SL-Ansicht, um sie zu bearbeiten. Entwürfe bleiben erhalten."));
        return;
    }
    if (_campaignId != campaign.metadata.id) { *this = SessionOutcomesPanel(_recoverySurface); _campaignId = campaign.metadata.id; }
    if (_drawnSession != sessionId)
    { checkpointDrafts(campaign); clearPreparationTarget(); _drawnSession = sessionId; _activeTab = -1; }
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (_windowWasFocused && !focused) checkpointDrafts(campaign);
    _windowWasFocused = focused;
    const auto *source = SessionPlan::findSession(campaign, sessionId);
    if (!source) { clearPreparationTarget(); ImGui::TextUnformatted(ot("Choose a session to record or review outcomes.", "Wähle eine Sitzung, um Ergebnisse festzuhalten oder zu prüfen.")); return; }
    const auto session = *source;
    const auto read = SessionOutcomes::read(session);
    if (!read.success) { ImGui::TextWrapped("%s", read.error.c_str()); return; }
    ImGui::TextWrapped("%s", session.title.c_str());
    if (!_message.empty()) ImGui::TextWrapped("%s", _message.c_str());
    if (ImGui::BeginTabBar("OutcomeWork"))
    {
        const auto guidance = [&](const char *en, const char *de) {
            ImGui::TextWrapped("%s", ot(en, de));
            UiGuidance::drawDetails(UiGuidance::Topic::SessionOutcomes);
        };
        const auto selectTab = [&](int tab) {
            if (_activeTab >= 0 && _activeTab != tab) checkpointDrafts(campaign);
            _activeTab = tab;
        };
        if (ImGui::BeginTabItem(ot("Record", "Festhalten")))
        { selectTab(0); guidance("Record what happened privately. Proposed campaign changes need a separate review before applying.",
            "Halte privat fest, was geschehen ist. Vorgeschlagene Kampagnenänderungen werden vor der Übernahme separat geprüft.");
          composer(campaign, session, persist, sceneId); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(ot("Review", "Prüfen")))
        { selectTab(1); guidance("Compare saved and proposed values, then apply the selected changes together.",
            "Vergleiche gespeicherte und vorgeschlagene Werte und übernimm dann die gewählten Änderungen gemeinsam.");
          review(campaign, session, read.ledger, persist); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(ot("Next session", "Nächste Sitzung")))
        { selectTab(2); guidance("Copy selected follow-up text and scene links into an unstarted session. This does not start it or apply campaign changes.",
            "Übernimm gewählte Folgetexte und Szenenverknüpfungen in eine noch nicht gestartete Sitzung. Sie wird dadurch nicht gestartet; Kampagnenänderungen werden nicht übernommen.");
          carry(campaign, session, read.ledger, persist, sink); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem(ot("Player recap", "Spieler-Rückblick")))
        { selectTab(3); UiGuidance::drawDetails(UiGuidance::Topic::SessionOutcomes);
          recap(campaign, session, read.ledger, persist); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    tickDrafts(campaign);
}
void SessionOutcomesPanel::checkpoint(Campaign &campaign, const EntityId &id, Draft &draft, bool force)
{
    if (draft.recoveryPending || (!draft.changed && !draft.cleanupPending) || !SessionOutcomeDraftState::checkpointDue(ImGui::GetTime(), draft.lastCheckpoint, force)) return;
    if (draft.cleanupPending)
    {
        draft.lastCheckpoint = ImGui::GetTime();
        draft.error = TextDraftRecovery::remove(campaign, outcomeRecoveryKey(id));
        if (draft.error.empty()) draft.cleanupPending = false;
        return;
    }
    try
    {
        SessionOutcomes::Ledger ledger; ledger.outcomes.push_back(draft.outcome);
        draft.error = TextDraftRecovery::save(campaign, outcomeRecoveryKey(id), {{"session", id}, {"ledger", SessionOutcomes::serialize(ledger)}});
    }
    catch (const std::exception &error) { draft.error = error.what(); }
    draft.lastCheckpoint = ImGui::GetTime();
    if (draft.error.empty()) draft.changed = false;
}
bool SessionOutcomesPanel::checkpointDrafts(Campaign &campaign)
{ pollDrafts(campaign, true); return !hasPendingDraftWrites(); }
bool SessionOutcomesPanel::hasPendingDraftWrites() const
{
    for (const auto &[id, draft] : _drafts)
        if (!draft.recoveryPending && (draft.changed || draft.cleanupPending)) return true;
    for (const auto &[id, draft] : _recapDrafts)
        if (draft.loaded && !draft.pending && recapNeedsCheckpoint(draft)) return true;
    return false;
}
void SessionOutcomesPanel::tickDrafts(Campaign &campaign)
{
    if (campaign.presentationModeActive || (_preparationTarget &&
        (_preparationTarget->campaignId != campaign.metadata.id || _preparationRoot != ApplicationPaths::writableRoot())))
        clearPreparationTarget();
    const bool presentationBoundary = campaign.presentationModeActive && !_presentationWasActive;
    _presentationWasActive = campaign.presentationModeActive;
    pollDrafts(campaign, presentationBoundary);
}
void SessionOutcomesPanel::pollDrafts(Campaign &campaign, bool force)
{
    if (_campaignId != campaign.metadata.id) return;
    for (auto &[id, draft] : _drafts) checkpoint(campaign, id, draft, force);
    for (const auto &[id, draft] : _recapDrafts)
        checkpointRecap(campaign, id, force);
}
void SessionOutcomesPanel::composer(Campaign &campaign, const SessionModel &session,
                                   const SessionPlan::Persist &persist, const EntityId &sceneId)
{
    using namespace SessionOutcomes;
    auto &draft = _drafts[session.metadata.id];
    if (!draft.loaded)
    {
        draft.loaded = true;
        const auto saved = TextDraftRecovery::read(campaign, outcomeRecoveryKey(session.metadata.id));
        if (!saved.success) { draft.error = saved.error; draft.recoveryPending = true; }
        else if (saved.found)
        {
            const auto it = saved.fields.find("ledger");
            const auto decoded = it == saved.fields.end() ? ReadResult{} : deserialize(it->second);
            draft.recoveryPending = true;
            const auto identity = saved.fields.find("session");
            if (decoded.success && decoded.ledger.outcomes.size() == 1 && identity != saved.fields.end() && identity->second == session.metadata.id)
                draft.recoveredOutcome = decoded.ledger.outcomes.front();
            else draft.error = ot("Recovery draft could not be read; its file was retained.", "Der Entwurf konnte nicht gelesen werden; seine Datei bleibt erhalten.");
        }
    }
    if (draft.recoveryPending)
    {
        ImGui::TextWrapped("%s", ot("An unfinished outcome draft was found. Restore it for review or discard its checkpoint.",
            "Ein unfertiger Ergebnisentwurf wurde gefunden. Stelle ihn zur Prüfung wieder her oder verwerfe seine Sicherung."));
        if (!draft.error.empty()) ImGui::TextWrapped("%s", draft.error.c_str());
        ImGui::BeginDisabled(!draft.recoveredOutcome);
        if (ob("Restore outcome draft", "Ergebnisentwurf wiederherstellen"))
        { draft.outcome = *draft.recoveredOutcome; draft.recovered = true; draft.sourcePinned = true; draft.recoveryPending = false; draft.error.clear(); }
        if (ob("Copy recovered outcome", "Wiederhergestelltes Ergebnis kopieren"))
            ImGui::SetClipboardText((draft.recoveredOutcome->privateText + "\n\n" + draft.recoveredOutcome->publicText + "\n\n" + draft.recoveredOutcome->changeText).c_str());
        ImGui::EndDisabled();
        if (ob("Discard recovered outcome", "Wiederhergestelltes Ergebnis verwerfen"))
        {
            draft.error = TextDraftRecovery::remove(campaign, outcomeRecoveryKey(session.metadata.id));
            if (draft.error.empty()) { draft.recoveryPending = false; draft.recoveredOutcome.reset(); }
        }
        return;
    }
    auto &outcome = draft.outcome;
    if (draft.recovered)
        ImGui::TextWrapped("%s", ot("Recovered unfinished draft. Review it before recording.", "Unfertiger Entwurf wiederhergestellt. Vor dem Speichern prüfen."));
    if (!draft.error.empty()) ImGui::TextWrapped("%s", draft.error.c_str());
    if (!draft.sourcePinned)
    {
        const auto context = gm::ux::resolveNavigationEntity(campaign, sceneId);
        const bool validContext = context.type == "scene" && context.available &&
            std::any_of(session.planEntries.begin(), session.planEntries.end(), [&](const auto &e) { return e.storyBeatId == sceneId; });
        outcome.sourceSceneId = validContext ? sceneId : EntityId{};
    }
    ol("What happened? (GM only)", "Was ist passiert? (nur SL)");
    draft.changed |= FormUi::InputTextMultiline("##outcome-private", &outcome.privateText, ImVec2(-1, 95), ImGuiInputTextFlags_WordWrap);
    int kind = static_cast<int>(outcome.kind);
    const char *kinds[] = {ot("Decision", "Entscheidung"), ot("Consequence", "Folge"), ot("Open hook", "Offener Ansatz")};
    if (ImGui::Combo(ot("Type", "Art"), &kind, kinds, 3)) { outcome.kind = static_cast<Kind>(kind); draft.changed = true; }
    ol("Linked entry", "Verknüpfter Eintrag");
    if (FormUi::BeginCombo("##outcome-target", targetLabel(campaign, outcome.target).c_str()))
    {
        auto item = [&](TargetType type, const EntityId &id, const std::string &name) {
            if (ImGui::Selectable((name + "##" + id).c_str(), outcome.target.id == id && outcome.target.type == type))
            { outcome.target = {type, id}; outcome.action = Action::RecordOnly; outcome.desiredStatus = 0; draft.changed = true; }
        };
        item(TargetType::None, {}, ot("Session only", "Nur Sitzung"));
        for (const auto &v : campaign.npcs) item(TargetType::Npc, v.metadata.id, v.name);
        for (const auto &v : campaign.worldPlaces) item(TargetType::Place, v.metadata.id, v.name);
        for (const auto &v : campaign.storyObjectives) item(TargetType::Objective, v.metadata.id, v.title);
        for (const auto &v : campaign.storyClues) item(TargetType::Clue, v.metadata.id, v.title);
        ImGui::EndCombo();
    }
    if (outcome.target.type != TargetType::None)
    {
        bool propose = outcome.action != Action::RecordOnly;
        if (FormUi::Checkbox(ot("Propose a change for wrap-up review", "Änderung zur Abschlussprüfung vorschlagen"), &propose))
        { outcome.action = propose ? defaultAction(outcome.target.type) : Action::RecordOnly; draft.changed = true; }
        if (propose && outcome.target.type == TargetType::Npc)
        {
            int npcAction = outcome.action == Action::NpcNote ? 1 : 0;
            const char *choices[] = {ot("Change NPC attitude", "NSC-Haltung ändern"), ot("Append NPC dialogue note", "NSC-Gesprächsnotiz hinzufügen")};
            if (ImGui::Combo(ot("NPC change", "NSC-Änderung"), &npcAction, choices, 2))
            { outcome.action = npcAction == 1 ? Action::NpcNote : Action::NpcAttitude; draft.changed = true; }
        }
        if (propose && (outcome.action == Action::NpcAttitude || outcome.action == Action::NpcNote || outcome.action == Action::PlaceNote))
        {
            if (outcome.action == Action::NpcAttitude) ol("New attitude toward the party", "Neue Haltung zur Gruppe");
            else if (outcome.action == Action::NpcNote) ol("Note to append to this NPC's dialogue notes", "Notiz zu den Gesprächsnotizen dieses NSC hinzufügen");
            else ol("Note to append to this place", "Notiz zu diesem Ort hinzufügen");
            draft.changed |= FormUi::InputTextMultiline("##outcome-change", &outcome.changeText, ImVec2(-1, 65), ImGuiInputTextFlags_WordWrap);
        }
        if (propose && (outcome.action == Action::ObjectiveState || outcome.action == Action::ClueStatus))
        {
            const char *objectives[] = {ot("Planned", "Geplant"), ot("Active", "Aktiv"), ot("Completed", "Abgeschlossen"), ot("Failed", "Gescheitert")};
            const char *clues[] = {ot("Hidden", "Verborgen"), ot("Discovered", "Entdeckt"), ot("Shared", "Geteilt")};
            draft.changed |= ImGui::Combo(ot("Proposed status", "Vorgeschlagener Status"), &outcome.desiredStatus,
                outcome.action == Action::ObjectiveState ? objectives : clues, outcome.action == Action::ObjectiveState ? 4 : 3);
        }
    }
    if (ImGui::CollapsingHeader(ot("Origin and player text", "Ursprung und Spielertext")))
    {
        if (FormUi::BeginCombo(ot("Source scene", "Ursprungsszene"), outcome.sourceSceneId.empty() ? "-" : outcome.sourceSceneId.c_str()))
        {
            if (ImGui::Selectable("-")) { outcome.sourceSceneId.clear(); draft.changed = true; }
            for (const auto &entry : session.planEntries)
                if (const auto *scene = SessionPlan::findScene(campaign, entry.storyBeatId))
                    if (ImGui::Selectable((scene->title + "##" + scene->metadata.id).c_str()))
                    { outcome.sourceSceneId = scene->metadata.id; draft.changed = true; }
            ImGui::EndCombo();
        }
        if (FormUi::BeginCombo(ot("Original note", "Ursprüngliche Notiz"), outcome.sourceNoteId.empty() ? "-" : outcome.sourceNoteId.c_str()))
        {
            if (ImGui::Selectable("-")) { outcome.sourceNoteId.clear(); draft.changed = true; }
            for (const auto &note : session.notes)
                if (ImGui::Selectable((note.body.substr(0, 70) + "##" + note.metadata.id).c_str()))
                { outcome.sourceNoteId = note.metadata.id; draft.changed = true; }
            ImGui::EndCombo();
        }
        ol("Separate text for players", "Eigener Text für Spieler");
        draft.changed |= FormUi::InputTextMultiline("##outcome-public", &outcome.publicText, ImVec2(-1, 70), ImGuiInputTextFlags_WordWrap);
        draft.changed |= FormUi::Checkbox(ot("Include this text in the player recap review", "Diesen Text im Spieler-Rückblick prüfen"), &outcome.shareWithPlayers);
    }
    // Pin before Record or the frame checkpoint can clear changed. Selecting
    // the existing origin (including '-') is deliberate and also sets changed.
    draft.sourcePinned |= draft.changed;
    ImGui::BeginDisabled(outcome.privateText.find_first_not_of(" \r\n\t") == std::string::npos);
    if (ob("Record outcome", "Ergebnis festhalten", true))
    {
        checkpoint(campaign, session.metadata.id, draft, true);
        if (report(capture(campaign, session.metadata.id, outcome, persist)))
        {
            const auto error = TextDraftRecovery::remove(campaign, outcomeRecoveryKey(session.metadata.id));
            draft = {}; draft.loaded = true; draft.error = error; draft.cleanupPending = !error.empty();
            draft.lastCheckpoint = ImGui::GetTime();
        }
    }
    ImGui::EndDisabled();
}
