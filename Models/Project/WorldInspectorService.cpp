#include "WorldInspectorServiceInternal.hpp"
#include "Models/Map/MapStructuredCreation.hpp"

#include <algorithm>
#include <cmath>

namespace WorldInspector
{
namespace detail
{
void synchronizeGeneratedWorldVisibility(Campaign &campaign)
{
    for (MapModel &map : campaign.maps)
    {
        for (MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (feature.kind == MapGeneratedFeatureKind::Nation ||
                (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.materialId.find("territory.") != std::string::npos))
            {
                const auto nation = std::find_if(campaign.nations.begin(), campaign.nations.end(), [&](const NationModel &candidate) {
                    const auto binding = mapSetupBindingForFeature(map, feature);
                    if (binding.present) return candidate.metadata.id == binding.entityId;
                    return candidate.name == feature.name;
                });
                if (nation != campaign.nations.end())
                {
                    feature.gmOnly = nation->visibility == WorldObjectVisibility::GmOnly;
                    feature.discovered = nation->visibility != WorldObjectVisibility::GmOnly;
                }
            }

            const auto place = std::find_if(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), [&](const WorldPlaceModel &candidate) {
                const auto binding = mapSetupBindingForFeature(map, feature);
                if (binding.present) return candidate.metadata.id == binding.entityId;
                if (candidate.linkedMapId != map.metadata.id)
                    return false;
                return (!candidate.linkedFeatureId.empty() && candidate.linkedFeatureId == feature.id) ||
                       (candidate.linkedFeatureId.empty() && candidate.name == feature.name);
            });
            if (place != campaign.worldPlaces.end())
            {
                feature.gmOnly = place->visibility == WorldObjectVisibility::GmOnly;
                feature.discovered = place->visibility != WorldObjectVisibility::GmOnly;
            }
        }
    }
}

MutationResult result(MutationStatus status, std::string message, EntityId entityId)
{
    return {status, std::move(message), std::move(entityId)};
}

void touch(Campaign &campaign, EntityMetadata &metadata)
{
    metadata.touch();
    campaign.metadata.touch();
}

bool mapExists(const Campaign &campaign, const EntityId &mapId)
{
    return mapId.empty() || std::any_of(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == mapId;
    });
}

const MapModel *findMap(const Campaign &campaign, const EntityId &mapId)
{
    const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(), [&](const MapModel &map) {
        return map.metadata.id == mapId;
    });
    return found == campaign.maps.end() ? nullptr : &*found;
}

const MapGeneratedFeature *findGeneratedFeature(const MapModel &map,
                                                const EntityId &featureId)
{
    const auto found = std::find_if(map.generation.graph.features.begin(),
                                    map.generation.graph.features.end(),
                                    [&](const MapGeneratedFeature &feature) {
                                        return feature.id == featureId;
                                    });
    return found == map.generation.graph.features.end() ? nullptr : &*found;
}

bool generatedFeatureCanBecomePlace(const MapGeneratedFeature &feature)
{
    return feature.kind == MapGeneratedFeatureKind::Settlement ||
           feature.kind == MapGeneratedFeatureKind::Landmark;
}

bool promotionRelevantFeatureStateMatches(const MapModel &draftMap,
                                          const MapGeneratedFeature &draftFeature,
                                          const MapModel &savedMap,
                                          const MapGeneratedFeature &savedFeature)
{
    constexpr float epsilon = 0.0001f;
    if (draftMap.width != savedMap.width || draftMap.height != savedMap.height ||
        draftFeature.id != savedFeature.id || draftFeature.kind != savedFeature.kind ||
        draftFeature.name != savedFeature.name ||
        draftFeature.discovered != savedFeature.discovered ||
        draftFeature.gmOnly != savedFeature.gmOnly ||
        draftFeature.points.empty() || savedFeature.points.empty())
        return false;
    return std::fabs(draftFeature.points.front().x - savedFeature.points.front().x) <= epsilon &&
           std::fabs(draftFeature.points.front().y - savedFeature.points.front().y) <= epsilon;
}

WorldObjectVisibility generatedPlaceVisibility(const MapGeneratedFeature &feature)
{
    if (feature.gmOnly || !feature.discovered)
        return WorldObjectVisibility::GmOnly;
    return WorldObjectVisibility::Discovered;
}

