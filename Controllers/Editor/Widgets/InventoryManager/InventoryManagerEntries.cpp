#include "InventoryManagerController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include <algorithm>
#include <cctype>

namespace
{
std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsText(std::string value, std::string needle)
{
    if (needle.empty())
        return true;
    return toLowerCopy(value).find(toLowerCopy(needle)) != std::string::npos;
}
}

void InventoryManagerController::removeInventoryEntry(int entryIndex)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries.erase(inventory->entries.begin() + entryIndex);
    if (_selectedInventoryEntryIndex >= static_cast<int>(inventory->entries.size()))
        _selectedInventoryEntryIndex = static_cast<int>(inventory->entries.size()) - 1;
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryQuantity(int entryIndex, int quantity)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].quantity = std::max(1, quantity);
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntrySlot(int entryIndex, const std::string &slot)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].slot = slot;
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryContainer(int entryIndex, const EntityId &containerEntryId)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    InventoryEntryModel &entry = inventory->entries[entryIndex];
    if (containerEntryId == entry.metadata.id)
        return;
    entry.containerEntryId = containerEntryId;
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryAttuned(int entryIndex, bool attuned)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].attuned = attuned;
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryCharges(int entryIndex, int charges)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].charges = std::max(-1, charges);
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryAmmunition(int entryIndex, int ammunitionRemaining)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].ammunitionRemaining = std::max(0, ammunitionRemaining);
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryDurability(int entryIndex, int durability)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].durability = std::max(-1, durability);
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setInventoryEntryStateNotes(int entryIndex, const std::string &stateNotes)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].stateNotes = stateNotes;
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::transferInventoryEntryToTarget(int entryIndex, int quantity)
{
    InventoryModel *source = getSelectedInventory();
    InventoryModel *target = getTransferInventory();
    if (!source || !target || source == target || quantity <= 0 || entryIndex < 0 || entryIndex >= static_cast<int>(source->entries.size()))
        return;

    InventoryEntryModel &sourceEntry = source->entries[entryIndex];
    if (sourceEntry.quantity <= 0)
        return;

    const int movedQuantity = std::min(quantity, sourceEntry.quantity);
    InventoryEntryModel movedEntry = sourceEntry;
    movedEntry.metadata = EntityMetadata{"inventory_entry"};
    movedEntry.quantity = movedQuantity;
    movedEntry.equipped = false;
    movedEntry.containerEntryId.clear();

    const std::vector<ItemModel> items = getAvailableItems();
    const ItemModel *item = findItemById(items, sourceEntry.itemId);
    bool merged = false;
    if (item && item->stackable)
    {
        for (InventoryEntryModel &targetEntry : target->entries)
        {
            if (targetEntry.itemId == sourceEntry.itemId && targetEntry.containerEntryId.empty())
            {
                targetEntry.quantity += movedQuantity;
                merged = true;
                break;
            }
        }
    }
    if (!merged)
        target->entries.push_back(movedEntry);

    sourceEntry.quantity -= movedQuantity;
    if (sourceEntry.quantity <= 0)
        source->entries.erase(source->entries.begin() + entryIndex);
    if (_selectedInventoryEntryIndex >= static_cast<int>(source->entries.size()))
        _selectedInventoryEntryIndex = static_cast<int>(source->entries.size()) - 1;
    touchSelectedOwner();
    if (_transferOwnerType == OwnerType::Player && _transferOwnerIndex >= 0 && _transferOwnerIndex < static_cast<int>(_currentCampaign.characters.size()))
        _currentCampaign.characters[_transferOwnerIndex].metadata.touch();
    else if (_transferOwnerType == OwnerType::NPC && _transferOwnerIndex >= 0 && _transferOwnerIndex < static_cast<int>(_currentCampaign.npcs.size()))
        _currentCampaign.npcs[_transferOwnerIndex].metadata.touch();
    else if (_transferOwnerType == OwnerType::Party)
        _currentCampaign.partyInventory.metadata.touch();
    saveCampaign();
}

void InventoryManagerController::toggleEquipped(int entryIndex)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || entryIndex < 0 || entryIndex >= static_cast<int>(inventory->entries.size()))
        return;

    inventory->entries[entryIndex].equipped = !inventory->entries[entryIndex].equipped;
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::touchSelectedOwner()
{
    if (_ownerType == OwnerType::Party)
    {
        _currentCampaign.partyInventory.metadata.touch();
        return;
    }
    if (_ownerType == OwnerType::Player)
    {
        if (_selectedOwnerIndex >= 0 && _selectedOwnerIndex < static_cast<int>(_currentCampaign.characters.size()))
            _currentCampaign.characters[_selectedOwnerIndex].metadata.touch();
        return;
    }

    if (_selectedOwnerIndex >= 0 && _selectedOwnerIndex < static_cast<int>(_currentCampaign.npcs.size()))
        _currentCampaign.npcs[_selectedOwnerIndex].metadata.touch();
}
