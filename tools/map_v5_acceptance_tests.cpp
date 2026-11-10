#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapWorldBrief.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct TestState
{
    std::uint64_t checks = 0;
    std::uint64_t failures = 0;
    std::uint64_t printedFailures = 0;

    bool expect(bool condition, const std::string &message)
    {
        ++checks;
        if (condition)
            return true;
        ++failures;
        if (printedFailures < 80)
        {
            std::cerr << "FAILED: " << message << '\n';
            ++printedFailures;
            if (printedFailures == 80)
                std::cerr << "Further failures are counted but not printed.\n";
        }
        return false;
    }
};

struct RunConfiguration
{
    int seedsPerWorldPreset = 3;
    int qualityAttempts = 12;
    bool longMode = false;
    bool showHelp = false;
};

bool parsePositiveInt(const char *text, int &value)
{
    if (!text || !*text)
        return false;
    try
    {
        const long parsed = std::stol(text);
        if (parsed < 1 || parsed > std::numeric_limits<int>::max())
            return false;
        value = static_cast<int>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

RunConfiguration parseArguments(int argc, char **argv)
{
    RunConfiguration configuration;
    bool explicitSeedCount = false;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--long")
        {
            configuration.longMode = true;
            if (!explicitSeedCount)
                configuration.seedsPerWorldPreset = 1000;
        }
        else if (argument == "--smoke")
        {
            configuration.longMode = false;
            if (!explicitSeedCount)
                configuration.seedsPerWorldPreset = 3;
        }
        else if (argument == "--seeds-per-world-preset" && index + 1 < argc)
        {
            int parsed = 0;
            if (!parsePositiveInt(argv[++index], parsed))
                throw std::runtime_error("--seeds-per-world-preset expects a positive integer");
            configuration.seedsPerWorldPreset = parsed;
            explicitSeedCount = true;
        }
        else if (argument == "--quality-attempts" && index + 1 < argc)
        {
            int parsed = 0;
            if (!parsePositiveInt(argv[++index], parsed) || parsed > 48)
                throw std::runtime_error("--quality-attempts expects an integer from 1 to 48");
            configuration.qualityAttempts = parsed;
        }
        else if (argument == "--help" || argument == "-h")
            configuration.showHelp = true;
        else
            throw std::runtime_error("Unknown argument: " + argument);
    }
    if (configuration.longMode && configuration.seedsPerWorldPreset < 1000)
        throw std::runtime_error("--long requires at least 1000 seeds per world preset");
    return configuration;
}

const char *intentName(MapCreationIntent intent)
{
    switch (intent)
    {
    case MapCreationIntent::WorldOverview: return "WorldOverview";
    case MapCreationIntent::RegionExploration: return "RegionExploration";
    case MapCreationIntent::Settlement: return "Settlement";
    case MapCreationIntent::TravelScene: return "TravelScene";
    case MapCreationIntent::Dungeon: return "Dungeon";
    case MapCreationIntent::Cave: return "Cave";
    case MapCreationIntent::TacticalEncounter: return "TacticalEncounter";
    case MapCreationIntent::ImportedReference: return "ImportedReference";
    }
    return "Unknown";
}

const char *presetName(MapSizePreset preset)
{
    switch (preset)
    {
    case MapSizePreset::Small: return "Small";
    case MapSizePreset::Standard: return "Standard";
    case MapSizePreset::Large: return "Large";
    case MapSizePreset::Custom: return "Custom";
    }
    return "Unknown";
}

MapLevel expectedLevel(MapCreationIntent intent)
{
    switch (intent)
    {
    case MapCreationIntent::WorldOverview: return MapLevel::World;
    case MapCreationIntent::RegionExploration: return MapLevel::Region;
    case MapCreationIntent::TacticalEncounter: return MapLevel::Tactical;
    case MapCreationIntent::Settlement:
    case MapCreationIntent::TravelScene:
    case MapCreationIntent::Dungeon:
    case MapCreationIntent::Cave:
    case MapCreationIntent::ImportedReference:
        return MapLevel::Site;
    }
    return MapLevel::Region;
}

MapDimensions expectedDimensions(MapLevel level, MapSizePreset preset)
{
    const int column = preset == MapSizePreset::Small ? 0 : preset == MapSizePreset::Large ? 2 : 1;
    constexpr MapDimensions world[] = {{48, 32}, {72, 48}, {96, 64}};
    constexpr MapDimensions region[] = {{64, 48}, {80, 60}, {96, 72}};
    constexpr MapDimensions site[] = {{48, 36}, {64, 48}, {80, 60}};
    constexpr MapDimensions tactical[] = {{30, 22}, {40, 30}, {60, 45}};
    switch (level)
    {
    case MapLevel::World: return world[column];
    case MapLevel::Region: return region[column];
    case MapLevel::Site: return site[column];
    case MapLevel::Tactical: return tactical[column];
    }
    return {};
}

bool dimensionsEqual(const MapDimensions &left, const MapDimensions &right)
{
    return left.width == right.width && left.height == right.height;
}

void testCreationContract(TestState &state)
{
    constexpr MapCreationIntent intents[] = {
        MapCreationIntent::WorldOverview,
        MapCreationIntent::RegionExploration,
        MapCreationIntent::Settlement,
        MapCreationIntent::TravelScene,
        MapCreationIntent::Dungeon,
        MapCreationIntent::Cave,
        MapCreationIntent::TacticalEncounter,
        MapCreationIntent::ImportedReference};
    constexpr MapSizePreset presets[] = {
        MapSizePreset::Small,
        MapSizePreset::Standard,
        MapSizePreset::Large,
        MapSizePreset::Custom};

    for (std::size_t intentIndex = 0; intentIndex < std::size(intents); ++intentIndex)
    {
        const MapCreationIntent intent = intents[intentIndex];
        const MapLevel level = expectedLevel(intent);
        for (MapSizePreset preset : presets)
        {
            const std::string context = std::string(intentName(intent)) + "/" + presetName(preset);
            MapCreationRequest request;
            request.intent = intent;
            request.sizePreset = preset;
            request.dimensions = {37 + static_cast<int>(intentIndex), 29 + static_cast<int>(intentIndex)};
            request.seed = 0x12340000u + static_cast<std::uint32_t>(intentIndex * 17u) + static_cast<std::uint32_t>(preset);

            const MapGenerationOptions options = mapGenerationOptionsFromCreationRequest(request);
            const MapDimensions expected = preset == MapSizePreset::Custom
                                               ? request.dimensions
                                               : expectedDimensions(level, preset);
            state.expect(mapLevelForCreationIntent(intent) == level, context + " maps to the wrong MapLevel");
            state.expect(options.creationIntent == intent, context + " loses the canonical intent");
            state.expect(options.scale == level, context + " produces an inconsistent generator MapLevel");
            state.expect(options.scope == mapLegacyScopeForCreationIntent(intent), context + " produces an inconsistent legacy scope");
            state.expect(options.width == expected.width && options.height == expected.height,
                         context + " produces dimensions outside the V5 preset contract");
            state.expect(options.seed == request.seed, context + " changes the uint32 seed");

            if (preset != MapSizePreset::Custom)
            {
                state.expect(dimensionsEqual(mapDimensionsForPreset(intent, preset), expected),
                             context + " helper dimensions differ from the public preset table");
                MapModel map;
                applyMapCreationIntent(map, intent, preset, true);
                state.expect(map.creationIntent == intent && map.sizePreset == preset && map.atlasLevel == level,
                             context + " does not apply atomically to a map");
                state.expect(map.width == expected.width && map.height == expected.height &&
                                 map.cells.size() == static_cast<std::size_t>(expected.width * expected.height),
                             context + " does not resize the cell projection correctly");
                state.expect(map.mapKind == mapKindForCreationIntent(intent) &&
                                 map.scope == mapLegacyScopeForCreationIntent(intent),
                             context + " creates contradictory legacy fields");
                state.expect(level == MapLevel::Tactical
                                 ? (map.gridType == GridType::Square && map.cellSize == 5.0f)
                                 : map.gridType == GridType::None,
                             context + " applies the wrong default grid contract");
            }
        }
    }
}

bool semanticFactsEqual(const std::vector<MapGenerationFact> &left,
                        const std::vector<MapGenerationFact> &right)
{
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const MapGenerationFact &a = left[index];
        const MapGenerationFact &b = right[index];
        if (a.id != b.id || a.type != b.type || a.strength != b.strength ||
            a.label != b.label || a.entityRefs != b.entityRefs || a.value != b.value ||
            a.confidence != b.confidence || a.confirmed != b.confirmed)
            return false;
    }
    return true;
}

