#include "Models/Map/PngExport/PngLabelLayout.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"

#include <algorithm>
#include <cmath>

namespace MapPngExportRaster
{
void blendRect(std::vector<Pixel> &pixels, int width, int height,
               int left, int top, int right, int bottom, Pixel color)
{
    const int clippedLeft = std::max(0, left);
    const int clippedTop = std::max(0, top);
    const int clippedRight = std::min(width, right);
    const int clippedBottom = std::min(height, bottom);
    for (int y = clippedTop; y < clippedBottom; ++y)
        for (int x = clippedLeft; x < clippedRight; ++x)
        {
            Pixel &destination = pixels[static_cast<std::size_t>(y * width + x)];
            destination = alphaBlend(destination, color, 255);
        }
}

bool pngLabelBoundsOverlap(const PngLabelBounds &left, const PngLabelBounds &right, int gap)
{
    return left.left < right.right + gap && left.right + gap > right.left &&
           left.top < right.bottom + gap && left.bottom + gap > right.top;
}

bool pngFeaturePolygonContains(const MapGeneratedFeature &feature, float x, float y)
{
    if (!feature.closed || feature.points.size() < 3)
        return false;
    bool inside = false;
    for (std::size_t current = 0, previous = feature.points.size() - 1;
         current < feature.points.size(); previous = current++)
    {
        const MapFeaturePoint &a = feature.points[current];
        const MapFeaturePoint &b = feature.points[previous];
        if ((a.y > y) == (b.y > y))
            continue;
        const float denominator = b.y - a.y;
        const float crossingX = a.x + (b.x - a.x) * (y - a.y) /
                                         (std::abs(denominator) < 0.00001f ? 0.00001f : denominator);
        if (x < crossingX)
            inside = !inside;
    }
    return inside;
}

bool pngMapPointIsVisibleLand(const MapModel &map, float x, float y, bool playerSafe)
{
    const int cellX = static_cast<int>(std::floor(x));
    const int cellY = static_cast<int>(std::floor(y));
    const MapCellModel *cell = mapCellAt(map, cellX, cellY);
    return cell &&
           gm::map_render::overviewGroundKindForRender(map, cellX, cellY) != MapTileKind::Water &&
           (!playerSafe || !cell->fogged);
}

bool pngTerritoryLabelAnchor(const MapModel &map, const MapGeneratedFeature &feature,
                             bool playerSafe, MapFeaturePoint &anchor)
{
    if (feature.points.empty())
        return false;
    MapFeaturePoint quietAnchor{};
    if (gm::map_render::overviewTerritoryLabelPoint(map, feature, quietAnchor) &&
        pngMapPointIsVisibleLand(map, quietAnchor.x, quietAnchor.y, playerSafe))
    {
        anchor = quietAnchor;
        return true;
    }
    float minimumX = feature.points.front().x;
    float maximumX = minimumX;
    float minimumY = feature.points.front().y;
    float maximumY = minimumY;
    MapFeaturePoint centroid{};
    for (const MapFeaturePoint &point : feature.points)
    {
        centroid.x += point.x;
        centroid.y += point.y;
        minimumX = std::min(minimumX, point.x);
        maximumX = std::max(maximumX, point.x);
        minimumY = std::min(minimumY, point.y);
        maximumY = std::max(maximumY, point.y);
    }
    centroid.x /= static_cast<float>(feature.points.size());
    centroid.y /= static_cast<float>(feature.points.size());
    if (pngFeaturePolygonContains(feature, centroid.x, centroid.y) &&
        pngMapPointIsVisibleLand(map, centroid.x, centroid.y, playerSafe))
    {
        anchor = centroid;
        return true;
    }

    float bestDistance = static_cast<float>(map.width * map.width + map.height * map.height + 1);
    bool found = false;
    for (int y = std::max(0, static_cast<int>(std::floor(minimumY)));
         y <= std::min(map.height - 1, static_cast<int>(std::ceil(maximumY))); ++y)
        for (int x = std::max(0, static_cast<int>(std::floor(minimumX)));
             x <= std::min(map.width - 1, static_cast<int>(std::ceil(maximumX))); ++x)
        {
            const MapFeaturePoint candidate{static_cast<float>(x) + 0.5f,
                                            static_cast<float>(y) + 0.5f};
            if (!pngFeaturePolygonContains(feature, candidate.x, candidate.y) ||
                !pngMapPointIsVisibleLand(map, candidate.x, candidate.y, playerSafe))
                continue;
            const float dx = candidate.x - centroid.x;
            const float dy = candidate.y - centroid.y;
            const float distance = dx * dx + dy * dy;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                anchor = candidate;
                found = true;
            }
        }
    return found;
}

bool pngLabelAreaIsPlayerSafe(const MapModel &map, const PngLabelBounds &bounds,
                              int pixelsPerCell, bool playerSafe)
{
    if (!playerSafe)
        return true;
    const int minimumX = std::clamp(bounds.left / pixelsPerCell, 0, map.width - 1);
    const int minimumY = std::clamp(bounds.top / pixelsPerCell, 0, map.height - 1);
    const int maximumX = std::clamp((std::max(bounds.left, bounds.right - 1)) / pixelsPerCell, 0, map.width - 1);
    const int maximumY = std::clamp((std::max(bounds.top, bounds.bottom - 1)) / pixelsPerCell, 0, map.height - 1);
    for (int y = minimumY; y <= maximumY; ++y)
        for (int x = minimumX; x <= maximumX; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->fogged)
                return false;
        }
    return true;
}


}
