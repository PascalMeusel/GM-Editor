#pragma once
#include "Models/Map/MapAuthoring.hpp"

// Tracks the complete stroke for a cheap editor overlay. This contains no
// authoring data: the map still changes immediately and remains the source of
// truth for undo, persistence and player visibility.
struct MapBrushStrokePreview
{
    int width = 0, height = 0;
    std::vector<unsigned char> touched;

    void begin(const MapModel &map)
    {
        width = map.width; height = map.height;
        touched.assign(map.cells.size(), 0);
    }
    void clear() { touched.clear(); width = height = 0; }
    void record(const std::vector<MapCellCoordinate> &footprint)
    {
        for (const auto &p : footprint)
            if (p.x >= 0 && p.y >= 0 && p.x < width && p.y < height)
            {
                const auto index = static_cast<std::size_t>(p.y) * width + p.x;
                if (index < touched.size()) touched[index] = 1;
            }
    }
    template<class Fill> void visitSpans(int minX, int minY, int maxX, int maxY, Fill fill) const
    {
        minX = std::max(0, minX); minY = std::max(0, minY);
        maxX = std::min(width-1, maxX); maxY = std::min(height-1, maxY);
        for (int y=minY; y<=maxY; ++y)
            for (int x=minX; x<=maxX;)
            {
                const auto marked = [&](int column) {
                    const auto index = static_cast<std::size_t>(y)*width+column;
                    return index < touched.size() && touched[index];
                };
                if (!marked(x)) { ++x; continue; }
                const int start=x;
                while(x<=maxX && marked(x)) ++x;
                fill(start,y,x,y+1);
            }
    }
};
