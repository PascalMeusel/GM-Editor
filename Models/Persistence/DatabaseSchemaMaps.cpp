#include "Models/Persistence/DatabaseSchemaBuilder.hpp"

#include <string>

void DatabaseSchemaBuilder::createMapAndAudioTables()
{
const std::string createMapsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS maps (
            id            INTEGER PRIMARY KEY,
            stable_id     TEXT UNIQUE,
            campaign_id   INTEGER NOT NULL,
            name          TEXT NOT NULL,
            description   TEXT,
            scope         INTEGER,
            grid_type     INTEGER,
            width         INTEGER,
            height        INTEGER,
            cell_size     REAL,
            is_main_map   INTEGER,
            parent_map_id TEXT,
            background_image_path TEXT,
            background_image_opacity REAL DEFAULT 0.65,
            regions       TEXT,
            features      TEXT,
            routes        TEXT,
            generation_data TEXT,
            atlas_data    TEXT,
            cells         TEXT,
            tags          TEXT,
            created_at    TEXT,
            updated_at    TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createMapsTableSQL);
    addColumnIfMissing("maps", "background_image_path", "TEXT");
    addColumnIfMissing("maps", "background_image_opacity", "REAL DEFAULT 0.65");
    addColumnIfMissing("maps", "regions", "TEXT");
    addColumnIfMissing("maps", "features", "TEXT");
    addColumnIfMissing("maps", "routes", "TEXT");
    addColumnIfMissing("maps", "generation_data", "TEXT");
    addColumnIfMissing("maps", "atlas_data", "TEXT");

    const std::string createMapLayersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS map_layers (
            id          INTEGER PRIMARY KEY,
            stable_id   TEXT UNIQUE,
            map_id      INTEGER NOT NULL,
            name        TEXT,
            visible     INTEGER DEFAULT 1,
            locked      INTEGER DEFAULT 0,
            sort_order  INTEGER DEFAULT 0,
            tags        TEXT,
            created_at  TEXT,
            updated_at  TEXT,
            FOREIGN KEY(map_id) REFERENCES maps(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createMapLayersTableSQL);

    const std::string createMapTokensTableSQL = R"(
        CREATE TABLE IF NOT EXISTS map_tokens (
            id                  INTEGER PRIMARY KEY,
            stable_id           TEXT UNIQUE,
            map_id              INTEGER NOT NULL,
            name                TEXT,
            kind                INTEGER DEFAULT 5,
            linked_entity_type  TEXT,
            linked_entity_id    TEXT,
            asset_id            TEXT,
            layer_id            TEXT,
            x                   REAL DEFAULT 0,
            y                   REAL DEFAULT 0,
            rotation            REAL DEFAULT 0,
            size                REAL DEFAULT 1,
            hidden_from_players INTEGER DEFAULT 0,
            tags                TEXT,
            created_at          TEXT,
            updated_at          TEXT,
            FOREIGN KEY(map_id) REFERENCES maps(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createMapTokensTableSQL);
    addColumnIfMissing("map_tokens", "kind", "INTEGER DEFAULT 5");
    addColumnIfMissing("map_tokens", "linked_entity_type", "TEXT");
    addColumnIfMissing("map_tokens", "layer_id", "TEXT");
    addColumnIfMissing("map_tokens", "size", "REAL DEFAULT 1");

    const std::string createAudioAssetsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS audio_assets (
            id                 INTEGER PRIMARY KEY,
            stable_id          TEXT UNIQUE,
            campaign_id        INTEGER NOT NULL,
            name               TEXT NOT NULL,
            original_file_name TEXT,
            original_source_path TEXT,
            file_path          TEXT NOT NULL,
            asset_type         INTEGER,
            default_volume     REAL,
            loop               INTEGER,
            duration_seconds   INTEGER DEFAULT 0,
            missing_file       INTEGER DEFAULT 0,
            last_played_at_utc TEXT,
            tags               TEXT,
            created_at         TEXT,
            updated_at         TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createAudioAssetsTableSQL);
    addColumnIfMissing("audio_assets", "original_file_name", "TEXT");
    addColumnIfMissing("audio_assets", "original_source_path", "TEXT");
    addColumnIfMissing("audio_assets", "duration_seconds", "INTEGER DEFAULT 0");
    addColumnIfMissing("audio_assets", "missing_file", "INTEGER DEFAULT 0");
    addColumnIfMissing("audio_assets", "last_played_at_utc", "TEXT");

    const std::string createAudioPlaylistsTableSQL = R"(
        CREATE TABLE IF NOT EXISTS audio_playlists (
            id                INTEGER PRIMARY KEY,
            stable_id         TEXT UNIQUE,
            campaign_id       INTEGER NOT NULL,
            name              TEXT NOT NULL,
            audio_asset_ids   TEXT,
            shuffle           INTEGER DEFAULT 0,
            repeat            INTEGER DEFAULT 0,
            one_shot          INTEGER DEFAULT 0,
            crossfade_seconds REAL DEFAULT 0,
            tags              TEXT,
            created_at        TEXT,
            updated_at        TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createAudioPlaylistsTableSQL);
    addColumnIfMissing("audio_playlists", "repeat", "INTEGER DEFAULT 0");
    addColumnIfMissing("audio_playlists", "one_shot", "INTEGER DEFAULT 0");

    const std::string createAudioAmbienceLayersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS audio_ambience_layers (
            id             INTEGER PRIMARY KEY,
            stable_id      TEXT UNIQUE,
            campaign_id    INTEGER NOT NULL,
            name           TEXT NOT NULL,
            audio_asset_id TEXT,
            volume         REAL DEFAULT 0.35,
            loop           INTEGER DEFAULT 1,
            active         INTEGER DEFAULT 0,
            sort_order     INTEGER DEFAULT 0,
            tags           TEXT,
            created_at     TEXT,
            updated_at     TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createAudioAmbienceLayersTableSQL);

    const std::string createAudioTriggersTableSQL = R"(
        CREATE TABLE IF NOT EXISTS audio_triggers (
            id               INTEGER PRIMARY KEY,
            stable_id        TEXT UNIQUE,
            campaign_id      INTEGER NOT NULL,
            name             TEXT NOT NULL,
            source_type      TEXT,
            source_entity_id TEXT,
            event_name       TEXT,
            audio_asset_id   TEXT,
            volume           REAL DEFAULT 1,
            loop             INTEGER DEFAULT 0,
            enabled          INTEGER DEFAULT 1,
            tags             TEXT,
            created_at       TEXT,
            updated_at       TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createAudioTriggersTableSQL);

    const std::string createAssetMetadataTableSQL = R"(
        CREATE TABLE IF NOT EXISTS asset_metadata (
            id             INTEGER PRIMARY KEY,
            stable_id      TEXT UNIQUE,
            campaign_id    INTEGER,
            asset_type     TEXT,
            display_name   TEXT,
            source_path    TEXT,
            project_path   TEXT,
            content_hash   TEXT,
            mime_type      TEXT,
            byte_size      INTEGER DEFAULT 0,
            missing        INTEGER DEFAULT 0,
            tags           TEXT,
            created_at     TEXT,
            updated_at     TEXT,
            FOREIGN KEY(campaign_id) REFERENCES campaigns(id) ON DELETE CASCADE
        );
    )";
    m_database.exec(createAssetMetadataTableSQL);
}
