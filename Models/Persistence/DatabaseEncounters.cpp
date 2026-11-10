#include "Models/Persistence/CollectionReconcile.hpp"
#include "Models/DataBaseManager.hpp"
#include <cstdint>
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

void DatabaseManager::saveCampaignEncounters(int64_t campaignId, const Campaign &campaign)
{
    std::vector<std::string> encounters, participants;
    for (const auto &encounter : campaign.encounters) {
        encounters.push_back(encounter.metadata.id);
        for (const auto &participant : encounter.participants) participants.push_back(participant.metadata.id);
    }
    CollectionReconcile::retain(*m_db, "encounter_participants",
        "SELECT p.id, p.stable_id, e.campaign_id FROM encounter_participants p JOIN encounters e ON e.id=p.encounter_id;", campaignId, participants);
    CollectionReconcile::retain(*m_db, "encounters",
        "SELECT id, stable_id, campaign_id FROM encounters;", campaignId, encounters);

    SQLite::Statement query_insert_encounter(*m_db, R"(
            INSERT INTO encounters
            (stable_id, campaign_id, name, description, map_id, map_context_name, map_context_x, map_context_y, battlefield_width, battlefield_height, cell_size_feet, use_map_cells, theater_of_mind, combat_paused, combat_phase, battlefield_cells, combat_log, dice_roll_presets, after_combat_summary, after_combat_loot_notes, after_combat_story_consequences, after_combat_session_notes, round, active_participant_index, tags, created_at, updated_at, source_map_id, source_rect_x, source_rect_y, source_rect_width, source_rect_height, source_map_revision, tactical_map_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            campaign_id = excluded.campaign_id,
            name = excluded.name,
            description = excluded.description,
            map_id = excluded.map_id,
            map_context_name = excluded.map_context_name,
            map_context_x = excluded.map_context_x,
            map_context_y = excluded.map_context_y,
            battlefield_width = excluded.battlefield_width,
            battlefield_height = excluded.battlefield_height,
            cell_size_feet = excluded.cell_size_feet,
            use_map_cells = excluded.use_map_cells,
            theater_of_mind = excluded.theater_of_mind,
            combat_paused = excluded.combat_paused,
            combat_phase = excluded.combat_phase,
            battlefield_cells = excluded.battlefield_cells,
            combat_log = excluded.combat_log,
            dice_roll_presets = excluded.dice_roll_presets,
            after_combat_summary = excluded.after_combat_summary,
            after_combat_loot_notes = excluded.after_combat_loot_notes,
            after_combat_story_consequences = excluded.after_combat_story_consequences,
            after_combat_session_notes = excluded.after_combat_session_notes,
            round = excluded.round,
            active_participant_index = excluded.active_participant_index,
            tags = excluded.tags,
            updated_at = excluded.updated_at,
            source_map_id = excluded.source_map_id,
            source_rect_x = excluded.source_rect_x,
            source_rect_y = excluded.source_rect_y,
            source_rect_width = excluded.source_rect_width,
            source_rect_height = excluded.source_rect_height,
            source_map_revision = excluded.source_map_revision,
            tactical_map_id = excluded.tactical_map_id
        WHERE encounters.campaign_id IS NOT excluded.campaign_id
           OR encounters.name IS NOT excluded.name
           OR encounters.description IS NOT excluded.description
           OR encounters.map_id IS NOT excluded.map_id
           OR encounters.map_context_name IS NOT excluded.map_context_name
           OR encounters.map_context_x IS NOT excluded.map_context_x
           OR encounters.map_context_y IS NOT excluded.map_context_y
           OR encounters.battlefield_width IS NOT excluded.battlefield_width
           OR encounters.battlefield_height IS NOT excluded.battlefield_height
           OR encounters.cell_size_feet IS NOT excluded.cell_size_feet
           OR encounters.use_map_cells IS NOT excluded.use_map_cells
           OR encounters.theater_of_mind IS NOT excluded.theater_of_mind
           OR encounters.combat_paused IS NOT excluded.combat_paused
           OR encounters.combat_phase IS NOT excluded.combat_phase
           OR encounters.battlefield_cells IS NOT excluded.battlefield_cells
           OR encounters.combat_log IS NOT excluded.combat_log
           OR encounters.dice_roll_presets IS NOT excluded.dice_roll_presets
           OR encounters.after_combat_summary IS NOT excluded.after_combat_summary
           OR encounters.after_combat_loot_notes IS NOT excluded.after_combat_loot_notes
           OR encounters.after_combat_story_consequences IS NOT excluded.after_combat_story_consequences
           OR encounters.after_combat_session_notes IS NOT excluded.after_combat_session_notes
           OR encounters.round IS NOT excluded.round
           OR encounters.active_participant_index IS NOT excluded.active_participant_index
           OR encounters.tags IS NOT excluded.tags
           OR encounters.source_map_id IS NOT excluded.source_map_id
           OR encounters.source_rect_x IS NOT excluded.source_rect_x
           OR encounters.source_rect_y IS NOT excluded.source_rect_y
           OR encounters.source_rect_width IS NOT excluded.source_rect_width
           OR encounters.source_rect_height IS NOT excluded.source_rect_height
           OR encounters.source_map_revision IS NOT excluded.source_map_revision
           OR encounters.tactical_map_id IS NOT excluded.tactical_map_id;
        )");
    SQLite::Statement query_insert_participant(*m_db, R"(
            INSERT INTO encounter_participants
            (stable_id, encounter_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, temporary_hp, armor_class, armor_damage_reduction, speed_feet, group_name, wave_number, starts_hidden, reinforcement_round, starting_x, starting_y, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, narrative_position, narrative_range_band, narrative_spotlight, damage_resistances, damage_vulnerabilities, damage_immunities, conditions, active_effects, actions, tags, created_at, updated_at, sort_order)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            sort_order = excluded.sort_order,
            encounter_id = excluded.encounter_id,
            kind = excluded.kind,
            source_entity_id = excluded.source_entity_id,
            monster_definition_id = excluded.monster_definition_id,
            display_name = excluded.display_name,
            initiative = excluded.initiative,
            current_hp = excluded.current_hp,
            max_hp = excluded.max_hp,
            temporary_hp = excluded.temporary_hp,
            armor_class = excluded.armor_class,
            armor_damage_reduction = excluded.armor_damage_reduction,
            speed_feet = excluded.speed_feet,
            group_name = excluded.group_name,
            wave_number = excluded.wave_number,
            starts_hidden = excluded.starts_hidden,
            reinforcement_round = excluded.reinforcement_round,
            starting_x = excluded.starting_x,
            starting_y = excluded.starting_y,
            grid_x = excluded.grid_x,
            grid_y = excluded.grid_y,
            turn_start_x = excluded.turn_start_x,
            turn_start_y = excluded.turn_start_y,
            movement_used_feet = excluded.movement_used_feet,
            defeated = excluded.defeated,
            friendly = excluded.friendly,
            narrative_position = excluded.narrative_position,
            narrative_range_band = excluded.narrative_range_band,
            narrative_spotlight = excluded.narrative_spotlight,
            damage_resistances = excluded.damage_resistances,
            damage_vulnerabilities = excluded.damage_vulnerabilities,
            damage_immunities = excluded.damage_immunities,
            conditions = excluded.conditions,
            active_effects = excluded.active_effects,
            actions = excluded.actions,
            tags = excluded.tags,
            updated_at = excluded.updated_at
        WHERE encounter_participants.sort_order IS NOT excluded.sort_order
           OR encounter_participants.encounter_id IS NOT excluded.encounter_id
           OR encounter_participants.kind IS NOT excluded.kind
           OR encounter_participants.source_entity_id IS NOT excluded.source_entity_id
           OR encounter_participants.monster_definition_id IS NOT excluded.monster_definition_id
           OR encounter_participants.display_name IS NOT excluded.display_name
           OR encounter_participants.initiative IS NOT excluded.initiative
           OR encounter_participants.current_hp IS NOT excluded.current_hp
           OR encounter_participants.max_hp IS NOT excluded.max_hp
           OR encounter_participants.temporary_hp IS NOT excluded.temporary_hp
           OR encounter_participants.armor_class IS NOT excluded.armor_class
           OR encounter_participants.armor_damage_reduction IS NOT excluded.armor_damage_reduction
           OR encounter_participants.speed_feet IS NOT excluded.speed_feet
           OR encounter_participants.group_name IS NOT excluded.group_name
           OR encounter_participants.wave_number IS NOT excluded.wave_number
           OR encounter_participants.starts_hidden IS NOT excluded.starts_hidden
           OR encounter_participants.reinforcement_round IS NOT excluded.reinforcement_round
           OR encounter_participants.starting_x IS NOT excluded.starting_x
           OR encounter_participants.starting_y IS NOT excluded.starting_y
           OR encounter_participants.grid_x IS NOT excluded.grid_x
           OR encounter_participants.grid_y IS NOT excluded.grid_y
           OR encounter_participants.turn_start_x IS NOT excluded.turn_start_x
           OR encounter_participants.turn_start_y IS NOT excluded.turn_start_y
           OR encounter_participants.movement_used_feet IS NOT excluded.movement_used_feet
           OR encounter_participants.defeated IS NOT excluded.defeated
           OR encounter_participants.friendly IS NOT excluded.friendly
           OR encounter_participants.narrative_position IS NOT excluded.narrative_position
           OR encounter_participants.narrative_range_band IS NOT excluded.narrative_range_band
           OR encounter_participants.narrative_spotlight IS NOT excluded.narrative_spotlight
           OR encounter_participants.damage_resistances IS NOT excluded.damage_resistances
           OR encounter_participants.damage_vulnerabilities IS NOT excluded.damage_vulnerabilities
           OR encounter_participants.damage_immunities IS NOT excluded.damage_immunities
           OR encounter_participants.conditions IS NOT excluded.conditions
           OR encounter_participants.active_effects IS NOT excluded.active_effects
           OR encounter_participants.actions IS NOT excluded.actions
           OR encounter_participants.tags IS NOT excluded.tags;
        )");
    for (const auto &encounter : campaign.encounters)
    {
        EncounterModel normalizedEncounter = encounter;
        normalizeEncounter(normalizedEncounter);
        query_insert_encounter.bind(1, normalizedEncounter.metadata.id);
        query_insert_encounter.bind(2, campaignId);
        query_insert_encounter.bind(3, normalizedEncounter.name);
        query_insert_encounter.bind(4, normalizedEncounter.description);
        query_insert_encounter.bind(5, normalizedEncounter.mapId);
        query_insert_encounter.bind(6, normalizedEncounter.mapContextName);
        query_insert_encounter.bind(7, normalizedEncounter.mapContextX);
        query_insert_encounter.bind(8, normalizedEncounter.mapContextY);
        query_insert_encounter.bind(9, normalizedEncounter.battlefieldWidth);
        query_insert_encounter.bind(10, normalizedEncounter.battlefieldHeight);
        query_insert_encounter.bind(11, normalizedEncounter.cellSizeFeet);
        query_insert_encounter.bind(12, normalizedEncounter.useMapCells ? 1 : 0);
        query_insert_encounter.bind(13, normalizedEncounter.theaterOfMind ? 1 : 0);
        query_insert_encounter.bind(14, normalizedEncounter.combatPaused ? 1 : 0);
        query_insert_encounter.bind(15, combatPhaseStorageValue(normalizedEncounter.combatPhase));
        query_insert_encounter.bind(16, serializeMapCells(normalizedEncounter.battlefieldCells));
        query_insert_encounter.bind(17, serializeCombatLog(normalizedEncounter.combatLog));
        query_insert_encounter.bind(18, serializeDiceRollPresets(normalizedEncounter.diceRollPresets));
        query_insert_encounter.bind(19, normalizedEncounter.afterCombatSummary);
        query_insert_encounter.bind(20, normalizedEncounter.afterCombatLootNotes);
        query_insert_encounter.bind(21, normalizedEncounter.afterCombatStoryConsequences);
        query_insert_encounter.bind(22, normalizedEncounter.afterCombatSessionNotes);
        query_insert_encounter.bind(23, normalizedEncounter.round);
        query_insert_encounter.bind(24, normalizedEncounter.activeParticipantIndex);
        query_insert_encounter.bind(25, serializeStringList(normalizedEncounter.metadata.tags));
        query_insert_encounter.bind(26, normalizedEncounter.metadata.createdAtUtc);
        query_insert_encounter.bind(27, currentUtcTimestamp());
        query_insert_encounter.bind(28, normalizedEncounter.sourceMapId);
        query_insert_encounter.bind(29, normalizedEncounter.sourceMapRect.x);
        query_insert_encounter.bind(30, normalizedEncounter.sourceMapRect.y);
        query_insert_encounter.bind(31, normalizedEncounter.sourceMapRect.width);
        query_insert_encounter.bind(32, normalizedEncounter.sourceMapRect.height);
        query_insert_encounter.bind(33, static_cast<int64_t>(normalizedEncounter.sourceMapRevision));
        query_insert_encounter.bind(34, normalizedEncounter.tacticalMapId ? *normalizedEncounter.tacticalMapId
                                                                          : EntityId{});
        query_insert_encounter.exec();
        const int64_t encounterId = CollectionReconcile::rowId(*m_db, "encounters", normalizedEncounter.metadata.id);
        query_insert_encounter.reset();

        int participantOrder = 0;
        for (const auto &participant : normalizedEncounter.participants)
        {
            query_insert_participant.bind(1, participant.metadata.id);
            query_insert_participant.bind(2, encounterId);
            query_insert_participant.bind(3, static_cast<int>(participant.kind));
            query_insert_participant.bind(4, participant.sourceEntityId);
            query_insert_participant.bind(5, participant.monsterDefinitionId);
            query_insert_participant.bind(6, participant.displayName);
            query_insert_participant.bind(7, participant.initiative);
            query_insert_participant.bind(8, participant.currentHitPoints);
            query_insert_participant.bind(9, participant.maxHitPoints);
            query_insert_participant.bind(10, participant.temporaryHitPoints);
            query_insert_participant.bind(11, participant.armorClass);
            query_insert_participant.bind(12, participant.armorDamageReduction);
            query_insert_participant.bind(13, participant.speedFeet);
            query_insert_participant.bind(14, participant.groupName);
            query_insert_participant.bind(15, participant.waveNumber);
            query_insert_participant.bind(16, participant.startsHidden ? 1 : 0);
            query_insert_participant.bind(17, participant.reinforcementRound);
            query_insert_participant.bind(18, participant.startingX);
            query_insert_participant.bind(19, participant.startingY);
            query_insert_participant.bind(20, participant.x);
            query_insert_participant.bind(21, participant.y);
            query_insert_participant.bind(22, participant.turnStartX);
            query_insert_participant.bind(23, participant.turnStartY);
            query_insert_participant.bind(24, participant.movementUsedFeet);
            query_insert_participant.bind(25, participant.defeated ? 1 : 0);
            query_insert_participant.bind(26, participant.friendly ? 1 : 0);
            query_insert_participant.bind(27, participant.narrativePosition);
            query_insert_participant.bind(28, participant.narrativeRangeBand);
            query_insert_participant.bind(29, participant.narrativeSpotlight ? 1 : 0);
            query_insert_participant.bind(30, serializeStringList(participant.damageResistances));
            query_insert_participant.bind(31, serializeStringList(participant.damageVulnerabilities));
            query_insert_participant.bind(32, serializeStringList(participant.damageImmunities));
            query_insert_participant.bind(33, serializeStringList(participant.conditions));
            query_insert_participant.bind(34, serializeActiveEffects(participant.activeEffects));
            query_insert_participant.bind(35, serializeMonsterActions(participant.actions));
            query_insert_participant.bind(36, serializeStringList(participant.metadata.tags));
            query_insert_participant.bind(37, participant.metadata.createdAtUtc);
            query_insert_participant.bind(38, currentUtcTimestamp());
            query_insert_participant.bind(39, participantOrder++);
            query_insert_participant.exec();
            query_insert_participant.reset();
        }
    }
}
