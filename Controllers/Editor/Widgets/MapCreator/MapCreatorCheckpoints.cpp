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

void MapCreatorController::refreshNamedCheckpoints()
{
    _history.namedCheckpoints.clear();
    _history.selectedNamedCheckpointIndex = -1;
    if (_draftMap.metadata.id.empty())
        return;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        const MapCheckpointListResult result = service.listNamedCheckpoints(_draftMap.metadata.id);
        if (!result.success)
        {
            _integration.statusMessage = std::string(gm::localization::trUi("Could not list map checkpoints:")) + " " +
                             gm::localization::trSystemMessage(result.error);
            return;
        }
        _history.namedCheckpoints = result.checkpoints;
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Could not list map checkpoints:")) + " " +
                         gm::localization::trSystemMessage(error.what());
    }
}

void MapCreatorController::createNamedCheckpoint(const std::string &name)
{
    std::string normalized = name;
    const auto first = std::find_if_not(normalized.begin(), normalized.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(normalized.rbegin(), normalized.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    normalized = first < last ? std::string(first, last) : std::string{};
    if (normalized.empty())
    {
        _integration.statusMessage = gm::localization::trUi("Enter a name for the map checkpoint.");
        return;
    }

    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        const MapCheckpointWriteResult result = service.createNamedCheckpoint(_draftMap, normalized);
        if (!result.success)
        {
            _integration.statusMessage = std::string(gm::localization::trUi("Map checkpoint failed:")) + " " +
                             gm::localization::trSystemMessage(result.error);
            return;
        }
        _history.newCheckpointName.clear();
        refreshNamedCheckpoints();
        _integration.statusMessage = gm::localization::trUi("Map checkpoint created.");
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map checkpoint failed:")) + " " +
                         gm::localization::trSystemMessage(error.what());
    }
}

void MapCreatorController::previewNamedCheckpointRestore(int checkpointIndex)
{
    _history.namedCheckpointRestorePreview = {};
    _history.hasNamedCheckpointRestorePreview = false;
    if (checkpointIndex < 0 || checkpointIndex >= static_cast<int>(_history.namedCheckpoints.size()))
        return;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        _history.namedCheckpointRestorePreview = service.previewRestore(
            _draftMap, _history.namedCheckpoints[static_cast<std::size_t>(checkpointIndex)].id);
        _history.hasNamedCheckpointRestorePreview = _history.namedCheckpointRestorePreview.valid;
        _history.selectedNamedCheckpointIndex = checkpointIndex;
        if (!_history.hasNamedCheckpointRestorePreview)
            _integration.statusMessage = std::string(gm::localization::trUi("Could not preview map checkpoint:")) + " " +
                             gm::localization::trSystemMessage(_history.namedCheckpointRestorePreview.error);
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Could not preview map checkpoint:")) + " " +
                         gm::localization::trSystemMessage(error.what());
    }
}

void MapCreatorController::restoreNamedCheckpoint()
{
    if (!_history.hasNamedCheckpointRestorePreview || !_history.namedCheckpointRestorePreview.valid)
        return;
    try
    {
        ProjectPackageLayout::ensureCampaignPackageLayout(_currentCampaign);
        MapCheckpointService service(ProjectPackageLayout::layoutForCampaign(_currentCampaign).rootDirectory);
        MapModel restored = _draftMap;
        std::string error;
        if (!service.applyRestore(_history.namedCheckpointRestorePreview, restored, true, &error))
        {
            _integration.statusMessage = std::string(gm::localization::trUi("Map checkpoint restore failed:")) + " " +
                             gm::localization::trSystemMessage(error);
            return;
        }
        const std::uint64_t nextRevision = _draftMap.revision + 1;
        captureUndoSnapshot("Restore Named Map Checkpoint");
        _draftMap = std::move(restored);
        _draftMap.revision = nextRevision;
        _draftMap.metadata.touch();
        resizeMapCells(_draftMap);
        ensureDefaultMapLayer(_draftMap);
        normalizeObjectSelection();
        syncReferenceCalibrationDraft();
        discardNamedCheckpointRestorePreview();
        refreshNamedCheckpoints();
        _integration.statusMessage = gm::localization::trUi("Map checkpoint restored. You can undo this change.");
    }
    catch (const std::exception &error)
    {
        _integration.statusMessage = std::string(gm::localization::trUi("Map checkpoint restore failed:")) + " " +
                         gm::localization::trSystemMessage(error.what());
    }
}

void MapCreatorController::discardNamedCheckpointRestorePreview()
{
    _history.namedCheckpointRestorePreview = {};
    _history.hasNamedCheckpointRestorePreview = false;
    _history.selectedNamedCheckpointIndex = -1;
}

bool MapCreatorController::generationCandidateEligible(const MapGenerationResult &candidate) const
{
    const MapGenerationQualityReport &quality = candidate.quality;
    if (!candidate.validation.eligible || candidate.validation.criticalIssueCount != 0 ||
        !candidate.validation.blockers.empty() || quality.criticalIssueCount != 0 || quality.score < 85)
        return false;

    const auto containsFeature = [&](MapGeneratedFeatureKind kind) {
        return std::any_of(candidate.map.generation.graph.features.begin(), candidate.map.generation.graph.features.end(),
                           [&](const MapGeneratedFeature &feature) { return feature.kind == kind; });
    };
    const bool hasRoads = containsFeature(MapGeneratedFeatureKind::Road) || containsFeature(MapGeneratedFeatureKind::Trail);
    const bool hasRivers = containsFeature(MapGeneratedFeatureKind::River);
    const bool hasSettlements = containsFeature(MapGeneratedFeatureKind::Settlement);

    if (quality.compositionPercent < 80 || quality.visualHierarchyPercent < 80 ||
        quality.scaleCorrectnessPercent < 80 || quality.assetCoveragePercent < 80)
        return false;
    if (candidate.map.atlasLevel != MapLevel::World && quality.traversableConnectivityPercent < 80)
        return false;
    if (hasRoads && (quality.wayConnectivityPercent < 80 || quality.roadEfficiencyPercent < 80))
        return false;
    if (hasRivers && (quality.hydrologyPercent < 80 || quality.bridgeValidityPercent < 80))
        return false;
    if (hasSettlements && (quality.structureAccessPercent < 80 || quality.settlementSuitabilityPercent < 80))
        return false;
    return true;
}
