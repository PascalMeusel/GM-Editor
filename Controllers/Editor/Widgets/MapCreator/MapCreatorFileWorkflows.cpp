#include "Models/Project/ContextLinkMetadata.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include "MapCreatorController.hpp"
#include <random>
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorFilePathPolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPlayerOutputPolicy.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStrokeAuthoring.hpp"
#include "Models/Map/MapCheckpointService.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Map/MapPackageDraftInstallService.hpp"
#include "Models/Map/MapPackageService.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapWorldBrief.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include "Models/Project/ProjectAssetService.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/WorldInspectorService.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#include <commdlg.h>
#endif

using namespace PersistenceSerialization;

#include "MapCreatorInternals.hpp"
using namespace gm::map_creator_detail;

void MapCreatorController::browseForMapPackageExport()
{
    std::filesystem::path path;
    const std::string suggested = sanitizeFileName(_draftMap.name) + ".ttrpgmappkg";
#if defined(_WIN32)
    if (!browseForSavePath("GM Editor Map Package\0*.ttrpgmappkg\0All Files\0*.*\0",
                           "ttrpgmappkg", suggested, path))
        return;
#else
    if (!mapCreatorManualPathIsReady(_transfer.importExportPath))
    {
        _integration.statusMessage = gm::localization::trUi(
            "Enter a map package path before exporting.");
        return;
    }
    path = mapCreatorPathWithDefaultExtension(
        mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath), ".ttrpgmappkg");
    std::error_code existsError;
    const bool destinationExists = std::filesystem::exists(path, existsError);
    if (existsError || !mapCreatorManualExportMayWrite(true, destinationExists, false))
    {
        _integration.statusMessage = existsError
                             ? gm::localization::trUi(
                                   "The map package destination could not be checked.")
                             : gm::localization::trUi(
                                   "A file already exists at this manual path. Choose a new path before exporting.");
        return;
    }
#endif
    _transfer.importExportPath = mapCreatorFilesystemPathToUtf8(path);
    exportDraftMap();
}

void MapCreatorController::browseForMapPackageImport()
{
    std::filesystem::path path;
#if defined(_WIN32)
    if (!browseForOpenPath("GM Editor Map Package\0*.ttrpgmappkg\0All Files\0*.*\0", path))
        return;
#else
    if (!mapCreatorManualPathIsReady(_transfer.importExportPath))
    {
        _integration.statusMessage = gm::localization::trUi(
            "Enter a map package path before importing.");
        return;
    }
    path = mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath);
#endif
    _transfer.importExportPath = mapCreatorFilesystemPathToUtf8(path);
    previewMapPackageImport();
}

void MapCreatorController::exportDraftMap()
{
    if (_transfer.importExportPath.empty())
    {
        _integration.statusMessage = gm::localization::trUi("Choose a map package destination first.");
        return;
    }

    MapModel normalized = _draftMap;
    resizeMapCells(normalized);
    ensureDefaultMapLayer(normalized);
    MapPackageExportOptions options;
    // The native save dialog already asked the user before replacing a file.
#if defined(_WIN32)
    options.allowOverwrite = true;
#else
    // A typed path has no native overwrite confirmation. The browse wrapper
    // admits only an unused destination and the service closes the race.
    options.allowOverwrite = false;
#endif
    if (normalized.importedReference.enabled && !normalized.importedReference.projectRelativePath.empty())
    {
        // A managed imported reference is also the visual background. Embed it
        // once and avoid carrying a second package entry for the same bytes.
        options.includeBackgroundAsset = false;
        normalized.backgroundImagePath.clear();
        options.importedAssetPath = ProjectAssets::resolveProjectPath(normalized.importedReference.projectRelativePath);
    }
    else if (!normalized.backgroundImagePath.empty())
        options.backgroundAssetPath = ProjectAssets::resolveProjectPath(normalized.backgroundImagePath);
    const MapPackageExportResult result = MapPackageService::exportMap(
        normalized, mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath), options);
    if (!result.success)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map package export failed:")) + " " +
                         gm::localization::trSystemMessage(result.error);
        return;
    }
    _integration.statusMessage = std::string(gm::localization::trUi("Editable map package exported:")) + " " +
                     mapCreatorFilesystemPathToUtf8(result.packagePath);
}

