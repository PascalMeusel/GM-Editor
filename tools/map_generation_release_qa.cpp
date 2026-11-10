#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapQualityAssurance.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
struct Config
{
    std::string tier = "pr";
    int mapCount = -1;
    std::filesystem::path output = "build/map-generation-qa";
    std::uint32_t startSeed = 700000u;
    int shardIndex = 0;
    int shardCount = 1;
    int determinismRate = -1;
    int qualityAttempts = -1;
    std::string caseId;
    bool reportOnly = false;
};

struct Generated
{
    MapModel map;
    MapGenerationQualityReport quality;
    MapGenerationValidationReport validation;
    MapCreationRequest request;
    std::vector<std::string> readinessIssues;
    MapGenerationBatchFailureCode batchFailure = MapGenerationBatchFailureCode::None;
    int attemptsEvaluated = 0;
    std::string entryPoint = "MapCreationRequest/MapGenerationBatchResult";
    std::string rejectionDetails;
    bool generated = false;
};

struct CaseResult
{
    std::string id;
    std::string group;
    std::string band;
    std::uint32_t seed = 0;
    std::uint64_t graphHash = 0;
    std::uint64_t rasterHash = 0;
    std::uint64_t outputHash = 0;
    int score = 0;
    int generatorVersion = 0;
    int recipeVersion = 0;
    int styleVersion = 0;
    int attemptsEvaluated = 0;
    bool validationEligible = false;
    std::string styleId;
    std::string styleSnapshotId;
    std::string entryPoint;
    std::string batchFailure;
    long long elapsedMicroseconds = 0;
    std::vector<GenerationInvariantResult> invariants;
};

constexpr int kV5GeneratorVersion = 5;
constexpr int kV5RecipeVersion = 3;
constexpr int kV5StyleVersion = 3;
constexpr const char *kV5StyleId = "fantasy_neutral";
constexpr const char *kV5ProductContract = "MapCreationRequest/MapGenerationBatchResult";

std::string escapeJson(const std::string &value)
{
    std::ostringstream out;
    for (unsigned char c : value)
    {
        switch (c)
        {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            else out << c;
        }
    }
    return out.str();
}

void hashBytes(std::uint64_t &hash, const void *data, std::size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t index = 0; index < size; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
}

template <typename T>
void hashValue(std::uint64_t &hash, const T &value) { hashBytes(hash, &value, sizeof(value)); }
void hashString(std::uint64_t &hash, const std::string &value) { hashBytes(hash, value.data(), value.size()); const unsigned char separator = 0xff; hashBytes(hash, &separator, 1); }

std::uint64_t rasterHash(const MapModel &map)
{
    std::uint64_t hash = 1469598103934665603ull;
    hashValue(hash, map.width); hashValue(hash, map.height); hashValue(hash, map.scope); hashValue(hash, map.atlasLevel);
    for (const MapCellModel &cell : map.cells)
    {
        hashValue(hash, cell.kind); hashValue(hash, cell.wayKind); hashValue(hash, cell.rotationQuarterTurns);
        hashValue(hash, cell.rotationLocked); hashValue(hash, cell.symbolVariantMode); hashValue(hash, cell.movementCost);
        hashValue(hash, cell.difficultTerrain); hashValue(hash, cell.blocksLineOfSight); hashValue(hash, cell.elevation);
        hashValue(hash, cell.movementBlockedMask); hashValue(hash, cell.lineOfSightBlockedMask);
    }
    return hash;
}

std::uint64_t graphHash(const MapModel &map)
{
    std::uint64_t hash = 1469598103934665603ull;
    hashValue(hash, map.generation.graph.schemaVersion);
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        hashString(hash, feature.id); hashValue(hash, feature.kind); hashString(hash, feature.name); hashString(hash, feature.materialId);
        hashValue(hash, feature.width); hashValue(hash, feature.closed); hashValue(hash, feature.locked);
        hashValue(hash, feature.discovered); hashValue(hash, feature.gmOnly);
        for (const MapFeaturePoint &point : feature.points) { hashValue(hash, point.x); hashValue(hash, point.y); }
    }
    return hash;
}

std::uint64_t outputHash(const MapModel &map)
{
    std::uint64_t hash = rasterHash(map);
    const std::uint64_t graph = graphHash(map);
    hashValue(hash, graph);
    hashValue(hash, map.creationIntent);
    hashValue(hash, map.sizePreset);
    hashValue(hash, map.mapKind);
    hashValue(hash, map.gridType);
    hashValue(hash, map.cellSize);
    hashValue(hash, map.generation.recipe.generatorVersion);
    hashValue(hash, map.generation.recipe.recipeVersion);
    hashString(hash, map.generation.recipe.styleId);
    hashValue(hash, map.generation.recipe.styleVersion);
    hashString(hash, map.generation.recipe.styleSnapshotId);
    hashValue(hash, map.generation.recipe.quality);
    hashString(hash, map.parentMapId);
    hashString(hash, map.atlasPlacement.parentId);
    hashValue(hash, map.atlasPlacement.parentRevision);
    hashValue(hash, map.atlasPlacement.sourceRevision);
    return hash;
}

const char *batchFailureName(MapGenerationBatchFailureCode failure)
{
    switch (failure)
    {
    case MapGenerationBatchFailureCode::None: return "none";
    case MapGenerationBatchFailureCode::InvalidRequest: return "invalid_request";
    case MapGenerationBatchFailureCode::NoEligibleCandidate: return "no_eligible_candidate";
    case MapGenerationBatchFailureCode::NoCandidateMetQualityFloor: return "quality_floor_not_met";
    case MapGenerationBatchFailureCode::Cancelled: return "cancelled";
    }
    return "unknown";
}

MapGenerationQualityLevel qualityLevelForAttemptBudget(int attempts)
{
    if (attempts >= 48)
        return MapGenerationQualityLevel::Polished;
    if (attempts <= 12)
        return MapGenerationQualityLevel::Draft;
    return MapGenerationQualityLevel::Balanced;
}

