#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <string>
#include <vector>

struct RulesetAbilityScoreDefinition
{
    std::string id;
    std::string name;
    std::string abbreviation;
    std::string description;
};

struct RulesetReference
{
    std::string rulesetId;
    std::string version;
    std::string overlayId;
    std::string overlayVersion;

    bool empty() const { return rulesetId.empty(); }
};

struct RulesetDerivedStatDefinition
{
    std::string id;
    std::string name;
    std::string formula;
    std::string description;
};

struct RulesetSkillDefinition
{
    std::string id;
    std::string name;
    std::string abilityScoreId;
    std::string description;
};

struct RulesetDefenseDefinition
{
    std::string id;
    std::string name;
    std::string formula;
    std::string description;
};

struct RulesetResourceDefinition
{
    std::string id;
    std::string name;
    std::string maximumFormula;
    std::string recoveryFormula;
    std::string recoveryTrigger = "Long Rest";
    std::string description;
};

struct RulesetDiceFormulaDefinition
{
    std::string id;
    std::string name;
    std::string formula;
    std::string description;
};

struct RulesetCurrencyDefinition
{
    std::string id;
    std::string name;
    std::string code;
    int baseUnitValue = 1;
    std::string description;
};

struct RulesetSizeDefinition
{
    std::string id;
    std::string name;
    int spaceFeet = 5;
    std::string description;
};

struct RulesetSpeedDefinition
{
    std::string id;
    std::string name;
    std::string unit = "feet";
    std::string description;
};

struct RulesetConditionDefinition
{
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> mechanicalEffects;
    std::string endCondition;
};

struct RulesetClassDefinition
{
    std::string id;
    std::string name;
    std::string hitDie;
    std::vector<std::string> primaryAbilityScoreIds;
    std::string description;
};

struct RulesetSubclassDefinition
{
    std::string id;
    std::string name;
    std::string parentClassId;
    std::string description;
};

struct RulesetAncestryDefinition
{
    std::string id;
    std::string name;
    std::string sizeId;
    std::string speedId;
    std::string description;
};

struct RulesetBackgroundDefinition
{
    std::string id;
    std::string name;
    std::vector<std::string> skillIds;
    std::string description;
};

struct RulesetFeatureDefinition
{
    std::string id;
    std::string name;
    std::string featureType;
    int level = 1;
    std::string description;
};

struct RulesetEquipmentCategoryDefinition
{
    std::string id;
    std::string name;
    std::string slotType;
    std::string description;
};

struct RulesetSpellTraditionDefinition
{
    std::string id;
    std::string name;
    std::string abilityScoreId;
    std::string description;
};

struct RulesetMonsterTemplateDefinition
{
    std::string id;
    std::string name;
    std::string sizeId;
    std::string challengeFormula;
    std::string description;
};

struct RulesetDefinitionModel
{
    EntityMetadata metadata{"ruleset_definition"};
    // Logical ID shared by all immutable versions. metadata.id identifies the stored row/version.
    std::string rulesetId;
    std::string name;
    std::string version;
    std::string baseRulesetId;
    std::string definitionJson;
    int schemaVersion = 2;
    std::string abilityModifierFormula = "(score - 10) / 2";
    std::string proficiencyFormula = "2 + (level - 1) / 4";
    std::string checkDiceFormula = "1d20";
    int passiveScoreBase = 10;
    bool published = false;
    std::string publishedAtUtc;
    bool installedGlobally = false;
    std::vector<RulesetAbilityScoreDefinition> abilityScores;
    std::vector<RulesetDerivedStatDefinition> derivedStats;
    std::vector<RulesetSkillDefinition> skills;
    std::vector<RulesetDefenseDefinition> defenses;
    std::vector<RulesetResourceDefinition> resources;
    std::vector<RulesetDiceFormulaDefinition> diceFormulas;
    std::vector<RulesetCurrencyDefinition> currencies;
    std::vector<RulesetSizeDefinition> sizes;
    std::vector<RulesetSpeedDefinition> speeds;
    std::vector<RulesetConditionDefinition> conditions;
    std::vector<RulesetClassDefinition> classes;
    std::vector<RulesetSubclassDefinition> subclasses;
    std::vector<RulesetAncestryDefinition> ancestries;
    std::vector<RulesetBackgroundDefinition> backgrounds;
    std::vector<RulesetFeatureDefinition> features;
    std::vector<RulesetEquipmentCategoryDefinition> equipmentCategories;
    std::vector<RulesetSpellTraditionDefinition> spellTraditions;
    std::vector<RulesetMonsterTemplateDefinition> monsterTemplates;
};
