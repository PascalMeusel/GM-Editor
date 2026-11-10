#pragma once

#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapLocalIllustration.hpp"

namespace gm::map_render
{
struct MapWayStrokeDisplayWidth
{
    float surfacePixels = 0.0f;
    float outerPixels = 0.0f;
    // Rivers taper at sources and may flare at an actual water connection.
    // The cursor describes the normal downstream trunk, not an estuary.
    bool tapers = false;
};

// Match the native continuous-feature renderer's presentation. A semantic
// brush diameter is not a pixel width: each map scale has its own line weight.
inline MapWayStrokeDisplayWidth continuousMapWayDisplayWidth(
    const MapModel &map, const MapGeneratedFeature &feature, float cellPixels)
{
    MapWayStrokeDisplayWidth result;
    if (!std::isfinite(cellPixels) || cellPixels <= 0.0f) return result;
    const auto &profile = atlasPresentationProfile(map);
    const bool world = map.atlasLevel == MapLevel::World || usesIllustratedAtlas(map);
    if (feature.kind == MapGeneratedFeatureKind::River)
    {
        result.surfacePixels = std::max(1.0f, feature.width) * cellPixels * profile.riverWidthCells;
        result.tapers = world;
        if (usesIllustratedAtlasRiverMesh(map))
        {
            result.surfacePixels = std::max(0.13f * cellPixels, result.surfacePixels);
            result.outerPixels = result.surfacePixels * 1.12f;
        }
        else result.outerPixels = result.surfacePixels + cellPixels * (world ? 0.070f : 0.22f);
        return result;
    }
    if (feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail)
        return result;
    const bool trail = feature.kind == MapGeneratedFeatureKind::Trail;
    const bool localRoad = !trail && (map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical);
    result.surfacePixels = usesIllustratedAtlasDetailed(map)
        ? illustratedAtlasV2RoadWidth(feature, cellPixels)
        : std::max(trail ? 2.0f : 3.0f, feature.width * cellPixels * profile.roadWidthCells *
                   (localRoad ? 1.14f : 1.0f) * (world ? kOverviewRoadWidthScale : 1.0f));
    result.outerPixels = result.surfacePixels + (usesIllustratedLocalV6(map)
        ? std::max(1.4f, cellPixels / 16.0f)
        : cellPixels * (world ? 0.055f : (!trail && feature.width >= 0.88f ? 0.20f : 0.16f)));
    return result;
}

inline MapWayStrokeDisplayWidth manualWayStrokeDisplayWidth(
    const MapModel &map, MapWayKind way, int brushSize, float cellPixels)
{
    if (way == MapWayKind::None) return {};
    const bool river = way == MapWayKind::River || way == MapWayKind::Canal;
    MapGeneratedFeature feature;
    feature.kind = river ? MapGeneratedFeatureKind::River :
                   way == MapWayKind::Trail ? MapGeneratedFeatureKind::Trail : MapGeneratedFeatureKind::Road;
    feature.width = std::clamp(brushSize, 1, 32) * (river ? 1.0f : way == MapWayKind::Trail ? 0.55f : 0.82f);
    return continuousMapWayDisplayWidth(map, feature, cellPixels);
}
}
