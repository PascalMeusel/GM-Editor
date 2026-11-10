#include "BattleTrackerControllerEffectSupport.hpp"

void BattleTrackerController::applySpellDamageToTarget(int targetIndex, int damageAmount, const std::string &actorName)
{
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
    const std::string damageType = _damageTypeDraft.empty() ? damageTypeFromText(getSelectedAction().damage) : lowerCopy(trimCopy(_damageTypeDraft));
    RuleDamageResult result;
    if (_context.campaign.ruleSet)
    {
        RuleDamageApplication request;
        request.currentHitPoints = target.currentHitPoints;
        request.maxHitPoints = target.maxHitPoints;
        request.temporaryHitPoints = target.temporaryHitPoints;
        request.amount = std::max(0, damageAmount - target.armorDamageReduction);
        request.damageType = damageType;
        request.resistances = target.damageResistances;
        request.vulnerabilities = target.damageVulnerabilities;
        request.immunities = target.damageImmunities;
        result = _context.campaign.ruleSet->applyDamageOrHealing(request);
    }
    else
    {
        result.currentHitPoints = std::clamp(target.currentHitPoints - std::max(0, damageAmount - target.armorDamageReduction), 0, std::max(0, target.maxHitPoints));
        result.temporaryHitPoints = target.temporaryHitPoints;
        result.effectiveAmount = target.currentHitPoints - result.currentHitPoints;
        result.defeated = result.currentHitPoints == 0;
    }
    target.currentHitPoints = result.currentHitPoints;
    target.temporaryHitPoints = result.temporaryHitPoints;
    target.defeated = result.defeated;
    syncPlayerHealthFromParticipant(_context.campaign, target);
    std::string notes;
    for (const std::string &note : result.notes)
        notes += " " + combatText(note.c_str());
    const std::string localizedDamageType = damageType.empty() ? std::string{} : combatText(damageType.c_str());
    const char *damageTemplate = damageType.empty()
                                     ? (target.defeated ? "{0} takes {1} spell damage and is defeated." : "{0} takes {1} spell damage.")
                                     : (target.defeated ? "{0} takes {1} {2} spell damage and is defeated." : "{0} takes {1} {2} spell damage.");
    addCombatLogEntry("Spell Damage",
                      combatFormat(damageTemplate,
                                   {target.displayName, std::to_string(result.effectiveAmount), localizedDamageType}) + notes,
                      actorName);
    if (result.effectiveAmount > 0 && participantIsConcentrating(target) && _context.campaign.ruleSet)
    {
        const RuleConcentrationCheck concentrationCheck = _context.campaign.ruleSet->createConcentrationCheck(result.effectiveAmount);
        addCombatLogEntry("Concentration",
                          combatFormat("{0} is concentrating and should make a Constitution save DC {1}.",
                                       {target.displayName, std::to_string(concentrationCheck.dc)}),
                          target.displayName);
    }
}

void BattleTrackerController::applySpellHealingToTarget(int targetIndex, int healingAmount, const std::string &actorName)
{
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
    const int previousHitPoints = target.currentHitPoints;
    if (_context.campaign.ruleSet)
    {
        RuleDamageApplication request;
        request.currentHitPoints = target.currentHitPoints;
        request.maxHitPoints = target.maxHitPoints;
        request.temporaryHitPoints = target.temporaryHitPoints;
        request.amount = healingAmount;
        request.healing = true;
        const RuleDamageResult result = _context.campaign.ruleSet->applyDamageOrHealing(request);
        target.currentHitPoints = result.currentHitPoints;
        target.temporaryHitPoints = result.temporaryHitPoints;
        target.defeated = result.defeated;
    }
    else
    {
        target.currentHitPoints = std::clamp(target.currentHitPoints + std::max(0, healingAmount), 0, std::max(0, target.maxHitPoints));
        target.defeated = target.currentHitPoints == 0;
    }
    syncPlayerHealthFromParticipant(_context.campaign, target);
    addCombatLogEntry("Healing",
                      combatFormat("{0} regains {1} HP.",
                                   {target.displayName, std::to_string(target.currentHitPoints - previousHitPoints)}),
                      actorName);
}

