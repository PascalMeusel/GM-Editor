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

MapCreatorController::MapCreatorController(Campaign &currentCampaign, MapCrashJournalWorker::BeforePublish journalPublish)
    : _currentCampaign(currentCampaign), _history(std::move(journalPublish))
{
    _view = std::make_unique<MapCreatorView>(*this);
    startNewMap();
    recoverInterruptedMapPackageImport();
}

bool MapCreatorController::openMapForEditing(const EntityId &mapId)
{
    const auto selected = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(),
                                      [&](const MapModel &map) { return map.metadata.id == mapId; });
    if (selected == _currentCampaign.maps.end())
        return false;

    const int selectedIndex = static_cast<int>(std::distance(_currentCampaign.maps.begin(), selected));
    show();
    if (_draftMap.metadata.id == mapId && _persistence.editingMapIndex >= 0)
    {
        _persistence.selectedMapIndex = selectedIndex;
        _persistence.editingMapIndex = selectedIndex;
        // A clean editor can safely refresh changes made in Atlas or the
        // presentation panel. Keep actual authoring drafts for the save merge.
        if (!hasUnsavedDraft() && _draftMap.revision != selected->revision)
            loadSelectedMap();
        _view->setWorkflowStep(3);
        return true;
    }
    if (_generation.inProgress || _generation.queued)
    {
        _integration.statusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                             ? "Warte, bis die Kartenerzeugung abgeschlossen ist, bevor du eine andere Karte lädst."
                             : "Wait for map generation to finish before loading another map.";
        return false;
    }

    _persistence.selectedMapIndex = selectedIndex;
    if (hasUnsavedDraft())
    {
        // Reuse the existing replacement review instead of silently losing
        // another map's draft when navigation originates outside the editor.
        _view->requestLoadMapReview();
        return true;
    }
    loadSelectedMap();
    if (_draftMap.metadata.id != mapId || _persistence.editingMapIndex != _persistence.selectedMapIndex)
        return false;
    setActiveTool(Tool::InspectSelect);
    _view->setWorkflowStep(3);
    return true;
}

void MapCreatorController::beginNewMap(MapCreationIntent intent)
{
    if (generationBusy())
    {
        _integration.statusMessage = gm::localization::trUi("Wait for map generation to finish before starting a new map.");
        return;
    }
    if (hasUnsavedDraft()) _view->requestNewMapReview(intent);
    else startNewMap(intent);
}

bool MapCreatorController::hasUnsavedDraft() const
{
    return _persistence.saveFailed || _editing.authoringSerial != _persistence.cleanAuthoringSerial ||
           !pendingAtlasSelectionMatchesDraft() || generationBusy() || _editing.strokeInProgress ||
           (_view && _view->hasPendingAuthoringInteraction());
}

void MapCreatorController::setAllDraftFog(bool hidden)
{
    if (generationBusy()) return;
    endBrushStroke();
    finishPropertyEditUndo();
    if (std::all_of(_draftMap.cells.begin(), _draftMap.cells.end(),
                    [hidden](const MapCellModel &cell) { return cell.fogged == hidden; })) return;
    captureUndoSnapshot(hidden ? "Hide whole map" : "Reveal whole map");
    for (auto &cell : _draftMap.cells) cell.fogged = hidden;
    ++_draftMap.revision;
    _draftMap.metadata.touch();
    markDraftEdited();
}

bool MapCreatorController::flushDraft()
{
    // Explicit application saves must include this independent map draft, but
    // never commit half a held stroke or an unfinished trace.
    if (generationBusy() || _editing.strokeInProgress || (_view && _view->hasPendingAuthoringInteraction()))
    {
        _integration.statusMessage = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                             ? "Die Karte konnte nicht gespeichert werden. Beende die laufende Kartenbearbeitung und versuche es erneut. Dein Entwurf bleibt erhalten."
                             : "The map could not be saved. Finish the current map interaction and try again. Your draft has been kept.";
        return false;
    }
    if (!hasUnsavedDraft())
        return true;
    finishPropertyEditUndo();
    return saveDraftMap();
}

