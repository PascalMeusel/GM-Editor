#pragma once

#include "Models/Map/MapModels.hpp"

namespace gm::map_render
{
inline bool usesAtlasSerifTypography(const MapModel &map)
{
    return map.generation.recipe.recipeVersion >= 10 &&
           map.generation.recipe.styleId == "illustrated_atlas" &&
           (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region);
}
} // namespace gm::map_render
