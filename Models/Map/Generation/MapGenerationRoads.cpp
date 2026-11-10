#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
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

bool mapGenBridgeCrossingLikeTerrain(MapTileKind kind)
{
    return kind == MapTileKind::Water || kind == MapTileKind::Swamp || kind == MapTileKind::Bridge;
}

bool mapGenDiagonalWayStepTouchesBridgeContext(const MapModel &map, int x, int y, int stepX, int stepY)
{
    const MapCellModel *current = mapCellAt(map, x, y);
    const MapCellModel *diagonal = mapCellAt(map, x + stepX, y + stepY);
    const MapCellModel *sideX = mapCellAt(map, x + stepX, y);
    const MapCellModel *sideY = mapCellAt(map, x, y + stepY);
    return (current && mapGenBridgeCrossingLikeTerrain(current->kind)) ||
           (diagonal && mapGenBridgeCrossingLikeTerrain(diagonal->kind)) ||
           (sideX && mapGenBridgeCrossingLikeTerrain(sideX->kind)) ||
           (sideY && mapGenBridgeCrossingLikeTerrain(sideY->kind));
}

bool mapGenBridgeApproachLikeCell(const MapCellModel &cell)
{
    return mapGenWayKindIsRoadLike(cell.wayKind) || cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Dirt ||
           cell.kind == MapTileKind::Stone || cell.kind == MapTileKind::Settlement;
}

bool mapGenBridgeCrossingContextCell(const MapCellModel &cell)
{
    return cell.kind == MapTileKind::Water || cell.kind == MapTileKind::Swamp || mapWayKindIsWaterLike(cell.wayKind);
}

void mapGenSetWay(MapModel &map, int x, int y, MapWayKind wayKind)
{
    MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell)
        return;
    // A road crossing is rendered as a bridge, but the single raster cell also
    // remains the hydrology sample for the river feature below it. Preserve the
    // water-surface elevation when changing the visual/way semantics so a
    // perfectly downhill river cannot appear to jump uphill at a later bridge.
    const bool crossingExistingWaterway = mapGenWayKindIsRoadLike(wayKind) && mapWayKindIsWaterLike(cell->wayKind);
    const int waterwayElevation = cell->elevation;
    if (mapGenWayKindIsRoadLike(wayKind) && (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Swamp))
    {
        mapGenSetCell(map, x, y, MapTileKind::Bridge);
        cell = mapCellAt(map, x, y);
        if (!cell)
            return;
        if (crossingExistingWaterway)
            cell->elevation = waterwayElevation;
    }
    if (mapGenWayKindIsRoadLike(wayKind))
    {
        const MapTileKind roadbed = mapGenArtContractRoadbedForWay(wayKind, cell->kind);
        if (roadbed != cell->kind)
        {
            mapGenSetCell(map, x, y, roadbed);
            cell = mapCellAt(map, x, y);
            if (!cell)
                return;
        }
    }
    if (!mapGenTerrainSupportsWay(cell->kind, wayKind))
    {
        if (wayKind == MapWayKind::River || wayKind == MapWayKind::Canal)
            mapGenSetCell(map, x, y, MapTileKind::Water);
        else if (wayKind == MapWayKind::DungeonCorridor)
            mapGenSetCell(map, x, y, MapTileKind::Room);
        else if (wayKind == MapWayKind::CaveTunnel)
            mapGenSetCell(map, x, y, MapTileKind::Cave);
        else
            mapGenSetCell(map, x, y, mapGenRoadbedForTerrain(cell->kind));
        cell = mapCellAt(map, x, y);
        if (!cell)
            return;
    }
    cell->wayKind = wayKind;
}

