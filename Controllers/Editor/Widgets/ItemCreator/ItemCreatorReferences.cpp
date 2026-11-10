#include "ItemCreatorController.hpp"
#include "ItemCreatorControllerSupport.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/CampaignEntityDraft.hpp"
#include "Models/Project/NativeAssetPicker.hpp"
#include "Models/Project/ProjectAssetImportService.hpp"
#include <algorithm>
#include <cctype>

namespace
{
std::string uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? german : english;
}

}

void ItemCreatorController::addPowerToDraftItem()
{
    markDraftEdited();

    _statusMessage.clear();
    _errorMessage.clear();
    if (_itemPowerDraft.name.empty())
    {
        _errorMessage = uiText("Item power name is required.", "Die Gegenstandskraft benötigt einen Namen.");
        return;
    }

    MonsterActionModel action = _itemPowerDraft;
    if (action.actionKind.empty() || action.actionKind == "Attack")
        action.actionKind = "Item Power";
    action.sourceLabel = _model.name;
    action.sourceEntityId = _model.metadata.id;
    action.reachFeet = std::max(0, action.reachFeet);
    action.normalRangeFeet = std::max(action.reachFeet, action.normalRangeFeet);
    action.rangeFeet = std::max(action.normalRangeFeet, action.rangeFeet);
    _model.powers.push_back(action);
    _powerDraftEdited = false;
    _powerDraftEdited = false;
    _itemPowerDraft = MonsterActionModel{};
    _itemPowerDraft.actionKind = "Item Power";
}

void ItemCreatorController::removePowerFromDraftItem(int actionIndex)
{
    markDraftEdited();

    if (actionIndex < 0 || actionIndex >= static_cast<int>(_model.powers.size()))
        return;
    _model.powers.erase(_model.powers.begin() + actionIndex);
}

void ItemCreatorController::selectItem(int itemIndex)
{
if (itemIndex < 0 || itemIndex >= static_cast<int>(_currentCampaign.customItems.size())) return;
    const EntityId id = _currentCampaign.customItems[static_cast<std::size_t>(itemIndex)].metadata.id;
    if (id == _selectedItemId) { _selectedItemIndex = itemIndex; return; }
    retainCurrentDraft();
    if (restoreDraft(id)) return;
    _selectedItemId = id;
    _selectedItemIndex = itemIndex;
    _model = _currentCampaign.customItems[static_cast<std::size_t>(itemIndex)];
    _draftBase = _model;
    _draftEdited = false;
    _powerDraftEdited = false;
    _itemPowerDraft = MonsterActionModel{};
    _itemPowerDraft.actionKind = "Item Power";
    _statusMessage.clear();
    _errorMessage.clear();
}

void ItemCreatorController::selectCatalogCustomItem(const CatalogEntry &entry)
{
    if (!entry.campaignCustom)
        return;
    selectItem(entry.customItemIndex);
}

void ItemCreatorController::archiveSelectedItem(bool archived)
{
    if(!draftRecoveryReady())return;
if (!refreshSelectedItemIndex() || _selectedItemId.empty()) return;
    setArchivedTag(_model, archived);
    if (!saveItem(true)) return;
    _statusMessage = (archived ? uiText("Archived: ", "Archiviert: ") : uiText("Restored: ", "Wiederhergestellt: ")) + _model.name;
}

void ItemCreatorController::deleteSelectedItem()
{
if (!draftRecoveryReady()) return;
if (!refreshSelectedItemIndex() || _selectedItemId.empty()) return;
    const EntityId removedId = _selectedItemId;
    const std::string removedName = _model.name;
    Campaign proposed = _currentCampaign;
    proposed.customItems.erase(std::remove_if(proposed.customItems.begin(), proposed.customItems.end(),
        [&](const ItemModel &item) { return item.metadata.id == removedId; }), proposed.customItems.end());
    removeDeletedItemReferences(proposed, removedId);
    proposed.metadata.touch();
    if (!ProjectSaveService::getInstance().saveCampaign(proposed, "delete item"))
    {
        _statusMessage.clear();
        _errorMessage = uiText("Deletion was not saved. The item and its links remain available. Retry Delete. ",
            "Das Löschen wurde nicht gespeichert. Gegenstand und Verknüpfungen bleiben erhalten. Versuche Löschen erneut. ") +
            ProjectSaveService::getInstance().state().lastError;
        return;
    }
    _currentCampaign = std::move(proposed);
    _retainedDrafts.erase(removedId);
    _draftEdited = false;
    _selectedItemId.clear();
    _selectedItemIndex = -1;
    _model = ItemModel{};
    _draftBase.reset();
    _powerDraftEdited = false;
    _itemPowerDraft = MonsterActionModel{};
    _itemPowerDraft.actionKind = "Item Power";
    _errorMessage.clear();
    _statusMessage = uiText("Deleted: ", "Gelöscht: ") + removedName;
    if(_recoveryCommitted)_recoveryCommitted();
}

void ItemCreatorController::removeDeletedItemReferences(Campaign &campaign, const EntityId &itemId)
{
    for (CharacterModel &character : campaign.characters)
    {
        auto &entries = character.inventory.entries;
        const auto originalSize = entries.size();
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const InventoryEntryModel &entry) {
            return entry.itemId == itemId;
        }), entries.end());
        if (entries.size() != originalSize)
            character.metadata.touch();
    }

    for (NPCModel &npc : campaign.npcs)
    {
        auto &entries = npc.inventory.entries;
        const auto originalSize = entries.size();
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const InventoryEntryModel &entry) {
            return entry.itemId == itemId;
        }), entries.end());
        if (entries.size() != originalSize)
            npc.metadata.touch();
    }

    for (StoryThreadModel &thread : campaign.storyThreads)
    {
        auto &threadLinks = thread.links;
        const auto originalThreadLinkSize = threadLinks.size();
        threadLinks.erase(std::remove_if(threadLinks.begin(), threadLinks.end(), [&](const StoryLinkModel &link) {
            return link.targetEntityType == "Item" && link.targetEntityId == itemId;
        }), threadLinks.end());

        bool changed = threadLinks.size() != originalThreadLinkSize;
        for (StoryBeatModel &beat : thread.beats)
        {
            auto &beatLinks = beat.links;
            const auto originalBeatLinkSize = beatLinks.size();
            beatLinks.erase(std::remove_if(beatLinks.begin(), beatLinks.end(), [&](const StoryLinkModel &link) {
                return link.targetEntityType == "Item" && link.targetEntityId == itemId;
            }), beatLinks.end());
            if (beatLinks.size() != originalBeatLinkSize)
            {
                beat.metadata.touch();
                changed = true;
            }
        }
        if (changed)
            thread.metadata.touch();
    }

    const auto removeEntries = [&](auto &entries) {
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const auto &entry) {
            return entry.itemId == itemId;
        }), entries.end());
    };
    removeEntries(campaign.partyInventory.entries);
    for (auto &shop : campaign.shops) removeEntries(shop.stock);
    for (auto &parcel : campaign.lootParcels) removeEntries(parcel.items);

}
