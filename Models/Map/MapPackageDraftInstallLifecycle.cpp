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

bool MapPackageDraftInstallService::acknowledgeRecovery(
    const MapPackageDraftRecoveryOptions &options,
    const std::string &transactionId,
    std::string *error)
{
    auto fail = [&](const std::string &message) {
        if (error) *error = message;
        return false;
    };
    if (!safeLeaf(transactionId))
        return fail("Map package import recovery identity is invalid.");
    ValidatedRoots roots;
    std::string validationError;
    if (!validateRoots(options.projectRoot, options.assetLibraryDirectory, roots, validationError))
        return fail(validationError);
    const std::filesystem::path transactionRoot = transactionDirectory(roots.assetLibrary, transactionId);
    TransactionRecord record;
    if (!readRecord(transactionRoot / TransactionMarker, options.limits, record, validationError))
        return fail(validationError);
    if (record.transactionId != transactionId)
        return fail("Map package import recovery identity does not match its record.");
    const std::filesystem::path finalDirectory = roots.assetLibrary / record.finalDirectoryName;
    if (!hasOwnership(finalDirectory, transactionId))
        return fail("Map package import recovery ownership marker is missing.");

    std::error_code ec;
    // Remove the recovery record first. A process interruption after this
    // point can leave only a harmless ownership marker, never a replayable
    // transaction that could overwrite a newer draft.
    std::filesystem::remove_all(transactionRoot, ec);
    if (ec)
        return fail("Map package import recovery record could not be cleared.");
    std::filesystem::remove(finalDirectory / OwnershipMarker, ec);
    if (ec)
        return fail("Map package import ownership marker could not be cleared.");
    if (error) error->clear();
    return true;
}

bool MapPackageDraftInstallService::discardRecovery(
    const MapPackageDraftRecoveryOptions &options,
    const std::string &transactionId,
    std::string *error)
{
    auto fail = [&](const std::string &message) {
        if (error) *error = message;
        return false;
    };
    if (!safeLeaf(transactionId))
        return fail("Map package import recovery identity is invalid.");
    ValidatedRoots roots;
    std::string validationError;
    if (!validateRoots(options.projectRoot, options.assetLibraryDirectory, roots, validationError))
        return fail(validationError);
    const std::filesystem::path transactionRoot = transactionDirectory(roots.assetLibrary, transactionId);
    TransactionRecord record;
    if (!readRecord(transactionRoot / TransactionMarker, options.limits, record, validationError))
        return fail(validationError);
    if (record.transactionId != transactionId)
        return fail("Map package import recovery identity does not match its record.");

    const std::filesystem::path payload = transactionRoot / PayloadDirectory;
    const std::filesystem::path finalDirectory = roots.assetLibrary / record.finalDirectoryName;
    std::error_code ec;
    const bool payloadExists = std::filesystem::exists(payload, ec);
    ec.clear();
    const bool finalExists = std::filesystem::exists(finalDirectory, ec);
    if (ec || (payloadExists && finalExists))
        return fail("Interrupted map package import has an ambiguous asset state.");
    if (finalExists && !removeOwnedFinalDirectory(finalDirectory, transactionId, validationError))
        return fail(validationError);
    if (payloadExists && !hasOwnership(payload, transactionId))
        return fail("Map package import discard refused unowned staged assets.");
    std::filesystem::remove_all(transactionRoot, ec);
    if (ec)
        return fail("Map package import recovery record could not be discarded.");
    if (error) error->clear();
    return true;
}

