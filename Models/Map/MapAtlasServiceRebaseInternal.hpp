#pragma once

#include "Models/Map/MapAtlasServiceInternal.hpp"

#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace MapAtlasServiceInternal
{
template <typename T>
const std::optional<T> &localRebasePayload(const MapAtlasRebaseItem &item)
{
    if constexpr (std::is_same_v<T, MapRegionModel>)
        return item.localRegion;
    else
        return item.localRoute;
}

template <typename T>
const std::optional<T> &parentRebasePayload(const MapAtlasRebaseItem &item)
{
    if constexpr (std::is_same_v<T, MapRegionModel>)
        return item.parentRegion;
    else
        return item.parentRoute;
}

template <typename T>
void setLocalRebasePayload(MapAtlasRebaseItem &item, const T &entry)
{
    if constexpr (std::is_same_v<T, MapRegionModel>)
        item.localRegion = entry;
    else
        item.localRoute = entry;
}

template <typename T>
void setParentRebasePayload(MapAtlasRebaseItem &item, const T &entry)
{
    if constexpr (std::is_same_v<T, MapRegionModel>)
        item.parentRegion = entry;
    else
        item.parentRoute = entry;
}

template <typename T, typename Equal>
void appendAtlasCollectionRebaseItems(
    const std::vector<T> &existing,
    const std::vector<T> &incoming,
    const MapModel &parent,
    const char *kind,
    MapAtlasRebaseItemKind itemKind,
    Equal equal,
    std::vector<MapAtlasRebaseItem> &items,
    std::vector<std::string> &errors)
{
    std::unordered_map<EntityId, const T *> localInherited;
    std::unordered_set<EntityId> localIds;
    std::unordered_set<EntityId> childIds;
    for (const T &entry : existing)
    {
        const bool inherited = metadataInheritedFrom(entry.metadata, parent, kind);
        if (entry.metadata.id.empty())
        {
            errors.push_back(std::string("A child ") + kind + " has no stable ID.");
            continue;
        }
        if (!childIds.insert(entry.metadata.id).second)
        {
            errors.push_back(std::string("The child contains duplicate ") +
                             (inherited ? "inherited " : "local ") + kind +
                             " IDs: " + entry.metadata.id);
            continue;
        }
        if (inherited)
        {
            if (atlasSourceEntityId(entry.metadata).empty())
            {
                errors.push_back(std::string("An inherited ") + kind +
                                 " has no stable source ID: " + entry.metadata.id);
                continue;
            }
            localInherited.emplace(entry.metadata.id, &entry);
        }
        else
        {
            localIds.insert(entry.metadata.id);
        }
    }

    std::unordered_set<EntityId> matched;
    std::unordered_set<EntityId> incomingIds;
    for (const T &parentEntry : incoming)
    {
        if (parentEntry.metadata.id.empty())
        {
            errors.push_back(std::string("The parent extraction produced a ") + kind +
                             " without a stable ID.");
            continue;
        }
        if (!incomingIds.insert(parentEntry.metadata.id).second)
        {
            errors.push_back(std::string("The parent extraction produced a duplicate ") +
                             kind + " ID: " + parentEntry.metadata.id);
            continue;
        }
        if (localIds.find(parentEntry.metadata.id) != localIds.end())
        {
            errors.push_back(std::string("An inherited ") + kind +
                             " ID conflicts with local child content: " +
                             parentEntry.metadata.id);
            continue;
        }

        MapAtlasRebaseItem item;
        item.id = rebaseItemId(itemKind, parentEntry.metadata.id);
        item.kind = itemKind;
        item.sourceEntityId = atlasSourceEntityId(parentEntry.metadata);
        if (item.sourceEntityId.empty())
        {
            errors.push_back(std::string("An inherited ") + kind +
                             " has no stable source ID: " + parentEntry.metadata.id);
            continue;
        }
        setParentRebasePayload(item, parentEntry);
        const auto localIt = localInherited.find(parentEntry.metadata.id);
        if (localIt == localInherited.end())
        {
            item.change = MapAtlasRebaseChange::AddedByParent;
        }
        else
        {
            item.localEntityId = localIt->second->metadata.id;
            setLocalRebasePayload(item, *localIt->second);
            matched.insert(localIt->first);
            item.change = equal(*localIt->second, parentEntry)
                              ? MapAtlasRebaseChange::Unchanged
                              : MapAtlasRebaseChange::ChangedByParent;
        }
        item.requiresDecision = item.change != MapAtlasRebaseChange::Unchanged;
        items.push_back(std::move(item));
    }

    // Preserve child ordering so identical inputs always produce the same
    // decision list, including entities removed from the parent.
    for (const T &entry : existing)
    {
        if (!metadataInheritedFrom(entry.metadata, parent, kind) ||
            matched.find(entry.metadata.id) != matched.end() ||
            atlasSourceEntityId(entry.metadata).empty())
            continue;
        MapAtlasRebaseItem item;
        item.id = rebaseItemId(itemKind, entry.metadata.id);
        item.kind = itemKind;
        item.sourceEntityId = atlasSourceEntityId(entry.metadata);
        item.localEntityId = entry.metadata.id;
        item.change = MapAtlasRebaseChange::RemovedFromParent;
        item.requiresDecision = true;
        setLocalRebasePayload(item, entry);
        items.push_back(std::move(item));
    }
}

template <typename T>
bool mergeAtlasCollectionByDecision(
    std::vector<T> &target,
    const MapModel &parent,
    const char *kind,
    MapAtlasRebaseItemKind itemKind,
    const std::vector<MapAtlasRebaseItem> &items,
    const std::unordered_map<std::string, MapAtlasRebaseResolution> &resolved,
    std::vector<std::string> &errors)
{
    std::vector<T> merged;
    merged.reserve(target.size() + items.size());
    std::unordered_set<EntityId> ids;
    std::unordered_set<std::string> appliedItems;

    const auto append = [&](const T &entry) {
        if (entry.metadata.id.empty() || !ids.insert(entry.metadata.id).second)
        {
            errors.push_back(std::string("An inherited ") + kind +
                             " conflicts with local child content: " +
                             entry.metadata.id);
            return false;
        }
        merged.push_back(entry);
        return true;
    };
    const auto applyItem = [&](const MapAtlasRebaseItem &item) {
        const MapAtlasRebaseResolution resolution = item.requiresDecision
                                                        ? resolved.at(item.id)
                                                        : MapAtlasRebaseResolution::UseParent;
        if (resolution == MapAtlasRebaseResolution::UseParent)
        {
            const std::optional<T> &payload = parentRebasePayload<T>(item);
            return !payload || append(*payload);
        }

        const std::optional<T> &payload = localRebasePayload<T>(item);
        if (!payload)
            return true;
        if (resolution == MapAtlasRebaseResolution::KeepLocal)
            return append(*payload);

        T detached = *payload;
        detachAtlasLineage(detached.metadata);
        return append(detached);
    };

    // Keep all unrelated entries in their original order. Existing inherited
    // entries are resolved in place, so a rebase does not reorder local data.
    for (const T &entry : target)
    {
        if (!metadataInheritedFrom(entry.metadata, parent, kind))
        {
            if (!append(entry))
                return false;
            continue;
        }
        const auto itemIt = std::find_if(items.begin(), items.end(), [&](const MapAtlasRebaseItem &item) {
            return item.kind == itemKind && item.localEntityId == entry.metadata.id;
        });
        if (itemIt == items.end())
        {
            errors.push_back(std::string("An inherited ") + kind +
                             " is absent from the rebase decision contract: " +
                             entry.metadata.id);
            return false;
        }
        if (!appliedItems.insert(itemIt->id).second || !applyItem(*itemIt))
            return false;
    }

    // Parent additions have no existing position and are appended in stable
    // extraction order. Keep-local or detach intentionally rejects the add.
    for (const MapAtlasRebaseItem &item : items)
    {
        if (item.kind != itemKind || !item.localEntityId.empty() ||
            appliedItems.find(item.id) != appliedItems.end())
            continue;
        if (!applyItem(item))
            return false;
        appliedItems.insert(item.id);
    }

    target = std::move(merged);
    return true;
}
}
