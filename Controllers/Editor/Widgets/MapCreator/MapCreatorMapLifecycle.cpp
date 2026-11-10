#include "Models/Project/ContextLinkMetadata.hpp"
#include "Models/Map/MapTravelCreation.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "MapCreatorController.hpp"
#include <algorithm>
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

void MapCreatorController::startNewMap(MapCreationIntent intent)
{
    if (!finishPendingMapPackageImportRecovery(true))
        return;
    invalidateGenerationRequest();
    _history.pendingNamedCheckpointLabel.clear();
    _history.namedCheckpoints.clear();
    _history.newCheckpointName.clear();
    _history.selectedNamedCheckpointIndex = -1;
    _history.namedCheckpointRestorePreview = {};
    _history.hasNamedCheckpointRestorePreview = false;
    _generation.partialRequested = false;
    _generation.partialSourceMapId.clear();
    _generation.partialSourceRevision = 0;
    _generation.partialSelection = {};
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    _history.hasRecoveryPreview = false;
    _history.recoveryPreview = {};
    _generation.hasLastQuality = false;
    _generation.lastValidation = {};
    _generation.lastBatchFailure = MapGenerationBatchFailureCode::None;
    _persistence.saveReceipt = {};
    _persistence.playerSettingsBaseline = {};
    _persistence.loadedBaseline = {};
    _persistence.saveFailed = false;
    markDraftEdited();
    _draftMap = MapModel{};
    _integration.statusMessage.clear();
    _generation.generatorSeed = std::random_device{}();
    _draftMap.name = gm::localization::trUi("New Map");
    _draftMap.generation.recipe.landform = MapLandform::Continental;
    _generation.travelPurposeIndex = 1; _generation.travelBiomeIndex = 0; _generation.travelDangerLevel = 1; _generation.travelCombatReady = false;
    _generation.structuredCreation = true;
    _generation.setupEntities.clear();
    _generation.selectedCreationIntentIndex = static_cast<int>(intent);
    _generation.selectedSizePresetIndex = static_cast<int>(MapSizePreset::Standard);
    applyMapCreationIntent(_draftMap, intent, MapSizePreset::Standard);
    applyMapCreatorArtStyle(_draftMap.generation.recipe, MapCreatorArtStyle::Illustrated);
    _persistence.selectedScopeIndex = static_cast<int>(_draftMap.scope);
    _draftMap.isMainMap = _currentCampaign.maps.empty() ||
        (intent == MapCreationIntent::WorldOverview &&
         std::none_of(_currentCampaign.maps.begin(), _currentCampaign.maps.end(),
                      [](const MapModel &map) { return map.isMainMap; }));
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    _persistence.editingMapIndex = -1;
    _persistence.selectedMapIndex = -1;
    _editing.selectedParentIndex = 0;
    if (!_draftMap.isMainMap && _draftMap.atlasLevel != MapLevel::World)
    {
        int onlyParent = 0;
        for (int index = 0; index < static_cast<int>(_currentCampaign.maps.size()); ++index)
        {
            if (!MapAtlasService::isNextLevel(_draftMap.atlasLevel,
                                              _currentCampaign.maps[static_cast<std::size_t>(index)].atlasLevel))
                continue;
            if (onlyParent != 0)
            {
                onlyParent = 0;
                break;
            }
            onlyParent = index + 1;
        }
        _editing.selectedParentIndex = onlyParent;
    }
    _editing.selectedLayerIndex = 0;
    _editing.selectedTokenIndex = -1;
    _editing.selectedRegionIndex = -1;
    _editing.selectedFeatureIndex = -1;
    _editing.selectedGeneratedFeatureIndex = -1;
    _editing.selectedRouteIndex = -1;
    _editing.showEditorGrid = _draftMap.atlasLevel == MapLevel::Tactical;
    _transfer.exportCurrentViewport = false;
    _transfer.exportViewportValid = false;
    _transfer.exportViewportRect = {};
    setActiveTool(Tool::PaintTile);
    _generation.worldBrief.clear();
    _generation.briefNationNames.clear();
    _generation.briefCityNames.clear();
    _generation.briefFacts.clear();
    _generation.briefCapitalName.clear();
    _generation.worldBriefParsed = false;
    _generation.candidates.clear();
    _generation.selectedGenerationCandidate = -1;
    _generation.recommendedGenerationCandidate = -1;
    _generation.rejectedGenerationCandidateCount = 0;
    clearUndoSnapshot();
    syncReferenceCalibrationDraft();
    if (_view)
        _view->setWorkflowStep(0);
    _persistence.cleanAuthoringSerial = _editing.authoringSerial;
    _history.draftHistory.saved();
}

void MapCreatorController::updateDraftArtwork(const std::string &undoLabel)
{
    if (generationBusy()) return;
    endBrushStroke();
    finishPropertyEditUndo();
    captureUndoSnapshot(undoLabel);
    applyMapCreatorArtStyle(_draftMap.generation.recipe, MapCreatorArtStyle::Illustrated);
    _draftMap.generation.recipe.recipeVersion = kCurrentMapIllustrationRecipe;
    ++_draftMap.revision;
    _draftMap.metadata.touch();
    invalidateGenerationRequest();
    markDraftEdited();
}

