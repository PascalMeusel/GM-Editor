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

void MapCreatorController::importDraftMapLegacy()
{
    try
    {
        if (_transfer.importExportPath.empty())
        {
            _integration.statusMessage = gm::localization::trUi("Enter an import path first.");
            return;
        }

        std::ifstream file(mapCreatorFilesystemPathFromUtf8(_transfer.importExportPath));
        std::string marker;
        std::getline(file, marker);
        const bool version2 = marker == "TTRPG_MAP_V2";
        if (!version2 && marker != "TTRPG_MAP_V1")
        {
            _integration.statusMessage = gm::localization::trUi("Unsupported map import file.");
            return;
        }

        MapModel imported;
        std::getline(file, imported.name);
        std::getline(file, imported.description);
        std::string dimensions;
        std::getline(file, dimensions);
        std::stringstream dimensionStream(dimensions);
        int scope = 0;
        int grid = 1;
        dimensionStream >> scope >> grid >> imported.width >> imported.height >> imported.cellSize;
        imported.scope = static_cast<MapScope>(std::clamp(scope, 0, 2));
        imported.gridType = static_cast<GridType>(std::clamp(grid, 0, 2));
        std::getline(file, imported.backgroundImagePath);
        std::string opacity;
        std::getline(file, opacity);
        imported.backgroundImageOpacity = opacity.empty() ? 0.65f : std::stof(opacity);
        std::string line;
        std::getline(file, line);
        imported.cells = deserializeMapCells(line);
        std::getline(file, line);
        imported.regions = deserializeMapRegions(line);
        std::getline(file, line);
        imported.features = deserializeMapFeatures(line);
        std::getline(file, line);
        imported.routes = deserializeMapRoutes(line);
        if (version2)
        {
            std::getline(file, line);
            const std::size_t generationDataSize = line.empty() ? 0 : static_cast<std::size_t>(std::stoull(line));
            std::string generationData(generationDataSize, '\0');
            if (generationDataSize > 0)
                file.read(generationData.data(), static_cast<std::streamsize>(generationDataSize));
            imported.generation = deserializeMapGenerationMetadata(generationData);
        }
        resizeMapCells(imported);
        ensureDefaultMapLayer(imported);
        captureUndoSnapshot("Import Legacy Map");
        _draftMap = std::move(imported);
        _draftMap.metadata.touch();
        ++_draftMap.revision;
        _persistence.editingMapIndex = -1;
        _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
        _generation.selectedCreationIntentIndex = static_cast<int>(inferredCreationIntent(_draftMap));
        _draftMap.creationIntent = static_cast<MapCreationIntent>(_generation.selectedCreationIntentIndex);
        _generation.selectedSizePresetIndex = static_cast<int>(inferredSizePreset(
            _draftMap.creationIntent, _draftMap.width, _draftMap.height));
        _draftMap.sizePreset = static_cast<MapSizePreset>(_generation.selectedSizePresetIndex);
        clearObjectSelection();
        syncReferenceCalibrationDraft();
        _history.pendingNamedCheckpointLabel = gm::localization::trUi("Legacy map imported");
        _integration.statusMessage = gm::localization::trUi("Imported map draft.");
    }
    catch (const std::exception &e)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map import failed:")) + " " +
                         gm::localization::trSystemMessage(e.what());
    }
}

