#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include "Models/Map/MapModels.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

enum class CombatantKind
{
    Player,
    NPC,
    Monster
};

enum class ActiveEffectKind
{
    Buff,
    Debuff,
    Condition,
    Aura,
    SummonedZone,
    Other
};

enum class CombatPhase
{
    Setup,
    Initiative,
    ActiveTurn,
    Movement,
    ActionResolution,
    ReactionsEffects,
    EndTurn,
    PausedBreak,
    WrapUp
};

struct ActiveEffectModel
{
    EntityMetadata metadata{"active_effect"};
    ActiveEffectKind kind = ActiveEffectKind::Other;
    std::string name;
    std::string sourceName;
    EntityId sourceEntityId;
    EntityId sourceSpellId;
    std::string notes;
    int remainingRounds = -1;
    bool concentration = false;
    bool active = true;
};

inline const char *activeEffectKindLabel(ActiveEffectKind kind)
{
    switch (kind)
    {
    case ActiveEffectKind::Buff:
        return "Buff";
    case ActiveEffectKind::Debuff:
        return "Debuff";
    case ActiveEffectKind::Condition:
        return "Condition";
    case ActiveEffectKind::Aura:
        return "Aura";
    case ActiveEffectKind::SummonedZone:
        return "Summoned Zone";
    case ActiveEffectKind::Other:
        return "Other";
    }
    return "Other";
}

inline const char *combatPhaseStorageValue(CombatPhase phase)
{
    switch (phase)
    {
    case CombatPhase::Setup:
        return "setup";
    case CombatPhase::Initiative:
        return "initiative";
    case CombatPhase::ActiveTurn:
        return "active_turn";
    case CombatPhase::Movement:
        return "movement";
    case CombatPhase::ActionResolution:
        return "action_resolution";
    case CombatPhase::ReactionsEffects:
        return "reactions_effects";
    case CombatPhase::EndTurn:
        return "end_turn";
    case CombatPhase::PausedBreak:
        return "paused_break";
    case CombatPhase::WrapUp:
        return "wrap_up";
    }
    return "setup";
}

inline CombatPhase combatPhaseFromStorageValue(const std::string &value)
{
    if (value == "initiative")
        return CombatPhase::Initiative;
    if (value == "active_turn")
        return CombatPhase::ActiveTurn;
    if (value == "movement")
        return CombatPhase::Movement;
    if (value == "action_resolution")
        return CombatPhase::ActionResolution;
    if (value == "reactions_effects")
        return CombatPhase::ReactionsEffects;
    if (value == "end_turn")
        return CombatPhase::EndTurn;
    if (value == "paused_break")
        return CombatPhase::PausedBreak;
    if (value == "wrap_up")
        return CombatPhase::WrapUp;
    return CombatPhase::Setup;
}

inline const char *combatPhaseDisplayLabel(CombatPhase phase)
{
    switch (phase)
    {
    case CombatPhase::Setup:
        return "Setup";
    case CombatPhase::Initiative:
        return "Initiative";
    case CombatPhase::ActiveTurn:
        return "Active Turn";
    case CombatPhase::Movement:
        return "Movement";
    case CombatPhase::ActionResolution:
        return "Action Resolution";
    case CombatPhase::ReactionsEffects:
        return "Reactions / Effects";
    case CombatPhase::EndTurn:
        return "End Turn";
    case CombatPhase::PausedBreak:
        return "Paused / Break";
    case CombatPhase::WrapUp:
        return "Wrap-up";
    }
    return "Setup";
}

inline bool combatPhaseIsPaused(CombatPhase phase)
{
    return phase == CombatPhase::PausedBreak || phase == CombatPhase::WrapUp;
}

struct MonsterActionModel
{
    std::string name;
    std::string actionKind = "Attack";
    std::string sourceLabel;
    EntityId sourceEntityId;
    EntityId linkedResourceId;
    int reachFeet = 5;
    int normalRangeFeet = 5;
    int rangeFeet = 5;
    std::string attackBonus;
    std::string damage;
    std::string notes;
    EntityId sourceSpellId;
    int spellLevel = 0;
    bool consumesSpellSlot = false;
};

