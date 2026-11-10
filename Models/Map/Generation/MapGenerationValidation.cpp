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

bool mapGenQualityTraversable(const MapCellModel &cell)
{
    return cell.movementCost < 99 && cell.movementBlockedMask != 0x0f;
}

bool mapGenQualityWayCell(const MapCellModel &cell, MapScope scope)
{
    if (scope == MapScope::Dungeon)
        return mapWayKindIsInteriorLike(cell.wayKind);
    return mapGenWayKindIsRoadLike(cell.wayKind);
}

bool mapGenCoordinatesConnected(const MapModel &map, const MapCellCoordinate &start, const MapCellCoordinate &end,
                                bool requireWays)
{
    if (!isMapCellInside(map, start.x, start.y) || !isMapCellInside(map, end.x, end.y))
        return false;

    const auto allowed = [&](const MapCellModel &cell) {
        return requireWays ? mapGenQualityWayCell(cell, map.scope) : mapGenQualityTraversable(cell);
    };
    const MapCellModel *startCell = mapCellAt(map, start.x, start.y);
    const MapCellModel *endCell = mapCellAt(map, end.x, end.y);
    if (!startCell || !endCell || !allowed(*startCell) || !allowed(*endCell))
        return false;

    std::vector<bool> visited(map.cells.size(), false);
    std::queue<MapCellCoordinate> pending;
    pending.push(start);
    visited[static_cast<std::size_t>(start.y * map.width + start.x)] = true;
    while (!pending.empty())
    {
        const MapCellCoordinate current = pending.front();
        pending.pop();
        if (current.x == end.x && current.y == end.y)
            return true;
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0)
                    continue;
                const int nextX = current.x + dx;
                const int nextY = current.y + dy;
                if (!isMapCellInside(map, nextX, nextY))
                    continue;
                const int nextIndex = nextY * map.width + nextX;
                if (visited[static_cast<std::size_t>(nextIndex)])
                    continue;
                const MapCellModel *next = mapCellAt(map, nextX, nextY);
                if (!next || !allowed(*next))
                    continue;
                visited[static_cast<std::size_t>(nextIndex)] = true;
                pending.push({nextX, nextY});
            }
        }
    }
    return false;
}

int mapGenCoherentTerrainPercent(const MapModel &map)
{
    int evaluated = 0;
    int coherent = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind == MapTileKind::Empty || cell->kind == MapTileKind::Wall)
                continue;
            // Props, entrances and one-cell interaction markers are deliberate
            // overlays, not terrain regions. Counting their lack of an
            // identical neighboring prop made otherwise coherent dungeon
            // floors score as low as 11%. Room and Cave cells, on the other
            // hand, are the actual subterranean terrain and must be evaluated.
            if (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building ||
                cell->kind == MapTileKind::Landmark || cell->kind == MapTileKind::Bridge ||
                cell->kind == MapTileKind::Door || cell->kind == MapTileKind::Stairs ||
                cell->kind == MapTileKind::Cover || cell->kind == MapTileKind::Rubble ||
                cell->kind == MapTileKind::Hazard || cell->kind == MapTileKind::Pit || cell->kind == MapTileKind::Chasm)
                continue;
            ++evaluated;
            bool hasFamilyNeighbor = false;
            for (int dy = -1; dy <= 1 && !hasFamilyNeighbor; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0)
                        continue;
                    const MapCellModel *neighbor = mapCellAt(map, x + dx, y + dy);
                    if (neighbor && neighbor->kind == cell->kind)
                    {
                        hasFamilyNeighbor = true;
                        break;
                    }
                }
            }
            if (hasFamilyNeighbor || mapGenIsStructureLike(cell->kind) || cell->kind == MapTileKind::Stairs ||
                cell->kind == MapTileKind::Door || cell->kind == MapTileKind::Bridge)
                ++coherent;
        }
    }
    return evaluated == 0 ? 100 : coherent * 100 / evaluated;
}

