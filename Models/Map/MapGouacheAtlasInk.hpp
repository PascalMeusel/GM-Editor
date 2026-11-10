#pragma once

#include "Models/Map/MapIllustratedAtlasTypes.hpp"

namespace gm::map_render
{
inline bool usesGouacheAtlasArtwork(const MapModel &map)
{
    return usesIllustratedAtlasDetailed(map) && map.generation.recipe.recipeVersion >= 11;
}

std::vector<AtlasInkPrimitive> buildGouacheAtlasSettlementInk(
    const MapModel &map, MapFeaturePoint center, const std::string &material);
std::vector<AtlasInkPrimitive> buildGouacheAtlasRoadInk(
    const MapModel &map, const MapGeneratedFeature &feature);
}
