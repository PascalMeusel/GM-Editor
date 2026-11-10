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

void MapGenerationOverworldBuilder::placeLandmarks()
{
    mapGenReportProgress(options, 7, totalSteps, "Placing landmarks");
    int placedLandmarks = 0;
    for (const MapGenerationConstraint &constraint : effectiveConstraints)
    {
        if (constraint.featureKind != MapGeneratedFeatureKind::Landmark ||
            placedLandmarks >= map.generation.recipe.landmarkCount)
            continue;
        const int requestedX = std::clamp(static_cast<int>(std::floor(constraint.x)), 1, map.width - 2);
        const int requestedY = std::clamp(static_cast<int>(std::floor(constraint.y)), 1, map.height - 2);
        int x = -1;
        int y = -1;
        const int maximumRadius = std::max(map.width, map.height);
        for (int radius = 0; radius <= maximumRadius && x < 0; ++radius)
        {
            for (int offsetY = -radius; offsetY <= radius && x < 0; ++offsetY)
            {
                for (int offsetX = -radius; offsetX <= radius; ++offsetX)
                {
                    if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                        continue;
                    const int candidateX = requestedX + offsetX;
                    const int candidateY = requestedY + offsetY;
                    const MapCellModel *cell = mapCellAt(map, candidateX, candidateY);
                    if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
                        cell->kind == MapTileKind::Bridge || mapGenRiverInfluenceCell(cell) ||
                        cell->wayKind != MapWayKind::None || mapGenIsStructureLike(cell->kind))
                        continue;
                    x = candidateX;
                    y = candidateY;
                    break;
                }
            }
        }
        if (x < 0)
            continue;
        mapGenSetCell(map, x, y, MapTileKind::Landmark, MapSymbolVariantMode::Individual);
        MapGeneratedFeature landmark;
        landmark.id =
            constraint.id.empty() ? mapGenV2FeatureId(options.seed, "landmark", placedLandmarks) : constraint.id;
        landmark.kind = MapGeneratedFeatureKind::Landmark;
        landmark.name = constraint.label.empty()
                            ? (options.generatorVersion >= 4
                                   ? mapGenV4LandmarkName(options.seed, placedLandmarks, options.nameCulture)
                                   : "Landmark " + std::to_string(placedLandmarks + 1))
                            : constraint.label;
        if (options.generatorVersion >= 10 && options.scale == MapLevel::Region && constraint.label.empty())
            landmark.name = gm::map_generation::region::landmarkName(landmark.name, options.seed, placedLandmarks,
                                                                     options.contentLanguage);
        landmark.materialId = "fantasy_neutral.landmark";
        landmark.locked = constraint.locked;
        landmark.points.push_back({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
        map.generation.graph.features.push_back(std::move(landmark));
        ++placedLandmarks;
    }
    for (int attempt = 0; attempt < 200 && placedLandmarks < map.generation.recipe.landmarkCount; ++attempt)
    {
        const int x = mapGenRandomBetween(rng, 2, map.width - 3);
        const int y = mapGenRandomBetween(rng, 2, map.height - 3);
        MapCellModel *cell = mapCellAt(map, x, y);
        if (!cell || cell->movementCost >= 99 || cell->wayKind != MapWayKind::None ||
            mapGenNearKind(map, x, y, MapTileKind::Settlement, 3))
            continue;
        if (options.generatorVersion >= 10 && (options.scale == MapLevel::World || options.scale == MapLevel::Region) &&
            (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Bridge ||
             cell->kind == MapTileKind::Landmark || mapWayKindIsWaterLike(cell->wayKind) ||
             mapGenNearKind(map, x, y, MapTileKind::Water, 1) || mapGenNearKind(map, x, y, MapTileKind::Bridge, 1) ||
             mapGenNearKind(map, x, y, MapTileKind::Landmark, std::clamp(std::min(map.width, map.height) / 10, 3, 6))))
            continue;
        mapGenSetCell(map, x, y, MapTileKind::Landmark, MapSymbolVariantMode::Individual);
        MapGeneratedFeature landmark;
        landmark.id = mapGenV2FeatureId(options.seed, "landmark", placedLandmarks);
        landmark.kind = MapGeneratedFeatureKind::Landmark;
        landmark.name = options.generatorVersion >= 4
                            ? mapGenV4LandmarkName(options.seed, placedLandmarks, options.nameCulture)
                            : "Landmark " + std::to_string(placedLandmarks + 1);
        if (options.generatorVersion >= 10 && options.scale == MapLevel::Region)
            landmark.name = gm::map_generation::region::landmarkName(landmark.name, options.seed, placedLandmarks,
                                                                     options.contentLanguage);
        landmark.materialId = options.generatorVersion >= 12 && options.scale == MapLevel::Region
                                  ? gm::map_generation::region::landmarkMaterial(options.seed, placedLandmarks)
                                  : "fantasy_neutral.landmark";
        landmark.points.push_back({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
        map.generation.graph.features.push_back(std::move(landmark));
        ++placedLandmarks;
    }
}

