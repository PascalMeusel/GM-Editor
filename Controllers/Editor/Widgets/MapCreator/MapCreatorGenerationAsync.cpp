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

void MapCreatorController::queueDraftGeneration()
{
    if (generationBusy())
        return;

    // A normal design search supersedes any unapplied partial preview.
    _generation.partialRequested = false;
    _generation.partialSourceMapId.clear();
    _generation.partialSourceRevision = 0;
    _generation.partialSelection = {};
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    syncScopeFromIndex();
    _generation.options = buildGenerationOptions();
    _generation.presetIndex = std::clamp(_generation.selectedGeneratorIndex, 0, 4);
    _generation.candidates.clear();
    _generation.selectedGenerationCandidate = -1;
    _generation.recommendedGenerationCandidate = -1;
    _generation.rejectedGenerationCandidateCount = 0;
    _generation.hasLastQuality = false;
    _generation.lastValidation = {};
    _generation.lastBatchFailure = MapGenerationBatchFailureCode::None;
    _generation.queuedSerial = ++_generation.requestSerial;
    _generation.queued = true;
    _integration.statusMessage = generationDetailLabel();
}

void MapCreatorController::dispatchQueuedGeneration()
{
    // The new campaign can queue/cancel normally while the previous immutable
    // job retires. Never overwrite its future: that could join on this thread.
    if (!_generation.queued || _generation.inProgress || _generation.retired)
        return;
    _generation.queued = false;
    generateDraftMap();
}

void MapCreatorController::invalidateGenerationRequest()
{
    if (_generation.cancellation)
        _generation.cancellation->store(true, std::memory_order_relaxed);
    ++_generation.requestSerial;
    _generation.queuedSerial = 0;
    _generation.queued = false;
    _generation.candidates.clear();
    _generation.selectedGenerationCandidate = -1;
    _generation.recommendedGenerationCandidate = -1;
    _generation.rejectedGenerationCandidateCount = 0;
    _generation.partialRequested = false;
    _generation.partialSourceMapId.clear();
    _generation.partialSourceRevision = 0;
    _generation.partialSelection = {};
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    if (!_generation.inProgress)
    {
        _generation.activeSerial = 0;
        _generation.previousDraft = {};
        _generation.progress.reset();
    }
}

void MapCreatorController::cancelDraftGeneration()
{
    if (!generationBusy())
        return;
    const bool workerStillRunning = _generation.inProgress;
    invalidateGenerationRequest();
    _integration.statusMessage = workerStillRunning
                         ? gm::localization::trUi("Cancelling map generation. Its result will be discarded.")
                         : gm::localization::trUi("Cancelled map generation.");
}

bool MapCreatorController::generationBusy() const
{
    return _generation.queued || _generation.inProgress;
}

std::string MapCreatorController::generationProgressLabel() const
{
    if (_generation.queued && !_generation.inProgress)
        return std::string(gm::localization::trUi("Queued")) + "...";
    if (!_generation.inProgress)
        return {};

    const MapGenerationProgress progress = currentGenerationProgress();
    std::ostringstream label;
    if (!progress.stage.empty() && progress.totalSteps > 0)
        label << localizedGenerationStage(progress.stage) << " " << progress.step << "/" << progress.totalSteps << "... ";
    else
        label << gm::localization::trUi("Creating map designs") << "... ";
    label << elapsedGenerationLabel(_generation.startedAt);
    return label.str();
}

