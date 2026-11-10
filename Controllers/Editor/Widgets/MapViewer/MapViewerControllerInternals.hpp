#pragma once

#include "MapViewerController.hpp"
#include "Controllers/Editor/Widgets/MapViewer/MapViewerCampaignSaveTransaction.hpp"
#include "Models/Audio/AudioTriggerService.hpp"
#include "Models/Combat/EncounterAtlasService.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <exception>
#include <utility>

namespace
{
constexpr int detailBalancedAttemptBudget = 24;
constexpr int detailCandidateCount = 3;

MapDetailGenerationRejectionReason detailRejectionReasonFor(const MapGenerationResult &candidate)
{
    const auto hasBlocker = [&](const char *code) {
        return std::any_of(candidate.validation.blockers.begin(), candidate.validation.blockers.end(),
                           [&](const MapGenerationIssue &issue) { return issue.code == code; });
    };
    if (hasBlocker("atlas.anchor_changed") || hasBlocker("requirement.constraint_missing"))
        return MapDetailGenerationRejectionReason::LockedAnchors;
    if (hasBlocker("terrain.disconnected"))
        return MapDetailGenerationRejectionReason::UsableTerrain;
    if (hasBlocker("river.disconnected") || hasBlocker("river.shared_requirement") || hasBlocker("bridge.invalid"))
        return MapDetailGenerationRejectionReason::RiverOrCrossingContinuity;
    if (hasBlocker("road.world_network_too_small") || hasBlocker("road.world_terrain_conflict") ||
        candidate.quality.wayConnectivityPercent < 80 || candidate.quality.roadEfficiencyPercent < 80)
        return MapDetailGenerationRejectionReason::RoadContinuity;
    if (hasBlocker("settlement.no_access") || hasBlocker("settlement.nation_unrepresented") ||
        candidate.quality.structureAccessPercent < 80 || candidate.quality.settlementSuitabilityPercent < 80)
        return MapDetailGenerationRejectionReason::SettlementAccess;
    if (candidate.quality.traversableConnectivityPercent < 80)
        return MapDetailGenerationRejectionReason::UsableTerrain;
    if (candidate.quality.hydrologyPercent < 80 || candidate.quality.bridgeValidityPercent < 80)
        return MapDetailGenerationRejectionReason::RiverOrCrossingContinuity;
    return MapDetailGenerationRejectionReason::QualityFloor;
}

void recordDetailRejection(MapDetailGenerationJobResult &job, const MapGenerationResult &candidate)
{
    ++job.rejectedCandidateCount;
    const MapDetailGenerationRejectionReason reason = detailRejectionReasonFor(candidate);
    ++job.rejectionReasonCounts[static_cast<std::size_t>(reason)];
}

void selectPrimaryDetailRejectionReason(MapDetailGenerationJobResult &job)
{
    int greatestCount = 0;
    for (std::size_t index = 1; index < job.rejectionReasonCounts.size(); ++index)
    {
        const int count = job.rejectionReasonCounts[index];
        if (count > greatestCount)
        {
            greatestCount = count;
            job.primaryRejectionReason = static_cast<MapDetailGenerationRejectionReason>(index);
        }
    }
}

MapLevel nextAtlasLevel(MapLevel level)
{
    const int nextDepth = std::min(
        MapAtlasService::levelDepth(level) + 1,
        MapAtlasService::levelDepth(MapLevel::Tactical));
    return static_cast<MapLevel>(nextDepth);
}

MapCreationIntent detailIntentForLevel(MapLevel level)
{
    switch (level)
    {
    case MapLevel::Region:
        return MapCreationIntent::RegionExploration;
    case MapLevel::Site:
        return MapCreationIntent::Settlement;
    case MapLevel::Tactical:
        return MapCreationIntent::TacticalEncounter;
    case MapLevel::World:
        break;
    }
    return MapCreationIntent::RegionExploration;
}

MapSpatialReference deriveChildSpatialReference(const MapModel &parent, const MapNormalizedRect &rect)
{
    if (!parent.spatialReference.calibrated)
        return {};
    MapSpatialReference result = parent.spatialReference;
    result.extentWidth = parent.spatialReference.extentWidth * rect.width;
    result.extentHeight = parent.spatialReference.extentHeight * rect.height;
    result.originX = parent.spatialReference.originX + parent.spatialReference.extentWidth * rect.x;
    result.originY = parent.spatialReference.originY + parent.spatialReference.extentHeight * rect.y;
    return result;
}

bool sameNormalizedRect(const MapNormalizedRect &left, const MapNormalizedRect &right)
{
    constexpr float epsilon = 0.00001f;
    return std::abs(left.x - right.x) <= epsilon && std::abs(left.y - right.y) <= epsilon &&
           std::abs(left.width - right.width) <= epsilon && std::abs(left.height - right.height) <= epsilon;
}

bool sameFeaturePoint(const MapFeaturePoint &left, const MapFeaturePoint &right)
{
    constexpr float epsilon = 0.001f;
    return std::abs(left.x - right.x) <= epsilon && std::abs(left.y - right.y) <= epsilon;
}

bool inheritedAnchorIsPreserved(const MapModel &map, const MapGeneratedFeature &anchor)
{
    const auto found = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                    [&](const MapGeneratedFeature &feature) { return feature.id == anchor.id; });
    if (found == map.generation.graph.features.end() || !found->locked || found->kind != anchor.kind ||
        found->points.size() != anchor.points.size())
        return false;
    for (std::size_t index = 0; index < anchor.points.size(); ++index)
        if (!sameFeaturePoint(found->points[index], anchor.points[index]))
            return false;
    return true;
}

