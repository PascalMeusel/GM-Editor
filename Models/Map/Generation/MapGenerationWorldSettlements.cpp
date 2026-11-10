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

std::string mapGenV11AuthoredSettlementNationName(const std::vector<MapGenerationFact> &facts,
                                                  const std::string &settlementName)
{
    for (const auto &city : facts)
    {
        if (city.type != MapGenerationFactType::Settlement || city.entityRefs.size() < 2 ||
            (city.sourceText != "map_setup:new" && city.sourceText != "map_setup:existing") ||
            mapGenV5RelationNameKey(city.label) != mapGenV5RelationNameKey(settlementName))
            continue;
        for (const auto &nation : facts)
            if (nation.type == MapGenerationFactType::Nation && !nation.entityRefs.empty() &&
                (nation.sourceText == "map_setup:new" || nation.sourceText == "map_setup:existing") &&
                nation.entityRefs.front() == city.entityRefs[1])
                return nation.label;
    }
    return {};
}

MapCellCoordinate mapGenV5ChooseWorldSettlement(const MapModel &map, const std::vector<MapCellCoordinate> &existing,
                                                std::uint32_t seed, int ordinal, const std::string &requiredNation)
{
    std::vector<const MapGeneratedFeature *> territories;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.closed &&
            feature.materialId.rfind("fantasy_neutral.territory.", 0) == 0)
            territories.push_back(&feature);

    int targetTerritory = -1;
    if (!territories.empty())
    {
        std::vector<int> population(territories.size(), 0);
        for (const MapCellCoordinate settlement : existing)
        {
            const MapFeaturePoint point{static_cast<float>(settlement.x) + 0.5f,
                                        static_cast<float>(settlement.y) + 0.5f};
            for (std::size_t index = 0; index < territories.size(); ++index)
                if (mapGenV5PointInsideTerritory(point, *territories[index]))
                {
                    ++population[index];
                    break;
                }
        }
        const int minimumPopulation = *std::min_element(population.begin(), population.end());
        std::vector<int> leastPopulated;
        for (std::size_t index = 0; index < population.size(); ++index)
            if (population[index] == minimumPopulation)
                leastPopulated.push_back(static_cast<int>(index));
        if (!leastPopulated.empty())
        {
            const std::size_t choice =
                static_cast<std::size_t>((seed ^ (seed >> 16u) ^ static_cast<std::uint32_t>(ordinal * 37 + 11))) %
                leastPopulated.size();
            targetTerritory = leastPopulated[choice];
        }
    }

    if (!requiredNation.empty())
    {
        targetTerritory = -1;
        for (std::size_t i = 0; i < territories.size(); ++i)
            if (mapGenV5RelationNameKey(territories[i]->name) == mapGenV5RelationNameKey(requiredNation))
            {
                targetTerritory = static_cast<int>(i);
                break;
            }
        if (targetTerritory < 0)
            return {-1, -1};
    }

    const bool explicitIslands =
        map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.landform == MapLandform::Islands;
    const int minimumSpacing =
        std::max(explicitIslands ? 3 : 5, std::min(map.width, map.height) / (explicitIslands ? 10 : 7));
    bool existingRiverSettlement = false;
    for (const MapCellCoordinate settlement : existing)
    {
        float tangentX = 0.0f;
        float tangentY = 0.0f;
        existingRiverSettlement =
            existingRiverSettlement ||
            mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::River,
                                          static_cast<float>(settlement.x) + 0.5f,
                                          static_cast<float>(settlement.y) + 0.5f, 3.5f, tangentX, tangentY);
    }

    // Restrict automatic principal places to the part of their nation that
    // the world-road planner can actually reach. Previously a town could win
    // on spacing alone behind a coastal choke point; the later road pass then
    // failed and attached only a decorative one-cell approach. A single cheap
    // flood fill is sufficient here and avoids running A* for every possible
    // settlement cell.
    const int area = map.width * map.height;
    std::vector<unsigned char> roadReachable(static_cast<std::size_t>(area), existing.empty() ? 1 : 0);
    if (!explicitIslands && map.generation.recipe.generatorVersion >= 10 && existing.empty() && territories.empty())
    {
        // Choose the initial automatic hub on the main landmass. Later hubs
        // remain road-reachable; small offshore islands retain their terrain
        // and landmarks without forcing impossible roads across the sea.
        std::vector<unsigned char> seen(static_cast<std::size_t>(area), 0);
        std::vector<int> largest;
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const int start = mapCellIndex(map, x, y);
                if (seen[start] || mapGenV5WorldOpenWaterAt(map, x, y))
                    continue;
                std::vector<int> component{start};
                seen[start] = 1;
                for (std::size_t i = 0; i < component.size(); ++i)
                {
                    const int cx = component[i] % map.width, cy = component[i] / map.width;
                    for (auto d : {MapCellCoordinate{1, 0}, MapCellCoordinate{-1, 0}, MapCellCoordinate{0, 1},
                                   MapCellCoordinate{0, -1}})
                    {
                        const int nx = cx + d.x, ny = cy + d.y;
                        if (!isMapCellInside(map, nx, ny))
                            continue;
                        const int index = mapCellIndex(map, nx, ny);
                        if (seen[index] || mapGenV5WorldOpenWaterAt(map, nx, ny))
                            continue;
                        seen[index] = 1;
                        component.push_back(index);
                    }
                }
                if (component.size() > largest.size())
                    largest = std::move(component);
            }
        std::fill(roadReachable.begin(), roadReachable.end(), 0);
        for (int index : largest)
            roadReachable[index] = 1;
    }
    if (!existing.empty())
    {
        std::queue<MapCellCoordinate> pending;
        for (const MapCellCoordinate origin : existing)
        {
            if (!isMapCellInside(map, origin.x, origin.y))
                continue;
            const int index = mapCellIndex(map, origin.x, origin.y);
            if (roadReachable[static_cast<std::size_t>(index)])
                continue;
            roadReachable[static_cast<std::size_t>(index)] = 1;
            pending.push(origin);
        }
        constexpr MapCellCoordinate directions[] = {{0, -1}, {1, 0}, {0, 1},  {-1, 0},
                                                    {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
        while (!pending.empty())
        {
            const MapCellCoordinate current = pending.front();
            pending.pop();
            for (const MapCellCoordinate direction : directions)
            {
                const MapCellCoordinate next{current.x + direction.x, current.y + direction.y};
                if (!isMapCellInside(map, next.x, next.y))
                    continue;
                const int nextIndex = mapCellIndex(map, next.x, next.y);
                if (roadReachable[static_cast<std::size_t>(nextIndex)])
                    continue;
                const MapCellModel *nextCell = mapCellAt(map, next.x, next.y);
                if (!nextCell || mapGenV2RoadTraversalCost(*nextCell) >= 100000)
                    continue;
                if (direction.x != 0 && direction.y != 0)
                {
                    const MapCellModel *sideX = mapCellAt(map, next.x, current.y);
                    const MapCellModel *sideY = mapCellAt(map, current.x, next.y);
                    if (!sideX || !sideY || mapGenV2RoadTraversalCost(*sideX) >= 100000 ||
                        mapGenV2RoadTraversalCost(*sideY) >= 100000 || sideX->kind == MapTileKind::Water ||
                        sideY->kind == MapTileKind::Water)
                        continue;
                }
                if (mapGenV5WorldOpenWaterDistance(map, next.x, next.y, 1) <= 1)
                {
                    bool endpointApproach = false;
                    for (const MapCellCoordinate origin : existing)
                        endpointApproach =
                            endpointApproach || std::abs(next.x - origin.x) + std::abs(next.y - origin.y) <= 2;
                    if (!endpointApproach)
                        continue;
                }
                roadReachable[static_cast<std::size_t>(nextIndex)] = 1;
                pending.push(next);
            }
        }
    }

    MapCellCoordinate best{-1, -1};
    int bestScore = std::numeric_limits<int>::min();
    for (int y = 2; y < map.height - 2; ++y)
    {
        for (int x = 2; x < map.width - 2; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
                cell->kind == MapTileKind::Mountain || cell->kind == MapTileKind::Bridge ||
                cell->wayKind != MapWayKind::None || mapGenIsStructureLike(cell->kind))
                continue;
            if (!explicitIslands && !existing.empty() &&
                !roadReachable[static_cast<std::size_t>(mapCellIndex(map, x, y))])
                continue;
            if (mapGenV5WorldOpenWaterDistance(map, x, y, 1) <= 1)
                continue;
            const MapFeaturePoint point{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
            if (targetTerritory >= 0 &&
                !mapGenV5PointInsideTerritory(point, *territories[static_cast<std::size_t>(targetTerritory)]))
                continue;

            int nearestSettlement = map.width + map.height;
            for (const MapCellCoordinate other : existing)
                nearestSettlement = std::min(nearestSettlement, std::abs(other.x - x) + std::abs(other.y - y));
            if (!existing.empty() && nearestSettlement < minimumSpacing)
                continue;

            int usableNeighbors = 0;
            for (int offsetY = -1; offsetY <= 1; ++offsetY)
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    if (offsetX == 0 && offsetY == 0)
                        continue;
                    const MapCellModel *neighbor = mapCellAt(map, x + offsetX, y + offsetY);
                    usableNeighbors += neighbor && mapGenV2RoadTraversalCost(*neighbor) < 100000 ? 1 : 0;
                }

            float riverX = 0.0f;
            float riverY = 0.0f;
            const bool onRiverCorridor = mapGenV2NearestFeatureTangent(
                map.generation.graph, MapGeneratedFeatureKind::River, point.x, point.y, 3.5f, riverX, riverY);
            int score = std::min(nearestSettlement, 24) * 4 + usableNeighbors * 5;
            score += mapGenNearKind(map, x, y, MapTileKind::Water, 5) ? 10 : 0;
            score += cell->kind == MapTileKind::Empty || cell->kind == MapTileKind::Dirt ? 18 : 5;
            // One river capital or trade town is useful. Once such a focus
            // exists, force the remaining principal places to articulate the
            // wider nations instead of stacking every icon on one corridor.
            if (existingRiverSettlement && onRiverCorridor)
                score -= 64;
            if (mapGenNearKind(map, x, y, MapTileKind::Mountain, 4))
                score -= 72;
            score += static_cast<int>(mapGenV2Hash01(seed + 4271u + static_cast<std::uint32_t>(ordinal * 101), x, y) *
                                      23.0f);
            if (score > bestScore)
            {
                bestScore = score;
                best = {x, y};
            }
        }
    }
    return best;
}

