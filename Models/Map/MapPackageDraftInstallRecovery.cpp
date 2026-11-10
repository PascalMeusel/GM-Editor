#include "Models/Map/MapPackageDraftInstallService.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>
#include <type_traits>
#include <vector>
#include "Models/Map/MapPackageDraftInstallInternal.hpp"


using namespace MapPackageDraftInstallDetail;

MapPackageDraftRecoveryResult MapPackageDraftInstallService::recoverLatest(
    const MapPackageDraftRecoveryOptions &options)
{
    MapPackageDraftRecoveryResult result;
    ValidatedRoots roots;
    if (!validateRoots(options.projectRoot, options.assetLibraryDirectory, roots, result.error))
        return result;

    std::vector<std::filesystem::directory_entry> transactions;
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(roots.assetLibrary, ec))
    {
        if (ec) break;
        const std::string name = entry.path().filename().u8string();
        if (name.rfind(TransactionPrefix, 0) != 0 || entry.is_symlink(ec) || !entry.is_directory(ec))
            continue;
        if (!std::filesystem::exists(entry.path() / TransactionMarker, ec))
        {
            // The commit phase is reached only after the marker was written.
            // Therefore an unmarked private directory is always pre-commit
            // debris and can be removed without touching any final asset tree.
            ec.clear();
            std::filesystem::remove_all(entry.path(), ec);
            if (ec)
            {
                result.error = "Incomplete map package import staging could not be cleaned up.";
                return result;
            }
            continue;
        }
        transactions.push_back(entry);
    }
    if (ec)
    {
        result.error = "Interrupted map package imports could not be inspected.";
        return result;
    }
    if (transactions.empty())
    {
        result.success = true;
        return result;
    }
    std::sort(transactions.begin(), transactions.end(), [](const auto &left, const auto &right) {
        std::error_code leftError;
        std::error_code rightError;
        return left.last_write_time(leftError) > right.last_write_time(rightError);
    });
    result.found = true;
    const std::filesystem::path transactionRoot = transactions.front().path();
    TransactionRecord record;
    if (!readRecord(transactionRoot / TransactionMarker, options.limits, record, result.error))
        return result;
    if (transactionRoot.filename().u8string() != std::string(TransactionPrefix) + record.transactionId)
    {
        result.error = "Map package import recovery identity does not match its directory.";
        return result;
    }
    result.transactionId = record.transactionId;
    result.finalAssetDirectory = roots.assetLibrary / record.finalDirectoryName;
    const std::filesystem::path payload = transactionRoot / PayloadDirectory;
    const bool payloadExists = std::filesystem::exists(payload, ec);
    ec.clear();
    const bool finalExists = std::filesystem::exists(result.finalAssetDirectory, ec);
    if (ec || (payloadExists && finalExists) || (!payloadExists && !finalExists))
    {
        result.error = "Interrupted map package import has an ambiguous asset state.";
        return result;
    }
    if (payloadExists)
    {
        const auto payloadStatus = std::filesystem::symlink_status(payload, ec);
        if (ec || std::filesystem::is_symlink(payloadStatus) ||
            !std::filesystem::is_directory(payloadStatus) ||
            !hasOwnership(payload, record.transactionId))
        {
            result.error = "Interrupted map package import does not own its staged assets.";
            return result;
        }
        std::filesystem::rename(payload, result.finalAssetDirectory, ec);
        if (ec)
        {
            result.error = "Interrupted map package assets could not be committed.";
            return result;
        }
    }
    const auto finalStatus = std::filesystem::symlink_status(result.finalAssetDirectory, ec);
    if (ec || std::filesystem::is_symlink(finalStatus) ||
        !std::filesystem::is_directory(finalStatus) ||
        !hasOwnership(result.finalAssetDirectory, record.transactionId))
    {
        result.error = "Interrupted map package import does not own its asset directory.";
        return result;
    }
    for (const TransactionAssetRecord &asset : record.assets)
    {
        const std::filesystem::path assetPath = result.finalAssetDirectory / asset.relativePath;
        if (!pathInside(result.finalAssetDirectory.lexically_normal(), assetPath.lexically_normal()))
        {
            result.error = "Recovered map package asset escaped its managed directory.";
            return result;
        }
        const std::string actualHash = fileHash(assetPath, options.limits, asset.byteCount, result.error);
        if (actualHash.empty() || actualHash != asset.contentHash)
        {
            if (result.error.empty()) result.error = "Recovered map package asset failed integrity verification.";
            return result;
        }
    }
    MapPackageImportOptions recoveryImport;
    recoveryImport.standaloneCopy = false;
    recoveryImport.remapEntityIds = false;
    recoveryImport.limits = options.limits;
    const MapPackageImportResult imported = MapPackageService::importPackage(
        transactionRoot / RecoveryPackage, recoveryImport);
    if (!imported.success)
    {
        result.error = imported.error;
        return result;
    }
    if (imported.map.metadata.id != record.finalDirectoryName)
    {
        result.error = "Recovered map package draft does not match its asset directory.";
        return result;
    }
    result.map = imported.map;
    result.success = true;
    return result;
}

