#pragma once
#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
void appendWeatheredRuinWallInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map,
                               int x,int y,bool applyFog);
void appendCivicPavingInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map,bool applyFog);
void appendWeatheredAltarInk(std::vector<AtlasInkPrimitive> &ink,MapFeaturePoint center,unsigned seed);
void appendFittedFountainInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map);
}