void MapCreatorController::setReviewSavedMapHandler(
    std::function<bool(const EntityId &)> handler)
{
    _integration.reviewSavedMapHandler = std::move(handler);
}

void MapCreatorController::setOpenSavedMapHandler(
    std::function<bool(const EntityId &)> handler)
{
    _integration.openSavedMapHandler = std::move(handler);
}

void MapCreatorController::setOpenEncounterPreparationHandler(
    std::function<bool()> handler)
{
    _integration.openEncounterPreparationHandler = std::move(handler);
}

void MapCreatorController::setAttachSavedMapToSceneHandler(
    std::function<bool(const EntityId &)> handler)
{
    _integration.attachSavedMapToSceneHandler = std::move(handler);
}

bool MapCreatorController::finishPendingMapPackageImportRecovery(bool discardAssets)
{
    if (_transfer.pendingPackageImportTransactionId.empty())
        return true;
    try
    {
        const std::filesystem::path assetLibrary =
            ProjectAssets::campaignAssetDirectory(_currentCampaign, "MapPackages");
        MapPackageDraftRecoveryOptions options;
        options.projectRoot = ProjectAssets::findProjectRoot();
        options.assetLibraryDirectory = assetLibrary;
        std::string error;
        const bool success = discardAssets
            ? MapPackageDraftInstallService::discardRecovery(
                  options, _transfer.pendingPackageImportTransactionId, &error)
            : MapPackageDraftInstallService::acknowledgeRecovery(
                  options, _transfer.pendingPackageImportTransactionId, &error);
        if (!success)
        {
            _integration.statusMessage = std::string(gm::localization::trUi(
                discardAssets
                    ? "The interrupted map package import could not be discarded:"
                    : "The map package recovery marker could not be cleared:")) +
                " " + gm::localization::trSystemMessage(error);
            return false;
        }
        _transfer.pendingPackageImportTransactionId.clear();
        return true;
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi(
            discardAssets
                ? "The interrupted map package import could not be discarded:"
                : "The map package recovery marker could not be cleared:")) +
            " " + gm::localization::trSystemMessage(error.what());
        return false;
    }
}

void MapCreatorController::recoverInterruptedMapPackageImport()
{
    try
    {
        const std::filesystem::path assetLibrary =
            ProjectAssets::campaignAssetDirectory(_currentCampaign, "MapPackages");
        if (!std::filesystem::exists(assetLibrary))
            return;
        MapPackageDraftRecoveryOptions options;
        options.projectRoot = ProjectAssets::findProjectRoot();
        options.assetLibraryDirectory = assetLibrary;
        MapPackageDraftRecoveryResult recovered =
            MapPackageDraftInstallService::recoverLatest(options);
        if (!recovered.found)
            return;
        if (!recovered.success)
        {
            _integration.statusMessage = std::string(gm::localization::trUi(
                "An interrupted map package import needs attention:")) +
                " " + gm::localization::trSystemMessage(recovered.error);
            return;
        }

        const bool alreadySaved = std::any_of(
            _currentCampaign.maps.begin(), _currentCampaign.maps.end(),
            [&](const MapModel &map) { return map.metadata.id == recovered.map.metadata.id; });
        if (alreadySaved)
        {
            _transfer.pendingPackageImportTransactionId = recovered.transactionId;
            if (finishPendingMapPackageImportRecovery(false))
                _integration.statusMessage = gm::localization::trUi(
                    "Finished recovery cleanup for an already saved map package import.");
            return;
        }

        _draftMap = std::move(recovered.map);
        _persistence.saveReceipt = {};
        markDraftEdited();
        resizeMapCells(_draftMap);
        ensureDefaultMapLayer(_draftMap);
        _persistence.editingMapIndex = -1;
        _persistence.selectedMapIndex = -1;
        _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
        _generation.selectedCreationIntentIndex = static_cast<int>(_draftMap.creationIntent);
    restoreTravelSetupFromRecipe();
        _generation.selectedSizePresetIndex = static_cast<int>(_draftMap.sizePreset);
        _editing.showEditorGrid = _draftMap.atlasLevel == MapLevel::Tactical;
        clearObjectSelection();
        clearUndoSnapshot();
        syncReferenceCalibrationDraft();
        _transfer.pendingPackageImportTransactionId = recovered.transactionId;
        _history.pendingNamedCheckpointLabel = gm::localization::trUi("Map package imported");
        _history.crashJournalPending = true;
        _history.crashJournalLabel = "Recover Map Package Import";
        try
        {
            ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
            MapCheckpointService checkpointService(
                ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
            _history.recoveryPreview = checkpointService.previewRecovery(_draftMap);
            _history.hasRecoveryPreview = _history.recoveryPreview.valid;
        }
        catch (const std::exception &)
        {
            _history.recoveryPreview = {};
            _history.hasRecoveryPreview = false;
        }
        if (_view)
            _view->setWorkflowStep(3);
        _integration.statusMessage = _history.hasRecoveryPreview
            ? gm::localization::trUi(
                  "Recovered an interrupted map package import. A newer unsaved edit recovery is available for review.")
            : gm::localization::trUi(
                  "Recovered an interrupted map package import. Review and save it to finish the import.");
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi(
            "An interrupted map package import needs attention:")) +
            " " + gm::localization::trSystemMessage(error.what());
    }
}

