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

void MapGenerationOverworldBuilder::prepareTerrain()
{
    mapGenReportProgress(options, 2, totalSteps, "Shaping terrain");
    elevation.assign(map.cells.size(), 0.0f);
    moisture.assign(map.cells.size(), 0.0f);
    noiseScale = std::max(8, std::min(map.width, map.height) / 2);
    worldForestShiftX = 0.f;
    worldForestShiftY = 0.f;
    secondaryForestShiftX = 0.f;
    secondaryForestShiftY = 0.f;
    islandWoodlands =
        options.generatorVersion >= 11 && options.landform == MapLandform::Islands && options.forestDensity >= 45;
    if (options.generatorVersion >= 10 && (options.scale == MapLevel::World || structuredRegion) &&
        options.forestDensity > 0)
    {
        const auto forestSeed = options.seed + 4513u;
        const float originalX = (mapGenV2Hash01(forestSeed, 5, 17) - .5f) * 1.18f;
        const float originalY = (mapGenV2Hash01(forestSeed + 7u, 19, 3) - .5f) * 1.04f;
        float best = std::numeric_limits<float>::max();
        for (int fy = 2; fy < map.height - 2; ++fy)
            for (int fx = 2; fx < map.width - 2; ++fx)
            {
                const float nx = (fx + .5f) / map.width * 2.f - 1.f, ny = (fy + .5f) / map.height * 2.f - 1.f;
                const float landScore =
                    options.generatorVersion >= 11 && options.landform != MapLandform::Automatic
                        ? mapWorldLandformScore(options.landform, options.seed, nx, ny, options.waterLevel,
                                                options.generatorVersion >= 12 && options.scale == MapLevel::Region)
                    : sharedGreatRiver
                        ? mapGenV5SharedRiverWorldLandScore(options.seed, nx, ny)
                        : mapGenV10WorldLandScore(options.seed, nx, ny, options.waterLevel, options.landscape);
                if (landScore < .24f ||
                    mapGenV5WorldMountainRidgeInfluence(options.seed, nx, ny, 0, options.mountainDensity) > .10f)
                    continue;
                if ((options.landscape == MapLandscapeProfile::Highlands ||
                     options.landscape == MapLandscapeProfile::MountainPass) &&
                    mapGenV5WorldMountainRidgeInfluence(options.seed, nx, ny, 1, options.mountainDensity) > .10f)
                    continue;
                const float height = mapGenV2FractalNoise(options.seed + 17u, static_cast<float>(fx),
                                                          static_cast<float>(fy), noiseScale) +
                                     ((options.landscape == MapLandscapeProfile::Highlands ||
                                       options.landscape == MapLandscapeProfile::MountainPass)
                                          ? .12f
                                          : 0.f);
                if (height > .76f)
                    continue;
                const float forestMoisture = mapGenV2FractalNoise(options.seed + 41u, static_cast<float>(fx),
                                                                  static_cast<float>(fy), noiseScale + 3) +
                                             ((options.landscape == MapLandscapeProfile::ForestFrontier ||
                                               options.landscape == MapLandscapeProfile::LakeCountry)
                                                  ? .13f
                                                  : 0.f);
                if (forestMoisture < .30f)
                    continue;
                const float cost = std::hypot(nx - originalX, ny - originalY) + std::max(0.f, .38f - landScore) * .35f;
                if (cost < best)
                {
                    best = cost;
                    worldForestShiftX = nx - originalX;
                    worldForestShiftY = ny - originalY;
                }
            }
        // Preserve the seed's entire province shape; translate it once onto
        // usable land instead of stamping replacement trees at random cells.
        secondaryForestShiftX = worldForestShiftX;
        secondaryForestShiftY = worldForestShiftY;
        if (islandWoodlands)
        {
            const auto secondarySeed = options.seed + 4513u + 811u;
            const float originalSecondaryX = (mapGenV2Hash01(secondarySeed, 5, 17) - .5f) * 1.18f;
            const float originalSecondaryY = (mapGenV2Hash01(secondarySeed + 7u, 19, 3) - .5f) * 1.04f;
            const float primaryX = originalX + worldForestShiftX, primaryY = originalY + worldForestShiftY;
            float bestSecondary = std::numeric_limits<float>::max();
            for (int fy = 2; fy < map.height - 2; ++fy)
                for (int fx = 2; fx < map.width - 2; ++fx)
                {
                    const float nx = (fx + .5f) / map.width * 2.f - 1.f, ny = (fy + .5f) / map.height * 2.f - 1.f;
                    if (mapWorldLandformScore(options.landform, options.seed, nx, ny, options.waterLevel,
                                              options.generatorVersion >= 12 && options.scale == MapLevel::Region) <
                            .23f ||
                        std::hypot(nx - primaryX, ny - primaryY) < .70f ||
                        mapGenV5WorldMountainRidgeInfluence(options.seed, nx, ny, 0, options.mountainDensity) > .10f)
                        continue;
                    if ((options.landscape == MapLandscapeProfile::Highlands ||
                         options.landscape == MapLandscapeProfile::MountainPass) &&
                        mapGenV5WorldMountainRidgeInfluence(options.seed, nx, ny, 1, options.mountainDensity) > .10f)
                        continue;
                    const float cost = std::hypot(nx - originalSecondaryX, ny - originalSecondaryY);
                    if (cost < bestSecondary)
                    {
                        bestSecondary = cost;
                        secondaryForestShiftX = nx - originalSecondaryX;
                        secondaryForestShiftY = ny - originalSecondaryY;
                    }
                }
        }
    }
}

