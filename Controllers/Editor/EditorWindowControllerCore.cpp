#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

EditorWindowController::~EditorWindowController() { _workspace.markdownExchange->detach();_workspace.worldHistory->detach(); }

void EditorWindowController::hideAllWorkspaceTools()
{
    if (_children.liveSessionDashboard) _children.liveSessionDashboard->checkpointDraftRecovery();
    _children.hideWorkspaceTools();
}

void EditorWindowController::update(std::shared_ptr<GLFWwindow> window)
{
    if (_children.audioPlaybackService)
    {
        std::string audioTransitionError;
        if (!_children.audioPlaybackService->update(ImGui::GetIO().DeltaTime, audioTransitionError) && !audioTransitionError.empty())
        {
            _shortcuts.statusMessage = uiText("Audio transition failed: ", "Audioübergang fehlgeschlagen: ") + audioTransitionError;
            std::cerr << _shortcuts.statusMessage << '\n';
        }
    }
    if (!view->isVisible)
    { _workspace.markdownExchange->poll();return; }
    view->update(window);
    _workspace.markdownExchange->poll();
    processPreparationTarget();
    _editorDraftRecovery->checkpoint(false);
    LocalPlayerDisplayModel &playerDisplay = _currentCampaign.localPlayerDisplay;
    if (playerDisplay.publicCamera.followGmCamera)
    {
        LocalPlayerDisplayCameraState followedCamera;
        // The visible editing canvas is the most specific GM view. Fall back
        // to the read-only viewer when the creator is on another workflow step
        // or another map. Only camera state is copied; reviewed player content
        // and publication fingerprints remain untouched.
        bool cameraFound = _children.mapCreator->isOpen() &&
                           _children.mapCreator->currentCameraForMap(playerDisplay.publicMapId, followedCamera);
        if (!cameraFound && _children.mapViewer->isOpen())
            cameraFound = _children.mapViewer->currentCameraForMap(playerDisplay.publicMapId, followedCamera);
        if (cameraFound)
        {
            followedCamera.followGmCamera = true;
            playerDisplay.publicCamera = followedCamera;
        }
    }
    _children.localPlayerDisplay->update(window);

    updateCampaignAutosave(ProjectSaveService::getInstance());
}

bool EditorWindowController::updateCampaignAutosave(ProjectSaveService &service)
{
    // A fog drag is one transaction; do not persist an intermediate gesture.
    if (_children.mapViewer->transientFogEditActive() || !service.updateAutosave(_currentCampaign)) return false;
    _children.liveSessionDashboard->onLiveCampaignCommitted();
    _children.gmScreen->onLiveCampaignCommitted();
    markSearchIndexDirty();
    return true;
}

WindowTypes EditorWindowController::getWindowType()
{
    return WindowTypes::EditorWindow;
}
bool EditorWindowController::getWindowActive()
{
    return view->isVisible;
}
void EditorWindowController::setWindowOpacity(bool showView)
{
    if(!showView)
    {
        _workspace.markdownExchange->reset();view->_exchange.reset();
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
    view->isVisible = showView;
};

bool EditorWindowController::hasUnsavedChanges() const
{
    const bool campaignDetailsDraftPending = view->_descriptionCampaignId == _currentCampaign.metadata.id &&
                                             (view->_campaignNameDraft != _currentCampaign.name ||
                                              view->_descriptionDraft != _currentCampaign.description);
    return campaignDetailsDraftPending || ProjectSaveService::getInstance().state().dirty || view->hasPendingWorldDrafts() || view->_worldDungeonDirty || !view->_retainedDungeonDrafts.empty() || view->_handoutCreationDirty || view->_randomTableDraftDirty || view->_sessionWorkspace.dirty() ||
           view->_sessionWorkspace.hasPendingOutcomeDraftWrites() || _children.hasUnsavedDrafts();
}

bool EditorWindowController::checkpointOutcomeRecoveryForLeave()
{
    const auto previousError = _outcomeRecoveryError;
    const bool prep = view->_sessionWorkspace.checkpointOutcomeDrafts(_currentCampaign);
    const bool live = _children.liveSessionDashboard->checkpointOutcomeDrafts();
    _outcomeRecoveryError = prep && live ? std::string{} : uiText(
        "Unfinished session text could not be saved to recovery. Your drafts are still open. Check that the campaign folder is writable, then retry Save and Continue or Cancel to keep working.",
        "Unfertiger Sitzungstext konnte nicht zur Wiederherstellung gesichert werden. Deine Entwürfe sind noch geöffnet. Prüfe, ob der Kampagnenordner beschreibbar ist, und wähle erneut Speichern und fortfahren oder Abbrechen.");
    if (!_outcomeRecoveryError.empty()) _shortcuts.statusMessage = _outcomeRecoveryError;
    else if (!previousError.empty() && _shortcuts.statusMessage == previousError) _shortcuts.statusMessage.clear();
    return prep && live;
}

bool EditorWindowController::flushPreparationDrafts(const std::string &reason)
{
    return view->_sessionWorkspace.flush(_currentCampaign, [this, reason](Campaign &staged) {
        const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, reason);
        if (saved) markSearchIndexDirty();
        return saved;
    });
}

