#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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


void mapGenBuildCaveNetwork(MapGenerationResult &result, const MapGenerationOptions &options, std::mt19937 &rng,
                            int totalSteps)
{
    result.map.name = "Generated Cave";
    result.map.description = "Editable cave network generated from sliders and a stable seed.";
    mapGenClear(result.map, MapTileKind::Wall);

    const int area = std::max(1, result.map.width * result.map.height);
    // Chambers are chosen with farthest-point sampling before anything is
    // painted.  The former sequence of unconstrained random discs frequently
    // collapsed into one giant blob; connecting those insertion-order centres
    // then produced a loop-shaped corridor with no readable chamber hierarchy.
    const bool natural = options.generatorVersion >= 11;
    const int cavernCount =
        natural ? std::clamp(std::max(2 + area / 300, std::max(result.map.width, result.map.height) >
                                                              2 * std::min(result.map.width, result.map.height)
                                                          ? std::max(result.map.width, result.map.height) / 8
                                                          : 4),
                             4, 14)
                : std::clamp(1 + area / 210, 4, 8);
    std::vector<MapCellCoordinate> cavernCenters;
    std::vector<int> cavernRadii;
    cavernCenters.reserve(static_cast<std::size_t>(cavernCount));
    cavernRadii.reserve(static_cast<std::size_t>(cavernCount));
    const int maximumCavernRadius = std::max(2, natural ? std::min({6, result.map.width / 6, result.map.height / 6})
                                                        : std::min({5, result.map.width / 7, result.map.height / 6}));
    const int cavernMargin = maximumCavernRadius + 1;
    for (int i = 0; i < cavernCount; ++i)
    {
        MapCellCoordinate best{
            std::clamp(result.map.width / 2, cavernMargin, std::max(cavernMargin, result.map.width - cavernMargin - 1)),
            std::clamp(result.map.height / 2, cavernMargin,
                       std::max(cavernMargin, result.map.height - cavernMargin - 1))};
        if (natural && cavernCenters.empty())
        {
            best.x = std::clamp(best.x + mapGenRandomBetween(rng, -std::max(1, result.map.width / 9),
                                                             std::max(1, result.map.width / 9)),
                                cavernMargin, std::max(cavernMargin, result.map.width - cavernMargin - 1));
            best.y = std::clamp(best.y + mapGenRandomBetween(rng, -std::max(1, result.map.height / 9),
                                                             std::max(1, result.map.height / 9)),
                                cavernMargin, std::max(cavernMargin, result.map.height - cavernMargin - 1));
        }
        int bestSeparation = -1;
        const int sampleCount = cavernCenters.empty() ? 1 : 64;
        for (int sample = 0; sample < sampleCount; ++sample)
        {
            const MapCellCoordinate candidate =
                cavernCenters.empty()
                    ? best
                    : MapCellCoordinate{
                          mapGenRandomBetween(rng, cavernMargin,
                                              std::max(cavernMargin, result.map.width - cavernMargin - 1)),
                          mapGenRandomBetween(rng, cavernMargin,
                                              std::max(cavernMargin, result.map.height - cavernMargin - 1))};
            int minimumDistanceSquared = std::numeric_limits<int>::max();
            for (const MapCellCoordinate &existing : cavernCenters)
            {
                const int dx = candidate.x - existing.x;
                const int dy = candidate.y - existing.y;
                minimumDistanceSquared = std::min(minimumDistanceSquared, dx * dx + dy * dy);
            }
            if (minimumDistanceSquared > bestSeparation)
            {
                best = candidate;
                bestSeparation = minimumDistanceSquared;
            }
        }
        const int radius = i == 0 ? maximumCavernRadius
                                  : mapGenRandomBetween(rng, std::min(3, maximumCavernRadius), maximumCavernRadius);
        cavernCenters.push_back(best);
        cavernRadii.push_back(radius);
    }

    for (int i = 0; i < static_cast<int>(cavernCenters.size()); ++i)
    {
        const MapCellCoordinate center = cavernCenters[static_cast<std::size_t>(i)];
        const int radius = cavernRadii[static_cast<std::size_t>(i)];
        if (natural)
        {
            const float phase = mapGenRandomBetween(rng, 0, 628) * .01f;
            const float stretch = mapGenRandomBetween(rng, 80, 125) * .01f;
            for (int dy = -radius - 2; dy <= radius + 2; ++dy)
                for (int dx = -radius - 2; dx <= radius + 2; ++dx)
                {
                    const float angle = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
                    const float edge =
                        radius * (1.f + .13f * std::sin(angle * 3.f + phase) + .09f * std::cos(angle * 5.f - phase));
                    if (std::sqrt(dx * dx / stretch + dy * dy * stretch) <= edge && center.x + dx > 0 &&
                        center.y + dy > 0 && center.x + dx < result.map.width - 1 &&
                        center.y + dy < result.map.height - 1)
                        mapGenSetCell(result.map, center.x + dx, center.y + dy, MapTileKind::Cave);
                }
        }
        else
            mapGenPaintDisc(result.map, center.x, center.y, radius, MapTileKind::Cave, rng, 14,
                            MapSymbolVariantMode::Continuous);

        const float centerX = static_cast<float>(center.x) + 0.5f;
        const float centerY = static_cast<float>(center.y) + 0.5f;
        const float extent = static_cast<float>(radius) + 0.5f;
        const float diagonal = extent * 0.71f;
        const auto clampX = [&](float value) { return std::clamp(value, 0.0f, static_cast<float>(result.map.width)); };
        const auto clampY = [&](float value) { return std::clamp(value, 0.0f, static_cast<float>(result.map.height)); };
        mapGenAddInteriorGraphFeature(result.map, options.seed, "cave_chamber", static_cast<std::size_t>(i), "Cave",
                                      "fantasy_neutral.cave.chamber", extent * 2.0f, true,
                                      {{clampX(centerX), clampY(centerY - extent)},
                                       {clampX(centerX + diagonal), clampY(centerY - diagonal)},
                                       {clampX(centerX + extent), clampY(centerY)},
                                       {clampX(centerX + diagonal), clampY(centerY + diagonal)},
                                       {clampX(centerX), clampY(centerY + extent)},
                                       {clampX(centerX - diagonal), clampY(centerY + diagonal)},
                                       {clampX(centerX - extent), clampY(centerY)},
                                       {clampX(centerX - diagonal), clampY(centerY - diagonal)}});
    }

    mapGenReportProgress(options, 3, totalSteps, "Connecting cave tunnels");
    std::vector<std::pair<std::size_t, std::size_t>> tunnelEdges;
    if (!cavernCenters.empty())
    {
        // A minimum spanning tree keeps every chamber connected with short,
        // legible passages and exactly N-1 tunnel features.  It also preserves
        // the long-standing serialized graph contract used by migrations and
        // deterministic tests.
        std::vector<unsigned char> connected(cavernCenters.size(), 0);
        connected[0] = 1;
        while (tunnelEdges.size() + 1 < cavernCenters.size())
        {
            int bestDistance = std::numeric_limits<int>::max();
            std::pair<std::size_t, std::size_t> best{0, 0};
            for (std::size_t from = 0; from < cavernCenters.size(); ++from)
            {
                if (!connected[from])
                    continue;
                for (std::size_t to = 0; to < cavernCenters.size(); ++to)
                {
                    if (connected[to])
                        continue;
                    const int dx = cavernCenters[from].x - cavernCenters[to].x;
                    const int dy = cavernCenters[from].y - cavernCenters[to].y;
                    const int distance = dx * dx + dy * dy;
                    if (distance < bestDistance || (distance == bestDistance && std::pair{from, to} < best))
                    {
                        bestDistance = distance;
                        best = {from, to};
                    }
                }
            }
            connected[best.second] = 1;
            tunnelEdges.push_back(best);
        }
    }
    if (natural && cavernCenters.size() > 4 && options.routeDensity >= 20)
    {
        // A second approach is a deliberate navigational choice, not a
        // decorative branch. Keep the historical spanning-tree graph intact.
        int loops = std::clamp(options.routeDensity / 25, 1, 3);
        while (loops-- > 0)
        {
            int bestDistance = std::numeric_limits<int>::max();
            std::pair<std::size_t, std::size_t> best{0, 0};
            for (std::size_t a = 0; a < cavernCenters.size(); ++a)
                for (std::size_t b = a + 1; b < cavernCenters.size(); ++b)
                {
                    bool exists = false;
                    for (auto e : tunnelEdges)
                        if ((e.first == a && e.second == b) || (e.first == b && e.second == a))
                        {
                            exists = true;
                            break;
                        }
                    if (exists)
                        continue;
                    const int dx = cavernCenters[a].x - cavernCenters[b].x,
                              dy = cavernCenters[a].y - cavernCenters[b].y;
                    if (dx * dx + dy * dy < bestDistance)
                    {
                        bestDistance = dx * dx + dy * dy;
                        best = {a, b};
                    }
                }
            if (best.first != best.second)
                tunnelEdges.push_back(best);
        }
    }
    for (std::size_t i = 0; i < tunnelEdges.size(); ++i)
    {
        const MapCellCoordinate from = cavernCenters[tunnelEdges[i].first];
        const MapCellCoordinate to = cavernCenters[tunnelEdges[i].second];
        std::vector<MapFeaturePoint> tunnelPoints;
        if (natural)
        {
            const float dx = static_cast<float>(to.x - from.x), dy = static_cast<float>(to.y - from.y);
            const float length = std::sqrt(dx * dx + dy * dy);
            float bend = mapGenRandomBetween(rng, -22, 22) * .01f * std::min(length, 14.f);
            if (length > 9.f)
                bend = (bend < 0.f ? -1.f : 1.f) * std::max(std::abs(bend), std::min(2.4f, length * .10f));
            const int samples = std::max(2, static_cast<int>(length * 3));
            MapCellCoordinate previous = from;
            for (int step = 0; step <= samples; ++step)
            {
                const float t = static_cast<float>(step) / samples;
                const float wave = std::sin(t * 3.14159265f) * bend;
                const int x =
                    std::clamp(static_cast<int>(std::lround(from.x + t * dx - wave * dy / std::max(1.f, length))), 1,
                               result.map.width - 2);
                const int y =
                    std::clamp(static_cast<int>(std::lround(from.y + t * dy + wave * dx / std::max(1.f, length))), 1,
                               result.map.height - 2);
                for (int oy = -1; oy <= 1; ++oy)
                    for (int ox = -1; ox <= 1; ++ox)
                        if (x + ox > 0 && y + oy > 0 && x + ox < result.map.width - 1 && y + oy < result.map.height - 1)
                            mapGenSetCell(result.map, x + ox, y + oy, MapTileKind::Cave);
                mapGenSetWay(result.map, x, y, MapWayKind::CaveTunnel);
                if (previous.x != x && previous.y != y)
                    mapGenSetWay(result.map, previous.x, y, MapWayKind::CaveTunnel);
                if (tunnelPoints.empty() || previous.x != x || previous.y != y)
                    tunnelPoints.push_back({x + .5f, y + .5f});
                previous = {x, y};
            }
        }
        else
        {
            mapGenDrawWay(result.map, from.x, from.y, to.x, to.y, MapWayKind::CaveTunnel, rng, 14, true,
                          MapTileKind::Cave, 1);
            tunnelPoints = {{from.x + .5f, from.y + .5f}, {to.x + .5f, to.y + .5f}};
        }
        mapGenAddInteriorGraphFeature(result.map, options.seed, "cave_tunnel", i, "Cave Tunnel",
                                      "fantasy_neutral.cave.tunnel", 3.0f, false, std::move(tunnelPoints));
    }

    std::pair<std::size_t, std::size_t> caveEntrances{0, 0};
    int entranceDistance = -1;
    for (std::size_t first = 0; first < cavernCenters.size(); ++first)
        for (std::size_t second = first + 1; second < cavernCenters.size(); ++second)
        {
            const int dx = cavernCenters[first].x - cavernCenters[second].x;
            const int dy = cavernCenters[first].y - cavernCenters[second].y;
            const int distance = dx * dx + dy * dy;
            if (distance > entranceDistance)
            {
                entranceDistance = distance;
                caveEntrances = {first, second};
            }
        }
    if (!cavernCenters.empty())
    {
        if (!natural && cavernCenters.size() > 2)
        {
            std::size_t poolIndex = cavernCenters.size() / 2;
            if (poolIndex == caveEntrances.first || poolIndex == caveEntrances.second)
                poolIndex = (poolIndex + 1) % cavernCenters.size();
            if (poolIndex == caveEntrances.first || poolIndex == caveEntrances.second)
                poolIndex = (poolIndex + 1) % cavernCenters.size();
            const std::array<MapCellCoordinate, 4> poolOffsets{{{std::max(1, cavernRadii[poolIndex] / 2), 0},
                                                                {-std::max(1, cavernRadii[poolIndex] / 2), 0},
                                                                {0, std::max(1, cavernRadii[poolIndex] / 2)},
                                                                {0, -std::max(1, cavernRadii[poolIndex] / 2)}}};
            for (const MapCellCoordinate &offset : poolOffsets)
            {
                const int poolX = cavernCenters[poolIndex].x + offset.x;
                const int poolY = cavernCenters[poolIndex].y + offset.y;
                const MapCellModel *poolCell = mapCellAt(result.map, poolX, poolY);
                if (!poolCell || poolCell->kind != MapTileKind::Cave || poolCell->wayKind != MapWayKind::None)
                    continue;
                mapGenSetCell(result.map, poolX, poolY, MapTileKind::Water);
                break;
            }
        }
        mapGenSetCell(result.map, cavernCenters[caveEntrances.first].x, cavernCenters[caveEntrances.first].y,
                      MapTileKind::Stairs);
        mapGenSetCell(result.map, cavernCenters[caveEntrances.second].x, cavernCenters[caveEntrances.second].y,
                      MapTileKind::Stairs);
    }

    if (natural)
    {
        // Close accidental one-cell rock pinholes between adjoining passages.
        std::vector<MapCellCoordinate> pinholes;
        for (int y = 1; y < result.map.height - 1; ++y)
            for (int x = 1; x < result.map.width - 1; ++x)
            {
                const auto *c = mapCellAt(result.map, x, y);
                if (c->kind != MapTileKind::Wall)
                    continue;
                int floor = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dx || dy)
                        {
                            const auto *n = mapCellAt(result.map, x + dx, y + dy);
                            floor += n && n->kind != MapTileKind::Wall;
                        }
                if (floor >= 7)
                    pinholes.push_back({x, y});
            }
        for (auto c : pinholes)
            mapGenSetCell(result.map, c.x, c.y, MapTileKind::Cave);
        // Water is a usable pocket with a dry approach, not a single blue tile.
        // Never replace tunnel traversal or either stair approach.
        const int pools = options.waterLevel <= 0 ? 0 : std::clamp(1 + options.waterLevel / 25, 1, 4);
        for (int pool = 0; pool < pools; ++pool)
            for (int attempt = 0; attempt < 80; ++attempt)
            {
                const int x = mapGenRandomBetween(rng, 2, result.map.width - 3),
                          y = mapGenRandomBetween(rng, 2, result.map.height - 3);
                const auto *c = mapCellAt(result.map, x, y);
                if (!c || c->kind != MapTileKind::Cave || c->wayKind != MapWayKind::None)
                    continue;
                bool interior = true;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const auto *n = mapCellAt(result.map, x + dx, y + dy);
                        if (!n || n->kind == MapTileKind::Stairs || n->wayKind != MapWayKind::None)
                            interior = false;
                    }
                if (!interior)
                    continue;
                std::vector<MapCellCoordinate> pocket{{x, y}}, frontier{{x, y}};
                const int target = std::clamp(3 + options.waterLevel / 5, 3, 20);
                const auto eligible = [&](MapCellCoordinate q) {
                    const auto *cell = mapCellAt(result.map, q.x, q.y);
                    if (!cell || cell->kind != MapTileKind::Cave || cell->wayKind != MapWayKind::None)
                        return false;
                    for (int oy = -1; oy <= 1; ++oy)
                        for (int ox = -1; ox <= 1; ++ox)
                        {
                            const auto *n = mapCellAt(result.map, q.x + ox, q.y + oy);
                            if (!n || n->kind == MapTileKind::Stairs || n->wayKind == MapWayKind::CaveTunnel)
                                return false;
                        }
                    return true;
                };
                while (!frontier.empty() && static_cast<int>(pocket.size()) < target)
                {
                    const int pick = mapGenRandomBetween(rng, 0, static_cast<int>(frontier.size()) - 1);
                    const auto from = frontier[pick];
                    frontier.erase(frontier.begin() + pick);
                    std::array<MapCellCoordinate, 4> adjacent{
                        {{from.x + 1, from.y}, {from.x, from.y + 1}, {from.x - 1, from.y}, {from.x, from.y - 1}}};
                    const int offset = mapGenRandomBetween(rng, 0, 3);
                    for (int side = 0; side < 4 && static_cast<int>(pocket.size()) < target; ++side)
                    {
                        const auto q = adjacent[(side + offset) % 4];
                        const int dx = q.x - x, dy = q.y - y;
                        if (dx * dx + dy * dy > 13 || !eligible(q) ||
                            std::any_of(pocket.begin(), pocket.end(), [&](auto p) { return p.x == q.x && p.y == q.y; }))
                            continue;
                        pocket.push_back(q);
                        frontier.push_back(q);
                    }
                }
                if (pocket.size() >= 3)
                    for (auto q : pocket)
                        mapGenSetCell(result.map, q.x, q.y, MapTileKind::Water);
                break;
            }
    }
    mapComposeCavePassagesV12(result.map, options.generatorVersion, options.seed);
    mapComposeCaveBasinsV12(result.map, options.waterLevel, options.generatorVersion, options.seed);
    mapGenClearIsolatedWays(result.map, MapWayKind::CaveTunnel);
    mapGenReportProgress(options, 4, totalSteps, "Placing tactical features");
    mapGenBuildTacticalFeatures(result.map, options, rng);

    MapRouteModel route;
    mapComposeCaveGeologyV12(result.map, options.generatorVersion, options.seed);
    route.name = "Generated Cave Route";
    route.fromMapId = result.map.metadata.id;
    route.startX = cavernCenters.empty() ? 0 : cavernCenters[caveEntrances.first].x;
    route.startY = cavernCenters.empty() ? 0 : cavernCenters[caveEntrances.first].y;
    route.endX = cavernCenters.empty() ? result.map.width - 1 : cavernCenters[caveEntrances.second].x;
    route.endY = cavernCenters.empty() ? result.map.height - 1 : cavernCenters[caveEntrances.second].y;
    route.distanceMiles = static_cast<float>(std::max(1, result.map.width / 8));
    route.terrain = mapWayKindLabel(MapWayKind::CaveTunnel);
    route.notes = "Representative cave route generated from the semantic tunnel network.";
    result.map.routes.push_back(route);
}
