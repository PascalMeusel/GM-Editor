#pragma once

#include "Models/Map/MapCheckpointPublication.hpp"

namespace gm::map_checkpoint::detail
{
MapCheckpointWriteResult writeSnapshot(const std::filesystem::path &projectDirectory,
                                       const MapModel &map,
                                       const std::string &id,
                                       const std::string &name,
                                       MapCheckpointKind kind,
                                       const std::string &transactionLabel,
                                       bool replace,
                                       const MapCheckpointPublicationGate &gate = {})
{
    MapCheckpointWriteResult result;
    if (map.metadata.id.empty() || !checkpointIdIsSafe(id, kind == MapCheckpointKind::CrashJournal) ||
        name.empty() || name.size() > 512 || transactionLabel.size() > 2048)
    {
        result.error = "Checkpoint name, ID, or map identity is invalid.";
        return result;
    }
    std::filesystem::path storageRoot;
    std::filesystem::path mapDirectory;
    if (!prepareStorage(projectDirectory, map.metadata.id, true, storageRoot, mapDirectory, result.error))
        return result;

    const std::filesystem::path staging = mapDirectory / (".stage-" + createEntityId("checkpoint"));
    const std::filesystem::path destination = mapDirectory / id;
    if (!isContainedPath(mapDirectory, staging) || !isContainedPath(mapDirectory, destination))
    {
        result.error = "Checkpoint path is invalid.";
        return result;
    }
    std::error_code ec;
    std::filesystem::create_directory(staging, ec);
    if (ec)
    {
        result.error = "Could not create checkpoint staging directory.";
        return result;
    }
    auto cleanupStaging = [&]() {
        std::error_code cleanupError;
        std::filesystem::remove_all(staging, cleanupError);
    };

    MapPackageExportOptions exportOptions;
    exportOptions.includeBackgroundAsset = false;
    exportOptions.includeImportedAsset = false;
    // Checkpoints persist semantic/edit state only. Managed image originals and
    // their derived working proxies stay in the campaign asset store and must
    // never be duplicated into every automatic checkpoint.
    exportOptions.includeWorkingProxyAsset = false;
    const auto packagePath = staging / PackageFileName;
    const MapPackageExportResult exported = MapPackageService::exportPackage(map, packagePath, exportOptions);
    if (!exported.success)
    {
        result.error = exported.error;
        cleanupStaging();
        return result;
    }

    MapCheckpointInfo info;
    info.id = id;
    info.name = name;
    info.createdAtUtc = currentUtcTimestamp();
    info.createdUnixNanos = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    info.mapId = map.metadata.id;
    info.sourceRevision = map.revision;
    info.kind = kind;
    info.transactionLabel = transactionLabel;
    if (!fileFingerprint(packagePath, info.packageFingerprint, info.packageBytes, result.error) ||
        !writeMetadataAtomic(staging / MetadataFileName, info, result.error))
    {
        cleanupStaging();
        return result;
    }
    bool attempted = false;
    std::filesystem::path obsoleteBackup;
    const auto publish = [&]() {
        attempted = true;
        return publishDirectory(storageRoot, staging, destination, replace, result.error, &obsoleteBackup);
    };
    bool published = false;
    try { published = gate ? gate(publish) : publish(); }
    catch (...) { cleanupStaging(); throw; }
    if (!published)
    {
        result.cancelled = !attempted;
        cleanupStaging();
        return result;
    }
    // Cleanup can be slow on a scanned volume. It must not hold the revision
    // guard that submit/cancel uses on the input thread.
    if (!obsoleteBackup.empty())
    {
        std::string ignored;
        removeOwnedDirectory(storageRoot, obsoleteBackup, ignored);
    }
    info.valid = true;
    result.success = true;
    result.checkpoint = std::move(info);
    return result;
}

MapCheckpointRestorePreview previewAt(const std::filesystem::path &projectDirectory,
                                      const MapModel &currentMap,
                                      const std::string &checkpointId,
                                      MapCheckpointKind kind)
{
    MapCheckpointRestorePreview result;
    result.recovery = kind == MapCheckpointKind::CrashJournal;
    result.targetMapId = currentMap.metadata.id;
    result.targetRevision = currentMap.revision;
    if (currentMap.metadata.id.empty() || !checkpointIdIsSafe(checkpointId, result.recovery))
    {
        result.error = "Checkpoint ID is invalid.";
        return result;
    }
    std::filesystem::path storageRoot;
    std::filesystem::path mapDirectory;
    if (!prepareStorage(projectDirectory, currentMap.metadata.id, false, storageRoot, mapDirectory, result.error))
        return result;
    const auto directory = mapDirectory / checkpointId;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec))
    {
        result.error = result.recovery ? "No recovery journal is available." : "Checkpoint does not exist.";
        return result;
    }
    result.checkpoint = inspectCheckpoint(storageRoot, directory, currentMap.metadata.id, checkpointId, kind);
    if (!result.checkpoint.valid)
    {
        result.error = result.checkpoint.error;
        return result;
    }
    result.package = MapPackageService::previewPackage(directory / PackageFileName);
    if (!result.package.valid)
    {
        result.error = result.package.error;
        return result;
    }
    result.valid = true;
    return result;
}
} // namespace gm::map_checkpoint::detail
