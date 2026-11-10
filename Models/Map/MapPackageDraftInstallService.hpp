#pragma once

#include "Models/Map/MapPackageService.hpp"

#include <filesystem>
#include <string>

// Installing a portable map package has two different commit domains: the
// embedded files on disk and the MapCreator draft in memory. This service
// keeps both behind one recoverable transaction. A confirmed import is first
// written below a private staging directory. The complete directory is then
// renamed into place and only afterwards is the caller's draft replaced.
enum class MapPackageDraftInstallFailurePoint
{
    None,
    AfterPackageDecode,
    AfterAssetMaterialization,
    AfterAssetDirectoryCommit,
    AfterDraftSwap
};

struct MapPackageDraftInstallOptions
{
    // projectRoot is used only to create portable, project-relative paths in
    // the imported draft. assetLibraryDirectory must be an existing real
    // directory below projectRoot (normally Assets/MapPackages).
    std::filesystem::path projectRoot;
    std::filesystem::path assetLibraryDirectory;
    MapPackageImportOptions importOptions;

    // When true, a successful install leaves its recovery record in place.
    // The UI should keep this record until the imported draft is durably saved,
    // then call acknowledgeRecovery. It may additionally write a crash journal
    // so edits made before that save can be offered as a newer recovery layer.
    bool deferRecoveryAcknowledgement = false;

    // Deterministic failure injection used by the focused transaction tests.
    // Production callers leave this at None. Retaining an injected failure
    // simulates abrupt process termination and permits testing restart recovery.
    MapPackageDraftInstallFailurePoint failurePoint = MapPackageDraftInstallFailurePoint::None;
    bool retainInjectedFailureForRecovery = false;
};

struct MapPackageDraftInstallResult
{
    bool success = false;
    bool recoveryAvailable = false;
    std::string transactionId;
    std::filesystem::path finalAssetDirectory;
    std::string error;
};

struct MapPackageDraftRecoveryOptions
{
    std::filesystem::path projectRoot;
    std::filesystem::path assetLibraryDirectory;
    MapPackageLimits limits;
};

struct MapPackageDraftRecoveryResult
{
    bool found = false;
    bool success = false;
    MapModel map;
    std::string transactionId;
    std::filesystem::path finalAssetDirectory;
    std::string error;
};

class MapPackageDraftInstallService
{
public:
    // On every ordinary failure targetDraft remains unchanged and no imported
    // files remain. A retained injected failure models a killed process: the
    // previous draft still remains unchanged and recoverLatest can replay the
    // already-confirmed import on the next start.
    static MapPackageDraftInstallResult installIntoDraft(
        const std::filesystem::path &packagePath,
        MapModel &targetDraft,
        const MapPackageDraftInstallOptions &options);

    // Finds the newest valid interrupted import without mutating any caller
    // state. Staged files are atomically finalized before the returned draft
    // is exposed. The recovery record remains until explicit acknowledgement.
    static MapPackageDraftRecoveryResult recoverLatest(
        const MapPackageDraftRecoveryOptions &options);

    // Removes the recovery record and its private ownership marker after the
    // caller has durably journaled or saved the returned draft.
    static bool acknowledgeRecovery(
        const MapPackageDraftRecoveryOptions &options,
        const std::string &transactionId,
        std::string *error = nullptr);

    // Cancels an unsaved recovered/imported draft and removes only the final
    // asset directory that still carries this transaction's ownership marker.
    static bool discardRecovery(
        const MapPackageDraftRecoveryOptions &options,
        const std::string &transactionId,
        std::string *error = nullptr);
};
