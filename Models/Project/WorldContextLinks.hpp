#pragma once
#include "Models/Project/WorldInspectorService.hpp"
#include <vector>
#include <functional>

namespace WorldContextLinks
{
enum class Kind { Place, Faction, Shop };
struct Link
{
    EntityId sourceId, targetId;
    std::string sourceType, sourceName;
    Kind kind;
};
const char *targetType(Kind kind);
bool supportsSource(const Campaign &campaign, const EntityId &id);
EntityId target(const Campaign &campaign, const EntityId &sourceId, Kind kind);
WorldInspector::MutationResult set(Campaign &campaign, const EntityId &sourceId, Kind kind, const EntityId &targetId);
std::vector<Link> links(const Campaign &campaign);
void remapTargets(Campaign &campaign, const std::function<EntityId(EntityId)> &remap);
}
