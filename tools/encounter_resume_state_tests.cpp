#include "Models/UX/EncounterResumeState.hpp"
#include <iostream>
#include <stdexcept>
void require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
int main() {
 try {
  EncounterModel completed; completed.name="Finished fight"; completed.combatPhase=CombatPhase::WrapUp;
  completed.combatPaused=true; completed.round=2; completed.activeParticipantIndex=1;
  completed.afterCombatSummary="Victory and recovered lantern"; completed.combatLog.emplace_back();
  std::vector<EncounterModel> list{completed};
  require(!gm::ux::encounterHasResumableState(completed) && !gm::ux::resumableEncounter(list),
          "Completed combat with pause/history was advertised as resumable");
  require(list.size()==1 && list[0].name=="Finished fight" && list[0].afterCombatSummary==completed.afterCombatSummary && list[0].combatLog.size()==1,
          "Resume classification removed or changed completed reference material");
  list[0].combatPaused=false;
  require(!gm::ux::resumableEncounter(list),"Completed combat round/history remained resumable without pause flag");
  EncounterModel active; active.combatPhase=CombatPhase::ActiveTurn;
  list.push_back(active);
  require(gm::ux::resumableEncounter(list)==&list[1],"Round-one active encounter was hidden behind completed history");
  EncounterModel paused; paused.combatPhase=CombatPhase::PausedBreak; paused.combatPaused=true;
  list.push_back(paused);
  require(gm::ux::resumableEncounter(list)==&list[2],"Actual paused encounter did not take priority over active/reference encounters");
  EncounterModel draft;
  require(!gm::ux::encounterHasResumableState(draft),"Untouched preparation became a resume suggestion");
  draft.combatPhase=CombatPhase::Initiative;
  require(!gm::ux::encounterHasResumableState(draft),"Untouched initiative draft became a resume suggestion");
  draft.combatPhase=CombatPhase::Setup; draft.round=2;
  require(gm::ux::encounterHasResumableState(draft),"Legacy progress fallback was lost");
  for(const auto phase:{CombatPhase::ActiveTurn,CombatPhase::Movement,CombatPhase::ActionResolution,CombatPhase::ReactionsEffects,CombatPhase::EndTurn,CombatPhase::PausedBreak}) {
   EncounterModel current; current.combatPhase=phase;
   require(gm::ux::encounterHasResumableState(current),"Explicit ongoing phase was not resumable");
  }
  std::cout<<"Encounter resume policy passed; completed references retained\n";
 } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
