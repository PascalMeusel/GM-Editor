#pragma once

#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <string_view>

namespace MapPngExportRaster
{
void drawGeneratedMapLabels(std::vector<Pixel> &pixels, int width, int height,
                            const MapModel &map, int pixelsPerCell, bool playerSafe,
                            std::string_view locale,
                            const gm::map_render::AtlasLabelInkMask &inkMask);
}
