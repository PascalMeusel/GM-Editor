#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace CampaignDeletion
{
// Other persistence paths retain their current connection policy. Deletion
// alone must activate the schema's cascades before opening its transaction.
class ScopedForeignKeys
{
    SQLite::Database &database_;
    const bool enabled_;
public:
    explicit ScopedForeignKeys(SQLite::Database &database)
        : database_(database), enabled_(database.execAndGet("PRAGMA foreign_keys;").getInt() != 0) {}
    void enable()
    {
        if (!enabled_) database_.exec("PRAGMA foreign_keys = ON;");
        // SQLite ignores this pragma inside an existing transaction. Refuse
        // before deleting anything rather than silently leaving orphan rows.
        if (database_.execAndGet("PRAGMA foreign_keys;").getInt() != 1)
            throw std::runtime_error("Campaign deletion could not enable foreign-key cascades.");
    }
    ~ScopedForeignKeys() noexcept
    {
        if (enabled_) return;
        try
        {
            database_.exec("PRAGMA foreign_keys = OFF;");
            if (database_.execAndGet("PRAGMA foreign_keys;").getInt() != 0)
                throw std::runtime_error("Foreign-key mode remained enabled.");
        }
        catch (const std::exception &error)
        {
            // The transaction has already committed or rolled back. Do not
            // throw a cleanup error that would claim a committed delete failed.
            try { std::cerr << "Campaign deletion could not restore connection settings: " << error.what() << '\n'; }
            catch (...) {}
        }
        catch (...)
        {
            try { std::cerr << "Campaign deletion could not restore connection settings.\n"; }
            catch (...) {}
        }
    }
    ScopedForeignKeys(const ScopedForeignKeys &) = delete;
    ScopedForeignKeys &operator=(const ScopedForeignKeys &) = delete;
};

inline void deleteCampaign(SQLite::Database &database, const std::string &stableId)
{
    ScopedForeignKeys keys(database);
    keys.enable();
    // Declared after the setting guard: a failed statement rolls back before
    // the connection mode is restored. Only this campaign's cascades execute.
    SQLite::Transaction transaction(database, SQLite::TransactionBehavior::IMMEDIATE);
    SQLite::Statement properties(database,
        "DELETE FROM campaign_entity_properties WHERE campaign_id IN (SELECT id FROM campaigns WHERE stable_id = ?);");
    properties.bind(1, stableId);
    properties.exec();
    SQLite::Statement campaign(database, "DELETE FROM campaigns WHERE stable_id = ?;");
    campaign.bind(1, stableId);
    campaign.exec();
    transaction.commit();
}
}