void BattleTrackerController::applySpellEffectToTarget(int targetIndex, const RuleSpellResolutionSummary &summary, const std::string &actorName)
{
    if (!summary.ongoingEffect)
        return;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
    const std::string condition = "Spell Effect: " + summary.spellName;
    ActiveEffectModel effect;
    effect.kind = effectKindForSpellSummary(summary);
    effect.name = summary.spellName;
    effect.sourceName = actorName;
    if (_selectedParticipantIndex >= 0 && _selectedParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
        effect.sourceEntityId = _draftEncounter.participants[_selectedParticipantIndex].sourceEntityId;
    effect.sourceSpellId = summary.spellId;
    effect.notes = summary.duration.empty() ? summary.scaling : summary.duration;
    effect.remainingRounds = estimateDurationRounds(summary.duration);
    effect.concentration = summary.concentration;
    effect.metadata.touch();
    const bool addedEffect = addOrRefreshActiveEffect(target.activeEffects, effect);
    const bool addedCondition = addUniqueCondition(target.conditions, condition);
    if (addedEffect || addedCondition)
    {
        syncParticipantConditionsToSource(_context.campaign, target);
        saveCampaign();
        addCombatLogEntry("Effect", combatFormat("{0} gains {1}.", {target.displayName, localizedConditionLabel(condition)}), actorName);
    }
    else
    {
        addCombatLogEntry("Effect", combatFormat("{0} remains affected by {1}.", {target.displayName, summary.spellName}), actorName);
    }
}

void BattleTrackerController::beginSelectedCasterConcentration(const RuleSpellResolutionSummary &summary)
{
    if (!summary.concentration)
        return;
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &caster = _draftEncounter.participants[_selectedParticipantIndex];
    caster.conditions.erase(std::remove_if(caster.conditions.begin(), caster.conditions.end(), [](const std::string &condition) {
                                return lowerCopy(condition).find("concentrat") != std::string::npos;
                            }),
                            caster.conditions.end());
    caster.activeEffects.erase(std::remove_if(caster.activeEffects.begin(), caster.activeEffects.end(), [](const ActiveEffectModel &effect) {
                                  return effect.concentration || lowerCopy(effect.name).find("concentrat") != std::string::npos;
                              }),
                              caster.activeEffects.end());
    const std::string concentration = "Concentrating: " + summary.spellName;
    addUniqueCondition(caster.conditions, concentration);
    ActiveEffectModel effect;
    effect.kind = ActiveEffectKind::Condition;
    effect.name = concentration;
    effect.sourceName = caster.displayName;
    effect.sourceEntityId = caster.sourceEntityId;
    effect.sourceSpellId = summary.spellId;
    effect.notes = summary.duration;
    effect.remainingRounds = estimateDurationRounds(summary.duration);
    effect.concentration = true;
    effect.metadata.touch();
    addOrRefreshActiveEffect(caster.activeEffects, effect);
    syncParticipantConditionsToSource(_context.campaign, caster);
    saveCampaign();
    addCombatLogEntry("Concentration",
                      combatFormat("{0} is now {1}.", {caster.displayName, localizedConditionLabel(concentration)}),
                      caster.displayName);
}





void BattleTrackerController::applyHitPointDeltaToSelected(int delta)
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return;

    EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    const auto before = captureCorrection(CombatEditHistory::Kind::Health);
    int effectiveAmount = std::abs(delta);
    if (_context.campaign.ruleSet)
    {
        RuleDamageApplication request;
        request.currentHitPoints = participant.currentHitPoints;
        request.maxHitPoints = participant.maxHitPoints;
        request.temporaryHitPoints = participant.temporaryHitPoints;
        request.amount = std::abs(delta);
        request.healing = delta > 0;
        request.damageType = lowerCopy(trimCopy(_damageTypeDraft));
        request.resistances = participant.damageResistances;
        request.vulnerabilities = participant.damageVulnerabilities;
        request.immunities = participant.damageImmunities;
        const RuleDamageResult result = _context.campaign.ruleSet->applyDamageOrHealing(request);
        participant.currentHitPoints = result.currentHitPoints;
        participant.temporaryHitPoints = result.temporaryHitPoints;
        participant.defeated = result.defeated;
        effectiveAmount = result.effectiveAmount;
    }
    else
    {
        participant.currentHitPoints = std::clamp(participant.currentHitPoints + delta, 0, std::max(0, participant.maxHitPoints));
        participant.defeated = participant.currentHitPoints == 0;
    }
    syncPlayerHealthFromParticipant(_context.campaign, participant);
    addCombatLogEntry(delta < 0 ? "Damage" : "Healing",
                      combatFormat(delta < 0 ? "{0} loses {1} HP." : "{0} gains {1} HP.",
                                   {participant.displayName, std::to_string(effectiveAmount)}),
                      participant.displayName);
    if (delta < 0 && effectiveAmount > 0 && participantIsConcentrating(participant) && _context.campaign.ruleSet)
    {
        const RuleConcentrationCheck concentrationCheck = _context.campaign.ruleSet->createConcentrationCheck(effectiveAmount);
        addCombatLogEntry("Concentration",
                          combatFormat("{0} is concentrating and should make a Constitution save DC {1}.",
                                       {participant.displayName, std::to_string(concentrationCheck.dc)}),
                          participant.displayName);
    }
    recordCorrection(before, delta < 0 ? "Apply manual damage" : "Apply manual healing");
}
