#pragma once

#include "Models/Map/MapIllustratedAtlas.hpp"
#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

namespace MapPngExportRaster
{
void drawAtlasInk(std::vector<Pixel> &pixels, int width, int height,
                  const std::vector<gm::map_render::AtlasInkPrimitive> &ink,
                  int pixelsPerCell, float mapOffsetX, float mapOffsetY);
void drawOverviewCoastTreatment(std::vector<Pixel> &pixels, int width, int height,
                                const MapModel &map, int pixelsPerCell);
}
