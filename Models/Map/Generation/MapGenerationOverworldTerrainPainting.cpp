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

void MapGenerationOverworldBuilder::paintTerrain()
{
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y * map.width + x);
            float heightValue =
                mapGenV2FractalNoise(options.seed + 17u, static_cast<float>(x), static_cast<float>(y), noiseScale);
            float moistureValue =
                mapGenV2FractalNoise(options.seed + 41u, static_cast<float>(x), static_cast<float>(y), noiseScale + 3);
            if (options.landscape == MapLandscapeProfile::MountainPass ||
                options.landscape == MapLandscapeProfile::Highlands)
                heightValue += 0.12f;
            if (options.landscape == MapLandscapeProfile::ForestFrontier ||
                options.landscape == MapLandscapeProfile::LakeCountry)
                moistureValue += 0.13f;
            elevation[index] =
                heightValue +
                (1.0f - static_cast<float>(y) / std::max(1.0f, static_cast<float>(map.height - 1))) * 0.18f;
            moisture[index] = moistureValue;
            MapTileKind terrain = MapTileKind::Empty;
            const float normalizedX =
                (static_cast<float>(x) + 0.5f) / std::max(1.0f, static_cast<float>(map.width)) * 2.0f - 1.0f;
            const float normalizedY =
                (static_cast<float>(y) + 0.5f) / std::max(1.0f, static_cast<float>(map.height)) * 2.0f - 1.0f;
            const float coastlineNoise = mapGenV2FractalNoise(options.seed + 2903u, static_cast<float>(x),
                                                              static_cast<float>(y), std::max(6, noiseScale / 2));
            const float worldLandScore =
                sharedGreatRiver                 ? (options.generatorVersion >= 5
                                                        ? mapGenV5SharedRiverWorldLandScore(options.seed, normalizedX, normalizedY)
                                                        : mapGenV4SharedRiverWorldLandScore(options.seed, normalizedX, normalizedY))
                : options.generatorVersion >= 10 ? mapGenV10WorldLandScore(options.seed, normalizedX, normalizedY,
                                                                           options.waterLevel, options.landscape)
                                                 : mapGenV4WorldLandScore(options.seed, normalizedX, normalizedY);
            const bool worldOcean =
                options.generatorVersion >= 4 && options.scale == MapLevel::World &&
                worldLandScore + (coastlineNoise - 0.5f) * (options.generatorVersion >= 5 ? 0.30f : 0.42f) < 0.035f;
            const bool explicitLandform = options.generatorVersion >= 11 &&
                                          options.landform != MapLandform::Automatic &&
                                          (options.scale == MapLevel::World || options.scale == MapLevel::Region);
            const bool selectedWater =
                explicitLandform &&
                mapWorldLandformScore(options.landform, options.seed, normalizedX, normalizedY, options.waterLevel,
                                      options.generatorVersion >= 12 && options.scale == MapLevel::Region) < 0.f;
            if (explicitLandform ? selectedWater : worldOcean)
                terrain = MapTileKind::Water;
            else if (options.generatorVersion >= 5 && (options.scale == MapLevel::World || structuredRegion))
            {
                float ridgeInfluence = mapGenV5WorldMountainRidgeInfluence(options.seed, normalizedX, normalizedY, 0,
                                                                           options.mountainDensity);
                // Density thickens the primary continental range; it must not
                // silently create a second unrelated chain. A second range is
                // a compositional promise of the mountain-heavy profiles only.
                if (options.landscape == MapLandscapeProfile::MountainPass ||
                    options.landscape == MapLandscapeProfile::Highlands)
                    ridgeInfluence = std::max(
                        ridgeInfluence, mapGenV5WorldMountainRidgeInfluence(options.seed, normalizedX, normalizedY, 1,
                                                                            options.mountainDensity));
                if (options.generatorVersion >= 11 && options.landform == MapLandform::InlandLake)
                {
                    // The requested basin is structural geography. Its relief
                    // follows one rim arc instead of clipping a pre-existing
                    // diagonal mountain chain into disconnected scraps.
                    const float basin = mapWorldLandformScore(
                        options.landform, options.seed, normalizedX, normalizedY, options.waterLevel,
                        options.generatorVersion >= 12 && options.scale == MapLevel::Region);
                    const float theta = std::atan2(normalizedY, normalizedX);
                    const float direction = mapGenV2Hash01(options.seed + 417u, 3, 7) * 6.2831853f;
                    const float arc = .5f + .5f * std::cos(theta - direction);
                    const float band =
                        std::max(0.f, 1.f - std::abs(basin - .42f) / (.19f + options.mountainDensity * .0018f));
                    ridgeInfluence =
                        band * std::max(0.f, (arc - .42f) / .58f) * (.52f + options.mountainDensity * .004f);
                }
                const auto forestProvince = [&](int clusterIndex) {
                    if (options.generatorVersion < 10)
                        return mapGenV5WorldForestClusterInfluence(options.seed, normalizedX, normalizedY,
                                                                   clusterIndex);
                    const auto clusterSeed = options.seed + 4513u + static_cast<std::uint32_t>(clusterIndex * 811u);
                    const float centerX = (mapGenV2Hash01(clusterSeed, 5, 17) - .5f) * 1.18f;
                    const float centerY = (mapGenV2Hash01(clusterSeed + 7u, 19, 3) - .5f) * 1.04f;
                    const float expansion = 1.f + std::clamp(options.forestDensity, 0, 100) * .007f;
                    float sampleX = normalizedX -
                                    (islandWoodlands && clusterIndex == 1 ? secondaryForestShiftX : worldForestShiftX);
                    float sampleY = normalizedY -
                                    (islandWoodlands && clusterIndex == 1 ? secondaryForestShiftY : worldForestShiftY);
                    if (options.generatorVersion >= 11)
                    {
                        const float phase = mapGenV2Hash01(clusterSeed + 317u, 3, 11) * 6.2831853f;
                        sampleX += .065f * std::sin(sampleY * 8.f + phase) + .027f * std::sin(sampleY * 17.f - phase);
                        sampleY += .050f * std::sin(sampleX * 9.f - phase);
                    }
                    return mapGenV5WorldForestClusterInfluence(options.seed, centerX + (sampleX - centerX) / expansion,
                                                               centerY + (sampleY - centerY) / expansion, clusterIndex);
                };
                const float primaryForestInfluence = forestProvince(0);
                // Keep one dominant, readable forest province. Equal-strength
                // independent blobs were frequently split again by a river or
                // road and left several similarly sized green islands with no
                // visual hierarchy.
                const float secondaryForestInfluence = forestProvince(1) * 0.70f;
                float forestInfluence = primaryForestInfluence;
                // Secondary shapes extend the main forest province instead
                // of forming an unrelated equal-weight island elsewhere on
                // the continent. The small overlap guard still permits an
                // irregular lobe beyond the primary ellipse's hard edge.
                if (primaryForestInfluence > 0.025f || islandWoodlands)
                    forestInfluence = std::max(forestInfluence, secondaryForestInfluence);
                if (options.forestDensity >= 45 || options.landscape == MapLandscapeProfile::ForestFrontier)
                {
                    const float tertiaryForestInfluence = forestProvince(2) * 0.78f;
                    if (primaryForestInfluence > 0.025f)
                        forestInfluence = std::max(forestInfluence, tertiaryForestInfluence);
                }
                if (ridgeInfluence >= 0.30f)
                    terrain = MapTileKind::Mountain;
                else if (ridgeInfluence >= 0.10f || heightValue > 0.82f - options.mountainDensity * 0.00035f)
                    terrain = MapTileKind::Hill;
                else if (options.forestDensity > 0 &&
                         forestInfluence > std::clamp(0.44f - options.forestDensity * 0.0026f, 0.18f, 0.41f) &&
                         moistureValue > (options.generatorVersion >= 10
                                              ? .34f - std::clamp(options.forestDensity - 35, 0, 65) * .0022f
                                              : .34f))
                    terrain = MapTileKind::Forest;
                else if (moistureValue > (options.generatorVersion >= 10 ? .70f : .61f) && options.waterLevel > 44 &&
                         (options.generatorVersion < 10 || (heightValue < .54f && forestInfluence < .12f)))
                    terrain = MapTileKind::Swamp;
            }
            else if (heightValue > 0.76f - options.mountainDensity * 0.0016f)
                terrain = MapTileKind::Mountain;
            else if (heightValue > 0.63f - options.mountainDensity * 0.0007f)
                terrain = MapTileKind::Hill;
            else if (moistureValue > 0.68f - options.forestDensity * 0.0018f)
                terrain = MapTileKind::Forest;
            else if (moistureValue > 0.61f && options.waterLevel > 44)
                terrain = MapTileKind::Swamp;
            if (options.generatorVersion >= 10 && options.scale == MapLevel::Region && !structuredRegion)
            {
                const auto composed = gm::map_generation::region::terrain(
                    options.seed, x, y, map.width, map.height, options.landscape, options.mountainDensity,
                    options.forestDensity, options.waterLevel, heightValue, moistureValue);
                terrain = composed.kind;
                if (explicitLandform)
                {
                    if (selectedWater)
                        terrain = MapTileKind::Water;
                    else if (terrain == MapTileKind::Water)
                        terrain = MapTileKind::Empty;
                }
                elevation[index] = composed.elevation;
            }
            if (options.generatorVersion >= 11 && options.mountainDensity == 0 &&
                (terrain == MapTileKind::Mountain || terrain == MapTileKind::Hill))
                terrain = MapTileKind::Empty;
            if (options.generatorVersion >= 11 && options.forestDensity == 0 && terrain == MapTileKind::Forest)
                terrain = MapTileKind::Empty;
            mapGenSetCell(map, x, y, terrain,
                          terrain == MapTileKind::Forest || terrain == MapTileKind::Mountain
                              ? MapSymbolVariantMode::Continuous
                              : MapSymbolVariantMode::Auto);
            if (MapCellModel *cell = mapCellAt(map, x, y))
                cell->elevation = (explicitLandform ? selectedWater : worldOcean)
                                      ? 0
                                      : std::clamp(static_cast<int>(std::lround(elevation[index] * 20.0f)), 0, 20);
        }
    }

    if (options.generatorVersion >= 10 && (options.scale == MapLevel::World || structuredRegion) &&
        (options.forestDensity >= 55 ||
         (options.generatorVersion >= 11 && options.landform != MapLandform::Automatic && options.forestDensity > 0)))
    {
        // High requested woodland needs a geographic footprint, even when a
        // seeded lobe is cut by the coastline. Grow its existing edges into
        // usable lowland before roads and places are created, never scatter
        // replacement stamps over mountains, water or authored content.
        struct WoodlandEdge
        {
            float cost;
            int index;
            bool operator<(const WoodlandEdge &b) const
            {
                return cost > b.cost;
            }
        };
        std::priority_queue<WoodlandEdge> edges;
        std::vector<float> distance(map.cells.size(), std::numeric_limits<float>::max());
        int landCells = 0, forestCells = 0;
        for (int i = 0; i < static_cast<int>(map.cells.size()); ++i)
        {
            landCells += map.cells[i].kind != MapTileKind::Water ? 1 : 0;
            if (map.cells[i].kind == MapTileKind::Forest)
            {
                ++forestCells;
                distance[i] = 0.f;
                edges.push({0.f, i});
            }
        }
        const int target = static_cast<int>(
            landCells * options.forestDensity *
            (options.generatorVersion >= 11 && options.landform != MapLandform::Automatic ? .0028f : .0019f));
        while (!edges.empty() && forestCells < target)
        {
            const auto next = edges.top();
            edges.pop();
            auto &cell = map.cells[next.index];
            if (next.cost > distance[next.index])
                continue;
            if (cell.kind == MapTileKind::Empty || cell.kind == MapTileKind::Dirt)
            {
                const int keptElevation = cell.elevation;
                mapGenSetCell(map, next.index % map.width, next.index / map.width, MapTileKind::Forest,
                              MapSymbolVariantMode::Continuous);
                cell.elevation = keptElevation;
                ++forestCells;
            }
            if (cell.kind != MapTileKind::Forest)
                continue;
            const int x = next.index % map.width, y = next.index / map.width;
            for (auto step :
                 {MapCellCoordinate{1, 0}, MapCellCoordinate{-1, 0}, MapCellCoordinate{0, 1}, MapCellCoordinate{0, -1}})
            {
                const int nx = x + step.x, ny = y + step.y;
                if (!isMapCellInside(map, nx, ny))
                    continue;
                const int index = mapCellIndex(map, nx, ny);
                const auto &neighbor = map.cells[index];
                if (neighbor.kind != MapTileKind::Empty && neighbor.kind != MapTileKind::Dirt)
                    continue;
                const float cost = next.cost + 1.f + mapGenV2Hash01(options.seed + 349u, nx, ny) * .65f;
                if (cost < distance[index])
                {
                    distance[index] = cost;
                    edges.push({cost, index});
                }
            }
        }
    }

    if (options.generatorVersion >= 11 && (options.scale == MapLevel::World || structuredRegion) &&
        options.forestDensity >= 55)
    {
        // Broad woodlands need a small number of legible interior glades.
        // Select only dense canopy interiors, before roads or authored places
        // are projected; preserve the surrounding connected forest province.
        std::vector<MapCellCoordinate> glades;
        const float radius = std::max(1.1f, std::min(map.width, map.height) * .038f);
        const int gladeCount = 1 + std::min(2, static_cast<int>(mapGenV2Hash01(options.seed + 9029u, 0, 0) * 3.f));
        for (int ordinal = 0; ordinal < gladeCount; ++ordinal)
        {
            MapCellCoordinate best{-1, -1};
            float score = -1.f;
            for (int y = 3; y < map.height - 3; ++y)
                for (int x = 3; x < map.width - 3; ++x)
                {
                    int dense = 0;
                    for (int dy = -2; dy <= 2; ++dy)
                        for (int dx = -2; dx <= 2; ++dx)
                            dense += mapCellAt(map, x + dx, y + dy)->kind == MapTileKind::Forest ? 1 : 0;
                    if (dense < 23)
                        continue;
                    bool separated = true;
                    for (auto prior : glades)
                        separated &= std::hypot(float(x - prior.x), float(y - prior.y)) > radius * 4.f;
                    if (!separated)
                        continue;
                    const float candidate = mapGenV2Hash01(options.seed + 9011u + ordinal * 107u, x, y);
                    if (candidate > score)
                    {
                        score = candidate;
                        best = {x, y};
                    }
                }
            if (best.x < 0)
                break;
            glades.push_back(best);
            // A repeated round hole reads as a stamp. Vary the clearing's
            // footprint and orientation while retaining a small connected
            // opening within the already selected dense forest interior.
            const float localRadius = radius * (.75f + .5f * mapGenV2Hash01(options.seed + 9037u, ordinal, 0));
            const float angle = mapGenV2Hash01(options.seed + 9041u, ordinal, 0) * 6.2831853f;
            const float aspect = 1.f + 1.7f * mapGenV2Hash01(options.seed + 9049u, ordinal, 0);
            const int reach = static_cast<int>(std::ceil(localRadius * 1.2f));
            for (int dy = -reach; dy <= reach; ++dy)
                for (int dx = -reach; dx <= reach; ++dx)
                {
                    auto *cell = mapCellAt(map, best.x + dx, best.y + dy);
                    const float rotatedX = dx * std::cos(angle) - dy * std::sin(angle);
                    const float rotatedY = dx * std::sin(angle) + dy * std::cos(angle);
                    const float theta = std::atan2(rotatedY, rotatedX);
                    const float gladeRadius =
                        localRadius * (.85f + .22f * std::sin(theta * 3.f + ordinal) + .12f * std::cos(theta * 2.f));
                    if (cell && cell->kind == MapTileKind::Forest &&
                        rotatedX * rotatedX + rotatedY * rotatedY * aspect < gladeRadius * gladeRadius)
                    {
                        const int elevation = cell->elevation;
                        mapGenSetCell(map, best.x + dx, best.y + dy, MapTileKind::Empty);
                        cell->elevation = elevation;
                    }
                }
        }
    }
}

