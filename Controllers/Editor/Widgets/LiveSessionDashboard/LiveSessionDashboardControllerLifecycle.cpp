#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

void LiveSessionDashboardController::selectDashboardPhase(LiveSessionDashboardPhase phase)
{
    if (const auto *session = activeSession())
    {
        const EntityId id = session->metadata.id;
        if (!acceptSessionResult(SessionPlan::setPhase(_currentCampaign, id, static_cast<SessionPhase>(phase), sessionPersist("change live phase")))) return;
    }
    _selectedDashboardPhase = phase;
    _dashboardPhaseManuallySelected = phase != inferredDashboardPhase();
    requestRunSessionTab();
}

void LiveSessionDashboardController::reviewSessionStart()
{
    _sessionReview.start = SessionPlan::reviewStart(_currentCampaign, defaultSessionTitle());
    _sessionPlanMessage.clear();
}
bool LiveSessionDashboardController::startSession()
{
    const EntityId id = _sessionReview.start.sessionId;
    const auto result = SessionPlan::startReviewed(_currentCampaign, _sessionReview.start, sessionPersist("start reviewed session"));
    if (!acceptSessionResult(result)) {
        if (gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German) {
            if (result.status == SessionPlan::Status::Conflict) _sessionPlanMessage = "Die geprüfte Sitzung hat sich geändert. Bitte abbrechen und den Start erneut prüfen.";
            else if (result.status == SessionPlan::Status::Invalid) _sessionPlanMessage = "Bitte vor dem Start einen Sitzungstitel eingeben.";
        }
        return false;
    }
    _sessionReview.start = {};
    afterSessionStarted(id);
    return true;
}

void LiveSessionDashboardController::afterSessionStarted(const EntityId &id, const EntityId &sceneId)
{
    const auto *session = SessionPlan::findSession(_currentCampaign, id);
    if (!session || !session->liveModeActive) return;
    const std::string title = session->title;
    clearPreparationTarget();
    _sessionReview.start = {}; _sessionReview.completedId.clear();
    _dashboardPhaseManuallySelected = false;
    _selectedDashboardPhase = _lastInferredDashboardPhase = inferredDashboardPhase();
    requestRunSessionTab();
    std::string triggerError;
    AudioTriggers::playMatchingTriggers(_currentCampaign, _playbackService, "SessionEvent", id, "Session Started", triggerError);
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::SessionStarted,
                                                        _currentCampaign.metadata.id,
                                                        id,
                                                        "session",
                                                        "Started live session: " + title});
    if (!sceneId.empty()) playCommittedScene(sceneId);
}

SessionPlan::WrapUpDraft LiveSessionDashboardController::currentWrapUpDraft() const
{
    SessionPlan::WrapUpDraft draft;
    const auto *session = activeSession(); if (!session) return draft;
    const bool hasDraft = _sessionSummaryDraftId == session->metadata.id;
    draft.summary = hasDraft ? _sessionSummaryDraft : session->summary;
    draft.recap = hasDraft ? _recapDraft : session->recap;
    draft.unresolvedThreads = hasDraft ? _unresolvedThreadsDraft : session->unresolvedThreads;
    draft.lootAwarded = hasDraft ? _lootAwardedDraft : session->lootAwarded;
    draft.xpMilestones = hasDraft ? _xpMilestonesDraft : session->xpMilestones;
    draft.npcsMet = hasDraft ? _npcsMetDraft : session->npcsMet;
    draft.locationsVisited = hasDraft ? _locationsVisitedDraft : session->locationsVisited;
    draft.openHooks = hasDraft ? _openHooksDraft : session->openHooks;
    if (hasDraft && !_sessionNoteDraft.empty())
    {
        draft.pendingNote.body = _sessionNoteDraft;
        const auto entries = noteLinkCandidates();
        const auto linked = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return entry.id == _selectedNoteEntityId; });
        if (linked != entries.end())
        {
            draft.pendingNote.linkedEntityId = linked->id;
            draft.pendingNote.linkedEntityType = linked->type;
            draft.pendingNote.linkedEntityLabel = linked->displayName;
        }
    }
    return draft;
}

