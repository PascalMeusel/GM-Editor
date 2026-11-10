#pragma once



#include "Models/Rulesets/IRuleSet.hpp"



inline int IRuleSet::getSpellAttackBonus(const std::map<std::string, int>& attributes, const CharacterSpellcastingModel& spellcasting, int level) const
    {
        if (spellcasting.spellAttackMode == SpellValueMode::Manual ||
            (spellcasting.spellAttackMode == SpellValueMode::Legacy && spellcasting.spellAttackBonus != 0))
            return spellcasting.spellAttackBonus;
        return getAbilityModifier(attributes, spellcasting.ability) + getProficiencyBonus(level);
    }

inline int IRuleSet::getSpellSaveDc(const std::map<std::string, int>& attributes, const CharacterSpellcastingModel& spellcasting, int level) const
    {
        if (spellcasting.spellSaveDcMode == SpellValueMode::Manual ||
            (spellcasting.spellSaveDcMode == SpellValueMode::Legacy && spellcasting.spellSaveDc > 10))
            return spellcasting.spellSaveDc;
        return 8 + getAbilityModifier(attributes, spellcasting.ability) + getProficiencyBonus(level);
    }

inline std::vector<MonsterActionModel> IRuleSet::getPreparedSpellActions(const CharacterSpellcastingModel& spellcasting, const std::vector<SpellModel>& spells, const std::map<std::string, int>& attributes, int level) const
    {
        std::vector<MonsterActionModel> actions;
        CharacterSpellcastingModel normalizedSpellcasting = spellcasting;
        normalizeSpellcasting(normalizedSpellcasting);
        for (const CharacterSpellEntryModel& entry : spellcasting.spells)
        {
            const SpellModel* spell = findSpellById(spells, entry.spellId);
            if (!spell || (spell->level > 0 && !entry.prepared))
                continue;

            MonsterActionModel action;
            const RuleSpellResolutionSummary resolution = summarizeSpellResolution(*spell, spellcasting, attributes, level, spell->level);
            action.name = spell->name;
            action.reachFeet = 5;
            action.rangeFeet = spell->range == "Self" ? 5 : 60;
            action.normalRangeFeet = action.rangeFeet;
            action.damage = !resolution.damage.empty() ? resolution.damage : (!resolution.healing.empty() ? resolution.healing : "manual result");
            action.sourceSpellId = spell->metadata.id;
            action.spellLevel = std::clamp(resolution.castAtLevel, 0, 9);
            action.consumesSpellSlot = spell->level > 0;
            if (spell->resolution == SpellResolutionType::SpellAttack)
            {
                action.attackBonus = spell->attackBonus.empty() ? formatSignedModifier(resolution.attackBonus) : spell->attackBonus;
                action.notes = "Spell attack. " + spellLevelLabel(spell->level);
            }
            else if (spell->resolution == SpellResolutionType::SavingThrow)
            {
                action.attackBonus = "+0";
                action.notes = (resolution.savingThrowAbility.empty() ? "Saving throw" : resolution.savingThrowAbility + " save") + " DC " + std::to_string(resolution.saveDc) + ". " + spellLevelLabel(spell->level);
            }
            else
            {
                action.attackBonus = "+0";
                action.notes = std::string(spellResolutionLabel(spell->resolution)) + ". " + spellLevelLabel(spell->level);
            }
            if (!resolution.area.shape.empty())
                action.notes += " Area: " + resolution.area.shape + " " + std::to_string(resolution.area.sizeFeet) + " ft.";
            if (resolution.ongoingEffect)
                action.notes += " Ongoing: " + resolution.duration + ".";
            if (spell->ritual)
                action.notes += " Ritual.";
            if (!resolution.scaling.empty())
                action.notes += " Scaling: " + resolution.scaling + ".";
            if (!spell->effectSummary.empty())
                action.notes += " " + spell->effectSummary;
            if (spell->level > 0)
            {
                const int maximum = normalizedSpellcasting.slotMaximums[static_cast<std::size_t>(action.spellLevel)];
                const int used = normalizedSpellcasting.slotUsed[static_cast<std::size_t>(action.spellLevel)];
                action.notes += " Slots left: " + std::to_string(std::max(0, maximum - used)) + "/" + std::to_string(maximum) + ".";
            }
            actions.push_back(action);
        }
        return actions;
    }

inline RuleAreaTemplate IRuleSet::parseAreaTemplate(const std::string& areaText) const
    {
        RuleAreaTemplate area;
        area.rawText = areaText;
        return area;
    }

