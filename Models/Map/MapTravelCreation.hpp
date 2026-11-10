#pragma once
#include "Models/Map/TravelSceneGenerator.hpp"
#include <algorithm>
#include <array>
#include <string>

inline constexpr std::array<const char *, 7> kStandaloneTravelBiomes = {
    "Forest", "Highlands", "Mountain pass", "River", "Grassland", "Lake", "Coast"};

inline int standaloneTravelBiomeIndex(const std::string &biome)
{
    const auto found = std::find_if(kStandaloneTravelBiomes.begin(), kStandaloneTravelBiomes.end(),
        [&](const char *name) { return biome == name; });
    return found == kStandaloneTravelBiomes.end() ? 0 : static_cast<int>(found - kStandaloneTravelBiomes.begin());
}

inline TravelSceneRecipe buildStandaloneTravelRecipe(const MapGenerationOptions &options,
    int purpose, int biome, int danger, bool combatReady)
{
    TravelSceneRecipe recipe;
    recipe.schemaVersion = options.generatorVersion >= 12 ? 4 : 3;
    recipe.width = options.width; recipe.height = options.height;
    recipe.seed = options.seed;
    recipe.purpose = static_cast<TravelScenePurpose>(std::clamp(purpose, 0, 4));
    recipe.biome = kStandaloneTravelBiomes[static_cast<std::size_t>(std::clamp(biome, 0, 6))];
    recipe.dangerLevel = std::clamp(danger, 0, 5); recipe.combatReady = combatReady;
    recipe.contentLanguage = options.contentLanguage; recipe.nameCulture = options.nameCulture;
    recipe.requiredElements = options.requiredElements; recipe.excludedElements = options.excludedElements;
    return recipe;
}

inline void storeStandaloneTravelRecipe(MapGenerationRecipe &stored, const TravelSceneRecipe &recipe)
{
    stored.travelScenePurpose = static_cast<int>(recipe.purpose);
    stored.travelSceneBiome = recipe.biome; stored.travelSceneDangerLevel = recipe.dangerLevel;
    stored.travelSceneCombatReady = recipe.combatReady; stored.travelSceneSeed = recipe.seed;
}

// Both the structured approximation and final asynchronous authoring call this
// adapter. It uses the reviewed purpose composer rather than generic local maps.
inline MapGenerationBatchResult generateStandaloneTravelMapBatch(
    const MapGenerationOptions &options, const TravelSceneRecipe &recipe, int count = 3)
{
    MapGenerationBatchResult batch;
    if (mapGenCancellationRequested(options)) { batch.failure = MapGenerationBatchFailureCode::Cancelled; return batch; }
    if ((recipe.schemaVersion != 3 && recipe.schemaVersion != 4) || recipe.width < 24 || recipe.width > 80 || recipe.height < 18 || recipe.height > 80)
    {
        batch.failure = MapGenerationBatchFailureCode::InvalidRequest;
        batch.requestValidation.eligible = false;
        batch.requestValidation.criticalIssueCount = 1;
        batch.requestValidation.blockers.push_back({"travel.request.dimensions", MapGenerationIssueSeverity::Critical,
            "Travel scenes require width 24–80 and height 18–80.", "Choose a supported map size."});
        return batch;
    }
    batch.requestValidation.eligible = true;
    auto candidates = TravelSceneGenerator::generateStandalone(recipe, count, options.cancellationRequested, options.progressCallback);
    if (mapGenCancellationRequested(options)) { batch.failure = MapGenerationBatchFailureCode::Cancelled; return batch; }
    for (auto &candidate : candidates)
    {
        MapGenerationResult result;
        result.map = std::move(candidate.localMap); result.quality = candidate.quality;
        storeStandaloneTravelRecipe(result.map.generation.recipe, recipe);
        result.validation = validateMapGeneration(result.map, result.quality);
        batch.attemptsEvaluated = std::max(batch.attemptsEvaluated, result.quality.candidatesEvaluated);
        if (result.validation.eligible && mapGenQualityMeetsCandidateFloor(result.map, result.quality))
            batch.candidates.push_back(std::move(result));
        else batch.rejectedCandidates.push_back(std::move(result));
    }
    batch.failure = batch.candidates.empty() ? MapGenerationBatchFailureCode::NoEligibleCandidate : MapGenerationBatchFailureCode::None;
    batch.recommendedIndex = batch.candidates.empty() ? -1 : 0;
    return batch;
}