bool LiveSessionDashboardController::hasUnsavedDrafts() const
{
    const auto *session = activeSession();
    const bool toolkitDrafts = hasPendingOutcomeDraftWrites() || (_tabletopToolkit && _tabletopToolkit->hasPendingDrafts());
    if (!session || _sessionSummaryDraftId != session->metadata.id) return toolkitDrafts;
    return toolkitDrafts || !_sessionNoteDraft.empty() || _sessionSummaryDraft != session->summary || _recapDraft != session->recap ||
        _unresolvedThreadsDraft != session->unresolvedThreads || _lootAwardedDraft != session->lootAwarded ||
        _xpMilestonesDraft != session->xpMilestones || _npcsMetDraft != session->npcsMet ||
        _locationsVisitedDraft != session->locationsVisited || _openHooksDraft != session->openHooks;
}

bool LiveSessionDashboardController::flushDrafts()
{
    if (!checkpointOutcomeDrafts()) return false;
    checkpointTextRecovery(true);
    if (_tabletopToolkit && !_tabletopToolkit->flushDraftsForCampaignSave()) return false;
    return flushSessionDrafts();
}

void LiveSessionDashboardController::onLiveCampaignCommitted()
{
    if (_tabletopToolkit) _tabletopToolkit->commitPendingHistory();
}

bool LiveSessionDashboardController::flushSessionDrafts()
{
    checkpointTextRecovery(true);
    if (_textRecoveryPending) { _sessionPlanMessage = "Restore or discard the recovered text before saving."; return false; }
    const auto *session = activeSession();
    const bool hasSessionDrafts = session && _sessionSummaryDraftId == session->metadata.id &&
        (!_sessionNoteDraft.empty() || _sessionSummaryDraft != session->summary || _recapDraft != session->recap ||
         _unresolvedThreadsDraft != session->unresolvedThreads || _lootAwardedDraft != session->lootAwarded ||
         _xpMilestonesDraft != session->xpMilestones || _npcsMetDraft != session->npcsMet ||
         _locationsVisitedDraft != session->locationsVisited || _openHooksDraft != session->openHooks);
    if (!hasSessionDrafts) return true;
    const EntityId id = _sessionSummaryDraftId;
    if (!acceptSessionResult(SessionPlan::saveWrapUpDraft(_currentCampaign, id, currentWrapUpDraft(), sessionPersist("save live session drafts")))) return false;
    _sessionNoteDraft.clear(); _selectedNoteEntityId.clear();
    clearTextRecovery();
    return true;
}

void LiveSessionDashboardController::resetDrafts()
{
    if (_view) _view->resetOutcomeDrafts();
    _textRecoveryRead = _textRecoveryPending = false;
    _textRecoveryCampaign.clear(); _textRecoveryBase.clear(); _textRecoveryFields.clear();
    _textRecoveredFields.clear(); _textRecoveryError.clear(); _textRecoveryAt = -10.0;
    if (_tabletopToolkit) _tabletopToolkit->resetDrafts();
    _sessionSummaryDraftId.clear(); _sessionSummaryDraft.clear(); _sessionNoteDraft.clear();
    _recapDraft.clear(); _unresolvedThreadsDraft.clear(); _lootAwardedDraft.clear();
    _xpMilestonesDraft.clear(); _npcsMetDraft.clear(); _locationsVisitedDraft.clear(); _openHooksDraft.clear();
    _selectedNoteEntityId.clear(); _selectedPlanEntryId.clear(); _sceneLaunchMessage.clear(); _sessionPlanMessage.clear();
    _clockDraftSessionId.clear(); _clockConfigDraftKey.clear(); _postSessionReportDraft.clear();
    _sessionReview = {};
    _dashboardPhaseManuallySelected = false;
    _selectedDashboardPhase = _lastInferredDashboardPhase = inferredDashboardPhase();
}