const char *groupName(int group)
{
    static const char *names[] = {"world", "region_travel", "city_settlement", "tactical_outdoor", "dungeon_cave"};
    return names[std::clamp(group, 0, 4)];
}

std::string bandName(int localIndex, int groupTarget)
{
    const int position = localIndex * 10000 / std::max(1, groupTarget);
    if (position < 6000) return "broad";
    if (position < 8000) return "stress";
    if (position < 9000) return "constraints";
    return "regression";
}

MapCreationIntent intentForCase(int group, int localIndex)
{
    switch (group)
    {
    case 0: return MapCreationIntent::WorldOverview;
    case 1: return localIndex % 2 == 0 ? MapCreationIntent::RegionExploration : MapCreationIntent::TravelScene;
    case 2: return MapCreationIntent::Settlement;
    case 3: return MapCreationIntent::TacticalEncounter;
    default: return localIndex % 2 == 0 ? MapCreationIntent::Dungeon : MapCreationIntent::Cave;
    }
}

MapCreationRequest baseRequest(int group, int localIndex, std::uint32_t seed, int qualityAttempts)
{
    MapCreationRequest request;
    request.intent = intentForCase(group, localIndex);
    switch (localIndex % 4)
    {
    case 0: request.sizePreset = MapSizePreset::Small; break;
    case 1: request.sizePreset = MapSizePreset::Standard; break;
    case 2: request.sizePreset = MapSizePreset::Large; break;
    default:
        request.sizePreset = MapSizePreset::Custom;
        request.dimensions = group == 3 || group == 4
                                 ? MapDimensions{24 + localIndex % 41, 18 + (localIndex / 3) % 37}
                                 : MapDimensions{40 + static_cast<int>(seed % 41u),
                                                 30 + static_cast<int>((seed / 7u) % 31u)};
        break;
    }
    request.seed = seed;
    request.generatorVersion = kV5GeneratorVersion;
    request.recipeVersion = kV5RecipeVersion;
    request.styleId = kV5StyleId;
    request.styleVersion = kV5StyleVersion;
    request.styleSnapshotId = mapStyleCurrentProductionSnapshotId();
    request.quality = qualityLevelForAttemptBudget(qualityAttempts);
    request.contentLanguage = localIndex % 2 == 0 ? "de" : "en";
    request.nameCulture = localIndex % 3 == 0 ? "germanic_central" : "neutral_fantasy";
    request.allowAdditionalNations = false;

    // Prefer facts exercise non-blocking, typed V5 evaluation in every band.
    MapGenerationFact preference;
    preference.id = "qa_preferred_landscape";
    preference.type = localIndex % 2 == 0 ? MapGenerationFactType::Forest : MapGenerationFactType::MountainRange;
    preference.strength = MapGenerationFactStrength::Prefer;
    preference.label = preference.type == MapGenerationFactType::Forest ? "forest" : "mountain range";
    preference.value = preference.label;
    preference.sourceText = "QA typed preference";
    preference.confidence = 1.0f;
    preference.confirmed = true;
    request.facts.push_back(std::move(preference));
    return request;
}

void addConstraintBand(MapCreationRequest &request, int group, int localIndex)
{
    const auto addFact = [&](std::string id,
                             MapGenerationFactType type,
                             MapGenerationFactStrength strength,
                             std::string label,
                             std::string value = {}) {
        MapGenerationFact fact;
        fact.id = std::move(id);
        fact.type = type;
        fact.strength = strength;
        fact.label = std::move(label);
        fact.value = value.empty() ? fact.label : std::move(value);
        fact.sourceText = "QA typed constraint band";
        fact.confidence = 1.0f;
        fact.confirmed = true;
        request.facts.push_back(std::move(fact));
    };

    if (group == 0)
    {
        static constexpr const char *nationNames[] = {"Aster", "Bram", "Cyr"};
        for (int index = 0; index < 3; ++index)
            addFact("qa_nation_" + std::to_string(index), MapGenerationFactType::Nation,
                    MapGenerationFactStrength::Must, nationNames[index]);
        addFact("qa_nation_count", MapGenerationFactType::NationCount,
                MapGenerationFactStrength::Must, "3", "3");

        MapGenerationFact sharedRiver;
        sharedRiver.id = "qa_shared_river";
        sharedRiver.type = MapGenerationFactType::SharedRiver;
        sharedRiver.strength = MapGenerationFactStrength::Must;
        sharedRiver.label = "Shared great river";
        sharedRiver.value = "river";
        sharedRiver.entityRefs = {"Aster", "Bram", "Cyr"};
        sharedRiver.sourceText = "Three named nations must share one great river.";
        sharedRiver.confidence = 1.0f;
        sharedRiver.confirmed = true;
        request.facts.push_back(std::move(sharedRiver));

        addFact("qa_capital", MapGenerationFactType::Capital,
                MapGenerationFactStrength::Must, "QA Capital");
        MapGenerationConstraint capital;
        capital.id = "qa_capital";
        capital.label = "QA Capital";
        capital.featureKind = MapGeneratedFeatureKind::Settlement;
        const MapDimensions dimensions = request.sizePreset == MapSizePreset::Custom
                                             ? request.dimensions
                                             : mapDimensionsForPreset(request.intent, request.sizePreset);
        capital.x = static_cast<float>(dimensions.width) * 0.52f;
        capital.y = static_cast<float>(dimensions.height) * 0.48f;
        capital.required = true;
        capital.locked = true;
        request.constraints.push_back(std::move(capital));
    }
    else
    {
        const bool settlement = group == 2;
        const std::string label = settlement ? "QA Civic Hall" : "QA Required Landmark";
        addFact("qa_required_fact_" + std::to_string(localIndex),
                settlement ? MapGenerationFactType::Settlement : MapGenerationFactType::IncludedElement,
                MapGenerationFactStrength::Must, label);
        // The tactical composer consumes typed required elements, but it does
        // not yet expose arbitrary placement anchors. Other V5 generators do,
        // so exercise the positional constraint contract everywhere it can be
        // represented without manufacturing a false failure in this harness.
        if (group != 3)
        {
            const MapDimensions dimensions = request.sizePreset == MapSizePreset::Custom
                                                 ? request.dimensions
                                                 : mapDimensionsForPreset(request.intent, request.sizePreset);
            MapGenerationConstraint constraint;
            constraint.id = "qa_required_anchor_" + std::to_string(localIndex);
            constraint.label = label;
            constraint.featureKind = settlement ? MapGeneratedFeatureKind::Settlement : MapGeneratedFeatureKind::Landmark;
            constraint.x = static_cast<float>(dimensions.width) * 0.42f;
            constraint.y = static_cast<float>(dimensions.height) * 0.57f;
            constraint.required = true;
            constraint.locked = true;
            request.constraints.push_back(std::move(constraint));
        }
    }

    addFact("qa_excluded_lava", MapGenerationFactType::ExcludedElement,
            MapGenerationFactStrength::Must, "lava");
}

