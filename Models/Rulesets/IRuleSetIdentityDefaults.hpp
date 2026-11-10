#pragma once



#include "Models/Rulesets/IRuleSet.hpp"



inline const char* IRuleSet::getId() const
{ return getName(); }

inline const char* IRuleSet::getVersionId() const
{ return getId(); }

inline const std::vector<std::string> IRuleSet::getLegacyNames() const
{ return {}; }

inline bool IRuleSet::matchesRulesetName(const std::string& name) const
    {
        if (name == getName() || name == getId() || name == getVersionId())
            return true;
        const std::vector<std::string> legacyNames = getLegacyNames();
        return std::find(legacyNames.begin(), legacyNames.end(), name) != legacyNames.end();
    }

inline RuleAdvancementProfile IRuleSet::advancementProfile(const std::string &) const
{ return {}; }

inline const std::vector<MonsterDefinitionModel> IRuleSet::getMonsters() const
{ return {}; }

inline const std::vector<SpellModel> IRuleSet::getSpells() const
{ return {}; }

inline const std::vector<RuleCurrencyDefinition> IRuleSet::getCurrencies() const
    {
        return {{"base", "Base", "base", 1}};
    }

inline const std::vector<RuleEquipmentSlotDefinition> IRuleSet::getEquipmentSlots() const
    {
        return {
            {"body", "Body", 1},
            {"main-hand", "Main Hand", 1},
            {"off-hand", "Off Hand", 1},
            {"two-handed", "Two-Handed", 1},
            {"ranged", "Ranged", 1},
            {"pack", "Pack", 99},
            {"pouch", "Pouch", 99}};
    }

inline std::vector<RuleContentAttribution> IRuleSet::getContentAttributions() const
{ return {}; }

inline RuleContentGateResult IRuleSet::validateContentAttribution(const RuleContentAttribution& attribution, bool publicBundle) const
    {
        RuleContentGateResult result;
        if (publicBundle && attribution.bundled)
        {
            if (attribution.sourceName.empty())
                result.errors.push_back("Bundled public content requires a source name.");
            if (attribution.licenseName.empty())
                result.errors.push_back("Bundled public content requires a license name.");
            if (attribution.attributionText.empty())
                result.errors.push_back("Bundled public content requires attribution text.");
        }
        result.allowed = result.errors.empty();
        return result;
    }

inline const std::vector<RuleSkillDefinition> IRuleSet::getSkills() const
{ return {}; }

inline const std::vector<RuleDifficultyClass> IRuleSet::getCommonDifficultyClasses() const
{ return {}; }

inline const std::vector<RuleConditionDefinition> IRuleSet::getConditionDefinitions() const
{ return {}; }

inline RuleConditionDefinition IRuleSet::findConditionDefinition(const std::string& idOrName) const
    {
        const std::vector<RuleConditionDefinition> definitions = getConditionDefinitions();
        for (const RuleConditionDefinition& condition : definitions)
        {
            if (condition.id == idOrName || condition.name == idOrName)
            {
                RuleConditionDefinition result = condition;
                result.valid = true;
                return result;
            }
        }
        return {};
    }

inline RuleActiveCondition IRuleSet::createActiveCondition(const std::string& idOrName, const std::string& sourceEntityId, const std::string& sourceName, int remainingRounds) const
    {
        RuleActiveCondition activeCondition;
        activeCondition.id = idOrName;
        activeCondition.name = idOrName;
        activeCondition.sourceEntityId = sourceEntityId;
        activeCondition.sourceName = sourceName;
        activeCondition.remainingRounds = remainingRounds;
        const std::vector<RuleConditionDefinition> definitions = getConditionDefinitions();
        for (const RuleConditionDefinition& definition : definitions)
        {
            if (definition.id == idOrName || definition.name == idOrName)
            {
                activeCondition.id = definition.id;
                activeCondition.name = definition.name;
                activeCondition.duration = definition.defaultDuration;
                activeCondition.endCondition = definition.endCondition;
                return activeCondition;
            }
        }
        activeCondition.duration = remainingRounds >= 0 ? std::to_string(remainingRounds) + " rounds" : "Until removed";
        activeCondition.endCondition = "Removed manually.";
        return activeCondition;
    }

inline RuleActiveCondition IRuleSet::advanceConditionRound(RuleActiveCondition condition) const
    {
        if (condition.remainingRounds > 0)
        {
            --condition.remainingRounds;
            if (condition.remainingRounds == 0)
                condition.active = false;
        }
        return condition;
    }

inline std::string IRuleSet::describeActiveCondition(const RuleActiveCondition& condition) const
    {
        std::string description = condition.name.empty() ? condition.id : condition.name;
        if (!condition.sourceName.empty())
            description += " from " + condition.sourceName;
        if (condition.remainingRounds >= 0)
            description += " (" + std::to_string(condition.remainingRounds) + " rounds)";
        else if (!condition.duration.empty())
            description += " (" + condition.duration + ")";
        return description;
    }
