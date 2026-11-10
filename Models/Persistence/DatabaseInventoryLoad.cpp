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

void DatabaseManager::loadCampaignItems(Campaign &campaign)
{
    SQLite::Statement query_items(*m_db, R"(
                    SELECT stable_id, name, description, ruleset_key, category, item_type, rarity, weight, base_value, stackable, armor_class_bonus, damage_reduction, weapon_attack_bonus, weapon_damage, weapon_damage_type, weapon_ability, weapon_reach_feet, weapon_range_feet, weapon_long_range_feet, weapon_attack_kind, weapon_mastery, weapon_versatile_damage, ammunition_type, weapon_properties, is_container, container_capacity_weight, container_capacity_items, requires_attunement, max_charges, recharge_rule, cursed, activation, linked_spell_id, command_words, consumable, is_ammunition, max_durability, effects, item_powers, tags, created_at, updated_at
                    FROM custom_items
                    WHERE campaign_id = ?;
                )");
    query_items.bind(1, static_cast<int64_t>(campaign.id));

    while (query_items.executeStep())
    {
        ItemModel item;
        if (!query_items.getColumn(0).isNull())
            item.metadata.id = query_items.getColumn(0).getString();
        item.name = query_items.getColumn(1).getString();
        if (!query_items.getColumn(2).isNull())
            item.description = query_items.getColumn(2).getString();
        if (!query_items.getColumn(3).isNull())
            item.rulesetKey = query_items.getColumn(3).getString();
        if (!query_items.getColumn(4).isNull())
            item.category = query_items.getColumn(4).getString();
        if (!query_items.getColumn(5).isNull())
            item.itemType = query_items.getColumn(5).getString();
        if (!query_items.getColumn(6).isNull())
            item.rarity = static_cast<ItemRarity>(query_items.getColumn(6).getInt());
        if (!query_items.getColumn(7).isNull())
            item.weight = query_items.getColumn(7).getDouble();
        if (!query_items.getColumn(8).isNull())
            item.baseValue = query_items.getColumn(8).getInt();
        if (!query_items.getColumn(9).isNull())
            item.stackable = query_items.getColumn(9).getInt() != 0;
        if (!query_items.getColumn(10).isNull())
            item.armorClassBonus = query_items.getColumn(10).getInt();
        if (!query_items.getColumn(11).isNull())
            item.damageReduction = query_items.getColumn(11).getInt();
        if (!query_items.getColumn(12).isNull())
            item.weaponAttackBonus = query_items.getColumn(12).getInt();
        if (!query_items.getColumn(13).isNull())
            item.weaponDamage = query_items.getColumn(13).getString();
        if (!query_items.getColumn(14).isNull())
            item.weaponDamageType = query_items.getColumn(14).getString();
        if (!query_items.getColumn(15).isNull())
            item.weaponAbility = query_items.getColumn(15).getString();
        if (!query_items.getColumn(16).isNull())
            item.weaponReachFeet = query_items.getColumn(16).getInt();
        if (!query_items.getColumn(17).isNull())
            item.weaponRangeFeet = query_items.getColumn(17).getInt();
        if (!query_items.getColumn(18).isNull())
            item.weaponLongRangeFeet = query_items.getColumn(18).getInt();
        if (!query_items.getColumn(19).isNull())
            item.weaponAttackKind = query_items.getColumn(19).getString();
        if (!query_items.getColumn(20).isNull())
            item.weaponMastery = query_items.getColumn(20).getString();
        if (!query_items.getColumn(21).isNull())
            item.weaponVersatileDamage = query_items.getColumn(21).getString();
        if (!query_items.getColumn(22).isNull())
            item.ammunitionType = query_items.getColumn(22).getString();
        if (!query_items.getColumn(23).isNull())
            item.weaponProperties = deserializeStringList(query_items.getColumn(23).getString());
        if (!query_items.getColumn(24).isNull())
            item.container = query_items.getColumn(24).getInt() != 0;
        if (!query_items.getColumn(25).isNull())
            item.containerCapacityWeight = query_items.getColumn(25).getDouble();
        if (!query_items.getColumn(26).isNull())
            item.containerCapacityItems = query_items.getColumn(26).getInt();
        if (!query_items.getColumn(27).isNull())
            item.requiresAttunement = query_items.getColumn(27).getInt() != 0;
        if (!query_items.getColumn(28).isNull())
            item.maxCharges = query_items.getColumn(28).getInt();
        if (!query_items.getColumn(29).isNull())
            item.rechargeRule = query_items.getColumn(29).getString();
        if (!query_items.getColumn(30).isNull())
            item.cursed = query_items.getColumn(30).getInt() != 0;
        if (!query_items.getColumn(31).isNull())
            item.activation = query_items.getColumn(31).getString();
        if (!query_items.getColumn(32).isNull())
            item.linkedSpellId = query_items.getColumn(32).getString();
        if (!query_items.getColumn(33).isNull())
            item.commandWords = query_items.getColumn(33).getString();
        if (!query_items.getColumn(34).isNull())
            item.consumable = query_items.getColumn(34).getInt() != 0;
        if (!query_items.getColumn(35).isNull())
            item.ammunition = query_items.getColumn(35).getInt() != 0;
        if (!query_items.getColumn(36).isNull())
            item.maxDurability = query_items.getColumn(36).getInt();
        if (!query_items.getColumn(37).isNull())
            item.effects = deserializeStringList(query_items.getColumn(37).getString());
        if (!query_items.getColumn(38).isNull())
            item.powers = deserializeMonsterActions(query_items.getColumn(38).getString());
        if (!query_items.getColumn(39).isNull())
            item.metadata.tags = deserializeStringList(query_items.getColumn(39).getString());
        if (!query_items.getColumn(40).isNull())
            item.metadata.createdAtUtc = query_items.getColumn(40).getString();
        if (!query_items.getColumn(41).isNull())
            item.metadata.updatedAtUtc = query_items.getColumn(41).getString();
        if (item.category.empty())
            item.category = item.itemType;

        campaign.customItems.push_back(item);
    }
}

