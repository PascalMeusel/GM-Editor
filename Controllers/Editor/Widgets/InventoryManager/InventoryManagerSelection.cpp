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

void InventoryManagerController::saveCampaign()
{
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().markDirty("Party inventory updated");
}

void InventoryManagerController::onShow()
{
    reconcileOwners();
    _view->show();
    _view->requestDockOnNextUpdate();
}


bool InventoryManagerController::selectOwnerById(const EntityId &ownerId)
{
    ensureCampaignSelection();
    for (const auto type : {OwnerType::Party, OwnerType::Player, OwnerType::NPC})
    {
        const int index = ownerIndex(type, ownerId);
        if (index >= 0) { selectOwner(type, index); return true; }
    }
    return false;
}

void InventoryManagerController::ensureCampaignSelection()
{
    if (_selectionCampaignId == _currentCampaign.metadata.id) return;
    _selectionCampaignId = _currentCampaign.metadata.id;
    _ownerType = _transferOwnerType = OwnerType::Party;
    _selectedOwnerId = _transferOwnerId = _currentCampaign.partyInventory.metadata.id;
    _selectedOwnerIndex = _transferOwnerIndex = 0;
    _selectedInventoryEntryId.clear();
    _selectedInventoryEntryIndex = -1;
    _selectedItemId.clear();
    _selectedItemIndex = -1;
}

int InventoryManagerController::ownerIndex(OwnerType type, const EntityId &id) const
{
    if (id.empty()) return -1;
    if (type == OwnerType::Party) return id == _currentCampaign.partyInventory.metadata.id ? 0 : -1;
    const auto find = [&](const auto &entries) {
        for (std::size_t i = 0; i < entries.size(); ++i)
            if (entries[i].metadata.id == id && std::find(entries[i].metadata.tags.begin(), entries[i].metadata.tags.end(), "archived") == entries[i].metadata.tags.end())
                return static_cast<int>(i);
        return -1;
    };
    return type == OwnerType::Player ? find(_currentCampaign.characters) : find(_currentCampaign.npcs);
}

void InventoryManagerController::reconcileOwners()
{
    ensureCampaignSelection();
    _selectedOwnerIndex = ownerIndex(_ownerType, _selectedOwnerId);
    _transferOwnerIndex = ownerIndex(_transferOwnerType, _transferOwnerId);
}

void InventoryManagerController::selectOwner(OwnerType type, int index)
{
    ensureCampaignSelection();
    _ownerType = type;
    _selectedOwnerId.clear();
    if (type == OwnerType::Party) _selectedOwnerId = _currentCampaign.partyInventory.metadata.id;
    else if (type == OwnerType::Player && index >= 0 && index < static_cast<int>(_currentCampaign.characters.size()))
        _selectedOwnerId = _currentCampaign.characters[static_cast<std::size_t>(index)].metadata.id;
    else if (type == OwnerType::NPC && index >= 0 && index < static_cast<int>(_currentCampaign.npcs.size()))
        _selectedOwnerId = _currentCampaign.npcs[static_cast<std::size_t>(index)].metadata.id;
    _selectedOwnerIndex = ownerIndex(type, _selectedOwnerId);
    _selectedInventoryEntryId.clear();
    _selectedInventoryEntryIndex = -1;
}

void InventoryManagerController::selectTransferOwner(OwnerType type, int index)
{
    ensureCampaignSelection();
    _transferOwnerType = type;
    _transferOwnerId.clear();
    if (type == OwnerType::Party) _transferOwnerId = _currentCampaign.partyInventory.metadata.id;
    else if (type == OwnerType::Player && index >= 0 && index < static_cast<int>(_currentCampaign.characters.size()))
        _transferOwnerId = _currentCampaign.characters[static_cast<std::size_t>(index)].metadata.id;
    else if (type == OwnerType::NPC && index >= 0 && index < static_cast<int>(_currentCampaign.npcs.size()))
        _transferOwnerId = _currentCampaign.npcs[static_cast<std::size_t>(index)].metadata.id;
    _transferOwnerIndex = ownerIndex(type, _transferOwnerId);
}

void InventoryManagerController::selectInventoryEntry(int index)
{
    const auto *inventory = getSelectedInventory();
    _selectedInventoryEntryId = inventory && index >= 0 && index < static_cast<int>(inventory->entries.size())
        ? inventory->entries[static_cast<std::size_t>(index)].metadata.id : EntityId{};
    _selectedInventoryEntryIndex = _selectedInventoryEntryId.empty() ? -1 : index;
}

void InventoryManagerController::reconcileItemSelection(const std::vector<ItemModel> &items)
{
    _selectedItemIndex = -1;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].metadata.id == _selectedItemId) _selectedItemIndex = static_cast<int>(i);
}

bool InventoryManagerController::selectCatalogItem(const EntityId &id)
{
    const auto items = getAvailableItems();
    const auto visible = getVisibleItems(items, getAvailableCategories(items));
    _selectedItemId = id;
    reconcileItemSelection(visible);
    return _selectedItemIndex >= 0;
}

bool InventoryManagerController::focusItemForAssignment(const EntityId &itemId, const EntityId &ownerId)
{
    ensureCampaignSelection();
    if (!ownerId.empty())
    {
        int index = ownerIndex(OwnerType::Player, ownerId);
        if (index >= 0) selectOwner(OwnerType::Player, index);
        else if ((index = ownerIndex(OwnerType::NPC, ownerId)) >= 0) selectOwner(OwnerType::NPC, index);
        else return false;
    }
    _activeTask = InventoryTask::FindItem;
    _itemSearch.clear();
    _selectedCategoryIndex = 0;
    return selectCatalogItem(itemId);
}

bool InventoryManagerController::persistInventory()
{
    return ProjectSaveService::getInstance().saveCampaign(_currentCampaign, "save inventory changes");
}

EntityId InventoryManagerController::currentSelectedItemId() const
{
    if (_selectionCampaignId != _currentCampaign.metadata.id) return {};
    EntityId id;
    if (_activeTask == InventoryTask::FindItem || _activeTask == InventoryTask::LootParcels)
        id = _selectedItemIndex >= 0 ? _selectedItemId : EntityId{};
    else
    {
        const InventoryModel *inventory = nullptr;
        const auto ownerInventory = [&](const auto &owners) -> const InventoryModel * {
            const InventoryModel *found = nullptr;
            for (const auto &owner : owners) if (owner.metadata.id == _selectedOwnerId)
            { if (found) return nullptr; found = &owner.inventory; }
            return found;
        };
        if (_ownerType == OwnerType::Party && _selectedOwnerId == _currentCampaign.partyInventory.metadata.id)
            inventory = &_currentCampaign.partyInventory;
        else if (_ownerType == OwnerType::Player) inventory = ownerInventory(_currentCampaign.characters);
        else if (_ownerType == OwnerType::NPC) inventory = ownerInventory(_currentCampaign.npcs);
        if (!inventory || _selectedInventoryEntryId.empty()) return {};
        int matches = 0;
        for (const auto &entry : inventory->entries) if (entry.metadata.id == _selectedInventoryEntryId)
        { id = entry.itemId; ++matches; }
        if (matches != 1) return {};
    }
    const auto items = getAvailableItems();
    return !id.empty() && std::count_if(items.begin(), items.end(), [&](const auto &item) {
        return item.metadata.id == id;
    }) == 1 ? id : EntityId{};
}