void MapCreatorController::previewMapPackageImport()
{
    _transfer.hasMapPackagePreview = false;
    _transfer.confirmMapPackageImport = false;
    _transfer.mapPackagePreview = {};
    if (_transfer.importExportPath.empty())
    {
        _integration.statusMessage = gm::localization::trUi("Choose a map package first.");
        return;
    }
    _transfer.mapPackagePreview = MapPackageService::previewImport(
        mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath));
    _transfer.hasMapPackagePreview = _transfer.mapPackagePreview.valid;
    // A structurally valid package with duplicate owned identities still
    // opens the review so the GM can see the validated blocker.  Only the
    // confirm path below is allowed to apply an eligible package.
    _transfer.confirmMapPackageImport = _transfer.mapPackagePreview.valid;
    if (!_transfer.mapPackagePreview.valid)
        _integration.statusMessage = std::string(gm::localization::trUi("Map package could not be read:")) + " " +
                         gm::localization::trSystemMessage(_transfer.mapPackagePreview.error);
    else if (!_transfer.mapPackagePreview.importEligible)
        _integration.statusMessage = gm::localization::trUi(
            "Map package validation found conflicting internal identities. Review the blocker before importing.");
    else
        _integration.statusMessage = gm::localization::trUi("Map package is valid. Review it before importing.");
}

void MapCreatorController::confirmMapPackageImport()
{
    if (!_transfer.hasMapPackagePreview || _transfer.importExportPath.empty())
    {
        _integration.statusMessage = gm::localization::trUi("Preview a valid map package before importing it.");
        return;
    }
    if (!mapCreatorMapPackageCanImport(
            _transfer.mapPackagePreview.valid, _transfer.mapPackagePreview.importEligible))
    {
        _integration.statusMessage = !_transfer.mapPackagePreview.valid
                             ? gm::localization::trUi(
                                   "Preview a valid map package before importing it.")
                             : gm::localization::trUi(
                                   "This map package cannot be imported until its conflicting internal identities are repaired.");
        return;
    }

    const std::filesystem::path projectRoot = ProjectAssets::findProjectRoot();
    const std::filesystem::path assetLibrary =
        ProjectAssets::campaignAssetDirectory(_currentCampaign, "MapPackages");
    std::error_code directoryError;
    std::filesystem::create_directories(assetLibrary, directoryError);
    if (directoryError)
    {
        _integration.statusMessage = gm::localization::trUi("The embedded map assets could not be prepared.");
        return;
    }

    // Prepare the previous state before the filesystem commit. Once the
    // service succeeds, moving this snapshot into history does not allocate
    // or copy the large map model and the import remains one undoable edit.
    UndoSnapshot previousDraft{_draftMap, "Import Map Package", _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()};
    _history.undo.reserve(std::max<std::size_t>(_history.undo.capacity(), _history.undo.size() + 1));
    MapPackageDraftInstallOptions installOptions;
    installOptions.projectRoot = projectRoot;
    installOptions.assetLibraryDirectory = assetLibrary;
    installOptions.deferRecoveryAcknowledgement = true;
    const MapPackageDraftInstallResult installed =
        MapPackageDraftInstallService::installIntoDraft(
            mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath), _draftMap, installOptions);
    if (!installed.success)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map package import failed:")) + " " +
                         gm::localization::trSystemMessage(installed.error);
        return;
    }

    _history.undo.push_back(std::move(previousDraft));
    while (_history.undo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
        _history.undo.erase(_history.undo.begin());
    _history.redo.clear();
    _history.crashJournalPending = true;
    _history.crashJournalLabel = "Import Map Package";
    refreshUndoState();
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    _draftMap.metadata.touch();
    ++_draftMap.revision;
    _persistence.editingMapIndex = -1;
    _persistence.selectedMapIndex = -1;
    _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
    _generation.selectedCreationIntentIndex = static_cast<int>(_draftMap.creationIntent);
    restoreTravelSetupFromRecipe();
    _generation.selectedSizePresetIndex = static_cast<int>(_draftMap.sizePreset);
    clearObjectSelection();
    syncReferenceCalibrationDraft();
    _transfer.confirmMapPackageImport = false;
    _transfer.hasMapPackagePreview = false;
    _transfer.mapPackagePreview = {};
    _history.namedCheckpoints.clear();
    _history.selectedNamedCheckpointIndex = -1;
    _history.namedCheckpointRestorePreview = {};
    _history.hasNamedCheckpointRestorePreview = false;
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    _history.pendingNamedCheckpointLabel = gm::localization::trUi("Map package imported");
    _transfer.pendingPackageImportTransactionId = installed.transactionId;

    // Persist the new in-memory draft immediately. The install marker remains
    // until saveDraftMap commits the campaign, so even a later process crash
    // can restore this otherwise-unsaved imported draft.
    bool recoveryRecorded = false;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService checkpointService(
            ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        _history.crashJournalWorker.cancel();
        _history.journalSubmittedSerial.reset();
        const MapCheckpointWriteResult journal =
            checkpointService.writeCrashJournal(_draftMap, "Import Map Package");
        recoveryRecorded = journal.success;
        if (!journal.success)
            _integration.statusMessage = std::string(gm::localization::trUi(
                "The map package was imported, but its recovery snapshot could not be written:")) +
                " " + gm::localization::trSystemMessage(journal.error);
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi(
            "The map package was imported, but its recovery snapshot could not be written:")) +
            " " + gm::localization::trSystemMessage(error.what());
    }

    if (recoveryRecorded)
    {
        _history.crashJournalPending = false;
        _history.crashJournalWriteFailed = false;
        _history.crashJournalLabel.clear();
        _integration.statusMessage = gm::localization::trUi(
            "Editable map package imported as an independent copy.");
    }
}

