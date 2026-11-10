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

using namespace MapAtlasServiceInternal;

namespace MapAtlasService
{
int levelDepth(MapLevel level)
{
    return static_cast<int>(level);
}

bool isDeeperLevel(MapLevel child, MapLevel parent)
{
    return levelDepth(child) > levelDepth(parent);
}

bool isNextLevel(MapLevel child, MapLevel parent)
{
    return levelDepth(child) == levelDepth(parent) + 1;
}

MapLevel levelFromLegacyScope(MapScope scope)
{
    switch (scope)
    {
    case MapScope::World:
        return MapLevel::World;
    case MapScope::Local:
    case MapScope::Dungeon:
        return MapLevel::Site;
    }
    return MapLevel::World;
}

MapKind kindFromLegacyScope(MapScope scope)
{
    switch (scope)
    {
    case MapScope::World:
        return MapKind::Geographic;
    case MapScope::Local:
        return MapKind::General;
    case MapScope::Dungeon:
        return MapKind::Dungeon;
    }
    return MapKind::General;
}

EntityId effectiveParentId(const MapModel &map)
{
    return !map.atlasPlacement.parentId.empty() ? map.atlasPlacement.parentId : map.parentMapId;
}

const MapModel *primaryWorldMap(const std::vector<MapModel> &maps)
{
    const auto main = std::find_if(maps.begin(), maps.end(), [](const MapModel &map) {
        return map.isMainMap && map.atlasLevel == MapLevel::World;
    });
    if (main != maps.end())
        return &*main;
    // Historical campaigns can contain a world map without isMainMap. Keep
    // that world as the visible atlas anchor without rewriting campaign data.
    const auto root = std::find_if(maps.begin(), maps.end(), [](const MapModel &map) {
        return map.atlasLevel == MapLevel::World && effectiveParentId(map).empty();
    });
    return root == maps.end() ? nullptr : &*root;
}

bool isNormalizedRectValid(const MapNormalizedRect &rect)
{
    return std::isfinite(rect.x) && std::isfinite(rect.y) && std::isfinite(rect.width) && std::isfinite(rect.height) &&
           rect.x >= 0.0f && rect.y >= 0.0f && rect.width > 0.0f && rect.height > 0.0f &&
           rect.x + rect.width <= 1.0f + atlasEpsilon && rect.y + rect.height <= 1.0f + atlasEpsilon;
}

MapAtlasValidationResult validate(const std::vector<MapModel> &maps)
{
    MapAtlasValidationResult result;
    const MapLookup lookup = buildLookup(maps);
    std::unordered_set<EntityId> seenIds;
    for (const MapModel &map : maps)
    {
        if (map.metadata.id.empty())
        {
            result.issues.push_back({"missing_id", {}, {}, "Atlas maps require a stable map ID."});
            continue;
        }
        if (!seenIds.insert(map.metadata.id).second)
            result.issues.push_back({"duplicate_id", map.metadata.id, {}, "Atlas map IDs must be unique."});
        const EntityId parentId = effectiveParentId(map);
        if (parentId.empty())
            continue;
        const bool legacyOnly = map.atlasPlacement.parentId.empty();
        if (!legacyOnly && !isNormalizedRectValid(map.atlasPlacement.parentRect))
            result.issues.push_back({"invalid_parent_rect", map.metadata.id, parentId, "The parent placement must be a non-empty normalized rectangle."});
        const auto parentIt = lookup.find(parentId);
        if (parentIt == lookup.end())
        {
            result.issues.push_back({"missing_parent", map.metadata.id, parentId, "The atlas parent map does not exist."});
            continue;
        }
        const MapLevel childLevel = legacyOnly ? levelFromLegacyScope(map.scope) : map.atlasLevel;
        const MapLevel parentLevel = legacyOnly ? levelFromLegacyScope(parentIt->second->scope) : parentIt->second->atlasLevel;
        if (!isDeeperLevel(childLevel, parentLevel))
            result.issues.push_back({"invalid_level", map.metadata.id, parentId, "A child map must be at a deeper atlas level than its parent."});
    }

    for (const MapModel &map : maps)
    {
        std::unordered_set<EntityId> path;
        const MapModel *current = &map;
        while (current && !effectiveParentId(*current).empty())
        {
            if (!path.insert(current->metadata.id).second)
            {
                result.issues.push_back({"cycle", map.metadata.id, current->metadata.id, "The atlas hierarchy contains a cycle."});
                break;
            }
            const auto parentIt = lookup.find(effectiveParentId(*current));
            current = parentIt == lookup.end() ? nullptr : parentIt->second;
        }
    }
    result.valid = result.issues.empty();
    return result;
}

std::vector<MapAtlasHierarchyNode> buildHierarchy(const std::vector<MapModel> &maps)
{
    const MapLookup lookup = buildLookup(maps);
    std::unordered_map<EntityId, std::vector<const MapModel *>> children;
    std::vector<const MapModel *> roots;
    for (const MapModel &map : maps)
    {
        const EntityId parentId = effectiveParentId(map);
        if (parentId.empty() || lookup.find(parentId) == lookup.end())
            roots.push_back(&map);
        else
            children[parentId].push_back(&map);
    }
    auto byName = [](const MapModel *left, const MapModel *right) {
        if (left->name != right->name)
            return left->name < right->name;
        return left->metadata.id < right->metadata.id;
    };
    std::sort(roots.begin(), roots.end(), byName);
    for (auto &entry : children)
        std::sort(entry.second.begin(), entry.second.end(), byName);

    std::vector<MapAtlasHierarchyNode> hierarchy;
    std::unordered_set<EntityId> emitted;
    for (const MapModel *root : roots)
    {
        std::unordered_set<EntityId> path;
        hierarchy.push_back(buildNode(*root, children, path, emitted));
    }
    // Invalid cyclic components have no root. Keep them inspectable instead of
    // silently dropping them from the hierarchy result.
    for (const MapModel &map : maps)
    {
        if (emitted.find(map.metadata.id) == emitted.end())
        {
            std::unordered_set<EntityId> path;
            hierarchy.push_back(buildNode(map, children, path, emitted));
        }
    }
    return hierarchy;
}

std::vector<MapAtlasBreadcrumb> breadcrumbs(const std::vector<MapModel> &maps, const EntityId &mapId)
{
    const MapLookup lookup = buildLookup(maps);
    std::vector<MapAtlasBreadcrumb> reversed;
    std::unordered_set<EntityId> visited;
    auto currentIt = lookup.find(mapId);
    while (currentIt != lookup.end() && visited.insert(currentIt->first).second)
    {
        const MapModel &map = *currentIt->second;
        const bool legacyChild = map.atlasPlacement.parentId.empty() && !map.parentMapId.empty();
        reversed.push_back({map.metadata.id, map.name, legacyChild ? levelFromLegacyScope(map.scope) : map.atlasLevel});
        const EntityId parentId = effectiveParentId(map);
        if (parentId.empty())
            break;
        currentIt = lookup.find(parentId);
    }
    std::reverse(reversed.begin(), reversed.end());
    return reversed;
}

MapAtlasPoint childToParentNormalized(const MapAtlasPlacement &placement, MapAtlasPoint childPoint)
{
    return {
        placement.parentRect.x + childPoint.x * placement.parentRect.width,
        placement.parentRect.y + childPoint.y * placement.parentRect.height};
}

std::optional<MapAtlasPoint> parentToChildNormalized(const MapAtlasPlacement &placement, MapAtlasPoint parentPoint)
{
    if (!isNormalizedRectValid(placement.parentRect) || !pointInside(placement.parentRect, parentPoint))
        return std::nullopt;
    return MapAtlasPoint{
        (parentPoint.x - placement.parentRect.x) / placement.parentRect.width,
        (parentPoint.y - placement.parentRect.y) / placement.parentRect.height};
}
}
