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

MapPackageDraftInstallResult MapPackageDraftInstallService::installIntoDraft(
    const std::filesystem::path &packagePath,
    MapModel &targetDraft,
    const MapPackageDraftInstallOptions &options)
{
    MapPackageDraftInstallResult result;
    ValidatedRoots roots;
    if (!validateRoots(options.projectRoot, options.assetLibraryDirectory, roots, result.error))
        return result;

    const MapPackageImportResult imported = MapPackageService::importPackage(packagePath, options.importOptions);
    if (!imported.success)
    {
        result.error = imported.error;
        return result;
    }
    if (shouldFail(options, MapPackageDraftInstallFailurePoint::AfterPackageDecode))
    {
        result.error = "Injected map package import failure after package decode.";
        return result;
    }

    MapModel replacement = imported.map;
    if (!safeLeaf(replacement.metadata.id))
    {
        result.error = "Imported map identity is unsafe for managed asset storage.";
        return result;
    }
    result.transactionId = createShortTransactionId();
    if (!safeLeaf(result.transactionId))
    {
        result.error = "Map package import transaction identity is invalid.";
        return result;
    }
    const std::filesystem::path transactionRoot = transactionDirectory(roots.assetLibrary, result.transactionId);
    const std::filesystem::path payload = transactionRoot / PayloadDirectory;
    result.finalAssetDirectory = roots.assetLibrary / replacement.metadata.id;
    std::error_code ec;
    if (std::filesystem::exists(result.finalAssetDirectory, ec) || ec)
    {
        result.error = "Map package asset destination already exists.";
        return result;
    }

    bool finalDirectoryCommitted = false;
    bool draftSwapped = false;
    const auto cleanup = [&](bool retain) {
        if (retain)
        {
            result.recoveryAvailable = true;
            return;
        }
        std::string cleanupError;
        if (finalDirectoryCommitted &&
            !removeOwnedFinalDirectory(result.finalAssetDirectory, result.transactionId, cleanupError) &&
            result.error.empty())
            result.error = cleanupError;
        std::filesystem::remove_all(transactionRoot, ec);
    };

    try
    {
        if (!std::filesystem::create_directory(transactionRoot, ec) || ec ||
            !std::filesystem::create_directory(payload, ec) || ec)
        {
            result.error = "Map package import staging directory could not be created.";
            cleanup(false);
            return result;
        }
        MapPackageMaterializationOptions materialization;
        materialization.targetDirectory = payload;
        materialization.limits = options.importOptions.limits;
        const MapPackageMaterializationResult materialized =
            MapPackageService::materializeAssets(imported.assets, materialization);
        if (!materialized.success)
        {
            result.error = materialized.error;
            cleanup(false);
            return result;
        }

        TransactionRecord record;
        record.transactionId = result.transactionId;
        record.finalDirectoryName = replacement.metadata.id;
        patchImportedAssetPaths(replacement, materialized.assets, roots.project,
                                result.finalAssetDirectory, record);
        std::string writeError;
        if (!writeTextFile(payload / OwnershipMarker, result.transactionId + "\n", writeError))
        {
            result.error = writeError;
            cleanup(false);
            return result;
        }
        MapPackageExportOptions recoveryExport;
        recoveryExport.includeBackgroundAsset = false;
        recoveryExport.includeImportedAsset = false;
        recoveryExport.includeWorkingProxyAsset = false;
        recoveryExport.limits = options.importOptions.limits;
        const MapPackageExportResult recoveryPackage = MapPackageService::exportPackage(
            replacement, transactionRoot / RecoveryPackage, recoveryExport);
        if (!recoveryPackage.success)
        {
            result.error = std::string("Map package import recovery snapshot failed: ") + recoveryPackage.error;
            cleanup(false);
            return result;
        }
        if (!writeTextFile(transactionRoot / TransactionMarker, serializeRecord(record), writeError))
        {
            result.error = writeError;
            cleanup(false);
            return result;
        }

        if (shouldFail(options, MapPackageDraftInstallFailurePoint::AfterAssetMaterialization))
        {
            result.error = "Injected map package import failure after asset materialization.";
            cleanup(options.retainInjectedFailureForRecovery);
            return result;
        }

        std::filesystem::rename(payload, result.finalAssetDirectory, ec);
        if (ec)
        {
            result.error = "Map package assets could not be atomically committed.";
            cleanup(false);
            return result;
        }
        finalDirectoryCommitted = true;
        if (shouldFail(options, MapPackageDraftInstallFailurePoint::AfterAssetDirectoryCommit))
        {
            result.error = "Injected map package import failure after asset directory commit.";
            cleanup(options.retainInjectedFailureForRecovery);
            return result;
        }

        using std::swap;
        swap(targetDraft, replacement);
        draftSwapped = true;
        if (shouldFail(options, MapPackageDraftInstallFailurePoint::AfterDraftSwap))
        {
            swap(targetDraft, replacement);
            draftSwapped = false;
            result.error = "Injected map package import failure after draft swap.";
            cleanup(options.retainInjectedFailureForRecovery);
            return result;
        }

        result.success = true;
        if (options.deferRecoveryAcknowledgement)
        {
            result.recoveryAvailable = true;
            return result;
        }
        std::string acknowledgeError;
        MapPackageDraftRecoveryOptions recoveryOptions;
        recoveryOptions.projectRoot = roots.project;
        recoveryOptions.assetLibraryDirectory = roots.assetLibrary;
        recoveryOptions.limits = options.importOptions.limits;
        if (!acknowledgeRecovery(recoveryOptions, result.transactionId, &acknowledgeError))
        {
            swap(targetDraft, replacement);
            draftSwapped = false;
            result.success = false;
            result.error = acknowledgeError;
            cleanup(false);
            return result;
        }
        result.recoveryAvailable = false;
        return result;
    }
    catch (const std::exception &exception)
    {
        if (draftSwapped)
        {
            using std::swap;
            swap(targetDraft, replacement);
        }
        result.success = false;
        result.error = std::string("Map package draft transaction failed: ") + exception.what();
        cleanup(false);
        return result;
    }
}