int mapGenV2RoadEfficiencyPercent(const MapModel &map)
{
    int evaluated = 0;
    float scoreTotal = 0.0f;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if ((feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail) ||
            feature.points.size() < 2)
            continue;
        float length = 0.0f;
        for (std::size_t i = 1; i < feature.points.size(); ++i)
        {
            const float dx = feature.points[i].x - feature.points[i - 1].x;
            const float dy = feature.points[i].y - feature.points[i - 1].y;
            length += std::sqrt(dx * dx + dy * dy);
        }
        const float dx = feature.points.back().x - feature.points.front().x;
        const float dy = feature.points.back().y - feature.points.front().y;
        const float direct = std::sqrt(dx * dx + dy * dy);
        if (length > 0.01f)
        {
            // A declared ring road is an intentional loop. Comparing its
            // identical start/end points to its length would incorrectly rate
            // every useful city ring as a complete detour.
            //
            // Open roads are cost-routed around water, cliffs, buildings and
            // steep terrain.  The old direct/length percentage treated every
            // necessary topographic diversion as wasted road (a perfectly
            // reasonable 1.5x mountain road scored only 67).  Keep a strict
            // penalty for paths longer than roughly twice the direct distance,
            // while giving normal terrain-following roads the expected
            // 80--100 quality range.
            const float directness = feature.closed ? 1.0f : std::clamp(direct / length, 0.0f, 1.0f);
            const float routeScore = feature.closed ? 1.0f : std::clamp((directness - 0.20f) / 0.55f, 0.0f, 1.0f);
            scoreTotal += routeScore;
            ++evaluated;
        }
    }
    return evaluated == 0 ? 100 : static_cast<int>(std::round(scoreTotal * 100.0f / static_cast<float>(evaluated)));
}

