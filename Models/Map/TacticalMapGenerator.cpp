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
TacticalMapRecipe recipeFromGenerationOptions(const MapGenerationOptions &options)
{
    TacticalMapRecipe recipe;
    recipe.generatorVersion = options.generatorVersion >= 7 ? options.generatorVersion : 5;
    recipe.illustrationRecipeVersion = std::max(3, options.recipeVersion);
    recipe.width = options.width;
    recipe.height = options.height;
    recipe.seed = options.seed;
    recipe.quality = options.quality;
    recipe.coverDensity = std::clamp(options.generatorVersion >= 11 ? options.tacticalDensity :
        (options.tacticalDensity > 0 ? options.tacticalDensity : 24), 0, 60);
    recipe.objectiveCount = std::clamp(options.requestedLandmarks > 0 ? options.requestedLandmarks : 1, 1, 3);
    recipe.minimumCreatureFootprint = 2;
    recipe.requiredElements = options.requiredElements;
    recipe.preservedFeatures = options.preservedFeatures;
    recipe.progressCallback = options.progressCallback;
    recipe.cancellationRequested = options.cancellationRequested;

    if (options.creationIntent == MapCreationIntent::Cave || options.caveNetwork)
        recipe.mapTemplate = TacticalMapTemplate::Cave;
    else if (options.creationIntent == MapCreationIntent::Dungeon || options.scope == MapScope::Dungeon)
        recipe.mapTemplate = TacticalMapTemplate::StoneDungeon;
    else if (options.purpose == MapGenerationPurpose::SettlementEnvirons)
        recipe.mapTemplate = (options.seed & 1u) != 0u
                                 ? TacticalMapTemplate::VillageStreet
                                 : TacticalMapTemplate::CitySquare;
    else if (options.landscape == MapLandscapeProfile::ForestFrontier)
        recipe.mapTemplate = TacticalMapTemplate::ForestRoad;
    else if (options.landscape == MapLandscapeProfile::RiverValley ||
             options.landscape == MapLandscapeProfile::LakeCountry)
        recipe.mapTemplate = TacticalMapTemplate::RiverCrossing;
    else if (options.landscape == MapLandscapeProfile::MountainPass ||
             options.landscape == MapLandscapeProfile::Highlands)
        recipe.mapTemplate = TacticalMapTemplate::Ruin;
    else
        recipe.mapTemplate = TacticalMapTemplate::OpenGround;
    return recipe;
}

