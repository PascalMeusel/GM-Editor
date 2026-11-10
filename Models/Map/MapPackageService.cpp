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

MapPackageExportResult MapPackageService::exportPackage(
    const MapModel &sourceMap, const std::filesystem::path &packagePath, const MapPackageExportOptions &options)
{
    MapPackageExportResult result; result.packagePath = packagePath;
    try
    {
        std::error_code ec;
        if (packagePath.empty() || packagePath.u8string().size() > options.limits.maximumPathBytes)
        {
            result.error = "Package destination path is empty or too long."; return result;
        }
        // Use one path representation for validation, staging and commit.
        // Stream-only conversion leaves directory checks and rename broken
        // for ordinary caller paths longer than MAX_PATH on Windows.
        const auto ioPackagePath = mapFileIoPath(packagePath);
        if (std::filesystem::exists(ioPackagePath, ec) && !options.allowOverwrite)
        {
            result.error = "Package destination already exists; overwrite was not allowed."; return result;
        }
        if (ec || std::filesystem::is_directory(ioPackagePath, ec))
        {
            result.error = "Package destination is not a writable file path."; return result;
        }
        const std::filesystem::path parent = ioPackagePath.has_parent_path() ? ioPackagePath.parent_path() : std::filesystem::current_path();
        if (!std::filesystem::is_directory(parent, ec) || ec)
        {
            result.error = "Package destination directory does not exist."; return result;
        }
        MapModel map = sourceMap;
        if (!reasonableMap(map, options.limits, result.error)) return result;
        std::vector<MapPackageAsset> assets;
        const auto addAsset = [&](MapPackageAssetKind kind, const std::filesystem::path &explicitPath,
                                  const std::string &modelPath, const std::string &fallbackName) -> bool {
            std::filesystem::path source = explicitPath;
            if (source.empty() && !modelPath.empty()) source = std::filesystem::path(modelPath);
            if (source.empty()) return true;
            source = resolveAssetSourcePath(source);
            MapPackageAsset asset; asset.kind = kind;
            if (!readAssetFile(source, options.limits, asset.bytes, result.error)) return false;
            asset.sourceFileName = safeFileName(source, fallbackName);
            const char *folder = kind == MapPackageAssetKind::Background ? "background/" :
                                 kind == MapPackageAssetKind::ImportedReference ? "imported/" : "working-proxy/";
            asset.packagePath = std::string("assets/") + folder + asset.sourceFileName;
            if (!safePackagePath(asset.packagePath, options.limits))
            {
                result.error = "Generated embedded asset path is unsafe."; return false;
            }
            asset.contentHash = contentHash(asset.bytes);
            // Managed image references are content-addressed. Refuse to
            // silently package different bytes if their on-disk file was
            // replaced after import; otherwise the package would rewrite the
            // stored identity and defeat the immutable-original contract.
            std::string declaredHash;
            if (kind == MapPackageAssetKind::ImportedReference)
                declaredHash = sourceMap.importedReference.contentHash;
            else if (kind == MapPackageAssetKind::WorkingProxy)
                declaredHash = sourceMap.importedReference.workingProxyContentHash;
            if (!declaredHash.empty() && declaredHash != asset.contentHash)
            {
                result.error = kind == MapPackageAssetKind::ImportedReference
                                   ? "Managed original image changed after import."
                                   : "Managed working proxy changed after import.";
                return false;
            }
            if (kind == MapPackageAssetKind::Background)
            {
                map.backgroundImagePath = asset.packagePath;
            }
            else if (kind == MapPackageAssetKind::ImportedReference)
            {
                map.importedReference.projectRelativePath = asset.packagePath;
                map.importedReference.sourceFileName = asset.sourceFileName;
                map.importedReference.contentHash = asset.contentHash;
                if (sourceMap.backgroundImagePath == sourceMap.importedReference.projectRelativePath)
                    map.backgroundImagePath = asset.packagePath;
            }
            else
            {
                map.importedReference.workingProxyProjectRelativePath = asset.packagePath;
                map.importedReference.workingProxyContentHash = asset.contentHash;
                if (sourceMap.backgroundImagePath == sourceMap.importedReference.workingProxyProjectRelativePath)
                    map.backgroundImagePath = asset.packagePath;
            }
            assets.push_back(std::move(asset)); return true;
        };
        const bool backgroundAliasesImported = options.includeImportedAsset && options.backgroundAssetPath.empty() &&
            (!sourceMap.importedReference.projectRelativePath.empty() &&
             sourceMap.backgroundImagePath == sourceMap.importedReference.projectRelativePath);
        const bool backgroundAliasesProxy = options.includeWorkingProxyAsset && options.backgroundAssetPath.empty() &&
            (!sourceMap.importedReference.workingProxyProjectRelativePath.empty() &&
             sourceMap.backgroundImagePath == sourceMap.importedReference.workingProxyProjectRelativePath);
        if (options.includeBackgroundAsset && !backgroundAliasesImported && !backgroundAliasesProxy &&
            !addAsset(MapPackageAssetKind::Background, options.backgroundAssetPath,
                      map.backgroundImagePath, "background.bin")) return result;
        if (options.includeImportedAsset && !addAsset(MapPackageAssetKind::ImportedReference, options.importedAssetPath,
                                                       map.importedReference.projectRelativePath, "imported.bin")) return result;
        if (options.includeWorkingProxyAsset && map.importedReference.workingProxyGenerated &&
            !addAsset(MapPackageAssetKind::WorkingProxy, options.workingProxyAssetPath,
                      map.importedReference.workingProxyProjectRelativePath, "working-proxy.png")) return result;

        Writer payloadWriter(&options.limits); writeMap(payloadWriter, map);
        payloadWriter.u32(static_cast<std::uint32_t>(assets.size()));
        for (const MapPackageAsset &asset : assets) writeAsset(payloadWriter, asset);
        std::vector<std::uint8_t> payload = payloadWriter.take();
        if (payload.size() > options.limits.maximumPackageBytes - std::min<std::uint64_t>(HeaderSize, options.limits.maximumPackageBytes))
        {
            result.error = "Map package exceeds the file-size safety limit."; return result;
        }
        Writer fileWriter;
        for (std::uint8_t byte : Magic) fileWriter.u8(byte);
        fileWriter.u32(CurrentFormatVersion); fileWriter.u64(payload.size());
        fileWriter.u64(fnv1a(payload.data(), payload.size()));
        std::vector<std::uint8_t> file = fileWriter.take();
        file.insert(file.end(), payload.begin(), payload.end());

        std::filesystem::path temporary = ioPackagePath;
        temporary += std::filesystem::u8path(
            ".tmp-" + createEntityId("map_package"));
        {
            std::ofstream stream(mapFileIoPath(temporary), std::ios::binary | std::ios::trunc);
            if (!stream || !stream.write(reinterpret_cast<const char *>(file.data()), static_cast<std::streamsize>(file.size())))
            {
                result.error = "Could not write temporary map package.";
                std::filesystem::remove(temporary, ec); return result;
            }
        }
        std::filesystem::path backup;
        const bool replacing = std::filesystem::exists(ioPackagePath, ec);
        if (replacing)
        {
            backup = ioPackagePath;
            backup += std::filesystem::u8path(
                ".backup-" + createEntityId("map_package"));
            std::filesystem::rename(ioPackagePath, backup, ec);
            if (ec)
            {
                result.error = "Could not protect the existing package before overwrite.";
                std::filesystem::remove(temporary, ec); return result;
            }
        }
        std::filesystem::rename(temporary, ioPackagePath, ec);
        if (ec)
        {
            result.error = "Could not commit the map package.";
            if (replacing)
            {
                std::error_code restoreError; std::filesystem::rename(backup, ioPackagePath, restoreError);
            }
            std::filesystem::remove(temporary, ec); return result;
        }
        if (replacing) std::filesystem::remove(backup, ec);
        result.success = true; result.packageBytes = file.size();
    }
    catch (const std::exception &exception)
    {
        result.error = std::string("Map package export failed: ") + exception.what();
    }
    return result;
}
