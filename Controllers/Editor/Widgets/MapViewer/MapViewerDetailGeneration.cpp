#include "Controllers/Editor/Widgets/MapViewer/MapViewerControllerInternals.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"

#include <cstdio>

namespace
{
const char *detailRejectionLabel(MapDetailGenerationRejectionReason reason)
{
    switch (reason)
    {
    case MapDetailGenerationRejectionReason::LockedAnchors:
        return gm::localization::trUi("the locked inherited anchors");
    case MapDetailGenerationRejectionReason::UsableTerrain:
        return gm::localization::trUi("connected usable terrain");
    case MapDetailGenerationRejectionReason::RoadContinuity:
        return gm::localization::trUi("road continuity");
    case MapDetailGenerationRejectionReason::RiverOrCrossingContinuity:
        return gm::localization::trUi("river or crossing continuity");
    case MapDetailGenerationRejectionReason::SettlementAccess:
        return gm::localization::trUi("settlement access");
    case MapDetailGenerationRejectionReason::QualityFloor:
    case MapDetailGenerationRejectionReason::None:
    case MapDetailGenerationRejectionReason::Count:
        return gm::localization::trUi("the required quality floor");
    }
    return gm::localization::trUi("the required quality floor");
}

const char *detailRecoveryMessage(MapDetailGenerationRejectionReason reason)
{
    switch (reason)
    {
    case MapDetailGenerationRejectionReason::LockedAnchors:
        return gm::localization::trUi("Widen the selection around the inherited features, or choose a neighboring area.");
    case MapDetailGenerationRejectionReason::UsableTerrain:
        return gm::localization::trUi("Widen the selection to include connected usable ground, or choose a neighboring area.");
    case MapDetailGenerationRejectionReason::RoadContinuity:
        return gm::localization::trUi("Widen the selection to include the nearby road approach, or choose a neighboring area.");
    case MapDetailGenerationRejectionReason::RiverOrCrossingContinuity:
        return gm::localization::trUi("Widen the selection to include both riverbanks and a crossing, or choose a neighboring area.");
    case MapDetailGenerationRejectionReason::SettlementAccess:
        return gm::localization::trUi("Widen the selection to include the settlement approach, or choose a neighboring area.");
    case MapDetailGenerationRejectionReason::QualityFloor:
    case MapDetailGenerationRejectionReason::None:
    case MapDetailGenerationRejectionReason::Count:
        return gm::localization::trUi("Choose a neighboring area with more surrounding terrain.");
    }
    return gm::localization::trUi("Choose a neighboring area with more surrounding terrain.");
}

std::string detailGenerationFailureMessage(const MapDetailGenerationJobResult &result)
{
    const std::string localizedFormat = gm::localization::trUi(
        "No compatible detail design was found after %d checks. This selection requires %d inherited anchors. Most often, rejected designs could not meet: %s. %s");
    const char *reason = detailRejectionLabel(result.primaryRejectionReason);
    const char *recovery = detailRecoveryMessage(result.primaryRejectionReason);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), result.checkedCandidateCount,
                                       result.inheritedAnchorCount, reason, recovery);
    if (required <= 0)
        return localizedFormat;
    std::string message(static_cast<std::size_t>(required), '\0');
    std::snprintf(message.data(), message.size() + 1, localizedFormat.c_str(), result.checkedCandidateCount,
                  result.inheritedAnchorCount, reason, recovery);
    return message;
}
} // namespace

