#include "BattleTrackerControllerEffectSupport.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"

bool BattleTrackerController::canCastSelectedSpell(std::string *reason) const
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
    {
        if (reason)
            *reason = combatText("No caster is selected.");
        return false;
    }

    const MonsterActionModel action = getSelectedAction();
    if (action.sourceSpellId.empty())
    {
        if (reason)
            *reason = combatText("Selected action is not a spell.");
        return false;
    }

    SpellModel spell;
    const auto caster = getParticipantSpellcasterSnapshot(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
    if (!caster.valid || !findAvailableSpellById(_context.campaign, action.sourceSpellId, spell)) {
        if (reason) *reason = combatText("Selected action is not a resolvable spell.");
        return false;
    }
    SpellcastingValues::Reason calculationReason = SpellcastingValues::Reason::None;
    if (!SpellcastingValues::readyForSpell(_context.campaign, caster.attributes, caster.spellcasting, caster.level, spell, &calculationReason)) {
        if (reason) *reason = SpellcastingValueUi::reasonText(calculationReason);
        return false;
    }

    if (_castSelectedSpellAsRitual && selectedSpellCanBeCastAsRitual())
    {
        if (reason)
            *reason = combatText("Ritual cast: no spell slot spent.");
        return true;
    }

    if (!action.consumesSpellSlot || action.spellLevel <= 0)
    {
        if (reason)
            *reason = combatText("Cantrip or slot-free spell.");
        return true;
    }

    const EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    const CharacterSpellcastingModel *spellcasting = findParticipantSpellcasting(_context.campaign, participant);
    if (!spellcasting || !_context.campaign.ruleSet)
    {
        if (reason)
            *reason = combatText("This combatant has no spell slot tracker.");
        return false;
    }

    std::string ruleReason;
    const bool canSpend = _context.campaign.ruleSet->canSpendSpellSlot(*spellcasting, std::clamp(action.spellLevel, 1, 9), &ruleReason);
    if (reason)
        *reason = localizedRuleReason(ruleReason);
    return canSpend;
}

bool BattleTrackerController::selectedSpellCanBeCastAsRitual() const
{
    const MonsterActionModel action = getSelectedAction();
    if (action.sourceSpellId.empty())
        return false;

    SpellModel spell;
    return findAvailableSpellById(_context.campaign, action.sourceSpellId, spell) && spell.ritual && spell.level > 0;
}

std::string BattleTrackerController::selectedSpellSlotStatus() const
{
    std::string reason;
    canCastSelectedSpell(&reason);
    return reason;
}

void BattleTrackerController::castSelectedSpell()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    std::string reason;
    if (!canCastSelectedSpell(&reason))
    {
        _lastAttackResolved = true;
        _lastAttackHit = false;
        _lastAttackMessage = reason;
        return;
    }

    const MonsterActionModel action = getSelectedAction();
    EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    CharacterSpellcastingModel *spellcasting = findParticipantSpellcasting(_context.campaign, participant);
    if (!spellcasting || !_context.campaign.ruleSet)
        return;

    if (!(_castSelectedSpellAsRitual && selectedSpellCanBeCastAsRitual()) &&
        action.consumesSpellSlot && action.spellLevel > 0 && !_context.campaign.ruleSet->spendSpellSlot(*spellcasting, std::clamp(action.spellLevel, 1, 9), &reason))
        return;

    touchParticipantSpellcaster(_context.campaign, participant);
    saveCampaign();
    synchronizeParticipantsWithSources();

    _lastAttackResolved = true;
    _lastAttackHit = false;
    _lastAttackMessage = combatFormat("{0} cast. {1}", {action.name, selectedSpellSlotStatus()});
    addCombatLogEntry("Spell", _lastAttackMessage, participant.displayName);
}

