#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <queue>
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
#include "Models/Map/Generation/MapGenerationSettlementBuilder.hpp"

MapGenerationSettlementBuilder::MapGenerationSettlementBuilder(MapGenerationResult &result,
                                                               const MapGenerationOptions &options,
                                                               std::mt19937 &rng, int totalSteps)
    : map(result.map), options(options), rng(rng), totalSteps(totalSteps)
{
}

void MapGenerationSettlementBuilder::run()
{
    initializeRecipe();
    shapeTerrain();
    planStreetsAndCrossings();
    buildDistrictsAndBlocks();
    finalizeMap();
}

void MapGenerationSettlementBuilder::initializeRecipe()
{

    mapGenClear(map, MapTileKind::Empty);
    map.generation.enabled = true;
    map.generation.continuousRendering = true;
    map.generation.recipe.landform = options.landform;
    if (options.generatorVersion >= 11)
    {
        map.generation.recipe.waterLevel = options.waterLevel;
        map.generation.recipe.mountainDensity = options.mountainDensity;
        map.generation.recipe.forestDensity = options.forestDensity;
        map.generation.recipe.settlementDensity = options.settlementDensity;
        map.generation.recipe.routeDensity = options.routeDensity;
        map.generation.recipe.tacticalDensity = options.tacticalDensity;
    }
    map.generation.recipe.generatorVersion = std::max(3, options.generatorVersion);
    map.generation.recipe.recipeVersion = std::max(1, options.recipeVersion);
    settlementArchetype = options.generatorVersion >= 4 ? static_cast<int>((options.seed / 17u) % 6u) : 0;
    const char *settlementRecipeIds[] = {"settlement_organic", "settlement_radial",    "settlement_river",
                                         "settlement_coastal", "settlement_fortified", "settlement_grid"};
    map.generation.recipe.recipeId = settlementRecipeIds[settlementArchetype];
    map.generation.recipe.styleId = options.styleId;
    const bool illustratedOverview =
        options.styleId == "illustrated_atlas" &&
        (options.generatorVersion >= 12 || options.scale == MapLevel::World || options.scale == MapLevel::Region);
    map.generation.recipe.styleVersion = illustratedOverview             ? (options.generatorVersion >= 12   ? 6
                                                                            : options.generatorVersion >= 10 ? 5
                                                                            : options.generatorVersion >= 9  ? 4
                                                                            : options.generatorVersion >= 8  ? 3
                                                                            : options.generatorVersion >= 7  ? 2
                                                                                                             : 1)
                                         : options.generatorVersion >= 4 ? std::max(3, options.styleVersion)
                                                                         : 2;
    map.generation.recipe.styleSnapshotId =
        illustratedOverview ? (options.generatorVersion >= 12   ? "illustrated_atlas_v6"
                               : options.generatorVersion >= 10 ? "illustrated_atlas_v5"
                               : options.generatorVersion >= 9  ? "illustrated_atlas_v4"
                               : options.generatorVersion >= 8  ? "illustrated_atlas_v3"
                               : options.generatorVersion >= 7  ? "illustrated_atlas_v2"
                                                                : "illustrated_atlas_v1")
        : options.generatorVersion >= 4
            ? (options.styleSnapshotId.empty() ? mapStyleCurrentProductionSnapshotId() : options.styleSnapshotId)
            : "";
    map.generation.recipe.scale = options.scale;
    map.generation.recipe.creationIntent = options.creationIntent;
    map.generation.recipe.sizePreset = options.sizePreset;
    map.generation.recipe.contentLanguage = options.contentLanguage;
    map.generation.recipe.nameCulture = options.nameCulture;
    map.generation.recipe.allowAdditionalNations = options.allowAdditionalNations;
    map.generation.recipe.purpose = MapGenerationPurpose::SettlementEnvirons;
    map.generation.recipe.landscape = options.landscape;
    map.generation.recipe.quality = options.quality;
    map.generation.recipe.seed = options.seed;
    map.generation.recipe.settlementCount = 1;
    map.generation.recipe.landmarkCount = std::max(options.generatorVersion >= 12 ? 0 : 3, options.requestedLandmarks);
    map.generation.recipe.edgeConnectionCount = std::clamp(options.edgeConnectionCount, 1, 4);
    map.generation.recipe.constraints = options.constraints;
    map.generation.recipe.knownFacts = options.knownFacts;
    map.generation.recipe.requiredElements = options.requiredElements;
    map.generation.recipe.optionalElements = options.optionalElements;
    map.generation.recipe.excludedElements = options.excludedElements;
    map.generation.recipe.lockedFeatureIds = options.lockedFeatureIds;
    map.generation.recipe.facts = options.facts;
    map.generation.graph.schemaVersion = 2;
}

