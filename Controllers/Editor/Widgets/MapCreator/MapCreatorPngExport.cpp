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

MapModel MapCreatorController::currentPlayerOutputMap() const
{
    return makeMapCreatorCampaignPlayerOutputSnapshot(_draftMap, _persistence.playerSettingsBaseline, _currentCampaign);
}

MapPngExportOptions MapCreatorController::selectedPngExportOptions(bool playerSafe) const
{
    const PngExportVisibilitySettings &settings = playerSafe ? _transfer.exportPlayerSettings : _transfer.exportGmSettings;
    MapPngExportOptions options;
    options.pixelsPerCell = selectedPngExportPixelsPerCell();
    options.locale = gm::localization::LocalizationService::instance().locale() ==
                             gm::localization::Locale::German ? "de" : "en";
    options.playerSafe = playerSafe;
    options.applyFog = playerSafe || settings.applyFog;
    options.showGrid = settings.showGrid;
    options.showLabels = settings.showLabels;
    options.showPoliticalOverlay = settings.showPoliticalOverlay;
    options.showAnnotations = settings.showAnnotations;
    options.showRoutes = settings.showRoutes;
    options.showTokens = settings.showTokens;
    options.showBackground = settings.showBackground;
    return options;
}

void MapCreatorController::startPngExport(MapPngExportJob job)
{
    const bool uvtt = job.universalVtt;
    try
    {
        _transfer.pngExportTask.start(std::move(job));
        _pngExportEpoch = _campaignEpoch;
        _integration.statusMessage = uvtt
            ? (gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                ? "UVTT wird im Hintergrund vorbereitet. Änderungen verwerfen den laufenden Export."
                : "Preparing UVTT in the background. Editing cancels the pending export.")
            : gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
            ? "PNG wird im Hintergrund vorbereitet. Änderungen verwerfen den laufenden Export."
            : "Preparing PNG in the background. Editing cancels the pending export.";
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("PNG export failed:")) + " " + error.what();
    }
}

void MapCreatorController::updatePngExport()
{
    auto result = _transfer.pngExportTask.takeReady();
    if (!result) return;
    // A same-ID reload can reuse map IDs and authoring serials. Publication
    // therefore also requires the exact campaign lifetime which started it.
    if (_pngExportEpoch != _campaignEpoch) return;
    const MapModel player = result->job.options.playerSafe ? currentPlayerOutputMap() : MapModel{};
    const bool consent = !result->job.options.playerSafe ||
        (player.visibility == MapVisibility::PlayersVisible &&
         mapPlayerReleasedRenderLayers(player) == mapPlayerReleasedRenderLayers(result->job.map));
    if (!result->publish(_draftMap.metadata.id, _editing.authoringSerial, consent))
    {
        const bool stale = result->job.map.metadata.id != _draftMap.metadata.id ||
            result->job.authoringSerial != _editing.authoringSerial || !consent;
        if (stale)
            _integration.statusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                ? "Karte oder Spielerfreigabe wurde geändert. Bitte erneut exportieren."
                : "The map or player review changed. Export again.";
        else
            _integration.statusMessage = std::string(gm::localization::trUi("PNG export failed:")) + " " +
                gm::localization::trSystemMessage(result->error);
        return;
    }
    _transfer.lastExportPath = mapCreatorFilesystemPathToUtf8(result->job.destination);
    _integration.statusMessage = result->job.universalVtt ? "UVTT: " + _transfer.lastExportPath
        : std::string(gm::localization::trUi(result->job.options.playerSafe
        ? "Player-safe PNG exported:" : "GM PNG exported:")) + " " + _transfer.lastExportPath;
}

void MapCreatorController::exportDraftPng(bool playerSafe)
{
    if (_transfer.pngExportTask.busy()) return;
    const int pixelsPerCell = selectedPngExportPixelsPerCell();
    if (_transfer.exportCurrentViewport && !_transfer.exportViewportValid)
    {
        _integration.statusMessage = gm::localization::trUi("The current map view is not available. Fit or open the map first.");
        return;
    }
    const MapPngExportSizing sizing = _transfer.exportCurrentViewport
                                         ? calculateMapPngExportSizing(_transfer.exportViewportRect, pixelsPerCell)
                                         : calculateMapPngExportSizing(_draftMap, pixelsPerCell);
    if (!sizing.valid)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("PNG export failed:")) + " " +
                         gm::localization::trSystemMessage(sizing.error);
        return;
    }

    std::filesystem::path path = std::filesystem::path("Resources") / "Maps" / "Exports" /
                                 (sanitizeFileName(_draftMap.name) +
                                  (_transfer.exportCurrentViewport ? "_view" : "") +
                                  (playerSafe ? "_player.png" : "_gm.png"));
#if defined(_WIN32)
    if (!browseForSavePath("PNG Image\0*.png\0All Files\0*.*\0", "png",
                           mapCreatorFilesystemPathToUtf8(path.filename()), path))
        return;