void BattleTrackerController::resolveSelectedSpellCast()
{
    _lastAttackResolved = false;
    _lastAttackHit = false;

    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    const std::vector<int> targets = getSelectedTargetIndices();
    if (targets.empty())
        return;

    const MonsterActionModel action = getSelectedAction();
    RuleSpellResolutionSummary summary;
    if (action.sourceSpellId.empty() || !getSelectedSpellResolutionSummary(summary))
    {
        _lastAttackResolved = true;
        std::string reason;
        _lastAttackMessage = !canCastSelectedSpell(&reason) && !reason.empty() ? reason :
            combatText("Selected action is not a resolvable spell.");
        return;
    }

    if (summary.resolution == SpellResolutionType::Healing && !_healingDiceEntered)
    {
        _lastAttackResolved = false;
        _lastAttackHit = false;
        _lastAttackMessage = combatText("Roll healing or enter a healing dice result before resolving the spell.");
        return;
    }

    const int actionRange = std::max(action.reachFeet, action.rangeFeet);
    const auto outOfRangeIt = std::find_if(targets.begin(), targets.end(), [&](int targetIndex) {
        return distanceBetweenParticipants(_selectedParticipantIndex, targetIndex) > actionRange;
    });
    if (outOfRangeIt != targets.end())
    {
        _lastAttackResolved = true;
        _lastAttackMessage = combatFormat("{0} is out of range for {1}.",
                                          {_draftEncounter.participants[*outOfRangeIt].displayName, action.name});
        return;
    }

    if (!_draftEncounter.theaterOfMind)
    {
        const EncounterParticipantModel &casterPreview = _draftEncounter.participants[_selectedParticipantIndex];
        const auto outOfSightIt = std::find_if(targets.begin(), targets.end(), [&](int targetIndex) {
            const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
            return encounterLineOfSightBlocked(_draftEncounter, casterPreview.x, casterPreview.y, target.x, target.y);
        });
        if (outOfSightIt != targets.end())
        {
            _lastAttackResolved = true;
            _lastAttackMessage = combatFormat("{0} is behind blocked line of sight for {1}.",
                                              {_draftEncounter.participants[*outOfSightIt].displayName, action.name});
            return;
        }
    }

    EncounterParticipantModel &caster = _draftEncounter.participants[_selectedParticipantIndex];
    std::string reason;
    if (!spendSelectedSpellSlot(action, caster, &reason))
    {
        _lastAttackResolved = true;
        _lastAttackMessage = reason;
        return;
    }

    const std::string actorName = caster.displayName;
    std::string targetNames;
    for (int targetIndex : targets)
    {
        if (!targetNames.empty())
            targetNames += ", ";
        targetNames += _draftEncounter.participants[targetIndex].displayName;
    }
    addCombatLogEntry("Spell",
                      combatFormat(_castSelectedSpellAsRitual
                                       ? "{0} casts {1} as a ritual targeting {2}. {3}"
                                       : "{0} casts {1} targeting {2}. {3}",
                                   {actorName, action.name, targetNames, selectedSpellSlotStatus()}),
                      actorName);
    beginSelectedCasterConcentration(summary);

    if (summary.resolution == SpellResolutionType::SpellAttack)
    {
        int hitCount = 0;
        for (int targetIndex : targets)
        {
            const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
            const int targetArmorClass = target.armorClass + (_targetHasCover ? 2 : 0);
            const bool hit = getAttackTotal() >= targetArmorClass;
            if (hit)
                ++hitCount;
            addCombatLogEntry("Attack",
                              combatFormat(hit
                                               ? (_targetHasCover ? "{0} hits {1} with {2} against cover." : "{0} hits {1} with {2}.")
                                               : (_targetHasCover ? "{0} misses {1} with {2} against cover." : "{0} misses {1} with {2}."),
                                           {action.name, target.displayName, std::to_string(getAttackTotal())}),
                              actorName);
            if (hit && getDamageTotal() > 0)
                applySpellDamageToTarget(targetIndex, getDamageTotal(), actorName);
            if (hit)
                applySpellEffectToTarget(targetIndex, summary, actorName);
        }
        _lastAttackHit = hitCount > 0;
        _lastAttackResolved = true;
        _lastAttackMessage = combatFormat("{0} resolved against {1} target(s): {2} hit.",
                                          {action.name, std::to_string(static_cast<int>(targets.size())), std::to_string(hitCount)});
        changeCombatPhase(CombatPhase::ReactionsEffects);
        return;
    }

    if (summary.resolution == SpellResolutionType::SavingThrow)
    {
        ensureSavingThrowInputs();
        int failedCount = 0;
        int succeededCount = 0;
        _lastAttackResolved = true;
        const std::string saveAbility = summary.savingThrowAbility.empty()
                                            ? combatText("saving throw")
                                            : combatFormat("{0} save", {combatText(summary.savingThrowAbility.c_str())});
        for (int targetIndex : targets)
        {
            const std::string targetName = _draftEncounter.participants[targetIndex].displayName;
            const int saveTotal = getSavingThrowTotalForTarget(targetIndex);
            const bool failedSave = saveTotal < summary.saveDc;
            if (failedSave)
                ++failedCount;
            else
                ++succeededCount;
            addCombatLogEntry("Save",
                              combatFormat(failedSave
                                               ? "{0} fails {1} against {2} DC {3} with {4}."
                                               : "{0} succeeds on {1} against {2} DC {3} with {4}.",
                                           {targetName, saveAbility, action.name, std::to_string(summary.saveDc), std::to_string(saveTotal)}),
                              actorName);
            if (failedSave && getDamageTotal() > 0)
                applySpellDamageToTarget(targetIndex, getDamageTotal(), actorName);
            if (failedSave)
                applySpellEffectToTarget(targetIndex, summary, actorName);
        }
        _lastAttackHit = failedCount > 0;
        _lastAttackMessage = combatFormat("{0} resolved against {1} target(s): {2} failed {3}, {4} succeeded.",
                                          {action.name,
                                           std::to_string(static_cast<int>(targets.size())),
                                           std::to_string(failedCount),
                                           saveAbility,
                                           std::to_string(succeededCount)});
        changeCombatPhase(CombatPhase::ReactionsEffects);
        return;
    }

    if (summary.resolution == SpellResolutionType::Healing)
    {
        const int healingAmount = getSpellHealingTotal();
        _lastAttackHit = healingAmount > 0;
        _lastAttackResolved = true;
        _lastAttackMessage = combatFormat("{0} restores {1} HP to {2} target(s).",
                                          {action.name, std::to_string(healingAmount), std::to_string(static_cast<int>(targets.size()))});
        if (healingAmount > 0)
        {
            for (int targetIndex : targets)
                applySpellHealingToTarget(targetIndex, healingAmount, actorName);
        }
        else
            addCombatLogEntry("Healing", _lastAttackMessage, actorName);
        changeCombatPhase(CombatPhase::ReactionsEffects);
        return;
    }

    _lastAttackHit = true;
    _lastAttackResolved = true;
    _lastAttackMessage = combatFormat("{0} effect resolved for {1} target(s).",
                                      {action.name, std::to_string(static_cast<int>(targets.size()))});
    for (int targetIndex : targets)
        applySpellEffectToTarget(targetIndex, summary, actorName);
    if (!summary.ongoingEffect)
        addCombatLogEntry("Effect", _lastAttackMessage, actorName);
    changeCombatPhase(CombatPhase::ReactionsEffects);
}

bool BattleTrackerController::getSelectedSpellResolutionSummary(RuleSpellResolutionSummary &summary) const
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return false;
    if (!_context.campaign.ruleSet)
        return false;

    const MonsterActionModel action = getSelectedAction();
    if (action.sourceSpellId.empty())
        return false;

    SpellModel spell;
    if (!findAvailableSpellById(_context.campaign, action.sourceSpellId, spell))
        return false;

    const EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    ParticipantSpellcasterSnapshot caster = getParticipantSpellcasterSnapshot(_context.campaign, participant);
    if (!caster.valid)
        return false;

    if (!SpellcastingValues::readyForSpell(_context.campaign, caster.attributes, caster.spellcasting, caster.level, spell))
        return false;
    summary = _context.campaign.ruleSet->summarizeSpellResolution(spell, caster.spellcasting, caster.attributes, caster.level, action.spellLevel);
    return true;
}
