#pragma once

#include "Models/Session/PlayerSafeDataFilter.hpp"
#include <optional>

// Contextual GM previews of existing allowlists, never publication authority.
// Resolve IDs before dropping them from the deliberately small public DTOs.
namespace PlayerSafeSelection
{
enum class State { Unavailable, Ambiguous, MapOnly, ChannelHidden, NotSelected, Staged };
struct NpcPreview
{
    State state = State::Unavailable;
    std::optional<PlayerSafeNpcSummary> candidate;
};
struct InventoryPreview
{
    State state = State::Unavailable;
    std::vector<PlayerSafeInventoryItemSummary> rows;
};

inline NpcPreview npc(const Campaign &campaign, const EntityId &id)
{
    NpcPreview result;
    if (id.empty()) return result;
    const NPCModel *selected = nullptr;
    for (const NPCModel &value : campaign.npcs)
        if (value.metadata.id == id)
        {
            if (selected) { result.state = State::Ambiguous; return result; }
            selected = &value;
        }
    if (!selected) return result;
    result.candidate = buildPlayerSafeNpcSummary(*selected);
    const auto &display = campaign.localPlayerDisplay;
    result.state = display.mapOnly ? State::MapOnly : !display.showVisibleNpcs ? State::ChannelHidden :
        playerSafeContainsId(display.visibleNpcIds, id) ? State::Staged : State::NotSelected;
    return result;
}

inline InventoryPreview inventory(const Campaign &campaign, const EntityId &id)
{
    InventoryPreview result;
    if (id.empty()) return result;
    std::vector<ItemModel> items;
    if (campaign.ruleSet) items = campaign.ruleSet->getItems();
    items.insert(items.end(), campaign.customItems.begin(), campaign.customItems.end());
    const auto count = std::count_if(items.begin(), items.end(), [&](const ItemModel &item) {
        return item.metadata.id == id;
    });
    if (count != 1) { result.state = count ? State::Ambiguous : State::Unavailable; return result; }
    const auto &display = campaign.localPlayerDisplay;
    if (display.mapOnly) { result.state = State::MapOnly; return result; }
    if (!display.showParty || !display.showPartyInventory)
    { result.state = State::ChannelHidden; return result; }
    for (const CharacterModel &character : campaign.characters)
    {
        if (!playerSafeContainsId(display.visibleCharacterIds, character.metadata.id)) continue;
        if (std::none_of(character.inventory.entries.begin(), character.inventory.entries.end(),
                [&](const InventoryEntryModel &entry) { return entry.itemId == id && entry.quantity > 0; })) continue;
        if (std::count_if(campaign.characters.begin(), campaign.characters.end(), [&](const CharacterModel &other) {
                return other.metadata.id == character.metadata.id;
            }) != 1)
        { result.rows.clear(); result.state = State::Ambiguous; return result; }
        appendPlayerSafeInventorySummaries(result.rows, character.inventory, items, character.nameBuffer, false, &id);
    }
    appendPlayerSafeInventorySummaries(result.rows, campaign.partyInventory, items, {}, true, &id);
    result.state = result.rows.empty() ? State::NotSelected : State::Staged;
    return result;
}
}
