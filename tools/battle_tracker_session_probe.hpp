#pragma once
struct BattleTrackerTurnsProbe
{
    static void inspectOther(BattleTrackerController &c){c._selectedParticipantIndex=1;c._rangeTargetIndex=0;c._selectedTargetIndices={0};}
    static int inspected(const BattleTrackerController &c){return c._selectedParticipantIndex;}
    static int action(const BattleTrackerController &c){return c._selectedActionIndex;}
    static int primaryTarget(const BattleTrackerController &c){return c._rangeTargetIndex;}
    static const std::vector<int> &targets(const BattleTrackerController &c){return c._selectedTargetIndices;}
    static const EncounterModel &draft(const BattleTrackerController &c){return c._draftEncounter;}
    static bool selectSpell(BattleTrackerController &c,const EntityId &id,bool ritual=false)
    {
        c._selectedParticipantIndex=0;c._rangeTargetIndex=1;c._selectedTargetIndices={1};
        c._castSelectedSpellAsRitual=ritual;c._attackRollResult=15;c._damageRollResult=4;
        c._targetSavingThrowRollResults={0,1};c._targetSavingThrowModifiers={0,0};
        const auto &actions=c._draftEncounter.participants[0].actions;
        for(int i=0;i<int(actions.size());++i)if(actions[i].sourceSpellId==id){c._selectedActionIndex=i;return true;}
        return false;
    }
    static bool canCast(const BattleTrackerController &c){return c.canCastSelectedSpell();}
    static bool summary(const BattleTrackerController &c,RuleSpellResolutionSummary &out){return c.getSelectedSpellResolutionSummary(out);}
    static bool spend(BattleTrackerController &c){return c.spendSelectedSpellSlot(c.getSelectedAction(),c._draftEncounter.participants[0]);}
    static void cast(BattleTrackerController &c){c.castSelectedSpell();}
    static void resolveSpell(BattleTrackerController &c){c.resolveSelectedSpellCast();}
    static void resolveAttack(BattleTrackerController &c){c.resolveAttackRoll();}
    static void damage(BattleTrackerController &c){c.applyDamageRoll();}
    static bool hit(const BattleTrackerController &c){return c._lastAttackResolved&&c._lastAttackHit;}
    static std::string feedback(const BattleTrackerController &c){return c._lastAttackMessage;}
    static int total(const BattleTrackerController &c){return c.getAttackTotal();}
};
