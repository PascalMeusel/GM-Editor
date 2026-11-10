#include "Models/DataBaseManager.hpp"
#include <string>
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectLogger.hpp"
#include "Models/Rulesets/RulesetDefinitionSerialization.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "Models/Rulesets/Clockwork/ClockworkBloodline.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Models/Persistence/DatabaseRows.hpp"

using namespace PersistenceSerialization;

void DatabaseManager::loadCampaignCharacters(Campaign &campaign)
{
    SQLite::Statement query_chars(*m_db, R"(
                    SELECT stable_id, name, level, class, race, background, attributes, tags, created_at, updated_at, backstory, player_notes, gm_notes, character_relationships, feats, traits_features, languages, senses, saving_throw_proficiencies, skill_proficiencies, tool_proficiencies, speed_feet, current_hp, max_hp, temporary_hp, portrait_asset_id, inventory_entries, conditions, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries, feature_actions, resource_trackers, rule_values, class_levels, spell_attack_mode, spell_save_dc_mode
                    FROM characters
                    WHERE campaign_id = ?;
                )");
    query_chars.bind(1, static_cast<int64_t>(campaign.id));

    while (query_chars.executeStep())
    {
        CharacterModel character;
        if (!query_chars.getColumn(0).isNull())
            character.metadata.id = query_chars.getColumn(0).getString();

        std::string name = query_chars.getColumn(1).getString();
#if defined(_WIN32)
        strncpy_s(character.nameBuffer, sizeof(character.nameBuffer), name.c_str(), _TRUNCATE);
#else
        strncpy(character.nameBuffer, name.c_str(), sizeof(character.nameBuffer) - 1);
        character.nameBuffer[sizeof(character.nameBuffer) - 1] = '\0'; // Ensure null-termination
#endif

        character.level = query_chars.getColumn(2).getInt();
        character.selectedClass = query_chars.getColumn(3).getString();
        character.selectedRace = query_chars.getColumn(4).getString();

        if (!query_chars.getColumn(5).isNull())
            character.background = query_chars.getColumn(5).getString();

        std::string serialized_attributes = query_chars.getColumn(6).getString();
        character.attributes = deserializeAttributes(serialized_attributes);
        if (!query_chars.getColumn(7).isNull())
            character.metadata.tags = deserializeStringList(query_chars.getColumn(7).getString());
        if (!query_chars.getColumn(8).isNull())
            character.metadata.createdAtUtc = query_chars.getColumn(8).getString();
        if (!query_chars.getColumn(9).isNull())
            character.metadata.updatedAtUtc = query_chars.getColumn(9).getString();
        if (!query_chars.getColumn(10).isNull())
            character.backstory = query_chars.getColumn(10).getString();
        if (!query_chars.getColumn(11).isNull())
            character.playerNotes = query_chars.getColumn(11).getString();
        if (!query_chars.getColumn(12).isNull())
            character.gmNotes = query_chars.getColumn(12).getString();
        if (!query_chars.getColumn(13).isNull())
            character.relationships = deserializeCharacterRelationships(query_chars.getColumn(13).getString());
        if (!query_chars.getColumn(14).isNull())
            character.feats = deserializeStringList(query_chars.getColumn(14).getString());
        if (!query_chars.getColumn(15).isNull())
            character.traitsAndFeatures = deserializeStringList(query_chars.getColumn(15).getString());
        if (!query_chars.getColumn(16).isNull())
            character.languages = deserializeStringList(query_chars.getColumn(16).getString());
        if (!query_chars.getColumn(17).isNull())
            character.senses = deserializeStringList(query_chars.getColumn(17).getString());
        if (!query_chars.getColumn(18).isNull())
            character.savingThrowProficiencies = deserializeStringList(query_chars.getColumn(18).getString());
        if (!query_chars.getColumn(19).isNull())
            character.skillProficiencies = deserializeStringList(query_chars.getColumn(19).getString());
        if (!query_chars.getColumn(20).isNull())
            character.toolProficiencies = deserializeStringList(query_chars.getColumn(20).getString());
        if (!query_chars.getColumn(21).isNull())
            character.speedFeet = query_chars.getColumn(21).getInt();
        if (!query_chars.getColumn(22).isNull())
            character.currentHitPoints = query_chars.getColumn(22).getInt();
        if (!query_chars.getColumn(23).isNull())
            character.maxHitPoints = query_chars.getColumn(23).getInt();
        if (!query_chars.getColumn(24).isNull())
            character.temporaryHitPoints = query_chars.getColumn(24).getInt();
        if (!query_chars.getColumn(25).isNull())
            character.portraitAssetId = query_chars.getColumn(25).getString();
        if (!query_chars.getColumn(26).isNull())
            character.inventory.entries = deserializeInventoryEntries(query_chars.getColumn(26).getString());
        if (!query_chars.getColumn(27).isNull())
            character.conditions = deserializeStringList(query_chars.getColumn(27).getString());
        if (!query_chars.getColumn(28).isNull())
            character.spellcasting.enabled = query_chars.getColumn(28).getInt() != 0;
        if (!query_chars.getColumn(29).isNull())
            character.spellcasting.ability = query_chars.getColumn(29).getString();
        if (!query_chars.getColumn(30).isNull())
            character.spellcasting.spellAttackBonus = query_chars.getColumn(30).getInt();
        if (!query_chars.getColumn(31).isNull())
            character.spellcasting.spellSaveDc = query_chars.getColumn(31).getInt();
        if (!query_chars.getColumn(32).isNull())
            character.spellcasting.slotMaximums = deserializeSpellSlots(query_chars.getColumn(32).getString());
        if (!query_chars.getColumn(33).isNull())
            character.spellcasting.slotUsed = deserializeSpellSlots(query_chars.getColumn(33).getString());
        if (!query_chars.getColumn(34).isNull())
            character.spellcasting.spells = deserializeSpellbookEntries(query_chars.getColumn(34).getString());
        if (!query_chars.getColumn(35).isNull())
            character.featureActions = deserializeMonsterActions(query_chars.getColumn(35).getString());
        if (!query_chars.getColumn(36).isNull())
            character.resourceTrackers = deserializeResourceTrackers(query_chars.getColumn(36).getString());
        if (!query_chars.getColumn(37).isNull())
            character.ruleValues = deserializeAttributes(query_chars.getColumn(37).getString());
        if (!query_chars.getColumn(38).isNull())
            character.classLevels = deserializeAttributes(query_chars.getColumn(38).getString());
        character.spellcasting.spellAttackMode = static_cast<SpellValueMode>(query_chars.getColumn(39).getInt());
        character.spellcasting.spellSaveDcMode = static_cast<SpellValueMode>(query_chars.getColumn(40).getInt());

        if (const auto *definitionRules = dynamic_cast<const DefinitionBackedRuleSet *>(campaign.ruleSet.get()))
            migrateLegacyCharacterRuleValues(character, definitionRules->definition());

        character.inventory.ownerEntityId = character.metadata.id;
        character.inventory.name = std::string(character.nameBuffer) + " Inventory";
        character.inventory =
            loadInventoryForOwner(campaign.id, "character", character.metadata.id, character.inventory);
        normalizeCharacterHitPoints(character);
        normalizeSpellcasting(character.spellcasting);
        campaign.characters.push_back(character);
    }
}

