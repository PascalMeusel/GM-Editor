#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"


std::uint32_t mapGenCandidateSeed(std::uint32_t baseSeed, int candidateIndex)
{
    if (candidateIndex <= 0)
        return baseSeed;
    // Odd Weyl increments distribute adjacent base seeds without introducing
    // platform-dependent entropy. The same base seed always explores the same
    // variations in the same order.
    return baseSeed + 0x9e3779b9u * static_cast<std::uint32_t>(candidateIndex);
}

int mapGenCompositionDistancePercent(const MapModel &left, const MapModel &right)
{
    if (left.width <= 0 || left.height <= 0 || right.width <= 0 || right.height <= 0)
        return 100;
    const auto visualClass = [](const MapCellModel *cell) {
        if (!cell)
            return 0;
        if (cell->kind == MapTileKind::Water)
            return 1;
        if (cell->kind == MapTileKind::Mountain || cell->kind == MapTileKind::Hill)
            return 2;
        if (cell->kind == MapTileKind::Forest)
            return 3;
        if (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building)
            return 4;
        if (cell->wayKind != MapWayKind::None || cell->kind == MapTileKind::Road || cell->kind == MapTileKind::Bridge)
            return 5;
        return 6;
    };

    constexpr int sampleColumns = 12;
    constexpr int sampleRows = 8;
    int different = 0;
    for (int sampleY = 0; sampleY < sampleRows; ++sampleY)
    {
        const int leftY = std::clamp((sampleY * 2 + 1) * left.height / (sampleRows * 2), 0, left.height - 1);
        const int rightY = std::clamp((sampleY * 2 + 1) * right.height / (sampleRows * 2), 0, right.height - 1);
        for (int sampleX = 0; sampleX < sampleColumns; ++sampleX)
        {
            const int leftX = std::clamp((sampleX * 2 + 1) * left.width / (sampleColumns * 2), 0, left.width - 1);
            const int rightX = std::clamp((sampleX * 2 + 1) * right.width / (sampleColumns * 2), 0, right.width - 1);
            different +=
                visualClass(mapCellAt(left, leftX, leftY)) != visualClass(mapCellAt(right, rightX, rightY)) ? 1 : 0;
        }
    }

    // Important point features carry more perceptual weight than one sampled
    // terrain cell. Compare their normalized centroids and counts without
    // depending on IDs or generation order.
    const MapGeneratedFeatureKind pointKinds[] = {
        MapGeneratedFeatureKind::Settlement, MapGeneratedFeatureKind::Landmark, MapGeneratedFeatureKind::MountainRange,
        MapGeneratedFeatureKind::WaterBody};
    int featureDifference = 0;
    for (const MapGeneratedFeatureKind kind : pointKinds)
    {
        const auto summarize = [&](const MapModel &map) {
            int count = 0;
            float x = 0.0f;
            float y = 0.0f;
            for (const MapGeneratedFeature &feature : map.generation.graph.features)
            {
                if (feature.kind != kind || feature.points.empty())
                    continue;
                ++count;
                x += feature.points.front().x / static_cast<float>(std::max(1, map.width));
                y += feature.points.front().y / static_cast<float>(std::max(1, map.height));
            }
            if (count > 0)
            {
                x /= static_cast<float>(count);
                y /= static_cast<float>(count);
            }
            return std::array<float, 3>{static_cast<float>(count), x, y};
        };
        const auto a = summarize(left);
        const auto b = summarize(right);
        featureDifference += std::min(4, static_cast<int>(std::fabs(a[0] - b[0]))) * 2;
        featureDifference += static_cast<int>((std::fabs(a[1] - b[1]) + std::fabs(a[2] - b[2])) * 8.0f);
    }
    const int terrainPercent = different * 100 / (sampleColumns * sampleRows);
    return std::clamp(terrainPercent * 4 / 5 + std::min(100, featureDifference * 4) / 5, 0, 100);
}