struct SharedRiverBriefs
{
    std::string germanSource;
    std::string englishSource;
    MapWorldBrief german;
    MapWorldBrief english;
};

SharedRiverBriefs interpretSharedRiverBriefs(TestState &state)
{
    SharedRiverBriefs result;
    result.germanSource =
        "Drei Nationen namens Alderreach, Veyra und Kharad teilen sich einen gro\xC3\x9F" "en Fluss. "
        "Es gibt eine gro\xC3\x9F" "e Hauptstadt namens Crosswater.";
    result.englishSource =
        "Three nations named Alderreach, Veyra, and Kharad share a great river. "
        "There is a large capital named Crosswater.";
    result.german = MapWorldBriefInterpreter::interpret(result.germanSource);
    result.english = MapWorldBriefInterpreter::interpret(result.englishSource);

    const std::vector<std::string> expectedNations = {"Alderreach", "Veyra", "Kharad"};
    state.expect(result.german.statedNationCount == 3 && result.english.statedNationCount == 3,
                 "The exact DE/EN shared-river briefs must both extract three nations");
    state.expect(result.german.nationNames == expectedNations && result.english.nationNames == expectedNations,
                 "The exact DE/EN shared-river briefs must preserve the same nation names and order");
    state.expect(result.german.capitalName == "Crosswater" && result.english.capitalName == "Crosswater",
                 "The exact DE/EN shared-river briefs must preserve the capital");
    state.expect(result.german.requiresSharedGreatRiver && result.english.requiresSharedGreatRiver,
                 "The exact DE/EN shared-river briefs must both require a shared great river");
    state.expect(semanticFactsEqual(result.german.facts, result.english.facts),
                 "Equivalent DE/EN notes must produce byte-equivalent typed fact semantics");
    state.expect(!result.german.facts.empty() &&
                     result.german.facts.front().sourceText == result.germanSource &&
                     result.english.facts.front().sourceText == result.englishSource,
                 "Typed facts must retain their original-language source provenance");
    return result;
}

