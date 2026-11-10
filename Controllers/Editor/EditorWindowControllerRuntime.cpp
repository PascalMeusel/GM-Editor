#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

void EditorWindowController::quickRollD20()
{
    static std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> die(1, 20);
    const int result = die(generator);
    _shortcuts.statusMessage = uiText("Quick roll d20: ", "Schnellwurf W20: ") + std::to_string(result) + ".";
}

void EditorWindowController::advanceCombatTurn()
{
    showBattleTracker();
    if (_children.battleTracker->advanceTurnFromShortcut())
        _shortcuts.statusMessage = uiText("Advanced combat to the next turn.", "Der Kampf wurde zum nächsten Zug weitergeschaltet.");
    else
        _shortcuts.statusMessage = uiText("Open Run Combat and start combat before using Next Turn.",
                                        "Öffne den Kampfbereich und starte einen Kampf, bevor du ‚Nächster Zug‘ verwendest.");
}

void EditorWindowController::openCampaignNotes()
{
    ImGui::SetWindowFocus("Campaign Home");
    view->_focusCampaignNotesNextFrame = true;
    _shortcuts.statusMessage = uiText("Campaign notes focused.", "Kampagnennotizen sind fokussiert.");
}

void EditorWindowController::toggleAudioMute()
{
    std::string errorMessage;
    if (!_shortcuts.audioMuted)
    {
        _shortcuts.preMuteMasterVolume = std::clamp(_currentCampaign.audioVolumeGroups.master, 0.0f, 1.0f);
        _currentCampaign.audioVolumeGroups.master = 0.0f;
        for (const EntityId &assetId : _children.audioPlaybackService->activeAssetIds())
            _children.audioPlaybackService->setVolume(assetId, 0.0f, errorMessage);
        _shortcuts.audioMuted = true;
        _shortcuts.statusMessage = uiText("Audio muted.", "Audio stummgeschaltet.");
    }
    else
    {
        _currentCampaign.audioVolumeGroups.master = _shortcuts.preMuteMasterVolume <= 0.0f ? 1.0f : _shortcuts.preMuteMasterVolume;
        for (const EntityId &assetId : _children.audioPlaybackService->activeAssetIds())
        {
            auto asset = std::find_if(_currentCampaign.audioAssets.begin(), _currentCampaign.audioAssets.end(), [&](const AudioAssetModel &candidate) {
                return candidate.metadata.id == assetId;
            });
            if (asset != _currentCampaign.audioAssets.end())
                _children.audioPlaybackService->setVolume(assetId, AudioMix::effectiveVolume(_currentCampaign, *asset, asset->defaultVolume), errorMessage);
        }
        _shortcuts.audioMuted = false;
        _shortcuts.statusMessage = uiText("Audio unmuted.", "Audiowiedergabe wieder aktiviert.");
    }

    persistCurrentCampaign(_shortcuts.audioMuted ? "mute audio shortcut" : "unmute audio shortcut");
}

bool EditorWindowController::shouldShowLiveUtilityRail() const
{
    return activeSession() != nullptr || view->_liveUtilityRailExplicitlyOpened;
}

