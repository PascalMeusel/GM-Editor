#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "Models/Map/MapAtlasServiceInternal.hpp"

namespace MapAtlasServiceInternal
{
bool rebasePreviewContractMatches(const MapAtlasRebasePreview &provided,
                                  const MapAtlasRebasePreview &fresh)
{
    if (!provided.valid || !fresh.valid || provided.childMapId != fresh.childMapId ||
        provided.parentMapId != fresh.parentMapId ||
        provided.expectedChildRevision != fresh.expectedChildRevision ||
        provided.expectedParentRevision != fresh.expectedParentRevision ||
        provided.expectedChildContentFingerprint != fresh.expectedChildContentFingerprint ||
        provided.expectedParentContentFingerprint != fresh.expectedParentContentFingerprint ||
        provided.items.size() != fresh.items.size())
        return false;
    for (std::size_t index = 0; index < provided.items.size(); ++index)
    {
        const MapAtlasRebaseItem &left = provided.items[index];
        const MapAtlasRebaseItem &right = fresh.items[index];
        if (left.id != right.id || left.kind != right.kind ||
            left.sourceEntityId != right.sourceEntityId ||
            left.localEntityId != right.localEntityId || left.change != right.change ||
            left.requiresDecision != right.requiresDecision)
            return false;
    }
    return true;
}

std::string atlasConstraintSourceId(const std::string &constraintId)
{
    constexpr const char *prefix = "atlas-edge::";
    constexpr std::size_t prefixLength = 12;
    if (constraintId.rfind(prefix, 0) != 0)
        return {};
    std::size_t separator = std::string::npos;
    const char *edgeMarkers[] = {"@north_", "@east_", "@south_", "@west_"};
    for (const char *marker : edgeMarkers)
    {
        const std::size_t found = constraintId.rfind(marker);
        if (found != std::string::npos)
        {
            separator = found;
            break;
        }
    }
    if (separator == std::string::npos || separator == prefixLength)
        return {};
    return constraintId.substr(prefixLength, separator - prefixLength);
}

bool featureIdsUnique(const std::vector<MapGeneratedFeature> &features)
{
    std::unordered_set<std::string> ids;
    for (const MapGeneratedFeature &feature : features)
        if (feature.id.empty() || !ids.insert(feature.id).second)
            return false;
    return true;
}

MapAtlasHierarchyNode buildNode(
    const MapModel &map,
    const std::unordered_map<EntityId, std::vector<const MapModel *>> &children,
    std::unordered_set<EntityId> &path,
    std::unordered_set<EntityId> &emitted)
{
    const bool legacyChild = map.atlasPlacement.parentId.empty() && !map.parentMapId.empty();
    MapAtlasHierarchyNode node{map.metadata.id, map.name, legacyChild ? MapAtlasService::levelFromLegacyScope(map.scope) : map.atlasLevel, {}};
    if (!path.insert(map.metadata.id).second)
        return node;
    emitted.insert(map.metadata.id);
    const auto childIt = children.find(map.metadata.id);
    if (childIt != children.end())
    {
        for (const MapModel *child : childIt->second)
        {
            if (path.find(child->metadata.id) == path.end())
                node.children.push_back(buildNode(*child, children, path, emitted));
        }
    }
    path.erase(map.metadata.id);
    return node;
}
}
