#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

EditorWindowController::EditorWindowController()
{
    view = std::make_unique<EditorWindowView>(*this);
    _workspace.worldHistory=std::make_shared<EditorWorldHistory>(*this);
    _workspace.markdownExchange=std::make_shared<EditorMarkdownExchange>(*this);
    const auto toolkitHistory = std::make_shared<TabletopToolkitHistoryState>();
    _children.audioPlaybackService = std::make_shared<AudioPlaybackService>();
    _children.characterSheet = std::make_shared<CharacterSheetController>(_currentCampaign);
    _children.itemCreator = std::make_shared<ItemCreatorController>(_currentCampaign);
    _editorDraftRecovery=std::make_unique<EditorDraftRecoveryController>(_currentCampaign,*_children.itemCreator,*_children.characterSheet);
    _children.inventoryManager = std::make_shared<InventoryManagerController>(_currentCampaign);
    _children.mapCreator = std::make_shared<MapCreatorController>(_currentCampaign);
    _children.mapViewer = std::make_shared<MapViewerController>(_currentCampaign, _children.audioPlaybackService);
    _children.battlefieldCreator = std::make_shared<BattlefieldCreatorController>(_currentCampaign);
    _children.encounterBuilder = std::make_shared<EncounterBuilderController>(_currentCampaign);
    _children.encounterBuilder->setStageHandler([this](EntityId id) { return stageEncounterForLive(std::move(id)); });
    _children.battleTracker = std::make_shared<BattleTrackerController>(_currentCampaign, _children.audioPlaybackService);
    _children.battleTracker->setHistoryHandler([this](bool redo){return redo?redoCommand():undoCommand();},
        [this]{return _workspace.worldHistory->registrationError();});
    _children.encounterBuilder->setPreparationSaveHandlers(
        [this]() {
            if (_children.battleTracker->canLeaveForPreparation()) return true;
            _children.battleTracker->show();
            return false;
        },
        [this](EntityId id) { _children.battleTracker->onPreparationSaved(id); });
    _children.gmScreen = std::make_shared<GMScreenController>(_currentCampaign, _children.audioPlaybackService, toolkitHistory);
    _children.localPlayerDisplay = std::make_shared<LocalPlayerDisplayController>(_currentCampaign);
    _children.liveSessionDashboard = std::make_shared<LiveSessionDashboardController>(_currentCampaign, _children.audioPlaybackService, toolkitHistory);
    _children.liveSessionDashboard->setPreparationTargetHandler([this](PreparationTargetRequest request) {
        queuePreparationTarget(std::move(request));
    });
    _children.liveSessionDashboard->setPreparationContextRevokedHandler([this]() {
        _workspace.markdownExchange->reset();view->_exchange.reset();
        revokePreparationTarget();
        _workspace.playerReviewTarget.reset();
        _workspace.playerReviewError.clear();
        if (_children.npcOverview) _children.npcOverview->resetReadingContext();
    });
    _children.audioManager = std::make_shared<AudioManagerController>(_currentCampaign, _children.audioPlaybackService);
    _children.audioQuickControls = std::make_shared<AudioQuickControlsController>(_currentCampaign, _children.audioPlaybackService);
    _children.playerCreator = std::make_shared<PlayerCreatorController>(_currentCampaign);
    _children.playerOverview = std::make_shared<PlayerOverviewController>(_currentCampaign);
    _children.npcCreator = std::make_shared<NPCCreatorController>(_currentCampaign);
    _children.npcOverview = std::make_shared<NPCOverviewController>(_currentCampaign);
    _children.npcOverview->setPlayerReviewHandler([this](EntityId id) {
        requestCurrentPlayerReview(PlayerReviewSource::Npc, id);
    });
    _children.itemCreator->setPlayerReviewHandler([this](EntityId id) {
        requestCurrentPlayerReview(PlayerReviewSource::Item, id);
    });
    _children.inventoryManager->setPlayerReviewHandler([this](EntityId id) {
        requestCurrentPlayerReview(PlayerReviewSource::Inventory, id);
    });
    _children.spellLibrary = std::make_shared<SpellLibraryController>(_currentCampaign);
    _children.storyWorkspace = std::make_shared<StoryWorkspaceController>(_currentCampaign, _children.audioPlaybackService);
    _children.itemCreator->setAssignHandler([this](EntityId itemId) {
        EntityId ownerId;
        const auto &cast = _workspace.navigation.location(EditorWorkspace::Cast);
        if ((cast.selectedEntityType == "Player" || cast.selectedEntityType == "NPC") &&
            gm::ux::resolveNavigationEntity(_currentCampaign, cast.selectedEntityId).available) ownerId = cast.selectedEntityId;
        if (_children.inventoryManager->focusItemForAssignment(itemId, ownerId)) showInventoryManager();
    });
    _children.inventoryManager->setEditItemHandler([this](EntityId itemId, bool copy) {
        view->_workspaceSelectionId = _children.inventoryManager->selectedOwnerId();
        if (!copy) { openWorkspaceEntity("Item", itemId); return; }
        const auto items = availableWorldItems();
        const auto found = std::find_if(items.begin(), items.end(), [&](const ItemModel &item) { return item.metadata.id == itemId; });
        if (found == items.end() || !openWorkspaceTool("show:item_creator", EditorWorkspace::Compendium)) return;
        _children.itemCreator->resumeDraft();
        _children.itemCreator->copyItemAsDraft(*found);
        view->_workspaceSelectionId.clear();
    });
    _children.mapCreator->setReviewSavedMapHandler([this](const EntityId &mapId) {
        return prepareSavedMapForPlayerReview(mapId);
    });
    _children.mapCreator->setOpenSavedMapHandler([this](const EntityId &mapId) {
        return openWorkspaceEntity("Map", mapId);
    });
    _children.encounterBuilder->setMapAuthoringHandler([this](EntityId encounterId, EntityId mapId) {
        view->_workspaceSelectionId = encounterId;
        if (!openWorkspaceTool("show:map_creator", EditorWorkspace::World)) return false;
        _children.mapCreator->setAttachSavedMapToEncounterHandler([this, encounterId](const EntityId &savedMapId) {
            const auto *destination = _workspace.navigation.returnDestination();
            if (!destination || destination->workspace != EditorWorkspace::Encounters ||
                destination->selectedEntityId != encounterId) return false;
            if (!_children.encounterBuilder->assignSavedMap(encounterId, savedMapId)) return false;
            return returnToPreviousWorkspace();
        });
        if (mapId.empty()) _children.mapCreator->beginNewMap(MapCreationIntent::TacticalEncounter);
        else if (!_children.mapCreator->openMapForEditing(mapId)) return false;
        return true;
    });
    _children.mapCreator->setAttachSavedMapToSceneHandler([this](const EntityId &mapId) {
        const auto origin = _workspace.navigation.current();
        if (origin.sessionId.empty() || origin.sceneId.empty() || origin.sessionUnavailable ||
            !gm::ux::resolveNavigationEntity(_currentCampaign, origin.sessionId).available ||
            !gm::ux::resolveNavigationEntity(_currentCampaign, origin.sceneId).available) return false;
        if (!view->_sessionWorkspace.restoreSelection(_currentCampaign, origin.sessionId, origin.planEntryId, false,
            [](Campaign &staged) { return ProjectSaveService::getInstance().saveCampaign(staged, "restore map scene destination"); })) return false;
        const bool linked = view->_sessionWorkspace.attachMap(_currentCampaign, mapId, [this](Campaign &staged) {
            const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "attach map to session scene");
            if (saved) markSearchIndexDirty();
            return saved;
        });
        if (linked)
        {
            _children.mapCreator->hide();
            view->activateWorkspace(EditorWorkspace::Home);
            view->_focusCampaignHomeNextFrame = true;
        }
        return linked;
    });
    _children.mapCreator->setOpenEncounterPreparationHandler([this]() {
        if (!_children.battleTracker->canLeaveForPreparation()) { _children.battleTracker->show(); return false; }
        if (!view->activateWorkspace(EditorWorkspace::Encounters)) return false;
        showEncounterBuilder();
        return true;
    });
    _children.playerCreator->setOpenCharacterSheetsHandler([this]() {
        openWorkspaceEntity("Player", _children.playerCreator->getModel().metadata.id);
    });
    _children.playerCreator->setOpenPlayerOverviewHandler([this]() {
        showPlayerOverview();
    });
    _children.playerOverview->setOpenCharacterSheetsHandler([this](const EntityId &characterId) {
        openWorkspaceEntity("Player", characterId);
    });
    _children.playerOverview->setOpenInventoryHandler([this](const EntityId &characterId) {
        if (!_children.inventoryManager->selectPlayerOwnerById(characterId)) return;
        showInventoryManager();
    });
    _children.playerOverview->setOpenMapHandler([this](const EntityId &mapId) {
        openWorkspaceEntity("Map", mapId);
    });
    _children.playerOverview->setOpenLiveSessionHandler([this]() {
        showLiveSessionDashboard();
    });
    _children.npcCreator->setOpenNPCOverviewHandler([this](const EntityId &npcId) {
        if (npcId.empty())
            showNPCOverview();
        else
            openWorkspaceEntity("NPC", npcId);
    });
    _children.characterSheet->setOpenStoryThreadHandler([this](const EntityId &threadId) {
        openWorkspaceEntity("story", threadId);
    });
    _children.npcOverview->setOpenStoryThreadHandler([this](const EntityId &threadId) {
        openWorkspaceEntity("story", threadId);
    });
    _children.liveSessionDashboard->setOpenStoryHandler([this](const EntityId &threadId, const EntityId &beatId) {
        openWorkspaceEntity(beatId.empty() ? "story" : "scene", beatId.empty() ? threadId : beatId);
    });
    _children.liveSessionDashboard->setOpenEntityHandler([this](std::string type, EntityId id) {
        if (type == "Map" || type == "map") editWorkspaceMap(id);
        else openWorkspaceEntity(std::move(type), std::move(id));
    });
    _children.liveSessionDashboard->setOpenMapHandler([this](const EntityId &mapId, bool travel) {
        if (openWorkspaceEntity("Map", mapId) && travel) _children.mapViewer->openTravelWorkflow();
    });
    _children.liveSessionDashboard->setOpenEncounterHandler([this](const EntityId &encounterId) {
        if (encounterId.empty())
        {
            if (!_children.battleTracker->canLeaveForPreparation()) { _children.battleTracker->show(); return; }
            if (!view->activateWorkspace(EditorWorkspace::Encounters)) return;
            EntityId preparationId = _currentCampaign.localPlayerDisplay.publicEncounterId;
            if (preparationId.empty() && !_currentCampaign.encounters.empty())
                preparationId = _currentCampaign.encounters.front().metadata.id;
            if (!preparationId.empty())
                _children.encounterBuilder->selectEncounterById(preparationId);
            showEncounterBuilder();
            return;
        }

        if (_children.battleTracker->selectEncounterById(encounterId))
        {
            _children.battleTracker->hide();
            _children.liveSessionDashboard->selectPhase(LiveSessionDashboardPhase::Combat);
            view->activateWorkspace(EditorWorkspace::Live);
        }
    });
    _children.liveSessionDashboard->setRenderCombatHandler([this](const EntityId &encounterId) {
        if (_children.battleTracker->selectEncounterById(encounterId))
        {
            _children.battleTracker->hide();
            _children.battleTracker->renderEmbedded();
        }
    });
    _children.liveSessionDashboard->setOpenNpcHandler([this]() { showNPCOverview(); });
    _children.liveSessionDashboard->setOpenItemHandler([this]() { showItemCreator(); });
    _children.liveSessionDashboard->setOpenAudioMiniHandler([this]() {
        showAudioQuickControls();
    });
    _children.liveSessionDashboard->setOpenAudioLibraryHandler([this]() {
        showAudioManager();
    });
}
