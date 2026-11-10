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

InventoryManagerController::InventoryManagerController(Campaign &currentCampaign) : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<InventoryManagerView>(*this);
}

void InventoryManagerController::update(std::shared_ptr<GLFWwindow> window)
{
    if (!m_isOpen)
        return;

    reconcileOwners();
    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

bool InventoryManagerController::selectPlayerOwnerById(const EntityId &playerId)
{
    if (playerId.empty())
        return false;

    const auto found = std::find_if(_currentCampaign.characters.begin(), _currentCampaign.characters.end(), [&](const CharacterModel &character) {
        return character.metadata.id == playerId;
    });
    if (found == _currentCampaign.characters.end())
        return false;

    selectOwner(OwnerType::Player, static_cast<int>(std::distance(_currentCampaign.characters.begin(), found)));
    _selectedInventoryEntryIndex = -1;
    _activeTask = InventoryTask::EditOwnerInventory;
    return true;
}

std::vector<ItemModel> InventoryManagerController::getAvailableItems() const
{
    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    return items;
}

std::vector<std::string> InventoryManagerController::getAvailableCategories(const std::vector<ItemModel> &items) const
{
    std::vector<std::string> categories{"All"};
    for (const auto &item : items)
    {
        const std::string category = getItemCategory(item);
        if (std::find(categories.begin(), categories.end(), category) == categories.end())
            categories.push_back(category);
    }

    std::sort(categories.begin() + 1, categories.end());
    return categories;
}

std::vector<ItemModel> InventoryManagerController::getVisibleItems(const std::vector<ItemModel> &items, const std::vector<std::string> &categories) const
{
    std::vector<ItemModel> visibleItems;
    const std::string selectedCategory =
        (_selectedCategoryIndex > 0 && _selectedCategoryIndex < static_cast<int>(categories.size())) ? categories[_selectedCategoryIndex] : "All";

    for (const auto &item : items)
    {
        const bool categoryMatches = selectedCategory == "All" || getItemCategory(item) == selectedCategory;
        const bool searchMatches = containsText(item.name, _itemSearch) ||
                                   containsText(getItemCategory(item), _itemSearch) ||
                                   containsText(item.itemType, _itemSearch);
        if (categoryMatches && searchMatches && !isItemArchived(item))
            visibleItems.push_back(item);
    }

    std::sort(visibleItems.begin(), visibleItems.end(), [](const ItemModel &left, const ItemModel &right) {
        const std::string leftCategory = getItemCategory(left);
        const std::string rightCategory = getItemCategory(right);
        if (leftCategory != rightCategory)
            return leftCategory < rightCategory;
        return left.name < right.name;
    });
    return visibleItems;
}

std::vector<InventoryManagerController::ItemBearer> InventoryManagerController::getBearersForItem(const EntityId &itemId) const
{
    std::vector<ItemBearer> bearers;
    for (int ownerIndex = 0; ownerIndex < static_cast<int>(_currentCampaign.characters.size()); ++ownerIndex)
    {
        const CharacterModel &character = _currentCampaign.characters[ownerIndex];
        for (const InventoryEntryModel &entry : character.inventory.entries)
        {
            if (entry.itemId == itemId)
                bearers.push_back({OwnerType::Player, ownerIndex, character.nameBuffer, entry.quantity, entry.equipped});
        }
    }

    for (int ownerIndex = 0; ownerIndex < static_cast<int>(_currentCampaign.npcs.size()); ++ownerIndex)
    {
        const NPCModel &npc = _currentCampaign.npcs[ownerIndex];
        for (const InventoryEntryModel &entry : npc.inventory.entries)
        {
            if (entry.itemId == itemId)
                bearers.push_back({OwnerType::NPC, ownerIndex, npc.name.empty() ? gm::localization::trUi("<Unnamed NPC>") : npc.name, entry.quantity, entry.equipped});
        }
    }
    return bearers;
}

InventoryModel *InventoryManagerController::getSelectedInventory()
{
reconcileOwners();
    InventoryModel *inventory = nullptr;
    if (_ownerType == OwnerType::Party && _selectedOwnerIndex >= 0) inventory = &_currentCampaign.partyInventory;
    else if (_selectedOwnerIndex >= 0 && _ownerType == OwnerType::Player)
        inventory = &_currentCampaign.characters[static_cast<std::size_t>(_selectedOwnerIndex)].inventory;
    else if (_selectedOwnerIndex >= 0)
        inventory = &_currentCampaign.npcs[static_cast<std::size_t>(_selectedOwnerIndex)].inventory;
    _selectedInventoryEntryIndex = -1;
    if (inventory && !_selectedInventoryEntryId.empty())
        for (std::size_t i = 0; i < inventory->entries.size(); ++i)
            if (inventory->entries[i].metadata.id == _selectedInventoryEntryId) _selectedInventoryEntryIndex = static_cast<int>(i);
    return inventory;
}

InventoryModel *InventoryManagerController::getTransferInventory()
{
    reconcileOwners();
    if (_transferOwnerType == OwnerType::Party)
        return _transferOwnerIndex >= 0 ? &_currentCampaign.partyInventory : nullptr;
    if (_transferOwnerType == OwnerType::Player)
    {
        if (_transferOwnerIndex < 0 || _transferOwnerIndex >= static_cast<int>(_currentCampaign.characters.size()))
            return nullptr;
        return &_currentCampaign.characters[_transferOwnerIndex].inventory;
    }
    if (_transferOwnerIndex < 0 || _transferOwnerIndex >= static_cast<int>(_currentCampaign.npcs.size()))
        return nullptr;
    return &_currentCampaign.npcs[_transferOwnerIndex].inventory;
}

std::map<std::string, int> InventoryManagerController::getSelectedOwnerAttributes() const
{
    if (_ownerType == OwnerType::Party)
        return {};
    if (_ownerType == OwnerType::Player)
    {
        if (_selectedOwnerIndex < 0 || _selectedOwnerIndex >= static_cast<int>(_currentCampaign.characters.size()))
            return {};
        return _currentCampaign.characters[_selectedOwnerIndex].attributes;
    }

    if (_selectedOwnerIndex < 0 || _selectedOwnerIndex >= static_cast<int>(_currentCampaign.npcs.size()))
        return {};
    return _currentCampaign.npcs[_selectedOwnerIndex].attributes;
}

std::string InventoryManagerController::getSelectedOwnerName() const
{
    if (_ownerType == OwnerType::Party)
        return gm::localization::trUi("Party Inventory");
    if (_ownerType == OwnerType::Player)
    {
        if (_selectedOwnerIndex < 0 || _selectedOwnerIndex >= static_cast<int>(_currentCampaign.characters.size()))
            return "";
        return _currentCampaign.characters[_selectedOwnerIndex].nameBuffer;
    }

    if (_selectedOwnerIndex < 0 || _selectedOwnerIndex >= static_cast<int>(_currentCampaign.npcs.size()))
        return "";
    return _currentCampaign.npcs[_selectedOwnerIndex].name;
}

std::string InventoryManagerController::getTransferOwnerName() const
{
    if (_transferOwnerType == OwnerType::Party)
        return gm::localization::trUi("Party Inventory");
    if (_transferOwnerType == OwnerType::Player)
    {
        if (_transferOwnerIndex < 0 || _transferOwnerIndex >= static_cast<int>(_currentCampaign.characters.size()))
            return "";
        return _currentCampaign.characters[_transferOwnerIndex].nameBuffer;
    }
    if (_transferOwnerIndex < 0 || _transferOwnerIndex >= static_cast<int>(_currentCampaign.npcs.size()))
        return "";
    return _currentCampaign.npcs[_transferOwnerIndex].name;
}

void InventoryManagerController::selectInventoryTask(InventoryTask task)
{
    _activeTask = task;
    _selectedInventoryEntryIndex = -1;

    if (_activeTask == InventoryTask::ManagePartyInventory || _activeTask == InventoryTask::LootParcels)
    {
        selectOwner(OwnerType::Party, 0);
    }

    if (_activeTask == InventoryTask::TransferBetweenOwners && _transferOwnerType == _ownerType && _transferOwnerIndex == _selectedOwnerIndex)
    {
        _transferOwnerType = OwnerType::Party;
        _transferOwnerIndex = 0;
        if (_ownerType == OwnerType::Party && !_currentCampaign.characters.empty())
            _transferOwnerType = OwnerType::Player;
        selectTransferOwner(_transferOwnerType, _transferOwnerIndex);
    }
}