struct ResourceTrackerModel
{
    EntityMetadata metadata{"resource_tracker"};
    EntityId definitionId;
    std::string name = "Resource";
    int maximum = 1;
    int used = 0;
    std::string rechargeRule = "Long Rest";
    std::string notes;
};

inline void normalizeResourceTracker(ResourceTrackerModel &tracker)
{
    tracker.maximum = std::max(0, tracker.maximum);
    tracker.used = std::clamp(tracker.used, 0, tracker.maximum);
    if (tracker.name.empty())
        tracker.name = "Resource";
}

struct MonsterDefinitionModel
{
    EntityMetadata metadata{"monster"};
    EntityId catalogSourceId;
    EntityId baseCatalogMonsterId;
    std::string name;
    std::string source;
    std::string sourceAttribution;
    std::string size;
    std::string type;
    std::string challengeRating;
    int armorClass = 10;
    int armorDamageReduction = 0;
    int hitPoints = 1;
    int speedFeet = 30;
    int experience = 0;
    std::string traits;
    std::string reactions;
    std::string legendaryActions;
    std::string lairActions;
    std::string rechargeActions;
    std::string senses;
    std::string languages;
    std::string savingThrows;
    std::string skills;
    std::string damageResistances;
    std::string damageImmunities;
    std::string damageVulnerabilities;
    std::string conditionImmunities;
    std::vector<std::string> encounterRoleTags;
    std::map<std::string, int> attributes;
    std::vector<MonsterActionModel> actions;
};

struct EncounterParticipantModel
{
    EntityMetadata metadata{"combatant"};
    CombatantKind kind = CombatantKind::Monster;
    EntityId sourceEntityId;
    EntityId monsterDefinitionId;
    std::string displayName;
    int initiative = 0;
    int currentHitPoints = 0;
    int maxHitPoints = 0;
    int temporaryHitPoints = 0;
    int armorClass = 10;
    int armorDamageReduction = 0;
    int speedFeet = 30;
    std::string groupName;
    int waveNumber = 1;
    bool startsHidden = false;
    int reinforcementRound = 0;
    int startingX = 0;
    int startingY = 0;
    int x = 0;
    int y = 0;
    int turnStartX = 0;
    int turnStartY = 0;
    int movementUsedFeet = 0;
    bool defeated = false;
    bool friendly = false;
    std::string narrativePosition;
    std::string narrativeRangeBand = "Near";
    bool narrativeSpotlight = false;
    std::vector<std::string> damageResistances;
    std::vector<std::string> damageVulnerabilities;
    std::vector<std::string> damageImmunities;
    std::vector<std::string> conditions;
    std::vector<ActiveEffectModel> activeEffects;
    std::vector<MonsterActionModel> actions;
};

struct CombatLogEntryModel
{
    EntityMetadata metadata{"combat_log"};
    int round = 1;
    std::string actorName;
    std::string eventType;
    std::string summary;
};

struct DiceRollPresetModel
{
    EntityMetadata metadata{"dice_roll_preset"};
    std::string name = "Roll";
    std::string expression = "1d20";
    std::string notes;
};

struct EncounterModel
{
    EntityMetadata metadata{"encounter"};
    std::string name = "New Encounter";
    std::string description;
    EntityId mapId;
    EntityId sourceMapId;
    MapNormalizedRect sourceMapRect;
    std::uint64_t sourceMapRevision = 0;
    std::optional<EntityId> tacticalMapId;
    std::string mapContextName;
    int mapContextX = 0;
    int mapContextY = 0;
    int battlefieldWidth = 16;
    int battlefieldHeight = 12;
    int cellSizeFeet = 5;
    bool useMapCells = false;
    bool theaterOfMind = false;
    bool combatPaused = false;
    CombatPhase combatPhase = CombatPhase::Setup;
    int round = 1;
    std::vector<MapCellModel> battlefieldCells;
    std::vector<EncounterParticipantModel> participants;
    std::vector<CombatLogEntryModel> combatLog;
    std::vector<DiceRollPresetModel> diceRollPresets;
    std::string afterCombatSummary;
    std::string afterCombatLootNotes;
    std::string afterCombatStoryConsequences;
    std::string afterCombatSessionNotes;
    int activeParticipantIndex = 0;
};