void MapViewerController::generateDetailCandidates()
{
    MapModel *parent = getSelectedMap();
    const bool levelValid = parent && (_detailAdvancedLevelOverride
                                           ? MapAtlasService::isDeeperLevel(_detailTargetLevel, parent->atlasLevel)
                                           : MapAtlasService::isNextLevel(_detailTargetLevel, parent->atlasLevel));
    if (!parent || !hasDetailSelection() || !levelValid || _detailTargetLevel == MapLevel::World)
    {
        _atlasStatusMessage = gm::localization::trUi("Choose a valid area and a deeper atlas level first.");
        return;
    }
    if (_detailGenerationRunning)
    {
        _atlasStatusMessage = gm::localization::trUi("Detail designs are already being generated.");
        return;
    }

    const MapCreationIntent intent = detailIntentForLevel(_detailTargetLevel);
    const MapDimensions dimensions = mapDimensionsForPreset(intent, MapSizePreset::Standard);
    MapAtlasPlacement placement;
    placement.parentId = parent->metadata.id;
    placement.parentRect = selectedDetailRect();
    placement.parentRevision = parent->revision;

    MapAtlasExtractionResult inherited = MapAtlasService::extractFromParent(*parent, placement, dimensions.width, dimensions.height);
    if (!inherited.valid)
    {
        _atlasStatusMessage = inherited.errors.empty()
                                  ? gm::localization::trUi("The selected atlas area could not be inherited.")
                                  : gm::localization::trSystemMessage(inherited.errors.front());
        return;
    }

    MapCreationRequest request;
    request.intent = intent;
    request.sizePreset = MapSizePreset::Standard;
    request.dimensions = dimensions;
    request.spatialReference = deriveChildSpatialReference(*parent, placement.parentRect);
    request.generatorVersion = 12;
    request.recipeVersion = kCurrentMapIllustrationRecipe;
    request.styleId = "illustrated_atlas";
    request.styleVersion = 6;
    request.styleSnapshotId = "illustrated_atlas_v6";
    request.quality = MapGenerationQualityLevel::Balanced;
    request.contentLanguage = parent->generation.recipe.contentLanguage;
    request.nameCulture = parent->generation.recipe.nameCulture;
    request.parentMapId = parent->metadata.id;
    request.parentSelection = placement.parentRect;
    const std::uint32_t baseSeed = parent->generation.recipe.seed;
    request.seed = baseSeed ^ (static_cast<std::uint32_t>(_detailStartX + 31) * 73856093u) ^
                   (static_cast<std::uint32_t>(_detailStartY + 17) * 19349663u) ^
                   (static_cast<std::uint32_t>(MapAtlasService::levelDepth(_detailTargetLevel)) * 83492791u);
    for (const MapAtlasBoundaryConstraint &boundary : inherited.boundaryConstraints)
    {
        MapGenerationConstraint constraint;
        constraint.id = boundary.id;
        constraint.label = boundary.id;
        constraint.featureKind = boundary.featureKind;
        constraint.x = boundary.childPoint.x;
        constraint.y = boundary.childPoint.y;
        constraint.required = true;
        constraint.locked = true;
        request.constraints.push_back(std::move(constraint));
    }
    for (const MapGeneratedFeature &feature : inherited.graph.features)
    {
        if (feature.points.empty() || (feature.kind != MapGeneratedFeatureKind::Settlement && feature.kind != MapGeneratedFeatureKind::Landmark))
            continue;
        request.constraints.push_back({feature.id, feature.name, feature.kind, feature.points.front().x, feature.points.front().y, true, true});
    }

    _detailCandidates.clear();
    _selectedDetailCandidate = -1;
    _detailGenerationProgress = std::make_shared<MapDetailGenerationProgressState>();
    _detailGenerationProgress->totalSteps.store(detailBalancedAttemptBudget, std::memory_order_relaxed);
    _detailGenerationRunning = true;
    const std::uint64_t requestSerial = ++_detailRequestSerial;
    _atlasStatusMessage = gm::localization::trUi("Generating linked detail designs in the background...");
    _detailGenerationFuture = std::async(
        std::launch::async,
        [requestSerial, parentCopy = *parent, placement, inherited = std::move(inherited), request,
         progress = _detailGenerationProgress]() mutable {
            return buildDetailCandidates(
                requestSerial, std::move(parentCopy), placement, std::move(inherited), request, progress);
        });
}

void MapViewerController::cancelDetailGeneration()
{
    if (!_detailGenerationRunning)
        return;
    if (_detailGenerationProgress)
        _detailGenerationProgress->discardResult.store(true, std::memory_order_relaxed);
    ++_detailRequestSerial;
    _atlasStatusMessage = gm::localization::trUi("The running detail search will be discarded.");
}

