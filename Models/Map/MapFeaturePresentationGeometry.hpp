#pragma once
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include <functional>
struct MapRenderPlan;

// Shared centerline for rendering and whole-object picking. Reshape continues
// to operate on source control points and source segment indices.
std::vector<MapFeaturePoint> mapFeaturePresentationPoints(const MapModel &map, const MapGeneratedFeature &feature);
MapGeneratedFeatureHit hitTestPresentedMapFeature(const MapModel &map, MapFeaturePoint point, float radius,
                                                  const MapRenderPlan *presentation = nullptr);
std::vector<gm::map_geometry::MapRoadCorridor> mapPresentationCorridors(
    const MapModel &map, const std::function<bool(const MapGeneratedFeature &)> &visible = {},
    const MapRenderPlan *presentation = nullptr);
