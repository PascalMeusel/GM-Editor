#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

namespace MapPngExportRaster
{
void applyWorldPainterlySurface(std::vector<Pixel> &pixels,
                                int width, int height, const MapModel &map);
}
