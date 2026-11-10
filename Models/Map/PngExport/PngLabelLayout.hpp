#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

namespace MapPngExportRaster
{
struct PngLabelBounds
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

void blendRect(std::vector<Pixel> &pixels, int width, int height,
               int left, int top, int right, int bottom, Pixel color);
bool pngLabelBoundsOverlap(const PngLabelBounds &left,
                           const PngLabelBounds &right, int gap);
bool pngFeaturePolygonContains(const MapGeneratedFeature &feature, float x, float y);
bool pngMapPointIsVisibleLand(const MapModel &map, float x, float y, bool playerSafe);
bool pngTerritoryLabelAnchor(const MapModel &map, const MapGeneratedFeature &feature,
                             bool playerSafe, MapFeaturePoint &anchor);
bool pngLabelAreaIsPlayerSafe(const MapModel &map, const PngLabelBounds &bounds,
                              int pixelsPerCell, bool playerSafe);
}
