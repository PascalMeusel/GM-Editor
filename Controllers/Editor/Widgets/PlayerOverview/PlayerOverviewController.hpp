#pragma once

#include "Controllers/Editor/Widgets/IWidgetController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Views/Editor/Widgets/Player/PlayerOverview/PlayerOverviewView.hpp"

#include <functional>
#include <string>
#include <vector>

class PlayerOverviewController : public IWidgetController
{
public:
    explicit PlayerOverviewController(Campaign &currentCampaign);

    void update(std::shared_ptr<GLFWwindow> window) override;
    void setOpenCharacterSheetsHandler(std::function<void(const EntityId &)> handler);
    void setOpenInventoryHandler(std::function<void(const EntityId &)> handler);
    void setOpenMapHandler(std::function<void(const EntityId &)> handler);
    void setOpenLiveSessionHandler(std::function<void()> handler);

private:
    friend class PlayerOverviewView;

    Campaign &_currentCampaign;
    std::unique_ptr<PlayerOverviewView> _view;
    std::string _searchText;
    int _classFilterIndex = 0;
    bool _showWoundedOnly = false;
    bool _showSpellcastersOnly = false;
    int _selectedPartyItemIndex = 0;
    int _partyItemQuantity = 1;
    std::string _newPartyVehicle;
    std::string _newPartyStronghold;
    std::function<void(const EntityId &)> _openCharacterSheetsHandler;
    std::function<void(const EntityId &)> _openInventoryHandler;
    std::function<void(const EntityId &)> _openMapHandler;
    std::function<void()> _openLiveSessionHandler;

    std::vector<std::string> getClassFilters() const;
    std::vector<int> getVisibleCharacterIndexes() const;
    std::vector<ItemModel> getAvailableItems() const;
    std::string getPartyLocationLabel() const;
    bool hasPartyMapTarget() const;
    std::string partyMapDisabledReason() const;
    void openCharacterSheet(int characterIndex);
    void openInventory(int characterIndex);
    void openPartyMap();
    void openLiveSessionContext();
    void setPartyCoin(const std::string &coinType, int value);
    void setPartyRenown(int value);
    void setPartyReputation(const std::string &value);
    void addPartyVehicle(const std::string &value);
    void removePartyVehicle(int index);
    void addPartyStronghold(const std::string &value);
    void removePartyStronghold(int index);
    void addPartyInventoryItem(const EntityId &itemId, int quantity);
    void setPartyInventoryEntryQuantity(int entryIndex, int quantity);
    void removePartyInventoryEntry(int entryIndex);
    void saveCampaign();
    void onShow() override;
};