MapGenerationOptions optionsForRequest(const MapCreationRequest &request, int group, int localIndex, int qualityAttempts,
                                       const std::string &band)
{
    MapGenerationOptions options = mapGenerationOptionsFromCreationRequest(request);
    options.qualityAttempts = qualityAttempts;
    options.landscape = static_cast<MapLandscapeProfile>(localIndex % 5);
    options.waterLevel = 12 + static_cast<int>((request.seed / 11u) % 58u);
    options.mountainDensity = 8 + static_cast<int>((request.seed / 13u) % 58u);
    options.forestDensity = 10 + static_cast<int>((request.seed / 17u) % 60u);
    options.routeDensity = 18 + static_cast<int>((request.seed / 19u) % 58u);
    options.tacticalDensity = 12 + static_cast<int>((request.seed / 23u) % 50u);
    options.requestedSettlements = group == 2 ? 1 : 2 + static_cast<int>(request.seed % 4u);
    options.requestedLandmarks = 2 + static_cast<int>((request.seed / 5u) % 5u);
    options.edgeConnectionCount = 2 + static_cast<int>(request.seed % 3u);
    options.roadNetwork = MapRoadNetworkMode::Connected;
    options.caveNetwork = request.intent == MapCreationIntent::Cave;
    if (group == 0 && band == "constraints")
    {
        options.waterLevel = std::max(options.waterLevel, 38);
        options.requestedSettlements = 4;
        options.edgeConnectionCount = 4;
    }
    if (group == 2)
        options.forestDensity = std::min(options.forestDensity, 22);
    if (band == "stress")
    {
        options.waterLevel = localIndex % 2 == 0 ? 68 : 8;
        options.mountainDensity = localIndex % 3 == 0 ? 68 : 10;
        options.forestDensity = localIndex % 5 == 0 ? 70 : 12;
        options.edgeConnectionCount = 4;
    }
    mapGenApplyFactsToOptions(options);
    return options;
}

Generated generateCase(int group, int localIndex, std::uint32_t seed, int qualityAttempts, const std::string &band)
{
    Generated generated;
    generated.request = baseRequest(group, localIndex, seed, qualityAttempts);
    if (band == "stress" && generated.request.sizePreset != MapSizePreset::Custom)
        generated.request.sizePreset = MapSizePreset::Large;
    if (band == "constraints")
        addConstraintBand(generated.request, group, localIndex);

    MapGenerationOptions options = optionsForRequest(generated.request, group, localIndex, qualityAttempts, band);
    if (group == 3)
    {
        generated.entryPoint = "MapCreationRequest/TacticalMapGenerator(V5Batch)";
        TacticalMapRecipe recipe;
        recipe.mapTemplate = static_cast<TacticalMapTemplate>(localIndex % 8);
        recipe.width = options.width;
        recipe.height = options.height;
        recipe.seed = seed;
        recipe.quality = generated.request.quality;
        recipe.coverDensity = 16 + localIndex % 42;
        recipe.minimumCreatureFootprint = 2;
        recipe.objectiveCount = 1 + localIndex % 3;
        recipe.requiredElements = options.requiredElements;
        MapModel source;
        source.metadata.id = "qa_source_region";
        source.atlasLevel = MapLevel::Region;
        source.revision = 4;
        auto candidates = TacticalMapGenerator::generate(recipe, &source, 1);
        if (candidates.empty())
        {
            generated.batchFailure = MapGenerationBatchFailureCode::NoEligibleCandidate;
            return generated;
        }
        generated.map = std::move(candidates.front().map);
        generated.quality = candidates.front().quality;
        generated.readinessIssues = candidates.front().readinessIssues;
        generated.map.generation.recipe.facts = generated.request.facts;
        generated.map.generation.recipe.constraints = generated.request.constraints;
        generated.map.generation.recipe.excludedElements = options.excludedElements;
        generated.validation = validateMapGeneration(generated.map, generated.quality);
        generated.attemptsEvaluated = generated.quality.candidatesEvaluated;
        generated.batchFailure = generated.validation.eligible
                                     ? MapGenerationBatchFailureCode::None
                                     : MapGenerationBatchFailureCode::NoEligibleCandidate;
        generated.generated = generated.validation.eligible &&
                              mapGenQualityMeetsCandidateFloor(generated.map, generated.quality) &&
                              generated.readinessIssues.empty();
        return generated;
    }

    const MapGenerationBatchResult batch = generateSemanticMapCandidateBatch(options, 1);
    generated.batchFailure = batch.failure;
    generated.attemptsEvaluated = batch.attemptsEvaluated;
    if (batch.candidates.empty())
    {
        const MapGenerationResult *bestRejected = nullptr;
        for (const MapGenerationResult &candidate : batch.rejectedCandidates)
        {
            if (!bestRejected || candidate.quality.score > bestRejected->quality.score)
                bestRejected = &candidate;
        }
        if (bestRejected)
        {
            const MapGenerationQualityReport &quality = bestRejected->quality;
            std::ostringstream details;
            details << "best_rejected_seed=" << quality.selectedSeed
                    << ",score=" << quality.score
                    << ",critical=" << quality.criticalIssueCount
                    << ",eligible=" << (bestRejected->validation.eligible ? "true" : "false")
                    << ",categories=" << quality.traversableConnectivityPercent << '/'
                    << quality.wayConnectivityPercent << '/'
                    << quality.structureAccessPercent << '/'
                    << quality.coherentTerrainPercent << '/'
                    << quality.roadEfficiencyPercent << '/'
                    << quality.hydrologyPercent << '/'
                    << quality.bridgeValidityPercent << '/'
                    << quality.settlementSuitabilityPercent << '/'
                    << quality.compositionPercent << '/'
                    << quality.visualHierarchyPercent << '/'
                    << quality.scaleCorrectnessPercent << '/'
                    << quality.assetCoveragePercent;
            if (!bestRejected->validation.blockers.empty())
            {
                details << ",blockers=";
                for (std::size_t index = 0; index < bestRejected->validation.blockers.size(); ++index)
                {
                    if (index > 0) details << '|';
                    details << bestRejected->validation.blockers[index].code;
                }
            }
            generated.rejectionDetails = details.str();
        }
        return generated;
    }
    const MapGenerationResult &candidate = batch.candidates.front();
    generated.map = candidate.map;
    generated.quality = candidate.quality;
    generated.validation = candidate.validation;
    generated.generated = batch.succeeded() && generated.validation.eligible &&
                          mapGenQualityMeetsCandidateFloor(generated.map, generated.quality);
    return generated;
}

