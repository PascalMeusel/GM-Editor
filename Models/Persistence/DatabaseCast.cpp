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

void DatabaseManager::saveCampaignCharacters(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("characters", campaignId, characterIds(campaign.characters));

    // Insert each character into the characters table
    SQLite::Statement query_insert_char(*m_db, R"(
            INSERT INTO characters
            (stable_id, campaign_id, name, level, class, race, background, attributes, tags, created_at, updated_at, backstory, player_notes, gm_notes, character_relationships, feats, traits_features, languages, senses, saving_throw_proficiencies, skill_proficiencies, tool_proficiencies, speed_feet, current_hp, max_hp, temporary_hp, portrait_asset_id, inventory_entries, conditions, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries, feature_actions, resource_trackers, rule_values, class_levels, spell_attack_mode, spell_save_dc_mode)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                level = excluded.level,
                class = excluded.class,
                race = excluded.race,
                background = excluded.background,
                attributes = excluded.attributes,
                tags = excluded.tags,
                updated_at = excluded.updated_at,
                backstory = excluded.backstory,
                player_notes = excluded.player_notes,
                gm_notes = excluded.gm_notes,
                character_relationships = excluded.character_relationships,
                feats = excluded.feats,
                traits_features = excluded.traits_features,
                languages = excluded.languages,
                senses = excluded.senses,
                saving_throw_proficiencies = excluded.saving_throw_proficiencies,
                skill_proficiencies = excluded.skill_proficiencies,
                tool_proficiencies = excluded.tool_proficiencies,
                speed_feet = excluded.speed_feet,
                current_hp = excluded.current_hp,
                max_hp = excluded.max_hp,
                temporary_hp = excluded.temporary_hp,
                portrait_asset_id = excluded.portrait_asset_id,
                inventory_entries = excluded.inventory_entries,
                conditions = excluded.conditions,
                spellcasting_enabled = excluded.spellcasting_enabled,
                spellcasting_ability = excluded.spellcasting_ability,
                spell_attack_bonus = excluded.spell_attack_bonus,
                spell_save_dc = excluded.spell_save_dc,
                spell_attack_mode = excluded.spell_attack_mode,
                spell_save_dc_mode = excluded.spell_save_dc_mode,
                spell_slots_max = excluded.spell_slots_max,
                spell_slots_used = excluded.spell_slots_used,
                spellbook_entries = excluded.spellbook_entries,
                feature_actions = excluded.feature_actions,
                resource_trackers = excluded.resource_trackers,
                rule_values = excluded.rule_values,
                class_levels = excluded.class_levels
            WHERE characters.campaign_id IS NOT excluded.campaign_id
               OR characters.name IS NOT excluded.name
               OR characters.level IS NOT excluded.level
               OR characters.class IS NOT excluded.class
               OR characters.race IS NOT excluded.race
               OR characters.background IS NOT excluded.background
               OR characters.attributes IS NOT excluded.attributes
               OR characters.tags IS NOT excluded.tags
               OR characters.backstory IS NOT excluded.backstory
               OR characters.player_notes IS NOT excluded.player_notes
               OR characters.gm_notes IS NOT excluded.gm_notes
               OR characters.character_relationships IS NOT excluded.character_relationships
               OR characters.feats IS NOT excluded.feats
               OR characters.traits_features IS NOT excluded.traits_features
               OR characters.languages IS NOT excluded.languages
               OR characters.senses IS NOT excluded.senses
               OR characters.saving_throw_proficiencies IS NOT excluded.saving_throw_proficiencies
               OR characters.skill_proficiencies IS NOT excluded.skill_proficiencies
               OR characters.tool_proficiencies IS NOT excluded.tool_proficiencies
               OR characters.speed_feet IS NOT excluded.speed_feet
               OR characters.current_hp IS NOT excluded.current_hp
               OR characters.max_hp IS NOT excluded.max_hp
               OR characters.temporary_hp IS NOT excluded.temporary_hp
               OR characters.portrait_asset_id IS NOT excluded.portrait_asset_id
               OR characters.inventory_entries IS NOT excluded.inventory_entries
               OR characters.conditions IS NOT excluded.conditions
               OR characters.spellcasting_enabled IS NOT excluded.spellcasting_enabled
               OR characters.spellcasting_ability IS NOT excluded.spellcasting_ability
               OR characters.spell_attack_bonus IS NOT excluded.spell_attack_bonus
               OR characters.spell_save_dc IS NOT excluded.spell_save_dc
               OR characters.spell_attack_mode IS NOT excluded.spell_attack_mode
               OR characters.spell_save_dc_mode IS NOT excluded.spell_save_dc_mode
               OR characters.spell_slots_max IS NOT excluded.spell_slots_max
               OR characters.spell_slots_used IS NOT excluded.spell_slots_used
               OR characters.spellbook_entries IS NOT excluded.spellbook_entries
               OR characters.feature_actions IS NOT excluded.feature_actions
               OR characters.resource_trackers IS NOT excluded.resource_trackers
               OR characters.rule_values IS NOT excluded.rule_values
               OR characters.class_levels IS NOT excluded.class_levels;
        )");
    for (const auto &character : campaign.characters)
    {
        CharacterModel normalizedCharacter = character;
        normalizeCharacterHitPoints(normalizedCharacter);
        normalizeSpellcasting(normalizedCharacter.spellcasting);
        if (const auto *definitionRules = dynamic_cast<const DefinitionBackedRuleSet *>(campaign.ruleSet.get()))
            migrateLegacyCharacterRuleValues(normalizedCharacter, definitionRules->definition());
        query_insert_char.bind(1, character.metadata.id);
        query_insert_char.bind(2, campaignId);
        query_insert_char.bind(3, std::string(character.nameBuffer));
        query_insert_char.bind(4, character.level);
        query_insert_char.bind(5, character.selectedClass);
        query_insert_char.bind(6, character.selectedRace);
        query_insert_char.bind(7, character.background);
        query_insert_char.bind(8, serializeAttributes(character.attributes));
        query_insert_char.bind(9, serializeStringList(character.metadata.tags));
        query_insert_char.bind(10, character.metadata.createdAtUtc);
        query_insert_char.bind(11, currentUtcTimestamp());
        query_insert_char.bind(12, character.backstory);
        query_insert_char.bind(13, character.playerNotes);
        query_insert_char.bind(14, character.gmNotes);
        query_insert_char.bind(15, serializeCharacterRelationships(character.relationships));
        query_insert_char.bind(16, serializeStringList(character.feats));
        query_insert_char.bind(17, serializeStringList(character.traitsAndFeatures));
        query_insert_char.bind(18, serializeStringList(character.languages));
        query_insert_char.bind(19, serializeStringList(character.senses));
        query_insert_char.bind(20, serializeStringList(character.savingThrowProficiencies));
        query_insert_char.bind(21, serializeStringList(character.skillProficiencies));
        query_insert_char.bind(22, serializeStringList(character.toolProficiencies));
        query_insert_char.bind(23, std::max(0, character.speedFeet));
        query_insert_char.bind(24, normalizedCharacter.currentHitPoints);
        query_insert_char.bind(25, normalizedCharacter.maxHitPoints);
        query_insert_char.bind(26, normalizedCharacter.temporaryHitPoints);
        query_insert_char.bind(27, character.portraitAssetId);
        query_insert_char.bind(28, serializeInventoryEntries(character.inventory.entries));
        query_insert_char.bind(29, serializeStringList(character.conditions));
        query_insert_char.bind(30, normalizedCharacter.spellcasting.enabled ? 1 : 0);
        query_insert_char.bind(31, normalizedCharacter.spellcasting.ability);
        query_insert_char.bind(32, normalizedCharacter.spellcasting.spellAttackBonus);
        query_insert_char.bind(33, normalizedCharacter.spellcasting.spellSaveDc);
        query_insert_char.bind(34, serializeSpellSlots(normalizedCharacter.spellcasting.slotMaximums));
        query_insert_char.bind(35, serializeSpellSlots(normalizedCharacter.spellcasting.slotUsed));
        query_insert_char.bind(36, serializeSpellbookEntries(normalizedCharacter.spellcasting.spells));
        query_insert_char.bind(37, serializeMonsterActions(character.featureActions));
        query_insert_char.bind(38, serializeResourceTrackers(character.resourceTrackers));
        query_insert_char.bind(39, serializeAttributes(normalizedCharacter.ruleValues));
        query_insert_char.bind(40, serializeAttributes(character.classLevels));
        query_insert_char.bind(41, static_cast<int>(normalizedCharacter.spellcasting.spellAttackMode));
        query_insert_char.bind(42, static_cast<int>(normalizedCharacter.spellcasting.spellSaveDcMode));

        query_insert_char.exec();
        query_insert_char.reset(); // Reset for the next iteration
        saveInventory(campaignId, "character", character.metadata.id, normalizedCharacter.inventory);
    }
}