bool inheritedBoundaryConstraintIsSatisfied(
    const MapGenerationConstraint &constraint,
    const MapAtlasExtractionResult &inherited)
{
    if (constraint.id.rfind("atlas-edge::", 0) != 0)
        return false;
    for (const MapGeneratedFeature &anchor : inherited.graph.features)
    {
        if (anchor.kind != constraint.featureKind)
            continue;
        const bool hasPoint = std::any_of(anchor.points.begin(), anchor.points.end(), [&](const MapFeaturePoint &point) {
            return std::abs(point.x - constraint.x) <= 0.01f && std::abs(point.y - constraint.y) <= 0.01f;
        });
        if (hasPoint)
            return true;
    }
    return false;
}

void mergeInheritedAnchors(MapGenerationResult &candidate, const MapAtlasExtractionResult &inherited)
{
    mapGenInstallPreservedFeatures(candidate.map, inherited.graph.features);
}

bool finalizeDetailCandidate(
    MapGenerationResult &candidate,
    const MapModel &parent,
    const MapAtlasPlacement &placement,
    const MapAtlasExtractionResult &inherited,
    const MapCreationRequest &request,
    const std::vector<std::string> &tacticalReadinessIssues = {})
{
    candidate.map.creationIntent = request.intent;
    candidate.map.sizePreset = request.sizePreset;
    candidate.map.atlasLevel = mapLevelForCreationIntent(request.intent);
    candidate.map.mapKind = mapKindForCreationIntent(request.intent);
    candidate.map.scope = mapLegacyScopeForCreationIntent(request.intent);
    candidate.map.gridType = candidate.map.atlasLevel == MapLevel::Tactical ? GridType::Square : GridType::None;
    candidate.map.cellSize = candidate.map.atlasLevel == MapLevel::Tactical ? 5.0f : candidate.map.cellSize;
    candidate.map.atlasStatus = MapAtlasStatus::Linked;
    candidate.map.visibility = MapVisibility::GameMasterOnly;
    candidate.map.parentMapId = parent.metadata.id;
    candidate.map.atlasPlacement = placement;
    candidate.map.spatialReference = request.spatialReference;
    candidate.map.revision = 1;
    candidate.map.atlasPlacement.sourceRevision = candidate.map.revision;

    MapGenerationRecipe &recipe = candidate.map.generation.recipe;
    recipe.generatorVersion = request.generatorVersion;
    recipe.recipeVersion = request.recipeVersion;
    recipe.styleId = request.styleId;
    recipe.styleVersion = request.styleVersion;
    recipe.styleSnapshotId = request.styleSnapshotId;
    recipe.scale = candidate.map.atlasLevel;
    recipe.quality = request.quality;
    recipe.creationIntent = request.intent;
    recipe.sizePreset = request.sizePreset;
    recipe.contentLanguage = request.contentLanguage;
    recipe.nameCulture = request.nameCulture;
    recipe.constraints = request.constraints;
    recipe.facts = request.facts;

    mergeInheritedAnchors(candidate, inherited);
    const int selectedCandidateIndex = candidate.quality.selectedCandidateIndex;
    const std::uint32_t selectedSeed = candidate.map.generation.recipe.seed;
    candidate.quality = analyzeMapGenerationQuality(candidate.map);
    candidate.quality.candidatesEvaluated = detailBalancedAttemptBudget;
    candidate.quality.selectedCandidateIndex = selectedCandidateIndex;
    candidate.quality.selectedSeed = selectedSeed;
    candidate.validation = validateMapGeneration(candidate.map, candidate.quality);
    candidate.validation.blockers.erase(
        std::remove_if(candidate.validation.blockers.begin(), candidate.validation.blockers.end(),
                       [&](const MapGenerationIssue &issue) {
                           if (issue.code != "requirement.constraint_missing")
                               return false;
                           const auto constraint = std::find_if(request.constraints.begin(), request.constraints.end(),
                                                                [&](const MapGenerationConstraint &value) {
                                                                    return value.id == issue.message || value.label == issue.message;
                                                                });
                           return constraint != request.constraints.end() &&
                                  inheritedBoundaryConstraintIsSatisfied(*constraint, inherited);
                       }),
        candidate.validation.blockers.end());
    for (const std::string &readinessIssue : tacticalReadinessIssues)
    {
        candidate.validation.blockers.push_back({
            "atlas.tactical_readiness", MapGenerationIssueSeverity::Critical, readinessIssue,
            "Generate another tactical design or adjust the selected area."});
    }
    for (const MapGeneratedFeature &anchor : inherited.graph.features)
    {
        if (inheritedAnchorIsPreserved(candidate.map, anchor))
            continue;
        candidate.validation.blockers.push_back({
            "atlas.anchor_changed", MapGenerationIssueSeverity::Critical, anchor.id,
            "Regenerate the detail map while keeping inherited atlas anchors locked."});
    }
    candidate.validation.criticalIssueCount = static_cast<int>(candidate.validation.blockers.size());
    candidate.validation.eligible = candidate.validation.blockers.empty();
    return candidate.validation.eligible && mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality);
}

