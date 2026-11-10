#pragma once
#include "Models/Map/MapModels.hpp"
#include <algorithm>

// Line widths are independent of the area brush and of one another. Painting
// a lake or widening a river must not silently widen the next road.
struct MapStrokeWidths
{
    int road = 1;
    int water = 1;
    static bool isWater(MapWayKind kind)
    { return kind == MapWayKind::River || kind == MapWayKind::Canal; }
    static int maximum(MapWayKind kind) { return isWater(kind) ? 8 : 3; }
    int &setting(MapWayKind kind) { return isWater(kind) ? water : road; }
    int value(MapWayKind kind) const
    { return std::clamp(isWater(kind) ? water : road, 1, maximum(kind)); }
};
