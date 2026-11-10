#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
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

int mapGenV2SettlementSuitabilityPercent(const MapModel &map)
{
    int settlements = 0;
    int suitabilityTotal = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind != MapTileKind::Settlement)
                continue;
            ++settlements;
            const bool connected = mapGenCellHasStructureApproach(map, x, y);
            int usableNeighbors = 0;
            int consideredNeighbors = 0;
            for (int offsetY = -1; offsetY <= 1; ++offsetY)
            {
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    if (offsetX == 0 && offsetY == 0)
                        continue;
                    const MapCellModel *neighbor = mapCellAt(map, x + offsetX, y + offsetY);
                    if (!neighbor)
                        continue;
                    ++consideredNeighbors;
                    if (neighbor->movementCost < 99 && neighbor->kind != MapTileKind::Water &&
                        neighbor->kind != MapTileKind::Lava && neighbor->kind != MapTileKind::Pit &&
                        neighbor->kind != MapTileKind::Chasm)
                        ++usableNeighbors;
                }
            }

            // Water, defensible terrain and productive woodland are useful
            // geographic contexts. A connected inter-settlement road is also
            // a legitimate reason for a plains town to exist, so do not mark
            // every trade-route settlement as geographically arbitrary.
            const bool inheritedSiteContext = map.generation.recipe.purpose == MapGenerationPurpose::SettlementEnvirons;
            const bool geographicContext =
                mapGenNearKind(map, x, y, MapTileKind::Water, 5) || mapGenNearKind(map, x, y, MapTileKind::Forest, 4) ||
                mapGenNearKind(map, x, y, MapTileKind::Hill, 4) || mapGenNearKind(map, x, y, MapTileKind::Mountain, 5);
            const bool tradeContext = connected;

            int settlementScore = connected ? 55 : 0;
            settlementScore += consideredNeighbors == 0
                                   ? 0
                                   : std::clamp(usableNeighbors * 25 / std::max(1, consideredNeighbors), 0, 25);
            if (inheritedSiteContext || geographicContext || tradeContext)
                settlementScore += 20;
            suitabilityTotal += std::clamp(settlementScore, 0, 100);
        }
    }
    int score = settlements == 0 ? 100 : suitabilityTotal / settlements;
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World)
    {
        std::vector<const MapGeneratedFeature *> territories;
        std::vector<MapFeaturePoint> settlementPoints;
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.closed &&
                feature.materialId.rfind("fantasy_neutral.territory.", 0) == 0)
                territories.push_back(&feature);
            else if (feature.kind == MapGeneratedFeatureKind::Settlement && !feature.points.empty())
                settlementPoints.push_back(feature.points.front());
        }
        if (territories.size() > 1 && settlementPoints.size() >= territories.size())
        {
            int represented = 0;
            for (const MapGeneratedFeature *territory : territories)
                represented += std::any_of(settlementPoints.begin(), settlementPoints.end(),
                                           [&](const MapFeaturePoint &point) {
                                               return mapGenV4PointInsidePolygon(point, territory->points);
                                           })
                                   ? 1
                                   : 0;
            // When enough principal places exist, every named nation needs a
            // visible anchor. Otherwise the political overview is technically
            // valid but unusable for deciding where play can begin.
            score = std::min(score, represented * 100 / static_cast<int>(territories.size()));
        }
    }
    return score;
}

