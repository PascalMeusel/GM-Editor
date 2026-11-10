#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "Models/Map/TacticalMapGeneratorInternal.hpp"

using namespace TacticalMapGeneratorInternal;

namespace TacticalMapGenerator
{
MapGenerationBatchResult generateBatch(const MapGenerationOptions &options,
                                       const MapModel *source,
                                       int candidateCount)
{
    MapGenerationBatchResult batch;
    MapGenerationOptions request = options;
    mapGenApplyFactsToOptions(request);
    batch.requestValidation = validateMapGenerationRequest(request);
    if (!batch.requestValidation.eligible)
    {
        batch.failure = MapGenerationBatchFailureCode::InvalidRequest;
        return batch;
    }
    if (mapGenCancellationRequested(request))
    {
        batch.failure = MapGenerationBatchFailureCode::Cancelled;
        return batch;
    }

    const int attemptBudget = request.quality == MapGenerationQualityLevel::Draft
                                  ? 12
                                  : (request.quality == MapGenerationQualityLevel::Polished ? 48 : 24);
    TacticalMapRecipe recipe = recipeFromGenerationOptions(request);
    std::vector<TacticalMapCandidate> tactical = generate(recipe, source, candidateCount);
    if (mapGenCancellationRequested(request))
    {
        batch.failure = MapGenerationBatchFailureCode::Cancelled;
        return batch;
    }
    batch.attemptsEvaluated = attemptBudget;
    for (TacticalMapCandidate &tacticalCandidate : tactical)
    {
        MapGenerationResult candidate;
        candidate.map = std::move(tacticalCandidate.map);
        candidate.quality = tacticalCandidate.quality;
        candidate.warnings = std::move(tacticalCandidate.readinessIssues);

        MapGenerationRecipe &persistedRecipe = candidate.map.generation.recipe;
        persistedRecipe.generatorVersion = request.generatorVersion;
        persistedRecipe.recipeVersion = request.recipeVersion;
        persistedRecipe.styleId = request.styleId;
        persistedRecipe.styleVersion = request.styleVersion;
        persistedRecipe.styleSnapshotId = request.styleSnapshotId;
        persistedRecipe.quality = request.quality;
        persistedRecipe.creationIntent = MapCreationIntent::TacticalEncounter;
        persistedRecipe.sizePreset = request.sizePreset;
        persistedRecipe.contentLanguage = request.contentLanguage;
        persistedRecipe.nameCulture = request.nameCulture;
        persistedRecipe.facts = request.facts;
        persistedRecipe.constraints = request.constraints;
        persistedRecipe.requiredElements = request.requiredElements;
        persistedRecipe.optionalElements = request.optionalElements;
        persistedRecipe.excludedElements = request.excludedElements;
        persistedRecipe.lockedFeatureIds = request.lockedFeatureIds;
        for (const MapGeneratedFeature &feature : request.preservedFeatures)
            if (feature.locked &&
                std::find(persistedRecipe.lockedFeatureIds.begin(),
                          persistedRecipe.lockedFeatureIds.end(), feature.id) ==
                    persistedRecipe.lockedFeatureIds.end())
                persistedRecipe.lockedFeatureIds.push_back(feature.id);

        candidate.validation = validateMapGeneration(candidate.map, candidate.quality);
        if (candidate.validation.eligible && candidate.warnings.empty() &&
            mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality))
            batch.candidates.push_back(std::move(candidate));
        else
            batch.rejectedCandidates.push_back(std::move(candidate));
    }
    batch.recommendedIndex = batch.candidates.empty() ? -1 : 0;
    batch.failure = batch.candidates.empty()
                        ? MapGenerationBatchFailureCode::NoEligibleCandidate
                        : MapGenerationBatchFailureCode::None;
    return batch;
}
}
