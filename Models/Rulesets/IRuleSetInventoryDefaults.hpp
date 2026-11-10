#pragma once



#include "Models/Rulesets/IRuleSet.hpp"



inline int IRuleSet::getInventoryCurrencyValue(const InventoryModel& inventory) const
    {
        int total = 0;
        for (const RuleCurrencyDefinition& currency : getCurrencies())
        {
            if (currency.code == "cp")
                total += inventory.copper * currency.baseUnitValue;
            else if (currency.code == "sp")
                total += inventory.silver * currency.baseUnitValue;
            else if (currency.code == "gp")
                total += inventory.gold * currency.baseUnitValue;
            else if (currency.code == "pp")
                total += inventory.platinum * currency.baseUnitValue;
        }
        return total;
    }

inline std::string IRuleSet::formatCurrencyValue(int baseUnitValue) const
    {
        std::vector<RuleCurrencyDefinition> currencies = getCurrencies();
        std::sort(currencies.begin(), currencies.end(), [](const RuleCurrencyDefinition& left, const RuleCurrencyDefinition& right) {
            return left.baseUnitValue > right.baseUnitValue;
        });

        std::ostringstream stream;
        int remaining = std::max(0, baseUnitValue);
        bool wroteAny = false;
        for (const RuleCurrencyDefinition& currency : currencies)
        {
            if (currency.baseUnitValue <= 0)
                continue;
            const int amount = remaining / currency.baseUnitValue;
            remaining %= currency.baseUnitValue;
            if (amount <= 0)
                continue;
            if (wroteAny)
                stream << " ";
            stream << amount << " " << currency.code;
            wroteAny = true;
        }
        return wroteAny ? stream.str() : "0";
    }

inline void IRuleSet::normalizeInventoryCurrency(InventoryModel& inventory) const
    {
        int remaining = getInventoryCurrencyValue(inventory);
        inventory.copper = 0;
        inventory.silver = 0;
        inventory.gold = 0;
        inventory.platinum = 0;

        std::vector<RuleCurrencyDefinition> currencies = getCurrencies();
        std::sort(currencies.begin(), currencies.end(), [](const RuleCurrencyDefinition& left, const RuleCurrencyDefinition& right) {
            return left.baseUnitValue > right.baseUnitValue;
        });
        for (const RuleCurrencyDefinition& currency : currencies)
        {
            if (currency.baseUnitValue <= 0)
                continue;
            const int amount = remaining / currency.baseUnitValue;
            remaining %= currency.baseUnitValue;
            if (currency.code == "cp")
                inventory.copper = amount;
            else if (currency.code == "sp")
                inventory.silver = amount;
            else if (currency.code == "gp")
                inventory.gold = amount;
            else if (currency.code == "pp")
                inventory.platinum = amount;
        }
    }

inline RuleCarryingCapacitySummary IRuleSet::evaluateCarryingCapacity(const std::map<std::string, int>& attributes, const InventoryModel& inventory, const std::vector<ItemModel>& items) const
    {
        RuleCarryingCapacitySummary summary;
        summary.carriedWeight = calculateInventoryWeight(inventory, items);
        const auto strength = attributes.find("Strength");
        if (strength == attributes.end())
            return summary;

        summary.capacityKnown = true;
        summary.capacityWeight = std::max(0, strength->second) * 15.0;
        summary.encumbered = summary.capacityWeight > 0.0 && summary.carriedWeight > summary.capacityWeight * 0.66;
        summary.overCapacity = summary.capacityWeight > 0.0 && summary.carriedWeight > summary.capacityWeight;
        if (summary.overCapacity)
            summary.warnings.push_back("Carried weight exceeds carrying capacity.");
        else if (summary.encumbered)
            summary.warnings.push_back("Carried weight is approaching carrying capacity.");
        return summary;
    }

inline RuleEquipmentSlotValidation IRuleSet::validateEquipmentSlots(const InventoryModel& inventory, const std::vector<ItemModel>& items) const
    {
        RuleEquipmentSlotValidation validation;
        std::map<std::string, int> equippedBySlot;
        std::map<std::string, int> slotLimits;
        for (const RuleEquipmentSlotDefinition& slot : getEquipmentSlots())
            slotLimits[slot.label] = std::max(1, slot.maxEquipped);

        for (const InventoryEntryModel& entry : inventory.entries)
        {
            if (!entry.equipped)
                continue;
            const ItemModel* item = findItemById(items, entry.itemId);
            const std::string slot = entry.slot.empty() ? "Unslotted" : entry.slot;
            ++equippedBySlot[slot];
            if (entry.slot.empty() && item && (isWeaponItem(*item) || isArmorItem(*item)))
                validation.warnings.push_back((item->name.empty() ? "Equipped item" : item->name) + " is equipped without a slot.");
        }

        for (const auto& [slot, count] : equippedBySlot)
        {
            const auto limit = slotLimits.find(slot);
            if (limit != slotLimits.end() && count > limit->second)
                validation.warnings.push_back(slot + " has " + std::to_string(count) + " equipped items; ruleset limit is " + std::to_string(limit->second) + ".");
        }
        validation.valid = validation.warnings.empty();
        return validation;
    }
