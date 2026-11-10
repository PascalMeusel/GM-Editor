#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapFileIoPath.hpp"
#include "Models/Project/ApplicationPaths.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <unordered_map>

#include "Models/Map/MapPackageServiceInternal.hpp"

using namespace MapPackageServiceInternal;

MapPackageMaterializationResult MapPackageService::materializeAssets(
    const std::vector<MapPackageAsset> &assets, const MapPackageMaterializationOptions &options)
{
    MapPackageMaterializationResult result;
    struct PreparedAsset
    {
        const MapPackageAsset *source = nullptr;
        std::filesystem::path relative;
        std::filesystem::path destination;
        std::filesystem::path staged;
        std::filesystem::path backup;
    };
    std::vector<PreparedAsset> prepared;
    std::vector<std::filesystem::path> committed;
    std::vector<std::filesystem::path> createdDirectories;
    std::filesystem::path stagingRoot;
    std::filesystem::path canonicalRoot;
    std::error_code cleanupError;

    const auto rollback = [&]() {
        for (auto item = committed.rbegin(); item != committed.rend(); ++item)
        {
            std::error_code ec;
            std::filesystem::remove(mapFileIoPath(*item), ec);
        }
        for (auto item = prepared.rbegin(); item != prepared.rend(); ++item)
        {
            if (item->backup.empty()) continue;
            std::error_code ec;
            if (std::filesystem::exists(mapFileIoPath(item->destination), ec))
                std::filesystem::remove(mapFileIoPath(item->destination), ec);
            ec.clear();
            std::filesystem::rename(mapFileIoPath(item->backup), mapFileIoPath(item->destination), ec);
        }
        for (auto item = createdDirectories.rbegin(); item != createdDirectories.rend(); ++item)
        {
            std::error_code ec;
            std::filesystem::remove(mapFileIoPath(*item), ec);
        }
        if (!stagingRoot.empty())
        {
            std::error_code ec;
            std::filesystem::remove_all(mapFileIoPath(stagingRoot), ec);
        }
    };

    try
    {
        if (options.targetDirectory.empty() ||
            options.targetDirectory.u8string().size() > options.limits.maximumPathBytes)
        {
            result.error = "Asset target directory is empty or exceeds the path safety limit.";
            return result;
        }
        const std::filesystem::file_status rootStatus =
            std::filesystem::symlink_status(mapFileIoPath(options.targetDirectory), cleanupError);
        if (cleanupError || !std::filesystem::is_directory(rootStatus) || std::filesystem::is_symlink(rootStatus))
        {
            result.error = "Asset target must be an existing, non-symbolic-link directory.";
            return result;
        }
        // Keep canonical model/result paths portable and extend only the I/O
        // arguments, including subsequent staging, validation and rollback.
        canonicalRoot = std::filesystem::canonical(mapFileIoPath(options.targetDirectory), cleanupError);
        if (cleanupError)
        {
            result.error = "Could not resolve the asset target directory.";
            return result;
        }
        if (assets.size() > options.limits.maximumCollectionItems)
        {
            result.error = "Embedded asset collection exceeds the safety limit.";
            return result;
        }

        std::unordered_set<std::string> pathKeys;
        std::uint64_t totalBytes = 0;
        prepared.reserve(assets.size());
        for (const MapPackageAsset &asset : assets)
        {
            if (static_cast<std::uint32_t>(asset.kind) >
                static_cast<std::uint32_t>(MapPackageAssetKind::WorkingProxy))
            {
                result.error = "Embedded asset contains an invalid kind.";
                return result;
            }
            if (!safePackagePath(asset.packagePath, options.limits) ||
                !safeLeafName(asset.sourceFileName, options.limits))
            {
                result.error = "Embedded asset contains an unsafe relative path.";
                return result;
            }
            if (asset.bytes.size() > options.limits.maximumAssetBytes ||
                asset.bytes.size() > options.limits.maximumPackageBytes ||
                totalBytes > options.limits.maximumPackageBytes - asset.bytes.size())
            {
                result.error = "Embedded assets exceed the materialization size limits.";
                return result;
            }
            totalBytes += asset.bytes.size();
            if (asset.contentHash != contentHash(asset.bytes))
            {
                result.error = "Embedded asset content hash does not match its bytes.";
                return result;
            }
            const std::filesystem::path relative = std::filesystem::path(asset.packagePath).lexically_normal();
            std::string pathKey = relative.generic_u8string();
#if defined(_WIN32)
            std::transform(pathKey.begin(), pathKey.end(), pathKey.begin(),
                           [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
#endif
            if (!pathKeys.insert(pathKey).second)
            {
                result.error = "Embedded assets contain duplicate destination paths.";
                return result;
            }
            PreparedAsset item;
            item.source = &asset;
            item.relative = relative;
            item.destination = canonicalRoot / item.relative;
            if (!validateMaterializationDestination(canonicalRoot, item.relative, item.destination,
                                                    options.allowOverwrite, result.error))
                return result;
            prepared.push_back(std::move(item));
        }

        if (prepared.empty())
        {
            result.success = true;
            return result;
        }

        stagingRoot = canonicalRoot / (".ttrpgmappkg-stage-" + createEntityId("asset_write"));
        if (!std::filesystem::create_directory(mapFileIoPath(stagingRoot), cleanupError) || cleanupError)
        {
            result.error = "Could not create the asset staging directory.";
            stagingRoot.clear();
            return result;
        }
        const std::filesystem::path stagedFiles = stagingRoot / "files";
        const std::filesystem::path backups = stagingRoot / "backups";
        std::filesystem::create_directories(mapFileIoPath(stagedFiles), cleanupError);
        if (cleanupError)
        {
            result.error = "Could not prepare the asset staging directory.";
            rollback();
            return result;
        }

        for (std::size_t index = 0; index < prepared.size(); ++index)
        {
            PreparedAsset &item = prepared[index];
            item.staged = stagedFiles / item.relative;
            std::filesystem::create_directories(mapFileIoPath(item.staged.parent_path()), cleanupError);
            if (cleanupError)
            {
                result.error = "Could not prepare an embedded asset staging path.";
                rollback();
                return result;
            }
            std::ofstream stream(mapFileIoPath(item.staged), std::ios::binary | std::ios::trunc);
            if (!stream || (!item.source->bytes.empty() &&
                            !stream.write(reinterpret_cast<const char *>(item.source->bytes.data()),
                                          static_cast<std::streamsize>(item.source->bytes.size()))))
            {
                result.error = "Could not stage an embedded asset.";
                rollback();
                return result;
            }
            stream.close();
            std::vector<std::uint8_t> verification;
            if (!readAssetFile(item.staged, options.limits, verification, result.error) ||
                contentHash(verification) != item.source->contentHash)
            {
                if (result.error.empty()) result.error = "Staged embedded asset failed hash verification.";
                rollback();
                return result;
            }
        }

        for (std::size_t index = 0; index < prepared.size(); ++index)
        {
            PreparedAsset &item = prepared[index];
            std::filesystem::path current = canonicalRoot;
            for (const auto &part : item.relative.parent_path())
            {
                current /= part;
                const std::filesystem::file_status status = std::filesystem::symlink_status(mapFileIoPath(current), cleanupError);
                if (!cleanupError && std::filesystem::exists(status))
                {
                    if (!std::filesystem::is_directory(status) || std::filesystem::is_symlink(status))
                    {
                        result.error = "Asset destination changed or became unsafe during commit.";
                        rollback();
                        return result;
                    }
                }
                else
                {
                    cleanupError.clear();
                    if (!std::filesystem::create_directory(mapFileIoPath(current), cleanupError) || cleanupError)
                    {
                        result.error = "Could not create an embedded asset destination directory.";
                        rollback();
                        return result;
                    }
                    createdDirectories.push_back(current);
                }
                cleanupError.clear();
            }

            const std::filesystem::file_status destinationStatus =
                std::filesystem::symlink_status(mapFileIoPath(item.destination), cleanupError);
            const bool destinationExists = !cleanupError && std::filesystem::exists(destinationStatus);
            cleanupError.clear();
            if (destinationExists)
            {
                if (!options.allowOverwrite || std::filesystem::is_directory(destinationStatus) ||
                    std::filesystem::is_symlink(destinationStatus))
                {
                    result.error = "Asset destination changed or overwrite was not allowed during commit.";
                    rollback();
                    return result;
                }
                std::filesystem::create_directories(mapFileIoPath(backups), cleanupError);
                item.backup = backups / (std::to_string(index) + ".backup");
                std::filesystem::rename(mapFileIoPath(item.destination), mapFileIoPath(item.backup), cleanupError);
                if (cleanupError)
                {
                    item.backup.clear();
                    result.error = "Could not protect an existing asset before overwrite.";
                    rollback();
                    return result;
                }
            }
            std::filesystem::rename(mapFileIoPath(item.staged), mapFileIoPath(item.destination), cleanupError);
            if (cleanupError)
            {
                result.error = "Could not atomically commit an embedded asset.";
                rollback();
                return result;
            }
            committed.push_back(item.destination);
        }

        for (const PreparedAsset &item : prepared)
        {
            const MapPackageAsset &asset = *item.source;
            result.assets.push_back({asset.kind, asset.packagePath, item.relative.generic_u8string(),
                                     item.destination, asset.contentHash, asset.bytes.size()});
        }
        std::filesystem::remove_all(mapFileIoPath(stagingRoot), cleanupError);
        result.success = true;
        return result;
    }
    catch (const std::exception &exception)
    {
        result.error = std::string("Asset materialization failed: ") + exception.what();
        rollback();
        return result;
    }
}

MapPackageMaterializationResult MapPackageService::materializePackageAssets(
    const std::filesystem::path &packagePath, const MapPackageMaterializationOptions &options)
{
    DecodedPackage decoded = decode(packagePath, options.limits);
    if (!decoded.error.empty())
    {
        MapPackageMaterializationResult result;
        result.error = decoded.error;
        return result;
    }
    return materializeAssets(decoded.assets, options);
}
