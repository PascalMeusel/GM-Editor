#pragma once

#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

namespace gm::map_routing
{
inline constexpr std::array<MapCellCoordinate, 8> mountainPassDirections{{
    {0, -1}, {1, 0}, {0, 1}, {-1, 0}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}}};
inline constexpr int maximumMountainPassSpan = 4;

struct MountainPassCandidate
{
    MapCellCoordinate entry;
    MapCellCoordinate exit;
    MapCellCoordinate direction;
    int mountainCells = 0;
    int peakElevation = 0;
};

struct MountainPassRoutingField
{
    std::vector<int> distance;
    std::vector<std::uint16_t> crossingDirections;
    std::vector<std::uint16_t> approachDirections;
    std::vector<int> crossingCost;
    std::vector<MountainPassCandidate> candidates;
};

inline bool usesBoundedMountainPassRouting(const MapModel &map)
{
    return map.generation.recipe.generatorVersion >= 6 &&
        (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region);
}

inline bool mountainPassDryApproach(const MapCellModel *cell)
{
    return cell && cell->kind != MapTileKind::Mountain && cell->kind != MapTileKind::Water &&
        cell->kind != MapTileKind::Bridge && cell->kind != MapTileKind::Building &&
        cell->kind != MapTileKind::Wall && cell->kind != MapTileKind::Lava &&
        cell->kind != MapTileKind::Pit && cell->kind != MapTileKind::Chasm;
}

inline MountainPassRoutingField buildMountainPassRoutingField(const MapModel &map)
{
    MountainPassRoutingField field;
    const int area = map.width * map.height;
    if (area <= 0)
        return field;
    field.distance.assign(static_cast<std::size_t>(area), 5);
    field.crossingDirections.assign(static_cast<std::size_t>(area), 0);
    field.approachDirections.assign(static_cast<std::size_t>(area), 0);
    field.crossingCost.assign(static_cast<std::size_t>(area), 100000);
    std::queue<int> pending;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (const auto *cell = mapCellAt(map, x, y); cell && cell->kind == MapTileKind::Mountain)
            {
                const int index = mapCellIndex(map, x, y);
                field.distance[index] = 0;
                pending.push(index);
            }
    while (!pending.empty())
    {
        const int current = pending.front();
        pending.pop();
        if (field.distance[current] >= 4)
            continue;
        for (const auto &direction : mountainPassDirections)
        {
            const int nextX = current % map.width + direction.x;
            const int nextY = current / map.width + direction.y;
            // mapCellIndex is raw row-major arithmetic: a linear bounds check
            // alone also permits wrapping a horizontal step into another row.
            if (!isMapCellInside(map, nextX, nextY))
                continue;
            const int next = mapCellIndex(map, nextX, nextY);
            if (field.distance[next] <= field.distance[current] + 1)
                continue;
            field.distance[next] = field.distance[current] + 1;
            pending.push(next);
        }
    }
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            if (!mountainPassDryApproach(mapCellAt(map, x, y)))
                continue;
            for (std::size_t directionIndex = 0; directionIndex < mountainPassDirections.size(); ++directionIndex)
            {
                const auto direction = mountainPassDirections[directionIndex];
                const auto *first = mapCellAt(map, x + direction.x, y + direction.y);
                if (!first || first->kind != MapTileKind::Mountain ||
                    !mountainPassDryApproach(mapCellAt(map, x - direction.x, y - direction.y)))
                    continue;
                int count = 0, peak = 0;
                while (count <= maximumMountainPassSpan)
                {
                    const auto *cell = mapCellAt(map, x + (count + 1) * direction.x, y + (count + 1) * direction.y);
                    if (!cell || cell->kind != MapTileKind::Mountain)
                        break;
                    ++count;
                    peak = std::max(peak, cell->elevation);
                }
                const MapCellCoordinate exit{x + (count + 1) * direction.x, y + (count + 1) * direction.y};
                if (count == 0 || count > maximumMountainPassSpan ||
                    !mountainPassDryApproach(mapCellAt(map, exit.x, exit.y)) ||
                    !mountainPassDryApproach(mapCellAt(map, exit.x + direction.x, exit.y + direction.y)))
                    continue;

                // Estimate the local ridge axis from its actual terrain mass.
                // A pass crosses that axis; following a long ridge is not a
                // pass merely because a short subsegment is cheap to carve.
                const float centerX = (static_cast<float>(x + exit.x)) * 0.5f;
                const float centerY = (static_cast<float>(y + exit.y)) * 0.5f;
                float xx = 0, yy = 0, xy = 0;
                int neighbors = 0;
                for (int my = static_cast<int>(centerY) - 4; my <= static_cast<int>(centerY) + 4; ++my)
                    for (int mx = static_cast<int>(centerX) - 4; mx <= static_cast<int>(centerX) + 4; ++mx)
                        if (const auto *cell = mapCellAt(map, mx, my); cell && cell->kind == MapTileKind::Mountain)
                        {
                            const float dx = mx - centerX, dy = my - centerY;
                            xx += dx * dx; yy += dy * dy; xy += dx * dy;
                            ++neighbors;
                        }
                const float anisotropy = std::sqrt((xx - yy) * (xx - yy) + 4.0f * xy * xy) /
                    std::max(0.001f, xx + yy);
                const float axis = 0.5f * std::atan2(2.0f * xy, xx - yy);
                const float dot = std::abs((direction.x * std::cos(axis) + direction.y * std::sin(axis)) /
                    std::sqrt(static_cast<float>(direction.x * direction.x + direction.y * direction.y)));
                if ((neighbors >= 3 && anisotropy > 0.25f && dot > 0.76f) ||
                    (anisotropy <= 0.25f && count > 2))
                    continue;
                field.candidates.push_back({{x, y}, exit, direction, count, peak});
                const std::uint16_t mask = static_cast<std::uint16_t>(1u << directionIndex);
                field.approachDirections[mapCellIndex(map, x, y)] |= mask;
                field.approachDirections[mapCellIndex(map, exit.x, exit.y)] |= mask;
                for (int step = 1; step <= count; ++step)
                {
                    const int index = mapCellIndex(map, x + step * direction.x, y + step * direction.y);
                    field.crossingDirections[index] |= mask;
                    field.crossingCost[index] = std::min(field.crossingCost[index], 26 + count * 5 + std::max(0, peak) * 3);
                }
            }
        }
    // A low natural gap is preferable to carving a new pass. Permit a bounded
    // dry corridor flanked by mountain mass on both sides, including its two
    // approaches. Ordinary parallel roads outside a ridge gain no exemption.
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            const int entry = mapCellIndex(map, x, y);
            if (field.distance[entry] < 2 || !mountainPassDryApproach(mapCellAt(map, x, y)))
                continue;
            for (std::size_t slot = 0; slot < mountainPassDirections.size(); ++slot)
            {
                const auto direction = mountainPassDirections[slot];
                std::vector<int> passage;
                bool flanked = false;
                int step = 1;
                for (; step <= 9; ++step)
                {
                    const int cx = x + direction.x * step, cy = y + direction.y * step;
                    const int at = mapCellIndex(map, cx, cy);
                    if (at < 0 || !mountainPassDryApproach(mapCellAt(map, cx, cy)))
                        break;
                    if (field.distance[at] >= 2)
                    {
                        if (!passage.empty() && flanked)
                        {
                            const auto mask = static_cast<std::uint16_t>(1u << slot);
                            field.approachDirections[entry] |= mask;
                            field.approachDirections[at] |= mask;
                            for (const int index : passage)
                                field.approachDirections[index] |= mask;
                            field.candidates.push_back({{x, y}, {cx, cy}, direction, 0, 0});
                        }
                        break;
                    }
                    if (step > 8)
                        break;
                    passage.push_back(at);
                    const auto *left = mapCellAt(map, cx - direction.y, cy + direction.x);
                    const auto *right = mapCellAt(map, cx + direction.y, cy - direction.x);
                    flanked = flanked || (left && right && left->kind == MapTileKind::Mountain && right->kind == MapTileKind::Mountain);
                }
            }
        }
    return field;
}

