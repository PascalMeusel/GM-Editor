#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"

namespace gm::map_render
{
// Material shading follows existing triangles. It must not change the contour,
// cell occupancy, props, entrances or fog contract of an underground map.
void shadeCaveFloorMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                            OverviewRenderColor floor);
void appendDungeonSlabMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                              int x, int y, OverviewRenderColor floor);
void appendUndergroundWallMaterial(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                                   MapFeaturePoint outerA, MapFeaturePoint outerB,
                                   MapFeaturePoint innerA, MapFeaturePoint innerB, bool cave);
}
