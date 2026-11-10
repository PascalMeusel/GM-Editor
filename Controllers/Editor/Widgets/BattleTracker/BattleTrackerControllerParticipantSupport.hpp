#pragma once
#include "BattleTrackerControllerCombatSupport.hpp"

namespace
{
bool participantIsConcentrating(const EncounterParticipantModel &participant)
{
    for (const std::string &condition : participant.conditions)
    {
        if (lowerCopy(condition).find("concentrat") != std::string::npos)
            return true;
    }
    return false;
}

std::string combatantActorName(const EncounterModel &encounter)
{
    if (encounter.participants.empty())
        return {};
    const int index = std::clamp(encounter.activeParticipantIndex, 0, static_cast<int>(encounter.participants.size()) - 1);
    return encounter.participants[static_cast<std::size_t>(index)].displayName;
}


void refreshHitPointBounds(EncounterParticipantModel &participant, int newMaxHitPoints)
{
    const bool hadNoStoredMaximum = participant.maxHitPoints <= 0;
    participant.maxHitPoints = std::max(1, newMaxHitPoints);
    if (hadNoStoredMaximum)
        participant.currentHitPoints = participant.maxHitPoints;
    else
        participant.currentHitPoints = std::clamp(participant.currentHitPoints, 0, participant.maxHitPoints);
    participant.defeated = participant.currentHitPoints == 0;
}

void syncPlayerHealthFromParticipant(Campaign &campaign, const EncounterParticipantModel &participant)
{
    syncEncounterPlayerHealthDraft(campaign, participant);
}

const CharacterSpellcastingModel *findParticipantSpellcasting(const Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        const auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == campaign.characters.end() ? nullptr : &characterIt->spellcasting;
    }

    if (participant.kind == CombatantKind::NPC)
    {
        const auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == campaign.npcs.end() ? nullptr : &npcIt->spellcasting;
    }

    return nullptr;
}

CharacterSpellcastingModel *findParticipantSpellcasting(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == campaign.characters.end() ? nullptr : &characterIt->spellcasting;
    }

    if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == campaign.npcs.end() ? nullptr : &npcIt->spellcasting;
    }

    return nullptr;
}

std::vector<ResourceTrackerModel> *findParticipantResources(Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == campaign.characters.end() ? nullptr : &characterIt->resourceTrackers;
    }

    if (participant.kind == CombatantKind::NPC)
    {
        auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == campaign.npcs.end() ? nullptr : &npcIt->resourceTrackers;
    }

    return nullptr;
}

const std::vector<ResourceTrackerModel> *findParticipantResources(const Campaign &campaign, const EncounterParticipantModel &participant)
{
    if (participant.kind == CombatantKind::Player)
    {
        const auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        return characterIt == campaign.characters.end() ? nullptr : &characterIt->resourceTrackers;
    }

    if (participant.kind == CombatantKind::NPC)
    {
        const auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        return npcIt == campaign.npcs.end() ? nullptr : &npcIt->resourceTrackers;
    }

    return nullptr;
}

struct ParticipantSpellcasterSnapshot
{
    CharacterSpellcastingModel spellcasting;
    std::map<std::string, int> attributes;
    int level = 1;
    bool valid = false;
};

ParticipantSpellcasterSnapshot getParticipantSpellcasterSnapshot(const Campaign &campaign, const EncounterParticipantModel &participant)
{
    ParticipantSpellcasterSnapshot snapshot;
    if (participant.kind == CombatantKind::Player)
    {
        const auto characterIt = std::find_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &character) {
            return character.metadata.id == participant.sourceEntityId;
        });
        if (characterIt == campaign.characters.end())
            return snapshot;

        snapshot.spellcasting = characterIt->spellcasting;
        snapshot.attributes = characterIt->attributes;
        snapshot.level = characterIt->level;
        snapshot.valid = true;
        return snapshot;
    }

    if (participant.kind == CombatantKind::NPC)
    {
        const auto npcIt = std::find_if(campaign.npcs.begin(), campaign.npcs.end(), [&](const NPCModel &npc) {
            return npc.metadata.id == participant.sourceEntityId;
        });
        if (npcIt == campaign.npcs.end())
            return snapshot;

        snapshot.spellcasting = npcIt->spellcasting;
        snapshot.attributes = npcIt->attributes;
        snapshot.level = 1;
        snapshot.valid = true;
    }

    return snapshot;
}

bool findAvailableSpellById(const Campaign &campaign, const EntityId &spellId, SpellModel &spell)
{
    const std::vector<SpellModel> spells = getAvailableSpells(campaign);
    const SpellModel *foundSpell = findSpellById(spells, spellId);
    if (!foundSpell)
        return false;

    spell = *foundSpell;
    return true;
}

std::vector<MonsterActionModel> getInventoryPowerActions(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    std::vector<MonsterActionModel> actions;
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (!entry.equipped && !entry.attuned)
            continue;
        const ItemModel *item = findItemById(items, entry.itemId);
        if (!item)
            continue;
        for (MonsterActionModel action : item->powers)
        {
            if (action.name.empty())
                action.name = item->name.empty() ? combatText("Item Power") : item->name;
            if (action.actionKind.empty() || action.actionKind == "Attack")
                action.actionKind = "Item Power";
            action.sourceLabel = item->name;
            action.sourceEntityId = item->metadata.id;
            actions.push_back(action);
        }
    }
    return actions;
}
}
