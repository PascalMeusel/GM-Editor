#pragma once

#include "Models/Map/MapAuthoring.hpp"

namespace gm::mapui
{
// Exact cell coverage, batched into horizontal spans. No softened edge may
// imply that a partly hidden cell is visible to players. Editor presentation
// is deliberately separate from the opaque player fog renderer.
template<class Fill, class Edge>
void visitEditorFog(const MapModel &map, int minX, int minY, int maxX, int maxY,
                    Fill fill, Edge edge, bool outline)
{
    minX = std::max(0, minX); minY = std::max(0, minY);
    maxX = std::min(map.width - 1, maxX); maxY = std::min(map.height - 1, maxY);
    const auto hidden = [&](int x, int y) {
        const auto *cell = mapCellAt(map, x, y);
        return cell && cell->fogged;
    };
    for (int y = minY; y <= maxY; ++y)
    {
        int x = minX;
        while (x <= maxX)
        {
            if (!hidden(x, y)) { ++x; continue; }
            const int start = x;
            while (x <= maxX && hidden(x, y)) ++x;
            fill(start, y, x, y + 1);
        }
    }
    if (!outline) return;
    // Coalesce boundary segments too; never draw seams between hidden cells.
    for (int y = minY; y <= maxY + 1; ++y)
        for (int x = minX; x <= maxX;)
        {
            if (hidden(x, y-1) == hidden(x, y)) { ++x; continue; }
            const int start = x;
            while (x <= maxX && hidden(x, y-1) != hidden(x, y)) ++x;
            edge(start, y, x, y);
        }
    for (int x = minX; x <= maxX + 1; ++x)
        for (int y = minY; y <= maxY;)
        {
            if (hidden(x-1, y) == hidden(x, y)) { ++y; continue; }
            const int start = y;
            while (y <= maxY && hidden(x-1, y) != hidden(x, y)) ++y;
            edge(x, start, x, y);
        }
}
}
