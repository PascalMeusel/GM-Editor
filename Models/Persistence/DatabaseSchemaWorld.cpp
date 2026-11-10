#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createWorldTables()
{
const std::string createNpcTableSQL = R"(
        CREATE TABLE IF NOT EXISTS npcs (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            description TEXT,
            faction     TEXT,
            role        TEXT,
            dialogue_notes TEXT,
            attributes  TEXT,
            conditions  TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            inventory_entries TEXT,
            spellcasting_enabled INTEGER DEFAULT 0,
            spellcasting_ability TEXT,
            spell_attack_bonus INTEGER DEFAULT 0,
            spell_save_dc INTEGER DEFAULT 10,
            spell_slots_max TEXT,
            spell_slots_used TEXT,
            spellbook_entries TEXT,
            resource_trackers TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createNpcTableSQL);
    addColumnIfMissing("npcs", "category", "INTEGER DEFAULT 0");
    addColumnIfMissing("npcs", "faction_id", "TEXT");
    addColumnIfMissing("npcs", "deity_id", "TEXT");
    addColumnIfMissing("npcs", "attitude_toward_party", "TEXT");
    addColumnIfMissing("npcs", "secrets", "TEXT");
    addColumnIfMissing("npcs", "goals", "TEXT");
    addColumnIfMissing("npcs", "resources", "TEXT");
    addColumnIfMissing("npcs", "threat_location", "TEXT");
    addColumnIfMissing("npcs", "relationships", "TEXT");
    addColumnIfMissing("npcs", "threat_level", "INTEGER DEFAULT 0");
    addColumnIfMissing("npcs", "inventory_entries", "TEXT");
    addColumnIfMissing("npcs", "spellcasting_enabled", "INTEGER DEFAULT 0");
    addColumnIfMissing("npcs", "spellcasting_ability", "TEXT");
    addColumnIfMissing("npcs", "spell_attack_bonus", "INTEGER DEFAULT 0");
    addColumnIfMissing("npcs", "spell_save_dc", "INTEGER DEFAULT 10");
    addColumnIfMissing("npcs", "spell_attack_mode", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing("npcs", "spell_save_dc_mode", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing("npcs", "spell_slots_max", "TEXT");
    addColumnIfMissing("npcs", "spell_slots_used", "TEXT");
    addColumnIfMissing("npcs", "spellbook_entries", "TEXT");
    addColumnIfMissing("npcs", "resource_trackers", "TEXT");

    const std::string createFactionsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS factions (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            faction_type TEXT,
            summary     TEXT,
            headquarters TEXT,
            leader_npc_id TEXT,
            attitude_toward_party TEXT,
            goals       TEXT,
            resources   TEXT,
            gm_notes    TEXT,
            influence   INTEGER DEFAULT 0,
            threat_level INTEGER DEFAULT 0,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createFactionsTableSQL);

    const std::string createDeitiesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS deities (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            power_type  TEXT,
            pantheon    TEXT,
            domains     TEXT,
            symbol      TEXT,
            alignment   TEXT,
            tenets      TEXT,
            worshipers  TEXT,
            relationships TEXT,
            gm_notes    TEXT,
            influence   INTEGER DEFAULT 0,
            threat_level INTEGER DEFAULT 0,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createDeitiesTableSQL);

    const std::string createNationsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS nations (
            id               INTEGER PRIMARY KEY,
            stable_id        TEXT UNIQUE,
            campaign_id      INTEGER NOT NULL,
            name             TEXT NOT NULL,
            description      TEXT,
            government       TEXT,
            culture          TEXT,
            capital_place_id TEXT,
            relationships    TEXT,
            faction_ids      TEXT,
            visibility       INTEGER DEFAULT 0,
            tags             TEXT,
            created_at       TEXT,
            updated_at       TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createNationsTableSQL);

    const std::string createNationTerritoriesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS nation_territories (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            nation_id   INTEGER NOT NULL,
            name        TEXT,
            polygon     TEXT NOT NULL,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(nation_id) REFERENCES nations(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createNationTerritoriesTableSQL);

    const std::string createWorldPlacesTableSQL = R"(
        CREATE TABLE IF NOT EXISTS world_places (
            id                INTEGER PRIMARY KEY,
            stable_id         TEXT UNIQUE,
            campaign_id       INTEGER NOT NULL,
            place_type        INTEGER DEFAULT 5,
            name              TEXT NOT NULL,
            description       TEXT,
            notes             TEXT,
            nation_id         TEXT,
            linked_map_id     TEXT,
            linked_feature_id TEXT,
            has_map_position  INTEGER DEFAULT 0,
            map_position_x    REAL DEFAULT 0,
            map_position_y    REAL DEFAULT 0,
            visibility        INTEGER DEFAULT 0,
            tags              TEXT,
            created_at        TEXT,
            updated_at        TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createWorldPlacesTableSQL);
    addColumnIfMissing("world_places", "has_map_position", "INTEGER DEFAULT 0");
    addColumnIfMissing("world_places", "map_position_x", "REAL DEFAULT 0");
    addColumnIfMissing("world_places", "map_position_y", "REAL DEFAULT 0");
}
