#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

bool EditorWindowController::editWorkspaceMap(EntityId mapId)
{
    _children.mapCreator->setAttachSavedMapToEncounterHandler({});

if (!gm::ux::resolveNavigationEntity(_currentCampaign, mapId).available) return false;
    if (!openWorkspaceTool("show:map_creator", EditorWorkspace::World)) return false;
    view->_worldCategoryFilter = 1;
    view->_workspaceSelectionId = mapId;
    return _children.mapCreator->openMapForEditing(mapId);
}

bool EditorWindowController::openWorkspaceEntity(std::string entityType, EntityId entityId)
{
    struct EntityNavigationScope
    {
        bool &flag;
        bool previous;
        explicit EntityNavigationScope(bool &value) : flag(value), previous(value) { flag = true; }
        ~EntityNavigationScope() { flag = previous; }
    } navigationScope(_workspace.openingEntity);

    // Navigation can commit a draft and replace the campaign. Own the IDs so
    // callers may safely pass references from campaign entities.
    if (entityId.empty() || !gm::ux::resolveNavigationEntity(_currentCampaign, entityId).available)
    {
        view->_workspaceSelectionId.clear();
        _workspace.navigation.current().targetUnavailable = true;
        _shortcuts.statusMessage = uiText("This entry is no longer available. Choose another entry.",
            "Dieser Eintrag ist nicht mehr verfügbar. Wähle einen anderen Eintrag.");
        return false;
    }

    if (entityType == "world_place" || entityType == "WorldPlace" || entityType == "Place")
    {
        if (!WorldInspector::findWorldPlace(_currentCampaign, entityId)) return false;
        if (!view->activateWorkspace(EditorWorkspace::World, true)) return false;
        hideAllWorkspaceTools();
        _workspace.navigation.current().editorRoute.clear();
        _workspace.navigation.current().targetUnavailable = false;
        view->_worldCategoryFilter = 2;
        view->_workspaceSelectionId = entityId;
        view->reloadWorldInspectorDraft(entityId);
        return true;
    }
    if (entityType == "Player" || entityType == "character")
    {
        if (!view->activateWorkspace(EditorWorkspace::Cast, true)) return false;
        view->_workspaceSelectionId = entityId;
        if (!_children.characterSheet->selectCharacterById(entityId))
            return false;
        showCharacterSheet();
        return true;
    }
    if (entityType == "NPC" || entityType == "npc" || entityType == "Monster" || entityType == "monster" ||
        entityType == "Faction" || entityType == "faction" || entityType == "Deity" || entityType == "deity")
    {
        const std::string canonicalType = entityType == "npc" ? "NPC" : entityType == "monster" ? "Monster" :
                                          entityType == "faction" ? "Faction" : entityType == "deity" ? "Deity" : entityType;
        if (!view->activateWorkspace(canonicalType == "Faction" || canonicalType == "Deity" ? EditorWorkspace::World : EditorWorkspace::Cast, true)) return false;
        if (canonicalType == "Faction" || canonicalType == "Deity")
            view->_worldCategoryFilter = 3;
        view->_workspaceSelectionId = entityId;
        if (!_children.npcOverview->selectEntityById(entityId, canonicalType))
            return false;
        showNPCOverview();
        return true;
    }
    if (entityType == "Map" || entityType == "map")
    {
        if (!view->activateWorkspace(EditorWorkspace::World, true)) return false;
        view->_worldCategoryFilter = 1;
        view->_workspaceSelectionId = entityId;
        if (!_children.mapViewer->selectMapById(entityId))
            return false;
        showMapViewer();
        return true;
    }
    if (entityType == "Encounter" || entityType == "encounter")
    {
        if (!_children.battleTracker->canLeaveForPreparation()) { _children.battleTracker->show(); return false; }
        if (!view->activateWorkspace(EditorWorkspace::Encounters, true)) return false;
        view->_workspaceSelectionId = entityId;
        if (!_children.encounterBuilder->selectEncounterById(entityId))
            return false;
        showEncounterBuilder();
        return true;
    }
    if (entityType == "Story Arc" || entityType == "story" || entityType == "Story Beat" || entityType == "scene")
    {
        if (!view->activateWorkspace(EditorWorkspace::Story, true)) return false;
        view->_workspaceSelectionId = entityId;
        bool selected = _children.storyWorkspace->selectThreadById(entityId);
        if (!selected)
        {
            for (const StoryThreadModel &thread : _currentCampaign.storyThreads)
            {
                const auto beat = std::find_if(thread.beats.begin(), thread.beats.end(), [&](const StoryBeatModel &candidate) {
                    return candidate.metadata.id == entityId;
                });
                if (beat == thread.beats.end())
                    continue;
                selected = _children.storyWorkspace->selectThreadById(thread.metadata.id) && _children.storyWorkspace->selectBeatById(entityId);
                break;
            }
        }
        if (!selected)
            return false;
        showStoryWorkspace();
        return true;
    }
    if (entityType == "Item" || entityType == "item")
    {
        if (!view->activateWorkspace(EditorWorkspace::Compendium, true)) return false;
        view->_compendiumCategoryFilter = 1;
        view->_workspaceSelectionId = entityId;
        _children.itemCreator->resumeDraft();
        if (!_children.itemCreator->selectItemById(entityId))
        {
            const auto catalog = availableWorldItems();
            const auto source = std::find_if(catalog.begin(), catalog.end(), [&](const ItemModel &item) { return item.metadata.id == entityId; });
            if (source == catalog.end()) return false;
            _children.itemCreator->copyItemAsDraft(*source);
            view->_workspaceSelectionId.clear();
        }
        openWorkspaceTool("show:item_creator", EditorWorkspace::Compendium);
        return true;
    }
    if (entityType == "Spell" || entityType == "spell")
    {
        if (!view->activateWorkspace(EditorWorkspace::Compendium, true)) return false;
        view->_compendiumCategoryFilter = 2;
        view->_workspaceSelectionId = entityId;
        openWorkspaceTool("show:spell_library", EditorWorkspace::Compendium);
        if (!_children.spellLibrary->selectSpellById(entityId))
            return false;
        return true;
    }
    if (entityType == "Audio" || entityType == "audio")
    {
        if (!view->activateWorkspace(EditorWorkspace::Compendium, true)) return false;
        view->_compendiumCategoryFilter = 3;
        view->_workspaceSelectionId = entityId;
        openWorkspaceTool("show:audio_manager", EditorWorkspace::Compendium);
        for (int index = 0; index < static_cast<int>(_currentCampaign.audioAssets.size()); ++index)
        {
            if (_currentCampaign.audioAssets[static_cast<std::size_t>(index)].metadata.id != entityId)
                continue;
            _children.audioManager->selectAsset(index);
            return true;
        }
        return false;
    }
    if (entityType == "Random Table" || entityType == "random_table")
    {
        if (!view->activateWorkspace(EditorWorkspace::Compendium, true)) return false;
        hideAllWorkspaceTools();
        _workspace.navigation.current().editorRoute.clear();
        view->_compendiumCategoryFilter = 4;
        view->_workspaceSelectionId = entityId;
        view->reloadRandomTableDraft(entityId);
        return !view->_randomTableDraftId.empty();
    }
    if (entityType == "Session")
    {
        if (!view->activateWorkspace(EditorWorkspace::Live, true)) return false;
        return true;
    }

    if (entityType == "Objective" || entityType == "objective")
    {
        if (!view->activateWorkspace(EditorWorkspace::Live, true)) return false;
        view->_workspaceSelectionId = entityId;
        return _children.liveSessionDashboard->focusObjective(entityId);
    }
    if (entityType == "Clue" || entityType == "clue")
    {
        if (!view->activateWorkspace(EditorWorkspace::Live, true)) return false;
        view->_workspaceSelectionId = entityId;
        return _children.liveSessionDashboard->focusClue(entityId);
    }

    if (entityType == "Shop" || entityType == "Dungeon" || entityType == "shop" || entityType == "dungeon")
    {
        if (!view->activateWorkspace(EditorWorkspace::World, true)) return false;
        hideAllWorkspaceTools();
        _workspace.navigation.current().editorRoute.clear();
        _workspace.navigation.current().targetUnavailable = false;
        view->_worldCategoryFilter = entityType == "Shop" || entityType == "shop" ? 4 : 5;
        view->_workspaceSelectionId = entityId;
        return true;
    }

    // Shops and dungeons remain visible as preserved campaign data, but their
    // retired standalone controllers are intentionally not reopened here.
    return false;
}

