#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createStoryTables()
{
const std::string createStoryThreadsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS story_threads (
            id            INTEGER PRIMARY KEY,
            stable_id     TEXT UNIQUE,
            campaign_id   INTEGER NOT NULL,
            title         TEXT NOT NULL,
            thread_type   INTEGER,
            status        INTEGER,
            summary       TEXT,
            stakes        TEXT,
            gm_notes      TEXT,
            tags          TEXT,
            created_at    TEXT,
            updated_at    TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createStoryThreadsTableSQL);

    const std::string createStoryBeatsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS story_beats (
            id            INTEGER PRIMARY KEY,
            stable_id     TEXT UNIQUE,
            thread_id     INTEGER NOT NULL,
            title         TEXT NOT NULL,
            summary       TEXT,
            player_summary TEXT,
            read_aloud_text TEXT,
            clues         TEXT,
            consequences  TEXT,
            gm_notes      TEXT,
            status        INTEGER,
            in_world_date TEXT,
            cue_audio_asset_id TEXT,
            cue_volume    REAL DEFAULT 1.0,
            cue_loop      INTEGER DEFAULT 1,
            tags          TEXT,
            created_at    TEXT,
            updated_at    TEXT,
            sort_order    INTEGER DEFAULT 0,
            FOREIGN KEY(thread_id) REFERENCES story_threads(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createStoryBeatsTableSQL);
    addColumnIfMissing("story_beats", "player_summary", "TEXT");
    addColumnIfMissing("story_beats", "read_aloud_text", "TEXT");
    addColumnIfMissing("story_beats", "clues", "TEXT");
    addColumnIfMissing("story_beats", "consequences", "TEXT");
    addColumnIfMissing("story_beats", "cue_audio_asset_id", "TEXT");
    addColumnIfMissing("story_beats", "cue_volume", "REAL DEFAULT 1.0");
    addColumnIfMissing("story_beats", "cue_loop", "INTEGER DEFAULT 1");

    const std::string createStoryLinksTableSQL = R"(
        CREATE TABLE IF NOT EXISTS story_links (
            id                 INTEGER PRIMARY KEY,
            stable_id          TEXT UNIQUE,
            campaign_id        INTEGER NOT NULL,
            owner_entity_id    TEXT NOT NULL,
            owner_kind         INTEGER,
            link_type          INTEGER,
            label              TEXT,
            target_entity_id   TEXT,
            target_entity_type TEXT,
            tags               TEXT,
            created_at         TEXT,
            updated_at         TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createStoryLinksTableSQL);
    addColumnIfMissing("story_links", "sort_order", "INTEGER DEFAULT 0");

    const std::string createStoryDocumentsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS story_documents (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            title       TEXT NOT NULL,
            body        TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createStoryDocumentsTableSQL);

    const std::string createTimelineEventsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS timeline_events (
            id                 INTEGER PRIMARY KEY,
            stable_id          TEXT UNIQUE,
            campaign_id        INTEGER NOT NULL,
            title              TEXT NOT NULL,
            description        TEXT,
            in_world_date      TEXT,
            related_entity_ids TEXT,
            tags               TEXT,
            created_at         TEXT,
            updated_at         TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createTimelineEventsTableSQL);

    const std::string createShopsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS shops (
            id            INTEGER PRIMARY KEY,
            stable_id     TEXT UNIQUE,
            campaign_id   INTEGER NOT NULL,
            name          TEXT NOT NULL,
            merchant_name TEXT,
            description   TEXT,
            faction_reputation_notes TEXT,
            inventory_id  TEXT,
            tags          TEXT,
            created_at    TEXT,
            updated_at    TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createShopsTableSQL);
    addColumnIfMissing("shops", "faction_reputation_notes", "TEXT");

    const std::string createShopStockTableSQL = R"(
        CREATE TABLE IF NOT EXISTS shop_stock (
            id               INTEGER PRIMARY KEY,
            shop_id          INTEGER NOT NULL,
            item_id          TEXT NOT NULL,
            quantity         INTEGER DEFAULT 1,
            price_multiplier REAL DEFAULT 1,
            sell_price_multiplier REAL DEFAULT 0.5,
            restocks         INTEGER DEFAULT 0,
            availability     TEXT,
            haggle_notes     TEXT,
            faction_modifier REAL DEFAULT 1,
            reputation_modifier REAL DEFAULT 1,
            FOREIGN KEY(shop_id) REFERENCES shops(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createShopStockTableSQL);
    addColumnIfMissing("shop_stock", "sell_price_multiplier", "REAL DEFAULT 0.5");
    addColumnIfMissing("shop_stock", "availability", "TEXT");
    addColumnIfMissing("shop_stock", "haggle_notes", "TEXT");
    addColumnIfMissing("shop_stock", "faction_modifier", "REAL DEFAULT 1");
    addColumnIfMissing("shop_stock", "reputation_modifier", "REAL DEFAULT 1");

    const std::string createLootParcelsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS loot_parcels (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            source      TEXT,
            notes       TEXT,
            awarded     INTEGER DEFAULT 0,
            awarded_to  TEXT,
            copper      INTEGER DEFAULT 0,
            silver      INTEGER DEFAULT 0,
            gold        INTEGER DEFAULT 0,
            platinum    INTEGER DEFAULT 0,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createLootParcelsTableSQL);

    const std::string createLootParcelItemsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS loot_parcel_items (
            id             INTEGER PRIMARY KEY,
            loot_parcel_id INTEGER NOT NULL,
            item_id        TEXT NOT NULL,
            quantity       INTEGER DEFAULT 1,
            notes          TEXT,
            FOREIGN KEY(loot_parcel_id) REFERENCES loot_parcels(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createLootParcelItemsTableSQL);

    const std::string createDungeonsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS dungeons (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            campaign_id INTEGER NOT NULL,
            name        TEXT NOT NULL,
            description TEXT,
            parent_map_id TEXT,
            active_room_id TEXT,
            gm_notes    TEXT,
            map_ids     TEXT,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createDungeonsTableSQL);
    addColumnIfMissing("dungeons", "parent_map_id", "TEXT");
    addColumnIfMissing("dungeons", "active_room_id", "TEXT");
    addColumnIfMissing("dungeons", "gm_notes", "TEXT");

    const std::string createDungeonRoomsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS dungeon_rooms (
            id            INTEGER PRIMARY KEY,
            stable_id     TEXT UNIQUE,
            dungeon_id    INTEGER NOT NULL,
            name          TEXT NOT NULL,
            description   TEXT,
            keyed_description TEXT,
            read_aloud_text TEXT,
            secrets       TEXT,
            traps         TEXT,
            exploration_notes TEXT,
            exploration_state INTEGER DEFAULT 0,
            map_x         INTEGER DEFAULT 0,
            map_y         INTEGER DEFAULT 0,
            encounter_ids TEXT,
            loot_item_ids TEXT,
            connections   TEXT,
            tags          TEXT,
            created_at    TEXT,
            updated_at    TEXT,
            FOREIGN KEY(dungeon_id) REFERENCES dungeons(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createDungeonRoomsTableSQL);
    addColumnIfMissing("dungeon_rooms", "keyed_description", "TEXT");
    addColumnIfMissing("dungeon_rooms", "read_aloud_text", "TEXT");
    addColumnIfMissing("dungeon_rooms", "secrets", "TEXT");
    addColumnIfMissing("dungeon_rooms", "traps", "TEXT");
    addColumnIfMissing("dungeon_rooms", "exploration_notes", "TEXT");
    addColumnIfMissing("dungeon_rooms", "exploration_state", "INTEGER DEFAULT 0");
    addColumnIfMissing("dungeon_rooms", "map_x", "INTEGER DEFAULT 0");
    addColumnIfMissing("dungeon_rooms", "map_y", "INTEGER DEFAULT 0");
    addColumnIfMissing("dungeon_rooms", "connections", "TEXT");
}