TacticalMapTemplate chooseTacticalTemplate(const MapModel &parent, MapLandscapeProfile landscape, std::uint32_t seed)
{
    if (parent.scope == MapScope::Dungeon)
        return parent.generation.recipe.recipeId.find("cave") != std::string::npos
                   ? TacticalMapTemplate::Cave
                   : TacticalMapTemplate::StoneDungeon;
    if (parent.mapKind == MapKind::Settlement)
        return (seed & 1u) != 0u ? TacticalMapTemplate::VillageStreet : TacticalMapTemplate::CitySquare;
    if (landscape == MapLandscapeProfile::ForestFrontier)
        return TacticalMapTemplate::ForestRoad;
    if (landscape == MapLandscapeProfile::RiverValley || landscape == MapLandscapeProfile::LakeCountry)
        return TacticalMapTemplate::RiverCrossing;
    return (seed & 1u) != 0u ? TacticalMapTemplate::Ruin : TacticalMapTemplate::OpenGround;
}

MapDetailGenerationJobResult buildDetailCandidates(
    std::uint64_t requestSerial,
    MapModel parent,
    MapAtlasPlacement placement,
    MapAtlasExtractionResult inherited,
    MapCreationRequest request,
    std::shared_ptr<MapDetailGenerationProgressState> progress)
{
    MapDetailGenerationJobResult job;
    job.requestSerial = requestSerial;
    job.parentMapId = parent.metadata.id;
    job.parentRevision = parent.revision;
    job.parentSelection = placement.parentRect;
    job.targetLevel = mapLevelForCreationIntent(request.intent);
    job.checkedCandidateCount = detailBalancedAttemptBudget;
    job.inheritedAnchorCount = static_cast<int>(inherited.graph.features.size());

    MapGenerationOptions options = mapGenerationOptionsFromCreationRequest(request);
    // Edge anchors are inherited as exact locked line geometry below. Feeding
    // them into the legacy point-constraint hook would turn a river or road
    // endpoint into a one-cell landmark before the real atlas line is merged.
    options.constraints.erase(
        std::remove_if(options.constraints.begin(), options.constraints.end(), [](const MapGenerationConstraint &constraint) {
            return constraint.id.rfind("atlas-edge::", 0) == 0;
        }),
        options.constraints.end());
    options.preservedFeatures = inherited.graph.features;
    options.quality = MapGenerationQualityLevel::Balanced;
    options.qualityAttempts = detailBalancedAttemptBudget;
    options.landscape = parent.generation.recipe.landscape;
    options.requestedSettlements = job.targetLevel == MapLevel::Region ? 4 : 2;
    options.requestedLandmarks = job.targetLevel == MapLevel::Tactical ? 2 : 5;
    options.edgeConnectionCount = static_cast<int>(inherited.boundaryConstraints.size());
    options.progressCallback = [progress](const MapGenerationProgress &generationProgress) {
        progress->step.store(generationProgress.step, std::memory_order_relaxed);
        progress->totalSteps.store(std::max(1, generationProgress.totalSteps), std::memory_order_relaxed);
    };
    options.cancellationRequested = [progress]() {
        return progress->discardResult.load(std::memory_order_relaxed);
    };

    std::vector<MapGenerationResult> generated;
    if (job.targetLevel == MapLevel::Tactical)
    {
        // Balanced generation evaluates 24 deterministic semantic attempts
        // internally and returns the best three playable tactical designs.
        if (progress->discardResult.load(std::memory_order_relaxed))
        {
            job.discarded = true;
            return job;
        }
        TacticalMapRecipe tacticalRecipe;
        tacticalRecipe.generatorVersion = request.generatorVersion;
        tacticalRecipe.illustrationRecipeVersion = request.recipeVersion;
        tacticalRecipe.quality = MapGenerationQualityLevel::Balanced;
        tacticalRecipe.progressCallback = options.progressCallback;
        tacticalRecipe.cancellationRequested = options.cancellationRequested;
        tacticalRecipe.width = request.dimensions.width;
        tacticalRecipe.height = request.dimensions.height;
        tacticalRecipe.seed = request.seed;
        tacticalRecipe.gridType = GridType::Square;
        tacticalRecipe.objectiveCount = 1;
        tacticalRecipe.minimumCreatureFootprint = 2;
        tacticalRecipe.preservedFeatures = inherited.graph.features;
        tacticalRecipe.mapTemplate = chooseTacticalTemplate(parent, options.landscape, tacticalRecipe.seed);
        const std::vector<TacticalMapCandidate> tacticalBatch = TacticalMapGenerator::generate(tacticalRecipe, nullptr, 3);
        if (progress->discardResult.load(std::memory_order_relaxed))
        {
            job.discarded = true;
            return job;
        }
        for (const TacticalMapCandidate &tactical : tacticalBatch)
        {
            MapGenerationResult candidate;
            candidate.map = tactical.map;
            candidate.quality = tactical.quality;
            candidate.warnings = tactical.readinessIssues;
            candidate.map.generation.recipe.constraints = request.constraints;
            if (finalizeDetailCandidate(candidate, parent, placement, inherited, request, tactical.readinessIssues))
                generated.push_back(std::move(candidate));
            else
                recordDetailRejection(job, candidate);
        }
        progress->step.store(detailBalancedAttemptBudget, std::memory_order_relaxed);
        progress->totalSteps.store(detailBalancedAttemptBudget, std::memory_order_relaxed);
    }
    else
    {
        MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, detailBalancedAttemptBudget);
        job.failure = batch.failure;
        if (batch.failure == MapGenerationBatchFailureCode::Cancelled)
        {
            job.discarded = true;
            return job;
        }
        for (MapGenerationResult &candidate : batch.candidates)
            if (finalizeDetailCandidate(candidate, parent, placement, inherited, request))
                generated.push_back(std::move(candidate));
            else
                recordDetailRejection(job, candidate);
        for (const MapGenerationResult &rejected : batch.rejectedCandidates)
            recordDetailRejection(job, rejected);
    }

    if (progress->discardResult.load(std::memory_order_relaxed))
    {
        job.discarded = true;
        return job;
    }
    std::stable_sort(generated.begin(), generated.end(), [](const MapGenerationResult &left, const MapGenerationResult &right) {
        return left.quality.score > right.quality.score;
    });
    if (generated.size() > detailCandidateCount)
        generated.resize(detailCandidateCount);
    if (generated.empty() && job.failure == MapGenerationBatchFailureCode::None)
        job.failure = MapGenerationBatchFailureCode::NoCandidateMetQualityFloor;
    selectPrimaryDetailRejectionReason(job);
    job.candidates = std::move(generated);
    progress->step.store(progress->totalSteps.load(std::memory_order_relaxed), std::memory_order_relaxed);
    return job;
}
}
