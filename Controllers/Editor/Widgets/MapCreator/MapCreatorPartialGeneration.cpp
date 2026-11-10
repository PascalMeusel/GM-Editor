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

void MapCreatorController::queuePartialRegeneration(const MapGenerationSelection &selection)
{
    if (generationBusy())
        return;
    if (selection.featureKinds.empty())
    {
        _integration.statusMessage = gm::localization::trUi("Choose a feature family before creating a change preview.");
        return;
    }
    if (selection.useArea &&
        (selection.normalizedArea.width <= 0.0f || selection.normalizedArea.height <= 0.0f))
    {
        _integration.statusMessage = gm::localization::trUi("Select a map area before creating a change preview.");
        return;
    }

    queueDraftGeneration();
    if (!_generation.queued)
        return;
    _generation.partialRequested = true;
    _generation.partialSourceMapId = _draftMap.metadata.id;
    _generation.partialSourceRevision = _draftMap.revision;
    _generation.partialSelection = selection;
    _integration.statusMessage = gm::localization::trUi("Creating a non-destructive change preview in the background.");
}

void MapCreatorController::discardPartialRegeneration()
{
    if (generationBusy())
    {
        cancelDraftGeneration();
        return;
    }
    _generation.partialRequested = false;
    _generation.partialSourceMapId.clear();
    _generation.partialSourceRevision = 0;
    _generation.partialSelection = {};
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    _integration.statusMessage = gm::localization::trUi("Discarded the unapplied change preview.");
}

void MapCreatorController::applyPartialRegeneration()
{
    if (!_generation.hasPendingPatch || !_generation.pendingPatch.valid)
        return;
    if (_draftMap.metadata.id != _generation.pendingPatch.before.metadata.id ||
        _draftMap.revision != _generation.pendingPatch.before.revision)
    {
        _generation.pendingPatch = {};
        _generation.hasPendingPatch = false;
        _integration.statusMessage = gm::localization::trUi("The map changed after this preview was created. Create a new change preview.");
        return;
    }

    // One confirmed patch is one editor transaction, regardless of how many
    // graph features or projected cells it contains.
    captureUndoSnapshot("Apply Partial Regeneration");
    _draftMap = _generation.pendingPatch.after;
    resizeMapCells(_draftMap);
    ensureDefaultMapLayer(_draftMap);
    normalizeObjectSelection();
    syncReferenceCalibrationDraft();
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
    _generation.partialSourceMapId.clear();
    _generation.partialSourceRevision = 0;
    _generation.partialSelection = {};
    _integration.statusMessage = gm::localization::trUi("The selected map part was regenerated. Other content was preserved.");
    _history.pendingNamedCheckpointLabel = gm::localization::trUi("Partial regeneration applied");
}

void MapCreatorController::setGeneratedFeatureLocked(const EntityId &featureId, bool locked)
{
    auto feature = std::find_if(_draftMap.generation.graph.features.begin(),
                                _draftMap.generation.graph.features.end(),
                                [&](const MapGeneratedFeature &candidate) { return candidate.id == featureId; });
    if (feature == _draftMap.generation.graph.features.end())
        return;
    const auto &recipeLocks = _draftMap.generation.recipe.lockedFeatureIds;
    const bool recipeLocked = std::find(recipeLocks.begin(), recipeLocks.end(), featureId) != recipeLocks.end();
    if (feature->locked == locked && recipeLocked == locked)
        return;

    captureUndoSnapshot("Change Generated Feature Lock");
    feature->locked = locked;
    auto &lockedIds = _draftMap.generation.recipe.lockedFeatureIds;
    const auto lockedId = std::find(lockedIds.begin(), lockedIds.end(), featureId);
    if (locked && lockedId == lockedIds.end())
        lockedIds.push_back(featureId);
    else if (!locked && lockedId != lockedIds.end())
        lockedIds.erase(lockedId);
    ++_draftMap.revision;
    _draftMap.metadata.touch();
    _generation.pendingPatch = {};
    _generation.hasPendingPatch = false;
}

bool MapCreatorController::promoteGeneratedPlace(const EntityId &featureId)
{
    if (generationBusy() || featureId.empty())
        return false;

    const WorldInspector::GeneratedPlacePromotionEligibility eligibility =
        WorldInspector::inspectGeneratedPlacePromotion(
            _currentCampaign, _draftMap, featureId);
    if (!eligibility.eligible())
    {
        _integration.statusMessage = gm::localization::trSystemMessage(eligibility.message);
        return false;
    }

    const WorldInspector::MutationResult promoted = WorldInspector::applyWithRollback(
        _currentCampaign,
        [&](Campaign &campaign) {
            return WorldInspector::promoteGeneratedPlace(
                campaign, _draftMap, featureId);
        },
        [&](Campaign &campaign) {
            return ProjectSaveService::getInstance().saveCampaign(
                campaign, "add generated place to campaign world");
        });
    if (!promoted.applied())
    {
        _integration.statusMessage = std::string(gm::localization::trUi(
                             "Could not add the generated place to the campaign world:")) +
                         " " + gm::localization::trSystemMessage(promoted.message);
        return false;
    }

    const WorldPlaceModel *place =
        WorldInspector::findWorldPlace(_currentCampaign, promoted.entityId);
    if (!place)
    {
        _integration.statusMessage = gm::localization::trUi(
            "The campaign place was saved but could not be found in the current campaign.");
        return false;
    }

    finishPropertyEditUndo();
    UndoSnapshot undoSnapshot{_draftMap, "Add Generated Place to Campaign", _persistence.playerSettingsBaseline, {}, _history.draftHistory.current()};
    undoSnapshot.worldPlaceTransition = UndoSnapshot::WorldPlaceTransition{*place, false};
    _history.undo.push_back(std::move(undoSnapshot));
    while (_history.undo.size() > map_creator_controller::HistoryState::maximumUndoEntries)
        _history.undo.erase(_history.undo.begin());
    _history.redo.clear();
    refreshUndoState();
    _integration.statusMessage = gm::localization::trUi(
        "The generated place was added to the campaign world and saved.");
    return true;
}

bool MapCreatorController::applyWorldPlaceHistoryTransition(
    const UndoSnapshot::WorldPlaceTransition &transition,
    const char *saveReason,
    const char *failurePrefix)
{
    const WorldInspector::MutationResult changed = WorldInspector::applyWithRollback(
        _currentCampaign,
        [&](Campaign &campaign) {
            return transition.shouldExist
                       ? WorldInspector::restoreGeneratedPlacePromotion(
                             campaign, transition.place)
                       : WorldInspector::removeGeneratedPlacePromotion(
                             campaign, transition.place);
        },
        [&](Campaign &campaign) {
            return ProjectSaveService::getInstance().saveCampaign(
                campaign, saveReason ? saveReason : "generated place transfer");
        });
    if (changed.applied())
        return true;

    _integration.statusMessage = std::string(gm::localization::trUi(failurePrefix)) + " " +
                     gm::localization::trSystemMessage(changed.message);
    return false;
}