#else
    if (!mapCreatorManualPathIsReady(_transfer.manualPngExportPath))
    {
        _integration.statusMessage = gm::localization::trUi(
            "Enter a PNG destination path before exporting.");
        return;
    }
    path = mapCreatorPathWithDefaultExtension(
        mapCreatorFilesystemPathFromUtf8(_transfer.manualPngExportPath), ".png");
    std::error_code existsError;
    const bool destinationExists = std::filesystem::exists(path, existsError);
    if (existsError || !mapCreatorManualExportMayWrite(true, destinationExists, false))
    {
        _integration.statusMessage = existsError
                             ? gm::localization::trUi(
                                   "The PNG destination could not be checked.")
                             : gm::localization::trUi(
                                   "A PNG already exists at this manual path. Choose a new path before exporting.");
        return;
    }
#endif
    _transfer.manualPngExportPath = mapCreatorFilesystemPathToUtf8(path);
    const std::optional<MapModel> playerSnapshot = playerSafe
        ? std::optional<MapModel>{currentPlayerOutputMap()} : std::nullopt;
    const MapModel &outputMap = playerSnapshot ? *playerSnapshot : _draftMap;
    MapPngExportJob job;
    job.map = outputMap;
    job.options = selectedPngExportOptions(playerSafe);
    job.destination = path;
    job.authoringSerial = _editing.authoringSerial;
    job.overwrite = true; // The native save dialog obtained replacement consent.
    if (_transfer.exportCurrentViewport) job.viewport = _transfer.exportViewportRect;
    startPngExport(std::move(job));
}

void MapCreatorController::exportDraftPngTiled(bool playerSafe, bool overwriteExisting)
{
    if (_transfer.pngExportTask.busy()) return;
    std::string fileStem = sanitizeFileName(_draftMap.name) +
                           (playerSafe ? "_player" : "_gm");
    std::filesystem::path manifestPath = std::filesystem::path("Resources") / "Maps" / "Exports" /
                                         (fileStem + "_tiles.json");
#if defined(_WIN32)
    if (!browseForSavePath("JSON Manifest\0*.json\0All Files\0*.*\0", "json",
                           mapCreatorFilesystemPathToUtf8(manifestPath.filename()), manifestPath))
        return;
#else
    if (!mapCreatorManualPathIsReady(_transfer.manualTiledPngManifestPath))
    {
        _integration.statusMessage = gm::localization::trUi(
            "Enter a tiled PNG manifest path before exporting.");
        return;
    }
    manifestPath = mapCreatorPathWithDefaultExtension(
        mapCreatorFilesystemPathFromUtf8(_transfer.manualTiledPngManifestPath), ".json");
#endif
    _transfer.manualTiledPngManifestPath = mapCreatorFilesystemPathToUtf8(manifestPath);
    std::string selectedStem = sanitizeFileName(
        mapCreatorFilesystemPathToUtf8(manifestPath.stem()));
    constexpr std::string_view tilesSuffix = "_tiles";
    if (selectedStem.size() > tilesSuffix.size() &&
        selectedStem.compare(selectedStem.size() - tilesSuffix.size(), tilesSuffix.size(), tilesSuffix) == 0)
        selectedStem.erase(selectedStem.size() - tilesSuffix.size());
    if (!selectedStem.empty())
        fileStem = selectedStem;
    std::filesystem::path directory = manifestPath.parent_path();
    if (directory.empty())
        directory = std::filesystem::current_path();

    const std::optional<MapModel> playerSnapshot = playerSafe
        ? std::optional<MapModel>{currentPlayerOutputMap()} : std::nullopt;
    const MapModel &outputMap = playerSnapshot ? *playerSnapshot : _draftMap;
    MapPngExportJob job;
    job.map = outputMap;
    job.options = selectedPngExportOptions(playerSafe);
    job.destination = directory / (fileStem + "_tiles.json");
    job.tileStem = fileStem;
    job.authoringSerial = _editing.authoringSerial;
    job.overwrite = overwriteExisting;
    startPngExport(std::move(job));
}

int MapCreatorController::selectedPngExportPixelsPerCell() const
{
    const auto preset = static_cast<MapPngExportSizePreset>(std::clamp(
        _transfer.exportPngSizePresetIndex,
        static_cast<int>(MapPngExportSizePreset::Screen),
        static_cast<int>(MapPngExportSizePreset::Custom)));
    if (_transfer.exportCurrentViewport && _transfer.exportViewportValid)
    {
        MapModel viewportDimensions;
        viewportDimensions.width = _transfer.exportViewportRect.width;
        viewportDimensions.height = _transfer.exportViewportRect.height;
        return mapPngPixelsPerCellForPreset(viewportDimensions, preset, _transfer.exportPixelsPerCell);
    }
    return mapPngPixelsPerCellForPreset(_draftMap, preset, _transfer.exportPixelsPerCell);
}

MapPngExportSizing MapCreatorController::selectedPngExportSizing() const
{
    if (_transfer.exportCurrentViewport && _transfer.exportViewportValid)
        return calculateMapPngExportSizing(_transfer.exportViewportRect, selectedPngExportPixelsPerCell());
    return calculateMapPngExportSizing(_draftMap, selectedPngExportPixelsPerCell());
}