bool EditorWindowController::workspaceEntityArchived(const std::string &entityType, const EntityId &entityId) const
{
    if (entityId.empty())
        return false;

    const auto containsId = [&](const auto &entities) {
        return std::any_of(entities.begin(), entities.end(), [&](const auto &entity) {
            return entity.metadata.id == entityId;
        });
    };

    const bool supportedEntity = (entityType == "character" && containsId(_currentCampaign.characters)) ||
                                 (entityType == "npc" && containsId(_currentCampaign.npcs)) ||
                                 (entityType == "monster" && containsId(_currentCampaign.customMonsters)) ||
                                 (entityType == "item" && containsId(_currentCampaign.customItems)) ||
                                 (entityType == "random_table" && containsId(_currentCampaign.randomTables));
    return supportedEntity && CampaignTags::isEntityArchived(_currentCampaign, entityId);
}

bool EditorWindowController::setWorkspaceEntityArchived(const std::string &entityType, const EntityId &entityId, bool archived)
{
    if (entityId.empty())
        return false;

    const auto containsId = [&](const auto &entities) {
        return std::any_of(entities.begin(), entities.end(), [&](const auto &entity) {
            return entity.metadata.id == entityId;
        });
    };
    const bool supportedEntity = (entityType == "character" && containsId(_currentCampaign.characters)) ||
                                 (entityType == "npc" && containsId(_currentCampaign.npcs)) ||
                                 (entityType == "monster" && containsId(_currentCampaign.customMonsters)) ||
                                 (entityType == "item" && containsId(_currentCampaign.customItems)) ||
                                 (entityType == "random_table" && containsId(_currentCampaign.randomTables));
    if (!supportedEntity)
        return false;

    EntityMetadata *metadata = CampaignTags::findEntityMetadata(_currentCampaign, entityId);
    if (metadata == nullptr)
        return false;

    const std::vector<std::string> previousTags = metadata->tags;
    const std::string previousEntityUpdatedAt = metadata->updatedAtUtc;
    const std::string previousCampaignUpdatedAt = _currentCampaign.metadata.updatedAtUtc;
    if (!CampaignTags::setEntityArchived(_currentCampaign, entityId, archived))
        return false;

    _currentCampaign.metadata.touch();
    if (persistCurrentCampaign(archived ? "archive workspace entity" : "restore workspace entity"))
        return true;

    metadata->tags = previousTags;
    metadata->updatedAtUtc = previousEntityUpdatedAt;
    _currentCampaign.metadata.updatedAtUtc = previousCampaignUpdatedAt;
    return false;
}

