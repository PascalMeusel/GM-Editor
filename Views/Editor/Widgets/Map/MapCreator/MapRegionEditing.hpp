#pragma once

#include "Models/Map/MapModels.hpp"
#include <algorithm>
#include <cmath>

namespace gm::mapui
{
enum RegionHandle { RegionNone = 0, RegionLeft = 1, RegionRight = 2,
    RegionTop = 4, RegionBottom = 8, RegionMove = 16, RegionCreate = 32 };

struct RegionBounds
{
    int x = 0, y = 0, width = 1, height = 1;
    bool operator==(const RegionBounds &other) const
    { return x == other.x && y == other.y && width == other.width && height == other.height; }
    static RegionBounds from(const MapRegionModel &region)
    { return {region.x, region.y, region.width, region.height}; }
    void apply(MapRegionModel &region) const
    { region.x=x; region.y=y; region.width=width; region.height=height; }
};

inline int regionHandleAt(RegionBounds bounds, MapFeaturePoint point, float tolerance)
{
    const float right = static_cast<float>(bounds.x + bounds.width);
    const float bottom = static_cast<float>(bounds.y + bounds.height);
    if (point.x < bounds.x-tolerance || point.x > right+tolerance ||
        point.y < bounds.y-tolerance || point.y > bottom+tolerance) return RegionNone;
    // Choose the nearest edge when a small region's handle hit areas overlap.
    int handle = RegionNone;
    const float leftDistance = std::abs(point.x-bounds.x), rightDistance = std::abs(point.x-right);
    const float topDistance = std::abs(point.y-bounds.y), bottomDistance = std::abs(point.y-bottom);
    if (std::min(leftDistance,rightDistance) <= tolerance)
        handle |= leftDistance < rightDistance ? RegionLeft : RegionRight;
    if (std::min(topDistance,bottomDistance) <= tolerance)
        handle |= topDistance < bottomDistance ? RegionTop : RegionBottom;
    if (handle) return handle;
    return point.x >= bounds.x && point.x <= right && point.y >= bounds.y && point.y <= bottom
        ? RegionMove : RegionNone;
}

inline RegionBounds regionDragBounds(RegionBounds before, int handle, MapFeaturePoint start,
                                     MapFeaturePoint end, int mapWidth, int mapHeight)
{
    mapWidth=std::max(1,mapWidth); mapHeight=std::max(1,mapHeight);
    if (handle == RegionCreate)
    {
        const int x0=std::clamp(static_cast<int>(std::floor(std::min(start.x,end.x))),0,mapWidth-1);
        const int y0=std::clamp(static_cast<int>(std::floor(std::min(start.y,end.y))),0,mapHeight-1);
        const int x1=std::clamp(static_cast<int>(std::ceil(std::max(start.x,end.x))),x0+1,mapWidth);
        const int y1=std::clamp(static_cast<int>(std::ceil(std::max(start.y,end.y))),y0+1,mapHeight);
        return {x0,y0,x1-x0,y1-y0};
    }
    const int dx=static_cast<int>(std::round(end.x-start.x));
    const int dy=static_cast<int>(std::round(end.y-start.y));
    before.width=std::clamp(before.width,1,mapWidth);
    before.height=std::clamp(before.height,1,mapHeight);
    before.x=std::clamp(before.x,0,mapWidth-before.width);
    before.y=std::clamp(before.y,0,mapHeight-before.height);
    if (handle == RegionMove)
        return {std::clamp(before.x+dx,0,mapWidth-before.width),
                std::clamp(before.y+dy,0,mapHeight-before.height),before.width,before.height};
    int left=before.x, top=before.y, right=left+before.width, bottom=top+before.height;
    if (handle & RegionLeft) left=std::clamp(left+dx,0,right-1);
    if (handle & RegionRight) right=std::clamp(right+dx,left+1,mapWidth);
    if (handle & RegionTop) top=std::clamp(top+dy,0,bottom-1);
    if (handle & RegionBottom) bottom=std::clamp(bottom+dy,top+1,mapHeight);
    return {left,top,right-left,bottom-top};
}
}
