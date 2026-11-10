#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
#include "Models/Map/Generation/MapGenerationOverworldBuilder.hpp"

MapGenerationOverworldBuilder::MapGenerationOverworldBuilder(MapGenerationResult &result,
                                                             const MapGenerationOptions &options,
                                                             std::mt19937 &rng, int totalSteps)
    : map(result.map), options(options), rng(rng), totalSteps(totalSteps)
{
}

void MapGenerationOverworldBuilder::run()
{
    initializeRecipe();
    prepareTerrain();
    paintTerrain();
    establishLakes();
    traceHydrology();
    placeSettlements();
    prepareRoadNetwork();
    growRoadNetwork();
    addSecondaryRoadLoops();
    connectSettlementApproaches();
    addRegionalRiverCrossing();
    publishCrossings();
    placeLandmarks();
    finalizeMap();
}

void MapGenerationOverworldBuilder::initializeRecipe()
{

    structuredRegion = options.generatorVersion >= 11 && options.scale == MapLevel::Region &&
                                  options.landform != MapLandform::Automatic;
    sharedGreatRiver = options.generatorVersion >= 4 && mapGenV4RequestsSharedGreatRiver(options);
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
    map.generation.recipe.generatorVersion = std::max(2, options.generatorVersion);
    map.generation.recipe.recipeVersion = std::max(1, options.recipeVersion);
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
    map.generation.recipe.seed = options.seed;
    map.generation.recipe.purpose = options.purpose;
    map.generation.recipe.landscape = options.landscape;
    map.generation.recipe.quality = options.quality;
    map.generation.recipe.settlementCount =
        options.requestedSettlements >= 0
            ? options.requestedSettlements
            : std::max(2, map.width * map.height * mapGenClampPercent(options.settlementDensity) / 2400);
    if (options.generatorVersion >= 5 && options.scale == MapLevel::World && !options.nationNames.empty())
    {
        // A named nation without any visible principal place makes the world
        // overview feel unfinished and can only yield a one-cell decorative
        // road at the sole named capital. Fill that missing high-level context
        // with one editable, automatically named settlement per nation.
        map.generation.recipe.settlementCount =
            std::max(map.generation.recipe.settlementCount, static_cast<int>(options.nationNames.size()));
    }
    map.generation.recipe.landmarkCount = std::max(0, options.requestedLandmarks);
    map.generation.recipe.edgeConnectionCount = std::clamp(options.edgeConnectionCount, 0, 4);
    map.generation.recipe.constraints = options.constraints;
    map.generation.recipe.knownFacts = options.knownFacts;
    map.generation.recipe.requiredElements = options.requiredElements;
    if (sharedGreatRiver &&
        std::find(map.generation.recipe.requiredElements.begin(), map.generation.recipe.requiredElements.end(),
                  "Main river") == map.generation.recipe.requiredElements.end())
        map.generation.recipe.requiredElements.push_back("Main river");
    map.generation.recipe.optionalElements = options.optionalElements;
    map.generation.recipe.excludedElements = options.excludedElements;
    map.generation.recipe.lockedFeatureIds = options.lockedFeatureIds;
    map.generation.recipe.facts = options.facts;
    map.generation.graph.schemaVersion = 2;
    const char *recipeIds[] = {"river_valley", "forest_frontier", "highlands", "mountain_pass", "lake_country"};
    map.generation.recipe.recipeId = recipeIds[std::clamp(static_cast<int>(options.landscape), 0, 4)];
}

