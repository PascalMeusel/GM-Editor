#include "Models/Project/WorldInspectorService.hpp"

#include <iostream>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAILED: " << message << '\n';
    return condition;
}

ItemModel makeItem(const std::string &name, bool stackable)
{
    ItemModel item;
    item.name = name;
    item.stackable = stackable;
    return item;
}

bool testShopCrudStockAndTransactions()
{
    Campaign campaign;
    campaign.name = "World test";
    campaign.customItems.push_back(makeItem("Rope", true));
    const EntityId itemId = campaign.customItems.front().metadata.id;

    const WorldInspector::MutationResult created = WorldInspector::createShop(campaign, "Wayfarer's Goods");
    bool ok = require(created.applied() && campaign.shops.size() == 1, "shop creation should add one shop");
    const EntityId shopId = created.entityId;
    ShopModel editedShop = campaign.shops.front();
    editedShop.merchantName = "Mara";
    editedShop.description = "Road supplies";
    ok &= require(WorldInspector::updateShop(campaign, shopId, editedShop).applied() && campaign.shops.front().merchantName == "Mara",
                  "shop details should be editable in place");
    ok &= require(WorldInspector::addStock(campaign, shopId, itemId).applied(), "stock should accept an existing item");
    ok &= require(WorldInspector::addStock(campaign, shopId, itemId).applied(), "adding duplicate stock should increase quantity");
    ok &= require(campaign.shops.front().stock.size() == 1 && campaign.shops.front().stock.front().quantity == 2,
                  "duplicate stock should remain one entry with quantity two");
    ShopStockEntryModel editedStock = campaign.shops.front().stock.front();
    editedStock.priceMultiplier = 1.25;
    editedStock.sellPriceMultiplier = 0.4;
    editedStock.restocks = true;
    ok &= require(WorldInspector::updateStock(campaign, shopId, 0, editedStock).applied() && campaign.shops.front().stock.front().restocks,
                  "stock pricing and availability details should be editable");

    ok &= require(WorldInspector::transferStockToParty(campaign, shopId, 0, 2).applied(), "party purchase should be recorded");
    ok &= require(campaign.shops.front().stock.front().quantity == 0 && WorldInspector::partyQuantity(campaign, itemId) == 2,
                  "purchase should atomically move stock to Party Inventory");
    ok &= require(WorldInspector::transferPartyToShop(campaign, shopId, itemId, 1).applied(), "party sale should be recorded");
    ok &= require(campaign.shops.front().stock.front().quantity == 1 && WorldInspector::partyQuantity(campaign, itemId) == 1,
                  "sale should atomically move stock back to the shop");

    const Campaign beforeInvalid = campaign;
    const WorldInspector::MutationResult invalid = WorldInspector::transferStockToParty(campaign, shopId, 0, 99);
    ok &= require(invalid.status == WorldInspector::MutationStatus::Invalid, "oversized purchase should be rejected");
    ok &= require(campaign.shops.front().stock.front().quantity == beforeInvalid.shops.front().stock.front().quantity &&
                      WorldInspector::partyQuantity(campaign, itemId) == WorldInspector::partyQuantity(beforeInvalid, itemId),
                  "rejected transaction should not partially move inventory");

    ok &= require(WorldInspector::deleteShop(campaign, shopId).applied() && campaign.shops.empty(), "shop deletion should remove the selected shop");
    ok &= require(WorldInspector::partyQuantity(campaign, itemId) == 1, "shop deletion should preserve Party Inventory");
    return ok;
}