void addInvariant(CaseResult &result, const std::string &code, bool passed, bool critical, const std::string &details = {})
{
    result.invariants.push_back({code, passed, critical, details});
}

bool containsInsensitive(std::string value, std::string needle)
{
    const auto normalize = [](std::string text) {
        bool previousSeparator = true;
        std::string normalized;
        normalized.reserve(text.size());
        for (unsigned char character : text)
        {
            if (std::isalnum(character))
            {
                normalized.push_back(static_cast<char>(std::tolower(character)));
                previousSeparator = false;
            }
            else if (!previousSeparator)
            {
                normalized.push_back(' ');
                previousSeparator = true;
            }
        }
        if (!normalized.empty() && normalized.back() == ' ')
            normalized.pop_back();
        return normalized;
    };
    value = normalize(std::move(value));
    needle = normalize(std::move(needle));
    if (needle.empty())
        return false;
    return value.find(needle) != std::string::npos;
}

bool generatedMapContainsElement(const MapModel &map, const std::string &element)
{
    if (element.empty())
        return false;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (containsInsensitive(feature.name, element) || containsInsensitive(feature.materialId, element))
            return true;
    }
    for (const MapCellModel &cell : map.cells)
    {
        if (containsInsensitive(mapTileKindLabel(cell.kind), element) ||
            (cell.wayKind != MapWayKind::None && containsInsensitive(mapWayKindLabel(cell.wayKind), element)))
            return true;
    }
    return false;
}