bool EditorWindowController::setCurrentCampaign(Campaign currentCampaign)
{
    if(!checkpointEditorDraftRecoveryForLeave())return false;
    if(!checkpointWorldRecoveryForLeave())return false;
    _workspace.markdownExchange->reset();view->_exchange.reset();
    _workspace.worldHistory->reset();
    revokePreparationTarget();
    ++view->_playerReviewContextEpoch;
    _workspace.playerReviewTarget.reset();
    _workspace.playerReviewError.clear();
    view->_hasPresentationRestoreSnapshot=false; view->_presentationRestoreSnapshot={};
    _children.gmScreen->resetPlayerDisplayReview();
    view->_playerReviewSession.clear();
    view->_presentationReviewAction = EditorWindowView::PresentationReviewAction::None;
    view->_openPresentationArmReviewNextFrame = false;
    _children.mapCreator->prepareForCampaignReplacement();
    if (_children.audioPlaybackService)
        _children.audioPlaybackService->stopAll();
    _currentCampaign = std::move(currentCampaign);
    _partialDraftDiscardError.clear();
    _children.mapCreator->onCampaignReplaced();
    _workspace.navigation.reset(_currentCampaign.metadata.id, activeSession() ? EditorWorkspace::Live : EditorWorkspace::Home);
    // Loading (including reopening the same campaign after discarding) must
    // clear view drafts from the previous in-memory instance.
    view->_sessionWorkspace = SessionWorkspace{};
    _children.resetDraftsAndSelection();
    view->_pendingContextKind = -1;
    _children.configureCampaignEditors();
    _editorDraftRecovery->initialize();
    hideAllWorkspaceTools();
    view->_activeWorkspace = activeSession() ? EditorWorkspace::Live : EditorWorkspace::Home;
    view->_workspaceSelectionId.clear();
    view->_worldDraftEntityId.clear();
    view->clearWorldDrafts(); _worldDraftSaveError.clear();
    view->resetWorldRecovery();
    view->clearDungeonDrafts();
    view->_worldPlaceRoom=false;
    view->_worldNewShopName.clear();view->_worldNewMerchantName.clear();
    view->_handoutCreationDraft=HandoutModel{};view->_handoutCreationDirty=false;
    view->_handoutCreationError.clear();view->_createHandoutRequested=false;
    view->_randomTableDraftId.clear();view->_randomTableDraftDirty=false;view->_randomTableEntryIndex=-1;
    view->_randomTableRollHistory.clear();view->_randomTableStatus.clear();
    view->_campaignNavigationOpen = false;
    view->_presentationPanelOpen = false;
    _children.localPlayerDisplay->onCampaignLoaded();
    InWorldTime::normalizeClock(_currentCampaign.clock);
    EntityRegistry::getInstance().rebuild(_currentCampaign);
    markSearchIndexDirty();
    ProjectCommandManager::getInstance().clear();
    ProjectEventBus::getInstance().publish(ProjectEvent{ProjectEventType::CampaignLoaded,
                                                        _currentCampaign.metadata.id,
                                                        _currentCampaign.metadata.id,
                                                        "campaign",
                                                        "Campaign loaded."});
    view->_focusCampaignHomeNextFrame = activeSession() == nullptr;
    view->_liveUtilityRailExplicitlyOpened = activeSession() != nullptr;
    return true;
}

bool EditorWindowController::discardUnsavedChanges(const std::string &reason)
{
    revokePreparationTarget();
    _partialDraftDiscardError.clear();
    // Each recovery domain discards only after its own checked file operation.
    // A later domain can refuse; its drafts remain open and its error is shown.
    // A disk-only checkpoint predates this editing instance. Discarding other
    // unsaved work must not implicitly discard that unreviewed history.
    if(!_editorDraftRecovery->bound()&&!checkpointEditorDraftRecoveryForLeave())return false;
    const bool worldDraftsWerePresent=view->hasPendingWorldDrafts();
    const bool preservePending=view->_worldRecovery.pending&&!worldDraftsWerePresent;
    if((preservePending&&!view->worldRecoveryBound())||
       (!preservePending&&!view->discardWorldDraftsChecked(WorldInspectorDrafts::Scope::all()))) {
        if(preservePending)view->worldRecoveryReady();
        _shortcuts.statusMessage=view->_worldRecovery.error;return false;
    }
    if(!_editorDraftRecovery->discard()) {
        _shortcuts.statusMessage=_editorDraftRecoveryStatus=_editorDraftRecovery->error();
        if(worldDraftsWerePresent)_partialDraftDiscardError=uiText(
            "World drafts were discarded; item/notes drafts remain open because their discard failed. Review Item & note drafts.",
            "Weltentwürfe wurden verworfen; Gegenstands-/Notizentwürfe bleiben wegen eines Fehlers geöffnet. Prüfe Gegenstands- und Notizentwürfe.");
        return false;
    }
    _workspace.markdownExchange->reset();view->_exchange.reset();
    ++view->_playerReviewContextEpoch;
    _workspace.playerReviewTarget.reset();
    _workspace.playerReviewError.clear();
    _children.npcOverview->resetReadingContext();
    view->_hasPresentationRestoreSnapshot=false; view->_presentationRestoreSnapshot={};
    view->_playerReviewSession.clear();
    view->_presentationReviewAction = EditorWindowView::PresentationReviewAction::None;
    view->_openPresentationArmReviewNextFrame = false;
    _children.gmScreen->resetPlayerDisplayReview();
    view->clearWorldDrafts(); _worldDraftSaveError.clear();
    // Discard is a boundary even while the main menu keeps this editor alive.
    _workspace.worldHistory->reset();
    ProjectCommandManager::getInstance().clear();
    // Never let an old map task publish after the user has left its campaign.
    _children.mapCreator->prepareForCampaignReplacement();
    _children.mapCreator->onCampaignReplaced();
    ProjectSaveService::getInstance().discardUnsavedChanges(reason);
    return true;
}

void EditorWindowController::pollRetiredMapWork()
{
    _children.mapCreator->pollRetiredCampaignWork();
}
