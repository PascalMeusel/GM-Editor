#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Views/Editor/Widgets/Npc/NPCOverview/NPCOverviewView.hpp"
#include "Views/Editor/Widgets/Combat/BattlefieldCreator/BattlefieldCreatorView.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
// Access the actual controller state in this headless contract fixture.
#include "Controllers/Editor/Widgets/NPCOverview/NPCOverviewController.hpp"
#include "Controllers/Editor/Widgets/BattlefieldCreator/BattlefieldCreatorController.hpp"

namespace {
bool failSave = false;
int saveCalls = 0;
Campaign committed;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
}

// Test doubles replace persistence and window plumbing only. The real controller
// methods below run unchanged; no native windows or audio devices are used.
ProjectSaveService::ProjectSaveService() = default;
ProjectSaveService &ProjectSaveService::getInstance() { static ProjectSaveService service; return service; }
bool ProjectSaveService::saveCampaign(Campaign &campaign, const std::string &) {
    ++saveCalls; _state.dirty = failSave; _state.lastError = failSave ? "injected write failure" : "";
    if (failSave) return false;
    committed = campaign; return true;
}
namespace gm::localization {
LocalizationService &LocalizationService::instance() { static LocalizationService service; return service; }
Locale LocalizationService::locale() const { return Locale::English; }
const char *LocalizationService::uiText(const char *text) const { return text; }
}
DockableWidget::DockableWidget(const std::string &title, ImGuiWindowFlags flags, ImVec2 size)
 : _title(title), _isVisible(false), _dockOnNextUpdate(false), _flags(flags), _defaultSize(size) {}
void DockableWidget::update() {}
void DockableWidget::show() { _isVisible = true; }
void DockableWidget::requestDockOnNextUpdate() {}
bool DockableWidget::isVisible() const { return _isVisible; }
NPCOverviewView::NPCOverviewView(NPCOverviewController &controller) : DockableWidget("test"), _controller(controller) {}
void NPCOverviewView::draw() {}
BattlefieldCreatorView::BattlefieldCreatorView(BattlefieldCreatorController &controller) : DockableWidget("test"), _controller(controller) {}
void BattlefieldCreatorView::draw() {}

struct SecondaryEditorPersistenceProbe {
static int run() {
 try {
    Campaign campaign;
    NPCModel npc; npc.name = "Original"; campaign.npcs.push_back(npc);
    MonsterDefinitionModel monster; monster.name = "Monster"; campaign.customMonsters.push_back(monster);
    committed = campaign;
    NPCOverviewController actors(campaign);
    failSave = true;
    actors.setSelectedNpcName("Edited during failure");
    require(actors._saveFailed && !actors._saveErrorMessage.empty(), "NPC failure not exposed");
    require(campaign.npcs[0].name == "Edited during failure" && committed.npcs[0].name == "Original", "NPC draft/committed state lost");
    failSave = false; actors.retrySave();
    require(!actors._saveFailed && committed.npcs[0].name == "Edited during failure", "NPC retry did not commit retained draft");

    failSave = true; actors.duplicateSelectedNpc();
    const auto duplicateId = campaign.npcs.back().metadata.id;
    require(campaign.npcs.size() == 2 && committed.npcs.size() == 1, "failed duplicate not retained once");
    actors.duplicateSelectedNpc();
    require(campaign.npcs.size() == 2, "failed duplicate retry created a second duplicate");
    failSave = false; actors.duplicateSelectedNpc();
    require(committed.npcs.size() == 2 && committed.npcs.back().metadata.id == duplicateId, "successful repeat did not only retry identity");

    actors._npcResourceDraft.name = "Breath";
    const auto resourceId = actors._npcResourceDraft.metadata.id;
    failSave = true; actors.addResourceTrackerToSelectedNpc();
    require(actors._npcResourceDraft.name == "Breath", "resource draft cleared before persistence");
    actors.addResourceTrackerToSelectedNpc();
    require(campaign.npcs.back().resourceTrackers.size() == 1, "resource added twice on retry");
    failSave = false; actors.retrySave();
    require(actors._npcResourceDraft.metadata.id != resourceId && committed.npcs.back().resourceTrackers[0].metadata.id == resourceId, "resource retry reset or lost wrong identity");

    actors._monsterActionDraft.name = "Claw";
    failSave = true; actors.addActionToSelectedMonster(); actors.addActionToSelectedMonster();
    require(campaign.customMonsters[0].actions.size() == 1 && actors._monsterActionDraft.name == "Claw", "action draft/retry failed");
    failSave = false; actors.retrySave();
    require(committed.customMonsters[0].actions.size() == 1 && actors._monsterActionDraft.name == "Attack", "action retry failed to clear durable draft");

    actors._pendingNpcRemovalIndex = 0;
    failSave = true; actors.deletePendingNpc();
    require(campaign.npcs.size() == 1 && committed.npcs.size() == 2, "delete draft was not retained");
    actors._pendingNpcRemovalIndex = 0; actors.deletePendingNpc();
    require(campaign.npcs.size() == 1, "delete retry removed the next NPC");
    failSave = false; actors.retrySave();
    require(committed.npcs.size() == 1 && committed.npcs[0].metadata.id == duplicateId, "delete retry did not preserve remaining NPC");

    EncounterModel old; old.name = "Existing"; normalizeEncounter(old); campaign.encounters.push_back(old);
    BattlefieldCreatorController battle(campaign);
    const auto newId = battle._draftBattlefield.metadata.id;
    battle._draftBattlefield.name = "New battlefield";
    battle._selectedBattlefieldIndex = 0; // The view chooser defaults to row zero.
    failSave = true; battle.saveBattlefield();
    require(battle._saveFailed && !battle._saveErrorMessage.empty(), "battlefield failure not exposed");
    require(campaign.encounters.size() == 2 && campaign.encounters[0].name == "Existing", "new battlefield overwrote chooser row");
    battle.startNewBattlefield(); battle.loadSelectedBattlefield();
    require(battle._draftBattlefield.metadata.id == newId, "failed battlefield draft was replaced");
    battle._draftBattlefield.name = "Retained and edited";
    battle._selectedBattlefieldIndex = 0;
    failSave = false; battle.saveBattlefield();
    require(!battle._saveFailed && committed.encounters.size() == 2 && committed.encounters[1].metadata.id == newId, "battlefield retry duplicated or overwrote identity");
    require(committed.encounters[1].name == "Retained and edited", "battlefield retry lost edits made after failure");

    EncounterParticipantModel participant; participant.displayName = "Combatant"; participant.currentHitPoints = 7; participant.maxHitPoints = 12;
    campaign.encounters[1].participants.push_back(participant); campaign.encounters[1].round = 6;
    battle._selectedBattlefieldIndex = 0;
    battle.saveBattlefield();
    require(committed.encounters[1].participants.size() == 1 && committed.encounters[1].participants[0].currentHitPoints == 7 && committed.encounters[1].round == 6, "battlefield save overwrote live combat state");
    std::cout << "Secondary editor persistence controller contracts passed (" << saveCalls << " save attempts).\n";
    return 0;
 } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}


};
int main() { return SecondaryEditorPersistenceProbe::run(); }
