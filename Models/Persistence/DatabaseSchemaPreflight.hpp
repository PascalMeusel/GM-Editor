#pragma once
#include "Models/Persistence/DatabaseSchemaVersion.hpp"
#include "SQLiteCpp/SQLiteCpp.h"
#include <stdexcept>
#include <string>

namespace DatabaseSchema
{
inline int supportedVersion(SQLite::Database &database)
{
    const int version = database.execAndGet("PRAGMA user_version;").getInt();
    if (version < 0 || version > CurrentVersion)
        throw std::runtime_error("This database uses a newer or unsupported schema (" +
                                 std::to_string(version) + "). Open it with a compatible GM Editor version.");
    return version;
}

inline void requireIntegrity(SQLite::Database &database)
{
    SQLite::Statement check(database, "PRAGMA integrity_check;");
    if (!check.executeStep() || check.getColumn(0).getString() != "ok" || check.executeStep())
        throw std::runtime_error("The database failed its integrity check.");
}
}
