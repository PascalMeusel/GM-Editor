#pragma once
#include "Models/Map/MapIllustratedAtlas.hpp"

namespace gm::map_render {
inline bool usesIllustratedTacticalV12(const MapModel &map) {
    const auto &r=map.generation.recipe;
    return map.atlasLevel==MapLevel::Tactical && r.generatorVersion>=12 && r.styleId=="illustrated_atlas" &&
        r.styleVersion==6 && r.styleSnapshotId=="illustrated_atlas_v6";
}
inline bool usesTacticalStructureInk(const MapModel &map) {
    return map.atlasLevel == MapLevel::Tactical && map.generation.recipe.generatorVersion >= 10;
}

// Settlement site roofs share the exact-cell footprint renderer. Other site
// maps, old saves and tactical objective/crossing helpers keep their rules.
inline bool supportsFootprintRoofInk(const MapModel &map) {
    return usesTacticalStructureInk(map) ||
        (map.atlasLevel == MapLevel::Site && map.mapKind == MapKind::Settlement &&
         map.generation.recipe.generatorVersion >= 10);
}

// Production crossing art must respect the resolved span's relative width.
// Historical maps retain their former fixed 1.75-cell stamp width.
inline float tacticalBridgeStampWidth(const MapModel &map,const MapGeneratedFeature &feature) {
    if(!usesTacticalStructureInk(map)) return 1.75f;
    const float materialScale=feature.materialId.find("stone_causeway")!=std::string::npos?.75f:1.75f;
    return std::max(1.f,materialScale*std::clamp(feature.width,.5f,1.5f));
}

inline bool tacticalShrineAt(const MapModel &map,int x,int y) {
    if(!usesTacticalStructureInk(map) || x!=map.width/2 || y!=map.height/2) return false;
    const auto kind=map.metadata.customProperties.find("tactical.template_kind");
    return kind!=map.metadata.customProperties.end() && kind->second=="0";
}

inline bool tacticalWaystoneAt(const MapModel &map,int x,int y) {
    if(!usesTacticalStructureInk(map)) return false;
    return std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &feature) {
        return feature.materialId=="fantasy_neutral.tactical.waystone" && !feature.points.empty() &&
            static_cast<int>(std::floor(feature.points.front().x))==x && static_cast<int>(std::floor(feature.points.front().y))==y;
    });
}

// Roof geometry occupies the actual blocked cells, rather than displaying a
// one-cell village icon somewhere inside a much larger invisible obstacle.
// Rectangular partitions also preserve edited L-shaped building footprints.
std::vector<AtlasInkPrimitive> buildTacticalStructureInk(const MapModel &map);
}
