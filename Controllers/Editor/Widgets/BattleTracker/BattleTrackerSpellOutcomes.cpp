#include "BattleTrackerControllerEffectSupport.hpp"

int BattleTrackerController::getSavingThrowTotal() const
{
    return _savingThrowRollResult + _savingThrowModifier;
}

int BattleTrackerController::getSavingThrowTotalForTarget(int targetIndex) const
{
    if (targetIndex >= 0 &&
        targetIndex < static_cast<int>(_targetSavingThrowRollResults.size()) &&
        targetIndex < static_cast<int>(_targetSavingThrowModifiers.size()))
    {
        return _targetSavingThrowRollResults[static_cast<std::size_t>(targetIndex)] +
               _targetSavingThrowModifiers[static_cast<std::size_t>(targetIndex)];
    }
    return getSavingThrowTotal();
}

void BattleTrackerController::ensureSavingThrowInputs()
{
    const std::size_t participantCount = _draftEncounter.participants.size();
    if (_targetSavingThrowRollResults.size() != participantCount)
    {
        std::vector<int> rolls(participantCount, _savingThrowRollResult);
        for (std::size_t index = 0; index < std::min(rolls.size(), _targetSavingThrowRollResults.size()); ++index)
            rolls[index] = _targetSavingThrowRollResults[index];
        _targetSavingThrowRollResults = std::move(rolls);
    }
    if (_targetSavingThrowModifiers.size() != participantCount)
    {
        std::vector<int> modifiers(participantCount, _savingThrowModifier);
        for (std::size_t index = 0; index < std::min(modifiers.size(), _targetSavingThrowModifiers.size()); ++index)
            modifiers[index] = _targetSavingThrowModifiers[index];
        _targetSavingThrowModifiers = std::move(modifiers);
    }
}

RuleDamageResult BattleTrackerController::previewSpellDamageOrHealingForTarget(int targetIndex, int amount, bool healing) const
{
    RuleDamageResult result;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return result;

    const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
    if (_context.campaign.ruleSet)
    {
        RuleDamageApplication request;
        request.currentHitPoints = target.currentHitPoints;
        request.maxHitPoints = target.maxHitPoints;
        request.temporaryHitPoints = target.temporaryHitPoints;
        request.amount = healing ? std::max(0, amount) : std::max(0, amount - target.armorDamageReduction);
        request.damageType = _damageTypeDraft.empty() ? damageTypeFromText(getSelectedAction().damage) : lowerCopy(trimCopy(_damageTypeDraft));
        request.resistances = target.damageResistances;
        request.vulnerabilities = target.damageVulnerabilities;
        request.immunities = target.damageImmunities;
        request.healing = healing;
        return _context.campaign.ruleSet->applyDamageOrHealing(request);
    }

    result.currentHitPoints = healing
                                  ? std::clamp(target.currentHitPoints + std::max(0, amount), 0, std::max(0, target.maxHitPoints))
                                  : std::clamp(target.currentHitPoints - std::max(0, amount - target.armorDamageReduction), 0, std::max(0, target.maxHitPoints));
    result.temporaryHitPoints = target.temporaryHitPoints;
    result.effectiveAmount = healing ? result.currentHitPoints - target.currentHitPoints : target.currentHitPoints - result.currentHitPoints;
    result.defeated = result.currentHitPoints == 0;
    return result;
}