MapGenerationOptions sharedRiverOptions(const MapWorldBrief &brief,
                                        MapSizePreset preset,
                                        std::uint32_t seed,
                                        int qualityAttempts)
{
    const MapDimensions dimensions = expectedDimensions(MapLevel::World, preset);
    MapGenerationOptions options;
    options.generatorVersion = 5;
    options.recipeVersion = 3;
    options.scope = MapScope::World;
    options.scale = MapLevel::World;
    options.creationIntent = MapCreationIntent::WorldOverview;
    options.sizePreset = preset;
    options.purpose = MapGenerationPurpose::Region;
    options.width = dimensions.width;
    options.height = dimensions.height;
    options.seed = seed;
    options.waterLevel = 18;
    options.mountainDensity = 12;
    options.forestDensity = 20;
    options.settlementDensity = 3;
    options.routeDensity = 28;
    options.roadNetwork = MapRoadNetworkMode::Connected;
    options.edgeConnectionCount = 4;
    options.requestedSettlements = std::max(4, static_cast<int>(brief.cityNames.size()) + (brief.capitalName.empty() ? 0 : 1));
    options.requestedLandmarks = 0;
    options.quality = MapGenerationQualityLevel::Balanced;
    options.qualityAttempts = qualityAttempts;
    options.nationNames = brief.nationNames;
    options.facts = brief.facts;
    if (!brief.capitalName.empty())
    {
        MapGenerationConstraint capital;
        capital.id = "brief_capital";
        capital.label = brief.capitalName;
        capital.featureKind = MapGeneratedFeatureKind::Settlement;
        capital.x = static_cast<float>(options.width) * 0.52f;
        capital.y = static_cast<float>(options.height) * 0.48f;
        capital.required = true;
        capital.locked = true;
        options.constraints.push_back(capital);
    }
    return options;
}

