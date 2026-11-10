#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "ItemCreatorView.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/ImageTextureCache.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include <algorithm>
#include <cctype>
#include <vector>

namespace
{
bool hasProperty(const ItemModel &item, const std::string &property)
{
    return std::find(item.weaponProperties.begin(), item.weaponProperties.end(), property) != item.weaponProperties.end();
}

void setProperty(ItemModel &item, const std::string &property, bool enabled)
{
    const auto found = std::find(item.weaponProperties.begin(), item.weaponProperties.end(), property);
    if (enabled && found == item.weaponProperties.end())
        item.weaponProperties.push_back(property);
    else if (!enabled && found != item.weaponProperties.end())
        item.weaponProperties.erase(found);
}

void propertyCheckbox(ItemModel &item, const char *label)
{
    bool enabled = hasProperty(item, label);
    if (FormUi::Checkbox(localizedUiLabel(label).c_str(), &enabled))
        setProperty(item, label, enabled);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsCaseInsensitive(const std::string &value, const char *needle)
{
    if (!needle || needle[0] == '\0')
        return false;
    return lowerCopy(value).find(lowerCopy(needle)) != std::string::npos;
}

bool isMagicItemCandidate(const ItemModel &item)
{
    return containsCaseInsensitive(getItemCategory(item), "magic") ||
           containsCaseInsensitive(item.itemType, "magic") ||
           containsCaseInsensitive(item.name, "magic");
}

bool hasMagicItemState(const ItemModel &item)
{
    return item.requiresAttunement ||
           item.maxCharges > 0 ||
           item.cursed ||
           !item.activation.empty() ||
           !item.linkedSpellId.empty() ||
           !item.commandWords.empty();
}

bool hasInventoryState(const ItemModel &item)
{
    return item.container ||
           item.consumable ||
           item.ammunition ||
           item.maxDurability > 0;
}

bool hasCombatMetadata(const ItemModel &item)
{
    return isArmorItem(item) ||
           isWeaponItem(item) ||
           !item.powers.empty();
}

const char *rarityLabel(ItemRarity rarity)
{
    switch (rarity)
    {
    case ItemRarity::Common:
        return "Common";
    case ItemRarity::Uncommon:
        return "Uncommon";
    case ItemRarity::Rare:
        return "Rare";
    case ItemRarity::VeryRare:
        return "Very Rare";
    case ItemRarity::Legendary:
        return "Legendary";
    case ItemRarity::Artifact:
        return "Artifact";
    case ItemRarity::Custom:
        return "Custom";
    }
    return "Common";
}

std::string formatCopperValue(int copper)
{
    if (copper >= 1000 && copper % 1000 == 0)
        return std::to_string(copper / 1000) + " pp";
    if (copper >= 100 && copper % 100 == 0)
        return std::to_string(copper / 100) + " gp";
    if (copper >= 10 && copper % 10 == 0)
        return std::to_string(copper / 10) + " sp";
    return std::to_string(copper) + " cp";
}

void drawCatalogItemDetails(const ItemModel &item, const std::string &source)
{
    ImGui::TextDisabled(localizedUiLabel("Source: %s").c_str(), source.c_str());
    ImGui::Separator();
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), item.name.empty() ? localizedUiText("Unnamed item") : item.name.c_str());
    const std::string localizedCategory = localizedUiLabel(getItemCategory(item).c_str());
    const std::string localizedType = localizedUiLabel(item.itemType.c_str());
    const std::string localizedRarity = localizedUiLabel(rarityLabel(item.rarity));
    ImGui::TextDisabled(localizedUiLabel("Category: %s | Type: %s | Rarity: %s").c_str(), localizedCategory.c_str(), localizedType.c_str(), localizedRarity.c_str());
    const std::string stackability = localizedUiLabel(item.stackable ? "Stackable" : "Single item");
    ImGui::TextDisabled(localizedUiLabel("Value: %s | Weight: %.2f lb | %s").c_str(),
                        formatCopperValue(item.baseValue).c_str(),
                        item.weight,
                        stackability.c_str());

