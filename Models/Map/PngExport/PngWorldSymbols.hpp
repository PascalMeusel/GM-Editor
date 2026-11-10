#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <string_view>

namespace MapPngExportRaster
{
void drawWorldForestPattern(std::vector<Pixel> &pixels, int width, int height,
                            const MapModel &map, int pixelsPerCell);
void drawWorldOverviewRidges(std::vector<Pixel> &pixels, int width, int height,
                             const MapModel &map, int pixelsPerCell);
void drawWorldSettlementGlyph(std::vector<Pixel> &pixels, int width, int height,
                              int centerX, int centerY, int pixelsPerCell,
                              std::string_view semanticRole);
}
