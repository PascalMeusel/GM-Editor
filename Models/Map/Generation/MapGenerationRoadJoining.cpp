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


bool mapGenV7NearestDryRoadAnchor(const MapModel &map, MapFeaturePoint from, MapFeaturePoint &anchor)
{
    float bestSquared = std::numeric_limits<float>::max();
    bool found = false;
    const auto consider = [&](MapFeaturePoint point) {
        const int x = static_cast<int>(point.x), y = static_cast<int>(point.y);
        const auto *cell = mapCellAt(map, x, y);
        if (!cell || cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Bridge ||
            cell->kind == MapTileKind::Mountain || !mapGenV7RoadChordIsSafe(map, {x + 0.5f, y + 0.5f}, point))
            return;
        const float dx = point.x - from.x, dy = point.y - from.y;
        const float squared = dx * dx + dy * dy;
        if (squared < bestSquared)
        {
            bestSquared = squared;
            anchor = point;
            found = true;
        }
    };
    for (const auto &road : map.generation.graph.features)
    {
        if (road.kind != MapGeneratedFeatureKind::Road && road.kind != MapGeneratedFeatureKind::Trail)
            continue;
        for (std::size_t index = 1; index < road.points.size(); ++index)
        {
            const auto a = road.points[index - 1], b = road.points[index];
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float lengthSquared = dx * dx + dy * dy;
            const float t = lengthSquared > 0.000001f
                                ? std::clamp(((from.x - a.x) * dx + (from.y - a.y) * dy) / lengthSquared, 0.0f, 1.0f)
                                : 0.0f;
            consider({a.x + dx * t, a.y + dy * t});
            // A nearest projection in a bridge span is not an appropriate
            // bank junction; retain dry segment ends as fallback candidates.
            consider(a);
            consider(b);
        }
    }
    return found;
}

void mapGenV7JoinFirstExistingRoad(MapGeneratedFeature &road, const MapFeatureGraph &graph,
                                   const std::vector<std::string> *eligibleIds, const MapModel *junctionMap)
{
    // New branches run from the new destination toward the existing network.
    // Stop at the first actual vector intersection; fitting cannot leave an
    // overdrawn triangle or a line crossing with no shared junction control.
    for (std::size_t index = 1; index < road.points.size(); ++index)
    {
        const auto a = road.points[index - 1], b = road.points[index];
        const float rx = b.x - a.x, ry = b.y - a.y;
        float earliest = 2.0f;
        MapFeaturePoint join;
        for (const auto &existing : graph.features)
        {
            if (existing.id == road.id ||
                (existing.kind != MapGeneratedFeatureKind::Road && existing.kind != MapGeneratedFeatureKind::Trail))
                continue;
            if (eligibleIds && std::find(eligibleIds->begin(), eligibleIds->end(), existing.id) == eligibleIds->end())
                continue;
            for (std::size_t segment = 1; segment < existing.points.size(); ++segment)
            {
                const auto c = existing.points[segment - 1], d = existing.points[segment];
                const float sx = d.x - c.x, sy = d.y - c.y;
                if (junctionMap && index > 1 && earliest > 1.f)
                {
                    const float squared = sx * sx + sy * sy;
                    const float u =
                        squared > 1.e-8f ? std::clamp(((b.x - c.x) * sx + (b.y - c.y) * sy) / squared, 0.f, 1.f) : 0.f;
                    const MapFeaturePoint junctionPoint{c.x + sx * u, c.y + sy * u};
                    // A fitted branch may touch an existing road shoulder
                    // without crossing its mathematical centerline. Snap
                    // that real near-junction before a thin lens is drawn.
                    if (std::hypot(junctionPoint.x - b.x, junctionPoint.y - b.y) <= .65f &&
                        mapGenV7RoadChordIsSafe(*junctionMap, a, junctionPoint))
                    {
                        earliest = 1.f;
                        join = junctionPoint;
                    }
                }
                const float denominator = rx * sy - ry * sx;
                if (std::abs(denominator) < 0.000001f)
                {
                    const float squaredLength = rx * rx + ry * ry;
                    if (squaredLength < 0.000001f || std::abs((c.x - a.x) * ry - (c.y - a.y) * rx) > 0.00001f)
                        continue;
                    const float first = ((c.x - a.x) * rx + (c.y - a.y) * ry) / squaredLength;
                    const float second = ((d.x - a.x) * rx + (d.y - a.y) * ry) / squaredLength;
                    const float begin = std::max(0.0f, std::min(first, second));
                    const float end = std::min(1.0f, std::max(first, second));
                    if (begin <= end && (index > 1 || begin > 0.00001f) && begin < earliest)
                    {
                        earliest = begin;
                        join = {a.x + rx * begin, a.y + ry * begin};
                    }
                    continue;
                }
                const float t = ((c.x - a.x) * sy - (c.y - a.y) * sx) / denominator;
                const float u = ((c.x - a.x) * ry - (c.y - a.y) * rx) / denominator;
                if (t >= -0.00001f && t <= 1.00001f && u >= -0.00001f && u <= 1.00001f && (index > 1 || t > 0.00001f) &&
                    t < earliest)
                {
                    earliest = std::clamp(t, 0.0f, 1.0f);
                    join = {a.x + rx * earliest, a.y + ry * earliest};
                }
            }
        }
        if (earliest <= 1.0f)
        {
            road.points.resize(index);
            if (earliest > 0.00001f)
                road.points.push_back(join);
            return;
        }
    }
}

MapFeaturePoint mapGenV11RoadBoundaryAnchor(const MapModel &map, MapCellCoordinate cell)
{
    MapFeaturePoint point{cell.x + .5f, cell.y + .5f};
    if (cell.x == 0)
        point.x = .015f;
    else if (cell.x == map.width - 1)
        point.x = map.width - .015f;
    if (cell.y == 0)
        point.y = .015f;
    else if (cell.y == map.height - 1)
        point.y = map.height - .015f;
    return point;
}

