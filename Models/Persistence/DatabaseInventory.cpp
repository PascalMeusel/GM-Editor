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

void DatabaseManager::saveCampaignPartyInventory(int64_t campaignId, const Campaign &campaign)
{
    inventoryUpsert.reset(); inventoryEntryUpsert.reset(); inventoryRowId.reset();
    std::vector<std::string> inventories, entries;
    const auto collect = [&](const InventoryModel &inventory) {
        inventories.push_back(inventory.metadata.id);
        for (const auto &entry : inventory.entries) entries.push_back(entry.metadata.id);
    };
    collect(campaign.partyInventory);
    for (const auto &character : campaign.characters) collect(character.inventory);
    for (const auto &npc : campaign.npcs) collect(npc.inventory);
    CollectionReconcile::retain(*m_db, "inventory_entries",
        "SELECT e.id, e.stable_id, i.campaign_id FROM inventory_entries e JOIN inventories i ON i.id=e.inventory_id;", campaignId, entries);
    CollectionReconcile::retain(*m_db, "inventories",
        "SELECT id, stable_id, campaign_id FROM inventories;", campaignId, inventories);

    InventoryModel normalizedPartyInventory = campaign.partyInventory;
    normalizedPartyInventory.ownerEntityId = campaign.metadata.id;
    if (normalizedPartyInventory.name.empty())
        normalizedPartyInventory.name = "Party Inventory";
    saveInventory(campaignId, "party", campaign.metadata.id, normalizedPartyInventory);
}