std::vector<TacticalMapCandidate> generate(const TacticalMapRecipe &recipe, const MapModel *source, int candidateCount)
{
    candidateCount = std::clamp(candidateCount, 1, 3);
    const int attemptBudget = recipe.quality == MapGenerationQualityLevel::Draft
                                  ? 12
                                  : (recipe.quality == MapGenerationQualityLevel::Polished ? 48 : 24);
    MapGenerationOptions options;
    options.generatorVersion = recipe.generatorVersion >= 7 ? recipe.generatorVersion : 5;
    options.recipeVersion = 3;
    options.styleId = recipe.generatorVersion >= 12 ? "illustrated_atlas" : defaultMapStyleId();
    options.styleVersion = recipe.generatorVersion >= 12 ? 6 : 3;
    options.styleSnapshotId = recipe.generatorVersion >= 12 ? "illustrated_atlas_v6" : mapStyleCurrentProductionSnapshotId();
    options.scope = MapScope::Local;
    options.scale = MapLevel::Tactical;
    options.creationIntent = MapCreationIntent::TacticalEncounter;
    options.sizePreset = MapSizePreset::Custom;
    options.quality = recipe.quality;
    options.width = std::clamp(recipe.width, 12, recipe.generatorVersion >= 10 ? 80 : 64);
    options.height = std::clamp(recipe.height, 10, recipe.generatorVersion >= 10 ? 80 : 64);
    options.seed = recipe.seed;
    options.qualityAttempts = attemptBudget;
    options.requestedSettlements = 0;
    options.requestedLandmarks = 0;
    options.edgeConnectionCount = 2;
    options.tacticalDensity = 0;
    options.roadNetwork = MapRoadNetworkMode::Sparse;
    options.requiredElements = recipe.requiredElements;
    options.cancellationRequested = recipe.cancellationRequested;

    // Reserve the second half of the progress range for actual encounter
    // composition. The semantic generator's "Complete" means only that the
    // base-layout search has completed, not that the playable battlefield is
    // ready yet.
    const int semanticProgressSteps = attemptBudget + 2;
    const int totalProgressSteps = semanticProgressSteps + attemptBudget;
    if (recipe.progressCallback)
    {
        const MapGenerationProgressCallback progressCallback = recipe.progressCallback;
        options.progressCallback = [progressCallback, semanticProgressSteps, totalProgressSteps](
                                       const MapGenerationProgress &progress) {
            const bool semanticComplete = progress.totalSteps > 0 && progress.step >= progress.totalSteps;
            progressCallback(MapGenerationProgress{
                std::clamp(progress.step, 0, semanticProgressSteps),
                totalProgressSteps,
                semanticComplete ? "Adding tactical detail" : progress.stage});
        };
    }

    switch (recipe.mapTemplate)
    {
    case TacticalMapTemplate::ForestRoad:
        options.landscape = MapLandscapeProfile::ForestFrontier;
        options.purpose = MapGenerationPurpose::TravelArea;
        options.forestDensity = 18;
        break;
    case TacticalMapTemplate::RiverCrossing:
        options.landscape = MapLandscapeProfile::RiverValley;
        options.purpose = MapGenerationPurpose::TravelArea;
        options.waterLevel = 18;
        break;
    case TacticalMapTemplate::Ruin:
        options.landscape = MapLandscapeProfile::Highlands;
        options.purpose = MapGenerationPurpose::Wilderness;
        options.mountainDensity = 8;
        break;
    case TacticalMapTemplate::VillageStreet:
    case TacticalMapTemplate::CitySquare:
        // Tactical streets replace their base with authored encounter geometry.
        // A gen12 settlement site can contain piers whose site-only validation
        // correctly differs from tactical bridges; it must not gate this base.
        options.purpose = recipe.generatorVersion >= 12 ? MapGenerationPurpose::Wilderness : MapGenerationPurpose::SettlementEnvirons;
        if (recipe.generatorVersion >= 12) {
            options.landscape = MapLandscapeProfile::Highlands;
            options.waterLevel = 0;
            options.mountainDensity = 0;
        }
        options.forestDensity = 0;
        break;
    case TacticalMapTemplate::Cave:
        options.scope = MapScope::Dungeon;
        options.caveNetwork = true;
        break;
    case TacticalMapTemplate::StoneDungeon:
        options.scope = MapScope::Dungeon;
        break;
    case TacticalMapTemplate::OpenGround:
    default:
        options.landscape = MapLandscapeProfile::Highlands;
        options.purpose = MapGenerationPurpose::Wilderness;
        options.mountainDensity = 0;
        options.forestDensity = 0;
        options.waterLevel = 0;
        break;
    }

    MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, attemptBudget);
    if (batch.failure == MapGenerationBatchFailureCode::InvalidRequest ||
        batch.failure == MapGenerationBatchFailureCode::Cancelled)
        return {};
    if (recipe.cancellationRequested && recipe.cancellationRequested())
        return {};

    // The semantic base can legitimately miss a soft hierarchy floor before
    // objectives, deployment zones and cover have been composed. Such maps
    // remain internal repair inputs only when their hard validation passed;
    // the finished tactical map is checked against every strict gate below.
    std::vector<MapGenerationResult> generated = std::move(batch.candidates);
    for (MapGenerationResult &rejected : batch.rejectedCandidates)
        if (rejected.validation.eligible)
            generated.push_back(std::move(rejected));
    if (generated.empty())
        return {};

    // The fixed 12/24/48 search budget has already generated, validated, and
    // ranked every semantic base above. Building deployments, objectives,
    // cover, crossings, and readiness evidence for all 48 bases made a single
    // polished preview unnecessarily expensive. Refine a deterministic ranked
    // shortlist, and only continue past it when too few playable results were
    // found. This keeps the quality contract and attempts-evaluated evidence
    // intact without making runtime depend on wall-clock speed.
    const int minimumCompositionBudget = std::min(
        static_cast<int>(generated.size()),
        std::max(8, candidateCount * 4));
    std::vector<TacticalMapCandidate> validCandidates;
    validCandidates.reserve(static_cast<std::size_t>(minimumCompositionBudget));
    for (int index = 0; index < static_cast<int>(generated.size()); ++index)
    {
        if (recipe.cancellationRequested && recipe.cancellationRequested())
            return {};
        if (recipe.progressCallback)
            recipe.progressCallback(MapGenerationProgress{
                semanticProgressSteps + std::min(index + 1, attemptBudget),
                totalProgressSteps,
                "Placing tactical features"});
        if (index >= minimumCompositionBudget &&
            static_cast<int>(validCandidates.size()) >= candidateCount)
            break;
        TacticalMapCandidate candidate;
        candidate.templateLabel = templateName(recipe.mapTemplate);
        candidate.quality = generated[static_cast<std::size_t>(index)].quality;
        candidate.map = std::move(generated[static_cast<std::size_t>(index)].map);
        const std::uint32_t selectedSeed = candidate.quality.selectedSeed;
        candidate.map.metadata.id = "tactical::" + std::to_string(static_cast<int>(recipe.mapTemplate)) + "::" + std::to_string(selectedSeed);
        candidate.map.metadata.customProperties["tactical.composition_revision"] =
            std::to_string(recipe.generatorVersion >= 12 && recipe.illustrationRecipeVersion >= 6 ? kTacticalCompositionRevisionRecipe6 : recipe.generatorVersion >= 12 ? kTacticalCompositionRevisionV12 : recipe.generatorVersion >= 11 ? kTacticalCompositionRevisionV11 : recipe.generatorVersion >= 10 ? kTacticalCompositionRevisionV10 : recipe.generatorVersion >= 9 ? kTacticalCompositionRevisionV9 : recipe.generatorVersion >= 8 ? kTacticalCompositionRevisionV8 : recipe.generatorVersion >= 7 ? kTacticalCompositionRevisionV7 : kTacticalCompositionRevision);
        if(recipe.generatorVersion >= 10)
            candidate.map.metadata.customProperties["tactical.template_kind"] = std::to_string(static_cast<int>(recipe.mapTemplate));
        candidate.map.creationIntent = MapCreationIntent::TacticalEncounter;
        candidate.map.sizePreset = MapSizePreset::Custom;
        candidate.map.scope = isSubterranean(recipe.mapTemplate) ? MapScope::Dungeon : MapScope::Local;
        candidate.map.atlasLevel = MapLevel::Tactical;
        candidate.map.mapKind = MapKind::Encounter;
        candidate.map.gridType = recipe.gridType;
        candidate.map.cellSize = 5.0f;
        candidate.map.visibility = MapVisibility::GameMasterOnly;
        candidate.map.revision = 1;
        candidate.map.generation.recipe.generatorVersion = options.generatorVersion;
        candidate.map.generation.recipe.recipeVersion = std::max(3, recipe.illustrationRecipeVersion);
        candidate.map.generation.recipe.styleId = options.styleId;
        candidate.map.generation.recipe.styleVersion = options.styleVersion;
        candidate.map.generation.recipe.styleSnapshotId = options.styleSnapshotId;
        candidate.map.generation.recipe.scale = MapLevel::Tactical;
        candidate.map.generation.recipe.quality = recipe.quality;
        candidate.map.generation.recipe.creationIntent = MapCreationIntent::TacticalEncounter;
        candidate.map.generation.recipe.sizePreset = MapSizePreset::Custom;

        composeTemplate(candidate.map, recipe, selectedSeed);
        // The template establishes only the local base terrain. Parent atlas
        // geometry is installed before deployments, objectives and cover are
        // chosen so those systems evaluate the actual inherited battlefield.
        installTacticalPreservedFeatures(candidate.map, recipe);
        connectTacticalPreservedRoutes(candidate.map, recipe);
        publishTacticalInheritedCrossings(candidate.map);
        if (source && (source->atlasLevel == MapLevel::Region || source->atlasLevel == MapLevel::Site))
        {
            candidate.map.parentMapId = source->metadata.id;
            candidate.map.atlasPlacement.parentId = source->metadata.id;
            candidate.map.atlasPlacement.parentRect = {0.1f, 0.1f, 0.8f, 0.8f};
            candidate.map.atlasPlacement.parentRevision = source->revision;
            candidate.map.atlasPlacement.sourceRevision = 1;
            candidate.map.atlasStatus = MapAtlasStatus::Linked;
        }
        else
        {
            candidate.map.atlasStatus = MapAtlasStatus::Standalone;
        }

        const std::array<Point, 2> deploymentPreferences =
            tacticalDeploymentPreferences(candidate.map, recipe);
        const Point partyOrigin = findTacticalSpawnOrigin(
            candidate.map, deploymentPreferences[0], 3, 4);
        const Point oppositionOrigin = findTacticalSpawnOrigin(
            candidate.map, deploymentPreferences[1], 3, 4);
        if (partyOrigin.x >= 0)
            addSpawnZone(candidate.map, "Party Start", partyOrigin.x, partyOrigin.y, 3, 4);
        if (oppositionOrigin.x >= 0)
            addSpawnZone(candidate.map, "Opposition Start", oppositionOrigin.x, oppositionOrigin.y, 3, 4);

        const int objectiveCount = std::clamp(recipe.objectiveCount, 1, 3);
        for (int objective = 0; objective < objectiveCount; ++objective)
        {
            int x = candidate.map.width / 2 + (objective - (objectiveCount - 1) / 2) * std::max(3, candidate.map.width / 8);
            int y = candidate.map.height / 2 + (objective % 2 == 0 ? -1 : 1);
            // A river objective belongs on a crossing rather than punching an
            // artificial island into the channel's semantic cells.
            if (recipe.mapTemplate == TacticalMapTemplate::RiverCrossing)
            {
                x = candidate.map.width / 2;
                y = objective % 2 == 0 ? tacticalCrossingUpperY(candidate.map)
                                       : tacticalCrossingLowerY(candidate.map);
            }
            const Point objectivePoint = findTacticalObjective(candidate.map, {x, y});
            if (objectivePoint.x >= 0)
                addObjectiveZone(candidate.map, objective, objectivePoint.x, objectivePoint.y);
        }
        addFairCover(candidate.map, recipe, selectedSeed);
        addRequiredElements(candidate.map, recipe, selectedSeed);
        installTacticalPreservedFeatures(candidate.map, recipe);

        const bool anchorsExact = std::all_of(
            recipe.preservedFeatures.begin(), recipe.preservedFeatures.end(),
            [&](const MapGeneratedFeature &feature) {
                return tacticalPreservedFeatureMatches(candidate.map, feature);
            });
        if (!anchorsExact)
            continue;

        const int selectedIndex = candidate.quality.selectedCandidateIndex;
        candidate.quality = analyzeMapGenerationQuality(candidate.map);
        candidate.quality.candidatesEvaluated = attemptBudget;
        candidate.quality.selectedCandidateIndex = selectedIndex;
        candidate.quality.selectedSeed = selectedSeed;
        candidate.readinessIssues = validate(candidate.map, recipe);
        const MapGenerationValidationReport validation = validateMapGeneration(candidate.map, candidate.quality);
        if (!validation.eligible || !mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality) ||
            !candidate.readinessIssues.empty())
        {
#ifdef TTRPG_TACTICAL_QA_TRACE
#endif
            continue;
        }
        validCandidates.push_back(std::move(candidate));
    }

    std::stable_sort(validCandidates.begin(), validCandidates.end(), [](const TacticalMapCandidate &left,
                                                                        const TacticalMapCandidate &right) {
        return left.quality.score > right.quality.score;
    });
    std::vector<TacticalMapCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(candidateCount));
    if (!validCandidates.empty())
    {
        candidates.push_back(std::move(validCandidates.front()));
        validCandidates.erase(validCandidates.begin());
    }
    while (static_cast<int>(candidates.size()) < candidateCount && !validCandidates.empty())
    {
        std::size_t bestIndex = 0;
        int bestUtility = std::numeric_limits<int>::min();
        for (std::size_t index = 0; index < validCandidates.size(); ++index)
        {
            int minimumDistance = 100;
            for (const TacticalMapCandidate &chosen : candidates)
                minimumDistance = std::min(minimumDistance,
                                           mapGenCompositionDistancePercent(validCandidates[index].map, chosen.map));
            const int utility = validCandidates[index].quality.score + minimumDistance * 3;
            if (utility > bestUtility)
            {
                bestUtility = utility;
                bestIndex = index;
            }
        }
        candidates.push_back(std::move(validCandidates[bestIndex]));
        validCandidates.erase(validCandidates.begin() + static_cast<std::ptrdiff_t>(bestIndex));
    }
    for (int index = 0; index < static_cast<int>(candidates.size()); ++index)
    {
        TacticalMapCandidate &candidate = candidates[static_cast<std::size_t>(index)];
        candidate.rank = index + 1;
        candidate.map.name = candidate.templateLabel + " #" + std::to_string(index + 1);
    }
    if (recipe.cancellationRequested && recipe.cancellationRequested())
        return {};
    if (recipe.progressCallback)
        recipe.progressCallback(MapGenerationProgress{
            totalProgressSteps,
            totalProgressSteps,
            "Complete"});
    return candidates;
}
}
