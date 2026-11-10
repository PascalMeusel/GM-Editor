#include "BattleTrackerControllerEffectSupport.hpp"

void BattleTrackerController::rollDiceExpression()
{
    if (!_context.campaign.ruleSet)
        return;

    const std::string expression = _diceRollExpression.empty() ? "1d20" : _diceRollExpression;
    const RuleDiceSummary summary = _context.campaign.ruleSet->summarizeDiceExpression(expression);
    if (!summary.valid)
    {
        _lastDiceRollMessage = combatText("Invalid dice expression.");
        return;
    }

    const std::vector<int> rolls = rollDiceForExpression(expression);
    RuleDiceRollResult result = _context.campaign.ruleSet->evaluateDiceExpression(expression, rolls);
    if (!result.valid)
    {
        _lastDiceRollMessage = result.error.empty() ? combatText("Invalid dice expression.") : combatText(result.error.c_str());
        return;
    }

    const std::string label = _diceRollPresetName.empty() ? combatText("Dice Roll") : _diceRollPresetName;
    _lastDiceRollMessage = label + ": " + expression + " = " + std::to_string(result.total);
    if (!result.rolls.empty())
        _lastDiceRollMessage += combatFormat(" (rolls: {0})", {joinRolls(result.rolls)});
    addCombatLogEntry("Roll", _lastDiceRollMessage, label);
}

bool BattleTrackerController::rollSelectedSpellHealing()
{
    RuleSpellResolutionSummary summary;
    if (!_context.campaign.ruleSet || !getSelectedSpellResolutionSummary(summary) ||
        summary.resolution != SpellResolutionType::Healing || summary.healing.empty())
        return false;

    const RuleDiceSummary diceSummary = _context.campaign.ruleSet->summarizeDiceExpression(summary.healing);
    if (!diceSummary.valid)
    {
        _lastDiceRollMessage = combatText("Healing formula is invalid.");
        return false;
    }

    const std::vector<int> rolls = rollDiceForExpression(summary.healing);
    const RuleDiceRollResult result = _context.campaign.ruleSet->evaluateDiceExpression(summary.healing, rolls);
    if (!result.valid)
    {
        _lastDiceRollMessage = result.error.empty() ? combatText("Healing formula is invalid.") : combatText(result.error.c_str());
        return false;
    }

    // The form stores dice only; getSpellHealingTotal applies the formula's
    // flat modifier exactly once. Fixed healing formulas remain supported.
    _damageRollResult = std::max(0, result.total - diceSummary.flatModifier);
    _healingDiceEntered = true;
    _lastDiceRollMessage = combatFormat("Healing roll: {0} = {1}",
                                        {summary.healing, std::to_string(result.total)});
    if (!result.rolls.empty())
        _lastDiceRollMessage += combatFormat(" (rolls: {0})", {joinRolls(result.rolls)});
    return true;
}

void BattleTrackerController::addDiceRollPreset()
{
    if (_diceRollPresetName.empty() || _diceRollExpression.empty() || !_context.campaign.ruleSet)
        return;
    if (!_context.campaign.ruleSet->summarizeDiceExpression(_diceRollExpression).valid)
    {
        _lastDiceRollMessage = combatText("Preset expression is invalid.");
        return;
    }

    DiceRollPresetModel preset;
    preset.name = _diceRollPresetName;
    preset.expression = _diceRollExpression;
    preset.notes = _diceRollPresetNotes;
    preset.metadata.touch();
    _draftEncounter.diceRollPresets.push_back(preset);
    _selectedDiceRollPresetIndex = static_cast<int>(_draftEncounter.diceRollPresets.size()) - 1;
    saveEncounter();
}

void BattleTrackerController::updateSelectedDiceRollPreset()
{
    if (_selectedDiceRollPresetIndex < 0 || _selectedDiceRollPresetIndex >= static_cast<int>(_draftEncounter.diceRollPresets.size()))
        return;
    if (_diceRollPresetName.empty() || _diceRollExpression.empty() || !_context.campaign.ruleSet)
        return;
    if (!_context.campaign.ruleSet->summarizeDiceExpression(_diceRollExpression).valid)
    {
        _lastDiceRollMessage = combatText("Preset expression is invalid.");
        return;
    }

    DiceRollPresetModel &preset = _draftEncounter.diceRollPresets[_selectedDiceRollPresetIndex];
    preset.name = _diceRollPresetName;
    preset.expression = _diceRollExpression;
    preset.notes = _diceRollPresetNotes;
    preset.metadata.touch();
    saveEncounter();
}

void BattleTrackerController::removeSelectedDiceRollPreset()
{
    if (_selectedDiceRollPresetIndex < 0 || _selectedDiceRollPresetIndex >= static_cast<int>(_draftEncounter.diceRollPresets.size()))
        return;

    _draftEncounter.diceRollPresets.erase(_draftEncounter.diceRollPresets.begin() + _selectedDiceRollPresetIndex);
    _selectedDiceRollPresetIndex = _draftEncounter.diceRollPresets.empty() ? 0 : std::min(_selectedDiceRollPresetIndex, static_cast<int>(_draftEncounter.diceRollPresets.size()) - 1);
    loadSelectedDiceRollPreset();
    saveEncounter();
}



int BattleTrackerController::getSelectedAttackModifier() const
{
    return extractSignedModifier(getSelectedAction().attackBonus);
}

int BattleTrackerController::getAttackTotal() const
{
    int d20 = _attackRollResult;
    if (_attackHasAdvantage && !_attackHasDisadvantage)
        d20 = std::max(_attackRollResult, _secondAttackRollResult);
    else if (_attackHasDisadvantage && !_attackHasAdvantage)
        d20 = std::min(_attackRollResult, _secondAttackRollResult);
    return d20 + getSelectedAttackModifier();
}

int BattleTrackerController::getSelectedDamageModifier() const
{
    return extractFlatDamageModifier(getSelectedAction().damage);
}

int BattleTrackerController::getDamageTotal() const
{
    return std::max(0, _damageRollResult + getSelectedDamageModifier());
}

int BattleTrackerController::getArmorAdjustedDamageTotal() const
{
    if (_rangeTargetIndex < 0 || _rangeTargetIndex >= static_cast<int>(_draftEncounter.participants.size()))
        return getDamageTotal();

    const EncounterParticipantModel &target = _draftEncounter.participants[_rangeTargetIndex];
    return std::max(0, getDamageTotal() - target.armorDamageReduction);
}

int BattleTrackerController::getSpellHealingTotal() const
{
    return std::max(0, _damageRollResult + getSelectedDamageModifier());
}