void mapGenV5SmoothWorldRoadPathFeature(const MapModel &map, MapGeneratedFeature &feature, int passes)
{
    if (map.generation.recipe.generatorVersion >= 7 &&
        (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region))
    {
        mapGenV7FitOverviewRoadPathFeature(map, feature);
        return;
    }
    if (feature.points.size() < 4)
        return;
    std::vector<MapFeaturePoint> smoothed = feature.points;

    // Cost-aware raster routing occasionally walks one cell around a coastal
    // or mountain corner and immediately doubles back. That detour is valid
    // as grid connectivity, but retaining its middle control produces an
    // almost 180-degree cartographic hairpin. Remove the redundant control
    // only when the direct rendered segment is independently terrain-safe.
    bool removedHairpin = true;
    while (removedHairpin && smoothed.size() >= 4)
    {
        removedHairpin = false;
        for (std::size_t index = 1; index + 1 < smoothed.size(); ++index)
        {
            const float incomingX = smoothed[index].x - smoothed[index - 1].x;
            const float incomingY = smoothed[index].y - smoothed[index - 1].y;
            const float outgoingX = smoothed[index + 1].x - smoothed[index].x;
            const float outgoingY = smoothed[index + 1].y - smoothed[index].y;
            const float divisor = std::sqrt((incomingX * incomingX + incomingY * incomingY) *
                                            (outgoingX * outgoingX + outgoingY * outgoingY));
            if (divisor <= 0.0001f || (incomingX * outgoingX + incomingY * outgoingY) / divisor >= 0.18f ||
                !mapGenV5WorldRoadSegmentIsSafe(map, smoothed[index - 1], smoothed[index + 1]) ||
                !mapGenV5WorldRoadSegmentIsDry(map, smoothed[index - 1], smoothed[index + 1]))
                continue;
            smoothed.erase(smoothed.begin() + static_cast<std::ptrdiff_t>(index));
            removedHairpin = true;
            break;
        }
    }
    for (int pass = 0; pass < passes; ++pass)
    {
        std::vector<MapFeaturePoint> next = smoothed;
        for (std::size_t index = 1; index + 1 < smoothed.size(); ++index)
        {
            MapFeaturePoint candidate;
            candidate.x = smoothed[index - 1].x * 0.22f + smoothed[index].x * 0.56f + smoothed[index + 1].x * 0.22f;
            candidate.y = smoothed[index - 1].y * 0.22f + smoothed[index].y * 0.56f + smoothed[index + 1].y * 0.22f;
            // Smoothing is purely cartographic and must never move the
            // editable centerline off the traversable raster corridor. In a
            // concave bay, averaging two dry controls can otherwise cut a
            // long visible chord across open water. Keep explicit river and
            // bridge controls fixed as well: crossing geometry is integrated
            // later from those exact raster cells.
            if (mapGenV5WorldRoadSegmentIsSafe(map, smoothed[index - 1], candidate) &&
                mapGenV5WorldRoadSegmentIsSafe(map, candidate, smoothed[index + 1]) &&
                mapGenV5WorldRoadSegmentIsDry(map, smoothed[index - 1], candidate) &&
                mapGenV5WorldRoadSegmentIsDry(map, candidate, smoothed[index + 1]))
                next[index] = candidate;
        }
        smoothed.swap(next);
    }

    // Hairpin removal deliberately replaces a two-cell raster detour with a
    // direct cartographic segment. Keep the resulting editable centerline
    // sampled densely enough to remain one continuous world-road feature;
    // consumers use those stable controls for hit testing, partial
    // regeneration and QA rather than inferring continuity from the raster.
    std::vector<MapFeaturePoint> resampled;
    resampled.reserve(smoothed.size());
    if (!smoothed.empty())
        resampled.push_back(smoothed.front());
    for (std::size_t index = 1; index < smoothed.size(); ++index)
    {
        const MapFeaturePoint &start = smoothed[index - 1];
        const MapFeaturePoint &end = smoothed[index];
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        const int segments = std::max(1, static_cast<int>(std::ceil(length / 1.5f)));
        for (int segment = 1; segment <= segments; ++segment)
        {
            const float amount = static_cast<float>(segment) / static_cast<float>(segments);
            resampled.push_back({start.x + deltaX * amount, start.y + deltaY * amount});
        }
    }
    feature.points = std::move(resampled);
}

void mapGenV7CommitFittedRoadProjection(MapModel &map, const MapGeneratedFeature &feature,
                                        std::vector<MapCellModel> previousCells, MapWayKind roadKind)
{
    // Fitting needs the proposed pass/bridge projection to validate its
    // corridor, but trimming may discard a long suffix of that proposal.
    // Publish only the retained geometry over the pre-branch terrain state.
    map.cells = std::move(previousCells);
    std::vector<MapCellCoordinate> fittedCells;
    for (std::size_t segment = 1; segment < feature.points.size(); ++segment)
    {
        const auto a = feature.points[segment - 1], b = feature.points[segment];
        const int samples = std::max(1, static_cast<int>(std::ceil(std::hypot(b.x - a.x, b.y - a.y) * 4.0f)));
        for (int sample = 0; sample <= samples; ++sample)
        {
            const float t = static_cast<float>(sample) / samples;
            const MapCellCoordinate point{static_cast<int>(a.x + (b.x - a.x) * t),
                                          static_cast<int>(a.y + (b.y - a.y) * t)};
            if (fittedCells.empty() || !(fittedCells.back() == point))
                fittedCells.push_back(point);
        }
    }
    mapGenV2RasterizeRoad(map, fittedCells, roadKind);
}

