#pragma once
#include "Models/Core/EntityMetadata.hpp"
#include "Models/Combat/EncounterModel.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Magic/SpellModels.hpp"
#include <algorithm>
#include <string>
#include <vector>
#include <map>

struct CharacterRelationshipModel
{
    EntityMetadata metadata{"character_relationship"};
    std::string targetEntityType;
    EntityId targetEntityId;
    std::string label;
    std::string notes;
};

struct CharacterModel
{
    EntityMetadata metadata{"character"};
    char nameBuffer[128] = "Hero";
    int level = 1;
    // Empty on legacy characters; advancement infers their single primary class.
    std::map<std::string, int> classLevels;

    // Use a map to store attributes dynamically. Key = attribute name, Value = score.
    std::map<std::string, int> attributes;
    // Stable ruleset-definition IDs. Legacy display-name attributes remain for
    // backwards compatibility and are mirrored during campaign migration.
    std::map<std::string, int> ruleValues;

    // Store the selected class and race as strings
    std::string selectedClass;
    std::string selectedRace;
    std::string background;
    std::string backstory;
    std::string playerNotes;
    std::string gmNotes;
    std::vector<CharacterRelationshipModel> relationships;
    std::vector<std::string> feats;
    std::vector<std::string> traitsAndFeatures;
    std::vector<MonsterActionModel> featureActions;
    std::vector<ResourceTrackerModel> resourceTrackers;
    std::vector<std::string> languages;
    std::vector<std::string> senses;
    std::vector<std::string> savingThrowProficiencies;
    std::vector<std::string> skillProficiencies;
    std::vector<std::string> toolProficiencies;
    int speedFeet = 30;
    int currentHitPoints = 0;
    int maxHitPoints = 0;
    // Canonical temporary HP. Encounter participants synchronize to this
    // value so player-safe summaries never have to consult combat-only data.
    int temporaryHitPoints = 0;
    EntityId portraitAssetId;
    std::vector<EntityId> inventoryIds;
    InventoryModel inventory;
    CharacterSpellcastingModel spellcasting;
    std::vector<std::string> conditions;
};

inline int characterAbilityModifier(const CharacterModel &character, const std::string &name)
{
    const auto it = character.attributes.find(name);
    if (it == character.attributes.end())
        return 0;
    return it->second >= 10 ? (it->second - 10) / 2 : (it->second - 11) / 2;
}

inline int calculateDefaultCharacterMaxHitPoints(const CharacterModel &character)
{
    const int level = std::max(1, character.level);
    return std::max(1, level * 8 + characterAbilityModifier(character, "Constitution") * level);
}

inline void normalizeCharacterHitPoints(CharacterModel &character)
{
    const bool initializeHitPoints = character.maxHitPoints <= 0;
    if (character.maxHitPoints <= 0)
        character.maxHitPoints = calculateDefaultCharacterMaxHitPoints(character);
    character.maxHitPoints = std::max(1, character.maxHitPoints);
    // Zero is a valid wounded character state once a maximum has been set.
    // Only fresh/legacy characters without HP data receive starting health.
    if (initializeHitPoints && character.currentHitPoints <= 0)
        character.currentHitPoints = character.maxHitPoints;
    character.currentHitPoints = std::clamp(character.currentHitPoints, 0, character.maxHitPoints);
    character.temporaryHitPoints = std::max(0, character.temporaryHitPoints);
}

inline void adjustCharacterHitPoints(CharacterModel &character, int delta)
{
    normalizeCharacterHitPoints(character);
    long long change = delta;
    if (change < 0)
    {
        const auto absorbed = std::min<long long>(character.temporaryHitPoints, -change);
        character.temporaryHitPoints -= static_cast<int>(absorbed);
        change += absorbed;
    }
    character.currentHitPoints = static_cast<int>(std::clamp<long long>(
        static_cast<long long>(character.currentHitPoints) + change, 0, character.maxHitPoints));
}