bool worldPlaceStateMatches(const WorldPlaceModel &left,
                            const WorldPlaceModel &right)
{
    constexpr double epsilon = 0.0000001;
    return left.metadata.id == right.metadata.id &&
           left.metadata.createdAtUtc == right.metadata.createdAtUtc &&
           left.metadata.updatedAtUtc == right.metadata.updatedAtUtc &&
           left.metadata.tags == right.metadata.tags &&
           left.metadata.customProperties == right.metadata.customProperties &&
           left.type == right.type && left.name == right.name &&
           left.description == right.description && left.notes == right.notes &&
           left.nationId == right.nationId && left.linkedMapId == right.linkedMapId &&
           left.linkedFeatureId == right.linkedFeatureId &&
           left.hasMapPosition == right.hasMapPosition &&
           std::fabs(left.mapPosition.x - right.mapPosition.x) <= epsilon &&
           std::fabs(left.mapPosition.y - right.mapPosition.y) <= epsilon &&
           left.visibility == right.visibility;
}

bool worldPlaceMatchesGeneratedFeature(const WorldPlaceModel &place,
                                       const MapModel &map,
                                       const MapGeneratedFeature &feature)
{
    if (!generatedFeatureCanBecomePlace(feature) || feature.points.empty() ||
        place.linkedMapId != map.metadata.id || place.linkedFeatureId != feature.id ||
        place.name != feature.name || !place.hasMapPosition ||
        place.visibility != generatedPlaceVisibility(feature))
        return false;
    if ((feature.kind == MapGeneratedFeatureKind::Settlement && place.type != WorldPlaceType::City) ||
        (feature.kind == MapGeneratedFeatureKind::Landmark && place.type != WorldPlaceType::Landmark))
        return false;
    constexpr double epsilon = 0.0000001;
    const double expectedX = normalizedWorldCoordinate(
        static_cast<double>(feature.points.front().x) / std::max(1, map.width));
    const double expectedY = normalizedWorldCoordinate(
        static_cast<double>(feature.points.front().y) / std::max(1, map.height));
    return std::fabs(place.mapPosition.x - expectedX) <= epsilon &&
           std::fabs(place.mapPosition.y - expectedY) <= epsilon;
}
}
using namespace detail;

MutationResult applyWithRollback(Campaign &campaign, const Mutation &mutation, const Persist &persist)
{
    if (!mutation || !persist)
        return result(MutationStatus::Invalid, "The World inspector operation is incomplete.");
    const Campaign snapshot = campaign;
    MutationResult mutationResult = mutation(campaign);
    if (!mutationResult.applied())
    {
        campaign = snapshot;
        return mutationResult;
    }
    if (persist(campaign))
        return mutationResult;
    campaign = snapshot;
    return result(MutationStatus::PersistenceFailed,
                  "The change could not be saved. The previous campaign state was restored in memory.",
                  mutationResult.entityId);
}

ShopModel *findShop(Campaign &campaign, const EntityId &shopId)
{
    auto found = std::find_if(campaign.shops.begin(), campaign.shops.end(), [&](const ShopModel &shop) { return shop.metadata.id == shopId; });
    return found == campaign.shops.end() ? nullptr : &*found;
}

const ShopModel *findShop(const Campaign &campaign, const EntityId &shopId)
{
    auto found = std::find_if(campaign.shops.begin(), campaign.shops.end(), [&](const ShopModel &shop) { return shop.metadata.id == shopId; });
    return found == campaign.shops.end() ? nullptr : &*found;
}

DungeonModel *findDungeon(Campaign &campaign, const EntityId &dungeonId)
{
    auto found = std::find_if(campaign.dungeons.begin(), campaign.dungeons.end(), [&](const DungeonModel &dungeon) { return dungeon.metadata.id == dungeonId; });
    return found == campaign.dungeons.end() ? nullptr : &*found;
}

const DungeonModel *findDungeon(const Campaign &campaign, const EntityId &dungeonId)
{
    auto found = std::find_if(campaign.dungeons.begin(), campaign.dungeons.end(), [&](const DungeonModel &dungeon) { return dungeon.metadata.id == dungeonId; });
    return found == campaign.dungeons.end() ? nullptr : &*found;
}

DungeonRoomModel *findRoom(DungeonModel &dungeon, const EntityId &roomId)
{
    auto found = std::find_if(dungeon.rooms.begin(), dungeon.rooms.end(), [&](const DungeonRoomModel &room) { return room.metadata.id == roomId; });
    return found == dungeon.rooms.end() ? nullptr : &*found;
}

const DungeonRoomModel *findRoom(const DungeonModel &dungeon, const EntityId &roomId)
{
    auto found = std::find_if(dungeon.rooms.begin(), dungeon.rooms.end(), [&](const DungeonRoomModel &room) { return room.metadata.id == roomId; });
    return found == dungeon.rooms.end() ? nullptr : &*found;
}