std::string BattleTrackerController::previewSpellOutcomeForTarget(int targetIndex, const RuleSpellResolutionSummary &summary, bool targetFailedOrHit) const
{
    if (targetIndex < 0 || targetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return combatText("Invalid target.");

    const EncounterParticipantModel &target = _draftEncounter.participants[targetIndex];
    if (!targetFailedOrHit && summary.resolution != SpellResolutionType::Healing)
        return combatText("No automatic HP/effect change.");

    std::string outcome;
    if (summary.resolution == SpellResolutionType::Healing)
    {
        const RuleDamageResult result = previewSpellDamageOrHealingForTarget(targetIndex, getSpellHealingTotal(), true);
        outcome = combatFormat("Heal {0} HP ({1} -> {2})",
                               {std::to_string(std::max(0, result.currentHitPoints - target.currentHitPoints)),
                                std::to_string(target.currentHitPoints),
                                std::to_string(result.currentHitPoints)});
    }
    else if ((summary.resolution == SpellResolutionType::SpellAttack || summary.resolution == SpellResolutionType::SavingThrow) && getDamageTotal() > 0)
    {
        const RuleDamageResult result = previewSpellDamageOrHealingForTarget(targetIndex, getDamageTotal(), false);
        outcome = combatFormat("Damage {0} ({1} -> {2} HP",
                               {std::to_string(result.effectiveAmount),
                                std::to_string(target.currentHitPoints),
                                std::to_string(result.currentHitPoints)});
        if (target.temporaryHitPoints != result.temporaryHitPoints)
            outcome += combatFormat(", temp {0} -> {1}",
                                    {std::to_string(target.temporaryHitPoints), std::to_string(result.temporaryHitPoints)});
        outcome += ")";
        for (const std::string &note : result.notes)
            outcome += " " + combatText(note.c_str());
    }

    if (summary.ongoingEffect)
    {
        if (!outcome.empty())
            outcome += "; ";
        outcome += combatFormat("Effect: {0}", {summary.spellName});
        if (!summary.duration.empty())
            outcome += " (" + combatText(summary.duration.c_str()) + ")";
        if (summary.concentration)
            outcome += combatText(" concentration");
    }

    if (outcome.empty())
        outcome = combatText("Log resolution only.");
    return outcome;
}

void BattleTrackerController::setSelectedSpellSlotMaximum(int spellLevel, int value)
{
    CharacterSpellcastingModel *spellcasting = getSelectedSourceSpellcasting();
    if (!spellcasting || spellLevel < 1 || spellLevel > 9)
        return;

    normalizeSpellcasting(*spellcasting);
    spellcasting->slotMaximums[static_cast<std::size_t>(spellLevel)] = std::max(0, value);
    spellcasting->slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(spellcasting->slotUsed[static_cast<std::size_t>(spellLevel)], 0, spellcasting->slotMaximums[static_cast<std::size_t>(spellLevel)]);
    if (_selectedParticipantIndex >= 0 && _selectedParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
        touchParticipantSpellcaster(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
    saveCampaign();
    synchronizeParticipantsWithSources();
}

void BattleTrackerController::setSelectedSpellSlotUsed(int spellLevel, int value)
{
    CharacterSpellcastingModel *spellcasting = getSelectedSourceSpellcasting();
    if (!spellcasting || spellLevel < 1 || spellLevel > 9)
        return;

    normalizeSpellcasting(*spellcasting);
    spellcasting->slotUsed[static_cast<std::size_t>(spellLevel)] = std::clamp(value, 0, spellcasting->slotMaximums[static_cast<std::size_t>(spellLevel)]);
    if (_selectedParticipantIndex >= 0 && _selectedParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
        touchParticipantSpellcaster(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
    saveCampaign();
    synchronizeParticipantsWithSources();
}

CharacterSpellcastingModel *BattleTrackerController::getSelectedSourceSpellcasting()
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return nullptr;
    return findParticipantSpellcasting(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
}

const CharacterSpellcastingModel *BattleTrackerController::getSelectedSourceSpellcasting() const
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return nullptr;
    return findParticipantSpellcasting(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
}



const std::vector<ResourceTrackerModel> *BattleTrackerController::getSelectedSourceResources() const
{
    if (_selectedParticipantIndex < 0 || _selectedParticipantIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return nullptr;
    const EncounterParticipantModel &participant = _draftEncounter.participants[_selectedParticipantIndex];
    if (participant.kind == CombatantKind::Player)
    {
        const auto characterIt = std::find_if(_context.campaign.characters.begin(), _context.campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == _context.campaign.characters.end() ? nullptr : &characterIt->resourceTrackers;
    }
    if (participant.kind == CombatantKind::NPC)
    {
        const auto npcIt = std::find_if(_context.campaign.npcs.begin(), _context.campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == _context.campaign.npcs.end() ? nullptr : &npcIt->resourceTrackers;
    }
    return nullptr;
}

void BattleTrackerController::setSelectedSourceResourceUsed(int trackerIndex, int used)
{
    std::vector<ResourceTrackerModel> *resources = getSelectedSourceResources();
    if (!resources || trackerIndex < 0 || trackerIndex >= static_cast<int>(resources->size()))
        return;

    ResourceTrackerModel &tracker = (*resources)[static_cast<std::size_t>(trackerIndex)];
    const auto before = captureCorrection(CombatEditHistory::Kind::Resource, tracker.metadata.id);
    normalizeResourceTracker(tracker);
    tracker.used = std::clamp(used, 0, tracker.maximum);
    tracker.metadata.touch();
    if (_selectedParticipantIndex >= 0 && _selectedParticipantIndex < static_cast<int>(_draftEncounter.participants.size()))
        touchParticipantSpellcaster(_context.campaign, _draftEncounter.participants[_selectedParticipantIndex]);
    saveCampaign();
    recordCorrection(before, "Edit combat resource usage");
}

void BattleTrackerController::resetSelectedSourceResource(int trackerIndex)
{
    setSelectedSourceResourceUsed(trackerIndex, 0);
}