    if (hasCombatMetadata(item) && beginAdvancedTreeNode("Combat Metadata", "Armor, weapon, and item action details for this catalog entry."))
    {
        if (isArmorItem(item))
            ImGui::TextDisabled(localizedUiLabel("Armor: AC +%d | Damage Reduction %d").c_str(), item.armorClassBonus, item.damageReduction);
        if (isWeaponItem(item))
        {
            ImGui::TextDisabled(localizedUiLabel("Weapon: %s | %s | Reach %d ft | Range %d/%d ft").c_str(),
                                item.weaponDamage.empty() ? "manual damage" : item.weaponDamage.c_str(),
                                item.weaponAbility.empty() ? "Strength" : item.weaponAbility.c_str(),
                                item.weaponReachFeet,
                                item.weaponRangeFeet,
                                item.weaponLongRangeFeet);
            if (!item.weaponMastery.empty() || !item.weaponVersatileDamage.empty() || !item.ammunitionType.empty())
                ImGui::TextDisabled(localizedUiLabel("Mastery: %s | Versatile: %s | Ammunition: %s").c_str(),
                                    item.weaponMastery.empty() ? "-" : item.weaponMastery.c_str(),
                                    item.weaponVersatileDamage.empty() ? "-" : item.weaponVersatileDamage.c_str(),
                                    item.ammunitionType.empty() ? "-" : item.ammunitionType.c_str());
            if (!item.weaponProperties.empty() && beginAdvancedTreeNode("Weapon Properties", "Weapon rules keywords are hidden until you need to inspect combat behavior."))
            {
                std::string properties;
                for (std::size_t i = 0; i < item.weaponProperties.size(); ++i)
                {
                    if (i > 0)
                        properties += ", ";
                    properties += localizedUiLabel(item.weaponProperties[i].c_str());
                }
                ImGui::TextDisabled(localizedUiLabel("%s").c_str(), properties.c_str());
                ImGui::TreePop();
            }
        }
        if (!item.powers.empty())
            ImGui::TextDisabled(localizedUiLabel("Item powers: %d").c_str(), static_cast<int>(item.powers.size()));
        ImGui::TreePop();
    }
    if ((hasInventoryState(item) || hasMagicItemState(item)) && beginAdvancedTreeNode("Item State", "Container, durability, charge, attunement, and curse state."))
    {
        if (item.container)
            ImGui::TextDisabled(localizedUiLabel("Container: %.2f lb capacity | %d item slots").c_str(), item.containerCapacityWeight, item.containerCapacityItems);
        if (item.consumable || item.ammunition)
        {
            std::string state;
            if (item.consumable)
                state = localizedUiLabel("Consumable");
            if (item.ammunition)
                state += (state.empty() ? "" : " · ") + localizedUiLabel("Ammunition");
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), state.c_str());
        }
        if (item.maxDurability > 0)
            ImGui::TextDisabled(localizedUiLabel("Durability: %d max").c_str(), item.maxDurability);
        if (item.requiresAttunement)
            ImGui::TextDisabled(localizedUiLabel("Requires attunement").c_str());
        if (item.maxCharges > 0)
            ImGui::TextDisabled(localizedUiLabel("Charges: %d max | Recharge: %s").c_str(), item.maxCharges, item.rechargeRule.empty() ? "-" : item.rechargeRule.c_str());
        if (item.cursed)
            ImGui::TextDisabled(localizedUiLabel("Cursed").c_str());
        if (!item.activation.empty())
            ImGui::TextDisabled(localizedUiLabel("Activation: %s").c_str(), item.activation.c_str());
        if (!item.linkedSpellId.empty())
            ImGui::TextDisabled(localizedUiLabel("Linked spell: %s").c_str(), item.linkedSpellId.c_str());
        if (!item.commandWords.empty())
            ImGui::TextDisabled(localizedUiLabel("Command words: %s").c_str(), item.commandWords.c_str());
        ImGui::TreePop();
    }
    if (!item.powers.empty() && beginAdvancedTreeNode("Item Powers", "Detailed item powers stay collapsed in the catalog preview."))
    {
        for (const MonsterActionModel &power : item.powers)
        {
        {
            const std::string powerName = power.name.empty() ? localizedUiLabel("<Unnamed Power>") : power.name;
            const std::string powerKind = power.actionKind.empty() ? localizedUiLabel("Item Power") : localizedUiLabel(power.actionKind.c_str());
            ImGui::BulletText("%s (%s)", powerName.c_str(), powerKind.c_str());
        }
            if (!power.damage.empty() || power.rangeFeet > 0 || power.reachFeet > 0)
                ImGui::TextDisabled(localizedUiLabel("Damage: %s | Range: %d ft").c_str(), power.damage.empty() ? "-" : power.damage.c_str(), std::max(power.reachFeet, power.rangeFeet));
        }
        ImGui::TreePop();
    }
    if (!item.description.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Description"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), item.description.c_str());
    }
}
}