int mapGenV5WorldRoadTerrainPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 100;

    int totalSamples = 0;
    int coastSamples = 0;
    int wetSamples = 0;
    int longestCoastRun = 0;
    int longestParallelMountainRun = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if ((feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) ||
            feature.points.size() < 2)
            continue;

        std::vector<MapFeaturePoint> samples;
        for (std::size_t pointIndex = 1; pointIndex < feature.points.size(); ++pointIndex)
        {
            const MapFeaturePoint &start = feature.points[pointIndex - 1];
            const MapFeaturePoint &end = feature.points[pointIndex];
            const float deltaX = end.x - start.x;
            const float deltaY = end.y - start.y;
            const int segmentSamples =
                std::max(1, static_cast<int>(std::ceil(std::sqrt(deltaX * deltaX + deltaY * deltaY) * 4.0f)));
            for (int sample = pointIndex == 1 ? 0 : 1; sample <= segmentSamples; ++sample)
            {
                const float amount = static_cast<float>(sample) / static_cast<float>(segmentSamples);
                samples.push_back({start.x + deltaX * amount, start.y + deltaY * amount});
            }
        }

        int coastRun = 0;
        int parallelMountainRun = 0;
        for (std::size_t sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex)
        {
            const MapFeaturePoint &point = samples[sampleIndex];
            const int x = std::clamp(static_cast<int>(std::floor(point.x)), 0, map.width - 1);
            const int y = std::clamp(static_cast<int>(std::floor(point.y)), 0, map.height - 1);
            const MapCellModel *cell = mapCellAt(map, x, y);
            ++totalSamples;
            if (mapGenV5WorldOpenWaterAt(map, x, y))
                ++wetSamples;

            // Permit the final two cells of a coastal town's approach, but a
            // road may not follow the shoreline for a visible stretch. The
            // renderer's shoulder/blend needs one dry cell on either side.
            const bool endpointGrace = sampleIndex <= 8 || sampleIndex + 9 >= samples.size();
            const bool exposedCoast = !endpointGrace && cell && cell->kind != MapTileKind::Bridge &&
                                      mapGenV5WorldOpenWaterDistance(map, x, y, 1) <= 1;
            coastRun = exposedCoast ? coastRun + 1 : 0;
            coastSamples += exposedCoast ? 1 : 0;
            longestCoastRun = std::max(longestCoastRun, coastRun);

            float mountainX = 0.0f;
            float mountainY = 0.0f;
            const bool nearMountainRange =
                mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::MountainRange, point.x,
                                              point.y, 1.8f, mountainX, mountainY);
            float roadX = 0.0f;
            float roadY = 0.0f;
            if (sampleIndex > 0 && sampleIndex + 1 < samples.size())
            {
                roadX = samples[sampleIndex + 1].x - samples[sampleIndex - 1].x;
                roadY = samples[sampleIndex + 1].y - samples[sampleIndex - 1].y;
            }
            const float roadLength = std::sqrt(roadX * roadX + roadY * roadY);
            const bool parallelMountain = nearMountainRange && roadLength > 0.001f &&
                                          std::abs((roadX * mountainX + roadY * mountainY) / roadLength) > 0.82f;
            parallelMountainRun = parallelMountain ? parallelMountainRun + 1 : 0;
            longestParallelMountainRun = std::max(longestParallelMountainRun, parallelMountainRun);
        }
    }
    if (wetSamples > 0)
        return 0;
    // Four quarter-cell samples equal one world raster cell. A short contact
    // is a coastal approach or mountain pass; a longer run is a visible
    // cartographic overlap and must be regenerated.
    if (longestCoastRun > 16 || (totalSamples > 0 && coastSamples * 100 > totalSamples * 18))
        return 55;
    if (longestParallelMountainRun > 20)
        return 65;
    return 100;
}

