#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

LiveSessionDashboardController::LiveSessionDashboardController(Campaign &currentCampaign, std::shared_ptr<AudioPlaybackService> playbackService,
                                                             std::shared_ptr<TabletopToolkitHistoryState> toolkitHistory)
    : _currentCampaign(currentCampaign), _playbackService(std::move(playbackService)), _randomGenerator(std::random_device{}())
{
    _clockAdvanceDraft = gm::localization::trUi("10 minutes");
    _tabletopToolkit = std::make_unique<TabletopToolkitController>(_currentCampaign, std::move(toolkitHistory));
    _view = std::make_unique<LiveSessionDashboardView>(*this);
    // Restoring a campaign restores state, never device playback or publication.
    if (_playbackService) _playbackService->stopAll();
    prepareLocalPlayerDisplayForApplicationStart(_currentCampaign.localPlayerDisplay);
}

void LiveSessionDashboardController::setOpenStoryHandler(std::function<void(const EntityId &, const EntityId &)> handler)
{
    _openStoryHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenEntityHandler(std::function<void(std::string, EntityId)> handler)
{
    _openEntityHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenMapHandler(std::function<void(const EntityId &, bool)> handler)
{
    _openMapHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenEncounterHandler(std::function<void(const EntityId &)> handler)
{
    _openEncounterHandler = std::move(handler);
}

void LiveSessionDashboardController::setRenderCombatHandler(std::function<void(const EntityId &)> handler)
{
    _renderCombatHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenNpcHandler(std::function<void()> handler)
{
    _openNpcHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenItemHandler(std::function<void()> handler)
{
    _openItemHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenAudioMiniHandler(std::function<void()> handler)
{
    _openAudioMiniHandler = std::move(handler);
}

void LiveSessionDashboardController::setOpenAudioLibraryHandler(std::function<void()> handler)
{
    _openAudioLibraryHandler = std::move(handler);
}

void LiveSessionDashboardController::setPresentationModeActive(bool active)
{
    if (active)
    {
        if (!_currentCampaign.presentationModeActive && _preparationContextRevokedHandler)
            _preparationContextRevokedHandler();
        else clearPreparationTarget();
    }
    _currentCampaign.presentationModeActive = active;
    saveCampaign(active ? "enable presentation mode" : "disable presentation mode");
}

void LiveSessionDashboardController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    if (!m_isOpen)
        return;

    configure();
    _view->update();
    checkpointTextRecovery();
    if (!_view->isVisible())
        m_isOpen = false;
}

SessionModel *LiveSessionDashboardController::activeSession()
{
    return SessionControl::activeSession(_currentCampaign);
}

const SessionModel *LiveSessionDashboardController::activeSession() const
{
    return SessionControl::activeSession(_currentCampaign);
}

std::string LiveSessionDashboardController::defaultSessionTitle() const
{
    return SessionControl::defaultSessionTitle(_currentCampaign);
}

const StoryThreadModel *LiveSessionDashboardController::activeStoryThread() const
{
    if (const auto *session = activeSession(); session && session->liveState.initialized)
    {
        const auto *entry = SessionPlan::findEntry(*session, session->liveState.activePlanEntryId);
        if (!entry) return nullptr;
        for (const auto &thread : _currentCampaign.storyThreads)
            for (const auto &scene : thread.beats)
                if (scene.metadata.id == entry->storyBeatId) return &thread;
        return nullptr;
    }
    for (const StoryThreadModel &thread : _currentCampaign.storyThreads)
    {
        if (thread.status == StoryStatus::Active)
            return &thread;
    }
    return _currentCampaign.storyThreads.empty() ? nullptr : &_currentCampaign.storyThreads.front();
}

const StoryBeatModel *LiveSessionDashboardController::activeStoryBeat() const
{
    if (const auto *session = activeSession(); session && session->liveState.initialized)
    {
        const auto *entry = SessionPlan::findEntry(*session, session->liveState.activePlanEntryId);
        return entry ? SessionPlan::findScene(_currentCampaign, entry->storyBeatId) : nullptr;
    }
    const StoryThreadModel *thread = activeStoryThread();
    if (!thread)
        return nullptr;

    for (const StoryBeatModel &beat : thread->beats)
    {
        if (beat.status == StoryStatus::Active)
            return &beat;
    }
    return thread->beats.empty() ? nullptr : &thread->beats.front();
}

const MapModel *LiveSessionDashboardController::activeMap() const
{
    const EntityId mapId = !_currentCampaign.partyLocation.mapId.empty() ? _currentCampaign.partyLocation.mapId : _currentCampaign.partyTravel.currentMapId;
    for (const MapModel &map : _currentCampaign.maps)
    {
        if (map.metadata.id == mapId)
            return &map;
    }
    for (const MapModel &map : _currentCampaign.maps)
    {
        if (map.isMainMap)
            return &map;
    }
    return _currentCampaign.maps.empty() ? nullptr : &_currentCampaign.maps.front();
}

const EncounterModel *LiveSessionDashboardController::activeEncounter() const
{
    if (const auto *session = activeSession(); session && session->liveState.initialized)
    {
        for (const auto &encounter : _currentCampaign.encounters)
            if (encounter.metadata.id == session->liveState.activeEncounterId) return &encounter;
        return nullptr;
    }
    if (!_currentCampaign.localPlayerDisplay.publicEncounterId.empty())
    {
        const auto staged = std::find_if(_currentCampaign.encounters.begin(), _currentCampaign.encounters.end(), [&](const EncounterModel &encounter) {
            return encounter.metadata.id == _currentCampaign.localPlayerDisplay.publicEncounterId;
        });
        if (staged != _currentCampaign.encounters.end())
            return &(*staged);
    }
    for (const EncounterModel &encounter : _currentCampaign.encounters)
    {
        if (dashboardEncounterHasStarted(&encounter))
            return &encounter;
    }
    return _currentCampaign.encounters.empty() ? nullptr : &_currentCampaign.encounters.front();
}

const AudioAssetModel *LiveSessionDashboardController::selectedAudioAsset() const
{
    if (_selectedAudioAssetIndex < 0 || _selectedAudioAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        return nullptr;
    return &_currentCampaign.audioAssets[static_cast<std::size_t>(_selectedAudioAssetIndex)];
}

AudioAssetModel *LiveSessionDashboardController::selectedAudioAsset()
{
    if (_selectedAudioAssetIndex < 0 || _selectedAudioAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        return nullptr;
    return &_currentCampaign.audioAssets[static_cast<std::size_t>(_selectedAudioAssetIndex)];
}

std::vector<const StoryBeatModel *> LiveSessionDashboardController::sceneOptions() const
{
    std::vector<const StoryBeatModel *> options;
    for (const StoryThreadModel &thread : _currentCampaign.storyThreads)
    {
        for (const StoryBeatModel &beat : thread.beats)
            options.push_back(&beat);
    }
    return options;
}

LiveSessionDashboardPhase LiveSessionDashboardController::inferredDashboardPhase() const
{
    const SessionModel *session = activeSession();
    if (!session)
        return LiveSessionDashboardPhase::Prep;
    if (session->liveState.initialized)
        return static_cast<LiveSessionDashboardPhase>(session->liveState.phase);
    if (isSessionBreakActive())
        return LiveSessionDashboardPhase::Break;

    const EncounterModel *encounter = activeEncounter();
    if (dashboardEncounterHasStarted(encounter))
        return LiveSessionDashboardPhase::Combat;
    if (!_currentCampaign.partyTravel.routeName.empty())
        return LiveSessionDashboardPhase::ExplorationTravel;
    if (activeStoryBeat())
        return LiveSessionDashboardPhase::RunningScene;
    if (activeMap())
        return LiveSessionDashboardPhase::ExplorationTravel;
    return LiveSessionDashboardPhase::RunningScene;
}

LiveSessionDashboardPhase LiveSessionDashboardController::dashboardPhase() const
{
    return _selectedDashboardPhase;
}

LiveSessionDashboardPhase LiveSessionDashboardController::selectedPhase() const
{
    return dashboardPhase();
}

void LiveSessionDashboardController::selectPhase(LiveSessionDashboardPhase phase)
{
    selectDashboardPhase(phase);
}

void LiveSessionDashboardController::renderEmbedded()
{
    configure();
    _view->drawEmbedded();
    checkpointTextRecovery();
}

void LiveSessionDashboardController::openTabletopToolkit()
{
    _view->requestTableToolsTab();
}

void LiveSessionDashboardController::openObjectives()
{
    _tabletopToolkit->showObjectives();
    _view->requestTableToolsTab();
}

void LiveSessionDashboardController::openClues()
{
    _tabletopToolkit->showClues();
    _view->requestTableToolsTab();
}

bool LiveSessionDashboardController::focusObjective(const EntityId &objectiveId)
{
    if (!_tabletopToolkit || !_tabletopToolkit->focusObjective(objectiveId))
        return false;
    _view->requestTableToolsTab();
    return true;
}

bool LiveSessionDashboardController::focusClue(const EntityId &clueId)
{
    if (!_tabletopToolkit || !_tabletopToolkit->focusClue(clueId))
        return false;
    _view->requestTableToolsTab();
    return true;
}

void LiveSessionDashboardController::configure()
{
    prepareTextRecovery();
    checkpointTextRecovery();
    if (_selectedAudioAssetIndex < 0 && !_currentCampaign.audioAssets.empty())
        selectAudioAsset(0);
    if (_selectedAudioAssetIndex >= static_cast<int>(_currentCampaign.audioAssets.size()))
        selectAudioAsset(static_cast<int>(_currentCampaign.audioAssets.size()) - 1);
    const std::vector<const StoryBeatModel *> scenes = sceneOptions();
    if (_selectedSceneIndex < 0 && !scenes.empty())
        _selectedSceneIndex = 0;
    if (_selectedSceneIndex >= static_cast<int>(scenes.size()))
        _selectedSceneIndex = scenes.empty() ? -1 : static_cast<int>(scenes.size()) - 1;

    const SessionModel *session = activeSession();
    if (session && _sessionSummaryDraftId != session->metadata.id)
    {
        checkpointTextRecovery(true);
        _sessionSummaryDraftId = session->metadata.id;
        _sessionSummaryDraft = session->summary;
        _recapDraft = session->recap;
        _unresolvedThreadsDraft = session->unresolvedThreads;
        _lootAwardedDraft = session->lootAwarded;
        _xpMilestonesDraft = session->xpMilestones;
        _npcsMetDraft = session->npcsMet;
        _locationsVisitedDraft = session->locationsVisited;
        _openHooksDraft = session->openHooks;
    }
    else if (!session)
    {
        checkpointTextRecovery(true);
        _sessionSummaryDraftId.clear();
        _sessionSummaryDraft.clear();
        _recapDraft.clear();
        _unresolvedThreadsDraft.clear();
        _lootAwardedDraft.clear();
        _xpMilestonesDraft.clear();
        _npcsMetDraft.clear();
        _locationsVisitedDraft.clear();
        _openHooksDraft.clear();
    }

    if (session && _clockDraftSessionId != session->metadata.id)
    {
        _clockDraftSessionId = session->metadata.id;
        _clockAdvanceDraft = gm::localization::trUi("10 minutes");
        _clockReasonDraft.clear();
        _clockStatusMessage.clear();
    }
    else if (!session)
    {
        _clockDraftSessionId.clear();
        _clockReasonDraft.clear();
        _clockStatusMessage.clear();
    }

    if (_clockConfigDraftKey != clockDraftKey())
        syncClockDraftsFromCampaign();

    const LiveSessionDashboardPhase inferredPhase = inferredDashboardPhase();
    if (!_dashboardPhaseManuallySelected || inferredPhase != _lastInferredDashboardPhase)
    {
        _selectedDashboardPhase = inferredPhase;
        _dashboardPhaseManuallySelected = false;
    }
    _lastInferredDashboardPhase = inferredPhase;
    prepareTextRecovery();
}

void LiveSessionDashboardController::setPreparationTargetHandler(PreparationTargetSink handler)
{ _preparationTargetHandler = std::move(handler); }

void LiveSessionDashboardController::setPreparationContextRevokedHandler(std::function<void()> handler)
{ _preparationContextRevokedHandler = std::move(handler); }

bool LiveSessionDashboardController::matchesPreparationTarget(const PreparationTargetRequest &request) const
{ return !_currentCampaign.presentationModeActive && _view && _view->matchesPreparationTarget(request); }

void LiveSessionDashboardController::reportPreparationTarget(const PreparationTargetRequest &request,
                                                            const PreparationTargetResult &result)
{ if (matchesPreparationTarget(request)) _view->reportPreparationTarget(request, result); }

void LiveSessionDashboardController::clearPreparationTarget()
{ if (_view) _view->clearPreparationTarget(); }

bool LiveSessionDashboardController::checkpointPreparationNavigation()
{
    checkpointTextRecovery(true);
    const bool outcomes = checkpointOutcomeDrafts();
    return _textRecoveryError.empty() && outcomes;
}

void LiveSessionDashboardController::requestRunSessionTab()
{ if (_view) _view->requestRunSessionTab(); }