void MapCreatorController::loadSelectedMap()
{
    if (_persistence.selectedMapIndex < 0 || _persistence.selectedMapIndex >= static_cast<int>(_currentCampaign.maps.size()))
        return;
    if (!finishPendingMapPackageImportRecovery(true))
        return;

    invalidateGenerationRequest();
    _generation.hasLastQuality = false;
    _generation.lastValidation = {};
    _generation.lastBatchFailure = MapGenerationBatchFailureCode::None;
    _history.pendingNamedCheckpointLabel.clear();
    _persistence.saveReceipt = {};
    _persistence.loadedBaseline = {};
    _persistence.saveFailed = false;
    markDraftEdited();
    _draftMap = _currentCampaign.maps[_persistence.selectedMapIndex];
    repairCollapsedMapSetupFeatureReferences(_currentCampaign, _draftMap);
    if (_draftMap.generation.enabled) _generation.generatorSeed = _draftMap.generation.recipe.seed;
    _integration.statusMessage.clear();
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    _persistence.editingMapIndex = _persistence.selectedMapIndex;
    MapCreationIntent creationIntent = _draftMap.creationIntent;
    if (mapLevelForCreationIntent(creationIntent) != _draftMap.atlasLevel)
        creationIntent = inferredCreationIntent(_draftMap);
    _draftMap.creationIntent = creationIntent;
    _draftMap.generation.recipe.creationIntent = creationIntent;
    _generation.selectedCreationIntentIndex = static_cast<int>(creationIntent);
    const MapSizePreset sizePreset = inferredSizePreset(creationIntent, _draftMap.width, _draftMap.height);
    _draftMap.sizePreset = sizePreset;
    _draftMap.generation.recipe.sizePreset = sizePreset;
    _generation.selectedSizePresetIndex = static_cast<int>(sizePreset);
    syncCreationIntentFromIndex(false);
    _editing.showEditorGrid = _draftMap.atlasLevel == MapLevel::Tactical;
    _transfer.exportCurrentViewport = false;
    _transfer.exportViewportValid = false;
    _transfer.exportViewportRect = {};
    _transfer.exportGmSettings.showGrid = _editing.showEditorGrid;
    _transfer.exportPlayerSettings.showGrid = _editing.showEditorGrid;
    _editing.selectedParentIndex = 0;
    for (int i = 0; i < static_cast<int>(_currentCampaign.maps.size()); ++i)
    {
        if (_currentCampaign.maps[i].metadata.id == MapAtlasService::effectiveParentId(_draftMap))
        {
            _editing.selectedParentIndex = i + 1;
            break;
        }
    }
    clearObjectSelection();
    _generation.worldBrief = _draftMap.creationIntent == MapCreationIntent::WorldOverview
        ? _draftMap.description : std::string{};
    _generation.briefNationNames.clear();
    _generation.briefCityNames.clear();
    _generation.briefFacts = _draftMap.generation.recipe.facts;
    _generation.setupEntities = mapSetupEntities(_generation.briefFacts);
    const auto &storedControls = _draftMap.generation.recipe;
    restoreTravelSetupFromRecipe();
    if (storedControls.waterLevel >= 0) _generation.generatorWaterLevel = storedControls.waterLevel;
    if (storedControls.forestDensity >= 0) _generation.generatorForestDensity = storedControls.forestDensity;
    if (storedControls.mountainDensity >= 0) _generation.generatorMountainDensity = storedControls.mountainDensity;
    if (storedControls.settlementDensity >= 0) _generation.generatorSettlementDensity = storedControls.settlementDensity;
    if (storedControls.routeDensity >= 0) _generation.generatorRouteDensity = storedControls.routeDensity;
    if (storedControls.tacticalDensity >= 0) _generation.generatorTacticalDensity = storedControls.tacticalDensity;
    _generation.generatorLandscapeIndex = static_cast<int>(storedControls.landscape);
    _generation.generatorPurposeIndex = static_cast<int>(storedControls.purpose);
    _generation.generatorRequestedSettlements = storedControls.settlementCount;
    _generation.generatorRequestedLandmarks = storedControls.landmarkCount;
    _generation.briefCapitalName.clear();
    _generation.worldBriefParsed = false;
    clearUndoSnapshot();
    syncReferenceCalibrationDraft();
    _history.hasRecoveryPreview = false;
    _history.recoveryPreview = {};
    _generation.partialRequested = false;
    _generation.partialSourceMapId.clear();
    _generation.partialSourceRevision = 0;
    _generation.partialSelection = {};
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    _history.namedCheckpointRestorePreview = {};
    _history.hasNamedCheckpointRestorePreview = false;
    refreshNamedCheckpoints();
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        _history.recoveryPreview = service.previewRecovery(_draftMap);
        _history.hasRecoveryPreview = _history.recoveryPreview.valid;
        if (_history.hasRecoveryPreview)
            _integration.statusMessage = gm::localization::trUi("Unsaved map recovery is available. Review it before editing.");
    }
    catch (const std::exception &)
    {
        _history.hasRecoveryPreview = false;
        _history.recoveryPreview = {};
    }
    _persistence.cleanAuthoringSerial = _editing.authoringSerial;
    _history.draftHistory.saved();
    _persistence.playerSettingsBaseline = captureMapCreatorPlayerSettings(_draftMap);
    _persistence.loadedBaseline = {_draftMap.metadata.id, _draftMap.revision, _editing.authoringSerial};
}
