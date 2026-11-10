#pragma once

#include "Models/Rulesets/RuleSetCoreTypes.hpp"

enum class RuleWeaponAttackKind
{
    Melee,
    Ranged,
    Thrown,
    MeleeOrThrown,
    Unknown
};

enum class RuleActionResource
{
    Action,
    BonusAction,
    Reaction,
    Movement,
    FreeObjectInteraction,
    Concentration
};

enum class RuleRestType
{
    ShortRest,
    LongRest
};

struct RuleRestRequest
{
    RuleRestType type = RuleRestType::LongRest;
    int currentHitPoints = 0;
    int maxHitPoints = 0;
    int hitDiceSpent = 0;
    int hitDiceAvailable = 0;
    CharacterSpellcastingModel spellcasting;
};

struct RuleRestResult
{
    RuleRestType type = RuleRestType::LongRest;
    int currentHitPoints = 0;
    int hitDiceAvailable = 0;
    CharacterSpellcastingModel spellcasting;
    std::vector<std::string> recoveredResources;
    std::vector<std::string> notes;
};

struct RuleDamageApplication
{
    int currentHitPoints = 0;
    int maxHitPoints = 0;
    int temporaryHitPoints = 0;
    int amount = 0;
    std::string damageType;
    std::vector<std::string> resistances;
    std::vector<std::string> vulnerabilities;
    std::vector<std::string> immunities;
    bool healing = false;
};

struct RuleDamageResult
{
    int currentHitPoints = 0;
    int temporaryHitPoints = 0;
    int effectiveAmount = 0;
    bool defeated = false;
    bool massiveDamageRisk = false;
    std::vector<std::string> notes;
};

struct RuleDeathSaveState
{
    int successes = 0;
    int failures = 0;
    bool stable = false;
    bool dead = false;
};

struct RuleDeathSaveResult
{
    RuleDeathSaveState state;
    bool valid = false;
    std::string message;
};

struct RuleEncounterDifficultySummary
{
    int partySize = 0;
    int partyLevelTotal = 0;
    int adjustedExperience = 0;
    int easyThreshold = 0;
    int mediumThreshold = 0;
    int hardThreshold = 0;
    int deadlyThreshold = 0;
    std::string difficulty = "Trivial";
};

struct RuleRewardSummary
{
    int experience = 0;
    bool milestoneAvailable = false;
    std::string milestoneLabel;
    std::vector<std::string> notes;
};

struct RuleTreasureParcelItem
{
    std::string name;
    int quantity = 1;
    int valueCopper = 0;
    double weight = 0.0;
    std::string notes;
};

struct RuleCurrencyDefinition
{
    std::string id;
    std::string name;
    std::string code;
    int baseUnitValue = 1;
};

struct RuleCarryingCapacitySummary
{
    double carriedWeight = 0.0;
    double capacityWeight = 0.0;
    bool capacityKnown = false;
    bool encumbered = false;
    bool overCapacity = false;
    std::vector<std::string> warnings;
};

struct RuleEquipmentSlotDefinition
{
    std::string id;
    std::string label;
    int maxEquipped = 1;
};

struct RuleEquipmentSlotValidation
{
    bool valid = true;
    std::vector<std::string> warnings;
};

struct RuleTreasureParcel
{
    std::string label = "Treasure Parcel";
    std::string sourceId;
    bool awarded = false;
    std::string awardedTo;
    int copperPieces = 0;
    int silverPieces = 0;
    int electrumPieces = 0;
    int goldPieces = 0;
    int platinumPieces = 0;
    std::vector<RuleTreasureParcelItem> items;
    std::vector<std::string> notes;

    int totalCoinValueCopper() const
    {
        return copperPieces + silverPieces * 10 + electrumPieces * 50 + goldPieces * 100 + platinumPieces * 1000;
    }

    int totalValueCopper() const
    {
        int total = totalCoinValueCopper();
        for (const RuleTreasureParcelItem& item : items)
            total += std::max(0, item.quantity) * std::max(0, item.valueCopper);
        return total;
    }
};

struct RuleLootGenerationRequest
{
    std::string label;
    std::string sourceId;
    std::vector<int> partyLevels;
    std::vector<std::string> sourceChallengeRatings;
    int targetValueCopper = 0;
    bool includeCoins = true;
    std::vector<RuleTreasureParcelItem> items;
    std::vector<std::string> notes;
};

struct RuleContentAttribution
{
    std::string contentId;
    std::string contentType;
    std::string sourceName;
    std::string sourceVersion;
    std::string sourceUrl;
    std::string licenseName;
    std::string licenseUrl;
    std::string attributionText;
    std::string importedOn;
    bool bundled = false;
};

struct RuleContentGateResult
{
    bool allowed = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

