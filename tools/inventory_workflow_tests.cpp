#include "workflow_test_database.hpp"
#include "Controllers/Editor/Widgets/InventoryManager/InventoryManagerController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
CharacterModel &character(Campaign &campaign, const EntityId &id)
{
    for (auto &entry : campaign.characters) if (entry.metadata.id == id) return entry;
    throw std::runtime_error("Fixture owner missing");
}
Campaign readCampaign(const EntityId &id)
{
    for (const auto &campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Persisted campaign missing");
}
}

struct InventoryWorkflowProbe
{
    static void run()
    {
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        Campaign campaign;
        campaign.name = "Generated inventory workflow fixture";
        campaign.ruleSet = std::make_shared<DND5e>();
        CharacterModel first, second;
        std::snprintf(first.nameBuffer, sizeof(first.nameBuffer), "%s", "First owner");
        std::snprintf(second.nameBuffer, sizeof(second.nameBuffer), "%s", "Second owner");
        const auto firstId = first.metadata.id, secondId = second.metadata.id;
        campaign.characters = {first, second};
        ItemModel item;
        item.name = "Stable test item";
        item.stackable = true;
        const auto itemId = item.metadata.id;
        campaign.customItems.push_back(item);
        CampaignRepository::saveCampaign(campaign);
        InventoryManagerController inventory(campaign);
        require(inventory.focusItemForAssignment(itemId, firstId), "Assignment did not resolve item and owner IDs");
        inventory.addSelectedItem();
        inventory.toggleEquipped(0);
        require(inventory.persistInventory(), "Initial inventory save failed");
        auto disk = readCampaign(campaign.metadata.id);
        require(character(disk, firstId).inventory.entries.size() == 1 && character(disk, firstId).inventory.entries[0].equipped,
            "Assign/equip did not persist to the intended owner");

        std::reverse(campaign.characters.begin(), campaign.characters.end());
        ItemModel earlier;
        earlier.name = "AAAA earlier catalog entry";
        campaign.customItems.insert(campaign.customItems.begin(), earlier);
        inventory.addSelectedItem();
        require(character(campaign, firstId).inventory.entries[0].quantity == 2 && character(campaign, secondId).inventory.entries.empty(),
            "Owner/catalog reordering redirected an assignment");
        inventory._itemSearch = "AAAA";
        inventory.addSelectedItem();
        require(character(campaign, firstId).inventory.entries[0].quantity == 2,
            "Filtering silently selected a different catalog item");
        inventory._itemSearch.clear();
        require(inventory.selectCatalogItem(itemId), "Could not restore the selected catalog item");

        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_inventory_commit BEFORE INSERT ON custom_items "
                     "BEGIN SELECT RAISE(ABORT, 'injected inventory commit failure'); END;");
        require(!inventory.persistInventory(), "Injected inventory failure was reported as committed");
        disk = readCampaign(campaign.metadata.id);
        require(character(campaign, firstId).inventory.entries[0].quantity == 2 && character(disk, firstId).inventory.entries[0].quantity == 1,
            "Failed inventory save lost the draft or changed the database");
        control.exec("DROP TRIGGER reject_inventory_commit;");
        require(inventory.persistInventory(), "Inventory retry failed");
        disk = readCampaign(campaign.metadata.id);
        require(character(disk, firstId).inventory.entries[0].quantity == 2, "Retry duplicated or lost an inventory mutation");

        inventory.selectInventoryEntry(0);
        const auto entryId = character(campaign, firstId).inventory.entries[0].metadata.id;
        InventoryEntryModel otherEntry(earlier.metadata.id);
        character(campaign, firstId).inventory.entries.insert(character(campaign, firstId).inventory.entries.begin(), otherEntry);
        require(inventory.getSelectedInventory() && inventory._selectedInventoryEntryIndex == 1 &&
            inventory._selectedInventoryEntryId == entryId, "Entry reordering redirected selection");
        character(campaign, firstId).inventory.entries.erase(character(campaign, firstId).inventory.entries.begin()+1);
        require(inventory.getSelectedInventory() && inventory._selectedInventoryEntryIndex == -1,
            "Deleted inventory entry selected a different row");

        character(campaign, firstId).metadata.tags.push_back("archived");
        inventory.addSelectedItem();
        require(!inventory.getSelectedInventory() && character(campaign, firstId).inventory.entries.size() == 1,
            "Archived owner remained an active mutation target");
        campaign.characters.erase(std::remove_if(campaign.characters.begin(), campaign.characters.end(),
            [&](const auto &entry) { return entry.metadata.id == firstId; }), campaign.characters.end());
        inventory.addSelectedItem();
        require(!inventory.getSelectedInventory() && character(campaign, secondId).inventory.entries.empty(),
            "Deleted owner redirected mutation to another character");
        require(inventory.focusItemForAssignment(itemId, secondId), "Could not recover by explicitly choosing a surviving owner");
        inventory.addSelectedItem();
        require(character(campaign, secondId).inventory.entries.size() == 1, "Recovered assignment failed");
    }
};

int main()
{
    try
    {
        isolateWorkflowDatabase();
        InventoryWorkflowProbe::run();
        std::cout << "Actual inventory controller/SQLite workflow passed: assign/equip, stable owner/item/entry IDs, filtering, failure/retry, archive/delete recovery.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
