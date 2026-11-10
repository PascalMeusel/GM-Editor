#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createSessionTables()
{
const std::string createSessionsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id                        INTEGER PRIMARY KEY,
            stable_id                 TEXT UNIQUE,
            campaign_id               INTEGER NOT NULL,
            title                     TEXT NOT NULL,
            scheduled_at_utc          TEXT,
            started_at_utc            TEXT,
            ended_at_utc              TEXT,
            summary                   TEXT,
            recap                     TEXT,
            unresolved_threads        TEXT,
            loot_awarded              TEXT,
            xp_milestones             TEXT,
            npcs_met                  TEXT,
            locations_visited         TEXT,
            open_hooks                TEXT,
            participant_character_ids TEXT,
            encounter_ids             TEXT,
            story_document_ids        TEXT,
            live_mode_active          INTEGER DEFAULT 0,
            tags                      TEXT,
            created_at                TEXT,
            updated_at                TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createSessionsTableSQL);
    addColumnIfMissing("sessions", "started_at_utc", "TEXT");
    addColumnIfMissing("sessions", "ended_at_utc", "TEXT");
    addColumnIfMissing("sessions", "recap", "TEXT");
    addColumnIfMissing("sessions", "unresolved_threads", "TEXT");
    addColumnIfMissing("sessions", "loot_awarded", "TEXT");
    addColumnIfMissing("sessions", "xp_milestones", "TEXT");
    addColumnIfMissing("sessions", "npcs_met", "TEXT");
    addColumnIfMissing("sessions", "locations_visited", "TEXT");
    addColumnIfMissing("sessions", "open_hooks", "TEXT");
    addColumnIfMissing("sessions", "rough_notes", "TEXT");
    addColumnIfMissing("sessions", "plan_entries_json", "TEXT");
    addColumnIfMissing("sessions", "live_state_json", "TEXT");

    const std::string createSessionNotesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS session_notes (
            id                  INTEGER PRIMARY KEY,
            stable_id           TEXT UNIQUE,
            session_id          INTEGER NOT NULL,
            timestamp_utc       TEXT,
            body                TEXT NOT NULL,
            linked_entity_id    TEXT,
            linked_entity_type  TEXT,
            linked_entity_label TEXT,
            tags                TEXT,
            created_at          TEXT,
            updated_at          TEXT,
            FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createSessionNotesTableSQL);

    const std::string createHandoutsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS handouts (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            title       TEXT NOT NULL,
            body        TEXT,
            file_path   TEXT,
            gm_notes    TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createHandoutsTableSQL);

    const std::string createRandomTablesSQL = R"(
        CREATE TABLE IF NOT EXISTS random_tables (
            id              INTEGER PRIMARY KEY,
            stable_id       TEXT NOT NULL UNIQUE,
            campaign_id     INTEGER NOT NULL,
            name            TEXT NOT NULL,
            description     TEXT,
            selection_mode  INTEGER NOT NULL DEFAULT 0,
            dice_expression TEXT NOT NULL DEFAULT '1d100',
            tags            TEXT,
            created_at      TEXT,
            updated_at      TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createRandomTablesSQL);

    const std::string createRandomTableEntriesSQL = R"(
        CREATE TABLE IF NOT EXISTS random_table_entries (
            id                  INTEGER PRIMARY KEY,
            stable_id           TEXT NOT NULL UNIQUE,
            table_id            INTEGER NOT NULL,
            result_text         TEXT NOT NULL,
            gm_notes            TEXT,
            weight              INTEGER NOT NULL DEFAULT 1,
            minimum_roll        INTEGER NOT NULL DEFAULT 1,
            maximum_roll        INTEGER NOT NULL DEFAULT 1,
            player_safe         INTEGER NOT NULL DEFAULT 0,
            linked_entity_id    TEXT,
            linked_entity_type  TEXT,
            linked_entity_label TEXT,
            tags                TEXT,
            created_at          TEXT,
            updated_at          TEXT,
            sort_order          INTEGER NOT NULL DEFAULT 0,
            FOREIGN KEY(table_id) REFERENCES random_tables(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createRandomTableEntriesSQL);

    const std::string createLocalPlayerDisplayTableSQL = R"(
        CREATE TABLE IF NOT EXISTS local_player_display (
            id                   INTEGER PRIMARY KEY,
            campaign_id          INTEGER NOT NULL UNIQUE,
            stable_id            TEXT UNIQUE,
            enabled              INTEGER DEFAULT 0,
            public_scene_id      TEXT,
            public_map_id        TEXT,
            public_encounter_id  TEXT,
            show_map             INTEGER DEFAULT 1,
            show_initiative      INTEGER DEFAULT 1,
            show_visible_npcs    INTEGER DEFAULT 1,
            show_party           INTEGER DEFAULT 0,
            show_party_spell_slots INTEGER DEFAULT 0,
            show_party_inventory  INTEGER DEFAULT 0,
            show_public_handouts INTEGER DEFAULT 1,
            show_shared_art      INTEGER DEFAULT 1,
            visible_npc_ids      TEXT,
            visible_character_ids TEXT,
            public_handout_ids   TEXT,
            shared_art_asset_ids TEXT,
            map_only             INTEGER DEFAULT 1,
            preferred_monitor_index INTEGER DEFAULT -1,
            fullscreen_map_only  INTEGER DEFAULT 1,
            camera_center_x      REAL DEFAULT 0.5,
            camera_center_y      REAL DEFAULT 0.5,
            camera_zoom          REAL DEFAULT 1.0,
            follow_public_camera INTEGER DEFAULT 1,
            player_text_scale    REAL DEFAULT 1.15,
            tags                 TEXT,
            created_at           TEXT,
            updated_at           TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createLocalPlayerDisplayTableSQL);
    addColumnIfMissing("local_player_display", "map_only", "INTEGER DEFAULT 1");
    addColumnIfMissing("local_player_display", "preferred_monitor_index", "INTEGER DEFAULT -1");
    // Existing panel-based layouts must not unexpectedly become full-screen
    // after upgrading. New rows persist the model's explicit preference.
    addColumnIfMissing("local_player_display", "fullscreen_map_only", "INTEGER DEFAULT 0");
    addColumnIfMissing("local_player_display", "camera_center_x", "REAL DEFAULT 0.5");
    addColumnIfMissing("local_player_display", "camera_center_y", "REAL DEFAULT 0.5");
    addColumnIfMissing("local_player_display", "camera_zoom", "REAL DEFAULT 1.0");
    addColumnIfMissing("local_player_display", "follow_public_camera", "INTEGER DEFAULT 1");
    // Legacy campaigns must opt in explicitly; a text scale is presentation
    // state only and is normalized by the model on load.
    addColumnIfMissing("local_player_display", "show_party", "INTEGER DEFAULT 0");
    addColumnIfMissing("local_player_display", "show_party_spell_slots", "INTEGER DEFAULT 0");
    addColumnIfMissing("local_player_display", "show_party_inventory", "INTEGER DEFAULT 0");
    addColumnIfMissing("local_player_display", "visible_character_ids", "TEXT");
    addColumnIfMissing("local_player_display", "player_text_scale", "REAL DEFAULT 1.15");
}