bool EditorWindowController::saveCurrentCampaign(const std::string &reason)
{
    if(!_workspace.worldHistory->beforeHistoryAction())return false;
    if(!_editorDraftRecovery->bound()&&!checkpointEditorDraftRecoveryForLeave())return false;
    // Canonical Save can rescue authored World RAM even when checkpoint I/O
    // fails. An untouched disk checkpoint is preserved while unrelated work
    // saves; World mutations still require explicit recovery reconciliation.
    const bool preservePending=view->_worldRecovery.pending&&!view->hasPendingWorldDrafts();
    if((preservePending&&!view->worldRecoveryBound())||(!preservePending&&!view->worldRecoveryReady())) {
        view->worldRecoveryReady();_shortcuts.statusMessage=view->_worldRecovery.error;return false;
    }
    if (!checkpointOutcomeRecoveryForLeave()) return false;
    if (view->_descriptionCampaignId == _currentCampaign.metadata.id &&
        (view->_campaignNameDraft != _currentCampaign.name || view->_descriptionDraft != _currentCampaign.description))
    {
        _shortcuts.statusMessage = uiText("Finish Save details or discard the campaign details draft first.",
                                          "Speichere oder verwirf zuerst den Kampagnendetails-Entwurf.");
        return false;
    }
    // Include independent editor drafts in Ctrl+S and Save and Continue.
    // A failed draft commit keeps the window open and retains that draft.
    if (!flushPreparationDrafts(reason)) return false;
    if (view->hasPendingWorldDrafts() || view->_worldDungeonDirty || !view->_retainedDungeonDrafts.empty())
    {
        const auto result = saveWorldDraft(WorldInspectorDrafts::Scope::all(), reason);
        view->reportWorldInspectorResult(result, "World drafts saved.", "Weltentwürfe gespeichert.");
        if (!result.applied()) return false;
    }
    if (!view->saveHandoutCreationDraft()) return false;
    if (!view->saveRandomTableDraft()) return false;
    if (!_children.liveSessionDashboard->flushDrafts()) return false;
    if (!_children.gmScreen->flushDrafts()) return false;
    if (!_children.characterSheet->flushDraft())
    {
        openWorkspaceTool("show:character_sheet", EditorWorkspace::Cast);
        return false;
    }
    if (!_children.playerCreator->flushDraft())
    {
        openWorkspaceTool("show:player_creator", EditorWorkspace::Cast);
        return false;
    }
    if (!_children.npcCreator->flushDraft())
    {
        openWorkspaceTool("show:npc_creator", EditorWorkspace::Cast);
        return false;
    }
    if (!_children.itemCreator->flushDrafts())
    {
        openWorkspaceTool("show:item_creator", EditorWorkspace::Compendium);
        return false;
    }
    if (!_children.spellLibrary->flushDrafts())
    {
        openWorkspaceTool("show:spell_library", EditorWorkspace::Compendium);
        return false;
    }
    if (!_children.mapCreator->flushDraft())
    {
        _shortcuts.statusMessage = _children.mapCreator->draftStatusMessage();
        return false;
    }
    if (!_children.encounterBuilder->flushDrafts())
    {
        openWorkspaceTool("show:encounter_builder", EditorWorkspace::Encounters);
        return false;
    }
    return persistCurrentCampaign(reason);
}

bool EditorWindowController::persistCurrentCampaign(const std::string &reason)
{
    // An explicit save while the mouse is still held cancels the transient
    // stroke first. This persists the last confirmed map rather than a partial
    // fog gesture.
    if (_children.mapViewer->transientFogEditActive())
        _children.mapViewer->cancelTransientFogEdit();
    const bool saved = ProjectSaveService::getInstance().saveCampaign(_currentCampaign, reason);
    if (saved)
    {
        _children.liveSessionDashboard->onLiveCampaignCommitted();
        _children.gmScreen->onLiveCampaignCommitted();
        markSearchIndexDirty();
    }
    return saved;
}

void EditorWindowController::executeCommand(std::unique_ptr<IProjectCommand> command)
{
    if(!_workspace.worldHistory->beforeHistoryAction())return;
    ProjectCommandManager::getInstance().execute(std::move(command));
    markSearchIndexDirty();
}

bool EditorWindowController::undoCommand()
{
    if(!_workspace.worldHistory->beforeHistoryAction())return false;
    const bool undone = ProjectCommandManager::getInstance().undo();
    if (undone)
        markSearchIndexDirty();
    return undone;
}

bool EditorWindowController::redoCommand()
{
    if(!_workspace.worldHistory->beforeHistoryAction())return false;
    const bool redone = ProjectCommandManager::getInstance().redo();
    if (redone)
        markSearchIndexDirty();
    return redone;
}

std::vector<CampaignSearchResult> EditorWindowController::searchCurrentCampaign(const std::string &query, std::size_t limit) const
{
    ensureSearchIndex();
    return _workspace.cachedSearchIndex.search(query, limit);
}

std::vector<std::string> EditorWindowController::tagsForEntity(const EntityId &entityId) const
{
    return CampaignTags::tagsForEntity(_currentCampaign, entityId);
}

std::vector<std::string> EditorWindowController::allCampaignTags() const
{
    return CampaignTags::collectAllTags(_currentCampaign);
}

bool EditorWindowController::setEntityTags(const EntityId &entityId, const std::vector<std::string> &tags, const std::string &saveReason)
{
    if (!CampaignTags::setTagsForEntity(_currentCampaign, entityId, tags))
        return false;

    persistCurrentCampaign(saveReason);
    return true;
}

std::vector<CampaignBacklink> EditorWindowController::backlinksToEntity(const EntityId &entityId) const
{
    return CampaignReferences::backlinksTo(_currentCampaign, entityId);
}

std::vector<CampaignBrokenReference> EditorWindowController::brokenReferences() const
{
    return CampaignReferences::brokenReferences(_currentCampaign);
}

CampaignKnowledgeGraph EditorWindowController::knowledgeGraph() const
{
    return CampaignGraph::buildKnowledgeGraph(_currentCampaign);
}

std::vector<CampaignCommandPaletteItem> EditorWindowController::commandPaletteItems(const std::string &query) const
{
    ensureSearchIndex();
    return CampaignCommandPalette::filterItems(CampaignCommandPalette::buildItems(_workspace.cachedSearchIndex), query, 80);
}