class StableHash
{
public:
    void addBytes(const void *data, std::size_t size)
    {
        const auto *bytes = static_cast<const unsigned char *>(data);
        for (std::size_t index = 0; index < size; ++index)
        {
            value_ ^= static_cast<std::uint64_t>(bytes[index]);
            value_ *= 1099511628211ull;
        }
    }

    template <typename T>
    void addPod(const T &value)
    {
        addBytes(&value, sizeof(T));
    }

    void addString(const std::string &value)
    {
        const std::uint64_t size = static_cast<std::uint64_t>(value.size());
        addPod(size);
        addBytes(value.data(), value.size());
    }

    std::uint64_t value() const { return value_; }

private:
    std::uint64_t value_ = 1469598103934665603ull;
};

std::uint64_t graphHash(const MapFeatureGraph &graph)
{
    StableHash hash;
    hash.addPod(graph.schemaVersion);
    hash.addPod(graph.features.size());
    for (const MapGeneratedFeature &feature : graph.features)
    {
        hash.addString(feature.id);
        hash.addPod(feature.kind);
        hash.addString(feature.name);
        hash.addString(feature.materialId);
        hash.addPod(feature.width);
        hash.addPod(feature.closed);
        hash.addPod(feature.locked);
        hash.addPod(feature.discovered);
        hash.addPod(feature.gmOnly);
        hash.addString(feature.lineage.sourceMapId);
        hash.addString(feature.lineage.sourceFeatureId);
        hash.addPod(feature.lineage.sourceRevision);
        hash.addPod(feature.points.size());
        for (const MapFeaturePoint &point : feature.points)
        {
            hash.addPod(point.x);
            hash.addPod(point.y);
        }
    }
    return hash.value();
}

std::uint64_t rasterHash(const MapModel &map)
{
    StableHash hash;
    hash.addPod(map.width);
    hash.addPod(map.height);
    hash.addPod(map.scope);
    hash.addPod(map.atlasLevel);
    hash.addPod(map.mapKind);
    hash.addPod(map.gridType);
    hash.addPod(map.cellSize);
    hash.addPod(map.cells.size());
    for (const MapCellModel &cell : map.cells)
    {
        hash.addPod(cell.kind);
        hash.addPod(cell.rotationQuarterTurns);
        hash.addPod(cell.rotationLocked);
        hash.addPod(cell.symbolVariantMode);
        hash.addPod(cell.wayKind);
        hash.addPod(cell.movementCost);
        hash.addPod(cell.difficultTerrain);
        hash.addPod(cell.blocksLineOfSight);
        hash.addPod(cell.lighting);
        hash.addPod(cell.fogged);
        hash.addPod(cell.elevation);
        hash.addPod(cell.movementBlockedMask);
        hash.addPod(cell.lineOfSightBlockedMask);
        hash.addPod(cell.movementOneWayBlockedMask);
        hash.addPod(cell.lineOfSightOneWayBlockedMask);
    }
    return hash.value();
}

