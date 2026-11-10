#pragma once
#include "CharacterSheetViewStorySpellSupport.hpp"

namespace
{
std::string itemSourceLabel(const Campaign &campaign, const ItemModel &item)
{
    const bool campaignCustom = std::any_of(campaign.customItems.begin(), campaign.customItems.end(), [&](const ItemModel &customItem) {
        return customItem.metadata.id == item.metadata.id;
    });
    if (campaignCustom)
        return localizedUiLabel("Campaign");
    return campaign.ruleSet ? campaign.ruleSet->getName() : localizedUiLabel("Ruleset");
}

std::string itemAssignmentContext(const CharacterModel &character, const ItemModel &item)
{
    int quantity = 0;
    bool equipped = false;
    for (const InventoryEntryModel &entry : character.inventory.entries)
    {
        if (entry.itemId != item.metadata.id)
            continue;
        quantity += entry.quantity;
        equipped = equipped || entry.equipped;
    }

    if (quantity <= 0)
        return localizedUiLabel("Not carried");
    return localizedUiLabel("Carried") + " ×" + std::to_string(quantity) +
           localizedUiLabel(equipped ? ", equipped" : ", not equipped");
}

std::string itemCombatSummary(const ItemModel &item)
{
    if (isWeaponItem(item))
    {
        std::string summary = item.weaponDamage.empty() ? localizedUiLabel("Weapon: manual damage") : localizedUiLabel("Weapon") + ": " + item.weaponDamage;
        if (!item.weaponDamageType.empty())
            summary += " " + localizedUiLabel(item.weaponDamageType.c_str());
        summary += " | " + localizedUiLabel(item.weaponAbility.c_str()) + " | " + localizedUiLabel("range") + " " + std::to_string(item.weaponRangeFeet) + " " + localizedUiLabel("ft");
        return summary;
    }
    if (isArmorItem(item))
        return localizedUiLabel("Armor") + ": " + localizedUiLabel("AC") + " +" + std::to_string(item.armorClassBonus) + " | " + localizedUiLabel("DR") + " " + std::to_string(item.damageReduction);
    if (item.ammunition)
        return localizedUiLabel("Ammunition: quantity becomes tracked rounds");
    if (item.consumable)
        return localizedUiLabel("Consumable");
    return localizedUiLabel(item.requiresAttunement ? "Requires attunement" : "No combat stats");
}

std::string itemUseSummary(const ItemModel &item)
{
    if (!item.description.empty())
        return item.description;
    if (!item.effects.empty())
        return item.effects.front();
    if (!item.activation.empty())
        return item.activation;
    return localizedUiLabel("No item notes.");
}

std::string itemWeightLabel(double weight)
{
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.2f lb", weight);
    return buffer;
}

bool itemAssignmentSearchMatches(const Campaign &campaign, const ItemModel &item, const std::string &query)
{
    if (containsText(item.name, query) ||
        containsText(getItemCategory(item), query) ||
        containsText(item.itemType, query) ||
        containsText(item.description, query) ||
        containsText(item.weaponDamage, query) ||
        containsText(item.weaponDamageType, query) ||
        containsText(itemSourceLabel(campaign, item), query))
    {
        return true;
    }

    for (const std::string &property : item.weaponProperties)
    {
        if (containsText(property, query))
            return true;
    }
    for (const std::string &effect : item.effects)
    {
        if (containsText(effect, query))
            return true;
    }
    return false;
}
}