void DatabaseManager::saveCampaignNpcs(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("npcs", campaignId, npcIds(campaign.npcs));

    SQLite::Statement query_insert_npc(*m_db, R"(
            INSERT INTO npcs
            (stable_id, campaign_id, category, faction_id, deity_id, name, description, faction, role, attitude_toward_party, secrets, goals, resources, threat_location, relationships, threat_level, dialogue_notes, attributes, conditions, tags, created_at, updated_at, inventory_entries, spellcasting_enabled, spellcasting_ability, spell_attack_bonus, spell_save_dc, spell_slots_max, spell_slots_used, spellbook_entries, resource_trackers, spell_attack_mode, spell_save_dc_mode)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                category = excluded.category,
                faction_id = excluded.faction_id,
                deity_id = excluded.deity_id,
                name = excluded.name,
                description = excluded.description,
                faction = excluded.faction,
                role = excluded.role,
                attitude_toward_party = excluded.attitude_toward_party,
                secrets = excluded.secrets,
                goals = excluded.goals,
                resources = excluded.resources,
                threat_location = excluded.threat_location,
                relationships = excluded.relationships,
                threat_level = excluded.threat_level,
                dialogue_notes = excluded.dialogue_notes,
                attributes = excluded.attributes,
                conditions = excluded.conditions,
                tags = excluded.tags,
                updated_at = excluded.updated_at,
                inventory_entries = excluded.inventory_entries,
                spellcasting_enabled = excluded.spellcasting_enabled,
                spellcasting_ability = excluded.spellcasting_ability,
                spell_attack_bonus = excluded.spell_attack_bonus,
                spell_save_dc = excluded.spell_save_dc,
                spell_attack_mode = excluded.spell_attack_mode,
                spell_save_dc_mode = excluded.spell_save_dc_mode,
                spell_slots_max = excluded.spell_slots_max,
                spell_slots_used = excluded.spell_slots_used,
                spellbook_entries = excluded.spellbook_entries,
                resource_trackers = excluded.resource_trackers
            WHERE npcs.campaign_id IS NOT excluded.campaign_id
               OR npcs.category IS NOT excluded.category
               OR npcs.faction_id IS NOT excluded.faction_id
               OR npcs.deity_id IS NOT excluded.deity_id
               OR npcs.name IS NOT excluded.name
               OR npcs.description IS NOT excluded.description
               OR npcs.faction IS NOT excluded.faction
               OR npcs.role IS NOT excluded.role
               OR npcs.attitude_toward_party IS NOT excluded.attitude_toward_party
               OR npcs.secrets IS NOT excluded.secrets
               OR npcs.goals IS NOT excluded.goals
               OR npcs.resources IS NOT excluded.resources
               OR npcs.threat_location IS NOT excluded.threat_location
               OR npcs.relationships IS NOT excluded.relationships
               OR npcs.threat_level IS NOT excluded.threat_level
               OR npcs.dialogue_notes IS NOT excluded.dialogue_notes
               OR npcs.attributes IS NOT excluded.attributes
               OR npcs.conditions IS NOT excluded.conditions
               OR npcs.tags IS NOT excluded.tags
               OR npcs.inventory_entries IS NOT excluded.inventory_entries
               OR npcs.spellcasting_enabled IS NOT excluded.spellcasting_enabled
               OR npcs.spellcasting_ability IS NOT excluded.spellcasting_ability
               OR npcs.spell_attack_bonus IS NOT excluded.spell_attack_bonus
               OR npcs.spell_save_dc IS NOT excluded.spell_save_dc
               OR npcs.spell_attack_mode IS NOT excluded.spell_attack_mode
               OR npcs.spell_save_dc_mode IS NOT excluded.spell_save_dc_mode
               OR npcs.spell_slots_max IS NOT excluded.spell_slots_max
               OR npcs.spell_slots_used IS NOT excluded.spell_slots_used
               OR npcs.spellbook_entries IS NOT excluded.spellbook_entries
               OR npcs.resource_trackers IS NOT excluded.resource_trackers;
        )");
    for (const auto &npc : campaign.npcs)
    {
        NPCModel normalizedNpc = npc;
        normalizeSpellcasting(normalizedNpc.spellcasting);
        query_insert_npc.bind(1, npc.metadata.id);
        query_insert_npc.bind(2, campaignId);
        query_insert_npc.bind(3, static_cast<int>(npc.category));
        query_insert_npc.bind(4, npc.factionId);
        query_insert_npc.bind(5, npc.deityId);
        query_insert_npc.bind(6, npc.name);
        query_insert_npc.bind(7, npc.description);
        query_insert_npc.bind(8, npc.faction);
        query_insert_npc.bind(9, npc.role);
        query_insert_npc.bind(10, npc.attitudeTowardParty);
        query_insert_npc.bind(11, npc.secrets);
        query_insert_npc.bind(12, npc.goals);
        query_insert_npc.bind(13, npc.resources);
        query_insert_npc.bind(14, npc.threatLocation);
        query_insert_npc.bind(15, npc.relationships);
        query_insert_npc.bind(16, npc.threatLevel);
        query_insert_npc.bind(17, npc.dialogueNotes);
        query_insert_npc.bind(18, serializeAttributes(npc.attributes));
        query_insert_npc.bind(19, serializeStringList(npc.conditions));
        query_insert_npc.bind(20, serializeStringList(npc.metadata.tags));
        query_insert_npc.bind(21, npc.metadata.createdAtUtc);
        query_insert_npc.bind(22, currentUtcTimestamp());
        query_insert_npc.bind(23, serializeInventoryEntries(npc.inventory.entries));
        query_insert_npc.bind(24, normalizedNpc.spellcasting.enabled ? 1 : 0);
        query_insert_npc.bind(25, normalizedNpc.spellcasting.ability);
        query_insert_npc.bind(26, normalizedNpc.spellcasting.spellAttackBonus);
        query_insert_npc.bind(27, normalizedNpc.spellcasting.spellSaveDc);
        query_insert_npc.bind(28, serializeSpellSlots(normalizedNpc.spellcasting.slotMaximums));
        query_insert_npc.bind(29, serializeSpellSlots(normalizedNpc.spellcasting.slotUsed));
        query_insert_npc.bind(30, serializeSpellbookEntries(normalizedNpc.spellcasting.spells));
        query_insert_npc.bind(31, serializeResourceTrackers(npc.resourceTrackers));
        query_insert_npc.bind(32, static_cast<int>(normalizedNpc.spellcasting.spellAttackMode));
        query_insert_npc.bind(33, static_cast<int>(normalizedNpc.spellcasting.spellSaveDcMode));

        query_insert_npc.exec();
        query_insert_npc.reset();
        saveInventory(campaignId, "npc", npc.metadata.id, normalizedNpc.inventory);
    }
}
