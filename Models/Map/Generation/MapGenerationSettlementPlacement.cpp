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

void mapGenBuildForestsAndSettlements(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    const int area = std::max(1, map.width * map.height);
    const int forestDensity = mapGenClampPercent(options.forestDensity);
    const int forestCount = area * forestDensity / 5000;
    const int maxForestRadius = std::max(2, std::min(forestDensity >= 55 ? 4 : 5, forestDensity / 20 + 2));
    mapGenScatterClumps(map, MapTileKind::Forest, forestCount, 2, maxForestRadius, rng, forestDensity >= 55 ? 46 : 40,
                        MapSymbolVariantMode::Continuous);

    const int settlementCount = area * mapGenClampPercent(options.settlementDensity) / 2400;
    for (int i = 0; i < settlementCount; ++i)
    {
        for (int attempt = 0; attempt < 40; ++attempt)
        {
            const int x = mapGenRandomBetween(rng, 1, map.width - 2);
            const int y = mapGenRandomBetween(rng, 1, map.height - 2);
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
                mapGenNearKind(map, x, y, MapTileKind::Water, 1))
                continue;
            mapGenSetCell(map, x, y, MapTileKind::Settlement);
            if (x + 1 < map.width - 1 && mapGenChance(rng, 55))
            {
                const MapCellModel *buildingCell = mapCellAt(map, x + 1, y);
                if (buildingCell && buildingCell->movementCost < 99 && buildingCell->kind != MapTileKind::Water &&
                    !mapGenNearKind(map, x + 1, y, MapTileKind::Water, 1))
                    mapGenSetCell(map, x + 1, y, MapTileKind::Building);
            }
            break;
        }
    }
}

MapCellCoordinate mapGenV2ChooseSettlement(const MapModel &map, const std::vector<MapCellCoordinate> &existing,
                                           std::mt19937 &rng)
{
    MapCellCoordinate best{-1, -1};
    MapCellCoordinate coastalFallback{-1, -1};
    int bestScore = std::numeric_limits<int>::min();
    int coastalFallbackScore = std::numeric_limits<int>::min();
    const bool composedRegion = map.generation.recipe.generatorVersion >= 10 && map.atlasLevel == MapLevel::Region;
    const int sampleCount = composedRegion ? std::clamp(map.width * map.height / 3, 180, 1200) : 180;
    for (int attempt = 0; attempt < sampleCount; ++attempt)
    {
        const int x = mapGenRandomBetween(rng, 3, std::max(3, map.width - 4));
        const int y = mapGenRandomBetween(rng, 3, std::max(3, map.height - 4));
        const MapCellModel *cell = mapCellAt(map, x, y);
        if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
            cell->kind == MapTileKind::Mountain)
            continue;
        int nearest = 10000;
        for (const MapCellCoordinate &other : existing)
            nearest = std::min(nearest, std::abs(other.x - x) + std::abs(other.y - y));
        if (!existing.empty() && nearest < std::max(5, std::min(map.width, map.height) / 5))
            continue;
        int score = std::min(nearest, 18) * 2;
        if (composedRegion)
        {
            // Exploration towns should span the region, even on tall maps.
            // The legacy fixed 18-cell ceiling treated half a large map as redundant.
            score += (std::min(nearest, std::max(map.width, map.height) * 3 / 5) - std::min(nearest, 18)) * 2;
            if (existing.empty())
                score -= static_cast<int>(
                    14.0f * std::hypot((2.0f * x - map.width) / map.width, (2.0f * y - map.height) / map.height));
        }
        if (mapGenNearKind(map, x, y, MapTileKind::Water, 3))
            score += 26;
        if (cell->kind == MapTileKind::Empty || cell->kind == MapTileKind::Dirt)
            score += 18;
        if (cell->kind == MapTileKind::Forest)
            score -= 8;
        if (map.generation.recipe.generatorVersion >= 7 && map.atlasLevel == MapLevel::Region)
        {
            // Prefer useful level sites over repeated rows of equally good
            // towns. Locked and inherited places never use this sampler.
            for (const auto &other : existing)
                if (std::abs(other.x - x) <= 1 || std::abs(other.y - y) <= 1)
                    score -= 9;
            for (const auto neighbor : {MapCellCoordinate{x - 1, y}, MapCellCoordinate{x + 1, y},
                                        MapCellCoordinate{x, y - 1}, MapCellCoordinate{x, y + 1}})
                if (const auto *adjacent = mapCellAt(map, neighbor.x, neighbor.y))
                    score -= std::abs(adjacent->elevation - cell->elevation) * 2;
        }
        score += mapGenRandomBetween(rng, 0, 12);
        if (score > bestScore)
        {
            bestScore = score;
            best = {x, y};
        }
    }
    return best;
}

