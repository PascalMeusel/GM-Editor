#include "Controllers/Editor/Widgets/GMScreen/GMScreenControllerSupport.hpp"

void GMScreenController::reviewSessionStart()
{
    _sessionStartReview = SessionPlan::reviewStart(_currentCampaign, _sessionTitleDraft.empty() ? defaultSessionTitle() : _sessionTitleDraft);
    _sessionOperationError.clear();
}
bool GMScreenController::startSession()
{
    const EntityId id = _sessionStartReview.sessionId;
    const std::string title = _sessionStartReview.title;
    const auto result = SessionPlan::startReviewed(_currentCampaign, _sessionStartReview, gmSessionPersist("start reviewed session"));
    if (!gmSessionResult(result, _sessionOperationError)) {
        if (gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German) {
            if (result.status == SessionPlan::Status::Conflict) _sessionOperationError = "Die geprüfte Sitzung hat sich geändert. Bitte abbrechen und den Start erneut prüfen.";
            else if (result.status == SessionPlan::Status::Invalid) _sessionOperationError = "Bitte vor dem Start einen Sitzungstitel eingeben.";
        }
        return false;
    }
    _sessionStartReview = {};
    _sessionTitleDraft.clear();
    std::string triggerError;
    AudioTriggers::playMatchingTriggers(_currentCampaign, _playbackService, "SessionEvent", id, "Session Started", triggerError);
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::SessionStarted, _currentCampaign.metadata.id, id, "session", "Started live session: " + title});
    return true;
}

void GMScreenController::endActiveSession()
{
    const auto *session = activeSession(); if (!session) return;
    const EntityId id = session->metadata.id;
    const std::string title = session->title;
    if (!gmSessionResult(SessionPlan::finish(_currentCampaign, id, gmSessionPersist("finish GM screen session")), _sessionOperationError)) return;
    if (_playbackService) _playbackService->stopAll();
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::SessionEnded, _currentCampaign.metadata.id, id, "session", "Ended live session: " + title});
}

bool GMScreenController::isSessionBreakActive() const
{ return SessionControl::isLiveSessionBreakActive(_currentCampaign); }

void GMScreenController::startSessionBreak(bool pauseAudio)
{
    const auto *session = activeSession(); if (!session || isSessionBreakActive()) return;
    const EntityId id = session->metadata.id;
    if (!gmSessionResult(SessionPlan::setPhase(_currentCampaign, id, SessionPhase::Break, gmSessionPersist("start GM screen break")), _sessionOperationError)) return;
    if (pauseAudio && _playbackService) { std::string error; _playbackService->pausePlaying(error); }
}

void GMScreenController::resumeSessionFromBreak(bool resumeAudio)
{
    const auto *session = activeSession(); if (!session || !isSessionBreakActive()) return;
    const EntityId id = session->metadata.id;
    const SessionPhase phase = session->liveState.phaseBeforeBreak;
    if (!gmSessionResult(SessionPlan::setPhase(_currentCampaign, id, phase, gmSessionPersist("resume GM screen break")), _sessionOperationError)) return;
    if (resumeAudio && _playbackService) { std::string error; _playbackService->resumePaused(error); }
}
int GMScreenController::rollDie(int sides)
{
    std::uniform_int_distribution<int> die(1, std::max(1, sides));
    return die(_randomGenerator);
}

void GMScreenController::addRulesBookmark()
{
    if (_rulesBookmarkDraft.empty())
        return;
    if (std::find(_rulesBookmarks.begin(), _rulesBookmarks.end(), _rulesBookmarkDraft) == _rulesBookmarks.end())
        _rulesBookmarks.push_back(_rulesBookmarkDraft);
    _rulesBookmarkDraft.clear();
}

void GMScreenController::removeRulesBookmark(int index)
{
    if (index < 0 || index >= static_cast<int>(_rulesBookmarks.size()))
        return;
    _rulesBookmarks.erase(_rulesBookmarks.begin() + index);
}

std::vector<ItemModel> GMScreenController::getAvailableItems() const
{
    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    return items;
}