void MapViewerController::pollDetailGeneration()
{
    if (!_detailGenerationRunning || !_detailGenerationFuture.valid() ||
        _detailGenerationFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;

    MapDetailGenerationJobResult result;
    try
    {
        result = _detailGenerationFuture.get();
    }
    catch (const std::exception &)
    {
        _detailGenerationRunning = false;
        _detailGenerationProgress.reset();
        _atlasStatusMessage = gm::localization::trUi("The detail search failed. The parent map was not changed.");
        return;
    }
    _detailGenerationRunning = false;
    _detailGenerationProgress.reset();

    const MapModel *parent = getSelectedMap();
    if (result.discarded || result.failure == MapGenerationBatchFailureCode::Cancelled)
    {
        if (parent && parent->metadata.id == result.parentMapId && hasDetailSelection() &&
            result.targetLevel == _detailTargetLevel && sameNormalizedRect(result.parentSelection, selectedDetailRect()))
            _atlasStatusMessage = gm::localization::trUi("Detail generation was cancelled.");
        return;
    }
    const bool stale = result.requestSerial != _detailRequestSerial || !parent ||
                       parent->metadata.id != result.parentMapId || parent->revision != result.parentRevision ||
                       result.targetLevel != _detailTargetLevel || !hasDetailSelection() ||
                       !sameNormalizedRect(result.parentSelection, selectedDetailRect());
    if (stale)
    {
        _atlasStatusMessage = gm::localization::trUi("The outdated detail result was discarded.");
        return;
    }

    _detailCandidates = std::move(result.candidates);
    _selectedDetailCandidate = -1;
    if (_detailCandidates.empty())
    {
        _atlasStatusMessage = result.failure == MapGenerationBatchFailureCode::InvalidRequest
                                  ? gm::localization::trUi("The inherited atlas anchors conflict with this detail request.")
                                  : detailGenerationFailureMessage(result);
        return;
    }
    _selectedDetailCandidate = 0;
    _atlasStatusMessage = gm::localization::trUi(
        _detailCandidates.size() == detailCandidateCount
            ? "Three linked detail designs are ready for review."
            : "The valid linked detail designs are ready for review.");
}

bool MapViewerController::createSelectedDetailMap()
{
    if (_selectedDetailCandidate < 0 || _selectedDetailCandidate >= static_cast<int>(_detailCandidates.size()))
        return false;
    const MapGenerationResult &selected = _detailCandidates[static_cast<std::size_t>(_selectedDetailCandidate)];
    const MapModel *parent = findMapById(selected.map.atlasPlacement.parentId);
    const bool levelValid = parent && (_detailAdvancedLevelOverride
                                           ? MapAtlasService::isDeeperLevel(selected.map.atlasLevel, parent->atlasLevel)
                                           : MapAtlasService::isNextLevel(selected.map.atlasLevel, parent->atlasLevel));
    if (!parent || parent->revision != selected.map.atlasPlacement.parentRevision || !levelValid ||
        !selected.validation.eligible || !mapGenQualityMeetsCandidateFloor(selected.map, selected.quality))
    {
        _atlasStatusMessage = gm::localization::trUi("The parent or selected design changed. Generate detail designs again.");
        _detailCandidates.clear();
        _selectedDetailCandidate = -1;
        return false;
    }
    MapModel created = selected.map;
    if (!_detailMapName.empty())
        created.name = _detailMapName;
    if (created.name.empty())
        created.name = gm::localization::trUi("Atlas Detail");
    const EntityId createdId = created.metadata.id;
    MapViewerCampaignSaveTransaction<MapGenerationResult> transaction(
        _currentCampaign,
        _selectedMapIndex,
        _detailCandidates,
        _selectedDetailCandidate);
    _currentCampaign.maps.push_back(std::move(created));
    if (!transaction.persistOrRollback([this]() { return saveCampaign(); }))
    {
        // The failed attempt remains selectable and retryable; invalidate any
        // plan version advanced by saveCampaign before the exact rollback.
        ++_mapRenderContentVersion;
        return false;
    }

    // Selection clears detail candidates only after the new map is durable.
    // A failed save above leaves the parent selection and all designs intact.
    const bool selectedCreatedMap = selectMapById(createdId);
    if (!selectedCreatedMap)
    {
        _detailCandidates.clear();
        _selectedDetailCandidate = -1;
        clearDetailSelection();
    }
    _detailMapName.clear();
    if (selectedCreatedMap)
        _atlasStatusMessage = gm::localization::trUi("The detail map was added to the atlas.");
    return selectedCreatedMap;
}
