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

int mapGenV2HydrologyPercent(const MapModel &map)
{
    if (map.generation.continuousRendering)
    {
        if (!mapGenV4SharedGreatRiverIsValid(map))
            return 0;
        int riverFeatures = 0;
        int validFeatures = 0;
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::River)
                continue;
            ++riverFeatures;
            if (feature.points.size() < 2)
                continue;
            // Feature-graph points are spline controls, not necessarily one
            // sample per raster cell. Atlas clipping deliberately retains a
            // long straight segment as two controls; spacing alone therefore
            // cannot indicate a broken river. Reject only malformed/out-of-map
            // controls here and let the raster/bridge gates verify projection.
            const bool continuous =
                std::all_of(feature.points.begin(), feature.points.end(), [&](const MapFeaturePoint &point) {
                    return std::isfinite(point.x) && std::isfinite(point.y) && point.x >= -0.5f && point.y >= -0.5f &&
                           point.x <= static_cast<float>(map.width) + 0.5f &&
                           point.y <= static_cast<float>(map.height) + 0.5f;
                });
            const MapFeaturePoint &end = feature.points.back();
            bool outlet = end.x <= 1.5f || end.y <= 1.5f || end.x >= map.width - 1.5f || end.y >= map.height - 1.5f;
            const int endX = std::clamp(static_cast<int>(std::floor(end.x)), 0, map.width - 1);
            const int endY = std::clamp(static_cast<int>(std::floor(end.y)), 0, map.height - 1);
            for (int offsetY = -1; !outlet && offsetY <= 1; ++offsetY)
                for (int offsetX = -1; !outlet && offsetX <= 1; ++offsetX)
                {
                    const MapCellModel *neighbor = mapCellAt(map, endX + offsetX, endY + offsetY);
                    outlet = neighbor && neighbor->kind == MapTileKind::Water && neighbor->wayKind != MapWayKind::River;
                }
            // Secondary streams legitimately terminate at a confluence. The
            // endpoint is projected onto the main stem (or an earlier
            // tributary), so accept contact with another river feature as a
            // hydrological outlet rather than requiring every tributary to
            // reach the sea independently.
            for (const MapGeneratedFeature &other : map.generation.graph.features)
            {
                if (outlet)
                    break;
                if (&other == &feature || other.kind != MapGeneratedFeatureKind::River)
                    continue;
                outlet = std::any_of(other.points.begin(), other.points.end(), [&](const MapFeaturePoint &point) {
                    const float deltaX = point.x - end.x;
                    const float deltaY = point.y - end.y;
                    return deltaX * deltaX + deltaY * deltaY <= 2.25f;
                });
            }
            if (continuous && outlet)
                ++validFeatures;
        }
        const int outletScore = riverFeatures == 0 ? 100 : validFeatures * 100 / riverFeatures;
        return std::min(outletScore, mapGenV5WorldRiverCompositionPercent(map));
    }
    const int riverCount =
        mapGenCountMatchingCells(map, [](const MapCellModel &cell) { return cell.wayKind == MapWayKind::River; });
    if (riverCount == 0)
        return 100;
    const int connected = mapGenLargestComponentSize(
        map, [](const MapCellModel &cell) { return cell.wayKind == MapWayKind::River; }, true);
    bool reachesEdge = false;
    for (int x = 0; x < map.width; ++x)
    {
        const MapCellModel *top = mapCellAt(map, x, 0);
        const MapCellModel *bottom = mapCellAt(map, x, map.height - 1);
        reachesEdge = reachesEdge || (top && top->wayKind == MapWayKind::River) ||
                      (bottom && bottom->wayKind == MapWayKind::River);
    }
    const int connectivity = connected * 100 / riverCount;
    return std::clamp(connectivity * 80 / 100 + (reachesEdge ? 20 : 0), 0, 100);
}

