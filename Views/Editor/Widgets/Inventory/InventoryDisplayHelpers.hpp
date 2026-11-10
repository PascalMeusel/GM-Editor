#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"

#include "Models/Inventory/InventoryModels.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "imgui.h"

#include <functional>
#include <cstdio>
#include <string>
#include <vector>

inline std::string inventoryCombatUseText(const ItemModel *item)
{
    if (!item)
        return localizedUiText("Missing item definition");
    if (isArmorItem(*item))
        return "+" + std::to_string(item->armorClassBonus) + " " + localizedUiText("Armor Class") + ", " +
               std::to_string(item->damageReduction) + " " + localizedUiText("Damage Reduction");
    if (isWeaponItem(*item))
        return item->weaponDamage.empty() ? localizedUiText("Weapon attack") : item->weaponDamage;
    if (!item->effects.empty())
        return item->effects.front();
    return "-";
}

inline bool inventoryHasEntriesMatching(const InventoryModel &inventory, bool equipped)
{
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (entry.equipped == equipped)
            return true;
    }
    return false;
}

inline void drawInventoryCombatSummary(const InventoryModel &inventory, const std::vector<ItemModel> &items)
{
    char weightLabel[64];
    std::snprintf(weightLabel, sizeof(weightLabel), "%.2f lb", calculateInventoryWeight(inventory, items));
    drawInlineLabelValue("Carried Weight", weightLabel);
    showHelpTooltip("Total weight of every assigned item, equipped or carried.");
    drawInlineLabelValue(
        "Equipped Armor",
        ("Armor Class +" + std::to_string(calculateEquippedArmorClassBonus(inventory, items)) +
         " | Damage Reduction " + std::to_string(calculateEquippedDamageReduction(inventory, items)))
            .c_str());
    showHelpTooltip("Only equipped armor contributes to these defensive combat values.");

    bool hasEquippedWeapon = false;
    for (const InventoryEntryModel &entry : inventory.entries)
    {
        if (!entry.equipped)
            continue;

        const ItemModel *item = findItemById(items, entry.itemId);
        if (!item || !isWeaponItem(*item))
            continue;

        if (!hasEquippedWeapon)
        {
            drawSectionTitle("Equipped Weapons", ImVec4(0.66f, 0.72f, 0.78f, 1.0f));
            hasEquippedWeapon = true;
        }
        ImGui::BulletText("%s - %s", item->name.c_str(), inventoryCombatUseText(item).c_str());
    }

    if (!hasEquippedWeapon)
        ImGui::TextDisabled(localizedUiLabel("No equipped weapons.").c_str());
}

inline void drawInventoryGroupTable(const char *tableId,
                                    const InventoryModel &inventory,
                                    const std::vector<ItemModel> &items,
                                    bool equipped,
                                    const std::function<void(int)> &onToggleEquipped = {},
                                    const std::function<void(int)> &onRemove = {},
                                    const std::function<void(int, int)> &onSetQuantity = {})
{
    if (!inventoryHasEntriesMatching(inventory, equipped))
    {
        ImGui::TextDisabled(localizedUiLabel(equipped ? "Nothing is currently equipped." : "No carried-only items.").c_str());
        return;
    }

    const bool hasActions = onToggleEquipped || onRemove;
    const int columnCount = hasActions ? 7 : 6;
    if (!ImGui::BeginTable(tableId, columnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Item").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.3f);
    ImGui::TableSetupColumn(localizedUiLabel("Category").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.16f);
    ImGui::TableSetupColumn(localizedUiLabel("Slot").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.12f);
    ImGui::TableSetupColumn(localizedUiLabel("Use in Play").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
    ImGui::TableSetupColumn(localizedUiLabel("Qty").c_str(), ImGuiTableColumnFlags_WidthFixed, onSetQuantity ? 76.0f : 44.0f);
    ImGui::TableSetupColumn(localizedUiLabel("Weight").c_str(), ImGuiTableColumnFlags_WidthFixed, 72.0f);
    if (hasActions)
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 152.0f);
    ImGui::TableHeadersRow();

    for (int entryIndex = 0; entryIndex < static_cast<int>(inventory.entries.size()); ++entryIndex)
    {
        const InventoryEntryModel &entry = inventory.entries[entryIndex];
        if (entry.equipped != equipped)
            continue;

        const ItemModel *item = findItemById(items, entry.itemId);
        const double eachWeight = item ? item->weight : 0.0;

        ImGui::PushID(entryIndex);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        tableTextWrapped(item ? item->name : localizedUiText("<Missing Item>"));
        ImGui::TableSetColumnIndex(1);
        tableTextWrapped(item ? localizedUiText(getItemCategory(*item).c_str()) : "");
        ImGui::TableSetColumnIndex(2);
        tableTextWrapped(entry.slot.empty() ? "-" : localizedUiText(entry.slot.c_str()));
        ImGui::TableSetColumnIndex(3);
        tableTextWrapped(inventoryCombatUseText(item));
        ImGui::TableSetColumnIndex(4);
        if (onSetQuantity)
        {
            int quantity = entry.quantity;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt(localizedUiLabel("##Quantity").c_str(), &quantity, 0, 0))
                onSetQuantity(entryIndex, quantity);
            showHelpTooltip("Edit the carried quantity directly.");
        }
        else
        {
            ImGui::Text(localizedUiLabel("%d").c_str(), entry.quantity);
        }
        ImGui::TableSetColumnIndex(5);
        ImGui::Text(localizedUiLabel("%.2f lb").c_str(), eachWeight * static_cast<double>(entry.quantity));
        if (hasActions)
        {
            ImGui::TableSetColumnIndex(6);
            const std::string equippedAction = localizedUiLabel(equipped ? "Unequip" : "Equip");
            if (onToggleEquipped && ImGui::SmallButton(equippedAction.c_str()))
                onToggleEquipped(entryIndex);
            showHelpTooltip(equipped ? "Move this item back to carried gear." : "Mark this item as worn, held, or otherwise active.");
            if (onToggleEquipped && onRemove)
                ImGui::SameLine();
            if (onRemove && ImGui::SmallButton(localizedUiLabel("Remove").c_str()))
                onRemove(entryIndex);
            if (onRemove)
                showHelpTooltip("Remove this item from this inventory.");
        }
        ImGui::PopID();
    }

    ImGui::EndTable();
}
