#pragma once
#include "Models/Persistence/DatabaseSchemaPreflight.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "SQLiteCpp/Backup.h"
#include <sqlite3.h>

// Include committed WAL pages without modifying the source. Only the owned
// temporary copy may be upgraded by DatabaseManager.
class DatabaseSnapshotReadCopy
{
public:
    explicit DatabaseSnapshotReadCopy(const std::filesystem::path &source)
    {
        SQLite::Database original(mapFileIoPath(source).u8string(), SQLite::OPEN_READONLY);
        DatabaseSchema::supportedVersion(original);
        DatabaseSchema::requireIntegrity(original);
        directory_ = mapFileIoPath(std::filesystem::temp_directory_path() /
                                  ("gm-snapshot-read-" + AtomicProjectFile::uniqueSuffix()));
        if (!std::filesystem::create_directory(directory_))
            throw std::runtime_error("Could not create a private snapshot read directory.");
        try
        {
#if !defined(_WIN32)
            std::filesystem::permissions(directory_, std::filesystem::perms::owner_all,
                                         std::filesystem::perm_options::replace);
#endif
            path_ = directory_ / "snapshot.db";
            SQLite::Database copy(path_.u8string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            {
                SQLite::Backup backup(copy, original);
                if (backup.executeStep(-1) != SQLITE_DONE)
                    throw std::runtime_error("Could not copy the selected database snapshot.");
            }
            DatabaseSchema::requireIntegrity(copy);
        }
        catch (...)
        {
            cleanup();
            throw;
        }
    }
    ~DatabaseSnapshotReadCopy() { cleanup(); }
    DatabaseSnapshotReadCopy(const DatabaseSnapshotReadCopy &) = delete;
    DatabaseSnapshotReadCopy &operator=(const DatabaseSnapshotReadCopy &) = delete;
    const std::filesystem::path &path() const { return path_; }

private:
    void cleanup() noexcept
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }
    std::filesystem::path directory_, path_;
};
