#pragma once

#include "Models/Map/MapIllustratedAtlasMountainPass.hpp"

namespace gm::map_render
{
inline void appendIllustratedAtlasWorldCompass(
    std::vector<AtlasInkPrimitive> &ink,
    const MapModel &map)
{
    if (!usesIllustratedAtlasV5(map) || map.atlasLevel != MapLevel::World)
        return;

    for (const MapFeaturePoint center : {
             MapFeaturePoint{map.width - 3.2f, 3.2f},
             MapFeaturePoint{3.2f, 3.2f},
             MapFeaturePoint{map.width - 3.2f, map.height - 3.2f},
             MapFeaturePoint{3.2f, map.height - 3.2f}})
    {
        bool emptySea = true;
        for (int iy = -2; iy <= 2; ++iy)
            for (int ix = -2; ix <= 2; ++ix)
            {
                const int x = static_cast<int>(center.x) + ix;
                const int y = static_cast<int>(center.y) + iy;
                if (!isMapCellInside(map, x, y) ||
                    overviewGroundKindIsLand(overviewGroundKindForRender(map, x, y)))
                    emptySea = false;
            }
        if (!emptySea)
            continue;

        const OverviewRenderColor brass{214, 191, 125, 205};
        const OverviewRenderColor shadow{40, 88, 103, 185};
        for (int arm = 0; arm < 8; ++arm)
        {
            const float angle = arm * 0.78539816f - 1.5707963f;
            const float radius = arm % 2 == 0 ? 1.15f : 0.69f;
            const MapFeaturePoint tip{center.x + std::cos(angle) * radius,
                                      center.y + std::sin(angle) * radius};
            const MapFeaturePoint left{center.x + std::cos(angle + 0.85f) * 0.24f,
                                       center.y + std::sin(angle + 0.85f) * 0.24f};
            const MapFeaturePoint right{center.x + std::cos(angle - 0.85f) * 0.24f,
                                        center.y + std::sin(angle - 0.85f) * 0.24f};
            ink.push_back({{center, tip, left}, brass, 0.0f, true});
            ink.push_back({{center, right, tip}, shadow, 0.0f, true});
            ink.push_back({{left, tip, right}, {210, 196, 151, 175}, 0.025f, false});
        }
        ink.push_back({{{center.x - 0.19f, center.y - 1.48f},
                        {center.x - 0.19f, center.y - 1.95f},
                        {center.x + 0.19f, center.y - 1.48f},
                        {center.x + 0.19f, center.y - 1.95f}},
                       brass, 0.035f, false});
        break;
    }
}

inline void appendIllustratedAtlasCountrysideAndPasses(
    std::vector<AtlasInkPrimitive> &ink,
    const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &roads,
    const std::vector<gm::map_geometry::MapRoadCorridor> &originalRoads)
{
    if (map.generation.recipe.recipeVersion >= 6)
    {
        auto countryside = buildAtlasCountrysideInk(map, roads);
        ink.insert(ink.end(), countryside.begin(), countryside.end());
    }
    if (map.generation.recipe.recipeVersion < 8)
    {
        auto passes = buildIllustratedAtlasMountainPassInk(
            map, map.generation.recipe.recipeVersion >= 6 ? roads : originalRoads);
        ink.insert(ink.end(), passes.begin(), passes.end());
    }
}
} // namespace gm::map_render
