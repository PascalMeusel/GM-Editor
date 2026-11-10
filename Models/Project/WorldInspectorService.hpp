#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <functional>
#include <string>

namespace WorldInspector
{
enum class MutationStatus
{
    Applied,
    Invalid,
    NotFound,
    Conflict,
    PersistenceFailed,
    CommittedContextChanged
};

struct MutationResult
{
    MutationStatus status = MutationStatus::Invalid;
    std::string message;
    EntityId entityId;

    bool applied() const { return status == MutationStatus::Applied; }
};

enum class GeneratedPlacePromotionStatus
{
    Eligible,
    AlreadyPromoted,
    UnsupportedFeature,
    MissingName,
    MissingPosition,
    MapNotSaved,
    FeatureNotSaved,
    DraftChanged
};

struct GeneratedPlacePromotionEligibility
{
    GeneratedPlacePromotionStatus status = GeneratedPlacePromotionStatus::UnsupportedFeature;
    std::string message;
    EntityId existingPlaceId;

    bool eligible() const { return status == GeneratedPlacePromotionStatus::Eligible; }
};

using Mutation = std::function<MutationResult(Campaign &)>;
using Persist = std::function<bool(Campaign &)>;

MutationResult applyWithRollback(Campaign &campaign, const Mutation &mutation, const Persist &persist);

ShopModel *findShop(Campaign &campaign, const EntityId &shopId);
const ShopModel *findShop(const Campaign &campaign, const EntityId &shopId);
DungeonModel *findDungeon(Campaign &campaign, const EntityId &dungeonId);
const DungeonModel *findDungeon(const Campaign &campaign, const EntityId &dungeonId);
DungeonRoomModel *findRoom(DungeonModel &dungeon, const EntityId &roomId);
const DungeonRoomModel *findRoom(const DungeonModel &dungeon, const EntityId &roomId);
NationModel *findNation(Campaign &campaign, const EntityId &nationId);
const NationModel *findNation(const Campaign &campaign, const EntityId &nationId);
WorldPlaceModel *findWorldPlace(Campaign &campaign, const EntityId &placeId);
const WorldPlaceModel *findWorldPlace(const Campaign &campaign, const EntityId &placeId);
const WorldPlaceModel *findWorldPlaceForGeneratedFeature(const Campaign &campaign,
                                                        const EntityId &mapId,
                                                        const EntityId &featureId);

GeneratedPlacePromotionEligibility inspectGeneratedPlacePromotion(
    const Campaign &campaign,
    const MapModel &draftMap,
    const EntityId &featureId);
MutationResult promoteGeneratedPlace(Campaign &campaign,
                                     const MapModel &draftMap,
                                     const EntityId &featureId);
MutationResult removeGeneratedPlacePromotion(Campaign &campaign,
                                             const WorldPlaceModel &expectedPlace);
MutationResult restoreGeneratedPlacePromotion(Campaign &campaign,
                                              const WorldPlaceModel &place);

MutationResult createNation(Campaign &campaign, const std::string &name);
MutationResult updateNation(Campaign &campaign, const EntityId &nationId, const NationModel &replacement);
MutationResult deleteNation(Campaign &campaign, const EntityId &nationId);
MutationResult addNationTerritory(Campaign &campaign, const EntityId &nationId, const std::string &name);
MutationResult deleteNationTerritory(Campaign &campaign, const EntityId &nationId, const EntityId &territoryId);
MutationResult createWorldPlace(Campaign &campaign, WorldPlaceType type, const std::string &name, const EntityId &nationId = {});
MutationResult updateWorldPlace(Campaign &campaign, const EntityId &placeId, const WorldPlaceModel &replacement);
MutationResult deleteWorldPlace(Campaign &campaign, const EntityId &placeId);

MutationResult createShop(Campaign &campaign, const std::string &name);
MutationResult updateShop(Campaign &campaign, const EntityId &shopId, const ShopModel &replacement);
MutationResult deleteShop(Campaign &campaign, const EntityId &shopId);
MutationResult addStock(Campaign &campaign, const EntityId &shopId, const EntityId &itemId);
MutationResult updateStock(Campaign &campaign, const EntityId &shopId, std::size_t stockIndex, const ShopStockEntryModel &replacement);
MutationResult removeStock(Campaign &campaign, const EntityId &shopId, std::size_t stockIndex);
MutationResult transferStockToParty(Campaign &campaign, const EntityId &shopId, std::size_t stockIndex, int quantity);
MutationResult transferPartyToShop(Campaign &campaign, const EntityId &shopId, const EntityId &itemId, int quantity);
int partyQuantity(const Campaign &campaign, const EntityId &itemId);
const ItemModel *findAvailableItem(const Campaign &campaign, const EntityId &itemId);

MutationResult createDungeon(Campaign &campaign, const std::string &name, const EntityId &parentMapId = {});
MutationResult updateDungeon(Campaign &campaign, const EntityId &dungeonId, const DungeonModel &replacement);
MutationResult deleteDungeon(Campaign &campaign, const EntityId &dungeonId);
MutationResult addRoom(Campaign &campaign, const EntityId &dungeonId, const std::string &name);
MutationResult updateRoom(Campaign &campaign, const EntityId &dungeonId, const EntityId &roomId, const DungeonRoomModel &replacement);
MutationResult deleteRoom(Campaign &campaign, const EntityId &dungeonId, const EntityId &roomId);
MutationResult addConnection(Campaign &campaign, const EntityId &dungeonId, const EntityId &sourceRoomId, const EntityId &targetRoomId);
MutationResult updateConnection(Campaign &campaign, const EntityId &dungeonId, const EntityId &sourceRoomId,
                                const EntityId &connectionId, const DungeonRoomConnectionModel &replacement);
MutationResult deleteConnection(Campaign &campaign, const EntityId &dungeonId, const EntityId &sourceRoomId, const EntityId &connectionId);
}
