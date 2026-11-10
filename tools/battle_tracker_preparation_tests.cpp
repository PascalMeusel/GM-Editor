#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderController.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include <iostream>
#include <stdexcept>
namespace { bool failSave = false; Campaign committed; int saveCalls = 0;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); } }

// Real selection/load/persist controller methods are linked below. Only window
// lifetime, source-rule synchronization and the disk boundary are test doubles.
// This fixture has no ruleset, matching the production synchronization no-op.
BattleTrackerController::BattleTrackerController(Campaign &campaign, std::shared_ptr<AudioPlaybackService> playback)
 : _currentCampaign(campaign), _playbackService(std::move(playback)) {}
void BattleTrackerController::update(std::shared_ptr<GLFWwindow>) {}
void BattleTrackerController::onShow() {}
void BattleTrackerController::synchronizeParticipantsWithSources() {
    require(!_currentCampaign.ruleSet, "fixture must not bypass active rules synchronization");
}
EncounterBuilderController::EncounterBuilderController(Campaign &campaign) : _currentCampaign(campaign) {}
EncounterBuilderController::~EncounterBuilderController() = default;
void DockableWidget::show() {}
void DockableWidget::requestDockOnNextUpdate() {}
void EncounterBuilderController::update(std::shared_ptr<GLFWwindow>) {}
void EncounterBuilderController::loadSelectedEncounter() {
 if (_selectedEncounterIndex>=0 && _selectedEncounterIndex<int(_currentCampaign.encounters.size()))
  _draftEncounter=_currentCampaign.encounters[_selectedEncounterIndex];
}
void EncounterBuilderController::synchronizeParticipantsWithSources() {
    require(!_currentCampaign.ruleSet, "fixture must not bypass preparation rules synchronization");
}
ProjectSaveService::ProjectSaveService() = default;
ProjectSaveService &ProjectSaveService::getInstance() { static ProjectSaveService service; return service; }
bool ProjectSaveService::saveCampaign(Campaign &campaign, const std::string &) {
    ++saveCalls; _state.dirty = failSave; _state.lastError = failSave ? "injected failure" : "";
    if (failSave) return false; committed = campaign; return true;
}
namespace gm::localization {
LocalizationService &LocalizationService::instance() { static LocalizationService service; return service; }
const char *LocalizationService::uiText(const char *text) const { return text; }
Locale LocalizationService::locale() const { return Locale::English; }
}