void mapGenDrawWay(MapModel &map, int x1, int y1, int x2, int y2, MapWayKind kind, std::mt19937 &rng, int wanderPercent,
                   bool paintTerrain, MapTileKind terrainKind, int terrainRadius, bool allowDiagonal)
{
    int x = std::clamp(x1, 0, std::max(0, map.width - 1));
    int y = std::clamp(y1, 0, std::max(0, map.height - 1));
    const int targetX = std::clamp(x2, 0, std::max(0, map.width - 1));
    const int targetY = std::clamp(y2, 0, std::max(0, map.height - 1));
    const int safetyLimit = std::max(16, (map.width + map.height) * 10);

    for (int step = 0; step < safetyLimit; ++step)
    {
        if (paintTerrain)
            mapGenPaintDisc(map, x, y, terrainRadius, terrainKind, rng, 0);
        mapGenSetWay(map, x, y, kind);
        if (x == targetX && y == targetY)
            break;

        const int dx = targetX - x;
        const int dy = targetY - y;
        const bool preferX = std::abs(dx) >= std::abs(dy);
        const bool wander = mapGenChance(rng, wanderPercent);
        const bool canStepDiagonal = dx != 0 && dy != 0;
        const int stepX = mapGenStepToward(x, targetX);
        const int stepY = mapGenStepToward(y, targetY);
        const bool diagonalWouldTouchBridgeContext = mapGenWayKindIsRoadLike(kind) && canStepDiagonal &&
                                                     mapGenDiagonalWayStepTouchesBridgeContext(map, x, y, stepX, stepY);

        if (allowDiagonal && canStepDiagonal && !diagonalWouldTouchBridgeContext && (!wander || mapGenChance(rng, 58)))
        {
            x += stepX;
            y += stepY;
        }
        else if ((preferX && !wander) || dy == 0)
            x += stepX;
        else if ((!preferX && !wander) || dx == 0)
            y += stepY;
        else if (mapGenChance(rng, 50))
            x = std::clamp(x + mapGenRandomBetween(rng, -1, 1), 0, std::max(0, map.width - 1));
        else
            y = std::clamp(y + mapGenRandomBetween(rng, -1, 1), 0, std::max(0, map.height - 1));

        x = std::clamp(x, 0, std::max(0, map.width - 1));
        y = std::clamp(y, 0, std::max(0, map.height - 1));
    }
}

bool mapGenIsStructureLike(MapTileKind kind)
{
    return kind == MapTileKind::Settlement || kind == MapTileKind::Building || kind == MapTileKind::Landmark;
}

bool mapGenCanStartStructureApproach(const MapCellModel *cell)
{
    if (!cell || cell->movementCost >= 99)
        return false;
    if (mapGenIsStructureLike(cell->kind))
        return false;
    return cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Swamp && cell->kind != MapTileKind::Chasm &&
           cell->kind != MapTileKind::Lava && cell->kind != MapTileKind::Pit && cell->kind != MapTileKind::Hazard;
}

bool mapGenCellHasStructureApproach(const MapModel &map, int x, int y)
{
    const MapCellModel *structure = mapCellAt(map, x, y);
    if (structure && (mapGenWayKindIsRoadLike(structure->wayKind) || mapWayKindIsInteriorLike(structure->wayKind)))
        return true;
    const MapCellCoordinate neighbors[] = {{x, y - 1}, {x + 1, y}, {x, y + 1}, {x - 1, y}};
    for (const MapCellCoordinate &neighbor : neighbors)
    {
        const MapCellModel *cell = mapCellAt(map, neighbor.x, neighbor.y);
        if (cell && map.generation.recipe.generatorVersion >= 11 && map.generation.recipe.routeDensity == 0 &&
            cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Mountain && cell->movementCost < 99)
            return true;
        if (cell && (mapGenWayKindIsRoadLike(cell->wayKind) || mapWayKindIsInteriorLike(cell->wayKind) ||
                     cell->kind == MapTileKind::Road || cell->kind == MapTileKind::Bridge))
            return true;
    }
    return false;
}

MapCellCoordinate mapGenNearestWayCell(const MapModel &map, int x, int y, int maxDistance, bool includeInteriorWays)
{
    MapCellCoordinate best{-1, -1};
    int bestDistance = maxDistance + 1;
    for (int candidateY = 0; candidateY < map.height; ++candidateY)
    {
        for (int candidateX = 0; candidateX < map.width; ++candidateX)
        {
            const MapCellModel *cell = mapCellAt(map, candidateX, candidateY);
            if (!cell || (!mapGenWayKindIsRoadLike(cell->wayKind) &&
                          !(includeInteriorWays && mapWayKindIsInteriorLike(cell->wayKind))))
                continue;

            const int distance = std::abs(candidateX - x) + std::abs(candidateY - y);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = {candidateX, candidateY};
            }
        }
    }
    return best;
}

