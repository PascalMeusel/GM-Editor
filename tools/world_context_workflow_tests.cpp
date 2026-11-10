#include "workflow_test_database.hpp"
#include "Models/Project/WorldContextLinks.hpp"
#include "Models/Project/CampaignReferenceService.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
Campaign read(const EntityId &id)
{
    for (auto campaign : CampaignRepository::loadAllCampaigns()) if (campaign.metadata.id == id) return campaign;
    throw std::runtime_error("Missing campaign");
}
}
int main()
{
    try
    {
        isolateWorkflowDatabase();
        using Kind = WorldContextLinks::Kind;
        auto &save = ProjectSaveService::getInstance(); save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        Campaign campaign; campaign.name = "Generated world context"; campaign.ruleSet = std::make_shared<DND5e>();
        ItemModel item; item.name = "Linked blade"; campaign.customItems.push_back(item);
        WorldPlaceModel place; place.name = "Generated village"; campaign.worldPlaces.push_back(place);
        FactionModel faction; faction.name = "Generated guild"; campaign.factions.push_back(faction);
        ShopModel shop; shop.name = "Generated shop"; campaign.shops.push_back(shop);
        NPCModel npc; npc.name = "Generated merchant"; campaign.npcs.push_back(npc);
        CampaignRepository::saveCampaign(campaign);
        ItemCreatorController editor(campaign); editor.resumeDraft(); require(editor.selectItemById(item.metadata.id), "Item selection failed");
        editor.getModel().description = "Retained before context edit"; editor.markDraftEdited();
        const auto change = [&](Kind kind, const EntityId &target) {
            return WorldInspector::applyWithRollback(campaign,
                [&](Campaign &proposed) { return WorldContextLinks::set(proposed, item.metadata.id, kind, target); },
                [&](Campaign &proposed) { return save.saveCampaign(proposed, "context link test"); });
        };
        require(change(Kind::Place, place.metadata.id).applied() && change(Kind::Faction, faction.metadata.id).applied() &&
            change(Kind::Shop, shop.metadata.id).applied(), "Context assignment failed");
        require(editor.saveItem(), "Retained item save failed");
        auto loaded = read(campaign.metadata.id);
        require(WorldContextLinks::target(loaded, item.metadata.id, Kind::Place) == place.metadata.id &&
            WorldContextLinks::target(loaded, item.metadata.id, Kind::Faction) == faction.metadata.id &&
            WorldContextLinks::target(loaded, item.metadata.id, Kind::Shop) == shop.metadata.id,
            "Retained editor overwrote links or SQLite restart dropped them");
        require(!CampaignReferences::backlinksTo(loaded, place.metadata.id).empty(), "Context backlink missing");
        require(!change(Kind::Place, faction.metadata.id).applied() && !change(Kind::Shop, item.metadata.id).applied(),
            "Wrong type or self-link accepted");
        SQLite::Database control(ApplicationPaths::databasePath().string(), SQLite::OPEN_READWRITE);
        control.exec("CREATE TRIGGER reject_context BEFORE INSERT ON campaign_entity_properties BEGIN SELECT RAISE(ABORT, 'injected link failure'); END;");
        require(!change(Kind::Place, {}).applied() && WorldContextLinks::target(campaign, item.metadata.id, Kind::Place) == place.metadata.id &&
            WorldContextLinks::target(read(campaign.metadata.id), item.metadata.id, Kind::Place) == place.metadata.id,
            "Failed link removal changed memory or disk");
        control.exec("DROP TRIGGER reject_context;");
        require(change(Kind::Place, {}).applied() && editor.saveItem() &&
            WorldContextLinks::target(read(campaign.metadata.id), item.metadata.id, Kind::Place).empty(), "Old draft resurrected removed context link");
        require(WorldContextLinks::set(campaign, npc.metadata.id, Kind::Faction, faction.metadata.id).applied() &&
            campaign.npcs[0].factionId == faction.metadata.id, "NPC faction link diverged from domain membership");
        campaign.shops[0].metadata.tags.push_back("archived");
        require(!change(Kind::Shop, shop.metadata.id).applied(), "Archived target accepted");
        std::cout << "Context links: typed assignment, SQLite restart, backlinks, stale draft merge/removal, failure retry and NPC faction parity passed.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
