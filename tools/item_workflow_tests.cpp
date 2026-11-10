#include "workflow_test_database.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
Campaign readCampaign(const EntityId &id)
{
    for (const auto &campaign : CampaignRepository::loadAllCampaigns())
        if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Campaign was not persisted");
}
}

int main()
{
    try
    {
        isolateWorkflowDatabase();
        auto &save = ProjectSaveService::getInstance();
        save.setBackupSnapshotsEnabled(false);
        save.setRecoverySnapshotsEnabled(false);
        Campaign campaign;
        campaign.name = "Generated item workflow fixture";
        campaign.ruleSet = std::make_shared<DND5e>();
        CampaignRepository::saveCampaign(campaign);
        ItemCreatorController editor(campaign);
        editor.resumeDraft();
        editor.getModel().name = "Test blade";
        editor.getModel().baseValue = 123;
        require(editor.saveItem(), "Could not create the generated item");
        const EntityId firstId = editor.getModel().metadata.id;
        require(campaign.customItems.size() == 1 && readCampaign(campaign.metadata.id).customItems[0].metadata.id == firstId,
            "Creation did not preserve the same campaign item identity on disk");

        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_item_write BEFORE INSERT ON custom_items "
                     "BEGIN SELECT RAISE(ABORT, 'injected item commit failure'); END;");
        editor.getModel().baseValue = 456;
        require(!editor.saveItem(), "Injected commit failure was reported as saved");
        require(editor.getModel().baseValue == 456 && campaign.customItems[0].baseValue == 123 &&
            readCampaign(campaign.metadata.id).customItems[0].baseValue == 123,
            "Failed save lost the draft or changed committed campaign content");
        editor.hide();
        editor.resumeDraft();
        require(editor.getModel().metadata.id == firstId && editor.getModel().baseValue == 456,
            "Library visit discarded the failed item draft");
        control.exec("DROP TRIGGER reject_item_write;");
        require(editor.saveItem() && campaign.customItems.size() == 1 &&
            readCampaign(campaign.metadata.id).customItems[0].baseValue == 456,
            "Retry did not update exactly the original item");

        editor.copyItemAsDraft(editor.getModel());
        const EntityId secondId = editor.getModel().metadata.id;
        require(secondId != firstId && editor.getModel().baseValue == 456, "Copy lost content or reused the source identity");
        require(editor.saveItem() && campaign.customItems.size() == 2, "Copy was not independently persisted");
        require(editor.selectItemById(firstId), "Could not select the original item");
        editor.getModel().description = "Retained while browsing another item";
        require(editor.selectItemById(secondId) && editor.selectItemById(firstId) &&
            editor.getModel().description == "Retained while browsing another item",
            "Changing the selected item discarded a retained draft");
        std::reverse(campaign.customItems.begin(), campaign.customItems.end());
        require(editor.saveItem(), "Save after list reordering failed");
        const auto original = std::find_if(campaign.customItems.begin(), campaign.customItems.end(),
            [&](const auto &item) { return item.metadata.id == firstId; });
        require(original != campaign.customItems.end() && original->description == "Retained while browsing another item",
            "List reordering redirected an edit to a different item");

        original->metadata.tags.push_back("archived");
        CampaignRepository::saveCampaign(campaign);
        require(!editor.saveItem(), "A stale draft silently reversed external archival");
        campaign.customItems.erase(std::remove_if(campaign.customItems.begin(), campaign.customItems.end(),
            [&](const auto &item) { return item.metadata.id == firstId; }), campaign.customItems.end());
        CampaignRepository::saveCampaign(campaign);
        require(!editor.saveItem() && readCampaign(campaign.metadata.id).customItems.size() == 1,
            "Saving a deleted target resurrected it or overwrote another item");
        editor.copyItemAsDraft(editor.getModel());
        require(editor.getModel().metadata.id != firstId && editor.saveItem() && campaign.customItems.size() == 2,
            "Retained unavailable draft could not be explicitly copied to a new identity");
        std::cout << "Actual item controller and SQLite workflow passed: create, failed commit/retry, resume, copy, retained edits, reorder, archive and deleted-target recovery.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