bool testDungeonRoomsConnectionsAndMapContext()
{
    Campaign campaign;
    MapModel map;
    map.name = "Crypt Map";
    const EntityId mapId = map.metadata.id;
    campaign.maps.push_back(map);

    const WorldInspector::MutationResult created = WorldInspector::createDungeon(campaign, "Ash Crypt", mapId);
    bool ok = require(created.applied(), "dungeon creation should accept an existing parent map");
    const EntityId dungeonId = created.entityId;
    ok &= require(campaign.dungeons.front().parentMapId == mapId && campaign.dungeons.front().mapIds.size() == 1,
                  "dungeon should retain its map context");

    const EntityId firstRoomId = WorldInspector::addRoom(campaign, dungeonId, "Entry").entityId;
    const EntityId secondRoomId = WorldInspector::addRoom(campaign, dungeonId, "Vault").entityId;
    ok &= require(!firstRoomId.empty() && !secondRoomId.empty(), "room creation should return stable IDs");
    DungeonModel *dungeon = WorldInspector::findDungeon(campaign, dungeonId);
    DungeonRoomModel replacement = *WorldInspector::findRoom(*dungeon, secondRoomId);
    replacement.mapX = 8;
    replacement.mapY = 3;
    replacement.explorationState = DungeonRoomExplorationState::Seen;
    ok &= require(WorldInspector::updateRoom(campaign, dungeonId, secondRoomId, replacement).applied(), "room map structure should be editable");

    const WorldInspector::MutationResult connected = WorldInspector::addConnection(campaign, dungeonId, firstRoomId, secondRoomId);
    ok &= require(connected.applied(), "rooms should support contextual connections");
    ok &= require(WorldInspector::addConnection(campaign, dungeonId, firstRoomId, secondRoomId).status == WorldInspector::MutationStatus::Conflict,
                  "duplicate room connections should be rejected");
    ok &= require(WorldInspector::addConnection(campaign, dungeonId, firstRoomId, firstRoomId).status == WorldInspector::MutationStatus::Invalid,
                  "self connections should be rejected");

    dungeon = WorldInspector::findDungeon(campaign, dungeonId);
    DungeonRoomConnectionModel editedConnection = dungeon->rooms.front().connections.front();
    editedConnection.label = "Iron gate";
    editedConnection.locked = true;
    ok &= require(WorldInspector::updateConnection(campaign, dungeonId, firstRoomId, connected.entityId, editedConnection).applied(),
                  "connection details should be editable");
    ok &= require(WorldInspector::deleteConnection(campaign, dungeonId, firstRoomId, connected.entityId).applied(),
                  "connection deletion should be supported");
    ok &= require(WorldInspector::addConnection(campaign, dungeonId, firstRoomId, secondRoomId).applied(),
                  "connection should be addable again after deletion");

    ok &= require(WorldInspector::deleteRoom(campaign, dungeonId, secondRoomId).applied(), "room deletion should succeed");
    dungeon = WorldInspector::findDungeon(campaign, dungeonId);
    ok &= require(dungeon && dungeon->rooms.size() == 1 && dungeon->rooms.front().connections.empty(),
                  "room deletion should remove inbound connections");
    return ok;
}

bool testPersistenceRollbackSeam()
{
    Campaign campaign;
    const EntityId originalCampaignId = campaign.metadata.id;
    const WorldInspector::MutationResult failed = WorldInspector::applyWithRollback(
        campaign,
        [](Campaign &value) { return WorldInspector::createShop(value, "Transient Shop"); },
        [](Campaign &) { return false; });
    bool ok = require(failed.status == WorldInspector::MutationStatus::PersistenceFailed, "failed persistence should be reported distinctly");
    ok &= require(campaign.shops.empty(), "failed persistence should restore the complete in-memory campaign");
    ok &= require(campaign.metadata.id == originalCampaignId, "rollback should preserve campaign identity");
    const WorldInspector::MutationResult invalid = WorldInspector::applyWithRollback(
        campaign,
        [](Campaign &value) {
            value.name = "partially changed";
            return WorldInspector::MutationResult{WorldInspector::MutationStatus::Invalid, "invalid", {}};
        },
        [](Campaign &) { return true; });
    ok &= require(invalid.status == WorldInspector::MutationStatus::Invalid && campaign.name.empty(),
                  "any rejected mutation should roll back partial in-memory changes before persistence");

    const WorldInspector::MutationResult saved = WorldInspector::applyWithRollback(
        campaign,
        [](Campaign &value) { return WorldInspector::createDungeon(value, "Saved Dungeon"); },
        [](Campaign &) { return true; });
    ok &= require(saved.applied() && campaign.dungeons.size() == 1, "successful persistence should keep the mutation");
    return ok;
}