bool MapCreatorController::beginSettlementFromWorldPlace(const EntityId &placeId)
{
    const auto placeIt = std::find_if(_currentCampaign.worldPlaces.begin(), _currentCampaign.worldPlaces.end(), [&](const WorldPlaceModel &place) {
        return place.metadata.id == placeId;
    });
    if (placeIt == _currentCampaign.worldPlaces.end())
        return false;

    startNewMap();
    _draftMap.name = placeIt->name.empty() ? gm::localization::trUi("Settlement map") : placeIt->name;
    _draftMap.description = placeIt->description;
    _generation.selectedCreationIntentIndex = static_cast<int>(MapCreationIntent::Settlement);
    _generation.selectedSizePresetIndex = static_cast<int>(MapSizePreset::Standard);
    applyMapCreationIntent(_draftMap, MapCreationIntent::Settlement, MapSizePreset::Standard);
    _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
    _generation.selectedGeneratorIndex = static_cast<int>(GeneratorPreset::City);
    _generation.generatorPurposeIndex = static_cast<int>(MapGenerationPurpose::SettlementEnvirons);
    _generation.generatorRequestedSettlements = 1;
    _generation.generatorRequestedLandmarks = 6;
    _generation.setupEntities = {{placeIt->metadata.id, placeIt->name, false, true, placeIt->nationId}};
    _generation.briefCityNames = {placeIt->name};
    _generation.briefNationNames.clear();
    _generation.briefCapitalName.clear();
    _generation.worldBriefParsed = true;
    _generation.worldBrief = placeIt->description;

    if (!placeIt->linkedMapId.empty())
    {
        const auto parentIt = std::find_if(_currentCampaign.maps.begin(), _currentCampaign.maps.end(), [&](const MapModel &map) {
            return map.metadata.id == placeIt->linkedMapId;
        });
        if (parentIt != _currentCampaign.maps.end())
        {
            _draftMap.parentMapId = parentIt->metadata.id;
            _draftMap.atlasPlacement.parentId = parentIt->metadata.id;
            _draftMap.atlasPlacement.parentRevision = parentIt->revision;
            const float x = placeIt->hasMapPosition ? static_cast<float>(placeIt->mapPosition.x) : 0.5f;
            const float y = placeIt->hasMapPosition ? static_cast<float>(placeIt->mapPosition.y) : 0.5f;
            _draftMap.atlasPlacement.parentRect = {
                std::clamp(x - 0.08f, 0.0f, 0.84f),
                std::clamp(y - 0.08f, 0.0f, 0.84f),
                0.16f,
                0.16f};
            _draftMap.atlasStatus = MapAtlasStatus::Linked;
        }
    }
    resizeDraftCells();
    synchronizeStructuredSetup();
    _integration.statusMessage.clear();
    _view->setWorkflowStep(1);
    _view->show();
    _view->requestDockOnNextUpdate();
    return true;
}
