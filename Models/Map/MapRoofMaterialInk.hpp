#pragma once
#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
// Bounds are already split into exact rectangular building footprints.
void appendWeatheredRoofMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                                 float left, float top, float right, float bottom,
                                 OverviewRenderColor light, OverviewRenderColor dark);
}
