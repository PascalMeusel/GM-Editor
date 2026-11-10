#include "Models/Persistence/CollectionReconcile.hpp"
#include "Models/DataBaseManager.hpp"
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

void DatabaseManager::saveInventory(int64_t campaignId, const std::string &ownerType, const EntityId &ownerEntityId,
                                    const InventoryModel &inventory)
{
    InventoryModel normalizedInventory = inventory;
    normalizedInventory.ownerEntityId = ownerEntityId;
    if (normalizedInventory.name.empty())
        normalizedInventory.name = ownerType + " inventory";

    if (!inventoryUpsert) inventoryUpsert = std::make_unique<SQLite::Statement>(*m_db, R"(
        INSERT INTO inventories
        (stable_id, campaign_id, owner_entity_id, owner_type, name, copper, silver, gold, platinum, tags, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(stable_id) DO UPDATE SET
            campaign_id = excluded.campaign_id,
            owner_entity_id = excluded.owner_entity_id,
            owner_type = excluded.owner_type,
            name = excluded.name,
            copper = excluded.copper,
            silver = excluded.silver,
            gold = excluded.gold,
            platinum = excluded.platinum,
            tags = excluded.tags,
            updated_at = excluded.updated_at
        WHERE inventories.campaign_id IS NOT excluded.campaign_id
           OR inventories.owner_entity_id IS NOT excluded.owner_entity_id
           OR inventories.owner_type IS NOT excluded.owner_type
           OR inventories.name IS NOT excluded.name
           OR inventories.copper IS NOT excluded.copper
           OR inventories.silver IS NOT excluded.silver
           OR inventories.gold IS NOT excluded.gold
           OR inventories.platinum IS NOT excluded.platinum
           OR inventories.tags IS NOT excluded.tags;
    )");
    auto &insertInventory = *inventoryUpsert;
    insertInventory.reset();
    insertInventory.bind(1, normalizedInventory.metadata.id);
    insertInventory.bind(2, campaignId);
    insertInventory.bind(3, normalizedInventory.ownerEntityId);
    insertInventory.bind(4, ownerType);
    insertInventory.bind(5, normalizedInventory.name);
    insertInventory.bind(6, normalizedInventory.copper);
    insertInventory.bind(7, normalizedInventory.silver);
    insertInventory.bind(8, normalizedInventory.gold);
    insertInventory.bind(9, normalizedInventory.platinum);
    insertInventory.bind(10, serializeStringList(normalizedInventory.metadata.tags));
    insertInventory.bind(11, normalizedInventory.metadata.createdAtUtc);
    insertInventory.bind(12, currentUtcTimestamp());
    insertInventory.exec();

    if (!inventoryRowId) inventoryRowId = std::make_unique<SQLite::Statement>(*m_db, "SELECT id FROM inventories WHERE stable_id = ?;");
    inventoryRowId->reset();
    inventoryRowId->bind(1, normalizedInventory.metadata.id);
    if (!inventoryRowId->executeStep()) throw std::runtime_error("Missing saved inventory");
    const int64_t inventoryId = inventoryRowId->getColumn(0).getInt64();
    inventoryRowId->reset();
    if (!inventoryEntryUpsert) inventoryEntryUpsert = std::make_unique<SQLite::Statement>(*m_db, R"(
        INSERT INTO inventory_entries
        (inventory_id, stable_id, item_id, container_entry_id, quantity, equipped, slot, attuned, charges, ammunition_remaining, durability, state_notes, sort_order, id)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, (SELECT id FROM inventory_entries WHERE stable_id = ?))
        ON CONFLICT(id) DO UPDATE SET
            inventory_id = excluded.inventory_id,
            item_id = excluded.item_id,
            container_entry_id = excluded.container_entry_id,
            quantity = excluded.quantity,
            equipped = excluded.equipped,
            slot = excluded.slot,
            attuned = excluded.attuned,
            charges = excluded.charges,
            ammunition_remaining = excluded.ammunition_remaining,
            durability = excluded.durability,
            state_notes = excluded.state_notes,
            sort_order = excluded.sort_order
        WHERE inventory_entries.inventory_id IS NOT excluded.inventory_id
           OR inventory_entries.item_id IS NOT excluded.item_id
           OR inventory_entries.container_entry_id IS NOT excluded.container_entry_id
           OR inventory_entries.quantity IS NOT excluded.quantity
           OR inventory_entries.equipped IS NOT excluded.equipped
           OR inventory_entries.slot IS NOT excluded.slot
           OR inventory_entries.attuned IS NOT excluded.attuned
           OR inventory_entries.charges IS NOT excluded.charges
           OR inventory_entries.ammunition_remaining IS NOT excluded.ammunition_remaining
           OR inventory_entries.durability IS NOT excluded.durability
           OR inventory_entries.state_notes IS NOT excluded.state_notes
           OR inventory_entries.sort_order IS NOT excluded.sort_order;
    )");
    auto &insertEntry = *inventoryEntryUpsert;
    insertEntry.reset();
    int sortOrder = 0;
    for (const InventoryEntryModel &entry : normalizedInventory.entries)
    {
        insertEntry.bind(1, inventoryId);
        insertEntry.bind(2, entry.metadata.id);
        insertEntry.bind(3, entry.itemId);
        insertEntry.bind(4, entry.containerEntryId);
        insertEntry.bind(5, std::max(1, entry.quantity));
        insertEntry.bind(6, entry.equipped ? 1 : 0);
        insertEntry.bind(7, entry.slot);
        insertEntry.bind(8, entry.attuned ? 1 : 0);
        insertEntry.bind(9, entry.charges);
        insertEntry.bind(10, entry.ammunitionRemaining);
        insertEntry.bind(11, entry.durability);
        insertEntry.bind(12, entry.stateNotes);
        insertEntry.bind(13, sortOrder++);
        insertEntry.bind(14, entry.metadata.id);
        insertEntry.exec();
        insertEntry.reset();
    }
}