std::string MapCreatorController::generationDetailLabel() const
{
    if (_generation.options.generatorVersion >= 2)
    {
        if (_generation.queued && !_generation.inProgress)
            return gm::localization::trUi("The design search is queued.");
        if (!_generation.inProgress)
            return {};
        const MapGenerationProgress progress = currentGenerationProgress();
        std::ostringstream detail;
        if (!progress.stage.empty())
            detail << localizedGenerationStage(progress.stage) << " · " << progress.step << "/" << progress.totalSteps;
        else
            detail << gm::localization::trUi("Creating and checking map designs.");
        return detail.str();
    }
    if (_generation.queued && !_generation.inProgress)
    {
        std::ostringstream detail;
        detail << gm::localization::trUi("Queued") << " " << gm::localization::trUi(generatorPresetLabel(_generation.presetIndex)) << " "
               << _generation.options.width << "x" << _generation.options.height << " "
               << gm::localization::trUi(mapScopeLabel(_generation.options.scope))
               << ", " << gm::localization::trUi("Seed") << " " << _generation.options.seed << ", "
               << gm::localization::trUi(qualitySearchLabel(_generation.generatorQualityIndex)) << ".";
        return detail.str();
    }
    if (!_generation.inProgress)
        return {};

    const MapGenerationProgress progress = currentGenerationProgress();
    std::ostringstream detail;
    if (!progress.stage.empty() && progress.totalSteps > 0)
        detail << gm::localization::trUi("Stage") << " " << progress.step << "/" << progress.totalSteps << ": " << localizedGenerationStage(progress.stage) << ". ";
    detail << gm::localization::trUi(generatorPresetLabel(_generation.presetIndex)) << " "
           << _generation.options.width << "x" << _generation.options.height << " "
           << gm::localization::trUi(mapScopeLabel(_generation.options.scope))
           << ", " << gm::localization::trUi("Seed") << " " << _generation.options.seed
           << ", " << _generation.options.qualityAttempts << " " << gm::localization::trUi("quality candidates")
           << ", " << gm::localization::trUi("water") << " " << _generation.options.waterLevel
           << ", " << gm::localization::trUi("mountains") << " " << _generation.options.mountainDensity << " (" << gm::localization::trUi(mountainPatternLabel(_generation.options.mountainPattern)) << ")"
           << ", " << gm::localization::trUi("forests") << " " << _generation.options.forestDensity
           << ", " << gm::localization::trUi("settlements") << " " << _generation.options.settlementDensity
           << ", " << gm::localization::trUi("routes") << " " << _generation.options.routeDensity << " (" << gm::localization::trUi(roadNetworkLabel(_generation.options.roadNetwork)) << ")";
    if (_generation.options.tacticalDensity > 0)
        detail << ", " << gm::localization::trUi("tactics") << " " << _generation.options.tacticalDensity;
    if (_generation.options.caveNetwork)
        detail << ", " << gm::localization::trUi("cave network");
    detail << ".";
    return detail.str();
}

float MapCreatorController::generationProgressFraction() const
{
    const MapGenerationProgress progress = currentGenerationProgress();
    if (progress.totalSteps <= 0)
        return -1.0f;
    return std::clamp(static_cast<float>(progress.step) / static_cast<float>(progress.totalSteps), 0.0f, 1.0f);
}

MapGenerationProgress MapCreatorController::currentGenerationProgress() const
{
    const std::shared_ptr<GenerationProgressSnapshot> progress = _generation.progress;
    if (!progress)
        return {};

    std::lock_guard<std::mutex> lock(progress->mutex);
    return MapGenerationProgress{progress->step, progress->totalSteps, progress->stage};
}

void MapCreatorController::restoreTravelSetupFromRecipe()
{
    if (_draftMap.creationIntent != MapCreationIntent::TravelScene) return;
    const auto &stored = _draftMap.generation.recipe;
    _generation.travelPurposeIndex = stored.travelScenePurpose < 0 ? 1 : std::clamp(stored.travelScenePurpose, 0, 4);
    _generation.travelBiomeIndex = standaloneTravelBiomeIndex(stored.travelSceneBiome);
    _generation.travelDangerLevel = std::clamp(stored.travelSceneDangerLevel, 0, 5);
    _generation.travelCombatReady = stored.travelSceneCombatReady;
    if (stored.travelScenePurpose >= 0) _generation.generatorSeed = stored.travelSceneSeed;
}
