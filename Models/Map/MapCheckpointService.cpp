#include "Models/Map/MapCheckpointSnapshot.hpp"

using namespace gm::map_checkpoint::detail;

MapCheckpointService::MapCheckpointService(std::filesystem::path projectDirectory,
                                           std::size_t namedCheckpointLimit)
    : projectDirectory_(std::move(projectDirectory)),
      namedCheckpointLimit_(std::max<std::size_t>(1, std::min(namedCheckpointLimit, MaximumNamedCheckpoints)))
{
}

MapCheckpointWriteResult MapCheckpointService::createNamedCheckpoint(const MapModel &map,
                                                                     const std::string &name) const
{
    MapCheckpointWriteResult result = writeSnapshot(projectDirectory_, map, createEntityId("checkpoint"),
                                                     name, MapCheckpointKind::Named, {}, false);
    if (!result.success) return result;

    MapCheckpointListResult listed = listNamedCheckpoints(map.metadata.id);
    if (!listed.success)
    {
        result.error = "Checkpoint was created, but retention could not be evaluated: " + listed.error;
        return result;
    }
    while (listed.checkpoints.size() > namedCheckpointLimit_)
    {
        const MapCheckpointInfo &oldest = listed.checkpoints.back();
        std::string removeError;
        if (!removeNamedCheckpoint(map.metadata.id, oldest.id, &removeError))
        {
            result.error = "Checkpoint was created, but an old checkpoint could not be pruned: " + removeError;
            break;
        }
        result.prunedCheckpointIds.push_back(oldest.id);
        listed.checkpoints.pop_back();
    }
    return result;
}

MapCheckpointListResult MapCheckpointService::listNamedCheckpoints(const EntityId &mapId) const
{
    MapCheckpointListResult result;
    std::filesystem::path storageRoot;
    std::filesystem::path mapDirectory;
    if (!prepareStorage(projectDirectory_, mapId, false, storageRoot, mapDirectory, result.error)) return result;
    std::error_code ec;
    if (!std::filesystem::exists(mapDirectory, ec))
    {
        result.success = true;
        return result;
    }
    for (const auto &entry : std::filesystem::directory_iterator(mapDirectory, ec))
    {
        if (ec)
        {
            result.error = "Could not enumerate checkpoints.";
            return result;
        }
        const std::string id = entry.path().filename().u8string();
        if (id == JournalId || id.rfind(".", 0) == 0 || !checkpointIdIsSafe(id, false)) continue;
        MapCheckpointInfo info;
        info.id = id;
        info.mapId = mapId;
        if (entry.is_symlink(ec) || !entry.is_directory(ec))
        {
            info.error = "Checkpoint storage entry is unsafe.";
        }
        else
        {
            info = inspectCheckpoint(storageRoot, entry.path(), mapId, id, MapCheckpointKind::Named);
        }
        result.checkpoints.push_back(std::move(info));
    }
    if (ec)
    {
        result.error = "Could not enumerate checkpoints.";
        return result;
    }
    std::sort(result.checkpoints.begin(), result.checkpoints.end(), [](const auto &left, const auto &right) {
        if (left.createdUnixNanos != right.createdUnixNanos) return left.createdUnixNanos > right.createdUnixNanos;
        return left.id > right.id;
    });
    result.success = true;
    return result;
}

MapCheckpointRestorePreview MapCheckpointService::previewRestore(const MapModel &currentMap,
                                                                 const std::string &checkpointId) const
{
    return previewAt(projectDirectory_, currentMap, checkpointId, MapCheckpointKind::Named);
}

