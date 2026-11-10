#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/PngExport/PngRasterCanvas.hpp"

#include <vector>

namespace MapPngExportRaster
{
float visualBuildingStreetRotation(const MapModel &map, int x, int y);
std::vector<MapFeaturePoint> offsetFeaturePoints(const std::vector<MapFeaturePoint> &points,
                                                 float offset);
const char *terrainSemanticRole(MapLevel level, MapTileKind kind);
int scaleClusterModulo(MapLevel level, MapTileKind kind);
Pixel terrainColor(MapTileKind kind);
Pixel blended(Pixel a, Pixel b, float amount);
bool generatedPointSymbolVisible(const MapModel &map, int x, int y, bool playerSafe);
const MapGeneratedFeature *generatedSettlementAt(const MapModel &map, int x, int y);
const char *overviewSettlementSemanticRole(const MapGeneratedFeature &feature);
const char *overviewSettlementSemanticRole(const MapModel &map, int x, int y);
}
