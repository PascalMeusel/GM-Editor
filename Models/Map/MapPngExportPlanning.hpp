#pragma once

#include "Models/Map/MapPngExport.hpp"

// Internal, side-effect-free map-export policy shared by whole-map, viewport,
// and tiled exports. Keeping these operations out of the rasterizer makes the
// trust/geometry boundary independently testable and keeps rendering focused
// on producing pixels.
bool pngRectIsValidForMap(const MapPngExportRect &rect, const MapModel &map);
MapNormalizedRect pngNormalizedViewport(const MapModel &map,
                                        const MapPngExportRect &rect);
MapModel cropMapForPngExport(const MapModel &source,
                             const MapPngExportRect &rect);
bool mapPermitsPngExport(const MapModel &map,
                         MapRenderAudience audience,
                         std::string *error);