void validateGenerated(CaseResult &result, const Generated &generated, int group, const std::string &band)
{
    if (!generated.generated)
    {
        addInvariant(result, "generation.completed", false, true,
                     std::string("Generator returned no candidate; batch_failure=") +
                         batchFailureName(generated.batchFailure) +
                         (generated.rejectionDetails.empty()
                              ? std::string()
                              : std::string("; ") + generated.rejectionDetails));
        return;
    }
    const MapModel &map = generated.map;
    const MapGenerationRecipe &recipe = map.generation.recipe;
    addInvariant(result, "batch.succeeded",
                 generated.batchFailure == MapGenerationBatchFailureCode::None, true,
                 batchFailureName(generated.batchFailure));
    addInvariant(result, "validation.eligible",
                 generated.validation.eligible && generated.validation.blockers.empty() &&
                     generated.validation.criticalIssueCount == 0,
                 true, std::to_string(generated.validation.criticalIssueCount));
    addInvariant(result, "contract.product_entry_point",
                 generated.entryPoint.rfind("MapCreationRequest/", 0) == 0, true,
                 generated.entryPoint);
    addInvariant(result, "contract.generator_version",
                 generated.request.generatorVersion == kV5GeneratorVersion &&
                     recipe.generatorVersion == kV5GeneratorVersion,
                 true, std::to_string(generated.request.generatorVersion) + "/" +
                           std::to_string(recipe.generatorVersion));
    addInvariant(result, "contract.recipe_version",
                 generated.request.recipeVersion == kV5RecipeVersion &&
                     recipe.recipeVersion == kV5RecipeVersion,
                 true, std::to_string(generated.request.recipeVersion) + "/" +
                           std::to_string(recipe.recipeVersion));
    addInvariant(result, "contract.style_id",
                 generated.request.styleId == kV5StyleId && recipe.styleId == generated.request.styleId,
                 true, generated.request.styleId + "/" + recipe.styleId);
    addInvariant(result, "contract.style_version",
                 generated.request.styleVersion == kV5StyleVersion &&
                     recipe.styleVersion == generated.request.styleVersion,
                 true, std::to_string(generated.request.styleVersion) + "/" +
                           std::to_string(recipe.styleVersion));
    const std::string &productionSnapshot = mapStyleCurrentProductionSnapshotId();
    addInvariant(result, "contract.production_snapshot",
                 !productionSnapshot.empty() && generated.request.styleSnapshotId == productionSnapshot &&
                     recipe.styleSnapshotId == productionSnapshot,
                 true, generated.request.styleSnapshotId + "/" + recipe.styleSnapshotId +
                           "/expected=" + productionSnapshot);
    addInvariant(result, "contract.creation_intent",
                 map.creationIntent == generated.request.intent &&
                     recipe.creationIntent == generated.request.intent,
                 true);
    addInvariant(result, "contract.fixed_attempt_budget",
                 generated.attemptsEvaluated == generated.quality.candidatesEvaluated &&
                     generated.attemptsEvaluated > 0 &&
                     (generated.attemptsEvaluated == 12 || generated.attemptsEvaluated == 24 ||
                      generated.attemptsEvaluated == 48),
                 true, std::to_string(generated.attemptsEvaluated) + "/quality=" +
                           std::to_string(generated.quality.candidatesEvaluated));

    bool allMustFactsFulfilled = true;
    bool allExcludedFactsFulfilled = true;
    std::string unfulfilledFacts;
    for (const MapGenerationFact &fact : generated.request.facts)
    {
        if (fact.strength != MapGenerationFactStrength::Must)
            continue;
        const auto found = std::find_if(
            generated.validation.facts.begin(), generated.validation.facts.end(),
            [&](const MapGenerationFactValidation &validation) { return validation.factId == fact.id; });
        const bool fulfilled = found != generated.validation.facts.end() &&
                               found->fulfillment == MapGenerationFactFulfillment::Fulfilled;
        allMustFactsFulfilled = allMustFactsFulfilled && fulfilled;
        if (fact.type == MapGenerationFactType::ExcludedElement)
            allExcludedFactsFulfilled = allExcludedFactsFulfilled && fulfilled;
        if (!fulfilled)
        {
            if (!unfulfilledFacts.empty())
                unfulfilledFacts += ',';
            unfulfilledFacts += fact.id;
        }
    }
    addInvariant(result, "facts.must_fulfilled", allMustFactsFulfilled, true, unfulfilledFacts);
    addInvariant(result, "facts.excluded_fulfilled", allExcludedFactsFulfilled, true, unfulfilledFacts);
    addInvariant(result, "dimensions.valid", map.width > 0 && map.height > 0 && map.cells.size() == static_cast<std::size_t>(map.width * map.height), true);
    std::string criticalCodes;
    for (const MapGenerationIssue &issue : generated.quality.issues)
    {
        if (issue.severity != MapGenerationIssueSeverity::Critical) continue;
        if (!criticalCodes.empty()) criticalCodes += ',';
        criticalCodes += issue.code;
    }
    addInvariant(result, "quality.no_critical", generated.quality.criticalIssueCount == 0, true,
                 std::to_string(generated.quality.criticalIssueCount) + (criticalCodes.empty() ? "" : ":" + criticalCodes));
    addInvariant(result, "quality.score", generated.quality.score >= 85, true, std::to_string(generated.quality.score));
    // Separate continents and islands are valid at world scale. Local,
    // tactical and dungeon maps still require a connected traversable surface.
    if (group != 0)
        addInvariant(result, "quality.traversable", generated.quality.traversableConnectivityPercent >= 80, true, std::to_string(generated.quality.traversableConnectivityPercent));
    if (group != 4)
        addInvariant(result, "quality.coherent_terrain", generated.quality.coherentTerrainPercent >= 80, false, std::to_string(generated.quality.coherentTerrainPercent));
    if (group <= 2)
    {
        addInvariant(result, "quality.way_connectivity", generated.quality.wayConnectivityPercent >= 80, true, std::to_string(generated.quality.wayConnectivityPercent));
        addInvariant(result, "quality.hydrology", generated.quality.hydrologyPercent >= 80, true, std::to_string(generated.quality.hydrologyPercent));
        addInvariant(result, "quality.bridges", generated.quality.bridgeValidityPercent == 100, true, std::to_string(generated.quality.bridgeValidityPercent));
        addInvariant(result, "quality.structure_access", generated.quality.structureAccessPercent >= 80, true, std::to_string(generated.quality.structureAccessPercent));
        addInvariant(result, "quality.road_efficiency", generated.quality.roadEfficiencyPercent >= 80, true, std::to_string(generated.quality.roadEfficiencyPercent));
        addInvariant(result, "quality.settlement_suitability", generated.quality.settlementSuitabilityPercent >= 80, true, std::to_string(generated.quality.settlementSuitabilityPercent));
        addInvariant(result, "quality.composition", generated.quality.compositionPercent >= 80, true, std::to_string(generated.quality.compositionPercent));
    }
    if (map.generation.continuousRendering)
    {
        addInvariant(result, "quality.visual_hierarchy", generated.quality.visualHierarchyPercent >= 80, true, std::to_string(generated.quality.visualHierarchyPercent));
        addInvariant(result, "quality.scale_correctness", generated.quality.scaleCorrectnessPercent >= 80, true, std::to_string(generated.quality.scaleCorrectnessPercent));
        addInvariant(result, "quality.production_assets", generated.quality.assetCoveragePercent >= 80, true, std::to_string(generated.quality.assetCoveragePercent));
    }
    if (group == 3) addInvariant(result, "tactical.readiness", generated.readinessIssues.empty(), true, generated.readinessIssues.empty() ? "" : generated.readinessIssues.front());
    std::unordered_set<std::string> ids;
    bool idsValid = true, pointsValid = true, buildingWayOverlap = false, riversFlowDownhill = true;
    std::string invalidPointDetails;
    std::string uphillRiverDetails;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.id.empty() || !ids.insert(feature.id).second) idsValid = false;
        int previousElevation = std::numeric_limits<int>::max();
        for (const MapFeaturePoint &point : feature.points)
        {
            if (point.x < -0.01f || point.y < -0.01f || point.x > map.width + 0.01f || point.y > map.height + 0.01f)
            {
                pointsValid = false;
                if (invalidPointDetails.empty())
                {
                    std::ostringstream details;
                    details << feature.id << "@(" << point.x << ',' << point.y << ") bounds=" << map.width << 'x' << map.height;
                    invalidPointDetails = details.str();
                }
            }
            if (feature.kind == MapGeneratedFeatureKind::River)
            {
                const int x = std::clamp(static_cast<int>(std::floor(point.x)), 0, std::max(0, map.width - 1));
                const int y = std::clamp(static_cast<int>(std::floor(point.y)), 0, std::max(0, map.height - 1));
                const MapCellModel *cell = mapCellAt(map, x, y);
                const int elevation = cell ? cell->elevation : previousElevation;
                if (elevation > previousElevation)
                {
                    riversFlowDownhill = false;
                    if (uphillRiverDetails.empty())
                        uphillRiverDetails = feature.id + "@(" + std::to_string(x) + "," + std::to_string(y) + ") " +
                                             std::to_string(previousElevation) + "->" + std::to_string(elevation);
                }
                previousElevation = elevation;
            }
        }
    }
    for (const MapCellModel &cell : map.cells)
        if (cell.kind == MapTileKind::Building && cell.wayKind != MapWayKind::None) buildingWayOverlap = true;
    addInvariant(result, "feature_ids.stable_unique", idsValid, true);
    addInvariant(result, "feature_points.in_bounds", pointsValid, true, invalidPointDetails);
    if (group <= 2)
        addInvariant(result, "hydrology.never_uphill", riversFlowDownhill, true, uphillRiverDetails);
    addInvariant(result, "placement.no_road_under_building", !buildingWayOverlap, true);
    if (band == "constraints")
    {
        bool requiredFound = !map.generation.recipe.requiredElements.empty() || !map.generation.recipe.constraints.empty();
        std::string missingRequired;
        auto recordMissingRequired = [&](const std::string &value) {
            if (!missingRequired.empty())
                missingRequired += ',';
            missingRequired += value;
        };
        for (const std::string &label : map.generation.recipe.requiredElements)
        {
            const bool found = std::any_of(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                           [&](const MapGeneratedFeature &feature){ return feature.name == label; });
            requiredFound = requiredFound && found;
            if (!found)
                recordMissingRequired("label:" + label);
        }
        for (const MapGenerationConstraint &constraint : map.generation.recipe.constraints)
            if (constraint.required)
            {
                const bool found = std::any_of(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                               [&](const MapGeneratedFeature &feature){
                                                   return feature.id == constraint.id ||
                                                          (!constraint.label.empty() && feature.name == constraint.label);
                                               });
                requiredFound = requiredFound && found;
                if (!found)
                    recordMissingRequired("constraint:" + constraint.id);
            }
        addInvariant(result, "constraints.required_present", requiredFound, true, missingRequired);
        std::string excludedMatches;
        for (const std::string &element : map.generation.recipe.excludedElements)
        {
            if (!generatedMapContainsElement(map, element))
                continue;
            if (!excludedMatches.empty())
                excludedMatches += ',';
            excludedMatches += element;
        }
        addInvariant(result, "constraints.excluded_absent", excludedMatches.empty(), true, excludedMatches);
    }
    if (group == 0)
    {
        int nationCount = 0;
        for (const MapGeneratedFeature &feature : map.generation.graph.features) if (feature.kind == MapGeneratedFeatureKind::TerrainRegion && (feature.name == "Aster" || feature.name == "Bram" || feature.name == "Cyr")) ++nationCount;
        if (band == "constraints") addInvariant(result, "world.named_nations", nationCount == 3, true, std::to_string(nationCount));
    }
    if (group == 3) addInvariant(result, "atlas.parent_preserved", map.parentMapId == "qa_source_region", true, map.parentMapId);
}

