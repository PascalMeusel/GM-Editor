#include "Models/DataBaseManager.hpp"
#include "Models/Persistence/DatabaseSchemaVersion.hpp"
#include "Models/Persistence/DatabaseSchemaBuilder.hpp"
#include "Models/Persistence/DatabaseSchemaPreflight.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include <exception>
#include <memory>
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

namespace
{
std::string safeBackupSegment(const std::string &value)
{
    std::string sanitized;
    sanitized.reserve(value.size());
    for (unsigned char character : value)
    {
        if (std::isalnum(character) || character == '-' || character == '_')
            sanitized.push_back(static_cast<char>(character));
        else
            sanitized.push_back('_');
    }
    return sanitized.empty() ? "backup" : sanitized;
}

std::filesystem::path findProjectRootForPersistence()
{
    return ApplicationPaths::writableRoot();
}

void pruneSnapshotFiles(const std::filesystem::path &directory, int maxSnapshots)
{
    std::vector<std::filesystem::directory_entry> snapshots;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".db")
            snapshots.push_back(entry);
    }

    std::sort(snapshots.begin(), snapshots.end(),
              [](const auto &left, const auto &right) { return left.last_write_time() > right.last_write_time(); });

    for (std::size_t index = static_cast<std::size_t>(std::max(0, maxSnapshots)); index < snapshots.size(); ++index)
        std::filesystem::remove(snapshots[index].path());
}
}


bool DatabaseManager::columnExists(const std::string &tableName, const std::string &columnName)
{
    SQLite::Statement query(*m_db, "PRAGMA table_info(" + tableName + ");");
    while (query.executeStep())
    {
        if (query.getColumn(1).getString() == columnName)
            return true;
    }
    return false;
}

void DatabaseManager::addColumnIfMissing(const std::string &tableName, const std::string &columnName,
                                         const std::string &columnDefinition)
{
    if (!columnExists(tableName, columnName))
    {
        m_db->exec("ALTER TABLE " + tableName + " ADD COLUMN " + columnName + " " + columnDefinition + ";");
    }
}

void DatabaseManager::recordSchemaMigrationIfMissing(int version, const std::string &description)
{
    SQLite::Statement exists(*m_db, "SELECT 1 FROM schema_migrations WHERE version = ? LIMIT 1;");
    exists.bind(1, version);
    if (exists.executeStep())
        return;

    SQLite::Statement insert(*m_db, R"(
        INSERT INTO schema_migrations (version, description, applied_at_utc)
        VALUES (?, ?, ?);
    )");
    insert.bind(1, version);
    insert.bind(2, description);
    insert.bind(3, ::currentUtcTimestamp());
    insert.exec();
}

