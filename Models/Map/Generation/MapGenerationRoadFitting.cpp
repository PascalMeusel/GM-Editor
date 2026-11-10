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


void mapGenV7FitOverviewRoadPathFeature(const MapModel &map, MapGeneratedFeature &feature)
{
    if (feature.locked || feature.points.size() < (map.generation.recipe.generatorVersion >= 9 ? 2u : 3u))
        return;
    const auto original = feature.points;
    const auto cornerEffort = [&](MapFeaturePoint a, MapFeaturePoint b) {
        if (map.generation.recipe.generatorVersion < 10)
            return mapGenV7RoadSegmentEffort(map, a, b);
        // Existing road rasterization lowers its own elevation. Cost a bend
        // against continuous terrain so that this projection is not mistaken
        // for a natural cliff preventing every rounded alignment.
        const float length = std::hypot(b.x - a.x, b.y - a.y);
        const int steps = std::max(1, static_cast<int>(std::ceil(length * 4.f)));
        const int scale = std::max(8, std::min(map.width, map.height) / 2);
        const auto relief = [&](float x, float y) {
            return mapGenV2FractalNoise(map.generation.recipe.seed + 17u, x, y, scale) +
                   (1.f - y / std::max(1.f, static_cast<float>(map.height - 1))) * .18f;
        };
        float effort = 0.f, prior = relief(a.x, a.y);
        for (int step = 1; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / steps, x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t;
            const auto *cell = mapCellAt(map, static_cast<int>(x), static_cast<int>(y));
            if (!cell)
                return std::numeric_limits<float>::max();
            const float height = relief(x, y);
            const float cover = cell->kind == MapTileKind::Swamp ? 1.f : cell->kind == MapTileKind::Forest ? .44f : 0.f;
            effort += length / steps * (1.f + cover + height * .65f) + std::abs(height - prior) * 2.f;
            prior = height;
        }
        return effort;
    };
    std::vector<float> cumulative(original.size(), 0.0f);
    for (std::size_t index = 1; index < original.size(); ++index)
        cumulative[index] =
            cumulative[index - 1] + (map.generation.recipe.generatorVersion >= 11
                                         ? cornerEffort(original[index - 1], original[index])
                                         : mapGenV7RoadSegmentEffort(map, original[index - 1], original[index]));
    std::vector<MapFeaturePoint> alignment{original.front()};
    for (std::size_t from = 0; from + 1 < original.size();)
    {
        std::size_t chosen = from + 1;
        for (std::size_t to = from + 2; to < original.size() && to <= from + 24; ++to)
        {
            // A shortcut may remove grid stair-steps but must not cut across
            // the expensive hillside, wood or marsh that the route avoided.
            if (!mapGenV7RoadChordIsSafe(map, original[from], original[to]))
                continue;
            if ((map.generation.recipe.generatorVersion >= 11
                     ? cornerEffort(original[from], original[to])
                     : mapGenV7RoadSegmentEffort(map, original[from], original[to])) <=
                (cumulative[to] - cumulative[from]) * 1.04f)
                chosen = to;
        }
        alignment.push_back(original[chosen]);
        from = chosen;
    }
    if (map.generation.recipe.generatorVersion >= 11 && alignment.size() > 2)
    {
        // Round structural bends before the long reaches are subdivided into
        // short bowed controls. Otherwise the later corner radius shrinks to
        // a fraction of one cell and leaves visibly square grid turns.
        std::vector<MapFeaturePoint> broad{alignment.front()};
        for (std::size_t i = 1; i + 1 < alignment.size(); ++i)
        {
            const auto a = alignment[i - 1], b = alignment[i], c = alignment[i + 1];
            const float incoming = std::hypot(b.x - a.x, b.y - a.y), outgoing = std::hypot(c.x - b.x, c.y - b.y);
            const float radius = std::min({2.4f, incoming * .30f, outgoing * .30f});
            if (radius < .35f)
            {
                broad.push_back(b);
                continue;
            }
            const MapFeaturePoint entry{b.x + (a.x - b.x) * radius / incoming, b.y + (a.y - b.y) * radius / incoming};
            const MapFeaturePoint exitPoint{b.x + (c.x - b.x) * radius / outgoing,
                                            b.y + (c.y - b.y) * radius / outgoing};
            std::vector<MapFeaturePoint> bend{entry};
            bool safe = mapGenV7RoadChordIsSafe(map, broad.back(), entry);
            float effort = 0.f;
            for (int step = 1; step <= 8; ++step)
            {
                const float t = step / 8.f, u = 1.f - t;
                const MapFeaturePoint point{u * u * entry.x + 2.f * u * t * b.x + t * t * exitPoint.x,
                                            u * u * entry.y + 2.f * u * t * b.y + t * t * exitPoint.y};
                safe &= mapGenV7RoadChordIsSafe(map, bend.back(), point);
                effort += cornerEffort(bend.back(), point);
                bend.push_back(point);
            }
            safe &= mapGenV7RoadChordIsSafe(map, exitPoint, c);
            if (safe && effort <= (cornerEffort(entry, b) + cornerEffort(b, exitPoint)) * 1.04f)
                broad.insert(broad.end(), bend.begin(), bend.end());
            else
                broad.push_back(b);
        }
        broad.push_back(alignment.back());
        alignment = std::move(broad);
    }
    if (map.generation.recipe.generatorVersion >= 9)
    {
        // Old paths are deliberately untouched. New paths may follow a
        // gently bowed alignment between fixed travel anchors when the full
        // shoulder stays dry and the extra terrain effort is small.
        const auto alignmentEffort = [&](MapFeaturePoint a, MapFeaturePoint b) {
            // Rasterizing an existing road lowers its cells. Compare new
            // alignments using the original continuous relief, so that the
            // first straight road does not win merely because it was drawn.
            const float length = std::hypot(b.x - a.x, b.y - a.y);
            const int steps = std::max(1, static_cast<int>(std::ceil(length * 4.f)));
            const int scale = std::max(8, std::min(map.width, map.height) / 2);
            const auto relief = [&](float x, float y) {
                return mapGenV2FractalNoise(map.generation.recipe.seed + 17u, x, y, scale) +
                       (1.f - y / std::max(1.f, static_cast<float>(map.height - 1))) * .18f;
            };
            float effort = 0.f, prior = relief(a.x, a.y);
            for (int step = 1; step <= steps; ++step)
            {
                const float t = static_cast<float>(step) / steps;
                const float x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t;
                const auto *cell = mapCellAt(map, static_cast<int>(x), static_cast<int>(y));
                if (!cell)
                    return std::numeric_limits<float>::max();
                const float height = relief(x, y);
                const float cover = cell->kind == MapTileKind::Swamp    ? 1.f
                                    : cell->kind == MapTileKind::Forest ? .44f
                                                                        : 0.f;
                effort += length / steps * (1.f + cover + height * .65f) + std::abs(height - prior) * 2.f;
                prior = height;
            }
            return effort;
        };
        std::vector<MapFeaturePoint> natural{alignment.front()};
        for (std::size_t i = 1; i < alignment.size(); ++i)
        {
            const auto a = alignment[i - 1], b = alignment[i];
            const float dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
            std::vector<MapFeaturePoint> best;
            float bestEffort = std::numeric_limits<float>::max();
            const float minimumBendSpan = map.generation.recipe.generatorVersion >= 11 ? 3.5f : 6.f;
            if (length >= minimumBendSpan && mapGenV7RoadChordIsSafe(map, a, b))
            {
                const float phase = mapGenV2Hash01(map.generation.recipe.seed, static_cast<int>(a.x * 11 + b.x * 7),
                                                   static_cast<int>(a.y * 13 + b.y * 5)) *
                                    6.2831853f;
                const bool overviewV10 = map.generation.recipe.generatorVersion >= 10 &&
                                         (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region);
                const float amplitude = overviewV10 ? std::min(3.0f, length * .16f) : std::min(2.15f, length * .12f);
                const float reference = alignmentEffort(a, b);
                const int steps = static_cast<int>(std::ceil(length / .70f));
                const bool adaptiveBend = map.generation.recipe.generatorVersion >= 11;
                for (int bendTrial = 0; bendTrial < (adaptiveBend ? 3 : 1) && best.empty(); ++bendTrial)
                {
                    const float bendScale = bendTrial == 0 ? 1.f : bendTrial == 1 ? .60f : .32f;
                    for (float sign : {-1.f, 1.f})
                    {
                        std::vector<MapFeaturePoint> curve{a};
                        bool safe = true;
                        float effort = 0.f;
                        for (int step = 1; step <= steps; ++step)
                        {
                            const float t = static_cast<float>(step) / steps;
                            const float ease = std::sin(t * 3.14159265f);
                            const float drift = sign * amplitude * bendScale * ease * ease *
                                                (.76f + .24f * std::sin(t * 6.2831853f + phase));
                            const MapFeaturePoint p = step == steps
                                                          ? b
                                                          : MapFeaturePoint{a.x + dx * t - dy / length * drift,
                                                                            a.y + dy * t + dx / length * drift};
                            safe &= mapGenV7RoadChordIsSafe(map, curve.back(), p);
                            effort += alignmentEffort(curve.back(), p);
                            curve.push_back(p);
                        }
                        if (safe && effort <= reference * (overviewV10 ? 1.14f : 1.10f) && effort < bestEffort)
                        {
                            best = std::move(curve);
                            bestEffort = effort;
                        }
                    }
                }
            }
            if (best.empty())
                natural.push_back(b);
            else
                natural.insert(natural.end(), best.begin() + 1, best.end());
        }
        alignment = std::move(natural);
    }
    std::vector<MapFeaturePoint> rounded{alignment.front()};
    for (std::size_t index = 1; index + 1 < alignment.size(); ++index)
    {
        const auto a = alignment[index - 1], b = alignment[index], c = alignment[index + 1];
        const float incoming = std::hypot(b.x - a.x, b.y - a.y);
        const float outgoing = std::hypot(c.x - b.x, c.y - b.y);
        const float radius = std::min({2.8f, incoming * 0.35f, outgoing * 0.35f});
        if (radius < 0.12f)
        {
            rounded.push_back(b);
            continue;
        }
        const MapFeaturePoint entry{b.x + (a.x - b.x) * radius / incoming, b.y + (a.y - b.y) * radius / incoming};
        const MapFeaturePoint exit{b.x + (c.x - b.x) * radius / outgoing, b.y + (c.y - b.y) * radius / outgoing};
        std::vector<MapFeaturePoint> bend{entry};
        bool safe = mapGenV7RoadChordIsSafe(map, rounded.back(), entry);
        float effort = 0.0f;
        for (int step = 1; step <= 10; ++step)
        {
            const float t = step / 10.0f, u = 1.0f - t;
            const MapFeaturePoint point{u * u * entry.x + 2.0f * u * t * b.x + t * t * exit.x,
                                        u * u * entry.y + 2.0f * u * t * b.y + t * t * exit.y};
            safe &= mapGenV7RoadChordIsSafe(map, bend.back(), point);
            effort += cornerEffort(bend.back(), point);
            bend.push_back(point);
        }
        safe &= mapGenV7RoadChordIsSafe(map, exit, c);
        const float reference = cornerEffort(entry, b) + cornerEffort(b, exit);
        if (safe && effort <= reference * 1.04f)
            rounded.insert(rounded.end(), bend.begin(), bend.end());
        else
            rounded.push_back(b);
    }
    rounded.push_back(alignment.back());
    feature.points.clear();
    feature.points.push_back(rounded.front());
    for (std::size_t index = 1; index < rounded.size(); ++index)
    {
        const auto a = rounded[index - 1], b = rounded[index];
        const int count = std::max(1, static_cast<int>(std::ceil(std::hypot(b.x - a.x, b.y - a.y) / 1.25f)));
        for (int step = 1; step <= count; ++step)
        {
            const float t = static_cast<float>(step) / count;
            feature.points.push_back({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
        }
    }
    if (map.generation.recipe.generatorVersion >= 10)
    {
        // The safety-constrained fit can retain a short angular join between
        // two valid reaches. Relax only interior controls, preserving all
        // anchors, dry shoulders and the pre-existing water crossings.
        for (int pass = 0; pass < 3; ++pass)
        {
            auto softened = feature.points;
            for (std::size_t i = 1; i + 1 < feature.points.size(); ++i)
            {
                const auto a = feature.points[i - 1], b = feature.points[i], c = feature.points[i + 1];
                const MapFeaturePoint p{a.x * .23f + b.x * .54f + c.x * .23f, a.y * .23f + b.y * .54f + c.y * .23f};
                if (mapGenV7RoadChordIsSafe(map, a, p) && mapGenV7RoadChordIsSafe(map, p, c) &&
                    cornerEffort(a, p) + cornerEffort(p, c) <= (cornerEffort(a, b) + cornerEffort(b, c)) * 1.06f)
                    softened[i] = p;
            }
            feature.points.swap(softened);
        }
    }
}