bool testNationAndPositionedPlaceAuthoring()
{
    Campaign campaign;
    MapModel worldMap;
    worldMap.name = "Known World";
    worldMap.atlasLevel = MapLevel::World;
    const EntityId mapId = worldMap.metadata.id;
    campaign.maps.push_back(worldMap);

    const WorldInspector::MutationResult nationCreated = WorldInspector::createNation(campaign, "Northern Compact");
    bool ok = require(nationCreated.applied() && campaign.nations.size() == 1, "nation creation should add a first-class nation");
    const WorldInspector::MutationResult placeCreated = WorldInspector::createWorldPlace(campaign, WorldPlaceType::Capital, "Aurora Gate", nationCreated.entityId);
    ok &= require(placeCreated.applied() && campaign.worldPlaces.size() == 1, "capital creation should link to an existing nation");

    WorldPlaceModel replacement = campaign.worldPlaces.front();
    replacement.linkedMapId = mapId;
    replacement.hasMapPosition = true;
    replacement.mapPosition = {-0.25, 1.4};
    ok &= require(WorldInspector::updateWorldPlace(campaign, placeCreated.entityId, replacement).applied(), "positioned world place should be editable");
    ok &= require(campaign.worldPlaces.front().mapPosition.x == 0.0 && campaign.worldPlaces.front().mapPosition.y == 1.0,
                  "world place map anchors should normalize into map bounds");

    NationModel nation = campaign.nations.front();
    nation.capitalPlaceId = placeCreated.entityId;
    ok &= require(WorldInspector::updateNation(campaign, nationCreated.entityId, nation).applied(), "nation should accept an existing capital");
    ok &= require(WorldInspector::deleteWorldPlace(campaign, placeCreated.entityId).status == WorldInspector::MutationStatus::Conflict,
                  "a capital should not be deleted until reassigned");
    return ok;
}

