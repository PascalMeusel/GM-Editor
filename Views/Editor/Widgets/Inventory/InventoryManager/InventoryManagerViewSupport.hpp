#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "InventoryManagerView.hpp"
#include "Controllers/Editor/Widgets/InventoryManager/InventoryManagerController.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Models/Project/ProjectSaveService.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
template <typename... Args>
std::string formatLocalizedUi(const char *englishFormat, Args... args)
{
    const std::string localizedFormat = localizedUiLabel(englishFormat);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), args...);
    if (required <= 0)
        return localizedFormat;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1U, '\0');
    std::snprintf(buffer.data(), buffer.size(), localizedFormat.c_str(), args...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

std::string displayNameOrFallback(const std::string &value, const char *fallback)
{
    return value.empty() ? std::string(localizedUiText(fallback)) : value;
}

std::string formatCoinBreakdown(const InventoryModel &inventory)
{
    return std::to_string(inventory.platinum) + " pp, " +
           std::to_string(inventory.gold) + " gp, " +
           std::to_string(inventory.silver) + " sp, " +
           std::to_string(inventory.copper) + " cp";
}

bool sameCoinBreakdown(const InventoryModel &left, const InventoryModel &right)
{
    return left.copper == right.copper &&
           left.silver == right.silver &&
           left.gold == right.gold &&
           left.platinum == right.platinum;
}

int defaultCurrencyBaseUnitValue(const std::string &code)
{
    if (code == "cp")
        return 1;
    if (code == "sp")
        return 10;
    if (code == "gp")
        return 100;
    if (code == "pp")
        return 1000;
    return 0;
}

int currencyBaseUnitValue(const IRuleSet *ruleset, const std::string &code)
{
    if (!ruleset)
        return defaultCurrencyBaseUnitValue(code);

    for (const RuleCurrencyDefinition &currency : ruleset->getCurrencies())
    {
        if (currency.code == code)
            return currency.baseUnitValue;
    }
    return 0;
}

int inventoryCurrencyTotalValue(const InventoryModel &inventory, const IRuleSet *ruleset)
{
    if (ruleset)
        return ruleset->getInventoryCurrencyValue(inventory);

    return inventory.copper * defaultCurrencyBaseUnitValue("cp") +
           inventory.silver * defaultCurrencyBaseUnitValue("sp") +
           inventory.gold * defaultCurrencyBaseUnitValue("gp") +
           inventory.platinum * defaultCurrencyBaseUnitValue("pp");
}

std::string formatCurrencyTotalValue(const IRuleSet *ruleset, int baseUnitValue)
{
    const int normalizedValue = std::max(0, baseUnitValue);
    if (!ruleset)
        return std::to_string(normalizedValue) + " cp";

    const std::string formatted = ruleset->formatCurrencyValue(normalizedValue);
    const std::string baseValue = std::to_string(normalizedValue) + " cp";
    if (formatted == baseValue || formatted == std::to_string(normalizedValue))
        return formatted;
    return formatted + " (" + baseValue + ")";
}

std::string formatCurrencyUnitValue(int baseUnitValue)
{
    if (baseUnitValue <= 0)
        return localizedUiText("Not in ruleset");
    return std::to_string(baseUnitValue) + " cp";
}

std::string formatCurrencySubtotal(const IRuleSet *ruleset, const std::string &code, int amount)
{
    const int unitValue = currencyBaseUnitValue(ruleset, code);
    if (unitValue <= 0)
        return localizedUiText("Not counted");

    const int normalizedAmount = std::max(0, amount);
    return std::to_string(normalizedAmount) + " " + code + " = " +
           formatCurrencyTotalValue(ruleset, normalizedAmount * unitValue);
}

bool lootParcelHasContents(const LootParcelModel &parcel)
{
    return parcel.copper > 0 ||
           parcel.silver > 0 ||
           parcel.gold > 0 ||
           parcel.platinum > 0 ||
           !parcel.items.empty();
}

int lootParcelTotalItemQuantity(const LootParcelModel &parcel)
{
    int quantity = 0;
    for (const LootParcelItemModel &item : parcel.items)
        quantity += std::max(1, item.quantity);
    return quantity;
}

void appendSummaryPart(std::string &summary, const std::string &part)
{
    if (part.empty())
        return;
    if (!summary.empty())
        summary += ", ";
    summary += part;
}

std::string lootParcelSummary(const LootParcelModel &parcel, const IRuleSet *ruleset)
{
    std::string summary;
    InventoryModel coins;
    coins.copper = parcel.copper;
    coins.silver = parcel.silver;
    coins.gold = parcel.gold;
    coins.platinum = parcel.platinum;

    if (ruleset)
    {
        const int totalCoinValue = ruleset->getInventoryCurrencyValue(coins);
        if (totalCoinValue > 0)
            appendSummaryPart(summary, ruleset->formatCurrencyValue(totalCoinValue));
    }
    else if (parcel.copper > 0 || parcel.silver > 0 || parcel.gold > 0 || parcel.platinum > 0)
    {
        appendSummaryPart(summary, formatCoinBreakdown(coins));
    }

    if (!parcel.items.empty())
    {
        const int itemQuantity = lootParcelTotalItemQuantity(parcel);
        if (itemQuantity == 1 && parcel.items.size() == 1)
            appendSummaryPart(summary, formatLocalizedUi("%d item across %d entry", itemQuantity, static_cast<int>(parcel.items.size())));
        else if (itemQuantity == 1)
            appendSummaryPart(summary, formatLocalizedUi("%d item across %d entries", itemQuantity, static_cast<int>(parcel.items.size())));
        else if (parcel.items.size() == 1)
            appendSummaryPart(summary, formatLocalizedUi("%d items across %d entry", itemQuantity, static_cast<int>(parcel.items.size())));
        else
            appendSummaryPart(summary, formatLocalizedUi("%d items across %d entries", itemQuantity, static_cast<int>(parcel.items.size())));
    }

    return summary.empty() ? std::string(localizedUiText("empty parcel")) : summary;
}

std::string inventoryEntryItemName(const ItemModel *item)
{
    return item ? displayNameOrFallback(item->name, "Unnamed Item") : std::string("<Missing Item>");
}

std::string lootParcelItemsPreview(const LootParcelModel &parcel, const std::vector<ItemModel> &items)
{
    if (parcel.items.empty())
        return localizedUiText("no items");

    std::string preview;
    constexpr int maxPreviewEntries = 3;
    for (int i = 0; i < static_cast<int>(parcel.items.size()); ++i)
    {
        if (i >= maxPreviewEntries)
        {
            appendSummaryPart(preview, formatLocalizedUi("+%d more", static_cast<int>(parcel.items.size()) - i));
            break;
        }

        const LootParcelItemModel &parcelItem = parcel.items[static_cast<std::size_t>(i)];
        const ItemModel *item = findItemById(items, parcelItem.itemId);
        appendSummaryPart(preview, std::to_string(std::max(1, parcelItem.quantity)) + "x " + inventoryEntryItemName(item));
    }
    return preview;
}

std::string lootParcelAwardPreview(const LootParcelModel &parcel, const IRuleSet *ruleset, const std::vector<ItemModel> &items)
{
    std::string preview = lootParcelSummary(parcel, ruleset);
    const std::string itemPreview = lootParcelItemsPreview(parcel, items);
    if (!parcel.items.empty())
        preview += " | " + itemPreview;
    return preview;
}

std::string lootParcelDestinationLabel(const LootParcelModel &parcel)
{
    return parcel.awarded
               ? formatLocalizedUi("Awarded to %s", displayNameOrFallback(parcel.awardedTo, "Party Inventory").c_str())
               : std::string(localizedUiText("Party Inventory"));
}

std::string lootParcelDestinationSummary(const LootParcelModel &parcel)
{
    if (parcel.awarded)
        return lootParcelDestinationLabel(parcel) + "\n" + localizedUiText("Locked after award");
    return std::string(localizedUiText("Party Inventory")) + "\n" + localizedUiText("Pending award");
}

std::string lootParcelAddItemDisabledReason(const LootParcelModel &parcel, bool hasSelectableItem)
{
    if (parcel.awarded)
        return localizedUiText("This parcel has already been awarded; create a new parcel for more rewards.");
    if (!hasSelectableItem)
        return localizedUiText("Select an item before adding it to a parcel.");
    return "";
}

std::string lootParcelAwardDisabledReason(const LootParcelModel &parcel)
{
    if (parcel.awarded)
        return localizedUiText("This parcel was already awarded.");
    if (!lootParcelHasContents(parcel))
        return localizedUiText("Add coins or items before awarding this parcel.");
    return "";
}

std::string lootParcelActionDisabledSummary(const LootParcelModel &parcel,
                                            const std::string &addItemReason,
                                            const std::string &awardReason)
{
    if (parcel.awarded)
        return formatLocalizedUi("Locked: %s.", lootParcelDestinationLabel(parcel).c_str());

    std::string summary;
    if (!addItemReason.empty())
        appendSummaryPart(summary, formatLocalizedUi("Add Item: %s", addItemReason.c_str()));
    if (!awardReason.empty())
        appendSummaryPart(summary, formatLocalizedUi("Award: %s", awardReason.c_str()));
    return summary;
}

std::string inventoryEntryCategory(const ItemModel *item)
{
    return item ? std::string(localizedUiText(getItemCategory(*item).c_str())) : std::string(localizedUiText("Missing"));
}

std::string inventoryEntryCombatUse(const ItemModel *item)
{
    if (!item)
        return localizedUiText("Missing item");
    if (isArmorItem(*item))
        return formatLocalizedUi("AC +%d / DR %d", item->armorClassBonus, item->damageReduction);
    if (isWeaponItem(*item))
        return item->weaponDamage.empty() ? std::string(localizedUiText("Weapon")) : item->weaponDamage;
    if (item->requiresAttunement)
        return localizedUiText("Attunement");
    if (item->consumable)
        return localizedUiText("Consumable");
    if (item->container)
        return localizedUiText("Container");
    return localizedUiText("Utility");
}

std::string inventoryEntryContainerLabel(const InventoryModel &inventory, const std::vector<ItemModel> &items, const InventoryEntryModel &entry)
{
    if (entry.containerEntryId.empty())
        return localizedUiText("None");

    const InventoryEntryModel *containerEntry = findInventoryEntryById(inventory, entry.containerEntryId);
    const ItemModel *containerItem = containerEntry ? findItemById(items, containerEntry->itemId) : nullptr;
    return containerItem ? inventoryEntryItemName(containerItem) : std::string(localizedUiText("<Missing Container>"));
}

std::string inventoryEntryStateSummary(const InventoryEntryModel &entry, const ItemModel *item)
{
    std::string summary = localizedUiText(entry.equipped ? "Equipped" : "Carried");
    if (!entry.slot.empty())
        appendSummaryPart(summary, formatLocalizedUi("Slot %s", entry.slot.c_str()));
    if (item && item->requiresAttunement)
        appendSummaryPart(summary, localizedUiText(entry.attuned ? "Attuned" : "Not attuned"));
    if (item && item->maxCharges > 0)
        appendSummaryPart(summary, formatLocalizedUi("Charges %d/%d", entry.charges < 0 ? item->maxCharges : entry.charges, item->maxCharges));
    if (item && item->ammunition)
        appendSummaryPart(summary, formatLocalizedUi("Ammo %d", entry.ammunitionRemaining));
    if (item && item->maxDurability > 0)
        appendSummaryPart(summary, formatLocalizedUi("Durability %d/%d", entry.durability < 0 ? item->maxDurability : entry.durability, item->maxDurability));
    return summary;
}

std::string inventoryEntryTransferDisabledReason(const InventoryModel *source,
                                                 const InventoryModel *target,
                                                 const InventoryEntryModel &entry)
{
    if (!target)
        return localizedUiText("Choose a valid transfer target first.");
    if (target == source)
        return localizedUiText("Choose a transfer target that is different from the inventory owner.");
    if (entry.quantity <= 0)
        return localizedUiText("This row has no quantity to move.");
    return "";
}

void drawWrappedDisabledText(const std::string &text)
{
    if (text.empty())
        return;

    const float wrapWidth = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::PushTextWrapPos(wrapWidth);
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), text.c_str());
    ImGui::PopTextWrapPos();
}
}