MapCellCoordinate mapGenChooseStructureApproachStart(const MapModel &map, int x, int y, const MapCellCoordinate &target)
{
    std::vector<MapCellCoordinate> candidates;
    const int stepX = mapGenStepToward(x, target.x);
    const int stepY = mapGenStepToward(y, target.y);
    if (std::abs(target.x - x) >= std::abs(target.y - y))
    {
        if (stepX != 0)
            candidates.push_back({x + stepX, y});
        if (stepY != 0)
            candidates.push_back({x, y + stepY});
    }
    else
    {
        if (stepY != 0)
            candidates.push_back({x, y + stepY});
        if (stepX != 0)
            candidates.push_back({x + stepX, y});
    }

    const MapCellCoordinate fallback[] = {{x, y - 1}, {x + 1, y}, {x, y + 1}, {x - 1, y}};
    for (const MapCellCoordinate &cell : fallback)
        candidates.push_back(cell);

    for (const MapCellCoordinate &candidate : candidates)
    {
        if (!isMapCellInside(map, candidate.x, candidate.y))
            continue;
        if (mapGenCanStartStructureApproach(mapCellAt(map, candidate.x, candidate.y)))
            return candidate;
    }

    return {-1, -1};
}

void mapGenConnectStructuresToWays(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    if (options.routeDensity == 0 || options.roadNetwork == MapRoadNetworkMode::None)
        return;

    std::vector<MapCellCoordinate> structures;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell && mapGenIsStructureLike(cell->kind) && !mapGenCellHasStructureApproach(map, x, y))
                structures.push_back({x, y});
        }
    }

    const bool includeInteriorWays = options.scope == MapScope::Dungeon;
    const MapWayKind approachKind = options.scope == MapScope::Dungeon
                                        ? (options.caveNetwork ? MapWayKind::CaveTunnel : MapWayKind::DungeonCorridor)
                                        : mapGenGeneratedRoadKind(options);
    const int maxDistance = std::max(8, std::min(28, (map.width + map.height) / 3));
    for (const MapCellCoordinate &structure : structures)
    {
        const MapCellCoordinate target =
            mapGenNearestWayCell(map, structure.x, structure.y, maxDistance, includeInteriorWays);
        if (target.x < 0)
            continue;
        const MapCellCoordinate start = mapGenChooseStructureApproachStart(map, structure.x, structure.y, target);
        if (start.x < 0)
            continue;
        mapGenDrawWay(map, start.x, start.y, target.x, target.y, approachKind, rng, 4,
                      options.scope == MapScope::Dungeon, options.caveNetwork ? MapTileKind::Cave : MapTileKind::Room,
                      options.scope == MapScope::Dungeon ? 1 : 0);
    }
}

MapWayKind mapGenSecondaryRoadKind(MapWayKind primary)
{
    return mapGenWayKindIsRoadLike(primary) ? primary : MapWayKind::None;
}

void mapGenAddSameFamilyRoadBranches(MapModel &map, const MapGenerationOptions &options, std::mt19937 &rng)
{
    if (options.scope == MapScope::Dungeon || options.routeDensity == 0 ||
        options.roadNetwork == MapRoadNetworkMode::None)
        return;

    const MapWayKind primary = mapGenGeneratedRoadKind(options);
    const MapWayKind branchKind = mapGenSecondaryRoadKind(primary);
    if (branchKind == MapWayKind::None)
        return;

    std::vector<MapCellCoordinate> anchors;
    for (int y = 1; y < map.height - 1; ++y)
    {
        for (int x = 1; x < map.width - 1; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell && cell->wayKind == primary)
                anchors.push_back({x, y});
        }
    }

    if (anchors.empty())
        return;

    const int density = mapGenClampPercent(options.routeDensity);
    const int branchCount = options.roadNetwork == MapRoadNetworkMode::Grid ? std::max(1, density / 35) : 1;
    const int branchRadius = std::max(4, std::min(9, (map.width + map.height) / 10));
    for (int branch = 0; branch < branchCount; ++branch)
    {
        const MapCellCoordinate anchor =
            anchors[static_cast<size_t>(mapGenRandomBetween(rng, 0, static_cast<int>(anchors.size()) - 1))];
        std::vector<MapCellCoordinate> targets;
        for (const MapCellCoordinate &candidate : anchors)
        {
            const int distance = std::abs(candidate.x - anchor.x) + std::abs(candidate.y - anchor.y);
            if (distance >= 4 && distance <= branchRadius * 2)
                targets.push_back(candidate);
        }
        if (targets.empty())
            continue;
        const MapCellCoordinate target =
            targets[static_cast<size_t>(mapGenRandomBetween(rng, 0, static_cast<int>(targets.size()) - 1))];
        mapGenDrawWay(map, anchor.x, anchor.y, target.x, target.y, branchKind, rng, 12);
    }
}