bool mapGenV5RelationFeatureCenter(const MapGeneratedFeature &feature, MapFeaturePoint &center)
{
    if (feature.points.empty())
        return false;
    center = {};
    for (const MapFeaturePoint &point : feature.points)
    {
        center.x += point.x;
        center.y += point.y;
    }
    center.x /= static_cast<float>(feature.points.size());
    center.y /= static_cast<float>(feature.points.size());
    return true;
}

const MapGeneratedFeature *mapGenV5FindRelationFeature(const MapModel &map, const std::string &name)
{
    const std::string key = mapGenV5RelationNameKey(name);
    const auto matchesName = [&](const MapGeneratedFeature &feature) {
        return mapGenV5RelationNameKey(feature.name) == key;
    };
    // Point entities are the least ambiguous representation when a name was
    // (intentionally or accidentally) reused for both a place and a region.
    for (const MapGeneratedFeatureKind kind : {MapGeneratedFeatureKind::Settlement, MapGeneratedFeatureKind::Landmark,
                                               MapGeneratedFeatureKind::Nation, MapGeneratedFeatureKind::TerrainRegion})
    {
        const auto found = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                        [&](const MapGeneratedFeature &feature) {
                                            if (!matchesName(feature) || feature.kind != kind)
                                                return false;
                                            return kind != MapGeneratedFeatureKind::TerrainRegion ||
                                                   feature.materialId.find("territory.") != std::string::npos;
                                        });
        if (found != map.generation.graph.features.end())
            return &*found;
    }
    return nullptr;
}

std::uint32_t mapGenV5RelationTextHash(const std::string &value)
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : value)
    {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

