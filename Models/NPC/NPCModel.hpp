#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include "Models/Combat/EncounterModel.hpp"
#include "Models/Inventory/InventoryModels.hpp"
#include "Models/Magic/SpellModels.hpp"
#include <map>
#include <string>
#include <vector>

enum class NPCCategory
{
    Person,
    Creature,
    FactionEntity,
    Deity
};

inline const char *npcCategoryLabel(NPCCategory category)
{
    switch (category)
    {
    case NPCCategory::Person:
        return "Person";
    case NPCCategory::Creature:
        return "Creature";
    case NPCCategory::FactionEntity:
        return "Faction Entity";
    case NPCCategory::Deity:
        return "Deity";
    }
    return "NPC";
}

struct NPCModel
{
    EntityMetadata metadata{"npc"};
    NPCCategory category = NPCCategory::Person;
    EntityId factionId;
    EntityId deityId;
    std::string name;
    std::string description;
    std::string faction;
    std::string role;
    std::string attitudeTowardParty;
    std::string secrets;
    std::string goals;
    std::string resources;
    std::string threatLocation;
    std::string relationships;
    std::string dialogueNotes;
    int threatLevel = 0;
    std::map<std::string, int> attributes;
    std::vector<std::string> conditions;
    std::vector<EntityId> inventoryIds;
    InventoryModel inventory;
    CharacterSpellcastingModel spellcasting;
    std::vector<ResourceTrackerModel> resourceTrackers;
};

struct FactionModel
{
    EntityMetadata metadata{"faction"};
    std::string name;
    std::string type = "Faction";
    std::string summary;
    std::string headquarters;
    std::string leaderNpcId;
    std::string attitudeTowardParty;
    std::string goals;
    std::string resources;
    std::string gmNotes;
    int influence = 0;
    int threatLevel = 0;
};

struct DeityModel
{
    EntityMetadata metadata{"deity"};
    std::string name;
    std::string powerType = "Deity";
    std::string pantheon;
    std::string domains;
    std::string symbol;
    std::string alignment;
    std::string tenets;
    std::string worshipers;
    std::string relationships;
    std::string gmNotes;
    int influence = 0;
    int threatLevel = 0;
};
