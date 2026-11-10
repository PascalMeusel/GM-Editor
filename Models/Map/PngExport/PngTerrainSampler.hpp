#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <vector>

namespace MapPngExportRaster
{
Pixel continuousTerrainPixel(
    const MapModel &map,
    float cellX,
    float cellY,
    int pixelX,
    int pixelY,
    int pixelsPerCell,
    const std::vector<MapTileKind> *overviewGroundProjection = nullptr);
}