bool featureGraphsEqual(const MapFeatureGraph &left, const MapFeatureGraph &right)
{
    return graphHash(left) == graphHash(right) && left.schemaVersion == right.schemaVersion &&
           left.features.size() == right.features.size() &&
           std::equal(left.features.begin(), left.features.end(), right.features.begin(),
                      [](const MapGeneratedFeature &a, const MapGeneratedFeature &b) {
                          if (a.id != b.id || a.kind != b.kind || a.name != b.name ||
                              a.materialId != b.materialId || a.width != b.width ||
                              a.closed != b.closed || a.locked != b.locked ||
                              a.discovered != b.discovered || a.gmOnly != b.gmOnly ||
                              a.lineage.sourceMapId != b.lineage.sourceMapId ||
                              a.lineage.sourceFeatureId != b.lineage.sourceFeatureId ||
                              a.lineage.sourceRevision != b.lineage.sourceRevision ||
                              a.points.size() != b.points.size())
                              return false;
                          for (std::size_t pointIndex = 0; pointIndex < a.points.size(); ++pointIndex)
                              if (a.points[pointIndex].x != b.points[pointIndex].x ||
                                  a.points[pointIndex].y != b.points[pointIndex].y)
                                  return false;
                          return true;
                      });
}

std::string seedContext(MapSizePreset preset, std::uint32_t seed)
{
    return std::string("World/") + presetName(preset) + "/seed=" + std::to_string(seed);
}

bool categoryFloorMet(const MapGenerationResult &candidate, std::string &failedCategory)
{
    const MapGenerationQualityReport &quality = candidate.quality;
    const auto check = [&](int score, const char *name) {
        if (score >= 80)
            return true;
        failedCategory = name;
        return false;
    };
    if (!check(quality.wayConnectivityPercent, "wayConnectivity")) return false;
    if (!check(quality.coherentTerrainPercent, "coherentTerrain")) return false;
    if (!check(quality.roadEfficiencyPercent, "roadEfficiency")) return false;
    if (!check(quality.hydrologyPercent, "hydrology")) return false;
    if (!check(quality.bridgeValidityPercent, "bridgeValidity")) return false;
    if (!check(quality.settlementSuitabilityPercent, "settlementSuitability")) return false;
    if (!check(quality.compositionPercent, "composition")) return false;
    if (!check(quality.visualHierarchyPercent, "visualHierarchy")) return false;
    if (!check(quality.scaleCorrectnessPercent, "scaleCorrectness")) return false;
    if (!check(quality.assetCoveragePercent, "assetCoverage")) return false;
    if (candidate.map.atlasLevel != MapLevel::World &&
        !check(quality.traversableConnectivityPercent, "traversableConnectivity")) return false;
    if ((candidate.map.atlasLevel == MapLevel::Site || candidate.map.atlasLevel == MapLevel::Tactical) &&
        !check(quality.structureAccessPercent, "structureAccess")) return false;
    return true;
}

void compareDeterministicBatches(TestState &state,
                                 const MapGenerationBatchResult &first,
                                 const MapGenerationBatchResult &second,
                                 const std::string &context)
{
    state.expect(first.failure == second.failure, context + " changes its structured batch result on repetition");
    state.expect(first.attemptsEvaluated == second.attemptsEvaluated,
                 context + " changes its deterministic attempt count on repetition");
    state.expect(first.candidates.size() == second.candidates.size(),
                 context + " changes its selectable candidate count on repetition");
    const std::size_t count = std::min(first.candidates.size(), second.candidates.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        const MapGenerationResult &a = first.candidates[index];
        const MapGenerationResult &b = second.candidates[index];
        const std::string candidateContext = context + "/candidate=" + std::to_string(index);
        state.expect(a.quality.selectedSeed == b.quality.selectedSeed && a.quality.score == b.quality.score,
                     candidateContext + " changes its selected seed or quality score");
        state.expect(graphHash(a.map.generation.graph) == graphHash(b.map.generation.graph),
                     candidateContext + " changes its feature-graph hash");
        state.expect(rasterHash(a.map) == rasterHash(b.map),
                     candidateContext + " changes its raster hash");
        state.expect(featureGraphsEqual(a.map.generation.graph, b.map.generation.graph),
                     candidateContext + " changes feature IDs, geometry, metadata, or ordering");
    }
}

