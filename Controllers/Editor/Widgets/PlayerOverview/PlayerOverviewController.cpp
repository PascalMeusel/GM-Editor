#include "PlayerOverviewController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsText(const std::string &value, const std::string &needle)
{
    if (needle.empty())
        return true;
    return lowerCopy(value).find(lowerCopy(needle)) != std::string::npos;
}

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    if (first >= last)
        return "";
    return std::string(first, last);
}
}

PlayerOverviewController::PlayerOverviewController(Campaign &currentCampaign)
    : _currentCampaign(currentCampaign)
{
    _view = std::make_unique<PlayerOverviewView>(*this);
}

void PlayerOverviewController::update(std::shared_ptr<GLFWwindow> window)
{
    (void)window;
    if (!m_isOpen)
        return;

    _view->update();
    if (!_view->isVisible())
        m_isOpen = false;
}

void PlayerOverviewController::setOpenCharacterSheetsHandler(std::function<void(const EntityId &)> handler)
{
    _openCharacterSheetsHandler = std::move(handler);
}

void PlayerOverviewController::setOpenInventoryHandler(std::function<void(const EntityId &)> handler)
{
    _openInventoryHandler = std::move(handler);
}

void PlayerOverviewController::setOpenMapHandler(std::function<void(const EntityId &)> handler)
{
    _openMapHandler = std::move(handler);
}

void PlayerOverviewController::setOpenLiveSessionHandler(std::function<void()> handler)
{
    _openLiveSessionHandler = std::move(handler);
}

std::vector<std::string> PlayerOverviewController::getClassFilters() const
{
    std::vector<std::string> filters{gm::localization::trUi("All Classes")};
    for (const CharacterModel &character : _currentCampaign.characters)
    {
        const std::string className = character.selectedClass.empty() ? gm::localization::trUi("Unclassed") : character.selectedClass;
        if (std::find(filters.begin(), filters.end(), className) == filters.end())
            filters.push_back(className);
    }
    std::sort(filters.begin() + 1, filters.end());
    return filters;
}

std::vector<int> PlayerOverviewController::getVisibleCharacterIndexes() const
{
    std::vector<int> indexes;
    const std::vector<std::string> classFilters = getClassFilters();
    const std::string selectedClass =
        (_classFilterIndex > 0 && _classFilterIndex < static_cast<int>(classFilters.size())) ? classFilters[static_cast<std::size_t>(_classFilterIndex)] : "";

    for (int i = 0; i < static_cast<int>(_currentCampaign.characters.size()); ++i)
    {
        CharacterModel character = _currentCampaign.characters[static_cast<std::size_t>(i)];
        normalizeCharacterHitPoints(character);

        const std::string className = character.selectedClass.empty() ? gm::localization::trUi("Unclassed") : character.selectedClass;
        if (!selectedClass.empty() && className != selectedClass)
            continue;
        if (_showWoundedOnly && character.currentHitPoints >= character.maxHitPoints)
            continue;
        if (_showSpellcastersOnly && !character.spellcasting.enabled)
            continue;

        const bool searchMatches = containsText(character.nameBuffer, _searchText) ||
                                   containsText(character.selectedClass, _searchText) ||
                                   containsText(character.selectedRace, _searchText) ||
                                   containsText(character.backstory, _searchText);
        if (searchMatches)
            indexes.push_back(i);
    }

    return indexes;
}

std::vector<ItemModel> PlayerOverviewController::getAvailableItems() const
{
    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());
    return items;
}

std::string PlayerOverviewController::getPartyLocationLabel() const
{
    if (_currentCampaign.partyLocation.mapId.empty())
        return gm::localization::trUi("No party map set");

    const auto mapIt = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == _currentCampaign.partyLocation.mapId;
    });
    const std::string mapName = mapIt == _currentCampaign.maps.end()
                                    ? gm::localization::trUi("Missing map")
                                    : (mapIt->name.empty() ? gm::localization::trUi("Unnamed map") : mapIt->name);
    return mapName + " (" + std::to_string(_currentCampaign.partyLocation.x) + ", " + std::to_string(_currentCampaign.partyLocation.y) + ")";
}

bool PlayerOverviewController::hasPartyMapTarget() const
{
    if (_currentCampaign.partyLocation.mapId.empty())
        return false;

    return std::any_of(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == _currentCampaign.partyLocation.mapId;
    });
}

