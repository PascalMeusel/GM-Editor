#include "Views/Editor/SessionWorkspaceSupport.hpp"

namespace
{
bool archivedTarget(const EntityMetadata &metadata)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), "archived") != metadata.tags.end();
}
PreparationTargetResult unavailableTarget()
{
    return {PreparationTargetStatus::Unavailable, st(
        "The carried-to session or its selected scene is unavailable. Choose and review the destination in Prepare.",
        "Die Zielsitzung oder ihre gewählte Szene ist nicht verfügbar. Wähle und prüfe das Ziel in der Vorbereitung.")};
}
}

PreparationTargetResult SessionWorkspace::inspectPreparationTarget(Campaign &campaign, const EntityId &id)
{
    if (campaign.presentationModeActive || campaign.metadata.id.empty() ||
        (!_campaignId.empty() && _campaignId != campaign.metadata.id))
        return {PreparationTargetStatus::StaleContext, st(
            "The session context changed. Return to GM mode and review the destination again.",
            "Der Sitzungskontext wurde geändert. Kehre zum SL-Modus zurück und prüfe das Ziel erneut.")};
    const SessionModel *target = nullptr;
    for (const auto &session : campaign.sessions)
        if (session.metadata.id == id) { if (target || id.empty()) return unavailableTarget(); target = &session; }
    if (!target || archivedTarget(target->metadata) || target->liveModeActive ||
        !target->startedAtUtc.empty() || !target->endedAtUtc.empty()) return unavailableTarget();

    const bool same = _sessionId == id && !_selectionUnavailable;
    const EntityId entryId = same ? _entryId : target->liveState.activePlanEntryId.empty()
        ? (target->planEntries.empty() ? EntityId{} : target->planEntries.front().metadata.id)
        : target->liveState.activePlanEntryId;
    if (!same && !target->planEntries.empty() && entryId.empty()) return unavailableTarget();
    if (!entryId.empty())
    {
        const SessionPlanEntry *entry = nullptr;
        for (const auto &candidate : target->planEntries)
            if (candidate.metadata.id == entryId) { if (entry) return unavailableTarget(); entry = &candidate; }
        if (!entry || archivedTarget(entry->metadata)) return unavailableTarget();
        if (same && _sceneId != entry->storyBeatId) return unavailableTarget();
        if (!entry->storyBeatId.empty())
        {
            const StoryBeatModel *scene = nullptr;
            for (const auto &thread : campaign.storyThreads)
                for (const auto &candidate : thread.beats)
                    if (candidate.metadata.id == entry->storyBeatId)
                    { if (scene || archivedTarget(thread.metadata) || archivedTarget(candidate.metadata)) return unavailableTarget(); scene = &candidate; }
            if (!scene) return unavailableTarget();
        }
    }
    // A same-target focus never reloads the draft or reconciles its checkpoint.
    if (same) return {PreparationTargetStatus::Opened, {}};
    if (_campaignId.empty()) _campaignId = campaign.metadata.id;
    readRecovery(campaign); // Live may be used before Prepare has ever drawn.
    if (_recoveryPending || !_recoveryError.empty())
        return {PreparationTargetStatus::NeedsReview, st(
            "Review the recovered preparation text and any recovery error in Prepare first, then retry Prepare this session.",
            "Prüfe zuerst den wiederhergestellten Vorbereitungstext und mögliche Sicherungsfehler in der Vorbereitung. Wähle danach erneut Diese Sitzung vorbereiten.")};
    if (!_newSessionTitle.empty() || !_newSceneTitle.empty())
        return {PreparationTargetStatus::NeedsReview, st(
            "Finish or clear the unfinished new-session or new-scene form in Prepare first, then retry Prepare this session.",
            "Schließe zuerst das offene Formular für eine neue Sitzung oder Szene in der Vorbereitung ab oder leere es. Wähle danach erneut Diese Sitzung vorbereiten.")};
    if (dirty())
        return {PreparationTargetStatus::NeedsReview, st(
            "Save or review the current preparation first, then retry Prepare this session. Its drafts have been retained.",
            "Speichere oder prüfe zuerst die aktuelle Vorbereitung und wähle dann erneut Diese Sitzung vorbereiten. Ihre Entwürfe bleiben erhalten.")};
    return {PreparationTargetStatus::Opened, {}};
}

PreparationTargetResult SessionWorkspace::openPreparationTarget(Campaign &campaign, const EntityId &id)
{
    const auto result = inspectPreparationTarget(campaign, id);
    if (result.opened() && (_sessionId != id || _selectionUnavailable)) load(campaign, id);
    return result;
}

bool SessionWorkspace::matchesPreparationTarget(const PreparationTargetRequest &request) const
{ return _outcomes.matchesPreparationTarget(request); }
void SessionWorkspace::reportPreparationTarget(const PreparationTargetRequest &request, const PreparationTargetResult &result)
{ _outcomes.reportPreparationTarget(request, result); }
void SessionWorkspace::clearPreparationTarget() { _outcomes.clearPreparationTarget(); }