struct MassRunStatistics
{
    std::uint64_t requestedSeeds = 0;
    std::uint64_t generatedBatches = 0;
    std::uint64_t selectableCandidates = 0;
};

MassRunStatistics testSharedRiverSeeds(TestState &state,
                                       const SharedRiverBriefs &briefs,
                                       const RunConfiguration &configuration)
{
    MassRunStatistics statistics;
    constexpr MapSizePreset presets[] = {MapSizePreset::Small, MapSizePreset::Standard, MapSizePreset::Large};
    for (std::size_t presetIndex = 0; presetIndex < std::size(presets); ++presetIndex)
    {
        const MapSizePreset preset = presets[presetIndex];
        const int progressInterval = std::max(1, configuration.seedsPerWorldPreset / 10);
        for (int seedIndex = 0; seedIndex < configuration.seedsPerWorldPreset; ++seedIndex)
        {
            const std::uint32_t seed = 0x51a70001u + static_cast<std::uint32_t>(presetIndex) * 0x01000193u +
                                       static_cast<std::uint32_t>(seedIndex) * 0x9e3779b9u;
            const std::string context = seedContext(preset, seed);
            const MapGenerationOptions options = sharedRiverOptions(
                briefs.german, preset, seed, configuration.qualityAttempts);
            const MapGenerationBatchResult first = generateSemanticMapCandidateBatch(options, 3);
            const MapGenerationBatchResult second = generateSemanticMapCandidateBatch(options, 3);
            ++statistics.requestedSeeds;
            statistics.generatedBatches += 2;

            state.expect(first.succeeded() && first.failure == MapGenerationBatchFailureCode::None,
                         context + " did not produce any selectable V5 design");
            state.expect(!first.candidates.empty() && first.candidates.size() <= 3,
                         context + " returned an invalid selectable candidate count");
            for (std::size_t candidateIndex = 0; candidateIndex < first.candidates.size(); ++candidateIndex)
            {
                const MapGenerationResult &candidate = first.candidates[candidateIndex];
                const std::string candidateContext = context + "/candidate=" + std::to_string(candidateIndex);
                std::string failedCategory;
                state.expect(candidate.validation.eligible && candidate.validation.blockers.empty() &&
                                 candidate.validation.criticalIssueCount == 0,
                             candidateContext + " exposes a blocker in a selectable design");
                state.expect(candidate.quality.score >= 85,
                             candidateContext + " is below the 85-point selectable quality floor");
                state.expect(categoryFloorMet(candidate, failedCategory),
                             candidateContext + " is below 80 in relevant category " + failedCategory);
                state.expect(mapGenQualityMeetsCandidateFloor(candidate.map, candidate.quality),
                             candidateContext + " disagrees with the production quality-floor predicate");
                state.expect(mapGenV4SharedGreatRiverIsValid(candidate.map),
                             candidateContext + " does not contain the required continuous shared river");
                ++statistics.selectableCandidates;
            }
            compareDeterministicBatches(state, first, second, context);
            if (configuration.longMode &&
                ((seedIndex + 1) % progressInterval == 0 || seedIndex + 1 == configuration.seedsPerWorldPreset))
                std::cout << "  " << presetName(preset) << ": " << (seedIndex + 1) << "/"
                          << configuration.seedsPerWorldPreset << " seeds\n" << std::flush;
        }
    }
    return statistics;
}

