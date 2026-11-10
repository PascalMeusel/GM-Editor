#pragma once

#include "Models/Map/MapAtlasTacticalWaterInk.hpp"

namespace gm::map_render
{
inline std::vector<AtlasInkPrimitive> buildIllustratedAtlasWaterInk(const MapModel &map)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesIllustratedAtlasDetailed(map) || !map.generation.continuousRendering ||
        map.width <= 0 || map.height <= 0)
        return ink;

    const bool fantasy = (usesIllustratedAtlasV4(map) || usesIllustratedAtlasV5(map));
    const auto ground = buildOverviewGroundProjection(map);
    const detail::OverviewGroundSamplingCache sampling(map, ground);
    const std::size_t count = static_cast<std::size_t>(map.width) * map.height;
    std::vector<std::uint8_t> land(count, 0), ocean(count, 0);
    const auto index = [&](int x, int y) { return static_cast<std::size_t>(y) * map.width + x; };
    const auto water = [&](int x, int y) {
        return isMapCellInside(map, x, y) &&
               overviewGroundKindFromProjection(map, ground, x, y) == MapTileKind::Water;
    };
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            land[index(x, y)] = overviewGroundKindIsLand(
                overviewGroundKindFromProjection(map, ground, x, y)) ? 1 : 0;

    // A lake or rasterized river must never acquire open-ocean waves. Only
    // water connected to a real map edge is eligible, and every wave footprint
    // additionally requires a surrounding patch of that confirmed water.
    std::queue<std::pair<int, int>> frontier;
    const auto enqueue = [&](int x, int y) {
        if (water(x, y) && !ocean[index(x, y)])
        {
            ocean[index(x, y)] = 1;
            frontier.emplace(x, y);
        }
    };
    for (int x = 0; x < map.width; ++x) { enqueue(x, 0); enqueue(x, map.height - 1); }
    for (int y = 0; y < map.height; ++y) { enqueue(0, y); enqueue(map.width - 1, y); }
    while (!frontier.empty())
    {
        const auto [x, y] = frontier.front(); frontier.pop();
        for (const auto d : std::array<std::pair<int, int>, 4>{{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}})
            enqueue(x + d.first, y + d.second);
    }
    const auto inWater = [&](MapFeaturePoint p) {
        return p.x >= 0 && p.y >= 0 && p.x < map.width && p.y < map.height &&
               detail::overviewLandCoverageFromMaskCached(map, land, p.x, p.y, true, &sampling) < .48f;
    };

    // Join marching-square segments before drawing. A continuous path avoids
    // a dotted alpha seam where hundreds of tiny independent strokes meet.
    struct Node { MapFeaturePoint p; float nx = 0, ny = 0; std::vector<std::size_t> edges; };
    struct Edge { std::size_t a = 0, b = 0; bool used = false; };
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::unordered_map<std::uint64_t, std::size_t> nodeIds;
    const auto node = [&](float x, float y, float nx, float ny) {
        const auto qx = static_cast<std::uint32_t>(std::lround(x * 4096.f));
        const auto qy = static_cast<std::uint32_t>(std::lround(y * 4096.f));
        const std::uint64_t key = (static_cast<std::uint64_t>(qx) << 32) | qy;
        const auto [found, inserted] = nodeIds.emplace(key, nodes.size());
        if (inserted) nodes.push_back({{x, y}, 0, 0, {}});
        auto &n = nodes[found->second]; n.nx += nx; n.ny += ny;
        return found->second;
    };
    for (const auto &s : detail::buildOverviewCoastSegmentsCached(map, &sampling))
    {
        const auto a = node(s.startX, s.startY, s.landNormalX, s.landNormalY);
        const auto b = node(s.endX, s.endY, s.landNormalX, s.landNormalY);
        if (a == b) continue;
        nodes[a].edges.push_back(edges.size()); nodes[b].edges.push_back(edges.size());
        edges.push_back({a, b, false});
    }
    const auto offset = [&](std::size_t id, float distance) {
        const auto &n = nodes[id];
        const float length = std::max(.0001f, std::sqrt(n.nx * n.nx + n.ny * n.ny));
        return MapFeaturePoint{n.p.x - n.nx / length * distance, n.p.y - n.ny / length * distance};
    };
    const auto midpoint = [](MapFeaturePoint a, MapFeaturePoint b) {
        return MapFeaturePoint{(a.x + b.x) * .5f, (a.y + b.y) * .5f};
    };
    std::vector<std::vector<std::size_t>> paths;
    const auto walk = [&](std::size_t start, std::size_t firstEdge) {
        std::vector<std::size_t> path{start};
        auto at = start;
        auto next = firstEdge;
        while (!edges[next].used)
        {
            auto &e = edges[next]; e.used = true;
            at = e.a == at ? e.b : e.a; path.push_back(at);
            if (at == start) break;
            const auto found = std::find_if(nodes[at].edges.begin(), nodes[at].edges.end(),
                [&](std::size_t id) { return !edges[id].used; });
            if (found == nodes[at].edges.end()) break;
            next = *found;
        }
        paths.push_back(std::move(path));
    };
    for (std::size_t n = 0; n < nodes.size(); ++n)
        if (nodes[n].edges.size() != 2)
            for (const auto edge : nodes[n].edges) if (!edges[edge].used) walk(n, edge);
    for (std::size_t e = 0; e < edges.size(); ++e)
        if (!edges[e].used) walk(edges[e].a, e);

    for (const auto &path : paths)
    {
        // Two narrow opaque washes avoid dark overlapping alpha triangles.
        // They live entirely on the water side and do not recolor the coast.
        for (int band = 0; band < 2; ++band)
        {
            const float innerOffset = band == 0 ? .06f : .28f, outerOffset = band == 0 ? .28f : .56f;
            const OverviewRenderColor color = fantasy ? (band == 0 ? OverviewRenderColor{97,158,159,255} : OverviewRenderColor{78,143,149,255}) :
                (band == 0 ? OverviewRenderColor{119,168,173,255} : OverviewRenderColor{107,159,167,255});
            for (std::size_t i = 1; i < path.size(); ++i)
            {
                const auto a = offset(path[i - 1], innerOffset), b = offset(path[i], innerOffset);
                const auto c = offset(path[i], outerOffset), d = offset(path[i - 1], outerOffset);
                if (!inWater(a) || !inWater(b) || !inWater(c) || !inWater(d) ||
                    !inWater(midpoint(a, b)) || !inWater(midpoint(c, d))) continue;
                ink.push_back({{a, b, c}, color, 0, true});
                ink.push_back({{a, c, d}, color, 0, true});
            }
        }
        for (int contour = 0; contour < 2; ++contour)
        {
            const float distance = contour == 0 ? 0.f : .88f;
            const OverviewRenderColor color = contour == 0 ? OverviewRenderColor{75, 113, 116, 255}
                                                          : OverviewRenderColor{116, 169, 175, 255};
            std::vector<MapFeaturePoint> line;
            const auto flush = [&]() {
                if (line.size() >= 2) ink.push_back({line, color, contour == 0 ? .026f : .019f, false});
                line.clear();
            };
            for (std::size_t i = 0; i < path.size(); ++i)
            {
                const auto p = offset(path[i], distance);
                if (contour != 0 && (!inWater(p) || (!line.empty() && !inWater(midpoint(line.back(), p)))))
                { flush(); continue; }
                line.push_back(p);
            }
            flush();
        }
    }

    const auto hashAt = [&](int x, int y) {
        std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^
                          static_cast<std::uint32_t>(y) * 19349663u ^ map.generation.recipe.seed ^ 0x871f5ca9u;
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; return h;
    };
    const auto openOcean = [&](MapFeaturePoint p) {
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        for (int oy = -1; oy <= 1; ++oy)
            for (int ox = -1; ox <= 1; ++ox)
                if (!isMapCellInside(map, x + ox, y + oy) || !ocean[index(x + ox, y + oy)]) return false;
        return inWater(p);
    };
    for (int gy = 1; gy * 3 < map.height; ++gy)
        for (int gx = 1; gx * 4 < map.width; ++gx)
        {
            const auto h = hashAt(gx, gy);
            if (h % 5 > 1) continue;
            const float x = gx * 4.f + (static_cast<float>((h >> 4) & 255u) / 255.f - .5f) * 2.8f;
            const float y = gy * 3.f + (static_cast<float>((h >> 12) & 255u) / 255.f - .5f) * 1.8f;
            const float span = .85f + static_cast<float>((h >> 20) & 15u) * .035f;
            const float lean = (static_cast<float>((h >> 24) & 15u) - 7.f) * .004f;
            std::vector<AtlasInkPrimitive> group;
            bool clear = true;
            for (int row = 0; row < 2 + static_cast<int>((h >> 29) & 1u); ++row)
            {
                std::vector<MapFeaturePoint> points;
                for (int sample = 0; sample <= 10; ++sample)
                {
                    const float t = sample / 10.f;
                    const MapFeaturePoint p{x + (t - .5f) * span * (1.f - row * .16f) + row * .1f,
                        y + row * .19f + std::sin(t * 6.2831853f + row * .7f) * .025f + lean * (t - .5f)};
                    clear = clear && openOcean(p); points.push_back(p);
                }
                group.push_back({std::move(points), {137, 180, 180, 165}, .018f, false});
            }
            if (clear) ink.insert(ink.end(), std::make_move_iterator(group.begin()), std::make_move_iterator(group.end()));
        }
    return ink;
}
} // namespace gm::map_render
