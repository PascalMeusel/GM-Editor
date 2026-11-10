#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <cstdint>
#include <string>

namespace MapPngExportRaster
{
void drawOverviewMountainRanges(std::vector<Pixel> &pixels, int width, int height,
                                const MapModel &map, int pixelsPerCell);
Pixel sampleTerrainAsset(const std::string &snapshotId, MapLevel level, MapTileKind kind,
                         int pixelX, int pixelY, int pixelsPerCell, std::uint32_t seed);
}
