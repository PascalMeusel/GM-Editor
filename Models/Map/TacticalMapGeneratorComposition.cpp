#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "Models/Map/TacticalMapGeneratorInternal.hpp"

namespace TacticalMapGeneratorInternal
{
const MapRegionModel *findRegion(const MapModel &map, const std::string &name);

bool protectedZoneCell(const MapModel &map, int x, int y)
{
    for (const MapRegionModel &region : map.regions)
    {
        if (region.name.rfind("Party Start", 0) != 0 && region.name.rfind("Opposition Start", 0) != 0 &&
            region.name.rfind("Objective ", 0) != 0)
            continue;
        if (x >= region.x && y >= region.y && x < region.x + region.width && y < region.y + region.height)
            return true;
    }
    return false;
}

Point findTacticalObjective(const MapModel &map, Point preferred)
{
    const MapRegionModel *party = findRegion(map, "Party Start");
    const MapRegionModel *opposition = findRegion(map, "Opposition Start");
    Point best{-1, -1};
    int bestScore = std::numeric_limits<int>::max();
    for (int y = 1; y + 1 < map.height; ++y)
        for (int x = 1; x + 1 < map.width; ++x)
        {
            if (!cellCanSpawn(mapCellAt(map, x, y)) || protectedZoneCell(map, x, y))
                continue;
            if (party && !mapGenCoordinatesConnected(
                             map, {party->x + party->width / 2, party->y + party->height / 2},
                             {x, y}, false))
                continue;
            if (opposition && !mapGenCoordinatesConnected(
                                  map, {opposition->x + opposition->width / 2,
                                        opposition->y + opposition->height / 2},
                                  {x, y}, false))
                continue;
            const int score = std::abs(x - preferred.x) + std::abs(y - preferred.y);
            if (score < bestScore)
            {
                best = {x, y};
                bestScore = score;
            }
        }
    return best;
}

bool addSetpiece(MapModel &map, int index, int x, int y, MapTileKind kind, const std::string &name)
{
    if(map.generation.recipe.generatorVersion >= 10 && std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &feature) {
        return feature.materialId=="fantasy_neutral.tactical.waystone" && !feature.points.empty() &&
            static_cast<int>(std::floor(feature.points.front().x))==x && static_cast<int>(std::floor(feature.points.front().y))==y;
    })) return false;
    MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell || protectedZoneCell(map, x, y) || cell->wayKind != MapWayKind::None || !cellCanSpawn(cell))
        return false;
    applyMapCellTerrainDefaults(*cell, kind);
    if (kind == MapTileKind::Cover)
        cell->blocksLineOfSight = false;
    else if (kind == MapTileKind::Rubble)
        cell->blocksLineOfSight = index % 3 == 0;
    MapGeneratedFeature setpiece;
    setpiece.id = map.metadata.id + "::setpiece::" + std::to_string(index);
    setpiece.kind = MapGeneratedFeatureKind::TacticalSetpiece;
    setpiece.name = name;
    setpiece.materialId = kind == MapTileKind::Rubble ? "fantasy_neutral.tactical.rubble" : "fantasy_neutral.tactical.cover";
    setpiece.points.push_back({x + 0.5f, y + 0.5f});
    map.generation.graph.features.push_back(std::move(setpiece));
    return true;
}