std::vector<MapGenerationResult> mapGenSelectDiverseCandidates(std::vector<MapGenerationResult> ranked, int resultCount)
{
    const std::size_t wanted = static_cast<std::size_t>(std::max(1, resultCount));
    if (ranked.size() <= wanted)
        return ranked;
    std::vector<MapGenerationResult> selected;
    selected.reserve(wanted);
    selected.push_back(std::move(ranked.front()));
    ranked.erase(ranked.begin());
    while (selected.size() < wanted && !ranked.empty())
    {
        std::size_t bestIndex = 0;
        int bestUtility = std::numeric_limits<int>::min();
        for (std::size_t index = 0; index < ranked.size(); ++index)
        {
            int minimumDistance = 100;
            for (const MapGenerationResult &chosen : selected)
                minimumDistance =
                    std::min(minimumDistance, mapGenCompositionDistancePercent(ranked[index].map, chosen.map));
            // Quality remains relevant, while perceptible layout difference is
            // weighted strongly enough that three near-clones are not shown.
            const int utility = ranked[index].quality.score + minimumDistance * 3;
            if (utility > bestUtility)
            {
                bestUtility = utility;
                bestIndex = index;
            }
        }
        selected.push_back(std::move(ranked[bestIndex]));
        ranked.erase(ranked.begin() + static_cast<std::ptrdiff_t>(bestIndex));
    }
    return selected;
}

