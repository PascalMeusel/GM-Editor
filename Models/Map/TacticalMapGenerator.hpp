#pragma once

#include "Models/Map/MapGeneration.hpp"

#include <string>
#include <vector>

enum class TacticalMapTemplate
{
    OpenGround,
    ForestRoad,
    RiverCrossing,
    Ruin,
    VillageStreet,
    CitySquare,
    Cave,
    StoneDungeon
};

// Composition changes are independent of the V5 generator and V3 recipe/art
// schemas. Persist this provenance on newly generated maps; loading existing
// maps always uses their saved cells and graph without regeneration.
inline constexpr int kTacticalCompositionRevision = 2;
inline constexpr int kTacticalCompositionRevisionV7 = 3;
inline constexpr int kTacticalCompositionRevisionV8 = 4;
inline constexpr int kTacticalCompositionRevisionV9 = 5;
inline constexpr int kTacticalCompositionRevisionV10 = 12;
inline constexpr int kTacticalCompositionRevisionV11 = 13;
inline constexpr int kTacticalCompositionRevisionV12 = 14;
inline constexpr int kTacticalCompositionRevisionRecipe6 = 15;

struct TacticalMapRecipe
{
    int schemaVersion = 3;
    // Explicit generation provenance. Existing direct callers retain V5.
    int generatorVersion = 5;
    // Drawing provenance is independent of tactical placement/composition.
    int illustrationRecipeVersion = 3;
    TacticalMapTemplate mapTemplate = TacticalMapTemplate::OpenGround;
    int width = 24;
    int height = 18;
    GridType gridType = GridType::Square;
    std::uint32_t seed = 240529;
    // Fixed V5 search budgets are derived from this level: 12/24/48 for
    // Draft/Balanced/Polished. Existing callers retain Balanced behavior.
    MapGenerationQualityLevel quality = MapGenerationQualityLevel::Balanced;
    // Cover is balanced independently on both deployment halves. Values are a
    // desired density, not a request for unrelated wilderness decoration.
    int coverDensity = 24;
    int objectiveCount = 1;
    // Side length in cells of the largest creature that must have a usable
    // placement (2 means a 10-by-10-foot footprint on the default grid).
    int minimumCreatureFootprint = 2;
    std::vector<std::string> requiredElements;
    // Runtime-only atlas geometry, already transformed into tactical map
    // coordinates. It participates in deployment/objective composition and is
    // never serialized as part of this short-lived recipe.
    std::vector<MapGeneratedFeature> preservedFeatures;
    // Runtime-only background-job hooks. They are observed only at stable
    // deterministic boundaries and are never persisted in a recipe.
    MapGenerationProgressCallback progressCallback;
    MapGenerationCancellationCallback cancellationRequested;
};

struct TacticalMapCandidate
{
    int rank = 0;
    std::string templateLabel;
    MapModel map;
    MapGenerationQualityReport quality;
    std::vector<std::string> readinessIssues;
};

namespace TacticalMapGenerator
{
std::vector<TacticalMapCandidate> generate(const TacticalMapRecipe &recipe, const MapModel *source = nullptr, int candidateCount = 3);

// Canonical adapter for editor and QA entry points that already own a V5
// MapGenerationOptions request. It deliberately delegates tactical
// composition/readiness validation to generate() instead of returning a
// generic landscape dressed up as an encounter map.
TacticalMapRecipe recipeFromGenerationOptions(const MapGenerationOptions &options);
MapGenerationBatchResult generateBatch(const MapGenerationOptions &options,
                                       const MapModel *source = nullptr,
                                       int candidateCount = 3);
}
