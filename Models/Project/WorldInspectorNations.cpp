#include "WorldInspectorServiceInternal.hpp"

namespace WorldInspector
{
using namespace detail;
MutationResult createNation(Campaign &campaign, const std::string &name)
{
    NationModel nation;
    nation.name = name.empty() ? "New Nation" : name;
    const EntityId id = nation.metadata.id;
    campaign.nations.push_back(std::move(nation));
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "Nation created.", id);
}

MutationResult updateNation(Campaign &campaign, const EntityId &nationId, const NationModel &replacement)
{
    NationModel *nation = findNation(campaign, nationId);
    if (!nation)
        return result(MutationStatus::NotFound, "The selected nation no longer exists.");
    if (replacement.name.empty())
        return result(MutationStatus::Invalid, "A nation name is required.");
    if (!replacement.capitalPlaceId.empty() && !findWorldPlace(campaign, replacement.capitalPlaceId))
        return result(MutationStatus::NotFound, "The selected capital no longer exists.");
    const EntityMetadata metadata = nation->metadata;
    *nation = replacement;
    nation->metadata = metadata;
    for (NationTerritoryModel &territory : nation->territories)
        normalizeNationTerritory(territory);
    synchronizeGeneratedWorldVisibility(campaign);
    touch(campaign, nation->metadata);
    return result(MutationStatus::Applied, "Nation saved.", nationId);
}

MutationResult deleteNation(Campaign &campaign, const EntityId &nationId)
{
    if (std::any_of(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), [&](const WorldPlaceModel &place) { return place.nationId == nationId; }))
        return result(MutationStatus::Conflict, "Move or remove the nation's places before deleting it.", nationId);
    const auto found = std::find_if(campaign.nations.begin(), campaign.nations.end(), [&](const NationModel &nation) { return nation.metadata.id == nationId; });
    if (found == campaign.nations.end())
        return result(MutationStatus::NotFound, "The selected nation no longer exists.");
    campaign.nations.erase(found);
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "Nation deleted.", nationId);
}

MutationResult addNationTerritory(Campaign &campaign, const EntityId &nationId, const std::string &name)
{
    NationModel *nation = findNation(campaign, nationId);
    if (!nation)
        return result(MutationStatus::NotFound, "The selected nation no longer exists.");
    NationTerritoryModel territory;
    territory.name = name.empty() ? "Territory" : name;
    territory.polygon = {{0.2, 0.2}, {0.5, 0.2}, {0.5, 0.5}, {0.2, 0.5}};
    const EntityId id = territory.metadata.id;
    nation->territories.push_back(std::move(territory));
    touch(campaign, nation->metadata);
    return result(MutationStatus::Applied, "Territory added.", id);
}

MutationResult deleteNationTerritory(Campaign &campaign, const EntityId &nationId, const EntityId &territoryId)
{
    NationModel *nation = findNation(campaign, nationId);
    if (!nation)
        return result(MutationStatus::NotFound, "The selected nation no longer exists.");
    const auto found = std::find_if(nation->territories.begin(), nation->territories.end(), [&](const NationTerritoryModel &territory) { return territory.metadata.id == territoryId; });
    if (found == nation->territories.end())
        return result(MutationStatus::NotFound, "The selected territory no longer exists.");
    nation->territories.erase(found);
    touch(campaign, nation->metadata);
    return result(MutationStatus::Applied, "Territory removed.", territoryId);
}

MutationResult createWorldPlace(Campaign &campaign, WorldPlaceType type, const std::string &name, const EntityId &nationId)
{
    if (!nationId.empty() && !findNation(campaign, nationId))
        return result(MutationStatus::NotFound, "The selected nation no longer exists.");
    WorldPlaceModel place;
    place.type = type;
    place.name = name.empty() ? "New Place" : name;
    place.nationId = nationId;
    const EntityId id = place.metadata.id;
    campaign.worldPlaces.push_back(std::move(place));
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "World place created.", id);
}

MutationResult updateWorldPlace(Campaign &campaign, const EntityId &placeId, const WorldPlaceModel &replacement)
{
    WorldPlaceModel *place = findWorldPlace(campaign, placeId);
    if (!place)
        return result(MutationStatus::NotFound, "The selected place no longer exists.");
    if (replacement.name.empty())
        return result(MutationStatus::Invalid, "A place name is required.");
    if (!replacement.nationId.empty() && !findNation(campaign, replacement.nationId))
        return result(MutationStatus::NotFound, "The selected nation no longer exists.");
    if (!replacement.linkedMapId.empty() && !mapExists(campaign, replacement.linkedMapId))
        return result(MutationStatus::NotFound, "The selected map no longer exists.");
    const EntityMetadata metadata = place->metadata;
    *place = replacement;
    place->metadata = metadata;
    normalizeWorldPlacePosition(*place);
    synchronizeGeneratedWorldVisibility(campaign);
    touch(campaign, place->metadata);
    return result(MutationStatus::Applied, "World place saved.", placeId);
}

MutationResult deleteWorldPlace(Campaign &campaign, const EntityId &placeId)
{
    if (std::any_of(campaign.nations.begin(), campaign.nations.end(), [&](const NationModel &nation) { return nation.capitalPlaceId == placeId; }))
        return result(MutationStatus::Conflict, "Choose another capital before deleting this place.", placeId);
    const auto found = std::find_if(campaign.worldPlaces.begin(), campaign.worldPlaces.end(), [&](const WorldPlaceModel &place) { return place.metadata.id == placeId; });
    if (found == campaign.worldPlaces.end())
        return result(MutationStatus::NotFound, "The selected place no longer exists.");
    campaign.worldPlaces.erase(found);
    campaign.metadata.touch();
    return result(MutationStatus::Applied, "World place deleted.", placeId);
}

} // namespace WorldInspector