bool EditorWindowController::stageSceneForLive(EntityId sceneId)
{
    if (!flushPreparationDrafts("prepare scene transition")) return false;
    if (!SessionPlan::findScene(_currentCampaign, sceneId)) return false;
    const auto persist = [this](Campaign &staged) {
        const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "stage session scene");
        if (saved) markSearchIndexDirty();
        return saved;
    };
    const auto *next = SessionPlan::nextSession(_currentCampaign);
    EntityId sessionId = next ? next->metadata.id : EntityId{};
    if (sessionId.empty())
    {
        const auto created = SessionPlan::createSession(_currentCampaign, SessionControl::defaultSessionTitle(_currentCampaign), persist);
        if (!created.applied()) return false;
        sessionId = created.sessionId;
    }
    EntityId entryId;
    if (const auto *session = SessionPlan::findSession(_currentCampaign, sessionId))
        for (const auto &entry : session->planEntries)
            if (entry.storyBeatId == sceneId) { entryId = entry.metadata.id; break; }
    if (entryId.empty())
    {
        const auto added = SessionPlan::addScene(_currentCampaign, sessionId, sceneId, persist);
        if (!added.applied()) return false;
        entryId = added.entryId;
    }
    if (SessionPlan::findSession(_currentCampaign, sessionId)->liveModeActive)
    {
        if (!SessionPlan::activateEntry(_currentCampaign, sessionId, entryId, persist).applied()) return false;
        view->activateWorkspace(EditorWorkspace::Live);
    }
    else view->activateWorkspace(EditorWorkspace::Home);
    return true;
}
bool EditorWindowController::stageEncounterForLive(EntityId encounterId)
{
    if (!_children.battleTracker->canLeaveForPreparation()) { _children.battleTracker->show(); return false; }
    if (!flushPreparationDrafts("prepare encounter transition")) return false;
    if (const auto *session = SessionControl::activeSession(_currentCampaign))
    {
        const EntityId id = session->metadata.id;
        const auto result = SessionPlan::activateEncounter(_currentCampaign, id, encounterId, [this](Campaign &staged) {
            const bool saved = ProjectSaveService::getInstance().saveCampaign(staged, "activate session encounter");
            if (saved) markSearchIndexDirty();
            return saved;
        });
        if (!result.applied()) return false;
        if (!_children.battleTracker->refreshSavedPreparationById(encounterId)) return false;
        _children.battleTracker->hide();
        view->activateWorkspace(EditorWorkspace::Live);
        return true;
    }
    const Campaign before = _currentCampaign;
    const auto found = std::find_if(_currentCampaign.encounters.begin(), _currentCampaign.encounters.end(), [&](const EncounterModel &encounter) {
        return encounter.metadata.id == encounterId;
    });
    if (found == _currentCampaign.encounters.end())
        return false;

    _currentCampaign.localPlayerDisplay.publicEncounterId = encounterId;
    _currentCampaign.localPlayerDisplay.showInitiative = true;
    if (!found->mapId.empty())
    {
        _currentCampaign.localPlayerDisplay.publicMapId = found->mapId;
        _currentCampaign.partyLocation.mapId = found->mapId;
    }
    _currentCampaign.localPlayerDisplay.metadata.touch();
    for (SessionModel &session : _currentCampaign.sessions)
    {
        if (!session.liveModeActive)
            continue;
        if (std::find(session.encounterIds.begin(), session.encounterIds.end(), encounterId) == session.encounterIds.end())
            session.encounterIds.push_back(encounterId);
        session.metadata.touch();
        break;
    }
    if (!persistCurrentCampaign("stage encounter for Live combat"))
    {
        _currentCampaign = before;
        return false;
    }
    if (!_children.battleTracker->refreshSavedPreparationById(encounterId)) return false;
    _children.battleTracker->hide();
    _children.liveSessionDashboard->selectPhase(LiveSessionDashboardPhase::Combat);
    view->activateWorkspace(EditorWorkspace::Live);
    return true;
}