void DatabaseManager::loadCampaignShopsAndLoot(Campaign &campaign)
{
    SQLite::Statement query_shops(*m_db, R"(
                    SELECT id, stable_id, name, merchant_name, description, faction_reputation_notes, inventory_id, tags, created_at, updated_at
                    FROM shops
                    WHERE campaign_id = ?
                    ORDER BY name ASC;
                )");
    query_shops.bind(1, static_cast<int64_t>(campaign.id));
    while (query_shops.executeStep())
    {
        ShopModel shop;
        const int64_t shopRowId = query_shops.getColumn(0).getInt64();
        if (!query_shops.getColumn(1).isNull())
            shop.metadata.id = query_shops.getColumn(1).getString();
        if (!query_shops.getColumn(2).isNull())
            shop.name = query_shops.getColumn(2).getString();
        if (!query_shops.getColumn(3).isNull())
            shop.merchantName = query_shops.getColumn(3).getString();
        if (!query_shops.getColumn(4).isNull())
            shop.description = query_shops.getColumn(4).getString();
        if (!query_shops.getColumn(5).isNull())
            shop.factionReputationNotes = query_shops.getColumn(5).getString();
        if (!query_shops.getColumn(6).isNull())
            shop.inventoryId = query_shops.getColumn(6).getString();
        if (!query_shops.getColumn(7).isNull())
            shop.metadata.tags = deserializeStringList(query_shops.getColumn(7).getString());
        if (!query_shops.getColumn(8).isNull())
            shop.metadata.createdAtUtc = query_shops.getColumn(8).getString();
        if (!query_shops.getColumn(9).isNull())
            shop.metadata.updatedAtUtc = query_shops.getColumn(9).getString();

        SQLite::Statement query_stock(*m_db, R"(
                        SELECT item_id, quantity, price_multiplier, sell_price_multiplier, restocks, availability, haggle_notes, faction_modifier, reputation_modifier
                        FROM shop_stock
                        WHERE shop_id = ?
                        ORDER BY id ASC;
                    )");
        query_stock.bind(1, shopRowId);
        while (query_stock.executeStep())
        {
            ShopStockEntryModel stock;
            if (!query_stock.getColumn(0).isNull())
                stock.itemId = query_stock.getColumn(0).getString();
            if (!query_stock.getColumn(1).isNull())
                stock.quantity = std::max(0, query_stock.getColumn(1).getInt());
            if (!query_stock.getColumn(2).isNull())
                stock.priceMultiplier = query_stock.getColumn(2).getDouble();
            if (!query_stock.getColumn(3).isNull())
                stock.sellPriceMultiplier = query_stock.getColumn(3).getDouble();
            if (!query_stock.getColumn(4).isNull())
                stock.restocks = query_stock.getColumn(4).getInt() != 0;
            if (!query_stock.getColumn(5).isNull())
                stock.availability = query_stock.getColumn(5).getString();
            if (!query_stock.getColumn(6).isNull())
                stock.haggleNotes = query_stock.getColumn(6).getString();
            if (!query_stock.getColumn(7).isNull())
                stock.factionModifier = query_stock.getColumn(7).getDouble();
            if (!query_stock.getColumn(8).isNull())
                stock.reputationModifier = query_stock.getColumn(8).getDouble();
            if (!stock.itemId.empty())
                shop.stock.push_back(stock);
        }
        campaign.shops.push_back(shop);
    }

    SQLite::Statement query_loot(*m_db, R"(
                    SELECT id, stable_id, name, source, notes, awarded, awarded_to, copper, silver, gold, platinum, tags, created_at, updated_at
                    FROM loot_parcels
                    WHERE campaign_id = ?
                    ORDER BY awarded ASC, name ASC;
                )");
    query_loot.bind(1, static_cast<int64_t>(campaign.id));
    while (query_loot.executeStep())
    {
        LootParcelModel parcel;
        const int64_t parcelRowId = query_loot.getColumn(0).getInt64();
        if (!query_loot.getColumn(1).isNull())
            parcel.metadata.id = query_loot.getColumn(1).getString();
        if (!query_loot.getColumn(2).isNull())
            parcel.name = query_loot.getColumn(2).getString();
        if (!query_loot.getColumn(3).isNull())
            parcel.source = query_loot.getColumn(3).getString();
        if (!query_loot.getColumn(4).isNull())
            parcel.notes = query_loot.getColumn(4).getString();
        if (!query_loot.getColumn(5).isNull())
            parcel.awarded = query_loot.getColumn(5).getInt() != 0;
        if (!query_loot.getColumn(6).isNull())
            parcel.awardedTo = query_loot.getColumn(6).getString();
        if (!query_loot.getColumn(7).isNull())
            parcel.copper = query_loot.getColumn(7).getInt();
        if (!query_loot.getColumn(8).isNull())
            parcel.silver = query_loot.getColumn(8).getInt();
        if (!query_loot.getColumn(9).isNull())
            parcel.gold = query_loot.getColumn(9).getInt();
        if (!query_loot.getColumn(10).isNull())
            parcel.platinum = query_loot.getColumn(10).getInt();
        if (!query_loot.getColumn(11).isNull())
            parcel.metadata.tags = deserializeStringList(query_loot.getColumn(11).getString());
        if (!query_loot.getColumn(12).isNull())
            parcel.metadata.createdAtUtc = query_loot.getColumn(12).getString();
        if (!query_loot.getColumn(13).isNull())
            parcel.metadata.updatedAtUtc = query_loot.getColumn(13).getString();

        SQLite::Statement query_loot_items(*m_db, R"(
                        SELECT item_id, quantity, notes
                        FROM loot_parcel_items
                        WHERE loot_parcel_id = ?
                        ORDER BY id ASC;
                    )");
        query_loot_items.bind(1, parcelRowId);
        while (query_loot_items.executeStep())
        {
            LootParcelItemModel item;
            if (!query_loot_items.getColumn(0).isNull())
                item.itemId = query_loot_items.getColumn(0).getString();
            if (!query_loot_items.getColumn(1).isNull())
                item.quantity = std::max(1, query_loot_items.getColumn(1).getInt());
            if (!query_loot_items.getColumn(2).isNull())
                item.notes = query_loot_items.getColumn(2).getString();
            if (!item.itemId.empty())
                parcel.items.push_back(item);
        }
        campaign.lootParcels.push_back(parcel);
    }
}