void MapCreatorController::importDraftMap()
{
    confirmMapPackageImport();
}

void MapCreatorController::exportDraftMapLegacy()
{
    try
    {
        MapModel normalized = _draftMap;
        resizeMapCells(normalized);
        ensureDefaultMapLayer(normalized);

        std::filesystem::path path = _transfer.importExportPath.empty()
                                         ? std::filesystem::path("Resources") / "Maps" / "Exports" / (sanitizeFileName(normalized.name) + ".ttrpgmap")
                                         : mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath);
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path, std::ios::trunc);
        file << "TTRPG_MAP_V2\n";
        file << normalized.name << "\n";
        file << normalized.description << "\n";
        file << static_cast<int>(normalized.scope) << " " << static_cast<int>(normalized.gridType) << " " << normalized.width << " " << normalized.height << " " << normalized.cellSize << "\n";
        file << normalized.backgroundImagePath << "\n";
        file << normalized.backgroundImageOpacity << "\n";
        file << serializeMapCells(normalized.cells) << "\n";
        file << serializeMapRegions(normalized.regions) << "\n";
        file << serializeMapFeatures(normalized.features) << "\n";
        file << serializeMapRoutes(normalized.routes) << "\n";
        const std::string generationData = serializeMapGenerationMetadata(normalized.generation);
        file << generationData.size() << "\n";
        file.write(generationData.data(), static_cast<std::streamsize>(generationData.size()));
        _transfer.importExportPath = mapCreatorFilesystemPathToUtf8(path);
        _integration.statusMessage = std::string(gm::localization::trUi("Exported map to")) + " " + _transfer.importExportPath;
    }
    catch (const std::exception &e)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map export failed:")) + " " +
                         gm::localization::trSystemMessage(e.what());
    }
}