InventoryModel DatabaseManager::loadInventoryForOwner(int64_t campaignId, const std::string &ownerType,
                                                      const EntityId &ownerEntityId,
                                                      const InventoryModel &fallbackInventory)
{
    SQLite::Statement queryInventory(*m_db, R"(
        SELECT id, stable_id, name, copper, silver, gold, platinum, tags, created_at, updated_at
        FROM inventories
        WHERE campaign_id = ? AND owner_entity_id = ? AND owner_type = ?
        LIMIT 1;
    )");
    queryInventory.bind(1, campaignId);
    queryInventory.bind(2, ownerEntityId);
    queryInventory.bind(3, ownerType);

    if (!queryInventory.executeStep())
        return fallbackInventory;

    InventoryModel inventory = fallbackInventory;
    const int64_t inventoryId = queryInventory.getColumn(0).getInt64();
    if (!queryInventory.getColumn(1).isNull())
        inventory.metadata.id = queryInventory.getColumn(1).getString();
    if (!queryInventory.getColumn(2).isNull())
        inventory.name = queryInventory.getColumn(2).getString();
    if (!queryInventory.getColumn(3).isNull())
        inventory.copper = queryInventory.getColumn(3).getInt();
    if (!queryInventory.getColumn(4).isNull())
        inventory.silver = queryInventory.getColumn(4).getInt();
    if (!queryInventory.getColumn(5).isNull())
        inventory.gold = queryInventory.getColumn(5).getInt();
    if (!queryInventory.getColumn(6).isNull())
        inventory.platinum = queryInventory.getColumn(6).getInt();
    if (!queryInventory.getColumn(7).isNull())
        inventory.metadata.tags = deserializeStringList(queryInventory.getColumn(7).getString());
    if (!queryInventory.getColumn(8).isNull())
        inventory.metadata.createdAtUtc = queryInventory.getColumn(8).getString();
    if (!queryInventory.getColumn(9).isNull())
        inventory.metadata.updatedAtUtc = queryInventory.getColumn(9).getString();
    inventory.ownerEntityId = ownerEntityId;
    inventory.entries.clear();

    SQLite::Statement queryEntries(*m_db, R"(
        SELECT stable_id, item_id, container_entry_id, quantity, equipped, slot, attuned, charges, ammunition_remaining, durability, state_notes
        FROM inventory_entries
        WHERE inventory_id = ?
        ORDER BY sort_order ASC, id ASC;
    )");
    queryEntries.bind(1, inventoryId);
    while (queryEntries.executeStep())
    {
        InventoryEntryModel entry;
        if (!queryEntries.getColumn(0).isNull())
            entry.metadata.id = queryEntries.getColumn(0).getString();
        if (!queryEntries.getColumn(1).isNull())
            entry.itemId = queryEntries.getColumn(1).getString();
        if (!queryEntries.getColumn(2).isNull())
            entry.containerEntryId = queryEntries.getColumn(2).getString();
        if (!queryEntries.getColumn(3).isNull())
            entry.quantity = std::max(1, queryEntries.getColumn(3).getInt());
        if (!queryEntries.getColumn(4).isNull())
            entry.equipped = queryEntries.getColumn(4).getInt() != 0;
        if (!queryEntries.getColumn(5).isNull())
            entry.slot = queryEntries.getColumn(5).getString();
        if (!queryEntries.getColumn(6).isNull())
            entry.attuned = queryEntries.getColumn(6).getInt() != 0;
        if (!queryEntries.getColumn(7).isNull())
            entry.charges = queryEntries.getColumn(7).getInt();
        if (!queryEntries.getColumn(8).isNull())
            entry.ammunitionRemaining = std::max(0, queryEntries.getColumn(8).getInt());
        if (!queryEntries.getColumn(9).isNull())
            entry.durability = queryEntries.getColumn(9).getInt();
        if (!queryEntries.getColumn(10).isNull())
            entry.stateNotes = queryEntries.getColumn(10).getString();
        if (!entry.itemId.empty())
            inventory.entries.push_back(entry);
    }

    return inventory;
}