bool LiveSessionDashboardController::viewCompletedRecap(const EntityId &id)
{
    const auto *session = id.empty() ? SessionReport::latestCompleted(_currentCampaign) : SessionPlan::findSession(_currentCampaign, id);
    if (!session || !SessionReport::completed(*session) || _currentCampaign.presentationModeActive) return false;
    _sessionReview.completedId = session->metadata.id;
    _selectedDashboardPhase = LiveSessionDashboardPhase::WrapUp;
    _dashboardPhaseManuallySelected = true;
    _lastInferredDashboardPhase = inferredDashboardPhase();
    return true;
}
void LiveSessionDashboardController::useCurrentRecap()
{
    _sessionReview.completedId.clear();
    _dashboardPhaseManuallySelected = false;
    _selectedDashboardPhase = _lastInferredDashboardPhase = inferredDashboardPhase();
}

void LiveSessionDashboardController::endActiveSession()
{
    checkpointTextRecovery(true);
    if (_textRecoveryPending) { _sessionPlanMessage = "Restore or discard the recovered text before finishing."; return; }
    const auto *session = activeSession(); if (!session) return;
    const EntityId sessionId = session->metadata.id;
    const std::string sessionTitle = session->title;
    const auto draft = currentWrapUpDraft();
    if (!acceptSessionResult(SessionPlan::finishWithDraft(_currentCampaign, sessionId, draft, sessionPersist("finish live session")))) return;
    _sessionNoteDraft.clear(); _selectedNoteEntityId.clear();
    clearTextRecovery();
    viewCompletedRecap(sessionId);
    if (_playbackService) _playbackService->stopAll();
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::SessionEnded,
                                                        _currentCampaign.metadata.id,
                                                        sessionId,
                                                        "session",
                                                        "Ended live session: " + sessionTitle});
}

bool LiveSessionDashboardController::isSessionBreakActive() const
{
    return SessionControl::isLiveSessionBreakActive(_currentCampaign);
}

void LiveSessionDashboardController::startSessionBreak(bool pauseAudio)
{
    const auto *session = activeSession();
    if (!session || isSessionBreakActive()) return;
    const EntityId id = session->metadata.id;
    if (!acceptSessionResult(SessionPlan::setPhase(_currentCampaign, id, SessionPhase::Break, sessionPersist("start session break")))) return;
    requestRunSessionTab();
    if (pauseAudio && _playbackService)
    {
        _playbackService->pausePlaying(_audioErrorMessage);
        _audioStatusMessage = gm::localization::tr(gm::localization::TextId::LivePaused);
    }
}

void LiveSessionDashboardController::resumeSessionFromBreak(bool resumeAudio)
{
    const auto *session = activeSession();
    if (!session || !isSessionBreakActive()) return;
    const EntityId id = session->metadata.id;
    const SessionPhase phase = session->liveState.phaseBeforeBreak;
    if (!acceptSessionResult(SessionPlan::setPhase(_currentCampaign, id, phase, sessionPersist("resume session break")))) return;
    requestRunSessionTab();
    if (resumeAudio && _playbackService)
    {
        _playbackService->resumePaused(_audioErrorMessage);
        _audioStatusMessage = gm::localization::tr(gm::localization::TextId::LiveRunning);
    }
}
std::string LiveSessionDashboardController::currentInWorldClockLabel() const
{
    return InWorldTime::formatDateTimeForDisplay(_currentCampaign.clock);
}

std::string LiveSessionDashboardController::currentClockPhaseLabel() const
{
    return InWorldTime::phaseLabelForDisplay(_currentCampaign.clock);
}

std::string LiveSessionDashboardController::clockDraftKey() const
{
    return clockStateKey(_currentCampaign);
}