DatabaseManager::DatabaseManager(const std::string &dbPath, bool migrationSourceAlreadyPreserved)
{
    try
    {
        // Snapshot writes already use extended Windows paths. Apply the same
        // I/O boundary on open: deep CI/user recovery paths can exceed MAX_PATH.
        const auto databasePath = mapFileIoPath(std::filesystem::u8path(dbPath));
        const bool existingDatabase = std::filesystem::exists(databasePath) && std::filesystem::file_size(databasePath) > 0;
        int previousVersion = 0;
        if (existingDatabase)
        {
            SQLite::Database preflight(databasePath.u8string(), SQLite::OPEN_READONLY);
            previousVersion = DatabaseSchema::supportedVersion(preflight);
        }
        if (databasePath.has_parent_path())
            std::filesystem::create_directories(databasePath.parent_path());
        m_db = std::make_unique<SQLite::Database>(databasePath.u8string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        if (existingDatabase && previousVersion == DatabaseSchema::CurrentVersion)
            return;
        if (existingDatabase && !migrationSourceAlreadyPreserved)
        {
            try
            {
                const std::filesystem::path backupDirectory = mapFileIoPath(
                    findProjectRootForPersistence() / "Resources" / "Backups" / "schema");
                std::filesystem::create_directories(backupDirectory);
                const std::string timestamp = safeBackupSegment(::currentUtcTimestamp());
                const std::filesystem::path backupPath =
                    backupDirectory / ("pre_schema_migration_" + timestamp + "_" + AtomicProjectFile::uniqueSuffix() + ".db");
                writeSnapshot(backupPath.u8string());
                SQLite::Database backup(backupPath.u8string(), SQLite::OPEN_READONLY);
                DatabaseSchema::requireIntegrity(backup);
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(std::string("Schema upgrade stopped because its safety backup failed: ") + e.what());
            }
        }

        SQLite::Transaction migration(*m_db, SQLite::TransactionBehavior::IMMEDIATE);
        if (existingDatabase && DatabaseSchema::supportedVersion(*m_db) != previousVersion)
            throw std::runtime_error("The database schema changed while preparing its upgrade. Reopen the campaign and retry.");
        DatabaseSchemaBuilder schemaBuilder(*m_db);
        schemaBuilder.initialize();

        recordSchemaMigrationIfMissing(1, "Baseline campaign editor schema with opportunistic column migrations.");
        recordSchemaMigrationIfMissing(
            2,
            "Foundation tables for shops, dungeons, sessions, documents, map tokens/layers, playlists, assets, and homebrew definitions.");
        recordSchemaMigrationIfMissing(3, "Normalized inventory and inventory entry persistence.");
        recordSchemaMigrationIfMissing(4, "Homebrew content pack source and import metadata.");
        recordSchemaMigrationIfMissing(5, "Campaign house rules with change history.");
        recordSchemaMigrationIfMissing(6, "Homebrew content pack enablement state.");
        recordSchemaMigrationIfMissing(
            7, "Map layers, tokens, travel, dungeon rooms, exploration features, and terrain metadata.");
        recordSchemaMigrationIfMissing(8, "Session start and end timestamps for live session mode.");
        recordSchemaMigrationIfMissing(9, "Timestamped session notes linked to campaign entities.");
        recordSchemaMigrationIfMissing(10, "Structured live session recap fields.");
        recordSchemaMigrationIfMissing(11, "Campaign presentation mode for player-safe live views.");
        recordSchemaMigrationIfMissing(12, "Local player display state and public handouts.");
        recordSchemaMigrationIfMissing(13, "Saved filters and smart campaign collections.");
        recordSchemaMigrationIfMissing(14, "Recent entity bookmarks and favorites.");
        recordSchemaMigrationIfMissing(15, "Adjustable campaign calendar and in-world session clock.");
        recordSchemaMigrationIfMissing(16,
                                       "Pinned ruleset versions, campaign overlays, and stable-ID character values.");
        recordSchemaMigrationIfMissing(17, "Atlas nations, normalized territory polygons, and world places.");
        recordSchemaMigrationIfMissing(
            18, "Persist multi-level atlas placement, lineage, visibility, and revision metadata.");
        recordSchemaMigrationIfMissing(19,
                                       "Persist normalized map positions for world places used as generation anchors.");
        recordSchemaMigrationIfMissing(
            20,
            "Persist the local player display layout and normalized public camera while keeping publication runtime-only.");
        recordSchemaMigrationIfMissing(
            21, "Persist safe local player display monitor and map-only fullscreen preferences.");
        recordSchemaMigrationIfMissing(
            22, "Persist campaign content language independently from the application UI locale.");
        recordSchemaMigrationIfMissing(23, "Ordered session plans and explicit recoverable live session state.");
        m_db->exec(R"(
            CREATE TABLE IF NOT EXISTS campaign_entity_properties (
                campaign_id INTEGER NOT NULL REFERENCES campaigns(id) ON DELETE CASCADE,
                entity_id TEXT NOT NULL,
                property_key TEXT NOT NULL,
                property_value TEXT NOT NULL,
                PRIMARY KEY (campaign_id, entity_id, property_key)
            );
        )");
        recordSchemaMigrationIfMissing(24, "Persist workspace context links and campaign entity attribution properties.");
        recordSchemaMigrationIfMissing(25, "Explicit encounter participant and story link ordering; inventory entry lookup index.");
        recordSchemaMigrationIfMissing(26, "Persist explicit character class-level history.");
        initializeTabletopToolkitSchema();
        recordSchemaMigrationIfMissing(27, "Campaign dice macros, objective and clue tracking, and safety tools.");
        recordSchemaMigrationIfMissing(28, "Persistent campaign random tables with weighted or dice-range entries.");
        recordSchemaMigrationIfMissing(
            29, "GM-opt-in player party summaries and presentation-only player text scale.");
        recordSchemaMigrationIfMissing(30, "Explicit GM opt-in for selected party spell slot counts.");
        recordSchemaMigrationIfMissing(31, "Explicit GM opt-in for selected party inventory names and quantities.");
        recordSchemaMigrationIfMissing(32, "Independent Legacy, Calculated and Manual spell attack and save DC values.");
        m_db->exec("PRAGMA user_version = " + std::to_string(DatabaseSchema::CurrentVersion) + ";");
        migration.commit();
        if (existingDatabase && !migrationSourceAlreadyPreserved)
        {
            // Retention runs after commit; a maintenance failure cannot undo
            // the completed upgrade or prune older backups before success.
            try { pruneSnapshotFiles(mapFileIoPath(findProjectRootForPersistence() / "Resources" / "Backups" / "schema"), 10); }
            catch (const std::exception &e) { ProjectLogger::getInstance().warning("DatabaseManager", e.what()); }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "DatabaseManager Fehler: " << e.what() << std::endl;
        ProjectLogger::getInstance().error("DatabaseManager", e.what());
        throw; // re-throw exception
    }
}
