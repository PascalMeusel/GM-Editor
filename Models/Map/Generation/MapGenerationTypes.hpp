#pragma once
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class MapMountainPattern
{
    Mixed,
    Ranges,
    IndividualPeaks
};

enum class MapRoadNetworkMode
{
    None,
    Sparse,
    Connected,
    Grid
};

struct MapGenerationProgress
{
    int step = 0;
    int totalSteps = 0;
    std::string stage;
};

using MapGenerationProgressCallback = std::function<void(const MapGenerationProgress &)>;
using MapGenerationCancellationCallback = std::function<bool()>;

struct MapGenerationOptions
{
    // Kept at v1 for compatibility with explicitly unversioned integration
    // callers. Product entry points select v5; tests and imported legacy tools
    // can still request their historical algorithm deterministically.
    int generatorVersion = 1;
    int recipeVersion = 3;
    std::string styleId = defaultMapStyleId();
    int styleVersion = 3;
    std::string styleSnapshotId = mapStyleCurrentProductionSnapshotId();
    MapScope scope = MapScope::World;
    int width = 24;
    int height = 18;
    std::uint32_t seed = 240529;
    int waterLevel = 20;
    int mountainDensity = 18;
    int forestDensity = 25;
    int settlementDensity = 8;
    int routeDensity = 28;
    int tacticalDensity = 18;
    MapMountainPattern mountainPattern = MapMountainPattern::Mixed;
    MapRoadNetworkMode roadNetwork = MapRoadNetworkMode::Connected;
    // A stable seed may evaluate several deterministic variations and retain the
    // most playable one. One preserves the fast legacy path; V5 product entry
    // points use the fixed 12/24/48 Draft/Balanced/Polished budgets.
    int qualityAttempts = 1;
    bool caveNetwork = false;
    MapGenerationPurpose purpose = MapGenerationPurpose::Region;
    MapLandscapeProfile landscape = MapLandscapeProfile::RiverValley;
    MapLandform landform = MapLandform::Automatic;
    MapLevel scale = MapLevel::Region;
    MapCreationIntent creationIntent = MapCreationIntent::RegionExploration;
    MapSizePreset sizePreset = MapSizePreset::Custom;
    MapGenerationQualityLevel quality = MapGenerationQualityLevel::Balanced;
    std::string contentLanguage = "de";
    std::string nameCulture = "neutral_fantasy";
    bool allowAdditionalNations = false;
    int requestedSettlements = -1;
    int requestedLandmarks = 4;
    int edgeConnectionCount = 2;
    std::vector<std::string> nationNames;
    std::vector<std::string> knownFacts;
    std::vector<std::string> requiredElements;
    std::vector<std::string> optionalElements;
    std::vector<std::string> excludedElements;
    std::vector<std::string> lockedFeatureIds;
    std::vector<MapGenerationConstraint> constraints;
    std::vector<MapGenerationFact> facts;
    // Runtime-only atlas input. These features have already been transformed
    // into the child map coordinate system and must participate in generation
    // before hydrology, settlement placement and route planning run. They are
    // deliberately not part of MapGenerationRecipe/persistence: lineage stays
    // on the exact inherited feature records themselves.
    std::vector<MapGeneratedFeature> preservedFeatures;
    MapGenerationProgressCallback progressCallback;
    // Background callers may request cancellation without making candidate
    // selection timing-dependent. The callback is observed only at stable
    // pipeline boundaries; every completed attempt remains deterministic.
    MapGenerationCancellationCallback cancellationRequested;
};

enum class MapGenerationIssueSeverity
{
    Info,
    Warning,
    Critical
};

struct MapGenerationIssue
{
    std::string code;
    MapGenerationIssueSeverity severity = MapGenerationIssueSeverity::Info;
    std::string message;
    std::string suggestedAction;
};

struct MapGenerationQualityReport
{
    int score = 0;
    int candidatesEvaluated = 1;
    int selectedCandidateIndex = 0;
    std::uint32_t selectedSeed = 0;
    int traversableConnectivityPercent = 0;
    int wayConnectivityPercent = 100;
    int structureAccessPercent = 100;
    int coherentTerrainPercent = 100;
    int roadEfficiencyPercent = 100;
    int hydrologyPercent = 100;
    int bridgeValidityPercent = 100;
    int settlementSuitabilityPercent = 100;
    int compositionPercent = 100;
    int visualHierarchyPercent = 100;
    int scaleCorrectnessPercent = 100;
    int assetCoveragePercent = 100;
    int criticalIssueCount = 0;
    bool criticalRouteConnected = true;
    std::vector<MapGenerationIssue> issues;
};

enum class MapGenerationFactFulfillment
{
    NotEvaluated,
    Fulfilled,
    Adjusted,
    Unsatisfied
};

// Per-fact evidence is intentionally part of hard validation rather than the
// soft score. The UI can therefore explain exactly which promise was kept,
// adjusted (Prefer), or rejected (Must), and can focus the corresponding
// generated features without parsing a diagnostic sentence.
struct MapGenerationFactValidation
{
    std::string factId;
    MapGenerationFactStrength strength = MapGenerationFactStrength::Automatic;
    MapGenerationFactFulfillment fulfillment = MapGenerationFactFulfillment::NotEvaluated;
    std::string issueCode;
    std::vector<EntityId> affectedFeatureIds;
    std::vector<MapFeaturePoint> positions;
};

// Hard generation validity is deliberately separate from the soft quality
// score. A visually promising map may score well while still violating a
// required fact; callers must only offer results whose validation is eligible.
// Product UI should localize the stable issue codes instead of persisting or
// presenting the compatibility diagnostic strings.
struct MapGenerationValidationReport
{
    bool eligible = false;
    int criticalIssueCount = 0;
    std::vector<MapGenerationIssue> blockers;
    std::vector<MapGenerationIssue> warnings;
    std::vector<MapGenerationFactValidation> facts;
};

struct MapGenerationResult
{
    MapModel map;
    std::vector<std::string> warnings;
    MapGenerationQualityReport quality;
    MapGenerationValidationReport validation;
};

struct MapGenerationCandidate
{
    MapGenerationResult generated;
    int rank = 0;
};

enum class MapGenerationBatchFailureCode
{
    None,
    InvalidRequest,
    NoEligibleCandidate,
    NoCandidateMetQualityFloor,
    Cancelled
};

struct MapGenerationBatchResult
{
    std::vector<MapGenerationResult> candidates;
    std::vector<MapGenerationResult> rejectedCandidates;
    MapGenerationValidationReport requestValidation;
    MapGenerationBatchFailureCode failure = MapGenerationBatchFailureCode::None;
    int attemptsEvaluated = 0;
    // Explicit product decision. A position is only assigned after the hard
    // validity and quality gates have passed and the final diverse result set
    // has been ranked. Failed, cancelled, and empty batches never recommend a
    // candidate.
    int recommendedIndex = -1;

    bool succeeded() const
    {
        return failure == MapGenerationBatchFailureCode::None && !candidates.empty();
    }
};

struct MapGenV7BridgeApproach
{
    std::vector<MapCellCoordinate> cells;
    MapFeaturePoint roadAnchor;
};
