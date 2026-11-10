#pragma once
#include "Models/Map/MapModels.hpp"
#include <algorithm>

namespace gm::map_render {
// Bounds are in map cells and include the label halo/plate. Both native
// drawing and PNG placement must keep new-map labels inside the canvas.
inline bool overviewLabelBoxFitsMap(const MapModel &map,float left,float top,float right,float bottom)
{
    return map.generation.recipe.generatorVersion<11 ||
        (left>=-.0001f && top>=-.0001f && right<=map.width+.0001f && bottom<=map.height+.0001f && right>=left && bottom>=top);
}
// Use the same size budget in editor/player drawing and PNG export. A small
// overview must not keep giant nation names after its city labels disappear.
// Names return unchanged when zoom provides enough space; do not abbreviate
// authored names or shrink them below a readable minimum.
inline float overviewTerritoryLabelFontSize(const MapModel &map,float cellPixels,
                                            float preferredPixels,float measuredWidth)
{
    if(map.generation.recipe.generatorVersion<11)return preferredPixels;
    if(cellPixels<5.f || preferredPixels<=0.f || measuredWidth<=0.f)return 0.f;
    const float widthBudget=map.width*cellPixels*.28f;
    const float heightBudget=map.height*cellPixels*.08f;
    const float fitted=std::min({preferredPixels,heightBudget,preferredPixels*widthBudget/measuredWidth});
    return fitted>=11.f?fitted:0.f;
}
}
