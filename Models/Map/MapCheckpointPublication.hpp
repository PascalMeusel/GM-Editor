#pragma once

#include "Models/Map/MapCheckpointMetadata.hpp"

namespace gm::map_checkpoint::detail
{
bool fileFingerprint(const std::filesystem::path &path, std::string &fingerprint,
                     std::uint64_t &byteCount, std::string &error)
{
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec || size > MapPackageLimits{}.maximumPackageBytes)
    {
        error = "Checkpoint package is missing or exceeds the safety limit.";
        return false;
    }
    std::ifstream stream(mapFileIoPath(path), std::ios::binary);
    if (!stream)
    {
        error = "Could not read checkpoint package.";
        return false;
    }
    std::array<std::uint8_t, 64 * 1024> buffer{};
    std::uint64_t hash = 1469598103934665603ull;
    while (stream)
    {
        stream.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize read = stream.gcount();
        if (read > 0) hash = fnv1a(buffer.data(), static_cast<std::size_t>(read), hash);
    }
    if (!stream.eof())
    {
        error = "Could not finish reading checkpoint package.";
        return false;
    }
    byteCount = static_cast<std::uint64_t>(size);
    fingerprint = hex64(hash);
    return true;
}

bool removeOwnedDirectory(const std::filesystem::path &storageRoot,
                          const std::filesystem::path &directory,
                          std::string &error)
{
    if (!existingPathIsSafe(storageRoot, directory, true, error)) return false;
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (ec)
        {
            error = "Could not inspect checkpoint before removal.";
            return false;
        }
        if (entry.is_symlink(ec) || !entry.is_regular_file(ec) ||
            (entry.path().filename() != PackageFileName && entry.path().filename() != MetadataFileName))
        {
            error = "Checkpoint directory contains an unexpected entry and was not removed.";
            return false;
        }
    }
    std::filesystem::remove(directory / PackageFileName, ec);
    if (ec)
    {
        error = "Could not remove checkpoint package.";
        return false;
    }
    std::filesystem::remove(directory / MetadataFileName, ec);
    if (ec)
    {
        error = "Could not remove checkpoint metadata.";
        return false;
    }
    if (!std::filesystem::remove(directory, ec) || ec)
    {
        error = "Could not remove checkpoint directory.";
        return false;
    }
    return true;
}

bool publishDirectory(const std::filesystem::path &storageRoot,
                      const std::filesystem::path &staging,
                      const std::filesystem::path &destination,
                      bool replace,
                      std::string &error,
                      std::filesystem::path *obsoleteBackup = nullptr)
{
    if (!isContainedPath(storageRoot, staging) || !isContainedPath(storageRoot, destination))
    {
        error = "Checkpoint publication path escapes project storage.";
        return false;
    }
    std::error_code ec;
    if (!replace && std::filesystem::exists(destination, ec))
    {
        error = "A checkpoint with this ID already exists.";
        return false;
    }
    std::filesystem::path backup;
    if (replace && std::filesystem::exists(destination, ec))
    {
        std::string safetyError;
        if (!existingPathIsSafe(storageRoot, destination, true, safetyError))
        {
            error = safetyError;
            return false;
        }
        backup = destination;
        backup += std::filesystem::u8path(".backup-" + createEntityId("journal"));
        std::filesystem::rename(destination, backup, ec);
        if (ec)
        {
            error = "Could not stage the previous recovery journal.";
            return false;
        }
    }
    std::filesystem::rename(staging, destination, ec);
    if (ec)
    {
        if (!backup.empty())
        {
            std::error_code restoreError;
            std::filesystem::rename(backup, destination, restoreError);
        }
        error = "Could not atomically publish checkpoint.";
        return false;
    }
    if (obsoleteBackup)
        *obsoleteBackup = backup;
    else if (!backup.empty())
    {
        std::string removeError;
        if (!removeOwnedDirectory(storageRoot, backup, removeError))
        {
            // The new journal is valid and published. A stale backup is kept
            // rather than deleting an unexpected path.
        }
    }
    return true;
}

MapCheckpointInfo inspectCheckpoint(const std::filesystem::path &storageRoot,
                                    const std::filesystem::path &directory,
                                    const EntityId &expectedMapId,
                                    const std::string &expectedId,
                                    MapCheckpointKind expectedKind)
{
    MapCheckpointInfo info;
    info.id = expectedId;
    info.mapId = expectedMapId;
    info.kind = expectedKind;
    std::string error;
    if (!existingPathIsSafe(storageRoot, directory, true, error))
    {
        info.error = error;
        return info;
    }
    const auto metadataPath = directory / MetadataFileName;
    const auto packagePath = directory / PackageFileName;
    if (!existingPathIsSafe(storageRoot, metadataPath, false, error) ||
        !existingPathIsSafe(storageRoot, packagePath, false, error))
    {
        info.error = error;
        return info;
    }
    StoredMetadata metadata;
    if (!readMetadata(metadataPath, metadata, error))
    {
        info.error = error;
        return info;
    }
    info = metadata.info;
    if (info.id != expectedId || info.mapId != expectedMapId || info.kind != expectedKind ||
        !checkpointIdIsSafe(info.id, expectedKind == MapCheckpointKind::CrashJournal))
    {
        info.valid = false;
        info.error = "Checkpoint metadata does not match its storage location.";
        return info;
    }
    std::string actualFingerprint;
    if (!fileFingerprint(packagePath, actualFingerprint, info.packageBytes, error))
    {
        info.error = error;
        return info;
    }
    if (actualFingerprint != info.packageFingerprint)
    {
        info.error = "Checkpoint package fingerprint does not match its metadata.";
        return info;
    }
    const MapPackagePreview preview = MapPackageService::previewPackage(packagePath);
    if (!preview.valid || preview.sourceMapId != expectedMapId)
    {
        info.error = preview.error.empty() ? "Checkpoint package belongs to a different map." : preview.error;
        return info;
    }
    info.valid = true;
    return info;
}

} // namespace gm::map_checkpoint::detail
