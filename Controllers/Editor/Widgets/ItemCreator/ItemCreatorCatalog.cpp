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

ItemCreatorController::ItemReferenceSummary ItemCreatorController::getReferenceSummary(const EntityId &itemId) const
{
    ItemReferenceSummary summary;
    for (const CharacterModel &character : _currentCampaign.characters)
    {
        int ownerQuantity = 0;
        int ownerEntries = 0;
        for (const InventoryEntryModel &entry : character.inventory.entries)
        {
            if (entry.itemId == itemId)
            {
                ++ownerEntries;
                ownerQuantity += std::max(1, entry.quantity);
            }
        }
        if (ownerEntries > 0)
        {
            const std::string characterName = character.nameBuffer;
            summary.playerEntryCount += ownerEntries;
            summary.totalQuantity += ownerQuantity;
            summary.labels.push_back((characterName.empty() ? "<Unnamed Player>" : characterName) + " carries " + std::to_string(ownerQuantity));
        }
    }

    for (const NPCModel &npc : _currentCampaign.npcs)
    {
        int ownerQuantity = 0;
        int ownerEntries = 0;
        for (const InventoryEntryModel &entry : npc.inventory.entries)
        {
            if (entry.itemId == itemId)
            {
                ++ownerEntries;
                ownerQuantity += std::max(1, entry.quantity);
            }
        }
        if (ownerEntries > 0)
        {
            summary.npcEntryCount += ownerEntries;
            summary.totalQuantity += ownerQuantity;
            summary.labels.push_back((npc.name.empty() ? "<Unnamed NPC>" : npc.name) + " carries " + std::to_string(ownerQuantity));
        }
    }

    for (const StoryThreadModel &thread : _currentCampaign.storyThreads)
    {
        for (const StoryLinkModel &link : thread.links)
        {
            if (link.targetEntityType == "Item" && link.targetEntityId == itemId)
            {
                ++summary.storyLinkCount;
                summary.labels.push_back("Story arc link: " + (thread.title.empty() ? "<Untitled Arc>" : thread.title));
            }
        }
        for (const StoryBeatModel &beat : thread.beats)
        {
            for (const StoryLinkModel &link : beat.links)
            {
                if (link.targetEntityType == "Item" && link.targetEntityId == itemId)
                {
                    ++summary.storyLinkCount;
                    summary.labels.push_back("Scene link: " + (beat.title.empty() ? "<Untitled Scene>" : beat.title));
                }
            }
        }
    }
    return summary;
}

std::vector<ItemCreatorController::CatalogEntry> ItemCreatorController::getCatalogEntries() const
{
    std::vector<CatalogEntry> entries;
    if (_currentCampaign.ruleSet)
    {
        const std::vector<ItemModel> rulesetItems = _currentCampaign.ruleSet->getItems();
        for (const ItemModel &item : rulesetItems)
            entries.push_back({item, _currentCampaign.ruleSet->getName(), false, -1});
    }

    for (int i = 0; i < static_cast<int>(_currentCampaign.customItems.size()); ++i)
    {
        const ItemModel &item = _currentCampaign.customItems[static_cast<std::size_t>(i)];
        entries.push_back({item, isItemArchived(item) ? "Campaign (Archived)" : "Campaign", true, i});
    }

    std::sort(entries.begin(), entries.end(), [](const CatalogEntry &left, const CatalogEntry &right) {
        const std::string leftCategory = getItemCategory(left.item);
        const std::string rightCategory = getItemCategory(right.item);
        if (leftCategory != rightCategory)
            return leftCategory < rightCategory;
        if (left.item.name != right.item.name)
            return left.item.name < right.item.name;
        return left.source < right.source;
    });
    return entries;
}

std::vector<std::string> ItemCreatorController::getCatalogCategories(const std::vector<CatalogEntry> &entries) const
{
    std::vector<std::string> categories{"All"};
    for (const CatalogEntry &entry : entries)
    {
        const std::string category = getItemCategory(entry.item);
        if (std::find(categories.begin(), categories.end(), category) == categories.end())
            categories.push_back(category);
    }

    std::sort(categories.begin() + 1, categories.end());
    return categories;
}

std::vector<ItemCreatorController::CatalogEntry> ItemCreatorController::getVisibleCatalogEntries(const std::vector<CatalogEntry> &entries, const std::vector<std::string> &categories) const
{
    std::vector<CatalogEntry> visibleEntries;
    const std::string selectedCategory =
        (_selectedCatalogCategoryIndex > 0 && _selectedCatalogCategoryIndex < static_cast<int>(categories.size())) ? categories[_selectedCatalogCategoryIndex] : "All";

    for (const CatalogEntry &entry : entries)
    {
        if (_selectedCatalogSourceIndex == 1 && entry.campaignCustom)
            continue;
        if (_selectedCatalogSourceIndex == 2 && !entry.campaignCustom)
            continue;

        const bool categoryMatches = selectedCategory == "All" || getItemCategory(entry.item) == selectedCategory;
        const bool searchMatches = containsText(entry.item.name, _catalogSearch) ||
                                   containsText(getItemCategory(entry.item), _catalogSearch) ||
                                   containsText(entry.item.itemType, _catalogSearch) ||
                                   containsText(entry.item.description, _catalogSearch) ||
                                   containsText(entry.source, _catalogSearch);
        if (categoryMatches && searchMatches)
            visibleEntries.push_back(entry);
    }

    return visibleEntries;
}

std::vector<std::string> ItemCreatorController::getAvailableCategories() const
{
    std::vector<std::string> categories{"Custom"};
    std::vector<ItemModel> items;
    if (_currentCampaign.ruleSet)
        items = _currentCampaign.ruleSet->getItems();
    items.insert(items.end(), _currentCampaign.customItems.begin(), _currentCampaign.customItems.end());

    for (const auto &item : items)
    {
        const std::string category = getItemCategory(item);
        if (std::find(categories.begin(), categories.end(), category) == categories.end())
            categories.push_back(category);
    }

    std::sort(categories.begin(), categories.end());
    return categories;
}
