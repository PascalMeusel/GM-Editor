#pragma once

#include <algorithm>
#include <map>
#include <sstream>
#include <vector>
#include <string>
#include "Models/Combat/EncounterModel.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Magic/SpellModels.hpp"

enum class RuleProficiencyTier
{
    None,
    Half,
    Proficient,
    Expertise
};

enum class RuleRollMode
{
    Normal,
    Advantage,
    Disadvantage
};

struct RuleSkillDefinition
{
    std::string id;
    std::string name;
    std::string ability;
};

struct RuleDifficultyClass
{
    std::string label;
    int value = 10;
};

struct RuleConditionDefinition
{
    std::string id;
    std::string name;
    std::string summary;
    std::vector<std::string> mechanicalEffects;
    std::string defaultDuration;
    std::string endCondition;
    bool valid = false;
};

struct RuleActiveCondition
{
    std::string id;
    std::string name;
    std::string sourceEntityId;
    std::string sourceName;
    std::string duration;
    std::string endCondition;
    int remainingRounds = -1;
    bool active = true;
};

struct RuleDiceSummary
{
    std::string expression;
    bool valid = false;
    int minimum = 0;
    int maximum = 0;
    double average = 0.0;
    int flatModifier = 0;
};

struct RuleDiceRollResult
{
    RuleDiceSummary summary;
    bool valid = false;
    std::vector<int> rolls;
    int total = 0;
    std::string error;
};

struct RuleCheckRequest
{
    std::string ability;
    RuleProficiencyTier proficiency = RuleProficiencyTier::None;
    RuleRollMode rollMode = RuleRollMode::Normal;
    int level = 1;
    int situationalModifier = 0;
    int dc = 0;
};

struct RuleCheckResult
{
    int modifier = 0;
    int passiveScore = 10;
    int dc = 0;
    RuleRollMode rollMode = RuleRollMode::Normal;
};

struct RuleCheckRollResult
{
    RuleCheckResult check;
    bool valid = false;
    int firstD20 = 0;
    int secondD20 = 0;
    int chosenD20 = 0;
    int total = 0;
    bool success = false;
    bool naturalOne = false;
    bool naturalTwenty = false;
    std::string error;
};

struct RuleArmorInput
{
    std::map<std::string, int> attributes;
    const InventoryModel* inventory = nullptr;
    const std::vector<ItemModel>* items = nullptr;
    std::string className;
    int naturalArmorClass = 0;
    bool allowUnarmoredDefense = true;
};

struct RuleArmorSummary
{
    int armorClass = 10;
    int damageReduction = 0;
    std::vector<std::string> sources;
    std::vector<std::string> warnings;
};

