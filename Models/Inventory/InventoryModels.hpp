#pragma once

#include "Models/Combat/EncounterModel.hpp"
#include "Models/Core/EntityMetadata.hpp"
#include <algorithm>
#include <string>
#include <vector>

enum class ItemRarity
{
    Common,
    Uncommon,
    Rare,
    VeryRare,
    Legendary,
    Artifact,
    Custom
};

struct ItemModel
{
    EntityMetadata metadata{"item"};
    std::string name;
    std::string description;
    std::string rulesetKey;
    std::string category;
    std::string itemType;
    ItemRarity rarity = ItemRarity::Common;
    double weight = 0.0;
    int baseValue = 0; // Stored as copper pieces.
    bool stackable = false;
    int armorClassBonus = 0;
    int damageReduction = 0;
    int weaponAttackBonus = 0;
    std::string weaponDamage;
    std::string weaponDamageType;
    std::string weaponAbility = "Strength";
    int weaponReachFeet = 5;
    int weaponRangeFeet = 5;
    int weaponLongRangeFeet = 5;
    std::string weaponAttackKind;
    std::string weaponMastery;
    std::string weaponVersatileDamage;
    std::string ammunitionType;
    std::vector<std::string> weaponProperties;
    bool container = false;
    double containerCapacityWeight = 0.0;
    int containerCapacityItems = 0;
    bool requiresAttunement = false;
    int maxCharges = 0;
    std::string rechargeRule;
    bool cursed = false;
    std::string activation;
    EntityId linkedSpellId;
    std::string commandWords;
    bool consumable = false;
    bool ammunition = false;
    int maxDurability = 0;
    std::vector<std::string> effects;
    std::vector<MonsterActionModel> powers;
};

inline std::string getItemCategory(const ItemModel &item)
{
    if (!item.category.empty())
        return item.category;
    if (!item.itemType.empty())
        return item.itemType;
    return "Uncategorized";
}

struct InventoryEntryModel
{
    EntityMetadata metadata{"inventory_entry"};
    EntityId itemId;
    EntityId containerEntryId;
    int quantity = 1;
    bool equipped = false;
    std::string slot;
    bool attuned = false;
    int charges = -1;
    int ammunitionRemaining = 0;
    int durability = -1;
    std::string stateNotes;

    InventoryEntryModel() = default;

    InventoryEntryModel(const EntityId &itemIdValue, int quantityValue = 1, bool equippedValue = false, const std::string &slotValue = "")
        : itemId(itemIdValue), quantity(quantityValue), equipped(equippedValue), slot(slotValue)
    {
    }
};

struct InventoryModel
{
    EntityMetadata metadata{"inventory"};
    std::string name;
    EntityId ownerEntityId;
    std::vector<InventoryEntryModel> entries;
    int copper = 0;
    int silver = 0;
    int gold = 0;
    int platinum = 0;
};

struct LootParcelItemModel
{
    EntityId itemId;
    int quantity = 1;
    std::string notes;
};

struct LootParcelModel
{
    EntityMetadata metadata{"loot_parcel"};
    std::string name = "Loot Parcel";
    std::string source;
    std::string notes;
    bool awarded = false;
    std::string awardedTo;
    int copper = 0;
    int silver = 0;
    int gold = 0;
    int platinum = 0;
    std::vector<LootParcelItemModel> items;
};

inline const ItemModel *findItemById(const std::vector<ItemModel> &items, const EntityId &itemId)
{
    for (const auto &item : items)
    {
        if (item.metadata.id == itemId)
            return &item;
    }
    return nullptr;
}

inline double calculateInventoryWeight(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    double totalWeight = 0.0;
    for (const auto &entry : inventory.entries)
    {
        const ItemModel *item = findItemById(items, entry.itemId);
        if (item)
            totalWeight += item->weight * static_cast<double>(entry.quantity);
    }
    return totalWeight;
}

inline bool isArmorItem(const ItemModel &item)
{
    return getItemCategory(item) == "Armor" || item.itemType == "Armor";
}

inline bool isWeaponItem(const ItemModel &item)
{
    return getItemCategory(item) == "Weapon" || item.itemType == "Weapon";
}

inline bool isItemArchived(const ItemModel &item)
{
    return std::find(item.metadata.tags.begin(), item.metadata.tags.end(), "archived") != item.metadata.tags.end();
}

inline const InventoryEntryModel *findInventoryEntryById(const InventoryModel &inventory, const EntityId &entryId)
{
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (entry.metadata.id == entryId)
            return &entry;
    }
    return nullptr;
}

inline double calculateContainedWeight(const InventoryModel &inventory, const std::vector<ItemModel> &items, const EntityId &containerEntryId)
{
    double totalWeight = 0.0;
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (entry.containerEntryId != containerEntryId)
            continue;
        const ItemModel *item = findItemById(items, entry.itemId);
        if (item)
            totalWeight += item->weight * static_cast<double>(entry.quantity);
    }
    return totalWeight;
}

inline int calculateEquippedArmorClassBonus(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    int totalBonus = 0;
    for (const auto &entry : inventory.entries)
    {
        if (!entry.equipped)
            continue;

        const ItemModel *item = findItemById(items, entry.itemId);
        if (item && isArmorItem(*item))
            totalBonus += item->armorClassBonus;
    }
    return totalBonus;
}

inline int calculateEquippedDamageReduction(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    int totalReduction = 0;
    for (const auto &entry : inventory.entries)
    {
        if (!entry.equipped)
            continue;

        const ItemModel *item = findItemById(items, entry.itemId);
        if (item && isArmorItem(*item))
            totalReduction += item->damageReduction;
    }
    return totalReduction;
}
