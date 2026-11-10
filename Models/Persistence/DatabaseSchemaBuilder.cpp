#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

DatabaseSchemaBuilder::DatabaseSchemaBuilder(SQLite::Database &database)
    : m_database(database)
{
}

bool DatabaseSchemaBuilder::columnExists(const std::string &tableName, const std::string &columnName) const
{
    SQLite::Statement query(m_database, "PRAGMA table_info(" + tableName + ");");
    while (query.executeStep())
    {
        if (query.getColumn(1).getString() == columnName)
            return true;
    }
    return false;
}

void DatabaseSchemaBuilder::addColumnIfMissing(const std::string &tableName, const std::string &columnName,
                                               const std::string &columnDefinition)
{
    if (!columnExists(tableName, columnName))
        m_database.exec("ALTER TABLE " + tableName + " ADD COLUMN " + columnName + " " + columnDefinition + ";");
}

void DatabaseSchemaBuilder::initialize()
{
    createCoreTables();
    createWorldTables();
    createContentTables();
    createMapAndAudioTables();
    createCombatTables();
    createStoryTables();
    createSessionTables();
    createRulesetTablesAndIndexes();
}

void DatabaseSchemaBuilder::createCoreTables()
{
const std::string createSchemaMigrationsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            id             INTEGER PRIMARY KEY,
            version        INTEGER NOT NULL UNIQUE,
            description    TEXT NOT NULL,
            applied_at_utc TEXT NOT NULL
        );
    )";
    m_database.exec(createSchemaMigrationsTableSQL);

    // SQL command to create the campaigns table
    const std::string createCampaignsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS campaigns (
            id            INTEGER PRIMARY KEY,
            stable_id     TEXT UNIQUE,
            name          TEXT NOT NULL UNIQUE,
            ruleset_name  TEXT NOT NULL,
            description   TEXT,
            tags          TEXT,
            created_at    TEXT,
            updated_at    TEXT,
            party_map_id  TEXT,
            party_x       INTEGER DEFAULT 0,
            party_y       INTEGER DEFAULT 0,
            party_vehicles TEXT,
            party_strongholds TEXT,
            party_renown INTEGER DEFAULT 0,
            party_reputation TEXT,
            audio_master_volume REAL DEFAULT 1,
            audio_music_volume REAL DEFAULT 1,
            audio_ambience_volume REAL DEFAULT 1,
            audio_effects_volume REAL DEFAULT 1,
            audio_voice_volume REAL DEFAULT 1,
            presentation_mode_active INTEGER DEFAULT 0,
            calendar_name TEXT,
            calendar_month_names TEXT,
            calendar_month_lengths TEXT,
            calendar_weekday_names TEXT,
            calendar_hours_per_day INTEGER DEFAULT 24,
            calendar_daylight_hours INTEGER DEFAULT 12,
            calendar_night_hours INTEGER DEFAULT 12,
            calendar_day_starts_at_hour INTEGER DEFAULT 6,
            clock_use_system_time INTEGER DEFAULT 1,
            clock_running INTEGER DEFAULT 1,
            clock_multiplier REAL DEFAULT 1,
            clock_anchor_minutes INTEGER DEFAULT 0,
            clock_anchor_utc TEXT,
            ruleset_id TEXT,
            ruleset_version TEXT,
            ruleset_overlay_id TEXT,
            ruleset_overlay_version TEXT,
            content_language TEXT NOT NULL DEFAULT 'en',
            last_modified DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    m_database.exec(createCampaignsTableSQL);
    addColumnIfMissing("campaigns", "stable_id", "TEXT");
    addColumnIfMissing("campaigns", "description", "TEXT");
    addColumnIfMissing("campaigns", "tags", "TEXT");
    addColumnIfMissing("campaigns", "created_at", "TEXT");
    addColumnIfMissing("campaigns", "updated_at", "TEXT");
    addColumnIfMissing("campaigns", "party_map_id", "TEXT");
    addColumnIfMissing("campaigns", "party_x", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "party_y", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "party_vehicles", "TEXT");
    addColumnIfMissing("campaigns", "party_strongholds", "TEXT");
    addColumnIfMissing("campaigns", "party_renown", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "party_reputation", "TEXT");
    addColumnIfMissing("campaigns", "content_language", "TEXT NOT NULL DEFAULT 'en'");
    addColumnIfMissing("campaigns", "travel_route_id", "TEXT");
    addColumnIfMissing("campaigns", "travel_route_name", "TEXT");
    addColumnIfMissing("campaigns", "travel_pace", "TEXT");
    addColumnIfMissing("campaigns", "travel_distance_miles", "REAL DEFAULT 0");
    addColumnIfMissing("campaigns", "travel_miles_traveled", "REAL DEFAULT 0");
    addColumnIfMissing("campaigns", "travel_watches", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "travel_rations_used", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "travel_random_encounter_checks", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "travel_notes", "TEXT");
    addColumnIfMissing("campaigns", "audio_master_volume", "REAL DEFAULT 1");
    addColumnIfMissing("campaigns", "audio_music_volume", "REAL DEFAULT 1");
    addColumnIfMissing("campaigns", "audio_ambience_volume", "REAL DEFAULT 1");
    addColumnIfMissing("campaigns", "audio_effects_volume", "REAL DEFAULT 1");
    addColumnIfMissing("campaigns", "audio_voice_volume", "REAL DEFAULT 1");
    addColumnIfMissing("campaigns", "presentation_mode_active", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "calendar_name", "TEXT");
    addColumnIfMissing("campaigns", "calendar_month_names", "TEXT");
    addColumnIfMissing("campaigns", "calendar_month_lengths", "TEXT");
    addColumnIfMissing("campaigns", "calendar_weekday_names", "TEXT");
    addColumnIfMissing("campaigns", "calendar_hours_per_day", "INTEGER DEFAULT 24");
    addColumnIfMissing("campaigns", "calendar_daylight_hours", "INTEGER DEFAULT 12");
    addColumnIfMissing("campaigns", "calendar_night_hours", "INTEGER DEFAULT 12");
    addColumnIfMissing("campaigns", "calendar_day_starts_at_hour", "INTEGER DEFAULT 6");
    addColumnIfMissing("campaigns", "clock_use_system_time", "INTEGER DEFAULT 1");
    addColumnIfMissing("campaigns", "clock_running", "INTEGER DEFAULT 1");
    addColumnIfMissing("campaigns", "clock_multiplier", "REAL DEFAULT 1");
    addColumnIfMissing("campaigns", "clock_anchor_minutes", "INTEGER DEFAULT 0");
    addColumnIfMissing("campaigns", "clock_anchor_utc", "TEXT");
    addColumnIfMissing("campaigns", "ruleset_id", "TEXT");
    addColumnIfMissing("campaigns", "ruleset_version", "TEXT");
    addColumnIfMissing("campaigns", "ruleset_overlay_id", "TEXT");
    addColumnIfMissing("campaigns", "ruleset_overlay_version", "TEXT");

    // SQL command to create the characters table with a foreign key to campaigns
    const std::string createCharactersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS characters (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            category    INTEGER DEFAULT 0,
            name        TEXT NOT NULL,
            level       INTEGER NOT NULL,
            class       TEXT,
            race        TEXT,
            background  TEXT,
            attributes  TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            backstory   TEXT,
            player_notes TEXT,
            gm_notes    TEXT,
            character_relationships TEXT,
            feats       TEXT,
            traits_features TEXT,
            languages   TEXT,
            senses      TEXT,
            saving_throw_proficiencies TEXT,
            skill_proficiencies TEXT,
            tool_proficiencies TEXT,
            speed_feet  INTEGER DEFAULT 30,
            current_hp  INTEGER DEFAULT 0,
            max_hp      INTEGER DEFAULT 0,
            temporary_hp INTEGER DEFAULT 0,
            portrait_asset_id TEXT,
            inventory_entries TEXT,
            conditions  TEXT,
            spellcasting_enabled INTEGER DEFAULT 0,
            spellcasting_ability TEXT,
            spell_attack_bonus INTEGER DEFAULT 0,
            spell_save_dc INTEGER DEFAULT 10,
            spell_slots_max TEXT,
            spell_slots_used TEXT,
            spellbook_entries TEXT,
            feature_actions TEXT,
            resource_trackers TEXT,
            rule_values TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createCharactersTableSQL);
    addColumnIfMissing("characters", "class_levels", "TEXT");
    addColumnIfMissing("characters", "stable_id", "TEXT");
    addColumnIfMissing("characters", "tags", "TEXT");
    addColumnIfMissing("characters", "created_at", "TEXT");
    addColumnIfMissing("characters", "updated_at", "TEXT");
    addColumnIfMissing("characters", "background", "TEXT");
    addColumnIfMissing("characters", "backstory", "TEXT");
    addColumnIfMissing("characters", "player_notes", "TEXT");
    addColumnIfMissing("characters", "gm_notes", "TEXT");
    addColumnIfMissing("characters", "character_relationships", "TEXT");
    addColumnIfMissing("characters", "feats", "TEXT");
    addColumnIfMissing("characters", "traits_features", "TEXT");
    addColumnIfMissing("characters", "languages", "TEXT");
    addColumnIfMissing("characters", "senses", "TEXT");
    addColumnIfMissing("characters", "saving_throw_proficiencies", "TEXT");
    addColumnIfMissing("characters", "skill_proficiencies", "TEXT");
    addColumnIfMissing("characters", "tool_proficiencies", "TEXT");
    addColumnIfMissing("characters", "speed_feet", "INTEGER DEFAULT 30");
    addColumnIfMissing("characters", "current_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing("characters", "max_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing("characters", "temporary_hp", "INTEGER DEFAULT 0");
    addColumnIfMissing("characters", "portrait_asset_id", "TEXT");
    addColumnIfMissing("characters", "inventory_entries", "TEXT");
    addColumnIfMissing("characters", "conditions", "TEXT");
    addColumnIfMissing("characters", "spellcasting_enabled", "INTEGER DEFAULT 0");
    addColumnIfMissing("characters", "spellcasting_ability", "TEXT");
    addColumnIfMissing("characters", "spell_attack_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing("characters", "spell_save_dc", "INTEGER DEFAULT 10");
    addColumnIfMissing("characters", "spell_attack_mode", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing("characters", "spell_save_dc_mode", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing("characters", "spell_slots_max", "TEXT");
    addColumnIfMissing("characters", "spell_slots_used", "TEXT");
    addColumnIfMissing("characters", "spellbook_entries", "TEXT");
    addColumnIfMissing("characters", "feature_actions", "TEXT");
    addColumnIfMissing("characters", "resource_trackers", "TEXT");
    addColumnIfMissing("characters", "rule_values", "TEXT");

    const std::string createInventoriesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS inventories (
            id              INTEGER PRIMARY KEY,
            stable_id       TEXT UNIQUE,
            campaign_id     INTEGER NOT NULL,
            owner_entity_id TEXT NOT NULL,
            owner_type      TEXT NOT NULL,
            name            TEXT,
            copper          INTEGER DEFAULT 0,
            silver          INTEGER DEFAULT 0,
            gold            INTEGER DEFAULT 0,
            platinum        INTEGER DEFAULT 0,
            tags            TEXT,
            created_at      TEXT,
            updated_at      TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createInventoriesTableSQL);

    const std::string createInventoryEntriesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS inventory_entries (
            id           INTEGER PRIMARY KEY,
            inventory_id INTEGER NOT NULL,
            stable_id    TEXT,
            item_id      TEXT NOT NULL,
            container_entry_id TEXT,
            quantity     INTEGER DEFAULT 1,
            equipped     INTEGER DEFAULT 0,
            slot         TEXT,
            attuned      INTEGER DEFAULT 0,
            charges      INTEGER DEFAULT -1,
            ammunition_remaining INTEGER DEFAULT 0,
            durability   INTEGER DEFAULT -1,
            state_notes  TEXT,
            sort_order   INTEGER DEFAULT 0,
            FOREIGN KEY(inventory_id) REFERENCES inventories(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createInventoryEntriesTableSQL);
    addColumnIfMissing("inventory_entries", "stable_id", "TEXT");
    addColumnIfMissing("inventory_entries", "container_entry_id", "TEXT");
    addColumnIfMissing("inventory_entries", "attuned", "INTEGER DEFAULT 0");
    addColumnIfMissing("inventory_entries", "charges", "INTEGER DEFAULT -1");
    addColumnIfMissing("inventory_entries", "ammunition_remaining", "INTEGER DEFAULT 0");
    addColumnIfMissing("inventory_entries", "durability", "INTEGER DEFAULT -1");
    addColumnIfMissing("inventory_entries", "state_notes", "TEXT");
}
