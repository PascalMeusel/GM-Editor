#pragma once
#include "CharacterSheetViewItemSupport.hpp"

namespace
{
std::string compactText(std::string value, std::size_t limit)
{
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    const std::size_t first = value.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    const std::size_t last = value.find_last_not_of(" \t");
    value = value.substr(first, last - first + 1);
    if (value.size() <= limit)
        return value;
    if (limit <= 3)
        return value.substr(0, limit);
    return value.substr(0, limit - 3) + "...";
}

std::string joinLimitedList(const std::vector<std::string> &entries, const std::string &emptyLabel, std::size_t limit = 3)
{
    std::string label;
    std::size_t shown = 0;
    for (const std::string &entry : entries)
    {
        const std::string compact = compactText(entry, 48);
        if (compact.empty())
            continue;
        if (!label.empty())
            label += ", ";
        label += localizedUiLabel(compact.c_str());
        ++shown;
        if (shown >= limit)
            break;
    }

    if (label.empty())
        return localizedUiLabel(emptyLabel.c_str());

    if (entries.size() > shown)
        label += ", +" + std::to_string(entries.size() - shown) + " " + localizedUiLabel("more");
    return label;
}

std::string characterPassivesAndSavesLabel(const CharacterModel &character, int passivePerception)
{
    std::string label = localizedUiLabel("Passive Perception") + " " + std::to_string(passivePerception);
    if (!character.savingThrowProficiencies.empty())
        label += " | " + localizedUiLabel("Saves") + ": " + joinLimitedList(character.savingThrowProficiencies, "None", 4);
    return label;
}

std::string characterResourceSummary(const CharacterModel &character)
{
    if (character.resourceTrackers.empty())
        return localizedUiLabel("No tracked resources");

    std::string label;
    const std::size_t limit = 4;
    std::size_t shown = 0;
    for (ResourceTrackerModel tracker : character.resourceTrackers)
    {
        normalizeResourceTracker(tracker);
        if (!label.empty())
            label += ", ";
        label += (tracker.name.empty() ? localizedUiLabel("Resource") : tracker.name) + " " +
                 std::to_string(std::max(0, tracker.maximum - tracker.used)) + "/" +
                 std::to_string(tracker.maximum);
        ++shown;
        if (shown >= limit)
            break;
    }

    if (character.resourceTrackers.size() > shown)
        label += ", +" + std::to_string(character.resourceTrackers.size() - shown) + " " + localizedUiLabel("more");
    return label;
}

std::string characterEquippedGearSummary(const CharacterModel &character, const std::vector<ItemModel> &items)
{
    std::string label;
    std::size_t shown = 0;
    std::size_t equippedCount = 0;
    for (const InventoryEntryModel &entry : character.inventory.entries)
    {
        if (!entry.equipped)
            continue;
        ++equippedCount;
        if (shown >= 4)
            continue;

        const ItemModel *item = findItemById(items, entry.itemId);
        if (!label.empty())
            label += "; ";
        label += item ? item->name : localizedUiLabel("<Missing Item>");
        if (entry.quantity > 1)
            label += " x" + std::to_string(entry.quantity);
        if (item)
            label += " – " + localizedUiLabel(inventoryCombatUseText(item).c_str());
        ++shown;
    }

    if (label.empty())
        return localizedUiLabel("No equipped combat gear");
    if (equippedCount > shown)
        label += "; +" + std::to_string(equippedCount - shown) + " " + localizedUiLabel("more");
    return label;
}

std::string characterMagicLoadoutSummary(const Campaign &campaign, const CharacterModel &character)
{
    std::string label = characterSpellSlotsLabel(character.spellcasting);
    if (!character.spellcasting.enabled)
        return label;

    int preparedCount = 0;
    for (const CharacterSpellEntryModel &entry : character.spellcasting.spells)
    {
        if (entry.prepared)
            ++preparedCount;
    }
    label += " | " + std::to_string(static_cast<int>(character.spellcasting.spells.size())) + " " + localizedUiLabel("known");
    if (!character.spellcasting.spells.empty())
        label += ", " + std::to_string(preparedCount) + " " + localizedUiLabel("prepared");

    const std::vector<SpellModel> availableSpells = getAvailableSpells(campaign);
    std::string preparedNames;
    std::size_t shown = 0;
    for (const CharacterSpellEntryModel &entry : character.spellcasting.spells)
    {
        if (!entry.prepared || shown >= 3)
            continue;
        const SpellModel *spell = findSpellById(availableSpells, entry.spellId);
        if (!spell)
            continue;
        if (!preparedNames.empty())
            preparedNames += ", ";
        preparedNames += spell->name;
        ++shown;
    }
    if (!preparedNames.empty())
        label += " | " + localizedUiLabel("Ready") + ": " + preparedNames;
    return label;
}

std::string characterKeyNotesSummary(const CharacterModel &character)
{
    const std::string playerNotes = compactText(character.playerNotes, 90);
    if (!playerNotes.empty())
        return localizedUiLabel("Player") + ": " + playerNotes;

    const std::string gmNotes = compactText(character.gmNotes, 90);
    if (!gmNotes.empty())
        return localizedUiLabel("GM") + ": " + gmNotes;

    const std::string backstory = compactText(character.backstory, 90);
    if (!backstory.empty())
        return localizedUiLabel("Backstory") + ": " + backstory;

    return localizedUiLabel("No key notes");
}

void renderCombo(const char *label, const std::vector<std::string> &items, std::string &currentItem)
{
    if (items.empty())
    {
        const std::string localizedLabel = localizedUiLabel(label);
        ImGui::TextDisabled(localizedUiLabel("%s: no options available").c_str(), localizedLabel.c_str());
        return;
    }

    const std::string localizedLabel = localizedUiLabel(label);
    const std::string localizedCurrent = currentItem.empty() ? localizedUiLabel("<Unset>") : localizedUiLabel(currentItem.c_str());
    if (FormUi::BeginCombo(localizedLabel.c_str(), localizedCurrent.c_str()))
    {
        for (const std::string &item : items)
        {
            const bool selected = currentItem == item;
            const std::string localizedItem = localizedUiLabel(item.c_str());
            if (ImGui::Selectable(localizedItem.c_str(), selected))
                currentItem = item;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}
}