MapCellCoordinate mapGenV4ChooseSharedRiverSettlement(const MapModel &map, const std::vector<MapCellCoordinate> &river,
                                                      const std::vector<MapCellCoordinate> &existing, int ordinal,
                                                      int targetCount, std::uint32_t seed, float requestedFraction)
{
    if (river.size() < 3)
        return {-1, -1};
    const float fraction = requestedFraction >= 0.0f
                               ? std::clamp(requestedFraction, 0.05f, 0.95f)
                               : static_cast<float>(ordinal + 1) / static_cast<float>(std::max(2, targetCount + 1));
    const int fractionIndex = std::clamp(static_cast<int>(std::lround(fraction * static_cast<float>(river.size() - 1))),
                                         1, static_cast<int>(river.size()) - 2);
    const MapCellCoordinate target = river[static_cast<std::size_t>(fractionIndex)];
    const MapCellCoordinate before = river[static_cast<std::size_t>(fractionIndex - 1)];
    const MapCellCoordinate after = river[static_cast<std::size_t>(fractionIndex + 1)];
    const float tangentX = static_cast<float>(after.x - before.x);
    const float tangentY = static_cast<float>(after.y - before.y);
    const float preferredBank = mapGenV2Hash01(seed + 3581u, 5, 7) < 0.5f ? -1.0f : 1.0f;
    const int minimumSpacing = std::max(4, std::min(map.width, map.height) / 8);
    MapCellCoordinate best{-1, -1};
    int bestScore = std::numeric_limits<int>::min();
    for (int y = 1; y < map.height - 1; ++y)
    {
        for (int x = 1; x < map.width - 1; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
                cell->kind == MapTileKind::Mountain || cell->kind == MapTileKind::Bridge ||
                cell->wayKind != MapWayKind::None)
                continue;
            int nearestRiverSquared = std::numeric_limits<int>::max();
            for (const MapCellCoordinate point : river)
            {
                const int deltaX = point.x - x;
                const int deltaY = point.y - y;
                nearestRiverSquared = std::min(nearestRiverSquared, deltaX * deltaX + deltaY * deltaY);
            }
            // A shared-river settlement belongs on a usable bank, not merely
            // somewhere on the same continent and not directly in the channel.
            if (nearestRiverSquared < 1 || nearestRiverSquared > 16)
                continue;
            int nearestSettlement = map.width + map.height;
            for (const MapCellCoordinate other : existing)
                nearestSettlement = std::min(nearestSettlement, std::abs(other.x - x) + std::abs(other.y - y));
            if (!existing.empty() && nearestSettlement < minimumSpacing)
                continue;
            const int targetDistance = std::abs(target.x - x) + std::abs(target.y - y);
            const int usableNeighbors = (mapGenV2RoadTraversalCost(*mapCellAt(map, x - 1, y)) < 100000 ? 1 : 0) +
                                        (mapGenV2RoadTraversalCost(*mapCellAt(map, x + 1, y)) < 100000 ? 1 : 0) +
                                        (mapGenV2RoadTraversalCost(*mapCellAt(map, x, y - 1)) < 100000 ? 1 : 0) +
                                        (mapGenV2RoadTraversalCost(*mapCellAt(map, x, y + 1)) < 100000 ? 1 : 0);
            const int terrainBonus = cell->kind == MapTileKind::Empty || cell->kind == MapTileKind::Dirt
                                         ? 18
                                         : (cell->kind == MapTileKind::Forest ? 4 : 10);
            const float candidateBank =
                tangentX * static_cast<float>(y - target.y) - tangentY * static_cast<float>(x - target.x);
            const int bankBonus = candidateBank * preferredBank >= 0.0f ? 28 : -8;
            const int score =
                -targetDistance * 7 - nearestRiverSquared * 2 + std::min(nearestSettlement, 20) * 2 +
                usableNeighbors * 5 + terrainBonus + bankBonus +
                (mapGenNearKind(map, x, y, MapTileKind::Mountain, 2) ? -72 : 0) +
                static_cast<int>(mapGenV2Hash01(seed + 3559u + static_cast<std::uint32_t>(ordinal * 31), x, y) * 11.0f);
            if (score > bestScore)
            {
                bestScore = score;
                best = {x, y};
            }
        }
    }
    return best;
}

void mapGenV2PaintSettlement(MapModel &map, MapCellCoordinate center, int index, std::mt19937 &rng,
                             bool paintIndividualBuildings)
{
    mapGenSetCell(map, center.x, center.y, MapTileKind::Settlement, MapSymbolVariantMode::Individual);
    const MapCellCoordinate offsets[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, -1}};
    const int buildingCount = paintIndividualBuildings ? 1 + (index + mapGenRandomBetween(rng, 0, 2)) % 3 : 0;
    for (int i = 0; i < buildingCount; ++i)
    {
        const MapCellCoordinate offset = offsets[(i + index) % 6];
        MapCellModel *cell = mapCellAt(map, center.x + offset.x, center.y + offset.y);
        if (cell && cell->movementCost < 99 && cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Mountain)
            mapGenSetCell(map, center.x + offset.x, center.y + offset.y, MapTileKind::Building,
                          MapSymbolVariantMode::Individual);
    }
    MapGeneratedFeature feature;
    feature.id = mapGenV2FeatureId(map.generation.recipe.seed, "settlement", index);
    feature.kind = MapGeneratedFeatureKind::Settlement;
    feature.name = map.generation.recipe.generatorVersion >= 4
                       ? mapGenV4SettlementName(map.generation.recipe.seed, index, map.generation.recipe.nameCulture)
                       : "Settlement " + std::to_string(index + 1);
    feature.materialId = map.generation.recipe.generatorVersion >= 8
                             ? (index % 5 == 0   ? "fantasy_neutral.settlement.city"
                                : index % 3 == 0 ? "fantasy_neutral.settlement.village"
                                                 : "fantasy_neutral.settlement.town")
                             : "fantasy_neutral.settlement.cluster";
    feature.width = 2.0f;
    feature.points.push_back({static_cast<float>(center.x) + 0.5f, static_cast<float>(center.y) + 0.5f});
    map.generation.graph.features.push_back(std::move(feature));
}

