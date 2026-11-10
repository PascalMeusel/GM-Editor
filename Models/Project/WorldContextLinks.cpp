#include "Models/Project/WorldContextLinks.hpp"
#include "Models/Project/CampaignTagService.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include <stdexcept>

namespace WorldContextLinks
{
namespace
{
const char *key(Kind kind)
{
    switch (kind)
    {
    case Kind::Place: return "gm.context.place";
    case Kind::Faction: return "gm.context.faction";
    case Kind::Shop: return "gm.context.shop";
    }
    throw std::invalid_argument("Unknown context link kind");
}
}
const char *targetType(Kind kind)
{
    switch (kind)
    {
    case Kind::Place: return "Place";
    case Kind::Faction: return "Faction";
    case Kind::Shop: return "Shop";
    }
    throw std::invalid_argument("Unknown context link kind");
}
EntityId target(const Campaign &campaign, const EntityId &sourceId, Kind kind)
{
    if (kind == Kind::Faction)
        for (const auto &npc : campaign.npcs)
            if (npc.metadata.id == sourceId) return npc.factionId;
    const auto *metadata = CampaignTags::findEntityMetadata(campaign, sourceId);
    if (!metadata) return {};
    const auto found = metadata->customProperties.find(key(kind));
    return found == metadata->customProperties.end() ? EntityId{} : found->second;
}
bool supportsSource(const Campaign &campaign, const EntityId &id)
{
    const auto resolved = gm::ux::resolveNavigationEntity(campaign, id);
    return resolved.available && CampaignTags::findEntityMetadata(campaign, id) &&
        (resolved.type == "Player" || resolved.type == "NPC" || resolved.type == "Item" ||
         resolved.type == "Place" || resolved.type == "Faction" || resolved.type == "Shop" ||
         resolved.type == "Map" || resolved.type == "Nation" || resolved.type == "Encounter");
}
WorldInspector::MutationResult set(Campaign &campaign, const EntityId &sourceId, Kind kind, const EntityId &targetId)
{
    using Status = WorldInspector::MutationStatus;
    auto *metadata = CampaignTags::findEntityMetadata(campaign, sourceId);
    if (!metadata || !supportsSource(campaign, sourceId))
        return {Status::NotFound, "Source entry is unavailable.", sourceId};
    if (!targetId.empty())
    {
        const auto resolved = gm::ux::resolveNavigationEntity(campaign, targetId);
        if (sourceId == targetId || !resolved.available || resolved.type != targetType(kind))
            return {Status::Invalid, "Linked entry is unavailable or has the wrong type.", sourceId};
    }
    // NPC faction membership already has a domain field. Keep every existing
    // consumer of that field synchronized instead of introducing a parallel link.
    bool existingDomainField = false;
    if (kind == Kind::Faction)
        for (auto &npc : campaign.npcs)
            if (npc.metadata.id == sourceId) { npc.factionId = targetId; existingDomainField = true; }
    if (targetId.empty() || existingDomainField) metadata->customProperties.erase(key(kind));
    else metadata->customProperties[key(kind)] = targetId;
    metadata->touch();
    return {Status::Applied, {}, sourceId};
}
std::vector<Link> links(const Campaign &campaign)
{
    std::vector<Link> result;
    const auto append = [&](const EntityMetadata &metadata, const char *type, const std::string &name) {
        for (const auto kind : {Kind::Place, Kind::Faction, Kind::Shop})
        {
            const auto found = metadata.customProperties.find(key(kind));
            if (found != metadata.customProperties.end() && !found->second.empty())
                result.push_back({metadata.id, found->second, type, name, kind});
        }
    };
    for (const auto &entry : campaign.characters) append(entry.metadata, "Player", entry.nameBuffer);
    const auto collection = [&](const auto &entries, const char *type) {
        for (const auto &entry : entries) append(entry.metadata, type, entry.name);
    };
    collection(campaign.npcs, "NPC"); collection(campaign.customItems, "Item");
    collection(campaign.worldPlaces, "World Place"); collection(campaign.factions, "Faction");
    collection(campaign.shops, "Shop"); collection(campaign.maps, "Map");
    collection(campaign.nations, "Nation"); collection(campaign.encounters, "Encounter");
    return result;
}
void remapTargets(Campaign &campaign, const std::function<EntityId(EntityId)> &remap)
{
    for (const auto &link : links(campaign))
        if (auto *metadata = CampaignTags::findEntityMetadata(campaign, link.sourceId))
            metadata->customProperties[key(link.kind)] = remap(link.targetId);
}
}