void MapCreatorController::browseForReferenceImage(bool preserveExistingMapContract)
{
    std::filesystem::path sourcePath;
    if (!browseForOpenPath("Map Images\0*.png;*.jpg;*.jpeg\0PNG Images\0*.png\0JPEG Images\0*.jpg;*.jpeg\0", sourcePath))
    {
#if !defined(_WIN32)
        _integration.statusMessage = gm::localization::trUi("Native image browsing is not available on this platform.");
#endif
        return;
    }

    MapModel importedDraft = _draftMap;
    MapImageImport::Request request;
    request.sourcePath = sourcePath;
    request.intent = static_cast<MapCreationIntent>(std::clamp(
        _generation.selectedCreationIntentIndex,
        static_cast<int>(MapCreationIntent::WorldOverview),
        static_cast<int>(MapCreationIntent::TacticalEncounter)));
    request.sizePreset = static_cast<MapSizePreset>(std::clamp(
        _generation.selectedSizePresetIndex,
        static_cast<int>(MapSizePreset::Small),
        static_cast<int>(MapSizePreset::Custom)));
    request.crop = {0.0f, 0.0f, 1.0f, 1.0f};
    request.rotationDegrees = 0.0f;
    request.spatialReference = importedDraft.spatialReference;
    request.preserveExistingMapContract = preserveExistingMapContract &&
                                          importedDraft.importedReference.enabled;
    const MapImageImport::Result result =
        MapImageImport::importReference(_currentCampaign, importedDraft, request);
    if (!result.success)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map image import failed:")) + " " +
                         localizedMapImageImportError(result.errorCode);
        return;
    }

    captureUndoSnapshot("Import Map Image");
    _draftMap = std::move(importedDraft);
    if (!request.preserveExistingMapContract &&
        (_draftMap.name.empty() || _draftMap.name == "New Map" ||
         _draftMap.name == gm::localization::trUi("New Map")))
        _draftMap.name = mapCreatorFilesystemPathToUtf8(sourcePath.stem());
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
    _generation.selectedCreationIntentIndex = static_cast<int>(_draftMap.creationIntent);
    restoreTravelSetupFromRecipe();
    _generation.selectedSizePresetIndex = static_cast<int>(_draftMap.sizePreset);
    syncReferenceCalibrationDraft();
    _integration.statusMessage = gm::localization::trUi(
        request.preserveExistingMapContract
            ? "Map image replaced. Cells, features, fog, tokens, and atlas links were preserved."
            : "Map image imported as a managed GM-only reference. Calibrate it and add map features in Edit.");
    _history.pendingNamedCheckpointLabel = gm::localization::trUi(
        request.preserveExistingMapContract ? "Map image replaced" : "Map image imported");
    if (_view)
        _view->setWorkflowStep(3);
}

void MapCreatorController::adoptLegacyReferenceImage()
{
    if (_draftMap.importedReference.enabled || _draftMap.backgroundImagePath.empty())
        return;

    MapModel adopted = _draftMap;
    const MapImageImport::Result result = MapImageImport::adoptLegacyReference(_currentCampaign, adopted);
    if (!result.success)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("The legacy map image could not be copied into the campaign:")) +
                         " " + localizedMapImageImportError(result.errorCode);
        return;
    }

    captureUndoSnapshot("Adopt Legacy Map Image");
    _draftMap = std::move(adopted);
    syncReferenceCalibrationDraft();
    _history.pendingNamedCheckpointLabel = gm::localization::trUi("Legacy map image adopted");
    _integration.statusMessage = gm::localization::trUi(
        "The legacy image is now a managed GM-only campaign asset. Map content and atlas placement were preserved.");
}

bool MapCreatorController::verifyReferenceImageForPlayerPublishing()
{
    const MapImageImport::ManagedReferenceInspection inspection =
        MapImageImport::inspectManagedReference(_draftMap.importedReference, true);
    if (!inspection.healthy)
    {
        _integration.statusMessage = gm::localization::trUi(
            MapImageImport::managedReferenceErrorMessage(inspection.errorCode));
        return false;
    }
    _integration.statusMessage = gm::localization::trUi("Map image integrity verified.");
    return true;
}

bool MapCreatorController::setReferenceImagePlayerVisible(bool visible)
{
    if (!_draftMap.importedReference.enabled)
        return false;
    if (visible && !verifyReferenceImageForPlayerPublishing())
        return false;
    if (_draftMap.importedReference.gmOnly == !visible)
        return true;

    captureUndoSnapshot("Change Map Image Visibility");
    _draftMap.importedReference.gmOnly = !visible;
    ++_draftMap.revision;
    _draftMap.metadata.touch();
    _integration.statusMessage = gm::localization::trUi(
        visible ? "Map image allowed in player output." : "Map image hidden from player output.");
    return true;
}

