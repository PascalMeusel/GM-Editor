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

void MapCreatorController::finishDraftGeneration(const DraftGenerationResult &result)
{
    const std::vector<MapGenerationResult> &evaluated = result.batch.candidates;
    _generation.candidates.clear();
    _generation.recommendedGenerationCandidate = -1;
    _generation.rejectedGenerationCandidateCount = static_cast<int>(result.batch.rejectedCandidates.size());
    _generation.lastBatchFailure = result.batch.failure;
    _generation.lastValidation = result.batch.requestValidation;
    std::vector<MapGenerationResult> eligible;
    for (const MapGenerationResult &candidate : evaluated)
    {
        if (generationCandidateEligible(candidate))
            eligible.push_back(candidate);
        else
            ++_generation.rejectedGenerationCandidateCount;
    }
    // The UI labels only the first eligible design as recommended. Keep that
    // promise local to the presentation boundary instead of relying on every
    // generator implementation to return candidates in the same order.
    std::stable_sort(eligible.begin(), eligible.end(), [](const MapGenerationResult &left,
                                                          const MapGenerationResult &right) {
        return left.quality.score > right.quality.score;
    });

    if (_generation.partialRequested)
    {
        const bool sourceStillCurrent = _draftMap.metadata.id == _generation.partialSourceMapId &&
                                        _draftMap.revision == _generation.partialSourceRevision &&
                                        _generation.previousDraft.metadata.id == _generation.partialSourceMapId &&
                                        _generation.previousDraft.revision == _generation.partialSourceRevision;
        if (!sourceStillCurrent)
        {
            _generation.pendingPatch = {};
            _generation.hasPendingPatch = false;
            _integration.statusMessage = gm::localization::trUi("The map changed while regeneration was running. Create a new change preview.");
        }
        else if (eligible.empty())
        {
            _generation.pendingPatch = {};
            _generation.hasPendingPatch = false;
            _integration.statusMessage = gm::localization::trUi("No valid partial regeneration could satisfy every required fact.");
        }
        else
        {
            _generation.pendingPatch = buildMapGenerationPatch(
                _generation.previousDraft, eligible.front().map, _generation.partialSelection);
            if (_generation.pendingPatch.valid)
            {
                const MapGenerationQualityReport patchedQuality =
                    analyzeMapGenerationQuality(_generation.pendingPatch.after);
                const MapGenerationValidationReport patchedValidation =
                    validateMapGeneration(_generation.pendingPatch.after, patchedQuality);
                if (!patchedValidation.eligible)
                {
                    _generation.pendingPatch.valid = false;
                    _generation.pendingPatch.errorCode = "map.regeneration.validation_failed";
                }
                else
                {
                    _generation.lastQuality = patchedQuality;
                    _generation.hasLastQuality = true;
                }
            }
            _generation.hasPendingPatch = _generation.pendingPatch.valid;
            const char *patchError = _generation.pendingPatch.errorCode == "map.regeneration.dimension_mismatch"
                                         ? gm::localization::trUi("The replacement map has different dimensions.")
                                     : _generation.pendingPatch.errorCode == "map.regeneration.invalid_selection"
                                         ? gm::localization::trUi("The selected map area is invalid.")
                                     : _generation.pendingPatch.errorCode == "map.regeneration.validation_failed"
                                         ? gm::localization::trUi("The combined map would violate a required fact or map rule.")
                                         : gm::localization::trUi("The generated replacement could not be combined with this map.");
            _integration.statusMessage = _generation.hasPendingPatch
                                 ? gm::localization::trUi("Change preview is ready. Review it before applying.")
                                 : std::string(gm::localization::trUi("Could not create the change preview:")) + " " +
                                       patchError;
        }
        _generation.partialRequested = false;
        _generation.selectedGenerationCandidate = -1;
        _generation.recommendedGenerationCandidate = -1;
    }
    else
    {
        _generation.candidates = std::move(eligible);
        _generation.selectedGenerationCandidate = _generation.candidates.empty() ? -1 : 0;
        _generation.recommendedGenerationCandidate = _generation.candidates.empty() ? -1 : 0;

        _generation.lastQuality = _generation.candidates.empty()
                                     ? (result.batch.rejectedCandidates.empty()
                                            ? MapGenerationQualityReport{}
                                            : result.batch.rejectedCandidates.front().quality)
                                     : _generation.candidates.front().quality;
        if (_generation.candidates.empty() && _generation.lastValidation.blockers.empty() &&
            !result.batch.rejectedCandidates.empty())
            _generation.lastValidation = result.batch.rejectedCandidates.front().validation;
        _generation.hasLastQuality = true;
        if (_generation.candidates.empty())
            _integration.statusMessage = gm::localization::trUi("No valid design could satisfy every required fact.");
        else if (_generation.candidates.size() > 1)
            _integration.statusMessage = gm::localization::trUi("Valid ranked designs are ready. Choose one to apply it.");
        else
            _integration.statusMessage = gm::localization::trUi("One valid design is ready. You can use it or try for more alternatives.");
    }

    _generation.inProgress = false;
    _generation.activeSerial = 0;
    _generation.previousDraft = {};
    _generation.progress.reset();
    _generation.cancellation.reset();
}

