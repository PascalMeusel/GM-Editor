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

void MapGenerationOverworldBuilder::publishCrossings()
{
    mapGenReportProgress(options, 6, totalSteps, "Integrating crossings");
    int bridgeIndex = 0;
    std::vector<bool> visitedBridges(map.cells.size(), false);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            const int startIndex = mapCellIndex(map, x, y);
            if (!cell || cell->kind != MapTileKind::Bridge || !mapGenWayKindIsRoadLike(cell->wayKind) ||
                visitedBridges[static_cast<std::size_t>(startIndex)])
                continue;
            std::vector<MapCellCoordinate> component;
            std::queue<MapCellCoordinate> pending;
            pending.push({x, y});
            visitedBridges[static_cast<std::size_t>(startIndex)] = true;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                component.push_back(current);
                for (int neighborY = current.y - 1; neighborY <= current.y + 1; ++neighborY)
                    for (int neighborX = current.x - 1; neighborX <= current.x + 1; ++neighborX)
                    {
                        if (!isMapCellInside(map, neighborX, neighborY) ||
                            (neighborX == current.x && neighborY == current.y))
                            continue;
                        const int neighborIndex = mapCellIndex(map, neighborX, neighborY);
                        const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
                        if (!visitedBridges[static_cast<std::size_t>(neighborIndex)] && neighbor &&
                            neighbor->kind == MapTileKind::Bridge && mapGenWayKindIsRoadLike(neighbor->wayKind))
                        {
                            visitedBridges[static_cast<std::size_t>(neighborIndex)] = true;
                            pending.push({neighborX, neighborY});
                        }
                    }
            }
            MapGeneratedFeature bridge;
            bridge.id = mapGenV2FeatureId(options.seed, "bridge", bridgeIndex++);
            bridge.kind = MapGeneratedFeatureKind::Bridge;
            bridge.name = "River crossing";
            bridge.materialId =
                cell->wayKind == MapWayKind::StoneRoad ? "fantasy_neutral.bridge.stone" : "fantasy_neutral.bridge.wood";
            bridge.width = 1.0f;
            float roadX = 1.0f;
            float roadY = 0.0f;
            float centerX = 0.0f;
            float centerY = 0.0f;
            for (const MapCellCoordinate &coordinate : component)
            {
                centerX += static_cast<float>(coordinate.x) + 0.5f;
                centerY += static_cast<float>(coordinate.y) + 0.5f;
            }
            centerX /= static_cast<float>(component.size());
            centerY /= static_cast<float>(component.size());
            if (!mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Road, centerX, centerY,
                                               2.0f, roadX, roadY))
                mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Trail, centerX, centerY,
                                              2.0f, roadX, roadY);
            float minimumProjection = 0.0f;
            float maximumProjection = 0.0f;
            for (const MapCellCoordinate &coordinate : component)
            {
                const float projection = (static_cast<float>(coordinate.x) + 0.5f - centerX) * roadX +
                                         (static_cast<float>(coordinate.y) + 0.5f - centerY) * roadY;
                minimumProjection = std::min(minimumProjection, projection);
                maximumProjection = std::max(maximumProjection, projection);
            }
            const auto clippedBridgePoint = [&](float projection) {
                return MapFeaturePoint{std::clamp(centerX + roadX * projection, 0.0f, static_cast<float>(map.width)),
                                       std::clamp(centerY + roadY * projection, 0.0f, static_cast<float>(map.height))};
            };
            bridge.points.push_back(clippedBridgePoint(minimumProjection - 0.58f));
            bridge.points.push_back(clippedBridgePoint(maximumProjection + 0.58f));
            map.generation.graph.features.push_back(std::move(bridge));
        }
    }

    // Hydrology and roads deliberately carve a source gorge or an occasional
    // pass through the mountain raster. Build the editable range graph from
    // that final terrain so QA, atlas inheritance and rendering all observe
    // the same components rather than a stale pre-road ridge.
    mapGenV5AddWorldMountainRangeFeatures(map, options);
}

