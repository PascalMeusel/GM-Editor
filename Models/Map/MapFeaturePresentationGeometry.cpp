#include "Models/Map/MapFeaturePresentationGeometry.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapRenderPlan.hpp"

std::vector<MapFeaturePoint> mapFeaturePresentationPoints(const MapModel &map, const MapGeneratedFeature &feature)
{
    using namespace gm::map_render;
    if (feature.kind == MapGeneratedFeatureKind::River)
    {
        if (usesTacticalRiverDetail(map))
            return tacticalRiverDetailPoints(map, feature);
        if (usesIllustratedAtlasDetailed(map))
            return illustratedAtlasFinalRiverPoints(map, feature);
    }
    if ((usesIllustratedAtlas(map) || map.generation.recipe.generatorVersion >= 6) &&
        (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail))
        return illustratedAtlasRoadInkPoints(map, feature);
    return feature.points;
}

std::vector<gm::map_geometry::MapRoadCorridor> mapPresentationCorridors(
    const MapModel &map, const std::function<bool(const MapGeneratedFeature &)> &visible,
    const MapRenderPlan *presentation)
{
    std::vector<gm::map_geometry::MapRoadCorridor> result;
    for (const auto &feature : map.generation.graph.features)
        if ((!visible || visible(feature)) &&
            (feature.kind == MapGeneratedFeatureKind::River || feature.kind == MapGeneratedFeatureKind::Road ||
             feature.kind == MapGeneratedFeatureKind::Trail))
        {
            const std::vector<MapFeaturePoint> *geometry = nullptr;
            if (presentation && presentation->mapId == map.metadata.id)
                for (const auto &primitive : presentation->primitives)
                    if (primitive.sourceId == feature.id && primitive.kind == MapRenderPrimitiveKind::GeneratedGeometry)
                    {
                        geometry = &primitive.points;
                        break;
                    }
            result.push_back(
                {feature.id, geometry ? *geometry : mapFeaturePresentationPoints(map, feature),
                 feature.kind == MapGeneratedFeatureKind::River ? std::max(.12f, feature.width * .12f) : .10f});
        }
    return result;
}

MapGeneratedFeatureHit hitTestPresentedMapFeature(const MapModel &map, MapFeaturePoint point, float radius,
                                                  const MapRenderPlan *presentation)
{
    if (presentation && presentation->mapId != map.metadata.id)
        presentation = nullptr;
    auto best = hitTestGeneratedMapFeature(map, point, radius);
    auto isRoute = [](const MapGeneratedFeature &feature) {
        return feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail ||
               feature.kind == MapGeneratedFeatureKind::River;
    };
    if (best.featureIndex >= 0)
    {
        const auto &feature = map.generation.graph.features[best.featureIndex];
        // Visible place symbols keep priority over terminating routes.
        if (!isRoute(feature) && best.pointIndex >= 0)
            return best;
        if (isRoute(feature))
            best = {};
    }
    float distanceSquared = best.featureIndex >= 0 ? best.distanceCells * best.distanceCells : radius * radius;
    for (int index = 0; index < static_cast<int>(map.generation.graph.features.size()); ++index)
    {
        const auto &feature = map.generation.graph.features[index];
        if (!isRoute(feature))
            continue;
        std::vector<MapFeaturePoint> resolved;
        const std::vector<MapFeaturePoint> *geometry = nullptr;
        if (presentation && presentation->mapId == map.metadata.id)
            for (const auto &primitive : presentation->primitives)
                if (primitive.sourceId == feature.id && primitive.kind == MapRenderPrimitiveKind::GeneratedGeometry)
                {
                    geometry = &primitive.points;
                    break;
                }
        if (!geometry)
        {
            // New objects have only their current source geometry until the
            // displayed preview catches up. Never rebuild every route on input.
            resolved = presentation ? feature.points : mapFeaturePresentationPoints(map, feature);
            geometry = &resolved;
        }
        const auto &points = *geometry;
        for (std::size_t segment = 1; segment < points.size(); ++segment)
        {
            const auto a = points[segment - 1], b = points[segment];
            const float dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
            const float t =
                length < 0.000001f ? 0.f : std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length, 0.f, 1.f);
            const float candidate = mapFeaturePointDistanceSquared(point, {a.x + t * dx, a.y + t * dy});
            if (candidate <= distanceSquared)
            {
                distanceSquared = candidate;
                // A presentation segment is never a valid insertion index.
                best = {index, -1, -1, 0.f, std::sqrt(candidate)};
            }
        }
    }
    return best;
}