NationModel *findNation(Campaign &campaign, const EntityId &nationId)
{
    const auto found = std::find_if(campaign.nations.begin(), campaign.nations.end(), [&](const NationModel &nation) { return nation.metadata.id == nationId; });
    return found == campaign.nations.end() ? nullptr : &*found;
}

const NationModel *findNation(const Campaign &campaign, const EntityId &nationId)
{
    const auto found = std::find_if(campaign.nations.begin(), campaign.nations.end(), [&](const NationModel &nation) { return nation.metadata.id == nationId; });
    return found == campaign.nations.end() ? nullptr : &*found;
}

WorldPlaceModel *findWorldPlace(Campaign &campaign, const EntityId &placeId)
{
    const auto found = std::find_if(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), [&](const WorldPlaceModel &place) { return place.metadata.id == placeId; });
    return found == campaign.worldPlaces.end() ? nullptr : &*found;
}

const WorldPlaceModel *findWorldPlace(const Campaign &campaign, const EntityId &placeId)
{
    const auto found = std::find_if(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), [&](const WorldPlaceModel &place) { return place.metadata.id == placeId; });
    return found == campaign.worldPlaces.end() ? nullptr : &*found;
}

const WorldPlaceModel *findWorldPlaceForGeneratedFeature(const Campaign &campaign,
                                                        const EntityId &mapId,
                                                        const EntityId &featureId)
{
    if (const auto *map = findMap(campaign, mapId))
        if (const auto *feature = findGeneratedFeature(*map, featureId))
        {
            const auto binding = mapSetupBindingForFeature(*map, *feature);
            if (binding.present) return findWorldPlace(campaign, binding.entityId);
        }
    const auto found = std::find_if(campaign.worldPlaces.begin(), campaign.worldPlaces.end(),
                                    [&](const WorldPlaceModel &place) {
                                        return !mapId.empty() && !featureId.empty() &&
                                               place.linkedMapId == mapId &&
                                               place.linkedFeatureId == featureId;
                                    });
    return found == campaign.worldPlaces.end() ? nullptr : &*found;
}

GeneratedPlacePromotionEligibility inspectGeneratedPlacePromotion(
    const Campaign &campaign,
    const MapModel &draftMap,
    const EntityId &featureId)
{
    const MapGeneratedFeature *draftFeature = findGeneratedFeature(draftMap, featureId);
    if (!draftFeature)
        return {GeneratedPlacePromotionStatus::FeatureNotSaved,
                "The generated place no longer exists on this map.", {}};
    if (!generatedFeatureCanBecomePlace(*draftFeature))
        return {GeneratedPlacePromotionStatus::UnsupportedFeature,
                "Only generated settlements and landmarks can be added to the campaign world.", {}};
    const auto binding = mapSetupBindingForFeature(draftMap, *draftFeature);
    if (binding.present)
        if (const auto *existing = findWorldPlace(campaign, binding.entityId))
            return {GeneratedPlacePromotionStatus::AlreadyPromoted,
                    "Already in the campaign world.", existing->metadata.id};
    if (const WorldPlaceModel *existing = findWorldPlaceForGeneratedFeature(
            campaign, draftMap.metadata.id, featureId))
        return {GeneratedPlacePromotionStatus::AlreadyPromoted,
                "Already in the campaign world.", existing->metadata.id};
    if (draftFeature->name.empty())
        return {GeneratedPlacePromotionStatus::MissingName,
                "Name this generated place before adding it to the campaign world.", {}};
    if (draftFeature->points.empty())
        return {GeneratedPlacePromotionStatus::MissingPosition,
                "This generated place has no usable map position.", {}};
    const MapModel *savedMap = findMap(campaign, draftMap.metadata.id);
    if (!savedMap)
        return {GeneratedPlacePromotionStatus::MapNotSaved,
                "Save this map before adding its generated places to the campaign world.", {}};
    const MapGeneratedFeature *savedFeature = findGeneratedFeature(*savedMap, featureId);
    if (!savedFeature)
        return {GeneratedPlacePromotionStatus::FeatureNotSaved,
                "Save the latest map changes before adding this generated place.", {}};
    if (!promotionRelevantFeatureStateMatches(draftMap, *draftFeature, *savedMap, *savedFeature))
        return {GeneratedPlacePromotionStatus::DraftChanged,
                "Save the latest map changes before adding this generated place.", {}};
    return {GeneratedPlacePromotionStatus::Eligible,
            "Ready to add to the campaign world.", {}};
}

