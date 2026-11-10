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

std::vector<MapCellCoordinate> mapGenV4BuildHydrology(MapModel &map, const MapGenerationOptions &options,
                                                      const std::vector<float> &elevation)
{
    const bool sharedGreatRiver = mapGenV4RequestsSharedGreatRiver(options);
    if (sharedGreatRiver && options.scale == MapLevel::World)
        return mapGenV4BuildSharedGreatRiver(map, options, elevation);
    const int riverCount =
        options.generatorVersion >= 11 && options.waterLevel == 0
            ? 0
            : std::clamp(1 + options.waterLevel / 38 + (options.landscape == MapLandscapeProfile::RiverValley ? 1 : 0),
                         1, 3);
    std::vector<MapCellCoordinate> primary;
    std::vector<MapCellCoordinate> sources;
    for (int riverIndex = 0; riverIndex < riverCount; ++riverIndex)
    {
        MapCellCoordinate source{map.width / 2, map.height / 2};
        float sourceScore = -1000.0f;
        for (int y = 2; y < map.height - 2; ++y)
        {
            for (int x = 2; x < map.width - 2; ++x)
            {
                const MapCellModel *sourceCell = mapCellAt(map, x, y);
                if (!sourceCell || sourceCell->kind == MapTileKind::Water)
                    continue;
                bool nearOpenWater = false;
                for (int offsetY = -3; offsetY <= 3 && !nearOpenWater; ++offsetY)
                    for (int offsetX = -3; offsetX <= 3; ++offsetX)
                    {
                        const MapCellModel *neighbor = mapCellAt(map, x + offsetX, y + offsetY);
                        if (neighbor && neighbor->kind == MapTileKind::Water && neighbor->wayKind != MapWayKind::River)
                        {
                            nearOpenWater = true;
                            break;
                        }
                    }
                if (nearOpenWater)
                    continue;
                if (options.generatorVersion >= 10 &&
                    (options.scale == MapLevel::World || options.scale == MapLevel::Region) && riverIndex > 0)
                {
                    const int minimumHeadwaterSeparation = std::max(5, std::min(map.width, map.height) * 28 / 100);
                    if (std::any_of(sources.begin(), sources.end(), [&](const auto existing) {
                            return std::abs(existing.x - x) + std::abs(existing.y - y) < minimumHeadwaterSeparation;
                        }))
                        continue;
                    bool nearRiver = false;
                    for (int dy = -3; dy <= 3 && !nearRiver; ++dy)
                        for (int dx = -3; dx <= 3; ++dx)
                            if (const auto *riverNeighbor = mapCellAt(map, x + dx, y + dy);
                                riverNeighbor && riverNeighbor->wayKind == MapWayKind::River)
                            {
                                nearRiver = true;
                                break;
                            }
                    if (nearRiver)
                        continue;
                }
                float separation = 1.0f;
                for (const MapCellCoordinate existing : sources)
                    separation =
                        std::min(separation, static_cast<float>(std::abs(existing.x - x) + std::abs(existing.y - y)) /
                                                 std::max(1.0f, static_cast<float>(map.width + map.height) * 0.25f));
                const float interiorSource =
                    options.generatorVersion >= 10 && options.scale == MapLevel::Region && riverIndex == 0
                        ? std::min(1.0f, static_cast<float>(std::min({x, y, map.width - 1 - x, map.height - 1 - y})) /
                                             std::max(1.0f, std::min(map.width, map.height) * .25f)) *
                              .18f
                        : 0.0f;
                float highlandHeadwater = 0.f;
                if (options.generatorVersion >= 11 &&
                    (options.scale == MapLevel::World || options.scale == MapLevel::Region))
                {
                    // The illustrated ridges are authored geographic relief,
                    // not the same field as broad fractal elevation. Prefer
                    // their foothills for visible headwaters without placing
                    // the river source directly under a mountain symbol.
                    int nearbyPeaks = 0;
                    for (int dy = -2; dy <= 2; ++dy)
                        for (int dx = -2; dx <= 2; ++dx)
                            if (const auto *nearby = mapCellAt(map, x + dx, y + dy);
                                nearby && nearby->kind == MapTileKind::Mountain)
                                ++nearbyPeaks;
                    highlandHeadwater = std::min(.24f, nearbyPeaks * .035f);
                    if (sourceCell->kind == MapTileKind::Mountain)
                        highlandHeadwater -= .20f;
                }
                const float score = highlandHeadwater + interiorSource +
                                    elevation[static_cast<std::size_t>(mapCellIndex(map, x, y))] * 0.82f +
                                    separation * 0.16f +
                                    mapGenV2Hash01(options.seed + 1907u + riverIndex * 53u, x, y) * 0.12f;
                if (score > sourceScore)
                {
                    sourceScore = score;
                    source = {x, y};
                }
            }
        }
        if (sourceScore <= -999.0f)
            continue;
        sources.push_back(source);

        MapCellCoordinate outlet{-1, -1};
        float outletScore = std::numeric_limits<float>::max();
        auto considerOutlet = [&](int x, int y, bool requireShoreline) {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (requireShoreline && (!cell || cell->kind != MapTileKind::Water || cell->wayKind == MapWayKind::River))
                return;
            if (options.generatorVersion >= 10 && options.scale == MapLevel::Region && riverIndex > 0 &&
                !requireShoreline)
            {
                // Distinct headwaters can still become parallel gutters at
                // adjacent edge outlets. Keep independent edge watersheds
                // apart; receiving lakes and real confluences remain valid.
                const float separation = std::max(5.0f, std::min(map.width, map.height) * .22f);
                for (const auto &feature : map.generation.graph.features)
                {
                    if (feature.kind != MapGeneratedFeatureKind::River || feature.points.empty())
                        continue;
                    const auto &mouth = feature.points.back();
                    if (mouth.x > 1.5f && mouth.y > 1.5f && mouth.x < map.width - 1.5f && mouth.y < map.height - 1.5f)
                        continue;
                    if (std::hypot(mouth.x - (x + .5f), mouth.y - (y + .5f)) < separation)
                        return;
                }
            }
            if (requireShoreline)
            {
                bool touchesLand = false;
                constexpr MapCellCoordinate cardinal[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
                for (const MapCellCoordinate offset : cardinal)
                {
                    const MapCellModel *neighbor = mapCellAt(map, x + offset.x, y + offset.y);
                    touchesLand = touchesLand || (neighbor && neighbor->kind != MapTileKind::Water);
                }
                if (!touchesLand)
                    return;
            }
            const float sourceDistance = static_cast<float>(std::abs(source.x - x) + std::abs(source.y - y));
            const bool regionalPrimary =
                options.generatorVersion >= 10 && options.scale == MapLevel::Region && riverIndex == 0;
            const bool inlandLakeOutlet =
                options.generatorVersion >= 11 && options.landform == MapLandform::InlandLake && requireShoreline;
            // A headwater beside an inland lake should drain into its nearby
            // shore. The old minimum river span forced it around the basin.
            const float minimumSpan = inlandLakeOutlet ? .10f : (regionalPrimary && !requireShoreline ? .55f : .30f);
            if (sourceDistance < std::min(map.width, map.height) * minimumSpan)
                return;
            const float score = elevation[static_cast<std::size_t>(mapCellIndex(map, x, y))] * 1.5f +
                                sourceDistance * (inlandLakeOutlet ? .065f : (regionalPrimary ? .007f : .018f)) +
                                mapGenV2Hash01(options.seed + 2017u + riverIndex * 71u, x, y) * 0.16f;
            if (score < outletScore)
            {
                outletScore = score;
                outlet = {x, y};
            }
        };
        // Prefer an actual coast or lake shore at any map position.  Regional
        // maps without an existing water body still use their lowest edge.
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
                considerOutlet(x, y, true);
        if (outlet.x < 0)
        {
            for (int x = 0; x < map.width; ++x)
            {
                considerOutlet(x, 0, false);
                considerOutlet(x, map.height - 1, false);
            }
            for (int y = 1; y < map.height - 1; ++y)
            {
                considerOutlet(0, y, false);
                considerOutlet(map.width - 1, y, false);
            }
        }
        if (outlet.x < 0)
            continue;
        std::vector<MapCellCoordinate> path = mapGenV4FindDrainagePath(
            map, elevation, source, outlet, options.seed + 2311u + static_cast<std::uint32_t>(riverIndex * 97),
            options.generatorVersion >= 10 && (options.scale == MapLevel::World || options.scale == MapLevel::Region));
        if (path.empty())
            continue;
        MapFeaturePoint geometricConfluence;
        const bool joinedGeometrically = mapGenV6RegionJoinFirstRiverConfluence(map, path, geometricConfluence);
        // A river ends when it reaches an existing lake or ocean. Continuing
        // the spline across the water body to an arbitrary map edge produced
        // the conspicuous straight blue bars seen in the V3 review corpus.
        std::vector<MapCellCoordinate> drainedPath;
        drainedPath.reserve(path.size());
        bool joinsExistingRiver = false;
        for (const MapCellCoordinate coordinate : path)
        {
            const MapCellModel *before = mapCellAt(map, coordinate.x, coordinate.y);
            const bool reachedWaterBody = !drainedPath.empty() && before && before->kind == MapTileKind::Water &&
                                          before->wayKind != MapWayKind::River;
            const bool reachedExistingRiver = !drainedPath.empty() && before && before->wayKind == MapWayKind::River;
            drainedPath.push_back(coordinate);
            if (reachedWaterBody || reachedExistingRiver)
            {
                joinsExistingRiver = reachedExistingRiver;
                break;
            }
        }
        path = std::move(drainedPath);
        const bool reachedGeometricConfluence = joinedGeometrically && !path.empty() &&
                                                path.back().x == static_cast<int>(std::floor(geometricConfluence.x)) &&
                                                path.back().y == static_cast<int>(std::floor(geometricConfluence.y));
        if (reachedGeometricConfluence)
            joinsExistingRiver = true;
        if (options.generatorVersion >= 10 && (options.scale == MapLevel::World || options.scale == MapLevel::Region) &&
            riverIndex > 0)
        {
            // Omit optional near-duplicate branches before rasterizing. A
            // connected required river uses the separate fact-preserving path.
            int crowdedSamples = 0;
            for (std::size_t sample = 0; sample + 3 < path.size(); ++sample)
            {
                bool crowded = false;
                for (int dy = -2; dy <= 2 && !crowded; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                        if (const auto *riverNeighbor = mapCellAt(map, path[sample].x + dx, path[sample].y + dy);
                            riverNeighbor && riverNeighbor->wayKind == MapWayKind::River)
                        {
                            crowded = true;
                            break;
                        }
                crowdedSamples += crowded ? 1 : 0;
            }
            if (path.size() < 5 || crowdedSamples * 100 > static_cast<int>(path.size()) * 32)
                continue;
        }
        const MapCellCoordinate terminal = path.back();
        const MapCellModel *terminalCell = mapCellAt(map, terminal.x, terminal.y);
        const int terminalElevation = joinsExistingRiver && terminalCell ? terminalCell->elevation : 0;
        const int sampledSourceElevation =
            std::clamp(static_cast<int>(std::lround(
                           elevation[static_cast<std::size_t>(mapCellIndex(map, source.x, source.y))] * 20.0f)),
                       1, 20);
        const int sourceElevation = std::max(sampledSourceElevation, terminalElevation);
        for (std::size_t pathIndex = 0; pathIndex < path.size(); ++pathIndex)
        {
            const MapCellCoordinate coordinate = path[pathIndex];
            const bool sharedConfluence = joinsExistingRiver && pathIndex + 1 == path.size();
            // Do not reclassify the receiving lake/ocean as a river cell: the
            // terrain renderer would reconstruct it as land, retreating the
            // shoreline and leaving a visibly disconnected mouth.
            const bool openWaterMouth = options.generatorVersion >= 8 && pathIndex + 1 == path.size() && terminalCell &&
                                        terminalCell->kind == MapTileKind::Water &&
                                        terminalCell->wayKind != MapWayKind::River;
            if (!sharedConfluence && !openWaterMouth)
                mapGenSetRiverCell(map, coordinate.x, coordinate.y);
            if (MapCellModel *riverCell = mapCellAt(map, coordinate.x, coordinate.y))
            {
                const float progress =
                    path.size() <= 1 ? 1.0f : static_cast<float>(pathIndex) / static_cast<float>(path.size() - 1);
                riverCell->elevation = std::clamp(
                    static_cast<int>(std::lround(static_cast<float>(sourceElevation) +
                                                 static_cast<float>(terminalElevation - sourceElevation) * progress)),
                    0, 20);
            }
        }
        mapGenV2AddPathFeature(
            map, MapGeneratedFeatureKind::River, mapGenV2FeatureId(options.seed, "river", riverIndex),
            riverIndex == 0 ? "Main river" : "River branch " + std::to_string(riverIndex + 1),
            riverIndex == 0 ? "fantasy_neutral.water.river" : "fantasy_neutral.water.creek",
            riverIndex == 0 && (options.waterLevel > 48 || sharedGreatRiver) ? 1.5f : (riverIndex == 0 ? 1.0f : 0.68f),
            path);
        if (reachedGeometricConfluence)
            map.generation.graph.features.back().points.back() = geometricConfluence;
        if (sharedGreatRiver && riverIndex == 0 && !map.generation.graph.features.empty())
            map.generation.graph.features.back().locked = true;
        if (primary.empty())
            primary = path;
    }
    mapGenFillDiagonalRiverCornerGaps(map);
    return primary;
}