bool casePassed(const CaseResult &result)
{
    return std::all_of(result.invariants.begin(), result.invariants.end(), [](const GenerationInvariantResult &item){ return item.passed; });
}

std::string failureSignature(const CaseResult &result)
{
    std::vector<std::string> codes;
    for (const auto &item : result.invariants) if (!item.passed) codes.push_back(item.code);
    std::sort(codes.begin(), codes.end());
    std::ostringstream out;
    for (std::size_t index = 0; index < codes.size(); ++index) { if (index) out << '+'; out << codes[index]; }
    return out.str();
}

void writeCase(std::ostream &out, const CaseResult &result)
{
    out << "{\"schema_version\":2,\"id\":\"" << escapeJson(result.id) << "\",\"group\":\"" << result.group
        << "\",\"band\":\"" << result.band << "\",\"seed\":" << result.seed << ",\"graph_hash\":\"" << std::hex << result.graphHash
        << "\",\"raster_hash\":\"" << result.rasterHash << "\",\"output_hash\":\"" << result.outputHash << std::dec
        << "\",\"generator_version\":" << result.generatorVersion
        << ",\"recipe_version\":" << result.recipeVersion
        << ",\"style_id\":\"" << escapeJson(result.styleId)
        << "\",\"style_version\":" << result.styleVersion
        << ",\"style_snapshot_id\":\"" << escapeJson(result.styleSnapshotId)
        << "\",\"entry_point\":\"" << escapeJson(result.entryPoint)
        << "\",\"attempts_evaluated\":" << result.attemptsEvaluated
        << ",\"validation_eligible\":" << (result.validationEligible ? "true" : "false")
        << ",\"batch_failure\":\"" << escapeJson(result.batchFailure)
        << "\",\"score\":" << result.score << ",\"elapsed_us\":" << result.elapsedMicroseconds
        << ",\"passed\":" << (casePassed(result) ? "true" : "false") << ",\"invariants\":[";
    for (std::size_t index = 0; index < result.invariants.size(); ++index)
    {
        if (index) out << ',';
        const auto &item = result.invariants[index];
        out << "{\"code\":\"" << escapeJson(item.code) << "\",\"passed\":" << (item.passed ? "true" : "false") << ",\"critical\":" << (item.critical ? "true" : "false") << ",\"details\":\"" << escapeJson(item.details) << "\"}";
    }
    out << "]}\n";
}