void DatabaseManager::saveCampaignItems(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("custom_items", campaignId, itemIds(campaign.customItems));

    SQLite::Statement query_insert_item(*m_db, R"(
            INSERT INTO custom_items
            (stable_id, campaign_id, name, description, ruleset_key, category, item_type, rarity, weight, base_value, stackable, armor_class_bonus, damage_reduction, weapon_attack_bonus, weapon_damage, weapon_damage_type, weapon_ability, weapon_reach_feet, weapon_range_feet, weapon_long_range_feet, weapon_attack_kind, weapon_mastery, weapon_versatile_damage, ammunition_type, weapon_properties, is_container, container_capacity_weight, container_capacity_items, requires_attunement, max_charges, recharge_rule, cursed, activation, linked_spell_id, command_words, consumable, is_ammunition, max_durability, effects, item_powers, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                description = excluded.description,
                ruleset_key = excluded.ruleset_key,
                category = excluded.category,
                item_type = excluded.item_type,
                rarity = excluded.rarity,
                weight = excluded.weight,
                base_value = excluded.base_value,
                stackable = excluded.stackable,
                armor_class_bonus = excluded.armor_class_bonus,
                damage_reduction = excluded.damage_reduction,
                weapon_attack_bonus = excluded.weapon_attack_bonus,
                weapon_damage = excluded.weapon_damage,
                weapon_damage_type = excluded.weapon_damage_type,
                weapon_ability = excluded.weapon_ability,
                weapon_reach_feet = excluded.weapon_reach_feet,
                weapon_range_feet = excluded.weapon_range_feet,
                weapon_long_range_feet = excluded.weapon_long_range_feet,
                weapon_attack_kind = excluded.weapon_attack_kind,
                weapon_mastery = excluded.weapon_mastery,
                weapon_versatile_damage = excluded.weapon_versatile_damage,
                ammunition_type = excluded.ammunition_type,
                weapon_properties = excluded.weapon_properties,
                is_container = excluded.is_container,
                container_capacity_weight = excluded.container_capacity_weight,
                container_capacity_items = excluded.container_capacity_items,
                requires_attunement = excluded.requires_attunement,
                max_charges = excluded.max_charges,
                recharge_rule = excluded.recharge_rule,
                cursed = excluded.cursed,
                activation = excluded.activation,
                linked_spell_id = excluded.linked_spell_id,
                command_words = excluded.command_words,
                consumable = excluded.consumable,
                is_ammunition = excluded.is_ammunition,
                max_durability = excluded.max_durability,
                effects = excluded.effects,
                item_powers = excluded.item_powers,
                tags = excluded.tags,
                updated_at = excluded.updated_at
            WHERE custom_items.campaign_id IS NOT excluded.campaign_id
               OR custom_items.name IS NOT excluded.name
               OR custom_items.description IS NOT excluded.description
               OR custom_items.ruleset_key IS NOT excluded.ruleset_key
               OR custom_items.category IS NOT excluded.category
               OR custom_items.item_type IS NOT excluded.item_type
               OR custom_items.rarity IS NOT excluded.rarity
               OR custom_items.weight IS NOT excluded.weight
               OR custom_items.base_value IS NOT excluded.base_value
               OR custom_items.stackable IS NOT excluded.stackable
               OR custom_items.armor_class_bonus IS NOT excluded.armor_class_bonus
               OR custom_items.damage_reduction IS NOT excluded.damage_reduction
               OR custom_items.weapon_attack_bonus IS NOT excluded.weapon_attack_bonus
               OR custom_items.weapon_damage IS NOT excluded.weapon_damage
               OR custom_items.weapon_damage_type IS NOT excluded.weapon_damage_type
               OR custom_items.weapon_ability IS NOT excluded.weapon_ability
               OR custom_items.weapon_reach_feet IS NOT excluded.weapon_reach_feet
               OR custom_items.weapon_range_feet IS NOT excluded.weapon_range_feet
               OR custom_items.weapon_long_range_feet IS NOT excluded.weapon_long_range_feet
               OR custom_items.weapon_attack_kind IS NOT excluded.weapon_attack_kind
               OR custom_items.weapon_mastery IS NOT excluded.weapon_mastery
               OR custom_items.weapon_versatile_damage IS NOT excluded.weapon_versatile_damage
               OR custom_items.ammunition_type IS NOT excluded.ammunition_type
               OR custom_items.weapon_properties IS NOT excluded.weapon_properties
               OR custom_items.is_container IS NOT excluded.is_container
               OR custom_items.container_capacity_weight IS NOT excluded.container_capacity_weight
               OR custom_items.container_capacity_items IS NOT excluded.container_capacity_items
               OR custom_items.requires_attunement IS NOT excluded.requires_attunement
               OR custom_items.max_charges IS NOT excluded.max_charges
               OR custom_items.recharge_rule IS NOT excluded.recharge_rule
               OR custom_items.cursed IS NOT excluded.cursed
               OR custom_items.activation IS NOT excluded.activation
               OR custom_items.linked_spell_id IS NOT excluded.linked_spell_id
               OR custom_items.command_words IS NOT excluded.command_words
               OR custom_items.consumable IS NOT excluded.consumable
               OR custom_items.is_ammunition IS NOT excluded.is_ammunition
               OR custom_items.max_durability IS NOT excluded.max_durability
               OR custom_items.effects IS NOT excluded.effects
               OR custom_items.item_powers IS NOT excluded.item_powers
               OR custom_items.tags IS NOT excluded.tags;
        )");
    for (const auto &item : campaign.customItems)
    {
        query_insert_item.bind(1, item.metadata.id);
        query_insert_item.bind(2, campaignId);
        query_insert_item.bind(3, item.name);
        query_insert_item.bind(4, item.description);
        query_insert_item.bind(5, item.rulesetKey);
        query_insert_item.bind(6, getItemCategory(item));
        query_insert_item.bind(7, item.itemType);
        query_insert_item.bind(8, static_cast<int>(item.rarity));
        query_insert_item.bind(9, item.weight);
        query_insert_item.bind(10, item.baseValue);
        query_insert_item.bind(11, item.stackable ? 1 : 0);
        query_insert_item.bind(12, item.armorClassBonus);
        query_insert_item.bind(13, item.damageReduction);
        query_insert_item.bind(14, item.weaponAttackBonus);
        query_insert_item.bind(15, item.weaponDamage);
        query_insert_item.bind(16, item.weaponDamageType);
        query_insert_item.bind(17, item.weaponAbility);
        query_insert_item.bind(18, item.weaponReachFeet);
        query_insert_item.bind(19, item.weaponRangeFeet);
        query_insert_item.bind(20, item.weaponLongRangeFeet);
        query_insert_item.bind(21, item.weaponAttackKind);
        query_insert_item.bind(22, item.weaponMastery);
        query_insert_item.bind(23, item.weaponVersatileDamage);
        query_insert_item.bind(24, item.ammunitionType);
        query_insert_item.bind(25, serializeStringList(item.weaponProperties));
        query_insert_item.bind(26, item.container ? 1 : 0);
        query_insert_item.bind(27, item.containerCapacityWeight);
        query_insert_item.bind(28, item.containerCapacityItems);
        query_insert_item.bind(29, item.requiresAttunement ? 1 : 0);
        query_insert_item.bind(30, item.maxCharges);
        query_insert_item.bind(31, item.rechargeRule);
        query_insert_item.bind(32, item.cursed ? 1 : 0);
        query_insert_item.bind(33, item.activation);
        query_insert_item.bind(34, item.linkedSpellId);
        query_insert_item.bind(35, item.commandWords);
        query_insert_item.bind(36, item.consumable ? 1 : 0);
        query_insert_item.bind(37, item.ammunition ? 1 : 0);
        query_insert_item.bind(38, item.maxDurability);
        query_insert_item.bind(39, serializeStringList(item.effects));
        query_insert_item.bind(40, serializeMonsterActions(item.powers));
        query_insert_item.bind(41, serializeStringList(item.metadata.tags));
        query_insert_item.bind(42, item.metadata.createdAtUtc);
        query_insert_item.bind(43, currentUtcTimestamp());

        query_insert_item.exec();
        query_insert_item.reset();
    }
}

void DatabaseManager::saveCampaignShopsAndLoot(int64_t campaignId, const Campaign &campaign)
{
    deleteRowsMissingStableIds("shops", campaignId, shopStableIds(campaign.shops));
    SQLite::Statement query_delete_shop_stock(*m_db, R"(
            DELETE FROM shop_stock
            WHERE shop_id IN (SELECT id FROM shops WHERE campaign_id = ?);
        )");
    query_delete_shop_stock.bind(1, campaignId);
    query_delete_shop_stock.exec();

    SQLite::Statement query_insert_shop(*m_db, R"(
            INSERT INTO shops
            (stable_id, campaign_id, name, merchant_name, description, faction_reputation_notes, inventory_id, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                merchant_name = excluded.merchant_name,
                description = excluded.description,
                faction_reputation_notes = excluded.faction_reputation_notes,
                inventory_id = excluded.inventory_id,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    SQLite::Statement query_shop_id(*m_db, "SELECT id FROM shops WHERE stable_id = ? LIMIT 1;");
    SQLite::Statement query_insert_shop_stock(*m_db, R"(
            INSERT INTO shop_stock
            (shop_id, item_id, quantity, price_multiplier, sell_price_multiplier, restocks, availability, haggle_notes, faction_modifier, reputation_modifier)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )");
    for (const ShopModel &shop : campaign.shops)
    {
        query_insert_shop.bind(1, shop.metadata.id);
        query_insert_shop.bind(2, campaignId);
        query_insert_shop.bind(3, shop.name.empty() ? "Unnamed Shop" : shop.name);
        query_insert_shop.bind(4, shop.merchantName);
        query_insert_shop.bind(5, shop.description);
        query_insert_shop.bind(6, shop.factionReputationNotes);
        query_insert_shop.bind(7, shop.inventoryId);
        query_insert_shop.bind(8, serializeStringList(shop.metadata.tags));
        query_insert_shop.bind(9, shop.metadata.createdAtUtc);
        query_insert_shop.bind(10, currentUtcTimestamp());
        query_insert_shop.exec();
        query_insert_shop.reset();

        query_shop_id.bind(1, shop.metadata.id);
        if (query_shop_id.executeStep())
        {
            const int64_t shopRowId = query_shop_id.getColumn(0).getInt64();
            for (const ShopStockEntryModel &stock : shop.stock)
            {
                if (stock.itemId.empty())
                    continue;
                query_insert_shop_stock.bind(1, shopRowId);
                query_insert_shop_stock.bind(2, stock.itemId);
                query_insert_shop_stock.bind(3, std::max(0, stock.quantity));
                query_insert_shop_stock.bind(4, stock.priceMultiplier);
                query_insert_shop_stock.bind(5, stock.sellPriceMultiplier);
                query_insert_shop_stock.bind(6, stock.restocks ? 1 : 0);
                query_insert_shop_stock.bind(7, stock.availability);
                query_insert_shop_stock.bind(8, stock.haggleNotes);
                query_insert_shop_stock.bind(9, stock.factionModifier);
                query_insert_shop_stock.bind(10, stock.reputationModifier);
                query_insert_shop_stock.exec();
                query_insert_shop_stock.reset();
            }
        }
        query_shop_id.reset();
    }

    deleteRowsMissingStableIds("loot_parcels", campaignId, lootParcelIds(campaign.lootParcels));
    SQLite::Statement query_delete_loot_items(*m_db, R"(
            DELETE FROM loot_parcel_items
            WHERE loot_parcel_id IN (SELECT id FROM loot_parcels WHERE campaign_id = ?);
        )");
    query_delete_loot_items.bind(1, campaignId);
    query_delete_loot_items.exec();

    SQLite::Statement query_insert_loot(*m_db, R"(
            INSERT INTO loot_parcels
            (stable_id, campaign_id, name, source, notes, awarded, awarded_to, copper, silver, gold, platinum, tags, created_at, updated_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(stable_id) DO UPDATE SET
                campaign_id = excluded.campaign_id,
                name = excluded.name,
                source = excluded.source,
                notes = excluded.notes,
                awarded = excluded.awarded,
                awarded_to = excluded.awarded_to,
                copper = excluded.copper,
                silver = excluded.silver,
                gold = excluded.gold,
                platinum = excluded.platinum,
                tags = excluded.tags,
                updated_at = excluded.updated_at;
        )");
    SQLite::Statement query_loot_id(*m_db, "SELECT id FROM loot_parcels WHERE stable_id = ? LIMIT 1;");
    SQLite::Statement query_insert_loot_item(*m_db, R"(
            INSERT INTO loot_parcel_items
            (loot_parcel_id, item_id, quantity, notes)
            VALUES (?, ?, ?, ?);
        )");
    for (const LootParcelModel &parcel : campaign.lootParcels)
    {
        query_insert_loot.bind(1, parcel.metadata.id);
        query_insert_loot.bind(2, campaignId);
        query_insert_loot.bind(3, parcel.name.empty() ? "Loot Parcel" : parcel.name);
        query_insert_loot.bind(4, parcel.source);
        query_insert_loot.bind(5, parcel.notes);
        query_insert_loot.bind(6, parcel.awarded ? 1 : 0);
        query_insert_loot.bind(7, parcel.awardedTo);
        query_insert_loot.bind(8, parcel.copper);
        query_insert_loot.bind(9, parcel.silver);
        query_insert_loot.bind(10, parcel.gold);
        query_insert_loot.bind(11, parcel.platinum);
        query_insert_loot.bind(12, serializeStringList(parcel.metadata.tags));
        query_insert_loot.bind(13, parcel.metadata.createdAtUtc);
        query_insert_loot.bind(14, currentUtcTimestamp());
        query_insert_loot.exec();
        query_insert_loot.reset();

        query_loot_id.bind(1, parcel.metadata.id);
        if (query_loot_id.executeStep())
        {
            const int64_t parcelRowId = query_loot_id.getColumn(0).getInt64();
            for (const LootParcelItemModel &item : parcel.items)
            {
                if (item.itemId.empty())
                    continue;
                query_insert_loot_item.bind(1, parcelRowId);
                query_insert_loot_item.bind(2, item.itemId);
                query_insert_loot_item.bind(3, std::max(1, item.quantity));
                query_insert_loot_item.bind(4, item.notes);
                query_insert_loot_item.exec();
                query_insert_loot_item.reset();
            }
        }
        query_loot_id.reset();
    }
}