std::string PlayerOverviewController::partyMapDisabledReason() const
{
    if (!_openMapHandler)
        return gm::localization::trUi("Map context is unavailable in the current workspace.");
    if (_currentCampaign.partyLocation.mapId.empty())
        return gm::localization::trUi("Set a party map position before opening map context from a row.");
    if (!hasPartyMapTarget())
        return gm::localization::trUi("The saved party map is missing from this campaign.");
    return "";
}

void PlayerOverviewController::openCharacterSheet(int characterIndex)
{
    if (characterIndex < 0 || characterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    if (_openCharacterSheetsHandler)
        _openCharacterSheetsHandler(_currentCampaign.characters[static_cast<std::size_t>(characterIndex)].metadata.id);
}

void PlayerOverviewController::openInventory(int characterIndex)
{
    if (characterIndex < 0 || characterIndex >= static_cast<int>(_currentCampaign.characters.size()))
        return;
    if (_openInventoryHandler)
        _openInventoryHandler(_currentCampaign.characters[static_cast<std::size_t>(characterIndex)].metadata.id);
}

void PlayerOverviewController::openPartyMap()
{
    if (_openMapHandler && hasPartyMapTarget())
        _openMapHandler(_currentCampaign.partyLocation.mapId);
}

void PlayerOverviewController::openLiveSessionContext()
{
    if (_openLiveSessionHandler)
        _openLiveSessionHandler();
}

void PlayerOverviewController::setPartyCoin(const std::string &coinType, int value)
{
    InventoryModel &inventory = _currentCampaign.partyInventory;
    if (coinType == "Copper")
        inventory.copper = std::max(0, value);
    else if (coinType == "Silver")
        inventory.silver = std::max(0, value);
    else if (coinType == "Gold")
        inventory.gold = std::max(0, value);
    else if (coinType == "Platinum")
        inventory.platinum = std::max(0, value);
    saveCampaign();
}

void PlayerOverviewController::setPartyRenown(int value)
{
    _currentCampaign.partyRenown = std::max(0, value);
    saveCampaign();
}

void PlayerOverviewController::setPartyReputation(const std::string &value)
{
    _currentCampaign.partyReputation = value;
    saveCampaign();
}

void PlayerOverviewController::addPartyVehicle(const std::string &value)
{
    const std::string trimmed = trimCopy(value);
    if (!trimmed.empty())
        _currentCampaign.partyVehicles.push_back(trimmed);
    saveCampaign();
}

void PlayerOverviewController::removePartyVehicle(int index)
{
    if (index >= 0 && index < static_cast<int>(_currentCampaign.partyVehicles.size()))
        _currentCampaign.partyVehicles.erase(_currentCampaign.partyVehicles.begin() + index);
    saveCampaign();
}

void PlayerOverviewController::addPartyStronghold(const std::string &value)
{
    const std::string trimmed = trimCopy(value);
    if (!trimmed.empty())
        _currentCampaign.partyStrongholds.push_back(trimmed);
    saveCampaign();
}

void PlayerOverviewController::removePartyStronghold(int index)
{
    if (index >= 0 && index < static_cast<int>(_currentCampaign.partyStrongholds.size()))
        _currentCampaign.partyStrongholds.erase(_currentCampaign.partyStrongholds.begin() + index);
    saveCampaign();
}

void PlayerOverviewController::addPartyInventoryItem(const EntityId &itemId, int quantity)
{
    if (itemId.empty())
        return;

    InventoryEntryModel entry;
    entry.itemId = itemId;
    entry.quantity = std::max(1, quantity);
    _currentCampaign.partyInventory.entries.push_back(entry);
    saveCampaign();
}

void PlayerOverviewController::setPartyInventoryEntryQuantity(int entryIndex, int quantity)
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(_currentCampaign.partyInventory.entries.size()))
        return;
    _currentCampaign.partyInventory.entries[entryIndex].quantity = std::max(1, quantity);
    saveCampaign();
}

void PlayerOverviewController::removePartyInventoryEntry(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(_currentCampaign.partyInventory.entries.size()))
        return;
    _currentCampaign.partyInventory.entries.erase(_currentCampaign.partyInventory.entries.begin() + entryIndex);
    saveCampaign();
}

void PlayerOverviewController::saveCampaign()
{
    _currentCampaign.partyInventory.ownerEntityId = _currentCampaign.metadata.id;
    if (_currentCampaign.partyInventory.name.empty())
        _currentCampaign.partyInventory.name = gm::localization::trUi("Party Inventory");
    _currentCampaign.metadata.touch();
    ProjectSaveService::getInstance().markDirty("Party overview updated");
}

void PlayerOverviewController::onShow()
{
    _view->show();
    _view->requestDockOnNextUpdate();
}