void DatabaseManager::loadCampaignNpcs(Campaign &campaign)
{
    SQLite::Statement query_npcs(*m_db, R"(
                    SELECT stable_id, category, faction_id, deity_id, name, description, faction, role, attitude_toward_party, secrets, goals, resources, threat_location, relationships, threat_level, dialogue_notes, attributes, conditions, tags, created_at, updated_at, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries, resource_trackers, spell_attack_mode, spell_save_dc_mode
                    FROM npcs
                    WHERE campaign_id = ?;
                )");
    query_npcs.bind(1, static_cast<int64_t>(campaign.id));

    while (query_npcs.executeStep())
    {
        NPCModel npc;
        if (!query_npcs.getColumn(0).isNull())
            npc.metadata.id = query_npcs.getColumn(0).getString();
        if (!query_npcs.getColumn(1).isNull())
            npc.category = static_cast<NPCCategory>(query_npcs.getColumn(1).getInt());
        if (!query_npcs.getColumn(2).isNull())
            npc.factionId = query_npcs.getColumn(2).getString();
        if (!query_npcs.getColumn(3).isNull())
            npc.deityId = query_npcs.getColumn(3).getString();
        npc.name = query_npcs.getColumn(4).getString();
        if (!query_npcs.getColumn(5).isNull())
            npc.description = query_npcs.getColumn(5).getString();
        if (!query_npcs.getColumn(6).isNull())
            npc.faction = query_npcs.getColumn(6).getString();
        if (!query_npcs.getColumn(7).isNull())
            npc.role = query_npcs.getColumn(7).getString();
        if (!query_npcs.getColumn(8).isNull())
            npc.attitudeTowardParty = query_npcs.getColumn(8).getString();
        if (!query_npcs.getColumn(9).isNull())
            npc.secrets = query_npcs.getColumn(9).getString();
        if (!query_npcs.getColumn(10).isNull())
            npc.goals = query_npcs.getColumn(10).getString();
        if (!query_npcs.getColumn(11).isNull())
            npc.resources = query_npcs.getColumn(11).getString();
        if (!query_npcs.getColumn(12).isNull())
            npc.threatLocation = query_npcs.getColumn(12).getString();
        if (!query_npcs.getColumn(13).isNull())
            npc.relationships = query_npcs.getColumn(13).getString();
        if (!query_npcs.getColumn(14).isNull())
            npc.threatLevel = query_npcs.getColumn(14).getInt();
        if (!query_npcs.getColumn(15).isNull())
            npc.dialogueNotes = query_npcs.getColumn(15).getString();
        if (!query_npcs.getColumn(16).isNull())
            npc.attributes = deserializeAttributes(query_npcs.getColumn(16).getString());
        if (!query_npcs.getColumn(17).isNull())
            npc.conditions = deserializeStringList(query_npcs.getColumn(17).getString());
        if (!query_npcs.getColumn(18).isNull())
            npc.metadata.tags = deserializeStringList(query_npcs.getColumn(18).getString());
        if (!query_npcs.getColumn(19).isNull())
            npc.metadata.createdAtUtc = query_npcs.getColumn(19).getString();
        if (!query_npcs.getColumn(20).isNull())
            npc.metadata.updatedAtUtc = query_npcs.getColumn(20).getString();
        if (!query_npcs.getColumn(21).isNull())
            npc.inventory.entries = deserializeInventoryEntries(query_npcs.getColumn(21).getString());
        if (!query_npcs.getColumn(22).isNull())
            npc.spellcasting.enabled = query_npcs.getColumn(22).getInt() != 0;
        if (!query_npcs.getColumn(23).isNull())
            npc.spellcasting.ability = query_npcs.getColumn(23).getString();
        if (!query_npcs.getColumn(24).isNull())
            npc.spellcasting.spellAttackBonus = query_npcs.getColumn(24).getInt();
        if (!query_npcs.getColumn(25).isNull())
            npc.spellcasting.spellSaveDc = query_npcs.getColumn(25).getInt();
        if (!query_npcs.getColumn(26).isNull())
            npc.spellcasting.slotMaximums = deserializeSpellSlots(query_npcs.getColumn(26).getString());
        if (!query_npcs.getColumn(27).isNull())
            npc.spellcasting.slotUsed = deserializeSpellSlots(query_npcs.getColumn(27).getString());
        if (!query_npcs.getColumn(28).isNull())
            npc.spellcasting.spells = deserializeSpellbookEntries(query_npcs.getColumn(28).getString());
        if (!query_npcs.getColumn(29).isNull())
            npc.resourceTrackers = deserializeResourceTrackers(query_npcs.getColumn(29).getString());
        npc.spellcasting.spellAttackMode = static_cast<SpellValueMode>(query_npcs.getColumn(30).getInt());
        npc.spellcasting.spellSaveDcMode = static_cast<SpellValueMode>(query_npcs.getColumn(31).getInt());

        npc.inventory.ownerEntityId = npc.metadata.id;
        npc.inventory.name = npc.name + " Inventory";
        npc.inventory = loadInventoryForOwner(campaign.id, "npc", npc.metadata.id, npc.inventory);
        normalizeSpellcasting(npc.spellcasting);
        campaign.npcs.push_back(npc);
    }
}