void addFairCover(MapModel &map, const TacticalMapRecipe &recipe, std::uint32_t seed)
{
    // Cover must read as encounter structure, not as evenly distributed prop
    // confetti.  A handful of two-cell clusters creates memorable positions,
    // leaves the authored lanes legible and still gives both deployment halves
    // enough useful cover.  The density controls the number of cover *cells*;
    // it no longer scales all the way to dozens of unrelated single stamps.
    const int requested = std::clamp(
        map.width * map.height * recipe.coverDensity / (recipe.generatorVersion >= 10 ? 4000 : 6000),
        6, recipe.generatorVersion >= 10 ? 32 : 12);
    const int sideQuota = std::max(3, requested / 2);
    std::array<int, 2> sideCounts{0, 0};
    int featureIndex = 0;
    int clusterIndex = 0;
    int attempt = 0;
    constexpr std::array<Point, 8> neighborOffsets{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {-1, 1}, {1, -1}, {-1, -1}}};
    while (sideCounts[0] + sideCounts[1] < requested && attempt < requested * 24)
    {
        int side = clusterIndex & 1;
        if (sideCounts[side] >= sideQuota && sideCounts[1 - side] < sideQuota)
            side = 1 - side;
        if (sideCounts[side] >= sideQuota)
        {
            ++attempt;
            continue;
        }
        const int sideMin = side == 0 ? 4 : map.width / 2 + 2;
        const int sideMax = side == 0 ? std::max(sideMin, map.width / 2 - 3) : std::max(sideMin, map.width - 5);
        const int x = sideMin + static_cast<int>(tacticalHash(seed, attempt, side, 0x811u) % static_cast<std::uint32_t>(std::max(1, sideMax - sideMin + 1)));
        const int y = 2 + static_cast<int>(tacticalHash(seed, attempt, side, 0x812u) % static_cast<std::uint32_t>(std::max(1, map.height - 4)));
        const MapTileKind kind = (clusterIndex % 3 == 0)
                                     ? MapTileKind::Rubble
                                     : MapTileKind::Cover;
        if (addSetpiece(map, featureIndex, x, y, kind,
                        kind == MapTileKind::Rubble ? "Rubble cluster" : "Cover cluster"))
        {
            ++featureIndex;
            ++sideCounts[side];
            const std::size_t offsetStart = static_cast<std::size_t>(
                tacticalHash(seed, clusterIndex, attempt, 0x813u) % neighborOffsets.size());
            for (std::size_t offsetIndex = 0;
                 offsetIndex < neighborOffsets.size() && sideCounts[side] < sideQuota &&
                 sideCounts[0] + sideCounts[1] < requested;
                 ++offsetIndex)
            {
                const Point offset = neighborOffsets[(offsetStart + offsetIndex) % neighborOffsets.size()];
                const int neighborX = x + offset.x;
                const int neighborY = y + offset.y;
                if ((side == 0 && neighborX >= map.width / 2) ||
                    (side == 1 && neighborX < map.width / 2))
                    continue;
                if (addSetpiece(map, featureIndex, neighborX, neighborY, kind,
                                kind == MapTileKind::Rubble ? "Rubble cluster" : "Cover cluster"))
                {
                    ++featureIndex;
                    ++sideCounts[side];
                    break;
                }
            }
            ++clusterIndex;
        }
        ++attempt;
    }

    // Deterministic safety anchors are only used when a dense template left a
    // side without the two pieces required by readiness validation.  They are
    // searched near the centre of that half rather than always adding four
    // extra stamps to every map.
    for (int side = 0; side < 2; ++side)
    {
        for (int ring = 0; sideCounts[side] < 2 && ring < map.height; ++ring)
        {
            const int baseX = side == 0 ? std::max(2, map.width / 4)
                                        : std::min(map.width - 3, map.width * 3 / 4);
            const int y = std::clamp(map.height / 2 + ((ring & 1) ? 1 : -1) * ((ring + 1) / 2),
                                     1, map.height - 2);
            for (int dx : {0, -1, 1, -2, 2})
            {
                if (addSetpiece(map, featureIndex, baseX + dx, y, MapTileKind::Cover,
                                "Cover cluster"))
                {
                    ++featureIndex;
                    ++sideCounts[side];
                    break;
                }
            }
        }
    }
}

void addRequiredElements(MapModel &map, const TacticalMapRecipe &recipe, std::uint32_t seed)
{
    for (int index = 0; index < static_cast<int>(recipe.requiredElements.size()); ++index)
    {
        const int x = std::clamp(map.width / 2 + (index % 2 == 0 ? -3 : 3), 2, map.width - 3);
        const int y = std::clamp(map.height / 2 + static_cast<int>(tacticalHash(seed, index, 0, 0xa11u) % 5u) - 2, 2, map.height - 3);
        if (MapCellModel *cell = mapCellAt(map, x, y); cell && cellCanSpawn(cell) && cell->wayKind == MapWayKind::None)
            applyMapCellTerrainDefaults(*cell, MapTileKind::Landmark);
        MapGeneratedFeature feature;
        feature.id = map.metadata.id + "::required::" + std::to_string(index);
        feature.kind = MapGeneratedFeatureKind::TacticalSetpiece;
        feature.name = recipe.requiredElements[static_cast<std::size_t>(index)];
        feature.materialId = "fantasy_neutral.tactical.required";
        feature.points.push_back({x + 0.5f, y + 0.5f});
        map.generation.graph.features.push_back(std::move(feature));
    }
}
}