void MapCreatorController::applyGenerationCandidate(int candidateIndex)
{
    if (candidateIndex < 0 || candidateIndex >= static_cast<int>(_generation.candidates.size()))
        return;
    if (!generationCandidateEligible(_generation.candidates[static_cast<std::size_t>(candidateIndex)]))
    {
        _integration.statusMessage = gm::localization::trUi("This design is not valid and cannot be applied.");
        return;
    }
    captureUndoSnapshot("Apply Generated Design");
    const MapModel previous = _draftMap;
    _draftMap = _generation.candidates[static_cast<std::size_t>(candidateIndex)].map;
    for (auto &fact : _draftMap.generation.recipe.facts)
        if (mapSetupEntityFact(fact) && fact.entityRefs.size() > 2) fact.entityRefs.resize(2);
    resizeMapCells(_draftMap);

    // Generation owns generated geography, terrain and ways. Everything the
    // GM authored outside that domain remains attached to the draft.
    retainMapAuthoredMetadata(_draftMap, previous);
    _draftMap.name = previous.name;
    if (_draftMap.name.empty() || _draftMap.name == "New Map" || _draftMap.name == gm::localization::trUi("New Map"))
    {
        _draftMap.name = gm::localization::trUi(mapCreationIntentLabel(previous.creationIntent));
        if (_generation.structuredCreation && (previous.atlasLevel == MapLevel::World || previous.atlasLevel == MapLevel::Region))
        {
            const auto landform = _draftMap.generation.recipe.landform;
            const bool germanName = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
            std::string landformName;
            if (landform == MapLandform::Islands) landformName = gm::localization::trUi("Islands");
            else if (landform == MapLandform::InlandLake) landformName = gm::localization::trUi("Big lake");
            else if (landform == MapLandform::Continental) landformName = gm::localization::trUi("Continental");
            else if (landform == MapLandform::SingleIsland) landformName = germanName ? "Einzelne Insel" : "Single island";
            else if (landform == MapLandform::Coast) landformName = germanName ? "Küste" : "Coast";
            else if (landform == MapLandform::Peninsula) landformName = germanName ? "Halbinsel" : "Peninsula";
            if (!landformName.empty()) _draftMap.name += " – " + landformName;
            if (!_generation.setupEntities.empty()) _draftMap.name += " - " + _generation.setupEntities.front().name;
        }
        const auto baseName = _draftMap.name;
        for (int suffix = 2; std::any_of(_currentCampaign.maps.begin(), _currentCampaign.maps.end(),
                 [&](const auto &saved) { return saved.metadata.id != previous.metadata.id && saved.name == _draftMap.name; }); ++suffix)
            _draftMap.name = baseName + " " + std::to_string(suffix);
    }
    if (!previous.description.empty())
        _draftMap.description = previous.description;
    _draftMap.isMainMap = previous.isMainMap;
    _draftMap.parentMapId = previous.parentMapId;
    _draftMap.atlasLevel = previous.atlasLevel;
    _draftMap.mapKind = previous.mapKind;
    _draftMap.atlasStatus = previous.atlasStatus;
    _draftMap.atlasPlacement = previous.atlasPlacement;
    _draftMap.spatialReference = previous.spatialReference;
    // Accepting geography is not a new sharing decision. Keep the authored
    // categories and the existing baseline so later canonical changes still
    // merge correctly on preview/save, including after failed saves or Undo.
    applyMapCreatorPlayerSettings(_draftMap, captureMapCreatorPlayerSettings(previous));
    _draftMap.creationIntent = previous.creationIntent;
    _draftMap.sizePreset = previous.sizePreset;
    _draftMap.generation.recipe.creationIntent = previous.creationIntent;
    _draftMap.generation.recipe.sizePreset = previous.sizePreset;
    _draftMap.gridType = previous.atlasLevel == MapLevel::Tactical ? GridType::Square : previous.gridType;
    _draftMap.cellSize = previous.cellSize;
    _draftMap.backgroundImagePath = previous.backgroundImagePath;
    _draftMap.backgroundImageOpacity = previous.backgroundImageOpacity;
    _draftMap.importedReference = previous.importedReference;
    _draftMap.layers = previous.layers;
    _draftMap.tokens = previous.tokens;
    _draftMap.regions = previous.regions;
    _draftMap.features = previous.features;
    _draftMap.routes = previous.routes;

    // Fog is authored/session state. Preserve it by coordinate even though
    // the underlying generated terrain is replaced.
    const int sharedWidth = std::min(previous.width, _draftMap.width);
    const int sharedHeight = std::min(previous.height, _draftMap.height);
    for (int y = 0; y < sharedHeight; ++y)
    {
        for (int x = 0; x < sharedWidth; ++x)
        {
            const std::size_t oldIndex = static_cast<std::size_t>(y * previous.width + x);
            const std::size_t newIndex = static_cast<std::size_t>(y * _draftMap.width + x);
            if (oldIndex < previous.cells.size() && newIndex < _draftMap.cells.size())
                _draftMap.cells[newIndex].fogged = previous.cells[oldIndex].fogged;
        }
    }

    // Locked and inherited graph features are explicit preservation anchors.
    // Replace a generated feature with the old instance when IDs match and
    // append it when the new design did not produce that feature at all.
    const auto oldIdWasLocked = [&](const std::string &id) {
        return std::find(previous.generation.recipe.lockedFeatureIds.begin(),
                         previous.generation.recipe.lockedFeatureIds.end(), id) !=
               previous.generation.recipe.lockedFeatureIds.end();
    };
    for (const MapGeneratedFeature &feature : previous.generation.graph.features)
    {
        if (!feature.locked && !oldIdWasLocked(feature.id))
            continue;
        auto replacement = std::find_if(_draftMap.generation.graph.features.begin(),
                                        _draftMap.generation.graph.features.end(),
                                        [&](const MapGeneratedFeature &candidateFeature) {
                                            return candidateFeature.id == feature.id;
                                        });
        if (replacement != _draftMap.generation.graph.features.end())
            *replacement = feature;
        else
            _draftMap.generation.graph.features.push_back(feature);
        if (std::find(_draftMap.generation.recipe.lockedFeatureIds.begin(),
                      _draftMap.generation.recipe.lockedFeatureIds.end(), feature.id) ==
            _draftMap.generation.recipe.lockedFeatureIds.end())
            _draftMap.generation.recipe.lockedFeatureIds.push_back(feature.id);
    }
    for (const std::string &lockedId : previous.generation.recipe.lockedFeatureIds)
    {
        if (std::find(_draftMap.generation.recipe.lockedFeatureIds.begin(),
                      _draftMap.generation.recipe.lockedFeatureIds.end(), lockedId) ==
            _draftMap.generation.recipe.lockedFeatureIds.end())
            _draftMap.generation.recipe.lockedFeatureIds.push_back(lockedId);
    }

    _draftMap.revision = previous.revision + 1;
    _draftMap.metadata.touch();
    ensureDefaultMapLayer(_draftMap);
    normalizeObjectSelection();
    syncReferenceCalibrationDraft();
    _generation.lastQuality = _generation.candidates[static_cast<std::size_t>(candidateIndex)].quality;
    _generation.hasLastQuality = true;
    _integration.statusMessage = gm::localization::trUi("The generated design is applied. Terrain and ways remain editable.");
    _generation.candidates.clear();
    _generation.selectedGenerationCandidate = -1;
    _generation.recommendedGenerationCandidate = -1;
    _generation.rejectedGenerationCandidateCount = 0;
    _history.pendingNamedCheckpointLabel = gm::localization::trUi("Generated design applied");
}