bool mapGenV4PointInsidePolygon(const MapFeaturePoint &point, const std::vector<MapFeaturePoint> &polygon)
{
    if (polygon.size() < 3)
        return false;
    bool inside = false;
    for (std::size_t index = 0, previous = polygon.size() - 1; index < polygon.size(); previous = index++)
    {
        const MapFeaturePoint &a = polygon[index];
        const MapFeaturePoint &b = polygon[previous];
        const bool verticalSpan = (a.y > point.y) != (b.y > point.y);
        if (!verticalSpan)
            continue;
        const float intersectionX = (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
        if (point.x < intersectionX)
            inside = !inside;
    }
    return inside;
}

bool mapGenV4PointTouchesPolygon(const MapFeaturePoint &point, const std::vector<MapFeaturePoint> &polygon,
                                 float boundaryDistance)
{
    if (mapGenV4PointInsidePolygon(point, polygon))
        return true;
    const float maximumSquared = boundaryDistance * boundaryDistance;
    for (std::size_t index = 0; index < polygon.size(); ++index)
    {
        const MapFeaturePoint &start = polygon[index];
        const MapFeaturePoint &end = polygon[(index + 1) % polygon.size()];
        const float segmentX = end.x - start.x;
        const float segmentY = end.y - start.y;
        const float lengthSquared = segmentX * segmentX + segmentY * segmentY;
        if (lengthSquared <= 0.0001f)
            continue;
        const float projection =
            std::clamp(((point.x - start.x) * segmentX + (point.y - start.y) * segmentY) / lengthSquared, 0.0f, 1.0f);
        const float deltaX = point.x - (start.x + segmentX * projection);
        const float deltaY = point.y - (start.y + segmentY * projection);
        if (deltaX * deltaX + deltaY * deltaY <= maximumSquared)
            return true;
    }
    return false;
}

bool mapGenV4SharedGreatRiverIsValid(const MapModel &map, std::string *failureCode)
{
    if (failureCode)
        failureCode->clear();
    const auto fail = [&](const char *code) {
        if (failureCode)
            *failureCode = code;
        return false;
    };
    if (!mapGenV4RequestsSharedGreatRiver(map))
        return true;

    const MapGeneratedFeature *river = nullptr;
    int lockedMainRiverCount = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::River)
            continue;
        if (feature.locked && feature.width >= 1.35f)
        {
            ++lockedMainRiverCount;
            river = &feature;
        }
    }
    // Tributaries are independent river features. The shared-river promise is
    // fulfilled by exactly one prominent, locked main stem; requiring the
    // entire drainage basin to contain only one feature rejected every
    // geographically useful tributary.
    if (lockedMainRiverCount != 1 || !river || river->points.size() < 2)
        return fail("river.shared.channel_missing");

    float length = 0.0f;
    for (std::size_t index = 1; index < river->points.size(); ++index)
    {
        const float deltaX = river->points[index].x - river->points[index - 1].x;
        const float deltaY = river->points[index].y - river->points[index - 1].y;
        const float segment = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        if (segment > 1.6f)
            return fail("river.shared.channel_gap");
        length += segment;
    }
    // Portrait-oriented basins should not fail merely because the map is much
    // wider than the actual watershed. Sixty percent of the shorter dimension
    // rejects the old 10--15-cell stubs while accepting a major coast-to-coast
    // river such as the north/south 41006 regression basin.
    const float minimumLength = std::max(8.0f, static_cast<float>(std::min(map.width, map.height)) * 0.60f);
    if (length < minimumLength)
        return fail("river.shared.too_short");

    const MapFeaturePoint &start = river->points.front();
    const MapFeaturePoint &end = river->points.back();
    const int startX = std::clamp(static_cast<int>(std::floor(start.x)), 0, map.width - 1);
    const int startY = std::clamp(static_cast<int>(std::floor(start.y)), 0, map.height - 1);
    const int unreachable = map.width + map.height + 1;
    std::vector<int> coastDistance(static_cast<std::size_t>(map.width * map.height), unreachable);
    std::queue<MapCellCoordinate> pending;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            if (!mapGenV5WorldOpenWaterAt(map, x, y))
                continue;
            // Only raster cells that belong to the channel are excluded
            // above. Excluding every ocean cell near the spline hid exactly
            // the invalid case this field is meant to detect: a source drawn
            // against another coastline looked coast-to-coast but received an
            // artificially large inland distance.
            const int index = mapCellIndex(map, x, y);
            coastDistance[static_cast<std::size_t>(index)] = 0;
            pending.push({x, y});
        }
    constexpr MapCellCoordinate cardinal[] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    while (!pending.empty())
    {
        const MapCellCoordinate current = pending.front();
        pending.pop();
        const int currentIndex = mapCellIndex(map, current.x, current.y);
        for (const MapCellCoordinate direction : cardinal)
        {
            if (!isMapCellInside(map, current.x + direction.x, current.y + direction.y))
                continue;
            const int nextIndex = mapCellIndex(map, current.x + direction.x, current.y + direction.y);
            if (coastDistance[static_cast<std::size_t>(nextIndex)] <=
                coastDistance[static_cast<std::size_t>(currentIndex)] + 1)
                continue;
            coastDistance[static_cast<std::size_t>(nextIndex)] =
                coastDistance[static_cast<std::size_t>(currentIndex)] + 1;
            pending.push({current.x + direction.x, current.y + direction.y});
        }
    }
    int deepestLandCell = 0;
    for (int y = 1; y < map.height - 1; ++y)
        for (int x = 1; x < map.width - 1; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || (cell->kind == MapTileKind::Water && cell->wayKind != MapWayKind::River))
                continue;
            deepestLandCell =
                std::max(deepestLandCell, coastDistance[static_cast<std::size_t>(mapCellIndex(map, x, y))]);
        }
    const int requiredClearance =
        std::max(1, std::min(std::max(2, std::min(map.width, map.height) / 6), deepestLandCell));
    const int sourceIndex = mapCellIndex(map, startX, startY);
    if (sourceIndex < 0 || coastDistance[static_cast<std::size_t>(sourceIndex)] < requiredClearance)
        return fail("river.shared.source_too_coastal");
    // The ordered feature contract is source -> outlet. Even on a small world
    // the source must not touch open sea, while the last point must. This
    // explicit asymmetric gate prevents a technically downhill coast-to-coast
    // channel from masquerading as a river.
    if (mapGenV5WorldOpenWaterDistance(map, startX, startY, 2) <= 2)
        return fail("river.shared.source_touches_ocean");

    bool hasOutlet = end.x <= 1.5f || end.y <= 1.5f || end.x >= map.width - 1.5f || end.y >= map.height - 1.5f;
    const int endX = std::clamp(static_cast<int>(std::floor(end.x)), 0, map.width - 1);
    const int endY = std::clamp(static_cast<int>(std::floor(end.y)), 0, map.height - 1);
    for (int offsetY = -1; !hasOutlet && offsetY <= 1; ++offsetY)
        for (int offsetX = -1; !hasOutlet && offsetX <= 1; ++offsetX)
        {
            const MapCellModel *neighbor = mapCellAt(map, endX + offsetX, endY + offsetY);
            hasOutlet = neighbor && neighbor->kind == MapTileKind::Water && neighbor->wayKind != MapWayKind::River;
        }
    if (!hasOutlet)
        return fail("river.shared.outlet_missing");
    const MapCellModel *sourceCell = mapCellAt(map, startX, startY);
    const MapCellModel *mouthCell = mapCellAt(map, endX, endY);
    if (!sourceCell || !mouthCell || sourceCell->elevation < mouthCell->elevation + 4 || sourceCell->elevation < 7)
        return fail("river.shared.not_downhill");

    int territories = 0;
    int touchedTerritories = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::TerrainRegion || !feature.closed ||
            feature.materialId.rfind("fantasy_neutral.territory.", 0) != 0)
            continue;
        ++territories;
        const bool touched = std::any_of(river->points.begin(), river->points.end(), [&](const MapFeaturePoint &point) {
            // A major river may form the political border itself. Treat a
            // nation as sharing it when the channel lies inside its territory
            // or directly on that editable boundary.
            return mapGenV4PointTouchesPolygon(point, feature.points);
        });
        touchedTerritories += touched ? 1 : 0;
    }
    if (territories > 0 && touchedTerritories != territories)
        return fail("river.shared.nation_access_missing");

    return true;
}