Config parseArgs(int argc, char **argv)
{
    Config config;
    for (int index = 1; index < argc; ++index)
    {
        const std::string arg = argv[index];
        auto value = [&]() -> std::string { if (index + 1 >= argc) throw std::runtime_error("Missing value for " + arg); return argv[++index]; };
        if (arg == "--tier") config.tier = value();
        else if (arg == "--maps") config.mapCount = std::stoi(value());
        else if (arg == "--out") config.output = value();
        else if (arg == "--start-seed") config.startSeed = static_cast<std::uint32_t>(std::stoul(value()));
        else if (arg == "--shard-index") config.shardIndex = std::stoi(value());
        else if (arg == "--shard-count") config.shardCount = std::stoi(value());
        else if (arg == "--determinism-rate") config.determinismRate = std::stoi(value());
        else if (arg == "--quality-attempts") config.qualityAttempts = std::stoi(value());
        else if (arg == "--case-id") config.caseId = value();
        else if (arg == "--report-only") config.reportOnly = true;
        else if (arg == "--help")
        {
            std::cout << "map_generation_release_qa --tier pr|nightly|release [--maps N] [--out DIR] [--case-id group_index] [--shard-index N --shard-count N] [--report-only]\n";
            std::exit(0);
        }
        else throw std::runtime_error("Unknown argument: " + arg);
    }
    if (config.tier != "pr" && config.tier != "nightly" && config.tier != "release") throw std::runtime_error("Tier must be pr, nightly, or release.");
    if (config.mapCount < 0) config.mapCount = config.tier == "release" ? 50000 : (config.tier == "nightly" ? 10000 : 500);
    if (config.determinismRate < 0) config.determinismRate = config.tier == "release" ? 1 : (config.tier == "nightly" ? 5 : 10);
    if (config.qualityAttempts < 0)
        config.qualityAttempts = config.tier == "release" ? 48 : (config.tier == "nightly" ? 24 : 12);
    if (config.qualityAttempts != 12 && config.qualityAttempts != 24 && config.qualityAttempts != 48)
        throw std::runtime_error("V5 quality attempts must use the fixed Draft/Balanced/Polished budget: 12, 24, or 48.");
    if (config.mapCount < 5 || config.determinismRate < 1 || config.shardCount < 1 ||
        config.shardIndex < 0 || config.shardIndex >= config.shardCount)
        throw std::runtime_error("Invalid map, determinism, or shard configuration.");
    return config;
}
}

