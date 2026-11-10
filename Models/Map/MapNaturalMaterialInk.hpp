#pragma once
#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
void appendSanctuaryPavingInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map);
std::vector<AtlasInkPrimitive> buildIllustratedLocalRoadDetailInk(
    const MapModel &map,const MapGeneratedFeature &feature,const std::vector<MapFeaturePoint> &points);
} // namespace gm::map_render