struct BattleTrackerPreparationProbe {
static int run() {
 try {
    Campaign campaign;
    EncounterModel encounter; encounter.name = "Same encounter";
    encounter.battlefieldWidth = 40; encounter.battlefieldHeight = 30;
    encounter.mapId = "old-map"; encounter.tacticalMapId = "old-map";
    encounter.round = 2; encounter.combatPhase = CombatPhase::ActionResolution;
    EncounterParticipantModel actor; actor.displayName = "Light spirit";
    actor.currentHitPoints = 6; actor.maxHitPoints = 10;
    encounter.participants.push_back(actor); normalizeEncounter(encounter);
    campaign.encounters.push_back(encounter);
    MapModel oldMap; oldMap.metadata.id="old-map"; campaign.maps.push_back(oldMap);
    MapModel newMap; newMap.metadata.id="new-map"; campaign.maps.push_back(newMap);
    EncounterBuilderController draftBuilder(campaign);
    draftBuilder._selectedEncounterIndex=0; draftBuilder._draftEncounter=encounter;
    require(draftBuilder.beginNewEncounterDraft(), "Explicit new encounter was rejected");
    const EntityId newDraftId=draftBuilder._draftEncounter.metadata.id;
    draftBuilder.show();
    require(draftBuilder._selectedEncounterIndex==-1 && newDraftId!=encounter.metadata.id &&
            draftBuilder._draftEncounter.metadata.id==newDraftId && draftBuilder._draftEncounter.participants.empty() &&
            draftBuilder._draftEncounter.afterCombatSummary.empty() && draftBuilder._draftEncounter.round==1,
            "Showing explicit New reloaded an existing encounter or its combat contents");
    require(campaign.encounters.size()==1 && campaign.encounters[0].metadata.id==encounter.metadata.id &&
            campaign.encounters[0].participants[0].currentHitPoints==6, "New draft mutated the saved encounter");
    draftBuilder._saveFailed=true; draftBuilder._draftEncounter.name="Retained failed draft";
    require(!draftBuilder.beginNewEncounterDraft(), "New discarded a failed draft");
    draftBuilder.show(); draftBuilder.startNewEncounter();
    require(draftBuilder._draftEncounter.metadata.id==newDraftId && draftBuilder._draftEncounter.name=="Retained failed draft" &&
            draftBuilder._saveFailed, "New/show lost pending failure or Retry draft");
    draftBuilder._saveFailed=false; draftBuilder._selectedEncounterIndex=0; draftBuilder._draftEncounter=encounter;
    draftBuilder.show();
    require(draftBuilder._draftEncounter.metadata.id==encounter.metadata.id, "Existing encounter editing was redirected to New");
    committed = campaign;
    BattleTrackerController tracker(campaign);
    require(tracker.selectEncounterById(encounter.metadata.id), "initial selection failed");
    require(tracker._draftEncounter.battlefieldWidth == 40, "initial battlefield wrong");
    tracker._selectedParticipantIndex = 0;

    // Simulate the already-successful EncounterBuilder save, preserving ID.
    auto &saved = campaign.encounters[0];
    resizeEncounterBattlefield(saved,80,80);
    saved.mapId="new-map";saved.tacticalMapId="new-map";
    auto &edge = *encounterCellAt(saved,79,79);
    edge.kind=MapTileKind::Cover;edge.elevation=4;edge.blocksLineOfSight=true;
    saved.participants[0].x=79;saved.participants[0].y=79;
    committed=campaign;
    require(tracker.selectEncounterById(encounter.metadata.id) && tracker._draftEncounter.battlefieldWidth==40,
            "ordinary per-frame selection must not replace a same-ID editable draft");
    require(tracker.refreshSavedPreparationById(encounter.metadata.id), "explicit saved handoff failed");
    require(tracker._draftEncounter.battlefieldWidth==80 && tracker._draftEncounter.battlefieldHeight==80 &&
            tracker._draftEncounter.battlefieldCells.size()==6400 && tracker._draftEncounter.mapId=="new-map" &&
            tracker._draftEncounter.tacticalMapId==std::optional<EntityId>("new-map"), "saved preparation geometry/map identity not refreshed");
    const auto *loadedEdge=encounterCellAt(tracker._draftEncounter,79,79);
    require(loadedEdge && loadedEdge->kind==MapTileKind::Cover && loadedEdge->elevation==4 && loadedEdge->blocksLineOfSight,
            "edge terrain was cropped or reinterpreted during refresh");
    require(tracker._draftEncounter.round==2 && tracker._draftEncounter.participants[0].currentHitPoints==6 &&
            tracker._draftEncounter.participants[0].x==79 && tracker._selectedParticipantIndex==0 && tracker._selectedMapIndex==1,
            "refresh lost combat continuity or selected map/actor");
    tracker._draftEncounter.participants[0].temporaryHitPoints=3;
    tracker.persistEncounterDraft();
    require(committed.encounters[0].battlefieldWidth==80 && committed.encounters[0].mapId=="new-map" &&
            committed.encounters[0].participants[0].temporaryHitPoints==3,
            "next combat mutation restored stale preparation");

    failSave=true;
    tracker._draftEncounter.participants[0].currentHitPoints=4;
    tracker.persistEncounterDraft();
    require(tracker._encounterSaveFailed && committed.encounters[0].participants[0].currentHitPoints==6,
            "failed combat save did not preserve committed HP");
    require(!tracker.canLeaveForPreparation() && !tracker.refreshSavedPreparationById(encounter.metadata.id),
            "pending combat save allowed preparation or destructive refresh");
    require(tracker._draftEncounter.participants[0].currentHitPoints==4 && tracker._encounterSaveFailed,
            "refresh discarded a failed-save combat draft");
    EncounterModel other;campaign.encounters.push_back(other);
    require(!tracker.selectEncounterById(other.metadata.id) && tracker._draftEncounter.metadata.id==encounter.metadata.id,
            "selecting another encounter discarded failed-save draft");
    require(tracker.selectEncounterById(encounter.metadata.id) && tracker._draftEncounter.participants[0].currentHitPoints==4,
            "embedded redraw overwrote failed-save health");
    failSave=false;tracker.persistEncounterDraft();
    require(!tracker._encounterSaveFailed && tracker.canLeaveForPreparation() &&
            committed.encounters[0].participants[0].currentHitPoints==4 && committed.encounters[0].battlefieldWidth==80,
            "combat retry lost pending HP or saved prep");
    require(!tracker.refreshSavedPreparationById("missing") && tracker._draftEncounter.metadata.id==encounter.metadata.id,
            "missing saved encounter destroyed current context");
    // Exercise real Save Encounter Prep followed by ordinary Live navigation.
    EncounterBuilderController builder(campaign);
    builder._draftEncounter=campaign.encounters[0];builder._selectedEncounterIndex=0;
    int notifications=0;
    builder.setPreparationSaveHandlers([&] {return tracker.canLeaveForPreparation();},
        [&](EntityId id) {
            require(committed.encounters[0].mapId==builder._draftEncounter.mapId,
                    "preparation notification happened before durable save");
            ++notifications;tracker.onPreparationSaved(id);
        });
    resizeEncounterBattlefield(builder._draftEncounter,52,38);
    builder._draftEncounter.mapId="third-map";builder._draftEncounter.tacticalMapId="third-map";
    require(builder.saveEncounter() && notifications==1, "Save-only preparation did not notify after success");
    require(tracker.selectEncounterById(encounter.metadata.id) && tracker._draftEncounter.battlefieldWidth==52 &&
            tracker._draftEncounter.mapId=="third-map", "ordinary Live return retained stale same-ID preparation");
    tracker._draftEncounter.participants[0].temporaryHitPoints=5;tracker.persistEncounterDraft();
    require(committed.encounters[0].battlefieldWidth==52 && committed.encounters[0].mapId=="third-map",
            "ordinary return then combat save overwrote saved map");

    resizeEncounterBattlefield(builder._draftEncounter,60,45);
    builder._draftEncounter.mapId="retry-map";builder._draftEncounter.tacticalMapId="retry-map";
    failSave=true;
    require(!builder.saveEncounter() && notifications==1 && builder._draftEncounter.battlefieldWidth==60,
            "failed preparation save notified or discarded draft");
    require(tracker._draftEncounter.battlefieldWidth==52 && committed.encounters[0].battlefieldWidth==52,
            "failed preparation save replaced clean committed Live draft");
    failSave=false;require(builder.saveEncounter() && notifications==2 && tracker._draftEncounter.battlefieldWidth==60,
            "successful prep retry failed to refresh same-ID combat");

    tracker._draftEncounter.participants[0].currentHitPoints=2;
    tracker._draftEncounter.round=5;
    CombatLogEntryModel log;log.summary="Damage while preparation open";tracker._draftEncounter.combatLog.push_back(log);
    failSave=true;tracker.persistEncounterDraft();
    resizeEncounterBattlefield(builder._draftEncounter,80,80);
    builder._draftEncounter.mapId="after-combat-retry";
    const auto callsBeforeGuard=saveCalls;
    require(!builder.saveEncounter() && saveCalls==callsBeforeGuard && notifications==2,
            "pending combat failure allowed a conflicting preparation save");
    require(tracker._draftEncounter.participants[0].currentHitPoints==2 && builder._draftEncounter.battlefieldWidth==80,
            "conflict guard discarded combat or preparation draft");
    failSave=false;tracker.persistEncounterDraft();
    require(builder.saveEncounter() && notifications==3 && tracker._draftEncounter.battlefieldWidth==80 &&
            tracker._draftEncounter.participants[0].currentHitPoints==2 && tracker._draftEncounter.round==5 &&
            tracker._draftEncounter.combatLog.back().summary==log.summary,
            "stale preparation rolled back current combat HP/round/log after retry");
    require(builder._tacticalStatus.empty(), "resolved combat conflict left a stale preparation error");
    const auto activeId=tracker._draftEncounter.metadata.id;
    tracker.onPreparationSaved(other.metadata.id);
    require(tracker._draftEncounter.metadata.id==activeId, "saving another preparation stole active combat selection");
    std::cout<<"BattleTracker saved preparation controller contracts passed ("<<saveCalls<<" save attempts).\n";
    return 0;
 } catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
};
int main(){return BattleTrackerPreparationProbe::run();}
