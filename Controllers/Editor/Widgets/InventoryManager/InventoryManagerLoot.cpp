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

void InventoryManagerController::selectBearer(const ItemBearer &bearer)
{
    selectOwner(bearer.ownerType, bearer.ownerIndex);
    _selectedInventoryEntryIndex = -1;
    _activeTask = InventoryTask::EditOwnerInventory;
}

void InventoryManagerController::addSelectedItem()
{
    InventoryModel *inventory = getSelectedInventory();
    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<std::string> categories = getAvailableCategories(items);
    const std::vector<ItemModel> visibleItems = getVisibleItems(items, categories);
    reconcileItemSelection(visibleItems);
    if (!inventory || visibleItems.empty() || _selectedItemIndex < 0 || _selectedItemIndex >= static_cast<int>(visibleItems.size()))
        return;

    const ItemModel &item = visibleItems[_selectedItemIndex];
    const int quantity = std::max(1, _quantity);
    for (auto &entry : inventory->entries)
    {
        if (entry.itemId == item.metadata.id && item.stackable)
        {
            entry.quantity += quantity;
            touchSelectedOwner();
            saveCampaign();
            return;
        }
    }

    InventoryEntryModel entry;
    entry.itemId = item.metadata.id;
    entry.quantity = quantity;
    if (item.maxCharges > 0)
        entry.charges = item.maxCharges;
    if (item.maxDurability > 0)
        entry.durability = item.maxDurability;
    if (item.ammunition)
        entry.ammunitionRemaining = quantity;
    inventory->entries.push_back(entry);
    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::setSelectedInventoryCoin(const std::string &code, int amount)
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory)
        return;

    const int normalizedAmount = std::max(0, amount);
    if (code == "cp")
        inventory->copper = normalizedAmount;
    else if (code == "sp")
        inventory->silver = normalizedAmount;
    else if (code == "gp")
        inventory->gold = normalizedAmount;
    else if (code == "pp")
        inventory->platinum = normalizedAmount;
    else
        return;

    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::normalizeSelectedInventoryCurrency()
{
    InventoryModel *inventory = getSelectedInventory();
    if (!inventory || !_currentCampaign.ruleSet)
        return;

    const InventoryModel before = *inventory;
    _currentCampaign.ruleSet->normalizeInventoryCurrency(*inventory);
    if (before.copper == inventory->copper &&
        before.silver == inventory->silver &&
        before.gold == inventory->gold &&
        before.platinum == inventory->platinum)
        return;

    touchSelectedOwner();
    saveCampaign();
}

void InventoryManagerController::createLootParcel()
{
    LootParcelModel parcel;
    parcel.name = gm::localization::trUi("Loot Parcel");
    _currentCampaign.lootParcels.push_back(parcel);
    saveCampaign();
}

void InventoryManagerController::setLootParcelName(int parcelIndex, const std::string &name)
{
    if (parcelIndex < 0 || parcelIndex >= static_cast<int>(_currentCampaign.lootParcels.size()))
        return;
    LootParcelModel &parcel = _currentCampaign.lootParcels[static_cast<std::size_t>(parcelIndex)];
    parcel.name = name.empty() ? gm::localization::trUi("Loot Parcel") : name;
    parcel.metadata.touch();
    saveCampaign();
}

void InventoryManagerController::setLootParcelCoin(int parcelIndex, const std::string &code, int amount)
{
    if (parcelIndex < 0 || parcelIndex >= static_cast<int>(_currentCampaign.lootParcels.size()))
        return;
    LootParcelModel &parcel = _currentCampaign.lootParcels[static_cast<std::size_t>(parcelIndex)];
    const int value = std::max(0, amount);
    if (code == "cp")
        parcel.copper = value;
    else if (code == "sp")
        parcel.silver = value;
    else if (code == "gp")
        parcel.gold = value;
    else if (code == "pp")
        parcel.platinum = value;
    parcel.metadata.touch();
    saveCampaign();
}

void InventoryManagerController::addSelectedItemToLootParcel(int parcelIndex)
{
    if (parcelIndex < 0 || parcelIndex >= static_cast<int>(_currentCampaign.lootParcels.size()))
        return;
    const std::vector<ItemModel> items = getAvailableItems();
    const std::vector<std::string> categories = getAvailableCategories(items);
    const std::vector<ItemModel> visibleItems = getVisibleItems(items, categories);
    reconcileItemSelection(visibleItems);
    if (visibleItems.empty() || _selectedItemIndex < 0 || _selectedItemIndex >= static_cast<int>(visibleItems.size()))
        return;

    addItemToLootParcel(parcelIndex,visibleItems[static_cast<std::size_t>(_selectedItemIndex)].metadata.id);
}

void InventoryManagerController::addItemToLootParcel(int parcelIndex,const EntityId &itemId)
{
    if(parcelIndex<0 || parcelIndex>=int(_currentCampaign.lootParcels.size()))return;
    auto &parcel=_currentCampaign.lootParcels[std::size_t(parcelIndex)];
    if(parcel.awarded)return;
    const auto available=getAvailableItems();
    const auto found=std::find_if(available.begin(),available.end(),[&](const auto &item){return item.metadata.id==itemId && !isItemArchived(item);});
    if(found==available.end())return;
    const auto &item=*found;
    for (LootParcelItemModel &parcelItem : parcel.items)
    {
        if (parcelItem.itemId == item.metadata.id)
        {
            ++parcelItem.quantity;
            parcel.metadata.touch();
            saveCampaign();
            return;
        }
    }
    parcel.items.push_back({item.metadata.id, 1, ""});
    parcel.metadata.touch();
    saveCampaign();
}

void InventoryManagerController::awardLootParcelToParty(int parcelIndex)
{
    if (parcelIndex < 0 || parcelIndex >= static_cast<int>(_currentCampaign.lootParcels.size()))
        return;
    LootParcelModel &parcel = _currentCampaign.lootParcels[static_cast<std::size_t>(parcelIndex)];
    if (parcel.awarded)
        return;

    _currentCampaign.partyInventory.copper += parcel.copper;
    _currentCampaign.partyInventory.silver += parcel.silver;
    _currentCampaign.partyInventory.gold += parcel.gold;
    _currentCampaign.partyInventory.platinum += parcel.platinum;
    const std::vector<ItemModel> availableItems = getAvailableItems();
    for (const LootParcelItemModel &parcelItem : parcel.items)
    {
        bool merged = false;
        const ItemModel *item = findItemById(availableItems, parcelItem.itemId);
        for (InventoryEntryModel &entry : _currentCampaign.partyInventory.entries)
        {
            if (entry.itemId == parcelItem.itemId && item && item->stackable)
            {
                entry.quantity += std::max(1, parcelItem.quantity);
                merged = true;
                break;
            }
        }
        if (!merged)
            _currentCampaign.partyInventory.entries.push_back({parcelItem.itemId, std::max(1, parcelItem.quantity), false, "Party"});
    }
    parcel.awarded = true;
    parcel.awardedTo = gm::localization::trUi("Party Inventory");
    parcel.metadata.touch();
    _currentCampaign.partyInventory.metadata.touch();
    saveCampaign();
}