bool testGeneratedPlacePromotionAndTransactionalHistory()
{
    MapModel worldMap;
    worldMap.name = "Generated world";
    worldMap.width = 100;
    worldMap.height = 50;
    worldMap.atlasLevel = MapLevel::World;

    MapGeneratedFeature settlement;
    settlement.id = "generated_settlement";
    settlement.kind = MapGeneratedFeatureKind::Settlement;
    settlement.name = "Rivergate";
    settlement.points = {{25.0f, 10.0f}};
    settlement.discovered = true;
    settlement.gmOnly = false;
    worldMap.generation.graph.features.push_back(settlement);

    MapGeneratedFeature landmark;
    landmark.id = "generated_landmark";
    landmark.kind = MapGeneratedFeatureKind::Landmark;
    landmark.name = "Silent Tower";
    landmark.points = {{75.0f, 40.0f}};
    landmark.discovered = false;
    landmark.gmOnly = true;
    worldMap.generation.graph.features.push_back(landmark);

    Campaign campaign;
    bool ok = true;
    const auto unsaved = WorldInspector::inspectGeneratedPlacePromotion(
        campaign, worldMap, settlement.id);
    ok &= require(unsaved.status == WorldInspector::GeneratedPlacePromotionStatus::MapNotSaved,
                  "an unsaved generated map should not produce a dangling campaign-place link");

    campaign.maps.push_back(worldMap);
    const auto eligible = WorldInspector::inspectGeneratedPlacePromotion(
        campaign, worldMap, settlement.id);
    ok &= require(eligible.eligible(),
                  "a named generated settlement on the saved draft should be eligible for promotion");

    MapModel namelessDraft = worldMap;
    namelessDraft.generation.graph.features.front().name.clear();
    const auto missingName = WorldInspector::inspectGeneratedPlacePromotion(
        campaign, namelessDraft, settlement.id);
    ok &= require(missingName.status ==
                          WorldInspector::GeneratedPlacePromotionStatus::MissingName,
                  "a nameless generated place should require a name before promotion");

    MapModel changedDraft = worldMap;
    changedDraft.generation.graph.features.front().points.front().x += 1.0f;
    const auto changed = WorldInspector::inspectGeneratedPlacePromotion(
        campaign, changedDraft, settlement.id);
    ok &= require(changed.status == WorldInspector::GeneratedPlacePromotionStatus::DraftChanged,
                  "unsaved promotion-relevant geometry must be rejected until the map is saved");

    const WorldInspector::MutationResult promoted =
        WorldInspector::promoteGeneratedPlace(campaign, worldMap, settlement.id);
    ok &= require(promoted.applied() && campaign.worldPlaces.size() == 1,
                  "eligible generated settlements should create one campaign place");
    const WorldPlaceModel promotionReceipt = campaign.worldPlaces.front();
    ok &= require(promotionReceipt.type == WorldPlaceType::City &&
                      promotionReceipt.name == settlement.name &&
                      promotionReceipt.linkedMapId == worldMap.metadata.id &&
                      promotionReceipt.linkedFeatureId == settlement.id,
                  "the promoted city should keep its semantic type and stable map-feature link");
    ok &= require(promotionReceipt.hasMapPosition &&
                      promotionReceipt.mapPosition.x > 0.249999 &&
                      promotionReceipt.mapPosition.x < 0.250001 &&
                      promotionReceipt.mapPosition.y > 0.199999 &&
                      promotionReceipt.mapPosition.y < 0.200001,
                  "the promoted place should store its normalized feature position");
    ok &= require(promotionReceipt.visibility == WorldObjectVisibility::Discovered,
                  "a discovered generated place should retain discovered visibility");
    ok &= require(WorldInspector::findWorldPlaceForGeneratedFeature(
                      campaign, worldMap.metadata.id, settlement.id) != nullptr,
                  "the stable source pair should resolve the promoted campaign place");

    const auto alreadyPromoted = WorldInspector::inspectGeneratedPlacePromotion(
        campaign, worldMap, settlement.id);
    ok &= require(alreadyPromoted.status ==
                          WorldInspector::GeneratedPlacePromotionStatus::AlreadyPromoted &&
                      alreadyPromoted.existingPlaceId == promotionReceipt.metadata.id,
                  "a generated feature can only be promoted once");
    ok &= require(WorldInspector::promoteGeneratedPlace(
                      campaign, worldMap, settlement.id).status ==
                          WorldInspector::MutationStatus::Conflict &&
                      campaign.worldPlaces.size() == 1,
                  "a duplicate promotion should be rejected without adding another place");

    Campaign failedPersistenceCampaign;
    failedPersistenceCampaign.maps.push_back(worldMap);
    const auto failedPersistence = WorldInspector::applyWithRollback(
        failedPersistenceCampaign,
        [&](Campaign &value) {
            return WorldInspector::promoteGeneratedPlace(
                value, worldMap, settlement.id);
        },
        [](Campaign &) { return false; });
    ok &= require(failedPersistence.status ==
                          WorldInspector::MutationStatus::PersistenceFailed &&
                      failedPersistenceCampaign.worldPlaces.empty(),
                  "a failed promotion save should restore the complete campaign state");

    campaign.worldPlaces.front().name = "Edited elsewhere";
    ok &= require(WorldInspector::removeGeneratedPlacePromotion(
                      campaign, promotionReceipt).status ==
                          WorldInspector::MutationStatus::Conflict &&
                      campaign.worldPlaces.size() == 1,
                  "map undo must not delete a campaign place edited in another workspace");
    campaign.worldPlaces.front() = promotionReceipt;
    ok &= require(WorldInspector::removeGeneratedPlacePromotion(
                      campaign, promotionReceipt).applied() &&
                      campaign.worldPlaces.empty(),
                  "an unchanged generated-place transfer should be removable as one transaction");
    ok &= require(WorldInspector::restoreGeneratedPlacePromotion(
                      campaign, promotionReceipt).applied() &&
                      campaign.worldPlaces.front().metadata.id == promotionReceipt.metadata.id,
                  "redo should restore the same campaign-place identity and linkage");

    ok &= require(WorldInspector::removeGeneratedPlacePromotion(
                      campaign, promotionReceipt).applied(),
                  "the restored transfer should remain undoable");
    campaign.maps.front().generation.graph.features.front().name = "Changed map place";
    ok &= require(WorldInspector::restoreGeneratedPlacePromotion(
                      campaign, promotionReceipt).status ==
                          WorldInspector::MutationStatus::Conflict &&
                      campaign.worldPlaces.empty(),
                  "redo should fail closed when the generated source changed");

    const MapModel currentDraft = campaign.maps.front();
    const WorldInspector::MutationResult landmarkPromoted =
        WorldInspector::promoteGeneratedPlace(campaign, currentDraft, landmark.id);
    ok &= require(landmarkPromoted.applied() && campaign.worldPlaces.size() == 1 &&
                      campaign.worldPlaces.front().type == WorldPlaceType::Landmark &&
                      campaign.worldPlaces.front().visibility == WorldObjectVisibility::GmOnly,
                  "a hidden landmark should become a linked GM-only campaign landmark");

    return ok;
}
}

int main()
{
    bool ok = true;
    ok &= testShopCrudStockAndTransactions();
    ok &= testDungeonRoomsConnectionsAndMapContext();
    ok &= testPersistenceRollbackSeam();
    ok &= testNationAndPositionedPlaceAuthoring();
    ok &= testGeneratedPlacePromotionAndTransactionalHistory();
    if (ok)
        std::cout << "World inspector tests passed.\n";
    return ok ? 0 : 1;
}
