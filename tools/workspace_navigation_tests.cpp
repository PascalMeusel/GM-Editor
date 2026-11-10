#include "Models/UX/WorkspaceNavigation.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main()
{
    using namespace gm::ux;
    try
    {
        WorkspaceNavigation navigation;
        navigation.reset("campaign-one");
        navigation.current().sessionId = "session-one";
        navigation.current().planEntryId = "plan-entry-two";
        navigation.current().sceneId = "scene-three";
        navigation.current().filter = "prepared scenes";
        require(!navigation.navigate(WorkspaceTarget::World, true, [] { return false; }),
            "A failed preparation save must prevent leaving the scene");
        require(navigation.current().workspace == WorkspaceTarget::Home &&
            navigation.current().planEntryId == "plan-entry-two" && !navigation.returnDestination(),
            "Failed navigation changed context or return history");
        require(navigation.navigate(WorkspaceTarget::World, true, [] { return true; }), "Retry did not navigate");
        navigation.current().selectedEntityId = "map-one";
        navigation.current().selectedEntityType = "Map";
        navigation.current().filter = "river";
        navigation.current().category = 1;
        navigation.current().editorRoute = "show:map_creator";
        require(navigation.navigate(WorkspaceTarget::Compendium, true, {}), "Library visit failed");
        navigation.current().filter = "healing";
        require(!navigation.returnToPrevious([] { return false; }), "Failed return guard was ignored");
        require(navigation.current().filter == "healing" && navigation.returnDestination()->selectedEntityId == "map-one",
            "Failed return destroyed the library state or map destination");
        require(navigation.returnToPrevious({}), "Return to map failed");
        require(navigation.current().filter == "river" && navigation.current().category == 1 &&
            navigation.current().selectedEntityId == "map-one" && navigation.current().editorRoute == "show:map_creator",
            "Map selection, filters or editor were lost on library return");
        require(navigation.returnToPrevious({}), "Return to session failed");
        require(navigation.current().sessionId == "session-one" && navigation.current().planEntryId == "plan-entry-two" &&
            navigation.current().sceneId == "scene-three", "Session/plan/scene identity changed during visits");
        require(navigation.location(WorkspaceTarget::Compendium).filter == "healing", "Library filter was not retained");

        require(navigation.navigate(WorkspaceTarget::Compendium, true, {}), "Open library failed");
        navigation.current().editorRoute = "show:item_creator";
        navigation.current().selectedEntityId = "item-draft";
        require(navigation.navigate(WorkspaceTarget::Compendium, true, {}), "Same-workspace specialist visit failed");
        navigation.current().editorRoute = "show:spell_library";
        navigation.current().selectedEntityId = "spell-reference";
        require(navigation.returnToPrevious({}) && navigation.current().editorRoute == "show:item_creator" &&
            navigation.current().selectedEntityId == "item-draft", "Same-workspace return lost the item draft destination");
        require(navigation.returnToPrevious({}), "Return from library to session failed");

        require(navigation.navigate(WorkspaceTarget::World, true, {}), "Revisit failed");
        navigation.reconcile([](const std::string &, const EntityId &id) { return id != "map-one"; });
        require(navigation.current().selectedEntityId.empty() && navigation.current().editorRoute.empty() &&
            navigation.current().targetUnavailable && navigation.current().filter == "river",
            "Deleted target must yield a recoverable empty selection without dropping filters");
        navigation.reconcile([](const std::string &, const EntityId &id) { return id != "session-one"; });
        require(navigation.returnToPrevious({}) && navigation.current().sessionId.empty() &&
            navigation.current().planEntryId.empty() && navigation.current().sceneId.empty() && navigation.current().sessionUnavailable,
            "A deleted session left stale child IDs in return navigation");
        navigation.reset("campaign-two", WorkspaceTarget::Cast);
        require(navigation.campaignId() == "campaign-two" && !navigation.returnDestination() &&
            navigation.location(WorkspaceTarget::World).filter.empty() && navigation.current().selectedEntityId.empty(),
            "Switching campaigns leaked navigation state");

        Campaign campaign;
        require(resolveNavigationEntity(campaign, campaign.partyInventory.metadata.id).available,
            "Party inventory must be an explicit, resolvable owner");
        ItemModel first, second;
        first.name = "First";
        second.name = "Second";
        const auto firstId = first.metadata.id;
        campaign.customItems = {first, second};
        require(resolveNavigationEntity(campaign, firstId).available, "Current campaign item did not resolve");
        std::reverse(campaign.customItems.begin(), campaign.customItems.end());
        require(resolveNavigationEntity(campaign, firstId).available, "Reordering redirected a stable item ID");
        campaign.customItems.back().metadata.tags.push_back("archived");
        const auto archived = resolveNavigationEntity(campaign, firstId);
        require(archived.type == "Item" && !archived.available, "Archived item remained an active target");
        campaign.customItems.pop_back();
        require(resolveNavigationEntity(campaign, firstId).type.empty(), "Deleted item resolved to its former list position");
        require(resolveNavigationEntity(campaign, second.metadata.id).available, "Deleting another item removed the survivor");
        RandomTableModel table,otherTable;table.name="Table to open";otherTable.name="Other table";
        const auto tableId=table.metadata.id;campaign.randomTables={table,otherTable};
        const auto tableTarget=resolveNavigationEntity(campaign,tableId);
        require(tableTarget.type=="Random Table"&&tableTarget.available,"Current random table was rejected before its editor route");
        std::reverse(campaign.randomTables.begin(),campaign.randomTables.end());
        require(resolveNavigationEntity(campaign,tableId).available,"Reordering changed a random table's stable target");
        campaign.randomTables.back().metadata.tags.push_back("archived");
        const auto archivedTable=resolveNavigationEntity(campaign,tableId);
        require(archivedTable.type=="Random Table"&&!archivedTable.available,"Archived random table remained an active target");
        campaign.randomTables.pop_back();
        require(resolveNavigationEntity(campaign,tableId).type.empty()&&resolveNavigationEntity(campaign,otherTable.metadata.id).available,
            "Deleted random table resolved to another table or removed the survivor");
        std::cout << "Workspace navigation: save retry, nested returns, filters, stable IDs, archive/delete and campaign reset passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
