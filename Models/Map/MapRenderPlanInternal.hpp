#pragma once

#include "Models/Map/MapRenderPlan.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace MapRenderPlanInternal
{
void hashBytes(std::uint64_t &hash, const void *data, std::size_t size);

template <typename Value>
void hashValue(std::uint64_t &hash, const Value &value)
{
    hashBytes(hash, &value, sizeof(Value));
}

void hashString(std::uint64_t &hash, const std::string &value);
std::uint64_t compiledContentHash(const MapModel &map,const MapRenderPlan &plan);

template <typename Collection>
bool containsId(const Collection &ids, const EntityId &id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool generatedFeatureLayerEnabled(const MapGeneratedFeature &feature, std::uint32_t layers);
std::uint32_t generatedFeatureContentLayer(const MapGeneratedFeature &feature);
const char *generatedFeatureAssetRole(const MapModel &map,
                                      const MapGeneratedFeature &feature);
MapRenderPrimitiveKind primitiveKindForLayer(std::uint32_t layer);
bool planContainsId(const std::vector<EntityId> &ids, const EntityId &id);
bool playerMapCellIsRevealed(const MapModel &map, int x, int y);
bool playerMapPositionIsRevealed(const MapModel &map, float x, float y);
bool playerMapLayerIsVisible(const MapModel &map, const EntityId &layerId);
bool playerMapBackgroundIsFullyRevealed(const MapModel &map);
bool generatedFeatureUsesPointProjection(MapGeneratedFeatureKind kind);
inline MapTileKind suppressedBuildingUnderlay(const MapModel &map)
{
    const auto type=map.metadata.customProperties.find("tactical.template_kind");
    const auto &recipe=map.generation.recipe;
    // The generated plaza's declared base is stone. Restoring generic grass
    // here would leave a building-shaped hole in its released paving.
    return map.atlasLevel==MapLevel::Tactical && recipe.generatorVersion>=12 && recipe.recipeVersion>=10 &&
        recipe.styleId=="illustrated_atlas" && recipe.styleVersion==6 &&
        recipe.styleSnapshotId=="illustrated_atlas_v6" && type!=map.metadata.customProperties.end() && type->second=="5"
            ? MapTileKind::Stone : MapTileKind::Empty;
}
void suppressUnreviewedPointProjections(MapModel &filtered, const MapModel &original,
                                        const MapRenderPlan &plan);
void suppressUnreviewedMountainProjections(MapModel &filtered, const MapModel &original,
                                           const MapRenderPlan &plan);
void suppressPlayerRasterLinearProjections(MapModel &filtered);
std::vector<MapRenderPrimitive> buildRenderPrimitives(const MapModel &map,
                                                      const MapRenderPlan &plan);
}
