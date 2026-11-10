#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <string_view>

namespace MapPngExportRaster
{
std::vector<MapFeaturePoint> smoothFeaturePoints(
    const std::vector<MapFeaturePoint> &source);
void drawCircleOutline(std::vector<Pixel> &pixels, int width, int height,
                       int centerX, int centerY, int radius,
                       int thickness, Pixel color);
void drawTacticalRegionOverlays(std::vector<Pixel> &pixels, int width, int height,
                                const MapModel &map, int pixelsPerCell, bool playerSafe);
void drawPoliticalBoundaries(std::vector<Pixel> &pixels, int width, int height,
                             const MapModel &map, int pixelsPerCell);
void drawManualAnnotations(std::vector<Pixel> &pixels, int width, int height,
                           const MapModel &map, int pixelsPerCell, bool showLabels,
                           std::string_view locale);
void drawRoutes(std::vector<Pixel> &pixels, int width, int height,
                const MapRenderPlan &plan,
                int pixelsPerCell, float mapOffsetX, float mapOffsetY);
void drawTokens(std::vector<Pixel> &pixels, int width, int height,
                const MapModel &map, int pixelsPerCell, bool showLabels);
}