int main(int argc, char **argv)
{
    try
    {
        const Config config = parseArgs(argc, argv);
        std::filesystem::create_directories(config.output);
        std::ofstream cases(config.output / "generation_qa_cases.jsonl");
        if (!cases) throw std::runtime_error("Could not create cases report.");
        const int groupTarget = config.mapCount / 5;
        int requestedGlobal = 0;
        int requestedShard = 0;
        for (int globalIndex = 0; globalIndex < config.mapCount; ++globalIndex)
        {
            const int group = globalIndex % 5;
            const int localIndex = globalIndex / 5;
            const std::string scenarioId = std::string(groupName(group)) + "_" + std::to_string(localIndex);
            if (!config.caseId.empty() && config.caseId != scenarioId)
                continue;
            ++requestedGlobal;
            if (globalIndex % config.shardCount == config.shardIndex)
                ++requestedShard;
        }
        if (requestedGlobal == 0)
            throw std::runtime_error("Requested case ID does not exist in this QA corpus.");
        std::map<std::string, int> counts, failures;
        std::map<std::string, std::set<std::uint64_t>> uniqueGraphHashes;
        std::map<std::string, std::vector<long long>> durations;
        std::map<std::string, CaseResult> regressionRepresentatives;
        int processed = 0;
        int determinismChecks = 0;
        int determinismFailures = 0;
        int serializationFailures = 0;
        const auto runStart = std::chrono::steady_clock::now();
        for (int globalIndex = 0; globalIndex < config.mapCount; ++globalIndex)
        {
            if (globalIndex % config.shardCount != config.shardIndex) continue;
            const int group = globalIndex % 5;
            const int localIndex = globalIndex / 5;
            const std::string groupLabel = groupName(group);
            const std::string scenarioId = groupLabel + "_" + std::to_string(localIndex);
            if (!config.caseId.empty() && config.caseId != scenarioId) continue;
            const std::string band = bandName(localIndex, groupTarget);
            const std::uint32_t seed = config.startSeed + static_cast<std::uint32_t>(group * 1000003 + localIndex * 7919);
            const auto started = std::chrono::steady_clock::now();
            Generated generated = generateCase(group, localIndex, seed, config.qualityAttempts, band);
            const auto stopped = std::chrono::steady_clock::now();
            CaseResult result;
            result.id = scenarioId; result.group = groupLabel; result.band = band; result.seed = seed;
            result.generatorVersion = generated.request.generatorVersion;
            result.recipeVersion = generated.request.recipeVersion;
            result.styleId = generated.request.styleId;
            result.styleVersion = generated.request.styleVersion;
            result.styleSnapshotId = generated.request.styleSnapshotId;
            result.entryPoint = generated.entryPoint;
            result.attemptsEvaluated = generated.attemptsEvaluated;
            result.validationEligible = generated.validation.eligible;
            result.batchFailure = batchFailureName(generated.batchFailure);
            result.elapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(stopped - started).count();
            if (generated.generated)
            {
                result.graphHash = graphHash(generated.map); result.rasterHash = rasterHash(generated.map); result.outputHash = outputHash(generated.map); result.score = generated.quality.score;
            }
            validateGenerated(result, generated, group, band);
            if (generated.generated && globalIndex % config.determinismRate == 0)
            {
                ++determinismChecks;
                Generated repeated = generateCase(group, localIndex, seed, config.qualityAttempts, band);
                const bool same = repeated.generated && outputHash(repeated.map) == result.outputHash;
                addInvariant(result, "determinism.same_output", same, true);
                if (!same) ++determinismFailures;
            }
            if (generated.generated)
            {
                MapModel restored = generated.map;
                restored.cells = PersistenceSerialization::deserializeMapCells(PersistenceSerialization::serializeMapCells(generated.map.cells));
                restored.regions = PersistenceSerialization::deserializeMapRegions(PersistenceSerialization::serializeMapRegions(generated.map.regions));
                restored.features = PersistenceSerialization::deserializeMapFeatures(PersistenceSerialization::serializeMapFeatures(generated.map.features));
                restored.routes = PersistenceSerialization::deserializeMapRoutes(PersistenceSerialization::serializeMapRoutes(generated.map.routes));
                PersistenceSerialization::deserializeMapPersistenceMetadata(
                    PersistenceSerialization::serializeMapGenerationMetadata(generated.map.generation),
                    PersistenceSerialization::serializeMapAtlasMetadata(generated.map), restored);
                const bool same = outputHash(restored) == result.outputHash;
                addInvariant(result, "persistence.roundtrip", same, true);
                if (!same) ++serializationFailures;
            }
            writeCase(cases, result);
            ++counts[groupLabel]; ++processed; uniqueGraphHashes[groupLabel].insert(result.graphHash); durations[groupLabel].push_back(result.elapsedMicroseconds);
            if (!casePassed(result))
            {
                ++failures[groupLabel];
                const std::string signature = failureSignature(result);
                if (!regressionRepresentatives.count(signature)) regressionRepresentatives[signature] = result;
            }
            if (processed % 1000 == 0) std::cout << "Processed " << processed << " maps\n";
        }
        cases.close();
        if (!cases) throw std::runtime_error("Could not finish cases report.");
        bool diversityPassed = true;
        for (const auto &entry : counts)
        {
            const double ratio = uniqueGraphHashes[entry.first].size() / static_cast<double>(std::max(1, entry.second));
            const double minimum = (entry.first == "tactical_outdoor" || entry.first == "dungeon_cave") ? 0.95 : 0.99;
            if (ratio < minimum) diversityPassed = false;
        }
        int totalFailures = 0; for (const auto &entry : failures) totalFailures += entry.second;
        const bool complete = processed == requestedShard;
        const bool passed = complete && totalFailures == 0 && determinismFailures == 0 &&
                            serializationFailures == 0 && diversityPassed;
        const auto runStop = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(runStop - runStart).count();
        std::ofstream report(config.output / "generation_qa_report.json");
        if (!report) throw std::runtime_error("Could not create summary report.");
        report << "{\n  \"schema_version\": 2,"
               << "\n  \"product_contract\": \"" << kV5ProductContract << "\","
               << "\n  \"generator_version\": " << kV5GeneratorVersion
               << ",\n  \"recipe_version\": " << kV5RecipeVersion
               << ",\n  \"style_id\": \"" << kV5StyleId << "\""
               << ",\n  \"style_version\": " << kV5StyleVersion
               << ",\n  \"style_snapshot_id\": \"" << escapeJson(mapStyleCurrentProductionSnapshotId()) << "\""
               << ",\n  \"tier\": \"" << config.tier << "\",\n  \"requested_map_count\": " << requestedGlobal
               << ",\n  \"requested_shard_map_count\": " << requestedShard
               << ",\n  \"processed_map_count\": " << processed << ",\n  \"shard_index\": " << config.shardIndex << ",\n  \"shard_count\": " << config.shardCount
               << ",\n  \"quality_attempts\": " << config.qualityAttempts << ",\n  \"determinism_checks\": " << determinismChecks
               << ",\n  \"determinism_failures\": " << determinismFailures << ",\n  \"serialization_failures\": " << serializationFailures
               << ",\n  \"failure_count\": " << totalFailures << ",\n  \"diversity_passed\": " << (diversityPassed ? "true" : "false")
               << ",\n  \"complete\": " << (complete ? "true" : "false")
               << ",\n  \"diversity_basis\": \"graph_hash\""
               << ",\n  \"elapsed_ms\": " << elapsedMs << ",\n  \"passed\": " << (passed ? "true" : "false") << ",\n  \"groups\": [\n";
        int groupIndex = 0;
        for (const auto &entry : counts)
        {
            auto times = durations[entry.first]; std::sort(times.begin(), times.end());
            const long long p95 = times.empty() ? 0 : times[std::min(times.size() - 1, times.size() * 95 / 100)];
            const double uniqueRatio = uniqueGraphHashes[entry.first].size() / static_cast<double>(std::max(1, entry.second));
            if (groupIndex++) report << ",\n";
            report << "    {\"name\":\"" << entry.first << "\",\"count\":" << entry.second << ",\"failures\":" << failures[entry.first]
                   << ",\"unique_ratio\":" << std::fixed << std::setprecision(5) << uniqueRatio << ",\"p95_us\":" << p95 << "}";
        }
        report << "\n  ],\n  \"failure_signatures\": [";
        int signatureIndex = 0;
        for (const auto &entry : regressionRepresentatives)
        {
            if (signatureIndex++) report << ',';
            report << "\n    {\"signature\":\"" << escapeJson(entry.first) << "\",\"scenario_id\":\"" << escapeJson(entry.second.id)
                   << "\",\"group\":\"" << entry.second.group << "\",\"seed\":" << entry.second.seed << "}";
        }
        if (!regressionRepresentatives.empty()) report << '\n';
        report << "  ]\n}\n";
        report.close();
        if (!report) throw std::runtime_error("Could not finish summary report.");
        std::cout << "Generation QA wrote " << (config.output / "generation_qa_report.json") << ": " << processed << " maps, " << totalFailures << " failures\n";
        if (!passed && !config.reportOnly) return 2;
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "map_generation_release_qa failed: " << error.what() << '\n';
        return 1;
    }
}
