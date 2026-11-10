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

void MapCreatorController::generateDraftMap()
{
    syncScopeFromIndex();
    resizeMapCells(_draftMap);
    _generation.previousDraft = _draftMap;
    _generation.options = buildGenerationOptions();
    _generation.presetIndex = std::clamp(_generation.selectedGeneratorIndex, 0, 4);
    _generation.progress = std::make_shared<GenerationProgressSnapshot>();
    _generation.cancellation = std::make_shared<std::atomic_bool>(false);
    {
        std::lock_guard<std::mutex> lock(_generation.progress->mutex);
        _generation.progress->stage = "Starting generator";
    }
    _generation.startedAt = std::chrono::steady_clock::now();
    _generation.inProgress = true;
    _generation.activeSerial = _generation.queuedSerial == 0 ? _generation.requestSerial : _generation.queuedSerial;
    _generation.queuedSerial = 0;
    _integration.statusMessage = generationDetailLabel() + " " + gm::localization::trUi("Running in the background.");
    MapGenerationOptions options = _generation.options;
    const std::shared_ptr<GenerationProgressSnapshot> progress = _generation.progress;
    const std::shared_ptr<std::atomic_bool> cancellation = _generation.cancellation;
    options.cancellationRequested = [cancellation]() {
        return cancellation && cancellation->load(std::memory_order_relaxed);
    };
    options.progressCallback = [progress](const MapGenerationProgress &progressUpdate) {
        if (!progress)
            return;
        std::lock_guard<std::mutex> lock(progress->mutex);
        progress->step = progressUpdate.step;
        progress->totalSteps = progressUpdate.totalSteps;
        progress->stage = progressUpdate.stage;
    };
    const TravelSceneRecipe travelRecipe = buildStandaloneTravelRecipe(options,
        _generation.travelPurposeIndex, _generation.travelBiomeIndex, _generation.travelDangerLevel, _generation.travelCombatReady);
    const std::uint64_t requestSerial = _generation.activeSerial;
    std::optional<MapModel> tacticalSource;
    if (options.creationIntent == MapCreationIntent::TacticalEncounter &&
        _editing.selectedParentIndex > 0 &&
        _editing.selectedParentIndex - 1 < static_cast<int>(_currentCampaign.maps.size()))
    {
        const MapModel &parent = _currentCampaign.maps[static_cast<std::size_t>(_editing.selectedParentIndex - 1)];
        if (parent.atlasLevel == MapLevel::Region || parent.atlasLevel == MapLevel::Site)
            tacticalSource = parent;
    }
    _generation.future = std::async(std::launch::async, [options, progress, requestSerial, travelRecipe,
                                                        tacticalSource = std::move(tacticalSource)]() {
        DraftGenerationResult result;
        result.requestSerial = requestSerial;
        result.batch = options.creationIntent == MapCreationIntent::TravelScene
                           ? generateStandaloneTravelMapBatch(options, travelRecipe, 3)
                           : options.creationIntent == MapCreationIntent::TacticalEncounter
                           ? TacticalMapGenerator::generateBatch(
                                 options,
                                 tacticalSource ? &*tacticalSource : nullptr,
                                 3)
                           : generateSemanticMapCandidateBatch(
                                 options,
                                 options.scope == MapScope::Dungeon ? 1 : 3);
        if (progress)
        {
            std::lock_guard<std::mutex> lock(progress->mutex);
            const int previousTotal = std::max(1, progress->totalSteps);
            progress->step = previousTotal;
            progress->totalSteps = previousTotal + 1;
            progress->stage = "Checking contextual art coverage";
        }
        const auto coverageStartedAt = std::chrono::steady_clock::now();
        result.contextualTargetCount = mapStyleContextualTargetAssetCount(options.styleId);
        result.contextualAcceptedCount = mapStyleContextualAcceptedAssetCount(options.styleId);
        result.contextualCandidateCount = mapStyleContextualCandidateAssetCount(options.styleId);
        result.contextualCoverageElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - coverageStartedAt).count();
        if (progress)
        {
            std::lock_guard<std::mutex> lock(progress->mutex);
            progress->step = progress->totalSteps;
            progress->stage = "Complete";
        }
        return result;
    });
}
