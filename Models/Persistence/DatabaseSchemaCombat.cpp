#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createCombatTables()
{
const std::string createCustomMonstersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS custom_monsters (
            id               INTEGER PRIMARY KEY,
            stable_id        TEXT UNIQUE,
            campaign_id      INTEGER NOT NULL,
            name             TEXT NOT NULL,
            source           TEXT,
            size             TEXT,
            creature_type    TEXT,
            challenge_rating TEXT,
            armor_class      INTEGER,
            armor_damage_reduction INTEGER DEFAULT 0,
            hit_points       INTEGER,
            speed_feet       INTEGER,
            experience       INTEGER,
            attributes       TEXT,
            actions          TEXT,
            tags             TEXT,
            created_at       TEXT,
            updated_at       TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createCustomMonstersTableSQL);
    addColumnIfMissing("custom_monsters", "armor_damage_reduction", "INTEGER DEFAULT 0");
    addColumnIfMissing("custom_monsters", "catalog_source_id", "TEXT");
    addColumnIfMissing("custom_monsters", "base_catalog_monster_id", "TEXT");
    addColumnIfMissing("custom_monsters", "source_attribution", "TEXT");
    addColumnIfMissing("custom_monsters", "traits", "TEXT");
    addColumnIfMissing("custom_monsters", "reactions", "TEXT");
    addColumnIfMissing("custom_monsters", "legendary_actions", "TEXT");
    addColumnIfMissing("custom_monsters", "lair_actions", "TEXT");
    addColumnIfMissing("custom_monsters", "recharge_actions", "TEXT");
    addColumnIfMissing("custom_monsters", "senses", "TEXT");
    addColumnIfMissing("custom_monsters", "languages", "TEXT");
    addColumnIfMissing("custom_monsters", "saving_throws", "TEXT");
    addColumnIfMissing("custom_monsters", "skills", "TEXT");
    addColumnIfMissing("custom_monsters", "damage_resistances", "TEXT");
    addColumnIfMissing("custom_monsters", "damage_immunities", "TEXT");
    addColumnIfMissing("custom_monsters", "damage_vulnerabilities", "TEXT");
    addColumnIfMissing("custom_monsters", "condition_immunities", "TEXT");
    addColumnIfMissing("custom_monsters", "encounter_role_tags", "TEXT");

    const std::string createEncountersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS encounters (
            id                       INTEGER PRIMARY KEY,
            stable_id                TEXT UNIQUE,
            campaign_id              INTEGER NOT NULL,
            name                     TEXT NOT NULL,
            description              TEXT,
            map_id                   TEXT,
            map_context_name         TEXT,
            map_context_x            INTEGER DEFAULT 0,
            map_context_y            INTEGER DEFAULT 0,
            battlefield_width        INTEGER,
            battlefield_height       INTEGER,
            cell_size_feet           INTEGER,
            use_map_cells            INTEGER,
            theater_of_mind          INTEGER DEFAULT 0,
            combat_paused            INTEGER DEFAULT 0,
            combat_phase             TEXT,
            battlefield_cells        TEXT,
            combat_log               TEXT,
            dice_roll_presets        TEXT,
            after_combat_summary     TEXT,
            after_combat_loot_notes  TEXT,
            after_combat_story_consequences TEXT,
            after_combat_session_notes TEXT,
            round                    INTEGER,
            active_participant_index INTEGER,
            tags                     TEXT,
            created_at               TEXT,
            updated_at               TEXT,
            source_map_id            TEXT,
            source_rect_x            REAL,
            source_rect_y            REAL,
            source_rect_width        REAL,
            source_rect_height       REAL,
            source_map_revision      INTEGER DEFAULT 0,
            tactical_map_id          TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createEncountersTableSQL);
    addColumnIfMissing("encounters", "map_context_name", "TEXT");
    addColumnIfMissing("encounters", "map_context_x", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounters", "map_context_y", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounters", "battlefield_cells", "TEXT");
    addColumnIfMissing("encounters", "combat_log", "TEXT");
    addColumnIfMissing("encounters", "dice_roll_presets", "TEXT");
    addColumnIfMissing("encounters", "combat_paused", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounters", "combat_phase", "TEXT");
    addColumnIfMissing("encounters", "theater_of_mind", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounters", "after_combat_summary", "TEXT");
    addColumnIfMissing("encounters", "after_combat_loot_notes", "TEXT");
    addColumnIfMissing("encounters", "after_combat_story_consequences", "TEXT");
    addColumnIfMissing("encounters", "after_combat_session_notes", "TEXT");
    addColumnIfMissing("encounters", "source_map_id", "TEXT");
    addColumnIfMissing("encounters", "source_rect_x", "REAL");
    addColumnIfMissing("encounters", "source_rect_y", "REAL");
    addColumnIfMissing("encounters", "source_rect_width", "REAL");
    addColumnIfMissing("encounters", "source_rect_height", "REAL");
    addColumnIfMissing("encounters", "source_map_revision", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounters", "tactical_map_id", "TEXT");

    const std::string createEncounterParticipantsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS encounter_participants (
            id                    INTEGER PRIMARY KEY,
            stable_id             TEXT UNIQUE,
            encounter_id          INTEGER NOT NULL,
            kind                  INTEGER,
            source_entity_id      TEXT,
            monster_definition_id TEXT,
            display_name          TEXT,
            initiative            INTEGER,
            current_hp            INTEGER,
            max_hp                INTEGER,
            temporary_hp          INTEGER DEFAULT 0,
            armor_class           INTEGER,
            armor_damage_reduction INTEGER DEFAULT 0,
            speed_feet            INTEGER,
            group_name            TEXT,
            wave_number           INTEGER DEFAULT 1,
            starts_hidden         INTEGER DEFAULT 0,
            reinforcement_round   INTEGER DEFAULT 0,
            starting_x            INTEGER DEFAULT 0,
            starting_y            INTEGER DEFAULT 0,
            grid_x                INTEGER,
            grid_y                INTEGER,
            turn_start_x          INTEGER DEFAULT 0,
            turn_start_y          INTEGER DEFAULT 0,
            movement_used_feet    INTEGER,
            defeated              INTEGER,
            friendly              INTEGER,
            narrative_position    TEXT,
            narrative_range_band  TEXT,
            narrative_spotlight   INTEGER DEFAULT 0,
            damage_resistances    TEXT,
            damage_vulnerabilities TEXT,
            damage_immunities     TEXT,
            conditions            TEXT,
            active_effects        TEXT,
            actions               TEXT,
            tags                  TEXT,
            created_at            TEXT,
            updated_at            TEXT,
            FOREIGN KEY(encounter_id) REFERENCES encounters(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createEncounterParticipantsTableSQL);
    addColumnIfMissing("encounter_participants", "sort_order", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "armor_damage_reduction", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "temporary_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "group_name", "TEXT");
    addColumnIfMissing("encounter_participants", "wave_number", "INTEGER DEFAULT 1");
    addColumnIfMissing("encounter_participants", "starts_hidden", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "reinforcement_round", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "starting_x", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "starting_y", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "damage_resistances", "TEXT");
    addColumnIfMissing("encounter_participants", "damage_vulnerabilities", "TEXT");
    addColumnIfMissing("encounter_participants", "damage_immunities", "TEXT");
    addColumnIfMissing("encounter_participants", "turn_start_x", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "turn_start_y", "INTEGER DEFAULT 0");
    addColumnIfMissing("encounter_participants", "active_effects", "TEXT");
    addColumnIfMissing("encounter_participants", "narrative_position", "TEXT");
    addColumnIfMissing("encounter_participants", "narrative_range_band", "TEXT");
    addColumnIfMissing("encounter_participants", "narrative_spotlight", "INTEGER DEFAULT 0");
}
