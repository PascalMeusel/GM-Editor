#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

void EditorWindowController::showPlayerCreator()
{
if (!openWorkspaceTool("show:player_creator", EditorWorkspace::Cast)) return;
    _children.playerCreator->resumeDraft();
    view->_workspaceSelectionId.clear();
}

void EditorWindowController::showNPCCreator()
{
if (!openWorkspaceTool("show:npc_creator", EditorWorkspace::Cast)) return;
    _children.npcCreator->resumeDraft();
    view->_workspaceSelectionId.clear();
}

void EditorWindowController::showPlayerOverview()
{
openWorkspaceTool("show:player_overview", EditorWorkspace::Cast);
}

void EditorWindowController::showCharacterSheet()
{
openWorkspaceTool("show:character_sheet", EditorWorkspace::Cast);
}

void EditorWindowController::showNPCOverview()
{
openWorkspaceTool("show:npc_overview", view->_activeWorkspace == EditorWorkspace::World ? EditorWorkspace::World : EditorWorkspace::Cast);
}

void EditorWindowController::showItemCreator()
{
if (!openWorkspaceTool("show:item_creator", EditorWorkspace::Compendium)) return;
    _children.itemCreator->resumeDraft();
    view->_workspaceSelectionId.clear();
}

void EditorWindowController::showItemCatalog()
{
    if (!openWorkspaceTool("show:item_creator", EditorWorkspace::Compendium)) return;
    _children.itemCreator->resumeDraft();
    _children.itemCreator->requestCatalogBrowser();
    view->_workspaceSelectionId.clear();
}

void EditorWindowController::showInventoryManager()
{
if (openWorkspaceTool("show:inventory_manager", EditorWorkspace::Cast))
        view->_workspaceSelectionId = _children.inventoryManager->selectedOwnerId();
}

void EditorWindowController::showMapCreator(bool newMap)
{
    _children.mapCreator->setAttachSavedMapToEncounterHandler({});

if (!openWorkspaceTool("show:map_creator", EditorWorkspace::World)) return;
    if (newMap) { view->_workspaceSelectionId.clear(); _children.mapCreator->beginNewMap(); }
}

bool EditorWindowController::createMapFromWorldPlace(const EntityId &placeId)
{
    _children.mapCreator->setAttachSavedMapToEncounterHandler({});

if (!gm::ux::resolveNavigationEntity(_currentCampaign, placeId).available) return false;
    if (!openWorkspaceTool("show:map_creator", EditorWorkspace::World)) return false;
    return _children.mapCreator->beginSettlementFromWorldPlace(placeId);
}

void EditorWindowController::showMapViewer()
{
openWorkspaceTool("show:map_viewer", EditorWorkspace::World);
}

void EditorWindowController::showDungeonBuilder()
{
    if (!view->activateWorkspace(EditorWorkspace::World)) return;
    hideAllWorkspaceTools();
    _workspace.navigation.current().editorRoute.clear();
    view->_worldCategoryFilter = 5;
    if (!_currentCampaign.dungeons.empty())
        view->_workspaceSelectionId = _currentCampaign.dungeons.front().metadata.id;
}

void EditorWindowController::showBattlefieldCreator()
{
    if (!_children.battleTracker->canLeaveForPreparation()) { _children.battleTracker->show(); return; }
    if (!view->activateWorkspace(EditorWorkspace::Encounters)) return;
    showEncounterBuilder();
}

void EditorWindowController::showNewEncounterBuilder()
{
if (!_children.battleTracker->canLeaveForPreparation()) { showBattleTracker(); return; }
    if (!openWorkspaceTool("show:encounter_builder", EditorWorkspace::Encounters)) return;
    _children.encounterBuilder->beginNewEncounterDraft();
}

void EditorWindowController::showEncounterBuilder()
{
if (!_children.battleTracker->canLeaveForPreparation()) { showBattleTracker(); return; }
    openWorkspaceTool("show:encounter_builder", EditorWorkspace::Encounters);
}

void EditorWindowController::showBattleTracker()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    EntityId encounterId = _currentCampaign.localPlayerDisplay.publicEncounterId;
    const auto matchesEncounter = [&](const EntityId &id) {
        return std::any_of(_currentCampaign.encounters.begin(), _currentCampaign.encounters.end(), [&](const EncounterModel &encounter) {
            return encounter.metadata.id == id;
        });
    };
    if (!matchesEncounter(encounterId))
    {
        const auto running = std::find_if(_currentCampaign.encounters.begin(), _currentCampaign.encounters.end(), [](const EncounterModel &encounter) {
            return encounter.combatPaused ||
                   (encounter.combatPhase != CombatPhase::Setup && encounter.combatPhase != CombatPhase::Initiative) ||
                   encounter.round > 1 || encounter.activeParticipantIndex > 0 || !encounter.combatLog.empty();
        });
        if (running != _currentCampaign.encounters.end())
            encounterId = running->metadata.id;
        else if (!_currentCampaign.encounters.empty())
            encounterId = _currentCampaign.encounters.front().metadata.id;
    }
    if (!encounterId.empty())
        _children.battleTracker->selectEncounterById(encounterId);
    _children.battleTracker->hide();
    _children.liveSessionDashboard->selectPhase(LiveSessionDashboardPhase::Combat);
}

void EditorWindowController::showGMScreen()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
}

void EditorWindowController::showHandoutCreator()
{
    if(!view->activateWorkspace(EditorWorkspace::Compendium))return;
    hideAllWorkspaceTools();
    _workspace.navigation.current().editorRoute.clear();
    view->_createHandoutRequested=true;
}

void EditorWindowController::showLiveSessionDashboard()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
}

void EditorWindowController::showTabletopToolkit()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    _children.liveSessionDashboard->openTabletopToolkit();
}

void EditorWindowController::showTabletopObjectives()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    _children.liveSessionDashboard->openObjectives();
}

void EditorWindowController::showTabletopClues()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    _children.liveSessionDashboard->openClues();
}

LiveSessionDashboardPhase EditorWindowController::liveSessionPhase() const
{
    return _children.liveSessionDashboard->selectedPhase();
}

void EditorWindowController::setLiveSessionPhase(LiveSessionDashboardPhase phase)
{
    _children.liveSessionDashboard->selectPhase(phase);
}

void EditorWindowController::renderLiveWorkspace()
{
    _children.liveSessionDashboard->renderEmbedded();
}
