#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Audio/AudioBackend.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
Campaign committed; int saveCalls=0, playCalls=0; bool failSave=false, throwSave=false, failAudio=false;
std::filesystem::path fixture; int expectedRound=1;
void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
class RecordingBackend final : public AudioCommandBackend {
public:
 const char *name() const override { return "Wrap-up recording backend"; }
 bool sendCommand(const std::string &command,std::string &error) const override {
  if(command.rfind("play ",0)==0) {
   require(!committed.encounters.empty() && committed.encounters[0].combatPhase==CombatPhase::ActiveTurn &&
      committed.encounters[0].round==expectedRound && committed.encounters[0].activeParticipantIndex==0 &&
      !committed.encounters[0].combatLog.empty() && committed.npcs[0].resourceTrackers[0].used==0,
      "audio command preceded the durable complete turn/resource snapshot");
   ++playCalls;
   if(failAudio) { error="injected audio failure";return false; }
  }
  error.clear();return true;
 }
 bool queryCommand(const std::string &,std::string &result,std::string &error) const override {
  result="1000";error.clear();return true;
 }
};
}
// Test seams: window lifetime, no-ruleset synchronization, durable storage,
// asset path resolution and command backend. Wrap-up, selection, persistence,
// matching triggers and playback commands are actual production methods.
BattleTrackerController::BattleTrackerController(Campaign &campaign,std::shared_ptr<AudioPlaybackService> playback)
 : _currentCampaign(campaign),_playbackService(std::move(playback)) {}
void BattleTrackerController::update(std::shared_ptr<GLFWwindow>) {}
void BattleTrackerController::onShow() {}
void BattleTrackerController::synchronizeParticipantsWithSources() {
 require(!_currentCampaign.ruleSet,"fixture cannot bypass active rules synchronization");
}
ProjectSaveService::ProjectSaveService()=default;
ProjectSaveService &ProjectSaveService::getInstance(){static ProjectSaveService service;return service;}
bool ProjectSaveService::saveCampaign(Campaign &campaign,const std::string &){
 ++saveCalls;if(throwSave)throw std::runtime_error("injected disk exception");
 if(failSave)return false;committed=campaign;return true;
}
namespace ProjectAssets { std::filesystem::path resolveAudioPath(const AudioAssetModel &){return fixture;} }
namespace gm::localization {
LocalizationService &LocalizationService::instance(){static LocalizationService service;return service;}
const char *LocalizationService::uiText(const char *text)const{return text;}
Locale LocalizationService::locale()const{return Locale::English;}
}

