#pragma once

#include "Models/Project/ProjectPackageImportPaths.hpp"

namespace
{
void rebaseClonedAssets(Campaign &campaign, const EntityId &originalId)
{
    const auto oldRoot = ProjectPackageLayout::campaignPackageDirectory(originalId).lexically_normal();
    const auto newRoot = ProjectPackageLayout::campaignPackageDirectory(campaign).lexically_normal();
    const auto rebase = [&](std::string &stored) {
        if (stored.empty()) return;
        const auto resolved = ProjectAssets::resolveProjectPath(stored).lexically_normal();
        const auto relative = resolved.lexically_relative(oldRoot);
        if (!isSafeRelativePath(relative)) return;
        stored = (newRoot / relative).lexically_relative(ProjectAssets::findProjectRoot()).generic_string();
    };
    for (auto &character : campaign.characters) rebase(character.portraitAssetId);
    for (auto &map : campaign.maps) rebase(map.backgroundImagePath);
    for (auto &audio : campaign.audioAssets) rebase(audio.filePath);
    for (auto &handout : campaign.handouts) rebase(handout.filePath);
    for (auto &item : campaign.customItems)
        if (auto found = item.metadata.customProperties.find("item.image"); found != item.metadata.customProperties.end())
            rebase(found->second);
}

void assignCloneId(EntityMetadata &metadata, const char *prefix, IdRemap &ids)
{
    const EntityId oldId = metadata.id;
    const EntityId newId = createEntityId(prefix);
    if (!oldId.empty())
        ids[oldId] = newId;
    metadata.id = newId;
    metadata.createdAtUtc = currentUtcTimestamp();
    metadata.updatedAtUtc = metadata.createdAtUtc;
}

void rewriteId(EntityId &id, const IdRemap &ids)
{
    const auto found = ids.find(id);
    if (found != ids.end())
        id = found->second;
}

void rewriteIds(std::vector<EntityId> &values, const IdRemap &ids)
{
    for (EntityId &value : values)
        rewriteId(value, ids);
}

void assignInventoryCloneIds(InventoryModel &inventory, IdRemap &ids)
{
    assignCloneId(inventory.metadata, "inventory", ids);
    for (InventoryEntryModel &entry : inventory.entries)
        assignCloneId(entry.metadata, "inventory_entry", ids);
}

void rewriteInventoryIds(InventoryModel &inventory, const IdRemap &ids)
{
    rewriteId(inventory.ownerEntityId, ids);
    for (InventoryEntryModel &entry : inventory.entries)
    {
        rewriteId(entry.itemId, ids);
        rewriteId(entry.containerEntryId, ids);
    }
}

void assignActionCloneIds(std::vector<MonsterActionModel> &, IdRemap &)
{
    // Actions have references but no row identity of their own.
}

void rewriteActionIds(std::vector<MonsterActionModel> &actions, const IdRemap &ids)
{
    for (MonsterActionModel &action : actions)
    {
        rewriteId(action.sourceEntityId, ids);
        rewriteId(action.linkedResourceId, ids);
        rewriteId(action.sourceSpellId, ids);
    }
}

void assignResourceCloneIds(std::vector<ResourceTrackerModel> &resources, IdRemap &ids)
{
    for (ResourceTrackerModel &resource : resources)
        assignCloneId(resource.metadata, "resource_tracker", ids);
}

void rewriteResourceIds(std::vector<ResourceTrackerModel> &resources, const IdRemap &ids)
{
    for (ResourceTrackerModel &resource : resources)
        rewriteId(resource.definitionId, ids);
}

} // namespace