inline bool mountainPassPathIsBounded(const MapModel &map,
                                      const std::vector<MapCellCoordinate> &path,
                                      const MountainPassRoutingField &field)
{
    for (std::size_t index = 0; index < path.size(); ++index)
    {
        const auto *cell = mapCellAt(map, path[index].x, path[index].y);
        if (!cell)
            return false;
        if (cell->kind != MapTileKind::Mountain)
            continue;
        if (index == 0)
            return false;
        const MapCellCoordinate direction{path[index].x - path[index - 1].x, path[index].y - path[index - 1].y};
        std::uint16_t mask = 0;
        for (std::size_t slot = 0; slot < mountainPassDirections.size(); ++slot)
            if (mountainPassDirections[slot] == direction)
                mask = static_cast<std::uint16_t>(1u << slot);
        int span = 0;
        do
        {
            const int at = mapCellIndex(map, path[index].x, path[index].y);
            if (at < 0 || mask == 0 || (field.crossingDirections[at] & mask) == 0 ||
                path[index].x - path[index - 1].x != direction.x ||
                path[index].y - path[index - 1].y != direction.y || ++span > maximumMountainPassSpan)
                return false;
            ++index;
            if (index >= path.size())
                return false;
            cell = mapCellAt(map, path[index].x, path[index].y);
        } while (cell && cell->kind == MapTileKind::Mountain);
        if (!cell || path[index].x - path[index - 1].x != direction.x ||
            path[index].y - path[index - 1].y != direction.y)
            return false;
    }
    return true;
}
}