bool MapCheckpointService::applyRestore(const MapCheckpointRestorePreview &preview,
                                        MapModel &targetMap,
                                        bool confirmed,
                                        std::string *error) const
{
    auto fail = [&](const std::string &message) {
        if (error) *error = message;
        return false;
    };
    if (!confirmed) return fail("Restoring a checkpoint requires explicit confirmation.");
    if (!preview.valid) return fail("The checkpoint restore preview is invalid.");
    if (targetMap.metadata.id != preview.targetMapId || targetMap.revision != preview.targetRevision)
        return fail("The map changed after the restore preview was created. Create a new preview.");
    if (!checkpointIdIsSafe(preview.checkpoint.id, preview.recovery))
        return fail("Checkpoint ID is invalid.");

    std::filesystem::path storageRoot;
    std::filesystem::path mapDirectory;
    std::string pathError;
    if (!prepareStorage(projectDirectory_, preview.targetMapId, false, storageRoot, mapDirectory, pathError))
        return fail(pathError);
    const std::filesystem::path directory = mapDirectory / preview.checkpoint.id;
    const MapCheckpointKind expectedKind = preview.recovery ? MapCheckpointKind::CrashJournal : MapCheckpointKind::Named;
    const MapCheckpointInfo currentInfo = inspectCheckpoint(storageRoot, directory, preview.targetMapId,
                                                            preview.checkpoint.id, expectedKind);
    if (!currentInfo.valid) return fail(currentInfo.error);
    if (currentInfo.packageFingerprint != preview.checkpoint.packageFingerprint)
        return fail("Checkpoint changed after the restore preview was created. Create a new preview.");

    MapPackageImportOptions options;
    options.standaloneCopy = false;
    options.remapEntityIds = false;
    const MapPackageImportResult imported = MapPackageService::importPackage(directory / PackageFileName, options);
    if (!imported.success) return fail(imported.error);
    if (imported.map.metadata.id != preview.targetMapId || imported.map.revision != currentInfo.sourceRevision)
        return fail("Checkpoint contents do not match their recorded map identity and revision.");

    // Assignment is the only mutation and happens only after every validation
    // and stale-preview check has completed.
    MapModel replacement = imported.map;
    targetMap = std::move(replacement);
    if (error) error->clear();
    return true;
}

bool MapCheckpointService::removeNamedCheckpoint(const EntityId &mapId,
                                                 const std::string &checkpointId,
                                                 std::string *error) const
{
    auto fail = [&](const std::string &message) {
        if (error) *error = message;
        return false;
    };
    if (!checkpointIdIsSafe(checkpointId, false)) return fail("Checkpoint ID is invalid.");
    std::filesystem::path storageRoot;
    std::filesystem::path mapDirectory;
    std::string pathError;
    if (!prepareStorage(projectDirectory_, mapId, false, storageRoot, mapDirectory, pathError)) return fail(pathError);
    const auto directory = mapDirectory / checkpointId;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) return fail("Checkpoint does not exist.");
    if (!removeOwnedDirectory(storageRoot, directory, pathError)) return fail(pathError);
    if (error) error->clear();
    return true;
}

MapCheckpointWriteResult MapCheckpointService::writeCrashJournal(const MapModel &map,
                                                                 const std::string &transactionLabel,
                                                                 const MapCheckpointPublicationGate &publish) const
{
    return writeSnapshot(projectDirectory_, map, JournalId, "Automatic recovery",
                         MapCheckpointKind::CrashJournal, transactionLabel, true, publish);
}

MapCheckpointRestorePreview MapCheckpointService::previewRecovery(const MapModel &currentMap) const
{
    return previewAt(projectDirectory_, currentMap, JournalId, MapCheckpointKind::CrashJournal);
}

bool MapCheckpointService::discardRecovery(const EntityId &mapId, std::string *error) const
{
    auto fail = [&](const std::string &message) {
        if (error) *error = message;
        return false;
    };
    std::filesystem::path storageRoot;
    std::filesystem::path mapDirectory;
    std::string pathError;
    if (!prepareStorage(projectDirectory_, mapId, false, storageRoot, mapDirectory, pathError)) return fail(pathError);
    const auto directory = mapDirectory / JournalId;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec))
    {
        if (error) error->clear();
        return true;
    }
    if (!removeOwnedDirectory(storageRoot, directory, pathError)) return fail(pathError);
    if (error) error->clear();
    return true;
}
