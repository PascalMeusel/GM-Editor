#pragma once

#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapFileIoPath.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>


namespace gm::map_checkpoint::detail
{
constexpr std::array<std::uint8_t, 8> MetadataMagic{{'G', 'M', 'C', 'K', 'P', 'T', '1', 0}};
constexpr std::uint32_t MetadataVersion = 1;
constexpr const char *InternalFolder = ".gm-editor";
constexpr const char *StorageFolder = "map-checkpoints";
constexpr const char *PackageFileName = "map.ttrpgmappkg";
constexpr const char *MetadataFileName = "checkpoint.meta";
constexpr const char *JournalId = "journal";

struct StoredMetadata
{
    MapCheckpointInfo info;
};

std::uint64_t fnv1a(const std::uint8_t *data, std::size_t size,
                    std::uint64_t seed = 1469598103934665603ull)
{
    std::uint64_t hash = seed;
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= data[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string hex64(std::uint64_t value)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

std::string mapStorageKey(const EntityId &mapId)
{
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(mapId.data());
    const std::uint64_t first = fnv1a(bytes, mapId.size());
    const std::uint64_t second = fnv1a(bytes, mapId.size(), 1099511628211ull ^ mapId.size());
    return "map-" + hex64(first) + hex64(second);
}

bool checkpointIdIsSafe(const std::string &id, bool allowJournal)
{
    if (id.empty() || id.size() > 128 || (!allowJournal && id == JournalId)) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_';
    });
}

bool isContainedPath(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    const std::filesystem::path normalizedRoot = std::filesystem::absolute(root).lexically_normal();
    const std::filesystem::path normalizedCandidate = std::filesystem::absolute(candidate).lexically_normal();
    auto rootIt = normalizedRoot.begin();
    auto candidateIt = normalizedCandidate.begin();
    for (; rootIt != normalizedRoot.end() && candidateIt != normalizedCandidate.end(); ++rootIt, ++candidateIt)
    {
#if defined(_WIN32)
        std::string left = rootIt->u8string();
        std::string right = candidateIt->u8string();
        std::transform(left.begin(), left.end(), left.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(right.begin(), right.end(), right.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (left != right) return false;
#else
        if (*rootIt != *candidateIt) return false;
#endif
    }
    return rootIt == normalizedRoot.end();
}

bool existingPathIsSafe(const std::filesystem::path &storageRoot,
                        const std::filesystem::path &path,
                        bool requireDirectory,
                        std::string &error)
{
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec)
    {
        error = "Could not inspect checkpoint storage.";
        return false;
    }
    if (std::filesystem::is_symlink(status))
    {
        error = "Checkpoint storage may not contain symbolic links.";
        return false;
    }
    if (requireDirectory ? !std::filesystem::is_directory(status) : !std::filesystem::is_regular_file(status))
    {
        error = requireDirectory ? "Checkpoint path is not a directory." : "Checkpoint path is not a regular file.";
        return false;
    }
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(storageRoot, ec);
    if (ec)
    {
        error = "Could not resolve checkpoint storage.";
        return false;
    }
    const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
    if (ec || !isContainedPath(canonicalRoot, canonicalPath))
    {
        error = "Checkpoint path escapes the project storage directory.";
        return false;
    }
    return true;
}

bool prepareStorage(const std::filesystem::path &projectDirectory,
                    const EntityId &mapId,
                    bool create,
                    std::filesystem::path &storageRoot,
                    std::filesystem::path &mapDirectory,
                    std::string &error)
{
    if (projectDirectory.empty() || mapId.empty() || mapId.size() > 4096)
    {
        error = "An explicit project directory and map ID are required.";
        return false;
    }
    std::error_code ec;
    const std::filesystem::path absoluteProject = mapFileIoPath(
        std::filesystem::absolute(projectDirectory, ec).lexically_normal());
    if (ec || !std::filesystem::exists(absoluteProject, ec) ||
        !std::filesystem::is_directory(absoluteProject, ec))
    {
        error = "The project directory does not exist.";
        return false;
    }
    const std::filesystem::path internalRoot = absoluteProject / InternalFolder;
    storageRoot = internalRoot / StorageFolder;
    mapDirectory = storageRoot / mapStorageKey(mapId);
    if (!isContainedPath(absoluteProject, storageRoot) || !isContainedPath(storageRoot, mapDirectory))
    {
        error = "Checkpoint storage path is invalid.";
        return false;
    }
    if (create)
    {
        // Create each internal component separately and reject symlinks before
        // descending, so a pre-planted .gm-editor link cannot redirect writes
        // outside the explicitly supplied project directory.
        for (const auto &directory : {internalRoot, storageRoot})
        {
            if (std::filesystem::exists(directory, ec))
            {
                const auto status = std::filesystem::symlink_status(directory, ec);
                if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_directory(status))
                {
                    error = "Checkpoint storage may not traverse symbolic links.";
                    return false;
                }
            }
            else
            {
                std::filesystem::create_directory(directory, ec);
                if (ec)
                {
                    error = "Could not create checkpoint storage.";
                    return false;
                }
            }
        }
    }
    if (std::filesystem::exists(storageRoot, ec))
    {
        if (!existingPathIsSafe(storageRoot, storageRoot, true, error)) return false;
        const std::filesystem::path canonicalProject = std::filesystem::weakly_canonical(absoluteProject, ec);
        const std::filesystem::path canonicalStorage = std::filesystem::weakly_canonical(storageRoot, ec);
        if (ec || !isContainedPath(canonicalProject, canonicalStorage))
        {
            error = "Checkpoint storage escapes the explicit project directory.";
            return false;
        }
    }
    else if (!create)
    {
        return true;
    }
    if (create)
    {
        if (std::filesystem::exists(mapDirectory, ec))
        {
            if (!existingPathIsSafe(storageRoot, mapDirectory, true, error)) return false;
        }
        else
        {
            std::filesystem::create_directory(mapDirectory, ec);
            if (ec)
            {
                error = "Could not create map checkpoint storage.";
                return false;
            }
            if (!existingPathIsSafe(storageRoot, mapDirectory, true, error)) return false;
        }
    }
    else if (std::filesystem::exists(mapDirectory, ec) &&
             !existingPathIsSafe(storageRoot, mapDirectory, true, error))
    {
        return false;
    }
    return true;
}

} // namespace gm::map_checkpoint::detail