inline RuleSpellResolutionSummary IRuleSet::summarizeSpellResolution(const SpellModel& spell, const CharacterSpellcastingModel& spellcasting, const std::map<std::string, int>& attributes, int casterLevel, int castAtLevel) const
    {
        RuleSpellResolutionSummary summary;
        summary.spellId = spell.metadata.id;
        summary.spellName = spell.name;
        summary.resolution = spell.resolution;
        summary.baseSpellLevel = std::clamp(spell.level, 0, 9);
        summary.castAtLevel = std::clamp(std::max(castAtLevel, spell.level), 0, 9);
        summary.attackBonus = getSpellAttackBonus(attributes, spellcasting, casterLevel);
        summary.saveDc = getSpellSaveDc(attributes, spellcasting, casterLevel);
        summary.savingThrowAbility = spell.savingThrowAbility;
        summary.damage = spell.damage;
        summary.healing = spell.healing;
        summary.scaling = spell.scaling;
        summary.duration = spell.duration;
        summary.concentration = spell.concentration;
        summary.ongoingEffect = spell.concentration || (!spell.duration.empty() && spell.duration != "Instantaneous");
        summary.area = parseAreaTemplate(spell.area);
        return summary;
    }

inline bool IRuleSet::canSpendSpellSlot(CharacterSpellcastingModel spellcasting, int spellLevel, std::string* reason) const
    {
        normalizeSpellcasting(spellcasting);
        spellLevel = std::clamp(spellLevel, 0, 9);
        if (spellLevel == 0)
        {
            if (reason)
                *reason = "Cantrip or slot-free spell.";
            return true;
        }
        if (spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] <= 0)
        {
            if (reason)
                *reason = "No level " + std::to_string(spellLevel) + " slots are configured.";
            return false;
        }
        if (spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)] >= spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)])
        {
            if (reason)
                *reason = "No unused level " + std::to_string(spellLevel) + " slots remain.";
            return false;
        }
        if (reason)
            *reason = "Level " + std::to_string(spellLevel) + " slots left: " +
                      std::to_string(spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] - spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)]) +
                      "/" + std::to_string(spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)]) + ".";
        return true;
    }

inline bool IRuleSet::spendSpellSlot(CharacterSpellcastingModel& spellcasting, int spellLevel, std::string* reason) const
    {
        normalizeSpellcasting(spellcasting);
        spellLevel = std::clamp(spellLevel, 0, 9);
        if (!canSpendSpellSlot(spellcasting, spellLevel, reason))
            return false;
        if (spellLevel > 0)
            ++spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)];
        return true;
    }

inline RuleDiceSummary IRuleSet::summarizeDiceExpression(const std::string& expression) const
    {
        RuleDiceSummary summary;
        summary.expression = expression;
        return summary;
    }

inline RuleDiceRollResult IRuleSet::evaluateDiceExpression(const std::string& expression, const std::vector<int>& suppliedRolls) const
    {
        RuleDiceRollResult result;
        result.summary = summarizeDiceExpression(expression);
        result.valid = result.summary.valid;
        result.total = result.summary.flatModifier;
        result.rolls = suppliedRolls;
        if (!result.valid)
            result.error = "Invalid dice expression.";
        return result;
    }

inline RuleCheckRollResult IRuleSet::resolveCheckRoll(const std::map<std::string, int>& attributes, const RuleCheckRequest& request, int firstD20, int secondD20) const
    {
        RuleCheckRollResult result;
        result.check = evaluateCheck(attributes, request);
        result.firstD20 = firstD20;
        result.secondD20 = secondD20;
        result.valid = firstD20 >= 1 && firstD20 <= 20;
        if (!result.valid)
        {
            result.error = "A d20 roll must be between 1 and 20.";
            return result;
        }

        result.chosenD20 = firstD20;
        if (request.rollMode != RuleRollMode::Normal)
        {
            if (secondD20 < 1 || secondD20 > 20)
            {
                result.valid = false;
                result.error = "Advantage and disadvantage need two d20 rolls.";
                return result;
            }
            result.chosenD20 = request.rollMode == RuleRollMode::Advantage ? std::max(firstD20, secondD20) : std::min(firstD20, secondD20);
        }

        result.total = result.chosenD20 + result.check.modifier;
        result.naturalOne = result.chosenD20 == 1;
        result.naturalTwenty = result.chosenD20 == 20;
        result.success = request.dc <= 0 || result.total >= request.dc;
        return result;
    }
