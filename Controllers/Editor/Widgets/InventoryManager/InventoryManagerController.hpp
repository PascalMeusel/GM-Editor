#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Inventory/InventoryManager/InventoryManagerView.hpp"
#include <map>
#include <functional>

class InventoryManagerController : public IWidgetController
{
public:
    explicit InventoryManagerController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    bool selectPlayerOwnerById(const EntityId &playerId);
    bool focusItemForAssignment(const EntityId &itemId, const EntityId &ownerId = {});
    void setEditItemHandler(std::function<void(EntityId, bool)> handler) { _editItemHandler = std::move(handler); }
    bool selectOwnerById(const EntityId &ownerId);
    bool persistInventory();
    void resetSelection() { _selectionCampaignId.clear(); ensureCampaignSelection(); _itemSearch.clear(); _selectedCategoryIndex = 0; }
    EntityId selectedOwnerId() const { return _selectedOwnerIndex >= 0 ? _selectedOwnerId : EntityId{}; }
    EntityId currentSelectedItemId() const;
    void setPlayerReviewHandler(std::function<void(EntityId)> handler) { _playerReviewHandler = std::move(handler); }

private:
    friend struct ContextPreviewUiProbe;
    std::function<void(EntityId)> _playerReviewHandler;
    friend class InventoryManagerView;
    friend struct InventoryWorkflowProbe;

    enum class OwnerType
    {
        Player,
        NPC,
        Party
    };

    enum class InventoryTask
    {
        FindItem,
        EditOwnerInventory,
        TransferBetweenOwners,
        ManagePartyInventory,
        LootParcels
    };

    struct ItemBearer
    {
        OwnerType ownerType = OwnerType::Player;
        int ownerIndex = 0;
        std::string ownerName;
        int quantity = 1;
        bool equipped = false;
    };

    EntityId _selectionCampaignId, _selectedOwnerId, _transferOwnerId, _selectedItemId, _selectedInventoryEntryId;
    std::function<void(EntityId, bool)> _editItemHandler;
    void ensureCampaignSelection();
    void reconcileOwners();
    int ownerIndex(OwnerType type, const EntityId &id) const;
    void selectOwner(OwnerType type, int index);
    void selectTransferOwner(OwnerType type, int index);
    void selectInventoryEntry(int index);
    bool selectCatalogItem(const EntityId &id);
    void reconcileItemSelection(const std::vector<ItemModel> &items);
    std::unique_ptr<InventoryManagerView> _view;
    Campaign &_currentCampaign;
    InventoryTask _activeTask = InventoryTask::FindItem;
    OwnerType _ownerType = OwnerType::Player;
    OwnerType _transferOwnerType = OwnerType::Party;
    int _selectedOwnerIndex = 0;
    int _transferOwnerIndex = 0;
    int _selectedItemIndex = 0;
    int _selectedCategoryIndex = 0;
    int _selectedInventoryEntryIndex = -1;
    int _quantity = 1;
    std::string _itemSearch;

    std::vector<ItemModel> getAvailableItems() const;
    std::vector<std::string> getAvailableCategories(const std::vector<ItemModel> &items) const;
    std::vector<ItemModel> getVisibleItems(const std::vector<ItemModel> &items, const std::vector<std::string> &categories) const;
    std::vector<ItemBearer> getBearersForItem(const EntityId &itemId) const;
    InventoryModel *getSelectedInventory();
    InventoryModel *getTransferInventory();
    std::map<std::string, int> getSelectedOwnerAttributes() const;
    std::string getSelectedOwnerName() const;
    std::string getTransferOwnerName() const;
    void selectInventoryTask(InventoryTask task);
    void selectBearer(const ItemBearer &bearer);
    void addSelectedItem();
    void setSelectedInventoryCoin(const std::string &code, int amount);
    void normalizeSelectedInventoryCurrency();
    void createLootParcel();
    void setLootParcelName(int parcelIndex, const std::string &name);
    void setLootParcelCoin(int parcelIndex, const std::string &code, int amount);
    void addSelectedItemToLootParcel(int parcelIndex);
    void addItemToLootParcel(int parcelIndex,const EntityId &itemId);
    void awardLootParcelToParty(int parcelIndex);
    void setInventoryEntryQuantity(int entryIndex, int quantity);
    void setInventoryEntrySlot(int entryIndex, const std::string &slot);
    void setInventoryEntryContainer(int entryIndex, const EntityId &containerEntryId);
    void setInventoryEntryAttuned(int entryIndex, bool attuned);
    void setInventoryEntryCharges(int entryIndex, int charges);
    void setInventoryEntryAmmunition(int entryIndex, int ammunitionRemaining);
    void setInventoryEntryDurability(int entryIndex, int durability);
    void setInventoryEntryStateNotes(int entryIndex, const std::string &stateNotes);
    void transferInventoryEntryToTarget(int entryIndex, int quantity);
    void toggleEquipped(int entryIndex);
    void removeInventoryEntry(int entryIndex);
    void touchSelectedOwner();
    void saveCampaign();
    void onShow() override;
};