int mapGenV2BridgeValidityPercent(const MapModel &map)
{
    if (map.generation.enabled)
    {
        int bridgeFeatures = 0;
        int validFeatures = 0;
        for (const MapGeneratedFeature &bridge : map.generation.graph.features)
        {
            if (bridge.kind != MapGeneratedFeatureKind::Bridge || bridge.points.size() < 2)
                continue;
            ++bridgeFeatures;
            if (map.generation.recipe.generatorVersion >= 12 && map.atlasLevel == MapLevel::Site &&
                map.mapKind == MapKind::Settlement && bridge.materialId == "fantasy_neutral.settlement.pier")
            {
                const auto a = bridge.points.front(), b = bridge.points.back();
                const auto *land = mapCellAt(map, int(a.x), int(a.y));
                const auto *tip = mapCellAt(map, int(b.x), int(b.y));
                bool valid = land && tip && land->kind != MapTileKind::Water && land->kind != MapTileKind::Bridge &&
                             mapGenWayKindIsRoadLike(land->wayKind) && tip->kind == MapTileKind::Bridge;
                bool water = false;
                for (auto d : std::array<MapCellCoordinate, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}})
                {
                    const auto *c = mapCellAt(map, int(b.x) + d.x, int(b.y) + d.y);
                    water |= c && c->kind == MapTileKind::Water;
                }
                for (std::size_t j = 1; j < bridge.points.size() && valid; ++j)
                {
                    const auto p = bridge.points[j - 1], q = bridge.points[j];
                    const int n = std::max(1, int(std::ceil(std::hypot(q.x - p.x, q.y - p.y) * 4.f)));
                    for (int k = 0; k <= n; ++k)
                    {
                        float t = float(k) / n;
                        const auto *c = mapCellAt(map, int(p.x + (q.x - p.x) * t), int(p.y + (q.y - p.y) * t));
                        if (!c || c->kind == MapTileKind::Water || c->kind == MapTileKind::Wall ||
                            c->kind == MapTileKind::Building || !mapGenWayKindIsRoadLike(c->wayKind))
                        {
                            valid = false;
                            break;
                        }
                    }
                }
                if (valid && water)
                    ++validFeatures;
                continue;
            }
            const MapFeaturePoint &start = bridge.points.front();
            const MapFeaturePoint &end = bridge.points.back();
            const float centerX = (start.x + end.x) * 0.5f;
            const float centerY = (start.y + end.y) * 0.5f;
            float roadX = 0.0f;
            float roadY = 0.0f;
            float riverX = 0.0f;
            float riverY = 0.0f;
            bool hasRoad = mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Road, centerX,
                                                         centerY, 2.0f, roadX, roadY);
            if (!hasRoad)
                hasRoad = mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Trail, centerX,
                                                        centerY, 2.0f, roadX, roadY);
            const bool hasRiver = mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::River,
                                                                centerX, centerY, 2.0f, riverX, riverY);
            const float crossingDot = std::abs(roadX * riverX + roadY * riverY);
            const float bridgeLength =
                std::sqrt((end.x - start.x) * (end.x - start.x) + (end.y - start.y) * (end.y - start.y));
            // Oblique crossings down to roughly 35 degrees remain readable
            // and structurally plausible; shallower alignments become roads
            // running along the channel and are rejected.
            if (hasRoad && hasRiver && crossingDot <= 0.82f && bridgeLength >= 0.75f)
                ++validFeatures;
        }

        int roadWaterIncidents = 0;
        int validRoadWaterIncidents = 0;
        const auto hasBridgeNear = [&](float x, float y) {
            float bridgeX = 0.0f;
            float bridgeY = 0.0f;
            return mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Bridge, x, y, 4.0f,
                                                 bridgeX, bridgeY);
        };
        if (map.generation.recipe.generatorVersion >= 4)
        {
            for (const MapGeneratedFeature &road : map.generation.graph.features)
            {
                if ((road.kind != MapGeneratedFeatureKind::Road && road.kind != MapGeneratedFeatureKind::Trail) ||
                    road.points.empty())
                    continue;
                for (const MapFeaturePoint &point : road.points)
                {
                    const int x = std::clamp(static_cast<int>(std::floor(point.x)), 0, map.width - 1);
                    const int y = std::clamp(static_cast<int>(std::floor(point.y)), 0, map.height - 1);
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell || (cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Bridge))
                        continue;
                    ++roadWaterIncidents;
                    // A road center in raw Water is always invalid. Rasterization
                    // must publish a Bridge cell and a matching editable feature.
                    if (cell->kind == MapTileKind::Bridge && hasBridgeNear(point.x, point.y))
                        ++validRoadWaterIncidents;
                }
                for (std::size_t pointIndex = 1; pointIndex < road.points.size(); ++pointIndex)
                {
                    const int startX =
                        std::clamp(static_cast<int>(std::floor(road.points[pointIndex - 1].x)), 0, map.width - 1);
                    const int startY =
                        std::clamp(static_cast<int>(std::floor(road.points[pointIndex - 1].y)), 0, map.height - 1);
                    const int endX =
                        std::clamp(static_cast<int>(std::floor(road.points[pointIndex].x)), 0, map.width - 1);
                    const int endY =
                        std::clamp(static_cast<int>(std::floor(road.points[pointIndex].y)), 0, map.height - 1);
                    if (startX == endX || startY == endY)
                        continue;
                    const MapCellModel *sideX = mapCellAt(map, endX, startY);
                    const MapCellModel *sideY = mapCellAt(map, startX, endY);
                    const bool wetCorner =
                        (sideX && (sideX->kind == MapTileKind::Water || sideX->kind == MapTileKind::Bridge)) ||
                        (sideY && (sideY->kind == MapTileKind::Water || sideY->kind == MapTileKind::Bridge));
                    if (!wetCorner)
                        continue;
                    ++roadWaterIncidents;
                    const float centerX = (road.points[pointIndex - 1].x + road.points[pointIndex].x) * 0.5f;
                    const float centerY = (road.points[pointIndex - 1].y + road.points[pointIndex].y) * 0.5f;
                    if (hasBridgeNear(centerX, centerY))
                        ++validRoadWaterIncidents;
                }
            }
        }

        int graphScore = 100;
        if (bridgeFeatures > 0)
            graphScore = validFeatures * 100 / bridgeFeatures;
        if (roadWaterIncidents > 0)
            graphScore = std::min(graphScore, validRoadWaterIncidents * 100 / roadWaterIncidents);
        if (bridgeFeatures > 0 || roadWaterIncidents > 0)
            return graphScore;
    }

    int bridges = 0;
    int valid = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind != MapTileKind::Bridge)
                continue;
            ++bridges;
            const MapCellModel *north = mapCellAt(map, x, y - 1);
            const MapCellModel *east = mapCellAt(map, x + 1, y);
            const MapCellModel *south = mapCellAt(map, x, y + 1);
            const MapCellModel *west = mapCellAt(map, x - 1, y);
            const bool waterNorthSouth =
                north && south && mapGenRiverInfluenceCell(north) && mapGenRiverInfluenceCell(south);
            const bool waterEastWest = east && west && mapGenRiverInfluenceCell(east) && mapGenRiverInfluenceCell(west);
            const bool roadNorthSouth =
                north && south && mapGenWayKindIsRoadLike(north->wayKind) && mapGenWayKindIsRoadLike(south->wayKind);
            const bool roadEastWest =
                east && west && mapGenWayKindIsRoadLike(east->wayKind) && mapGenWayKindIsRoadLike(west->wayKind);
            if ((waterNorthSouth && roadEastWest) || (waterEastWest && roadNorthSouth))
                ++valid;
        }
    }
    return bridges == 0 ? 100 : valid * 100 / bridges;
}

