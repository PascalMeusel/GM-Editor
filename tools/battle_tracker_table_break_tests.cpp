#include "Controllers/Editor/Widgets/BattleTracker/BattleTrackerController.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Audio/AudioBackend.hpp"
#include "Models/Session/SessionControlService.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
Campaign committed; int saveCalls=0, playCalls=0; bool failSave=false, throwSave=false, failAudio=false;
std::filesystem::path fixture; bool armed=false, expectedBreak=false;
void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
class RecordingBackend final : public AudioCommandBackend {
public:
 const char *name() const override { return "Wrap-up recording backend"; }
 bool sendCommand(const std::string &command,std::string &error) const override {
  if(armed && (command.rfind("play ",0)==0 || command.rfind("stop ",0)==0)) {
   require(!committed.sessions.empty() && SessionControl::isBreakActive(committed.sessions[0])==expectedBreak &&
      committed.encounters[0].combatPhase==(expectedBreak?CombatPhase::PausedBreak:CombatPhase::ActiveTurn) &&
      committed.encounters[0].combatLog.size()==1 && committed.sessions[0].notes.size()==1 && committed.timelineEvents.size()==1,
      "audio pause/resume preceded durable session/combat/note/timeline transition");
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

struct BattleTrackerTableBreakProbe {
 static Campaign makeCampaign(bool resuming) {
  Campaign campaign;EncounterModel encounter;encounter.name="Table break";encounter.round=3;
  encounter.combatPhase=resuming?CombatPhase::PausedBreak:CombatPhase::ActiveTurn;
  EncounterParticipantModel actor;actor.displayName="Hero";actor.currentHitPoints=6;actor.maxHitPoints=10;encounter.participants.push_back(actor);
  campaign.encounters.push_back(encounter);
  SessionModel session;session.title="Current session";session.liveModeActive=true;session.liveState.initialized=true;
  session.liveState.phase=resuming?SessionPhase::Break:SessionPhase::Combat;
  session.liveState.phaseBeforeBreak=SessionPhase::Combat;session.liveState.activeEncounterId=encounter.metadata.id;
  campaign.sessions.push_back(session);
  AudioAssetModel asset;asset.name="Music";asset.filePath=fixture.string();campaign.audioAssets.push_back(asset);
  return campaign;
 }
 static int run(){try {
  fixture=std::filesystem::temp_directory_path()/("gm_break_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".wav");
  {std::ofstream out(fixture,std::ios::binary);out<<"test";}
  int total=0;
  for(int resume=0;resume<2;++resume)for(int failure=0;failure<3;++failure)for(int audio=0;audio<2;++audio){
   saveCalls=playCalls=0;failSave=throwSave=failAudio=armed=false;expectedBreak=!resume;
   Campaign campaign=makeCampaign(resume);committed=campaign;
   auto playback=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());std::string error;
   require(playback->play(campaign.audioAssets[0],fixture,1,false,error),"fixture audio start failed");
   if(resume)require(playback->pause(campaign.audioAssets[0].metadata.id,error),"fixture audio pause failed");
   BattleTrackerController tracker(campaign,playback);tracker.selectEncounterById(campaign.encounters[0].metadata.id);
   armed=true;failSave=failure==1;throwSave=failure==2;
   auto invoke=[&]{if(resume)tracker.resumeCombatFromTableBreak(audio!=0);else tracker.startCombatTableBreak(audio!=0);};
   invoke();require(saveCalls==1 && tracker._draftEncounter.combatLog.size()==1 && tracker._draftEncounter.round==3 &&
       tracker._draftEncounter.participants[0].currentHitPoints==6,"table break split save or lost combat state");
   if(failure){
    require(playCalls==0 && tracker._encounterSaveFailed && tracker._pendingTableBreakResume.has_value() &&
        SessionControl::isBreakActive(campaign.sessions[0])==bool(resume) &&
        SessionControl::isBreakActive(committed.sessions[0])==bool(resume) &&
        campaign.sessions[0].notes.empty() && campaign.timelineEvents.empty() && committed.encounters[0].combatLog.empty(),
        "failed table break published Live phase/note/timeline or changed committed combat/audio");
    invoke();require(saveCalls==1 && tracker._draftEncounter.combatLog.size()==1,"repeated failure duplicated transition");
    EncounterModel another;campaign.encounters.push_back(another);tracker._selectedEncounterIndex=1;tracker.loadSelectedEncounter();
    require(tracker._selectedEncounterIndex==0 && tracker._pendingTableBreakResume.has_value() &&
       tracker._draftEncounter.metadata.id==campaign.encounters[0].metadata.id,"legacy chooser discarded pending break draft");
    campaign.sessions[0].summary="Unrelated edit while waiting";
    failSave=throwSave=false;tracker.saveEncounter();
    require(playCalls==0 && !tracker._encounterSaveFailed && !tracker._pendingTableBreakResume.has_value() &&
        committed.sessions[0].summary=="Unrelated edit while waiting","silent Retry lost unrelated edit or retained failure");
   }else require(playCalls==audio,"successful requested audio was missing/duplicated");
   require(SessionControl::isBreakActive(campaign.sessions[0])==expectedBreak &&
     SessionControl::isBreakActive(committed.sessions[0])==expectedBreak && committed.sessions[0].notes.size()==1 &&
     committed.timelineEvents.size()==1 && committed.encounters[0].combatLog.size()==1,"durable table break incomplete");
   require(playback->isPaused(campaign.audioAssets[0].metadata.id)==(failure||!audio?bool(resume):expectedBreak),
      "audio state changed without successful requested effect");
   const int before=saveCalls;invoke();require(saveCalls==before && committed.sessions[0].notes.size()==1,"duplicate success repeated state transition");
   armed=false;total+=saveCalls;
  }
  // A pending unrelated failure blocks break before constructing another draft.
  saveCalls=playCalls=0;failSave=true;throwSave=failAudio=armed=false;
  Campaign campaign=makeCampaign(false);committed=campaign;
  auto playback=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());
  BattleTrackerController tracker(campaign,playback);tracker.selectEncounterById(campaign.encounters[0].metadata.id);
  tracker._draftEncounter.afterCombatSessionNotes="Pending combat edit";tracker.saveEncounter();tracker.startCombatTableBreak(true);
  require(saveCalls==1 && !tracker._pendingTableBreakResume.has_value() && tracker._draftEncounter.combatLog.empty(),"pending combat save allowed table break");
  failSave=false;tracker.saveEncounter();failSave=true;tracker.startCombatTableBreak(true);
  require(tracker._pendingTableBreakResume.has_value(),"failed break did not retain retry intent");
  // If a different session becomes active, do not overwrite it on Retry.
  campaign.sessions[0].liveModeActive=false;SessionModel other;other.liveModeActive=true;campaign.sessions.push_back(other);
  failSave=false;const int before=saveCalls;tracker.saveEncounter();
  require(saveCalls==before && tracker._encounterSaveFailed && tracker._pendingTableBreakResume.has_value() &&
      campaign.sessions[1].liveState.phase==SessionPhase::Setup,"Retry overwrote a changed active session");
  total+=saveCalls;
  for(int resume=0;resume<2;++resume){
   saveCalls=playCalls=0;failSave=throwSave=failAudio=armed=false;expectedBreak=!resume;
   Campaign current=makeCampaign(resume);committed=current;
   auto service=std::make_shared<AudioPlaybackService>(std::make_unique<RecordingBackend>());std::string error;
   service->play(current.audioAssets[0],fixture,1,false,error);if(resume)service->pause(current.audioAssets[0].metadata.id,error);
   BattleTrackerController target(current,service);target.selectEncounterById(current.encounters[0].metadata.id);
   armed=failAudio=true;if(resume)target.resumeCombatFromTableBreak(true);else target.startCombatTableBreak(true);
   require(!target._encounterSaveFailed && !target._combatErrorMessage.empty() &&
      SessionControl::isBreakActive(committed.sessions[0])==expectedBreak && playCalls==1,
      "audio failure lost saved break or hid error");armed=false;total+=saveCalls;
  }
  std::filesystem::remove(fixture);
  std::cout<<"BattleTracker table-break controller/session/audio contracts passed ("<<total<<" save attempts).\n";return 0;
 }catch(const std::exception&e){armed=false;std::cerr<<e.what()<<'\n';if(!fixture.empty())std::filesystem::remove(fixture);return 1;}}
};
int main(){return BattleTrackerTableBreakProbe::run();}