void testStructuredConflict(TestState &state, const SharedRiverBriefs &briefs, int qualityAttempts)
{
    MapGenerationOptions impossible = sharedRiverOptions(
        briefs.german, MapSizePreset::Small, 0xdeadbeefu, std::min(qualityAttempts, 12));
    // A typed Must fact is deliberately used instead of relying on a tiny-map
    // implementation detail. Future hydrology may legitimately support very
    // small maps, but it must never invent this unsupported feature merely to
    // make the request look successful.
    MapGenerationFact unsupported;
    unsupported.id = "acceptance_unsupported_feature";
    unsupported.type = MapGenerationFactType::IncludedElement;
    unsupported.strength = MapGenerationFactStrength::Must;
    unsupported.label = "clockwork moon elevator";
    unsupported.value = unsupported.label;
    unsupported.confirmed = true;
    impossible.facts.push_back(unsupported);
    const MapGenerationBatchResult noCandidate = generateSemanticMapCandidateBatch(impossible, 3);
    state.expect(noCandidate.candidates.empty() && !noCandidate.succeeded(),
                 "An impossible shared-river request must not expose a fallback map");
    state.expect(noCandidate.failure == MapGenerationBatchFailureCode::NoEligibleCandidate,
                 "An impossible shared-river request must return a structured no-eligible-candidate conflict");
    state.expect(!noCandidate.rejectedCandidates.empty(),
                 "An impossible shared-river request must retain diagnostic rejected candidates");
    for (const MapGenerationResult &rejected : noCandidate.rejectedCandidates)
    {
        state.expect(!rejected.validation.eligible && !rejected.validation.blockers.empty(),
                     "A rejected impossible map must explain at least one hard blocker");
        state.expect(rejected.quality.score == 0,
                     "A rejected invalid map must not expose a normal user-facing quality score");
    }

    MapGenerationOptions contradiction = sharedRiverOptions(
        briefs.german, MapSizePreset::Small, 0xc0ffeeu, std::min(qualityAttempts, 12));
    contradiction.requiredElements.push_back("Ancient tower");
    contradiction.excludedElements.push_back("Ancient tower");
    const MapGenerationBatchResult invalidRequest = generateSemanticMapCandidateBatch(contradiction, 3);
    state.expect(invalidRequest.candidates.empty() &&
                     invalidRequest.failure == MapGenerationBatchFailureCode::InvalidRequest &&
                     invalidRequest.attemptsEvaluated == 0,
                 "A contradictory request must fail before generation with a structured conflict");
    state.expect(!invalidRequest.requestValidation.eligible &&
                     !invalidRequest.requestValidation.blockers.empty(),
                 "A contradictory request must provide request-level blocker details");
}
}

int main(int argc, char **argv)
{
    RunConfiguration configuration;
    try
    {
        configuration = parseArguments(argc, argv);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\nUse --help for usage.\n";
        return 2;
    }

    if (configuration.showHelp)
    {
        std::cout
            << "GM Editor map V5 acceptance tests\n\n"
            << "  --smoke                         3 seeds per world preset (default)\n"
            << "  --long                          1000 seeds per world preset; release gate\n"
            << "  --seeds-per-world-preset N      override the seed count\n"
            << "  --quality-attempts N             deterministic attempts per seed (1..48, default 12)\n\n"
            << "The long mode repeats every seed and compares graph/raster hashes and full features.\n";
        return 0;
    }

    std::cout << "Map V5 acceptance: " << (configuration.longMode ? "LONG" : "SMOKE")
              << ", seeds/preset=" << configuration.seedsPerWorldPreset
              << ", quality-attempts=" << configuration.qualityAttempts << '\n';
    TestState state;
    const auto started = std::chrono::steady_clock::now();
    testCreationContract(state);
    const SharedRiverBriefs briefs = interpretSharedRiverBriefs(state);
    testStructuredConflict(state, briefs, configuration.qualityAttempts);
    const MassRunStatistics statistics = testSharedRiverSeeds(state, briefs, configuration);
    const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    std::cout << "Checked " << statistics.requestedSeeds << " world seeds in "
              << std::fixed << std::setprecision(2) << elapsedSeconds << " s ("
              << statistics.generatedBatches << " deterministic batches, "
              << statistics.selectableCandidates << " selectable candidates).\n"
              << "Assertions: " << state.checks << ", failures: " << state.failures << '\n';
    if (state.failures == 0)
    {
        std::cout << "Map V5 acceptance passed.\n";
        if (!configuration.longMode)
            std::cout << "Release command: map_v5_acceptance_tests --long\n";
        return 0;
    }
    return 1;
}
