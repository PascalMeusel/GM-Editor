#pragma once

#include "Models/Rulesets/RuleSetInventoryTypes.hpp"

struct RuleActionEconomyState
{
    bool actionAvailable = true;
    bool bonusActionAvailable = true;
    bool reactionAvailable = true;
    bool freeObjectInteractionAvailable = true;
    bool concentrating = false;
    std::string concentrationSource;
    int movementAvailableFeet = 30;
    int movementUsedFeet = 0;
};

struct RuleActionSpendResult
{
    bool success = false;
    std::string message;
    RuleActionEconomyState state;
};

struct RuleConcentrationCheck
{
    int damageTaken = 0;
    int dc = 10;
    RuleCheckRequest savingThrow;
};

struct RuleConcentrationResult
{
    RuleConcentrationCheck check;
    RuleCheckRollResult roll;
    bool concentrationMaintained = false;
    bool concentrationBroken = false;
};

struct RuleAreaTemplate
{
    std::string shape;
    int sizeFeet = 0;
    std::string rawText;
};

struct RuleSpellResolutionSummary
{
    std::string spellId;
    std::string spellName;
    SpellResolutionType resolution = SpellResolutionType::Utility;
    int baseSpellLevel = 0;
    int castAtLevel = 0;
    int attackBonus = 0;
    int saveDc = 0;
    std::string savingThrowAbility;
    std::string damage;
    std::string healing;
    std::string scaling;
    std::string duration;
    bool concentration = false;
    bool ongoingEffect = false;
    RuleAreaTemplate area;
};

struct RuleWeaponProfile
{
    std::string name;
    RuleWeaponAttackKind attackKind = RuleWeaponAttackKind::Unknown;
    std::string primaryAbility = "Strength";
    std::string damage;
    std::string damageType;
    std::string versatileDamage;
    std::string ammunitionType;
    std::string mastery;
    int reachFeet = 5;
    int normalRangeFeet = 5;
    int longRangeFeet = 5;
    bool finesse = false;
    bool ammunition = false;
    bool loading = false;
    bool heavy = false;
    bool light = false;
    bool twoHanded = false;
    bool thrown = false;
    bool versatile = false;
    std::vector<std::string> properties;
    std::vector<std::string> warnings;
};

// Zero values mean this ruleset requires an explicit manual value. Starting
// HP is separate because some rulesets have a different first-level rule.
struct RuleAdvancementProfile
{
    int fixedHitPointsPerLevel = 0;
    int startingHitPoints = 0;
    std::string hitPointAbility;
    int maximumLevel = 1000;
};