struct BattleTrackerTurnsProbe {
 static Campaign makeCampaign(bool roundBoundary) {
  Campaign campaign;NPCModel npc;ResourceTrackerModel resource;resource.name="Reaction";resource.maximum=1;resource.used=1;resource.rechargeRule="Each turn";
  npc.resourceTrackers.push_back(resource);npc.conditions.push_back("Spell Effect: Shield");campaign.npcs.push_back(npc);
  EncounterModel encounter;encounter.name="Turn boundary";encounter.round=1;
  encounter.combatPhase=roundBoundary?CombatPhase::ActionResolution:CombatPhase::Setup;
  EncounterParticipantModel first;first.displayName="First";first.kind=CombatantKind::NPC;first.sourceEntityId=npc.metadata.id;
  first.initiative=18;first.currentHitPoints=8;first.maxHitPoints=10;first.startingX=3;first.startingY=4;
  first.x=7;first.y=8;first.movementUsedFeet=15;first.conditions=npc.conditions;
  ActiveEffectModel effect;effect.name="Shield";effect.remainingRounds=1;first.activeEffects.push_back(effect);
  EncounterParticipantModel second;second.displayName="Second";second.initiative=4;second.currentHitPoints=9;second.maxHitPoints=10;
  if(roundBoundary){encounter.participants={first,second};encounter.activeParticipantIndex=1;}
  else encounter.participants={second,first}; // exercise real stable initiative sort
  campaign.encounters.push_back(encounter);
  AudioAssetModel asset;asset.name="Transition";asset.filePath=fixture.string();campaign.audioAssets.push_back(asset);
  for(const std::string event : {"Combat Started","Round Started"}) {
   AudioTriggerModel trigger;trigger.sourceType="CombatEvent";trigger.eventName=event;
   trigger.sourceEntityId=encounter.metadata.id;trigger.audioAssetId=asset.metadata.id;campaign.audioTriggers.push_back(trigger);
  }
  return campaign;
 }
 static int run(){try {
  fixture=std::filesystem::temp_directory_path()/("gm_turns_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".wav");
  {std::ofstream out(fixture,std::ios::binary);out<<"test";}
  int totalSaves=0;
  for(int action=0;action<3;++action)for(int failure=0;failure<3;++failure){
   saveCalls=playCalls=0;failSave=throwSave=failAudio=false;expectedRound=action?2:1;
   Campaign campaign=makeCampaign(action!=0);committed=campaign;
   auto playback=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());
   BattleTrackerController tracker(campaign,playback);tracker.selectEncounterById(campaign.encounters[0].metadata.id);
   if(!action)tracker._initiativeLocked=false;
   auto invoke=[&]{if(!action)tracker.beginCombat();else if(action==1)tracker.nextTurn();else tracker.skipTurn();};
   failSave=failure==1;throwSave=failure==2;invoke();
   require(saveCalls==1 && !tracker._deferCombatSave,"transition used intermediate saves or retained batch flag");
   require(tracker._draftEncounter.round==expectedRound && tracker._draftEncounter.activeParticipantIndex==0 &&
      tracker._draftEncounter.participants[0].displayName=="First" &&
      tracker._draftEncounter.participants[0].movementUsedFeet==0 && campaign.npcs[0].resourceTrackers[0].used==0,
      "turn transition lost initiative, movement reset or source recharge");
   if(action)require(tracker._draftEncounter.participants[0].activeEffects.empty() && campaign.npcs[0].conditions.empty(),
      "round boundary did not expire effect and canonical condition");
   else require(tracker._draftEncounter.participants[0].x==3 && tracker._draftEncounter.participants[0].y==4,
      "combat start did not restore starting position");
   const auto pendingLog=tracker._draftEncounter.combatLog.size();
   if(failure){
    require(playCalls==0 && tracker._encounterSaveFailed && committed.encounters[0].round==1 &&
      committed.encounters[0].combatLog.empty() && committed.npcs[0].resourceTrackers[0].used==1 &&
      committed.encounters[0].participants[action?0:1].activeEffects.size()==1,
      "failed transition changed durable round/effects/resources or played audio");
    invoke();require(saveCalls==1 && tracker._draftEncounter.combatLog.size()==pendingLog &&
      tracker._draftEncounter.round==expectedRound,"repeat failed transition advanced/logged again");
    require(!tracker.advanceTurnFromShortcut() && saveCalls==1,"shortcut bypassed pending failure");
    failSave=throwSave=false;tracker.saveEncounter();
    require(!tracker._encounterSaveFailed && playCalls==0 && committed.encounters[0].round==expectedRound,
      "silent Retry did not persist exact pending transition");
   }else require(playCalls==1,"successful start/round transition omitted or duplicated audio");
   if(!action){
    const int saves=saveCalls;invoke();require(saveCalls==saves && tracker._draftEncounter.combatLog.size()==pendingLog,
      "repeated Start reset active combat");
   }
   if(action){
    const int plays=playCalls;tracker.nextTurn();
    require(tracker._draftEncounter.round==2 && tracker._draftEncounter.activeParticipantIndex==1 && playCalls==plays,
      "non-boundary next turn played Round Started or advanced twice");
   }
   totalSaves+=saveCalls;
  }
  saveCalls=playCalls=0;failSave=throwSave=false;failAudio=true;expectedRound=2;
  Campaign campaign=makeCampaign(true);committed=campaign;
  auto playback=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());
  BattleTrackerController tracker(campaign,playback);tracker.selectEncounterById(campaign.encounters[0].metadata.id);tracker.nextTurn();
  require(committed.encounters[0].round==2 && !tracker._encounterSaveFailed && playCalls==1 && !tracker._combatErrorMessage.empty(),
      "post-commit audio failure lost round or hid feedback");
  tracker._draftEncounter.combatPhase=CombatPhase::PausedBreak;const int saves=saveCalls;tracker.nextTurn();
  require(saveCalls==saves && tracker._draftEncounter.round==2,"paused combat allowed next turn");
  std::filesystem::remove(fixture);
  std::cout<<"BattleTracker start/next/skip controller + audio contracts passed ("<<totalSaves+saveCalls<<" save attempts).\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';if(!fixture.empty())std::filesystem::remove(fixture);return 1;}}
};
int main(){return BattleTrackerTurnsProbe::run();}
