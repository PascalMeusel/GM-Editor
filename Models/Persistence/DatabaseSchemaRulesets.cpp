#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createRulesetTablesAndIndexes()
{
const std::string createRulesetDefinitionsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS ruleset_definitions (
            id             INTEGER PRIMARY KEY,
            stable_id      TEXT UNIQUE,
            campaign_id    INTEGER,
            name           TEXT NOT NULL,
            version        TEXT,
            base_ruleset_id TEXT,
            definition_json TEXT,
            installed      INTEGER DEFAULT 0,
            tags           TEXT,
            created_at     TEXT,
            updated_at     TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createRulesetDefinitionsTableSQL);

    const std::string createHomebrewDefinitionsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS homebrew_definitions (
            id             INTEGER PRIMARY KEY,
            stable_id      TEXT UNIQUE,
            campaign_id    INTEGER,
            ruleset_id     TEXT,
            content_type   TEXT NOT NULL,
            name           TEXT NOT NULL,
            version        TEXT,
            author         TEXT,
            license        TEXT,
            source         TEXT,
            imported_at_utc TEXT,
            enabled        INTEGER DEFAULT 1,
            definition_json TEXT,
            tags           TEXT,
            created_at     TEXT,
            updated_at     TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createHomebrewDefinitionsTableSQL);
    addColumnIfMissing("homebrew_definitions", "source", "TEXT");
    addColumnIfMissing("homebrew_definitions", "imported_at_utc", "TEXT");
    addColumnIfMissing("homebrew_definitions", "enabled", "INTEGER DEFAULT 1");

    const std::string createHouseRulesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS house_rules (
            id             INTEGER PRIMARY KEY,
            stable_id      TEXT UNIQUE,
            campaign_id    INTEGER NOT NULL,
            label          TEXT NOT NULL,
            description    TEXT,
            enabled        INTEGER DEFAULT 1,
            change_history TEXT,
            tags           TEXT,
            created_at     TEXT,
            updated_at     TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createHouseRulesTableSQL);

    const std::string createSavedFiltersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS saved_filters (
            id               INTEGER PRIMARY KEY,
            stable_id        TEXT UNIQUE,
            campaign_id      INTEGER NOT NULL,
            name             TEXT NOT NULL,
            query            TEXT,
            entity_types     TEXT,
            required_tags    TEXT,
            match_all_tags   INTEGER DEFAULT 0,
            smart_collection INTEGER DEFAULT 1,
            tags             TEXT,
            created_at       TEXT,
            updated_at       TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createSavedFiltersTableSQL);

    const std::string createEntityBookmarksTableSQL = R"(
        CREATE TABLE IF NOT EXISTS entity_bookmarks (
            id                 INTEGER PRIMARY KEY,
            stable_id          TEXT UNIQUE,
            campaign_id        INTEGER NOT NULL,
            entity_id          TEXT NOT NULL,
            entity_type        TEXT,
            label              TEXT,
            favorite           INTEGER DEFAULT 0,
            last_opened_at_utc TEXT,
            open_count         INTEGER DEFAULT 0,
            tags               TEXT,
            created_at         TEXT,
            updated_at         TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createEntityBookmarksTableSQL);

    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_campaigns_stable_id ON campaigns(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_characters_stable_id ON characters(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_inventories_stable_id ON inventories(stable_id);");
    m_database.exec("CREATE INDEX IF NOT EXISTS idx_inventory_entries_stable_id ON inventory_entries(stable_id);");
    m_database.exec(
        "CREATE INDEX IF NOT EXISTS idx_inventories_owner ON inventories(campaign_id, owner_entity_id, owner_type);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_npcs_stable_id ON npcs(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_factions_stable_id ON factions(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_deities_stable_id ON deities(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_nations_stable_id ON nations(stable_id);");
    m_database.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_nation_territories_stable_id ON nation_territories(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_world_places_stable_id ON world_places(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_custom_items_stable_id ON custom_items(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_custom_spells_stable_id ON custom_spells(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_custom_monsters_stable_id ON custom_monsters(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_maps_stable_id ON maps(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_map_layers_stable_id ON map_layers(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_map_tokens_stable_id ON map_tokens(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_audio_assets_stable_id ON audio_assets(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_audio_playlists_stable_id ON audio_playlists(stable_id);");
    m_database.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_audio_ambience_layers_stable_id ON audio_ambience_layers(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_audio_triggers_stable_id ON audio_triggers(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_asset_metadata_stable_id ON asset_metadata(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_encounters_stable_id ON encounters(stable_id);");
    m_database.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_encounter_participants_stable_id ON encounter_participants(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_story_threads_stable_id ON story_threads(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_story_beats_stable_id ON story_beats(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_story_links_stable_id ON story_links(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_story_documents_stable_id ON story_documents(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_timeline_events_stable_id ON timeline_events(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_shops_stable_id ON shops(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_dungeons_stable_id ON dungeons(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_dungeon_rooms_stable_id ON dungeon_rooms(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_sessions_stable_id ON sessions(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_session_notes_stable_id ON session_notes(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_handouts_stable_id ON handouts(stable_id);");
    m_database.exec("CREATE INDEX IF NOT EXISTS idx_random_tables_campaign ON random_tables(campaign_id);");
    m_database.exec("CREATE INDEX IF NOT EXISTS idx_random_table_entries_table ON random_table_entries(table_id, sort_order);");
    m_database.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_local_player_display_campaign ON local_player_display(campaign_id);");
    m_database.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_ruleset_definitions_stable_id ON ruleset_definitions(stable_id);");
    m_database.exec(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_homebrew_definitions_stable_id ON homebrew_definitions(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_house_rules_stable_id ON house_rules(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_saved_filters_stable_id ON saved_filters(stable_id);");
    m_database.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_entity_bookmarks_stable_id ON entity_bookmarks(stable_id);");
    m_database.exec("CREATE INDEX IF NOT EXISTS idx_story_links_owner ON story_links(owner_entity_id);");
}
