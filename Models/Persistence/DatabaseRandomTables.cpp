#include "Models/DataBaseManager.hpp"
#include "Models/Persistence/DatabaseRows.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"

#include <algorithm>
#include <stdexcept>

using namespace PersistenceSerialization;

void DatabaseManager::saveCampaignRandomTables(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("random_tables", campaignId, randomTableIds(campaign.randomTables));

    SQLite::Statement upsertTable(*m_db, R"(
        INSERT INTO random_tables
        (stable_id, campaign_id, name, description, selection_mode, dice_expression, tags, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            campaign_id = excluded.campaign_id,
            name = excluded.name,
            description = excluded.description,
            selection_mode = excluded.selection_mode,
            dice_expression = excluded.dice_expression,
            tags = excluded.tags,
            updated_at = excluded.updated_at;
    )");
    SQLite::Statement findTable(*m_db, "SELECT id FROM random_tables WHERE stable_id = ? LIMIT 1;");
    SQLite::Statement deleteEntries(*m_db, "DELETE FROM random_table_entries WHERE table_id = ?;");
    SQLite::Statement insertEntry(*m_db, R"(
        INSERT INTO random_table_entries
        (stable_id, table_id, result_text, gm_notes, weight, minimum_roll, maximum_roll, player_safe,
         linked_entity_id, linked_entity_type, linked_entity_label, tags, created_at, updated_at, sort_order)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )");

    for (const RandomTableModel &table : campaign.randomTables)
    {
        upsertTable.bind(1, table.metadata.id);
        upsertTable.bind(2, campaignId);
        upsertTable.bind(3, table.name);
        upsertTable.bind(4, table.description);
        upsertTable.bind(5, static_cast<int>(table.selectionMode));
        upsertTable.bind(6, table.diceExpression);
        upsertTable.bind(7, serializeStringList(table.metadata.tags));
        upsertTable.bind(8, table.metadata.createdAtUtc);
        upsertTable.bind(9, table.metadata.updatedAtUtc);
        upsertTable.exec();
        upsertTable.reset();

        findTable.bind(1, table.metadata.id);
        if (!findTable.executeStep())
            throw std::runtime_error("Could not resolve saved random table row.");
        const int64_t tableId = findTable.getColumn(0).getInt64();
        findTable.reset();

        deleteEntries.bind(1, tableId);
        deleteEntries.exec();
        deleteEntries.reset();

        for (std::size_t index = 0; index < table.entries.size(); ++index)
        {
            const RandomTableEntryModel &entry = table.entries[index];
            insertEntry.bind(1, entry.metadata.id);
            insertEntry.bind(2, tableId);
            insertEntry.bind(3, entry.result);
            insertEntry.bind(4, entry.gmNotes);
            insertEntry.bind(5, std::max(1, entry.weight));
            insertEntry.bind(6, entry.minimumRoll);
            insertEntry.bind(7, entry.maximumRoll);
            insertEntry.bind(8, entry.playerSafe ? 1 : 0);
            insertEntry.bind(9, entry.linkedEntityId);
            insertEntry.bind(10, entry.linkedEntityType);
            insertEntry.bind(11, entry.linkedEntityLabel);
            insertEntry.bind(12, serializeStringList(entry.metadata.tags));
            insertEntry.bind(13, entry.metadata.createdAtUtc);
            insertEntry.bind(14, entry.metadata.updatedAtUtc);
            insertEntry.bind(15, static_cast<int>(index));
            insertEntry.exec();
            insertEntry.reset();
        }
    }
}

void DatabaseManager::loadCampaignRandomTables(Campaign &campaign)
{
    SQLite::Statement queryTables(*m_db, R"(
        SELECT id, stable_id, name, description, selection_mode, dice_expression, tags, created_at, updated_at
        FROM random_tables
        WHERE campaign_id = ?
        ORDER BY name COLLATE NOCASE, id;
    )");
    queryTables.bind(1, static_cast<int64_t>(campaign.id));

    SQLite::Statement queryEntries(*m_db, R"(
        SELECT stable_id, result_text, gm_notes, weight, minimum_roll, maximum_roll, player_safe,
               linked_entity_id, linked_entity_type, linked_entity_label, tags, created_at, updated_at
        FROM random_table_entries
        WHERE table_id = ?
        ORDER BY sort_order, id;
    )");

    while (queryTables.executeStep())
    {
        RandomTableModel table;
        const int64_t tableId = queryTables.getColumn(0).getInt64();
        table.metadata.id = queryTables.getColumn(1).getString();
        table.name = queryTables.getColumn(2).getString();
        table.description = queryTables.getColumn(3).getString();
        table.selectionMode = queryTables.getColumn(4).getInt() == static_cast<int>(RandomTableSelectionMode::DiceRange)
                                  ? RandomTableSelectionMode::DiceRange
                                  : RandomTableSelectionMode::Weighted;
        table.diceExpression = queryTables.getColumn(5).getString();
        table.metadata.tags = deserializeStringList(queryTables.getColumn(6).getString());
        table.metadata.createdAtUtc = queryTables.getColumn(7).getString();
        table.metadata.updatedAtUtc = queryTables.getColumn(8).getString();

        queryEntries.bind(1, tableId);
        while (queryEntries.executeStep())
        {
            RandomTableEntryModel entry;
            entry.metadata.id = queryEntries.getColumn(0).getString();
            entry.result = queryEntries.getColumn(1).getString();
            entry.gmNotes = queryEntries.getColumn(2).getString();
            entry.weight = std::max(1, queryEntries.getColumn(3).getInt());
            entry.minimumRoll = queryEntries.getColumn(4).getInt();
            entry.maximumRoll = queryEntries.getColumn(5).getInt();
            entry.playerSafe = queryEntries.getColumn(6).getInt() != 0;
            entry.linkedEntityId = queryEntries.getColumn(7).getString();
            entry.linkedEntityType = queryEntries.getColumn(8).getString();
            entry.linkedEntityLabel = queryEntries.getColumn(9).getString();
            entry.metadata.tags = deserializeStringList(queryEntries.getColumn(10).getString());
            entry.metadata.createdAtUtc = queryEntries.getColumn(11).getString();
            entry.metadata.updatedAtUtc = queryEntries.getColumn(12).getString();
            table.entries.push_back(std::move(entry));
        }
        queryEntries.reset();
        campaign.randomTables.push_back(std::move(table));
    }
}
