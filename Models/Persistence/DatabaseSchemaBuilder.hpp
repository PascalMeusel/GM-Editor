#pragma once

#include "SQLiteCpp/SQLiteCpp.h"

#include <string>

class DatabaseSchemaBuilder final
{
  public:
    explicit DatabaseSchemaBuilder(SQLite::Database &database);

    void initialize();

  private:
    bool columnExists(const std::string &tableName, const std::string &columnName) const;
    void addColumnIfMissing(const std::string &tableName, const std::string &columnName,
                            const std::string &columnDefinition);
    void createCoreTables();
    void createWorldTables();
    void createContentTables();
    void createMapAndAudioTables();
    void createCombatTables();
    void createStoryTables();
    void createSessionTables();
    void createRulesetTablesAndIndexes();

    SQLite::Database &m_database;
};
