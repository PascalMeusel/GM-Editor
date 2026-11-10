#pragma once

#include "Models/Map/MapModels.hpp"
#include "Models/Map/MapStyleRegistry.hpp"

enum class MapCreatorArtStyle { Illustrated, IllustratedClassic, Neutral };

inline bool mapCreatorArtStyleMatches(const MapGenerationRecipe &recipe, MapCreatorArtStyle style)
{
    if (style == MapCreatorArtStyle::Neutral)
        return recipe.styleId == "fantasy_neutral" && recipe.styleVersion == 3 &&
               recipe.styleSnapshotId == mapStyleCurrentProductionSnapshotId();
    const int version = style == MapCreatorArtStyle::Illustrated
        ? (recipe.generatorVersion >= 12 && recipe.scale != MapLevel::World ? 6 : 5) : 1;
    const char *snapshot = version == 6 ? "illustrated_atlas_v6" : version == 5 ? "illustrated_atlas_v5" : "illustrated_atlas_v1";
    return recipe.styleId == "illustrated_atlas" && recipe.styleVersion == version &&
           recipe.styleSnapshotId == snapshot;
}

// An explicit artwork choice changes rendering only. Stored generation,
// geography and consent are preserved until the user requests a new design.
inline void applyMapCreatorArtStyle(MapGenerationRecipe &recipe, MapCreatorArtStyle style)
{
    const bool neutral = style == MapCreatorArtStyle::Neutral;
    recipe.styleId = neutral ? "fantasy_neutral" : "illustrated_atlas";
    recipe.styleVersion = neutral ? 3 : style == MapCreatorArtStyle::Illustrated
        ? (recipe.generatorVersion >= 12 && recipe.scale != MapLevel::World ? 6 : 5) : 1;
    recipe.styleSnapshotId = neutral ? mapStyleCurrentProductionSnapshotId()
                                    : style == MapCreatorArtStyle::Illustrated
                                          ? (recipe.styleVersion == 6 ? "illustrated_atlas_v6" : "illustrated_atlas_v5") : "illustrated_atlas_v1";
}
