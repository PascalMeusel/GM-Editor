#include "Controllers/Editor/Widgets/GMScreen/GMScreenControllerSupport.hpp"

GMScreenController::GMScreenController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService,
                                     std::shared_ptr<TabletopToolkitHistoryState> toolkitHistory)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService)), _randomGenerator(std::random_device{}())
{
    _tabletopToolkit = std::make_unique<TabletopToolkitController>(_currentCampaign, std::move(toolkitHistory));
    _view = std::make_unique<GMScreenView>(*this);
}

void GMScreenController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    if (_currentCampaign.presentationModeActive || !m_isOpen || !_view->isVisible()) _view->resetPlayerReview();
    if (!m_isOpen)
        return;

    _view->update();
    _view->finishPlayerReviewFrame();
    if (!_view->isVisible())
        m_isOpen = false;
}

bool GMScreenController::hasUnsavedDrafts() const
{
    return _tabletopToolkit && _tabletopToolkit->hasPendingDrafts();
}

bool GMScreenController::flushDrafts()
{
    return !_tabletopToolkit || _tabletopToolkit->flushDraftsForCampaignSave();
}

void GMScreenController::onLiveCampaignCommitted()
{
    if (_tabletopToolkit) _tabletopToolkit->commitPendingHistory();
}

void GMScreenController::resetDrafts()
{
    resetPlayerDisplayReview();
    if (_tabletopToolkit) _tabletopToolkit->resetDrafts();
    _sessionStartReview = {}; _sessionTitleDraft.clear(); _sessionOperationError.clear();
}

SessionModel *GMScreenController::activeSession()
{
    return SessionControl::activeSession(_currentCampaign);
}

const SessionModel *GMScreenController::activeSession() const
{
    return SessionControl::activeSession(_currentCampaign);
}

std::string GMScreenController::defaultSessionTitle() const
{
    return SessionControl::defaultSessionTitle(_currentCampaign);
}
