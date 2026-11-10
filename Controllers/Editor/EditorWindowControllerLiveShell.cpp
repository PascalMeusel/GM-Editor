#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

const SessionModel *EditorWindowController::activeSession() const
{
    return SessionControl::activeSession(_currentCampaign);
}

bool EditorWindowController::isSessionBreakActive() const
{
    return SessionControl::isLiveSessionBreakActive(_currentCampaign);
}

std::string EditorWindowController::currentInWorldClockLabel() const
{
    return InWorldTime::formatDateTime(_currentCampaign.clock);
}

std::string EditorWindowController::currentInWorldClockPhaseLabel() const
{
    return InWorldTime::phaseLabel(_currentCampaign.clock);
}

void EditorWindowController::setCampaignClockRunning(bool running, const std::string &reason)
{
    if (!_currentCampaign.clock.useSystemTime && _currentCampaign.clock.running == running)
        return;

    InWorldTime::setRunning(_currentCampaign.clock, running);
    _currentCampaign.metadata.touch();
    persistCurrentCampaign(reason);
}

void EditorWindowController::setCampaignClockMultiplier(double multiplier, const std::string &reason)
{
    InWorldTime::setMultiplier(_currentCampaign.clock, multiplier);
    _currentCampaign.metadata.touch();
    persistCurrentCampaign(reason);
}

void EditorWindowController::resetCampaignClockToSystemTime()
{
    InWorldTime::setUseSystemTime(_currentCampaign.clock);
    _currentCampaign.metadata.touch();
    persistCurrentCampaign("reset campaign clock to system time");
}

void EditorWindowController::startLiveSessionFromShell()
{
    if (!flushPreparationDrafts("prepare live session start")) return;
    const auto persist = [this](Campaign &staged) {
        const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "start prepared session");
        if (saved) markSearchIndexDirty();
        return saved;
    };
    const auto *next = SessionPlan::nextSession(_currentCampaign);
    EntityId id = next ? next->metadata.id : EntityId{};
    if (id.empty())
    {
        const auto created = SessionPlan::createSession(_currentCampaign, SessionControl::defaultSessionTitle(_currentCampaign), persist);
        if (!created.applied()) return;
        id = created.sessionId;
    }
    if (!SessionPlan::start(_currentCampaign, id, persist).applied()) return;
    const auto *session = SessionPlan::findSession(_currentCampaign, id);
    const std::string title = session ? session->title : std::string{};
    std::string triggerError;
    AudioTriggers::playMatchingTriggers(_currentCampaign, _children.audioPlaybackService, "SessionEvent", id, "Session Started", triggerError);
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::SessionStarted,
        _currentCampaign.metadata.id, id, "session", "Started live session: " + title});
}

void EditorWindowController::startSessionBreakFromShell()
{
    _children.liveSessionDashboard->startSessionBreak(true);
}

void EditorWindowController::resumeSessionFromBreakFromShell()
{
    _children.liveSessionDashboard->resumeSessionFromBreak(true);
}

void EditorWindowController::endLiveSessionFromShell()
{
    _children.liveSessionDashboard->endActiveSession();
}
void EditorWindowController::touchLocalPlayerDisplayAndSave(const std::string &reason)
{
    _currentCampaign.localPlayerDisplay.metadata.touch();
    _currentCampaign.metadata.touch();
    persistCurrentCampaign(reason);
}

bool EditorWindowController::prepareSavedMapForPlayerReview(const EntityId &mapId)
{
    if (mapId.empty())
        return false;

    const auto map = std::find_if(
        _currentCampaign.maps.begin(), _currentCampaign.maps.end(),
        [&](const MapModel &candidate) { return candidate.metadata.id == mapId; });
    if (map == _currentCampaign.maps.end())
    {
        _shortcuts.statusMessage = uiText(
            "The saved map is no longer available.",
            "Die gespeicherte Karte ist nicht mehr verfügbar.");
        return false;
    }

    // Preparing a selection is persisted, but it is not publication.  Any
    // currently visible detached output is blacked out first and the existing
    // review service remains the only path that can arm or publish it again.
    const LocalPlayerDisplayModel previousDisplay = _currentCampaign.localPlayerDisplay;
    const MapVisibility previousVisibility = map->visibility;
    const EntityMetadata previousMapMetadata = map->metadata;
    const EntityMetadata previousCampaignMetadata = _currentCampaign.metadata;

    map->visibility = MapVisibility::PlayersVisible;
    map->metadata.touch();
    LocalPlayerDisplayModel &display = _currentCampaign.localPlayerDisplay;
    stageMapCreatorPlayerReviewSelection(display, mapId);
    display.metadata.touch();
    _currentCampaign.metadata.touch();

    if (!persistCurrentCampaign("prepare saved map for player review"))
    {
        map->visibility = previousVisibility;
        map->metadata = previousMapMetadata;
        _currentCampaign.localPlayerDisplay = previousDisplay;
        // A failed persistence attempt must not restore a previously visible
        // output while the editor and disk disagree about the staged map.
        ::blackoutLocalPlayerDisplay(_currentCampaign.localPlayerDisplay);
        _currentCampaign.metadata = previousCampaignMetadata;
        _shortcuts.statusMessage = uiText(
            "The player view could not be prepared because the campaign was not saved. Nothing was shown to players.",
            "Die Spieleransicht konnte nicht vorbereitet werden, weil die Kampagne nicht gespeichert wurde. Den Spielenden wurde nichts angezeigt.");
        return false;
    }

    _shortcuts.statusMessage = uiText(
        "Player view prepared in blackout. Review it before arming the display.",
        "Die Spieleransicht wurde im Blackout vorbereitet. Prüfe sie, bevor du die Anzeige aktivierst.");
    showPresentationPanel();
    view->_openPresentationArmReviewNextFrame = true;
    return true;
}

void EditorWindowController::setPresentationModeActive(bool active)
{
    if(active)
    {
        revokePreparationTarget();
        ++view->_playerReviewContextEpoch;
        _workspace.playerReviewTarget.reset();
        _workspace.playerReviewError.clear();
        _children.npcOverview->resetReadingContext();
        view->_playerReviewSession.clear();
        view->_presentationReviewAction=EditorWindowView::PresentationReviewAction::None;
        view->_openPresentationArmReviewNextFrame=false;
        _children.gmScreen->resetPlayerDisplayReview(false);
    }
    _currentCampaign.presentationModeActive = active;
    _currentCampaign.metadata.touch();
    persistCurrentCampaign(active ? "enable presentation mode" : "disable presentation mode");
}
