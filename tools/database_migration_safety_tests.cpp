#include "Models/DataBaseManager.hpp"
#include "Models/Persistence/DatabaseSchemaVersion.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "SQLiteCpp/SQLiteCpp.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

void environment(const char *name, const fs::path &value)
{
#if defined(_WIN32)
    require(_putenv_s(name, value.u8string().c_str()) == 0, "Could not isolate test environment");
#else
    require(setenv(name, value.u8string().c_str(), 1) == 0, "Could not isolate test environment");
#endif
}

std::string bytes(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Could not read generated fixture");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void legacy(const fs::path &path, bool malformed = false)
{
    SQLite::Database database(path.u8string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    database.exec("CREATE TABLE campaigns(id INTEGER PRIMARY KEY, stable_id TEXT UNIQUE, name TEXT NOT NULL UNIQUE, ruleset_name TEXT NOT NULL, last_modified TEXT);");
    SQLite::Statement insert(database, "INSERT INTO campaigns(stable_id,name,ruleset_name) VALUES('legacy_campaign','Legacy fixture',?);");
    insert.bind(1, DND5e{}.getVersionId());
    insert.exec();
    database.exec("PRAGMA user_version = 1;");
    // Failure occurs after earlier schema additions. A transaction must roll
    // them all back, not just leave user_version at its old value.
    if (malformed) database.exec("CREATE VIEW characters AS SELECT 1 AS unrelated_column;");
}

std::size_t backups(const fs::path &root)
{
    const auto directory = root / "Resources" / "Backups" / "schema";
    std::size_t count = 0;
    if (fs::is_directory(directory))
        for (const auto &entry : fs::directory_iterator(directory))
            if (entry.is_regular_file() && entry.path().extension() == ".db") ++count;
    return count;
}

void expectOpenFailure(const fs::path &path, const std::string &label)
{
    environment("TTRPG_DB_PATH", path);
    bool failed = false;
    try { DatabaseManager::getInstance(); }
    catch (const std::exception &) { failed = true; }
    require(failed, label + " must reject opening");
}

void testFuture(const fs::path &root)
{
    const auto path = root / "future.db";
    legacy(path);
    {
        SQLite::Database future(path.u8string(), SQLite::OPEN_READWRITE);
        future.exec("PRAGMA user_version = " + std::to_string(DatabaseSchema::CurrentVersion + 1));
    }
    const auto before = bytes(path);
    const auto count = backups(root);
    Campaign retained;
    retained.metadata.id = "retained";
    retained.name = "Unsaved draft";
    retained.ruleSet = std::make_shared<DND5e>();
    const auto originalRules = retained.ruleSet;
    std::string error;
    require(!DatabaseManager::readCampaignFromSnapshot(path.u8string(), "legacy_campaign", retained, error),
            "Future snapshot must be refused");
    require(error.find("newer") != std::string::npos, "Future refusal should explain schema incompatibility");
    require(retained.metadata.id == "retained" && retained.name == "Unsaved draft" && retained.ruleSet == originalRules,
            "Failed snapshot read changed caller's campaign or rules pointer");
    expectOpenFailure(path, "Future live database");
    require(bytes(path) == before && backups(root) == count, "Future refusal modified source or created upgrade backup");
}

void testLegacyRead(const fs::path &root)
{
    const auto path = root / "legacy-read.db";
    legacy(path);
    const auto before = bytes(path);
    const auto backupCount = backups(root);
    Campaign loaded;
    std::string error;
    require(DatabaseManager::readCampaignFromSnapshot(path.u8string(), "legacy_campaign", loaded, error),
            "Legacy snapshot should load through migrated copy: " + error);
    require(loaded.metadata.id == "legacy_campaign" && loaded.name == "Legacy fixture" && loaded.ruleSet,
            "Legacy snapshot changed authored identity or rules");
    require(bytes(path) == before, "Reading legacy snapshot changed its original bytes");
    require(backups(root) == backupCount, "Temporary restore migration changed managed schema backups");
    SQLite::Database original(path.u8string(), SQLite::OPEN_READONLY);
    require(original.execAndGet("PRAGMA user_version").getInt() == 1, "Legacy source version was upgraded");
}

void testRollback(const fs::path &root)
{
    const auto path = root / "malformed.db";
    legacy(path, true);
    const auto before = bytes(path);
    Campaign retained;
    retained.name = "Keep this draft";
    std::string error;
    require(!DatabaseManager::readCampaignFromSnapshot(path.u8string(), "legacy_campaign", retained, error),
            "Malformed snapshot migration should fail");
    require(retained.name == "Keep this draft" && bytes(path) == before,
            "Failed snapshot migration changed caller/source");
    expectOpenFailure(path, "Malformed live schema");
    require(bytes(path) == before, "Failed live migration did not roll back every schema write");
    SQLite::Database original(path.u8string(), SQLite::OPEN_READONLY);
    require(original.execAndGet("SELECT count(*) FROM sqlite_master WHERE name='schema_migrations'").getInt() == 0,
            "Failed migration left ledger table behind");
}

void testWalSnapshot(const fs::path &root)
{
    const auto path = root / "wal-source.db";
    legacy(path);
    SQLite::Database writer(path.u8string(), SQLite::OPEN_READWRITE);
    writer.exec("PRAGMA journal_mode=WAL;");
    writer.exec("PRAGMA wal_autocheckpoint=0;");
    writer.exec("UPDATE campaigns SET name='Committed WAL campaign' WHERE stable_id='legacy_campaign';");
    const auto before = bytes(path);
    const auto walPath = fs::path(path.u8string() + "-wal");
    const auto walBefore = bytes(walPath);
    Campaign loaded;
    std::string error;
    require(DatabaseManager::readCampaignFromSnapshot(path.u8string(), "legacy_campaign", loaded, error),
            "Snapshot copy should include committed WAL rows: " + error);
    require(loaded.name == "Committed WAL campaign", "Snapshot copy omitted committed WAL data");
    require(bytes(path) == before && bytes(walPath) == walBefore,
            "Reading WAL snapshot changed its source database or committed journal");
}

void testBackupFailure(const fs::path &root)
{
    const auto data = root / "blocked-backup";
    fs::create_directory(data);
    const auto path = data / "legacy.db";
    legacy(path);
    { std::ofstream blocker(data / "Resources"); blocker << "generated blocker"; }
    environment("TTRPG_DATA_ROOT", data);
    const auto before = bytes(path);
    expectOpenFailure(path, "Required upgrade backup failure");
    require(bytes(path) == before, "Migration changed database after safety backup failed");
    environment("TTRPG_DATA_ROOT", root);
}

void testSuccessfulUpgradeAndCurrentRead(const fs::path &root)
{
    const auto path = root / "live-legacy.db";
    legacy(path);
    environment("TTRPG_DB_PATH", path);
    const auto count = backups(root);
    auto database = DatabaseManager::getInstance();
    require(backups(root) == count + 1, "Actual live upgrade must create one safety backup");
    SQLite::Database check(path.u8string(), SQLite::OPEN_READONLY);
    require(check.execAndGet("PRAGMA user_version").getInt() == DatabaseSchema::CurrentVersion,
            "Successful migration did not advance schema version");
    const auto before = bytes(path);
    Campaign loaded;
    std::string error;
    const auto afterUpgrade = backups(root);
    require(DatabaseManager::readCampaignFromSnapshot(path.u8string(), "legacy_campaign", loaded, error),
            "Current snapshot should load: " + error);
    require(backups(root) == afterUpgrade && bytes(path) == before,
            "Current-schema read unexpectedly migrated or backed up source");
}
}

int main()
{
    try
    {
        // Keep each run isolated even when this executable is invoked manually.
        const auto root = fs::temp_directory_path() / ("gm-migration-safety-" + AtomicProjectFile::uniqueSuffix());
        require(fs::create_directory(root), "Could not create isolated test root");
        environment("TTRPG_DATA_ROOT", root);
        environment("TTRPG_DB_PATH", root / "unused.db");
        environment("TTRPG_SETTINGS_PATH", root / "settings");
        testFuture(root);
        testLegacyRead(root);
        testRollback(root);
        testWalSnapshot(root);
        testBackupFailure(root);
        testSuccessfulUpgradeAndCurrentRead(root);
        std::cout << "Schema refusal, immutable snapshot reads, migration rollback and required backups passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