int mapGenV5WorldRoadNetworkPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 ||
        (map.atlasLevel != MapLevel::World &&
         !(map.generation.recipe.generatorVersion >= 11 && map.atlasLevel == MapLevel::Region &&
           map.generation.recipe.landform == MapLandform::Islands)))
        return 100;
    int settlements = 0;
    int roadFeatures = 0;
    int sharpTurns = 0;
    float roadLength = 0.0f;
    float minimumX = static_cast<float>(map.width);
    float maximumX = 0.0f;
    float minimumY = static_cast<float>(map.height);
    float maximumY = 0.0f;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        settlements += feature.kind == MapGeneratedFeatureKind::Settlement ? 1 : 0;
        if ((feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) ||
            feature.points.size() < 2)
            continue;
        ++roadFeatures;
        for (std::size_t index = 0; index < feature.points.size(); ++index)
        {
            minimumX = std::min(minimumX, feature.points[index].x);
            maximumX = std::max(maximumX, feature.points[index].x);
            minimumY = std::min(minimumY, feature.points[index].y);
            maximumY = std::max(maximumY, feature.points[index].y);
            if (index > 0)
            {
                const float dx = feature.points[index].x - feature.points[index - 1].x;
                const float dy = feature.points[index].y - feature.points[index - 1].y;
                roadLength += std::sqrt(dx * dx + dy * dy);
            }
            if (index > 0 && index + 1 < feature.points.size())
            {
                const float incomingX = feature.points[index].x - feature.points[index - 1].x;
                const float incomingY = feature.points[index].y - feature.points[index - 1].y;
                const float outgoingX = feature.points[index + 1].x - feature.points[index].x;
                const float outgoingY = feature.points[index + 1].y - feature.points[index].y;
                const float divisor = std::sqrt((incomingX * incomingX + incomingY * incomingY) *
                                                (outgoingX * outgoingX + outgoingY * outgoingY));
                if (divisor > 0.0001f && (incomingX * outgoingX + incomingY * outgoingY) / divisor < 0.18f)
                    ++sharpTurns;
            }
        }
    }
    if (map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.routeDensity == 0)
        return roadFeatures == 0 ? 100 : 40;
    if (settlements < 2)
        return 100;
    int requiredEdges = std::max(1, settlements - 1);
    const bool islands =
        map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.landform == MapLandform::Islands;
    if (islands)
    {
        // Separate islands need separate road trees. Requiring N-1 roads
        // across N settlements globally would demand bridges across ocean.
        std::vector<int> component(map.cells.size(), -1);
        int count = 0;
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const int start = mapCellIndex(map, x, y);
                if (component[start] >= 0 || mapGenV5WorldOpenWaterAt(map, x, y))
                    continue;
                std::vector<int> pending{start};
                component[start] = count;
                for (std::size_t i = 0; i < pending.size(); ++i)
                {
                    const int cx = pending[i] % map.width, cy = pending[i] / map.width;
                    for (auto d : {MapCellCoordinate{1, 0}, MapCellCoordinate{-1, 0}, MapCellCoordinate{0, 1},
                                   MapCellCoordinate{0, -1}})
                    {
                        const int nx = cx + d.x, ny = cy + d.y;
                        if (!isMapCellInside(map, nx, ny))
                            continue;
                        const int next = mapCellIndex(map, nx, ny);
                        if (component[next] >= 0 || mapGenV5WorldOpenWaterAt(map, nx, ny))
                            continue;
                        component[next] = count;
                        pending.push_back(next);
                    }
                }
                ++count;
            }
        std::vector<int> occupied(count, 0);
        for (const auto &f : map.generation.graph.features)
            if (f.kind == MapGeneratedFeatureKind::Settlement && !f.points.empty())
            {
                const int x = static_cast<int>(f.points[0].x), y = static_cast<int>(f.points[0].y);
                if (isMapCellInside(map, x, y))
                {
                    const int c = component[mapCellIndex(map, x, y)];
                    if (c >= 0)
                        ++occupied[c];
                }
            }
        requiredEdges = 0;
        for (int population : occupied)
            requiredEdges += std::max(0, population - 1);
        if (requiredEdges == 0)
            return 100;
    }
    const int edgeScore = std::clamp(roadFeatures * 100 / requiredEdges, 0, 100);
    const float requiredLength = islands ? static_cast<float>(requiredEdges) * 2.f
                                         : std::max(6.0f, static_cast<float>(std::max(map.width, map.height)) * 0.30f);
    const int lengthScore = std::clamp(static_cast<int>(std::lround(roadLength / requiredLength * 100.0f)), 0, 100);
    const float normalizedSpan = std::max((maximumX - minimumX) / std::max(1.0f, static_cast<float>(map.width)),
                                          (maximumY - minimumY) / std::max(1.0f, static_cast<float>(map.height)));
    const int spanScore =
        std::clamp(static_cast<int>(std::lround(normalizedSpan / (islands ? 0.12f : 0.30f) * 100.0f)), 0, 100);
    const int geometryScore = std::clamp(100 - sharpTurns * 35, 0, 100);
    return std::min({edgeScore, lengthScore, spanScore, geometryScore});
}