void MapCreatorController::syncReferenceCalibrationDraft()
{
    const MapNormalizedRect &crop = _draftMap.importedReference.crop;
    _transfer.importCropX = crop.x;
    _transfer.importCropY = crop.y;
    _transfer.importCropWidth = crop.width > 0.0f ? crop.width : 1.0f;
    _transfer.importCropHeight = crop.height > 0.0f ? crop.height : 1.0f;
    _transfer.importRotationDegrees = _draftMap.importedReference.rotationDegrees;
    _transfer.importSpatialCalibrated = _draftMap.spatialReference.calibrated;
    _transfer.importExtentWidth = _draftMap.spatialReference.extentWidth;
    _transfer.importExtentHeight = _draftMap.spatialReference.extentHeight;
    _transfer.importDistanceUnitIndex = std::clamp(static_cast<int>(_draftMap.spatialReference.unit),
                                          static_cast<int>(MapDistanceUnit::Unspecified),
                                          static_cast<int>(MapDistanceUnit::Feet));
}

void MapCreatorController::applyReferenceCalibration()
{
    if (!_draftMap.importedReference.enabled)
        return;

    MapNormalizedRect crop;
    crop.x = std::clamp(_transfer.importCropX, 0.0f, 0.99f);
    crop.y = std::clamp(_transfer.importCropY, 0.0f, 0.99f);
    crop.width = std::clamp(_transfer.importCropWidth, 0.01f, 1.0f - crop.x);
    crop.height = std::clamp(_transfer.importCropHeight, 0.01f, 1.0f - crop.y);
    MapSpatialReference spatial = _draftMap.spatialReference;
    spatial.calibrated = _transfer.importSpatialCalibrated;
    spatial.extentWidth = std::max(0.0f, _transfer.importExtentWidth);
    spatial.extentHeight = std::max(0.0f, _transfer.importExtentHeight);
    spatial.unit = static_cast<MapDistanceUnit>(std::clamp(
        _transfer.importDistanceUnitIndex,
        static_cast<int>(MapDistanceUnit::Unspecified),
        static_cast<int>(MapDistanceUnit::Feet)));

    MapModel calibrated = _draftMap;
    std::string errorCode;
    if (!MapImageImport::updateCalibration(calibrated, crop,
                                            std::clamp(_transfer.importRotationDegrees, -180.0f, 180.0f),
                                            spatial, &errorCode))
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map calibration could not be applied:")) + " " +
                         localizedMapImageImportError(errorCode);
        return;
    }
    captureUndoSnapshot("Calibrate Map Image");
    _draftMap = std::move(calibrated);
    syncReferenceCalibrationDraft();
    _integration.statusMessage = gm::localization::trUi("Map image calibration applied.");
}

bool MapCreatorController::commitImageTrace(const MapImageImport::TraceRequest &request)
{
    if (_generation.inProgress)
    {
        _integration.statusMessage = gm::localization::trUi("Wait for map generation to finish before tracing.");
        return false;
    }

    // Validate and build on a detached copy first. This guarantees a rejected
    // trace does not create an empty undo entry or partially mutate the draft.
    MapModel traced = _draftMap;
    EntityId createdFeatureId;
    std::string errorCode;
    if (!MapImageImport::addTraceFeature(traced, request, &createdFeatureId, &errorCode))
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map feature could not be traced:")) + " " +
                         localizedMapImageImportError(errorCode);
        return false;
    }

    captureUndoSnapshot("Trace Map Image Feature");
    _draftMap = std::move(traced);
    _history.pendingNamedCheckpointLabel = gm::localization::trUi("Map image feature traced");
    _integration.statusMessage = gm::localization::trUi("Map image feature traced. The control points remain editable and undoable.");
    return true;
}

void MapCreatorController::removeReferenceImage()
{
    if (!_draftMap.importedReference.enabled)
        return;
    MapModel withoutReference = _draftMap;
    MapImageImport::removeReference(withoutReference);
    captureUndoSnapshot("Remove Map Image");
    _draftMap = std::move(withoutReference);
    syncReferenceCalibrationDraft();
    _integration.statusMessage = gm::localization::trUi("Map image reference removed. Traced features were kept.");
}