MutationResult promoteGeneratedPlace(Campaign &campaign,
                                     const MapModel &draftMap,
                                     const EntityId &featureId)
{
    const GeneratedPlacePromotionEligibility eligibility =
        inspectGeneratedPlacePromotion(campaign, draftMap, featureId);
    if (!eligibility.eligible())
    {
        const MutationStatus status =
            eligibility.status == GeneratedPlacePromotionStatus::AlreadyPromoted ||
                    eligibility.status == GeneratedPlacePromotionStatus::DraftChanged
                ? MutationStatus::Conflict
                : (eligibility.status == GeneratedPlacePromotionStatus::MapNotSaved ||
                           eligibility.status == GeneratedPlacePromotionStatus::FeatureNotSaved
                       ? MutationStatus::NotFound
                       : MutationStatus::Invalid);
        return result(status, eligibility.message, eligibility.existingPlaceId);
    }

    const MapModel *savedMap = findMap(campaign, draftMap.metadata.id);
    const MapGeneratedFeature *feature = savedMap ? findGeneratedFeature(*savedMap, featureId) : nullptr;
    if (!savedMap || !feature)
        return result(MutationStatus::NotFound,
                      "Save the latest map changes before adding this generated place.");

    WorldPlaceModel place;
    place.type = feature->kind == MapGeneratedFeatureKind::Settlement
                     ? WorldPlaceType::City
                     : WorldPlaceType::Landmark;
    place.name = feature->name;
    place.linkedMapId = savedMap->metadata.id;
    place.linkedFeatureId = feature->id;
    place.hasMapPosition = true;
    place.mapPosition = {
        static_cast<double>(feature->points.front().x) / std::max(1, savedMap->width),
        static_cast<double>(feature->points.front().y) / std::max(1, savedMap->height)};
    normalizeWorldPlacePosition(place);
    place.visibility = generatedPlaceVisibility(*feature);
    const EntityId placeId = place.metadata.id;
    campaign.worldPlaces.push_back(std::move(place));
    campaign.metadata.touch();
    return result(MutationStatus::Applied,
                  "Generated place added to the campaign world.", placeId);
}

MutationResult removeGeneratedPlacePromotion(Campaign &campaign,
                                             const WorldPlaceModel &expectedPlace)
{
    WorldPlaceModel *current = findWorldPlace(campaign, expectedPlace.metadata.id);
    if (!current)
        return result(MutationStatus::NotFound,
                      "The transferred campaign place no longer exists.",
                      expectedPlace.metadata.id);
    if (!worldPlaceStateMatches(*current, expectedPlace))
        return result(MutationStatus::Conflict,
                      "The transferred campaign place was edited and cannot be removed by map undo.",
                      expectedPlace.metadata.id);
    const MutationResult removed = deleteWorldPlace(campaign, expectedPlace.metadata.id);
    if (!removed.applied())
        return removed;
    return result(MutationStatus::Applied,
                  "The generated place transfer was undone.", expectedPlace.metadata.id);
}

MutationResult restoreGeneratedPlacePromotion(Campaign &campaign,
                                              const WorldPlaceModel &place)
{
    if (place.metadata.id.empty() || place.linkedMapId.empty() || place.linkedFeatureId.empty())
        return result(MutationStatus::Invalid,
                      "The generated place transfer has incomplete identity data.",
                      place.metadata.id);
    if (findWorldPlace(campaign, place.metadata.id) ||
        findWorldPlaceForGeneratedFeature(campaign, place.linkedMapId, place.linkedFeatureId))
        return result(MutationStatus::Conflict,
                      "A campaign place already uses this generated map feature.",
                      place.metadata.id);
    if (!place.nationId.empty() && !findNation(campaign, place.nationId))
        return result(MutationStatus::NotFound,
                      "The selected nation no longer exists.", place.metadata.id);
    const MapModel *map = findMap(campaign, place.linkedMapId);
    const MapGeneratedFeature *feature = map ? findGeneratedFeature(*map, place.linkedFeatureId) : nullptr;
    if (!map || !feature || !worldPlaceMatchesGeneratedFeature(place, *map, *feature))
        return result(MutationStatus::Conflict,
                      "The generated map place changed and cannot be restored by redo.",
                      place.metadata.id);

    campaign.worldPlaces.push_back(place);
    normalizeWorldPlacePosition(campaign.worldPlaces.back());
    campaign.metadata.touch();
    return result(MutationStatus::Applied,
                  "The generated place transfer was restored.", place.metadata.id);
}

} // namespace WorldInspector
