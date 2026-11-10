#pragma once
#include <SQLiteCpp/SQLiteCpp.h>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include <stdexcept>

namespace CollectionReconcile {
// Table/query identifiers are compile-time call-site constants, never user input.
inline void retain(SQLite::Database &db, const std::string &table,
                   const std::string &ownershipQuery, int64_t campaignId,
                   const std::vector<std::string> &ids) {
    std::unordered_set<std::string> wanted;
    for (const auto &id : ids)
        if (id.empty() || !wanted.insert(id).second)
            throw std::runtime_error("Empty or duplicate identity in " + table);
    std::vector<int64_t> removed;
    std::unordered_set<std::string> existing;
    {
        SQLite::Statement rows(db, ownershipQuery);
        while (rows.executeStep()) {
            const auto id = rows.getColumn(1).getString();
            const auto owner = rows.getColumn(2).getInt64();
            if (wanted.count(id)) {
                if (owner != campaignId || !existing.insert(id).second)
                    throw std::runtime_error("Conflicting persisted identity in " + table);
            } else if (owner == campaignId) removed.push_back(rows.getColumn(0).getInt64());
        }
    }
    SQLite::Statement erase(db, "DELETE FROM " + table + " WHERE id = ?;");
    for (auto id : removed) { erase.bind(1, id); erase.exec(); erase.reset(); }
}
inline int64_t rowId(SQLite::Database &db, const std::string &table, const std::string &id) {
    SQLite::Statement query(db, "SELECT id FROM " + table + " WHERE stable_id = ?;");
    query.bind(1, id);
    if (!query.executeStep()) throw std::runtime_error("Missing persisted row in " + table);
    return query.getColumn(0).getInt64();
}
}
