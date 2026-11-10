#include "Views/Editor/SessionOutcomesPanelSupport.hpp"
#include "Models/Project/ApplicationPaths.hpp"

void SessionOutcomesPanel::review(Campaign &campaign, const SessionModel &session,
                                 const SessionOutcomes::Ledger &ledger, const SessionPlan::Persist &persist)
{
    using namespace SessionOutcomes;
    ImGui::TextWrapped("%s", ot("Select outcomes, inspect the proposed changes, then apply them together. Original notes are kept.",
        "Ergebnisse auswählen, vorgeschlagene Änderungen prüfen und gemeinsam übernehmen. Ursprüngliche Notizen bleiben erhalten."));
    if (ledger.outcomes.empty()) ImGui::TextUnformatted(ot("No outcomes recorded yet.", "Noch keine Ergebnisse festgehalten."));
    for (const auto &outcome : ledger.outcomes)
    {
        ImGui::PushID(outcome.id.c_str());
        bool chosen = _selected.count(outcome.id) != 0;
        ImGui::BeginDisabled(outcome.state != State::Pending);
        if (FormUi::Checkbox("##review-select", &chosen))
        { if (chosen) _selected.insert(outcome.id); else _selected.erase(outcome.id); }
        ImGui::EndDisabled();
        ImGui::SameLine(); ImGui::TextUnformatted(stateLabel(outcome.state));
        ImGui::TextWrapped("%s", outcome.privateText.c_str());
        ImGui::TextWrapped("%s", targetLabel(campaign, outcome.target).c_str());
        ImGui::TextUnformatted(actionLabel(outcome.action));
        if (outcome.state == State::Pending && ob("Reject proposal", "Vorschlag verwerfen"))
        {
            report(reject(campaign, session.metadata.id, outcome, persist));
            _applyReview = {};
        }
        ImGui::Separator(); ImGui::PopID();
    }
    std::vector<EntityId> ids;
    for (const auto &o : ledger.outcomes) if (o.state == State::Pending && _selected.count(o.id)) ids.push_back(o.id);
    ImGui::BeginDisabled(ids.empty());
    if (ob("Review selected changes", "Ausgewählte Änderungen prüfen", true))
    {
        _applyReview = reviewApply(campaign, session.metadata.id, ids);
        if (!_applyReview.success) _message = _applyReview.error;
    }
    ImGui::EndDisabled();
    if (_applyReview.success && _applyReview.sessionId == session.metadata.id)
    {
        ImGui::SeparatorText(ot("Proposed changes", "Vorgeschlagene Änderungen"));
        for (const auto &change : _applyReview.receipt.changes)
        {
            ImGui::TextWrapped("%s", targetLabel(campaign, change.target).c_str());
            ImGui::TextUnformatted(actionLabel(change.action));
            if (change.action == Action::RecordOnly)
                ImGui::TextUnformatted(ot("Record as reviewed; no campaign field changes.", "Als geprüft markieren; keine Kampagnenfelder ändern."));
            else
            {
                ImGui::TextWrapped("%s: %s", ot("Before", "Vorher"), valueLabel(change.action, change.before).c_str());
                ImGui::TextWrapped("%s: %s", ot("After", "Nachher"), valueLabel(change.action, change.after).c_str());
            }
            ImGui::Separator();
        }
        if (ob("Apply reviewed changes", "Geprüfte Änderungen übernehmen", true))
        {
            if (report(apply(campaign, _applyReview, persist))) { _applyReview = {}; _selected.clear(); }
        }
        if (ob("Cancel review", "Prüfung abbrechen")) _applyReview = {};
    }
    if (!ledger.receipts.empty() && ImGui::CollapsingHeader(ot("Applied reviews and correction", "Übernommene Prüfungen und Korrektur")))
        for (auto receipt = ledger.receipts.rbegin(); receipt != ledger.receipts.rend(); ++receipt)
        {
            ImGui::PushID(receipt->id.c_str());
            ImGui::TextUnformatted(receipt->appliedAtUtc.c_str());
            ImGui::BeginDisabled(receipt->undone);
            if (ob("Undo this review", "Diese Übernahme rückgängig machen"))
                report(undo(campaign, session.metadata.id, receipt->id, persist));
            ImGui::EndDisabled(); ImGui::PopID();
        }
}
void SessionOutcomesPanel::carry(Campaign &campaign, const SessionModel &session,
                                const SessionOutcomes::Ledger &ledger, const SessionPlan::Persist &persist,
                                const PreparationTargetSink &sink)
{
    using namespace SessionOutcomes;
    if (!ledger.followUps.empty())
    {
        ImGui::SeparatorText(ot("Carried into this session", "In diese Sitzung übernommen"));
        for (const auto &follow : ledger.followUps)
        {
            ImGui::PushID(follow.id.c_str()); bool done = follow.completed;
            if (FormUi::Checkbox(ot("Resolved", "Erledigt"), &done))
                report(setFollowUpCompleted(campaign, session.metadata.id, follow.id, done, persist));
            ImGui::TextWrapped("%s", follow.privateText.c_str());
            const auto *origin = SessionPlan::findSession(campaign, follow.sourceSessionId);
            ImGui::TextWrapped("%s: %s", ot("From", "Aus"), origin ? origin->title.c_str() : ot("Unavailable session", "Nicht verfügbare Sitzung"));
            ImGui::PopID();
        }
    }
    ImGui::SeparatorText(ot("Choose material for the next session", "Material für die nächste Sitzung wählen"));
    bool selectionChanged = false;
    for (const auto &outcome : ledger.outcomes)
    {
        if (outcome.state == State::Rejected) continue;
        ImGui::PushID(outcome.id.c_str()); bool selected = _selected.count(outcome.id) != 0;
        if (FormUi::Checkbox("##carry-select", &selected))
        { if (selected) _selected.insert(outcome.id); else _selected.erase(outcome.id); selectionChanged = true; }
        ImGui::SameLine(); ImGui::TextWrapped("%s", outcome.privateText.c_str()); ImGui::PopID();
    }
    if (ImGui::CollapsingHeader(ot("Reuse scenes", "Szenen wiederverwenden")))
        for (const auto &entry : session.planEntries)
            if (const auto *scene = SessionPlan::findScene(campaign, entry.storyBeatId))
            {
                bool selected = _scenes.count(scene->metadata.id) != 0;
                if (FormUi::Checkbox((scene->title + "##" + scene->metadata.id).c_str(), &selected))
                { if (selected) _scenes.insert(scene->metadata.id); else _scenes.erase(scene->metadata.id); selectionChanged = true; }
            }
    const auto *target = SessionPlan::findSession(campaign, _targetSession);
    ol("Destination", "Ziel");
    if (FormUi::BeginCombo("##carry-target", target ? target->title.c_str() : ot("Create a new session", "Neue Sitzung erstellen")))
    {
        if (ImGui::Selectable(ot("Create a new session", "Neue Sitzung erstellen"))) { _targetSession.clear(); selectionChanged = true; }
        for (const auto &candidate : campaign.sessions)
            if (candidate.metadata.id != session.metadata.id && candidate.startedAtUtc.empty() && candidate.endedAtUtc.empty() &&
                std::find(candidate.metadata.tags.begin(), candidate.metadata.tags.end(), "archived") == candidate.metadata.tags.end())
                if (ImGui::Selectable((candidate.title + "##" + candidate.metadata.id).c_str()))
                { _targetSession = candidate.metadata.id; selectionChanged = true; }
        ImGui::EndCombo();
    }
    if (_targetSession.empty()) { ol("New session title", "Titel der neuen Sitzung"); selectionChanged |= FormUi::InputText("##next-title", &_nextTitle); }
    if (selectionChanged) _carrySubmitted = false;
    if (ob("Carry selected material forward", "Ausgewähltes Material übernehmen", true))
    {
        if (!_carrySubmitted || _carryRequest.sourceSessionId != session.metadata.id)
        {
            _carryRequest = {}; _carryRequest.sourceSessionId = session.metadata.id;
            _carryRequest.targetSessionId = _targetSession; _carryRequest.newSessionTitle = _nextTitle;
            for (const auto &outcome : ledger.outcomes)
                if (outcome.state != State::Rejected && _selected.count(outcome.id)) _carryRequest.outcomeIds.push_back(outcome.id);
            for (const auto &entry : session.planEntries) if (_scenes.count(entry.storyBeatId)) _carryRequest.sceneIds.push_back(entry.storyBeatId);
            _carrySubmitted = true;
        }
        const auto request = _carryRequest;
        const auto campaignId = campaign.metadata.id;
        const auto root = ApplicationPaths::writableRoot();
        const auto epoch = _preparationEpoch;
        const auto result = carryForward(campaign, request, persist);
        // Persistence subscribers can replace/reset the owning editor. Do not
        // attach a successful old operation to a new source or campaign.
        if (campaign.metadata.id != campaignId || _campaignId != campaignId ||
            _drawnSession != request.sourceSessionId || _carryRequest.requestId != request.requestId || _preparationEpoch != epoch ||
            campaign.presentationModeActive || root != ApplicationPaths::writableRoot()) return;
        if (report(result) && !result.targetSessionId.empty())
        {
            _preparationTarget = PreparationTargetRequest{campaignId, request.sourceSessionId,
                result.targetSessionId, request.requestId, _recoverySurface};
            const auto *destination = SessionPlan::findSession(campaign, result.targetSessionId);
            _preparationTargetTitle = destination ? destination->title : request.newSessionTitle;
            _preparationRoot = root;
            _preparationNavigationError.clear();
            _message = ot("Material saved. You can now prepare the destination session.",
                "Material gespeichert. Du kannst jetzt die Zielsitzung vorbereiten.");
        }
    }
    drawPreparationTarget(campaign, sink);
}
