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

void DatabaseManager::loadCampaignEncounters(Campaign &campaign)
{
    SQLite::Statement query_encounters(*m_db, R"(
                    SELECT id, stable_id, name, description, map_id, map_context_name, map_context_x, map_context_y, battlefield_width, battlefield_height, cell_size_feet, use_map_cells, theater_of_mind, combat_paused, combat_phase, battlefield_cells, combat_log, dice_roll_presets, after_combat_summary, after_combat_loot_notes, after_combat_story_consequences, after_combat_session_notes, round, active_participant_index, tags, created_at, updated_at, source_map_id, source_rect_x, source_rect_y, source_rect_width, source_rect_height, source_map_revision, tactical_map_id
                    FROM encounters
                    WHERE campaign_id = ?
                    ORDER BY updated_at DESC, name ASC;
                )");
    query_encounters.bind(1, static_cast<int64_t>(campaign.id));

    while (query_encounters.executeStep())
    {
        EncounterModel encounter;
        const int64_t encounterRowId = query_encounters.getColumn(0).getInt64();
        bool loadedCombatPhase = false;
        if (!query_encounters.getColumn(1).isNull())
            encounter.metadata.id = query_encounters.getColumn(1).getString();
        encounter.name = query_encounters.getColumn(2).getString();
        if (!query_encounters.getColumn(3).isNull())
            encounter.description = query_encounters.getColumn(3).getString();
        if (!query_encounters.getColumn(4).isNull())
            encounter.mapId = query_encounters.getColumn(4).getString();
        if (!query_encounters.getColumn(5).isNull())
            encounter.mapContextName = query_encounters.getColumn(5).getString();
        if (!query_encounters.getColumn(6).isNull())
            encounter.mapContextX = query_encounters.getColumn(6).getInt();
        if (!query_encounters.getColumn(7).isNull())
            encounter.mapContextY = query_encounters.getColumn(7).getInt();
        if (!query_encounters.getColumn(8).isNull())
            encounter.battlefieldWidth = query_encounters.getColumn(8).getInt();
        if (!query_encounters.getColumn(9).isNull())
            encounter.battlefieldHeight = query_encounters.getColumn(9).getInt();
        if (!query_encounters.getColumn(10).isNull())
            encounter.cellSizeFeet = query_encounters.getColumn(10).getInt();
        if (!query_encounters.getColumn(11).isNull())
            encounter.useMapCells = query_encounters.getColumn(11).getInt() != 0;
        if (!query_encounters.getColumn(12).isNull())
            encounter.theaterOfMind = query_encounters.getColumn(12).getInt() != 0;
        if (!query_encounters.getColumn(13).isNull())
            encounter.combatPaused = query_encounters.getColumn(13).getInt() != 0;
        if (!query_encounters.getColumn(14).isNull() && !query_encounters.getColumn(14).getString().empty())
        {
            encounter.combatPhase = combatPhaseFromStorageValue(query_encounters.getColumn(14).getString());
            loadedCombatPhase = true;
        }
        if (!query_encounters.getColumn(15).isNull())
            encounter.battlefieldCells = deserializeMapCells(query_encounters.getColumn(15).getString());
        if (!query_encounters.getColumn(16).isNull())
            encounter.combatLog = deserializeCombatLog(query_encounters.getColumn(16).getString());
        if (!query_encounters.getColumn(17).isNull())
            encounter.diceRollPresets = deserializeDiceRollPresets(query_encounters.getColumn(17).getString());
        if (!query_encounters.getColumn(18).isNull())
            encounter.afterCombatSummary = query_encounters.getColumn(18).getString();
        if (!query_encounters.getColumn(19).isNull())
            encounter.afterCombatLootNotes = query_encounters.getColumn(19).getString();
        if (!query_encounters.getColumn(20).isNull())
            encounter.afterCombatStoryConsequences = query_encounters.getColumn(20).getString();
        if (!query_encounters.getColumn(21).isNull())
            encounter.afterCombatSessionNotes = query_encounters.getColumn(21).getString();
        if (!query_encounters.getColumn(22).isNull())
            encounter.round = query_encounters.getColumn(22).getInt();
        if (!query_encounters.getColumn(23).isNull())
            encounter.activeParticipantIndex = query_encounters.getColumn(23).getInt();
        if (!query_encounters.getColumn(24).isNull())
            encounter.metadata.tags = deserializeStringList(query_encounters.getColumn(24).getString());
        if (!query_encounters.getColumn(25).isNull())
            encounter.metadata.createdAtUtc = query_encounters.getColumn(25).getString();
        if (!query_encounters.getColumn(26).isNull())
            encounter.metadata.updatedAtUtc = query_encounters.getColumn(26).getString();
        if (!query_encounters.getColumn(27).isNull())
            encounter.sourceMapId = query_encounters.getColumn(27).getString();
        if (!query_encounters.getColumn(28).isNull())
            encounter.sourceMapRect.x = static_cast<float>(query_encounters.getColumn(28).getDouble());
        if (!query_encounters.getColumn(29).isNull())
            encounter.sourceMapRect.y = static_cast<float>(query_encounters.getColumn(29).getDouble());
        if (!query_encounters.getColumn(30).isNull())
            encounter.sourceMapRect.width = static_cast<float>(query_encounters.getColumn(30).getDouble());
        if (!query_encounters.getColumn(31).isNull())
            encounter.sourceMapRect.height = static_cast<float>(query_encounters.getColumn(31).getDouble());
        if (!query_encounters.getColumn(32).isNull())
            encounter.sourceMapRevision = static_cast<std::uint64_t>(query_encounters.getColumn(32).getInt64());
        if (!query_encounters.getColumn(33).isNull() && !query_encounters.getColumn(33).getString().empty())
            encounter.tacticalMapId = query_encounters.getColumn(33).getString();

        SQLite::Statement query_participants(*m_db, R"(
                        SELECT stable_id, kind, source_entity_id, monster_definition_id, display_name, initiative, current_hp, max_hp, temporary_hp, armor_class, armor_damage_reduction, speed_feet, group_name, wave_number, starts_hidden, reinforcement_round, starting_x, starting_y, grid_x, grid_y, turn_start_x, turn_start_y, movement_used_feet, defeated, friendly, narrative_position, narrative_range_band, narrative_spotlight, damage_resistances, damage_vulnerabilities, damage_immunities, conditions, active_effects, actions, tags, created_at, updated_at
                        FROM encounter_participants
                        WHERE encounter_id = ?
                        ORDER BY sort_order ASC, id ASC;
                    )");
        query_participants.bind(1, encounterRowId);

        while (query_participants.executeStep())
        {
            EncounterParticipantModel participant;
            if (!query_participants.getColumn(0).isNull())
                participant.metadata.id = query_participants.getColumn(0).getString();
            if (!query_participants.getColumn(1).isNull())
                participant.kind = static_cast<CombatantKind>(query_participants.getColumn(1).getInt());
            if (!query_participants.getColumn(2).isNull())
                participant.sourceEntityId = query_participants.getColumn(2).getString();
            if (!query_participants.getColumn(3).isNull())
                participant.monsterDefinitionId = query_participants.getColumn(3).getString();
            if (!query_participants.getColumn(4).isNull())
                participant.displayName = query_participants.getColumn(4).getString();
            if (!query_participants.getColumn(5).isNull())
                participant.initiative = query_participants.getColumn(5).getInt();
            if (!query_participants.getColumn(6).isNull())
                participant.currentHitPoints = query_participants.getColumn(6).getInt();
            if (!query_participants.getColumn(7).isNull())
                participant.maxHitPoints = query_participants.getColumn(7).getInt();
            if (!query_participants.getColumn(8).isNull())
                participant.temporaryHitPoints = query_participants.getColumn(8).getInt();
            if (!query_participants.getColumn(9).isNull())
                participant.armorClass = query_participants.getColumn(9).getInt();
            if (!query_participants.getColumn(10).isNull())
                participant.armorDamageReduction = query_participants.getColumn(10).getInt();
            if (!query_participants.getColumn(11).isNull())
                participant.speedFeet = query_participants.getColumn(11).getInt();
            if (!query_participants.getColumn(12).isNull())
                participant.groupName = query_participants.getColumn(12).getString();
            if (!query_participants.getColumn(13).isNull())
                participant.waveNumber = query_participants.getColumn(13).getInt();
            if (!query_participants.getColumn(14).isNull())
                participant.startsHidden = query_participants.getColumn(14).getInt() != 0;
            if (!query_participants.getColumn(15).isNull())
                participant.reinforcementRound = query_participants.getColumn(15).getInt();
            if (!query_participants.getColumn(16).isNull())
                participant.startingX = query_participants.getColumn(16).getInt();
            if (!query_participants.getColumn(17).isNull())
                participant.startingY = query_participants.getColumn(17).getInt();
            if (!query_participants.getColumn(18).isNull())
                participant.x = query_participants.getColumn(18).getInt();
            if (!query_participants.getColumn(19).isNull())
                participant.y = query_participants.getColumn(19).getInt();
            if (!query_participants.getColumn(20).isNull())
                participant.turnStartX = query_participants.getColumn(20).getInt();
            if (!query_participants.getColumn(21).isNull())
                participant.turnStartY = query_participants.getColumn(21).getInt();
            if (!query_participants.getColumn(22).isNull())
                participant.movementUsedFeet = query_participants.getColumn(22).getInt();
            if (!query_participants.getColumn(23).isNull())
                participant.defeated = query_participants.getColumn(23).getInt() != 0;
            if (!query_participants.getColumn(24).isNull())
                participant.friendly = query_participants.getColumn(24).getInt() != 0;
            if (!query_participants.getColumn(25).isNull())
                participant.narrativePosition = query_participants.getColumn(25).getString();
            if (!query_participants.getColumn(26).isNull())
                participant.narrativeRangeBand = query_participants.getColumn(26).getString();
            if (!query_participants.getColumn(27).isNull())
                participant.narrativeSpotlight = query_participants.getColumn(27).getInt() != 0;
            if (!query_participants.getColumn(28).isNull())
                participant.damageResistances = deserializeStringList(query_participants.getColumn(28).getString());
            if (!query_participants.getColumn(29).isNull())
                participant.damageVulnerabilities = deserializeStringList(query_participants.getColumn(29).getString());
            if (!query_participants.getColumn(30).isNull())
                participant.damageImmunities = deserializeStringList(query_participants.getColumn(30).getString());
            if (!query_participants.getColumn(31).isNull())
                participant.conditions = deserializeStringList(query_participants.getColumn(31).getString());
            if (!query_participants.getColumn(32).isNull())
                participant.activeEffects = deserializeActiveEffects(query_participants.getColumn(32).getString());
            if (!query_participants.getColumn(33).isNull())
                participant.actions = deserializeMonsterActions(query_participants.getColumn(33).getString());
            if (!query_participants.getColumn(34).isNull())
                participant.metadata.tags = deserializeStringList(query_participants.getColumn(34).getString());
            if (!query_participants.getColumn(35).isNull())
                participant.metadata.createdAtUtc = query_participants.getColumn(35).getString();
            if (!query_participants.getColumn(36).isNull())
                participant.metadata.updatedAtUtc = query_participants.getColumn(36).getString();
            if (participant.startingX == 0 && participant.startingY == 0 && (participant.x != 0 || participant.y != 0))
            {
                participant.startingX = participant.x;
                participant.startingY = participant.y;
            }

            encounter.participants.push_back(participant);
        }

        if (!loadedCombatPhase)
            encounter.combatPhase = inferCombatPhaseFromLegacyState(encounter);
        normalizeEncounter(encounter);
        campaign.encounters.push_back(encounter);
    }
}
