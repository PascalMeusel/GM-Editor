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
std::filesystem::path fixture;
void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
class RecordingBackend final : public AudioCommandBackend {
public:
 const char *name() const override { return "Wrap-up recording backend"; }
 bool sendCommand(const std::string &command,std::string &error) const override {
  if(command.rfind("play ",0)==0) {
   require(!committed.encounters.empty() && committed.encounters[0].combatPhase==CombatPhase::WrapUp &&
      !committed.encounters[0].afterCombatSummary.empty() && committed.encounters[0].combatLog.size()==1,
      "audio command preceded the durable complete wrap-up snapshot");
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
struct BattleTrackerWrapUpProbe {
 static Campaign makeCampaign(){
  Campaign campaign;EncounterModel encounter;encounter.name="Forest ambush";
  encounter.combatPhase=CombatPhase::ActionResolution;encounter.round=3;
  EncounterParticipantModel actor;actor.displayName="Defeated bandit";actor.currentHitPoints=0;actor.maxHitPoints=10;actor.defeated=true;
  encounter.participants.push_back(actor);encounter.afterCombatLootNotes="Silver key";
  campaign.encounters.push_back(encounter);
  AudioAssetModel asset;asset.name="Victory";asset.filePath=fixture.string();campaign.audioAssets.push_back(asset);
  AudioTriggerModel trigger;trigger.sourceType="CombatEvent";trigger.eventName="Combat Ended";
  trigger.sourceEntityId=encounter.metadata.id;trigger.audioAssetId=asset.metadata.id;campaign.audioTriggers.push_back(trigger);
  return campaign;
 }
 static void reset(){saveCalls=playCalls=0;failSave=throwSave=failAudio=false;}
 static int run(){try{
  fixture=std::filesystem::temp_directory_path()/("gm_wrapup_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".wav");
  {std::ofstream out(fixture,std::ios::binary);out<<"test";}
  for(int mode=0;mode<3;++mode){
   reset();Campaign campaign=makeCampaign();committed=campaign;
   auto playback=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());
   BattleTrackerController tracker(campaign,playback);tracker.selectEncounterById(campaign.encounters[0].metadata.id);
   failSave=mode==1;throwSave=mode==2;
   tracker.generateAfterCombatSummary();
   require(saveCalls==1,"wrap-up must attempt one coherent save");
   require(tracker._draftEncounter.combatPhase==CombatPhase::WrapUp && tracker._draftEncounter.combatLog.size()==1 &&
     tracker._draftEncounter.afterCombatSummary.find("Silver key")!=std::string::npos &&
     tracker._draftEncounter.afterCombatSummary.find("Defeated bandit")!=std::string::npos,"wrap-up draft lost content");
   if(mode){
    require(playCalls==0 && tracker._encounterSaveFailed && !tracker._combatErrorMessage.empty() &&
      committed.encounters[0].combatPhase==CombatPhase::ActionResolution && committed.encounters[0].combatLog.empty(),
      "failed completion changed committed state or played audio");
    tracker.generateAfterCombatSummary();
    require(saveCalls==1 && tracker._draftEncounter.combatLog.size()==1 && playCalls==0,"repeat failed wrap-up reapplied mutation");
    failSave=throwSave=false;tracker.saveEncounter();
    require(!tracker._encounterSaveFailed && committed.encounters[0].combatPhase==CombatPhase::WrapUp && playCalls==0,
      "Retry failed or unexpectedly replayed audio");
   }else require(playCalls==1,"successful transition must play matching end trigger once");
   const int expectedPlays=mode?0:1;
   tracker._draftEncounter.afterCombatSessionNotes="Return to village";
   tracker.generateAfterCombatSummary();
   require(playCalls==expectedPlays && committed.encounters[0].combatLog.size()==1 &&
      committed.encounters[0].afterCombatSummary.find("Return to village")!=std::string::npos,
      "regenerated summary duplicated completion or lost edited notes");
   BattleTrackerController reopened(campaign,playback);reopened.selectEncounterById(campaign.encounters[0].metadata.id);
   reopened.generateAfterCombatSummary();require(playCalls==expectedPlays && committed.encounters[0].combatLog.size()==1,
      "reopened completed encounter replayed completion");
  }
  reset();Campaign campaign=makeCampaign();committed=campaign;
  auto playback=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());
  BattleTrackerController tracker(campaign,playback);tracker.selectEncounterById(campaign.encounters[0].metadata.id);
  failSave=true;tracker._draftEncounter.afterCombatSessionNotes="Pending notes";tracker.saveEncounter();
  tracker.generateAfterCombatSummary();require(saveCalls==1 && playCalls==0 && tracker._draftEncounter.combatLog.empty() &&
    tracker._draftEncounter.combatPhase==CombatPhase::ActionResolution,"preexisting failed edit did not block wrap-up");
  failSave=false;tracker.saveEncounter();failAudio=true;tracker.generateAfterCombatSummary();
  require(committed.encounters[0].combatPhase==CombatPhase::WrapUp && !tracker._encounterSaveFailed &&
    playCalls==1 && !tracker._combatErrorMessage.empty(),"audio failure lost durable completion or hid feedback");
  tracker.generateAfterCombatSummary();require(playCalls==1,"audio failure repeated completion effect");
  std::filesystem::remove(fixture);
  std::cout<<"BattleTracker wrap-up controller/backend contracts passed (success, false, exception, pending guard, silent Retry, repeat/reopen, audio failure).\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';if(!fixture.empty())std::filesystem::remove(fixture);return 1;}}
};
int main(){return BattleTrackerWrapUpProbe::run();}