MapGenerationBatchResult generateSemanticMapCandidateBatch(const MapGenerationOptions &options, int resultCount)
{
    MapGenerationBatchResult batch;
    if (mapGenCancellationRequested(options))
    {
        batch.failure = MapGenerationBatchFailureCode::Cancelled;
        return batch;
    }
    MapGenerationOptions effectiveOptions = options;
    mapGenApplyFactsToOptions(effectiveOptions);
    batch.requestValidation = validateMapGenerationRequest(effectiveOptions);
    if (!batch.requestValidation.eligible)
    {
        batch.failure = MapGenerationBatchFailureCode::InvalidRequest;
        return batch;
    }
    const int attempts = std::clamp(effectiveOptions.qualityAttempts, 1, 48);
    batch.attemptsEvaluated = 0;
    batch.candidates.reserve(static_cast<std::size_t>(std::min(attempts, std::max(1, resultCount))));
    batch.rejectedCandidates.reserve(static_cast<std::size_t>(attempts));
    const int totalSteps = attempts + 2;
    if (attempts > 1)
        mapGenReportProgress(effectiveOptions, 1, totalSteps, "Preparing quality search");
    int hardEligibleCandidates = 0;
    for (int candidateIndex = 0; candidateIndex < attempts; ++candidateIndex)
    {
        if (mapGenCancellationRequested(options))
        {
            batch.failure = MapGenerationBatchFailureCode::Cancelled;
            break;
        }
        MapGenerationOptions candidateOptions = effectiveOptions;
        candidateOptions.seed = mapGenCandidateSeed(effectiveOptions.seed, candidateIndex);
        // Keep the initial settlement layout family while searching variants.
        // Otherwise structural ranking can replace every requested layout with
        // a coastal town. The adjusted seed remains independently replayable.
        if (effectiveOptions.generatorVersion >= 11 &&
            (effectiveOptions.creationIntent == MapCreationIntent::Settlement ||
             effectiveOptions.purpose == MapGenerationPurpose::SettlementEnvirons))
        {
            const std::uint64_t family = (effectiveOptions.seed / 17u) % 6u;
            std::uint64_t adjusted =
                (std::uint64_t(candidateOptions.seed) / 102u) * 102u + family * 17u + candidateOptions.seed % 17u;
            if (adjusted > std::numeric_limits<std::uint32_t>::max())
                adjusted -= 102u;
            candidateOptions.seed = static_cast<std::uint32_t>(adjusted);
        }
        candidateOptions.qualityAttempts = 1;
        if (attempts > 1)
            candidateOptions.progressCallback = {};
        MapGenerationResult candidate = mapGenGenerateCandidate(candidateOptions);
        ++batch.attemptsEvaluated;
        if (mapGenCancellationRequested(options))
        {
            batch.failure = MapGenerationBatchFailureCode::Cancelled;
            break;
        }
        mapGenApplyExcludedElements(candidate, candidateOptions);
        mapGenRestorePreservedFeatures(candidate.map, candidateOptions);
        candidate.quality = analyzeMapGenerationQuality(candidate.map);
        candidate.quality.candidatesEvaluated = attempts;
        candidate.quality.selectedCandidateIndex = candidateIndex;
        candidate.quality.selectedSeed = candidateOptions.seed;
        candidate.validation = validateMapGeneration(candidate.map, candidate.quality);
        if (candidate.validation.eligible)
            ++hardEligibleCandidates;
        if (candidate.validation.eligible && mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality))
            batch.candidates.push_back(std::move(candidate));
        else
        {
            if (candidate.validation.eligible)
                candidate.validation.warnings.push_back(
                    {"quality.minimum_not_met", MapGenerationIssueSeverity::Warning,
                     "The generated map did not reach the minimum quality floor in every relevant category.",
                     "Try another deterministic candidate or adjust the recipe."});
            candidate.quality.score = 0;
            batch.rejectedCandidates.push_back(std::move(candidate));
        }
        if (attempts > 1)
            mapGenReportProgress(effectiveOptions, candidateIndex + 2, totalSteps,
                                 "Evaluating candidate " + std::to_string(candidateIndex + 1) + "/" +
                                     std::to_string(attempts));
    }
    std::stable_sort(batch.candidates.begin(), batch.candidates.end(),
                     [](const MapGenerationResult &left, const MapGenerationResult &right) {
                         return left.quality.score > right.quality.score;
                     });
    if (batch.failure == MapGenerationBatchFailureCode::Cancelled)
    {
        // A cancelled request must never leak a partially evaluated design
        // into a product picker, even if earlier deterministic attempts passed.
        batch.candidates.clear();
        batch.rejectedCandidates.clear();
        return batch;
    }
    batch.candidates = mapGenSelectDiverseCandidates(std::move(batch.candidates), resultCount);
    batch.recommendedIndex = batch.candidates.empty() ? -1 : 0;
    for (MapGenerationResult &candidate : batch.candidates)
    {
        if (candidate.quality.score < 70)
            candidate.warnings.push_back(
                "Generated layout has a low playability score; adjust the recipe or try another base seed.");
    }
    if (batch.failure != MapGenerationBatchFailureCode::Cancelled && batch.candidates.empty())
        batch.failure = hardEligibleCandidates == 0 ? MapGenerationBatchFailureCode::NoEligibleCandidate
                                                    : MapGenerationBatchFailureCode::NoCandidateMetQualityFloor;
    if (attempts > 1 && batch.failure != MapGenerationBatchFailureCode::Cancelled)
        mapGenReportProgress(effectiveOptions, totalSteps, totalSteps, "Complete");
    return batch;
}

std::vector<MapGenerationResult> generateSemanticMapCandidates(const MapGenerationOptions &options, int resultCount)
{
    return generateSemanticMapCandidateBatch(options, resultCount).candidates;
}

MapGenerationResult generateSemanticMap(const MapGenerationOptions &options)
{
    MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, 1);
    if (!batch.candidates.empty())
        return std::move(batch.candidates.front());
    // Compatibility path for low-level tests and legacy callers that require a
    // diagnostic map even when no candidate is eligible. Product candidate
    // pickers use the batch/vector API and therefore never receive this map as
    // a selectable design.
    if (!batch.rejectedCandidates.empty())
        return std::move(batch.rejectedCandidates.front());
    return {};
}
