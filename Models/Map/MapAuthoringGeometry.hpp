#pragma once

#include "Models/Map/MapAuthoringManualWays.hpp"

struct MapGeneratedFeatureHit
{
    int featureIndex = -1;
    int pointIndex = -1;
    int segmentStartIndex = -1;
    float segmentAmount = 0.0f;
    float distanceCells = std::numeric_limits<float>::max();
};

inline float mapFeaturePointDistanceSquared(MapFeaturePoint left, MapFeaturePoint right)
{
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    return dx * dx + dy * dy;
}

inline std::vector<int> generatedMapFeatureEditHandles(const MapGeneratedFeature &feature)
{
    std::vector<int> handles;
    float distance = 0.0f;
    for (int i = 0; i < static_cast<int>(feature.points.size()); ++i)
    {
        if (i > 0) distance += std::sqrt(mapFeaturePointDistanceSquared(feature.points[i - 1], feature.points[i]));
        if (feature.points.size() <= 16 || i == 0 || i + 1 == static_cast<int>(feature.points.size()) || distance >= 2.0f)
        {
            handles.push_back(i);
            distance = 0.0f;
        }
    }
    return handles;
}

inline MapGeneratedFeatureHit hitTestGeneratedMapFeature(
    const MapModel &map,
    MapFeaturePoint position,
    float maximumDistanceCells,
    bool pointsOnly = false)
{
    MapGeneratedFeatureHit best;
    const float maximumDistanceSquared = maximumDistanceCells * maximumDistanceCells;
    float bestDistanceSquared = maximumDistanceSquared;
    if (!pointsOnly)
    {
        // At atlas fit scale, roads often terminate exactly under place
        // symbols. Select the visible symbol within the existing hit radius
        // before considering an overlapping way's geometry control points.
        // Explicit control-point picking retains its nearest-vertex behavior.
        for (int featureIndex = 0;
             featureIndex < static_cast<int>(map.generation.graph.features.size()); ++featureIndex)
        {
            const MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
            if (feature.points.size() != 1 ||
                (feature.kind != MapGeneratedFeatureKind::Settlement &&
                 feature.kind != MapGeneratedFeatureKind::Landmark &&
                 feature.kind != MapGeneratedFeatureKind::Bridge &&
                 feature.kind != MapGeneratedFeatureKind::TacticalSetpiece))
                continue;
            const float distanceSquared = mapFeaturePointDistanceSquared(position, feature.points.front());
            if (distanceSquared <= bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = {featureIndex, 0, -1, 0.0f, std::sqrt(distanceSquared)};
            }
        }
        if (best.featureIndex >= 0)
            return best;
    }
    for (int featureIndex = 0;
         featureIndex < static_cast<int>(map.generation.graph.features.size()); ++featureIndex)
    {
        const MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
        for (int pointIndex : generatedMapFeatureEditHandles(feature))
        {
            const float distanceSquared = mapFeaturePointDistanceSquared(
                position, feature.points[static_cast<std::size_t>(pointIndex)]);
            if (distanceSquared <= bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = {featureIndex, pointIndex, -1, 0.0f, std::sqrt(distanceSquared)};
            }
        }
    }
    if (best.featureIndex >= 0 || pointsOnly)
        return best;

    for (int featureIndex = 0;
         featureIndex < static_cast<int>(map.generation.graph.features.size()); ++featureIndex)
    {
        const MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
        if (feature.points.size() < 2)
            continue;
        const int segmentCount = static_cast<int>(feature.points.size()) - 1 + (feature.closed ? 1 : 0);
        for (int segment = 0; segment < segmentCount; ++segment)
        {
            const MapFeaturePoint &start = feature.points[static_cast<std::size_t>(segment)];
            const MapFeaturePoint &end = feature.points[static_cast<std::size_t>((segment + 1) % feature.points.size())];
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const float lengthSquared = dx * dx + dy * dy;
            const float amount = lengthSquared <= 0.000001f
                                     ? 0.0f
                                     : std::clamp(((position.x - start.x) * dx +
                                                   (position.y - start.y) * dy) /
                                                      lengthSquared,
                                                  0.0f, 1.0f);
            const MapFeaturePoint closest{start.x + dx * amount, start.y + dy * amount};
            const float distanceSquared = mapFeaturePointDistanceSquared(position, closest);
            if (distanceSquared <= bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = {featureIndex, -1, segment, amount, std::sqrt(distanceSquared)};
            }
        }
    }
    return best;
}

inline bool moveGeneratedMapFeaturePoint(MapModel &map, int featureIndex, int pointIndex,
                                         MapFeaturePoint position)
{
    if (featureIndex < 0 || featureIndex >= static_cast<int>(map.generation.graph.features.size()))
        return false;
    MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
    if (feature.locked || pointIndex < 0 || pointIndex >= static_cast<int>(feature.points.size()))
        return false;
    feature.points[static_cast<std::size_t>(pointIndex)] = {
        std::clamp(position.x, 0.0f, static_cast<float>(map.width)),
        std::clamp(position.y, 0.0f, static_cast<float>(map.height))};
    return true;
}

// Dense generated polylines are display geometry. Move a section with a smooth
// falloff, measured from the drag's immutable start so event rate cannot alter it.
inline bool moveGeneratedMapFeatureSection(MapModel &map, int featureIndex, int pointIndex,
                                           MapFeaturePoint position, const MapGeneratedFeature &before)
{
    if (featureIndex < 0 || featureIndex >= static_cast<int>(map.generation.graph.features.size()) ||
        before.locked || pointIndex < 0 || pointIndex >= static_cast<int>(before.points.size())) return false;
    auto &feature = map.generation.graph.features[featureIndex];
    if (feature.id != before.id || feature.locked) return false;
    feature.points = before.points;
    if (before.points.size() <= 16 || (before.kind != MapGeneratedFeatureKind::Road &&
        before.kind != MapGeneratedFeatureKind::Trail && before.kind != MapGeneratedFeatureKind::River))
        return moveGeneratedMapFeaturePoint(map, featureIndex, pointIndex, position);
    const auto anchor = before.points[pointIndex];
    const float dx = position.x - anchor.x, dy = position.y - anchor.y;
    const float radius = std::max(3.0f, std::hypot(dx, dy) * 1.25f);
    for (int direction : {-1, 1})
    {
        float distance = 0.0f;
        for (int i = pointIndex; i >= 0 && i < static_cast<int>(before.points.size()); i += direction)
        {
            if (i != pointIndex) distance += std::sqrt(mapFeaturePointDistanceSquared(before.points[i], before.points[i - direction]));
            if (distance >= radius) break;
            if (i != pointIndex && (i == 0 || i + 1 == static_cast<int>(before.points.size()))) break;
            const float t = 1.0f - distance / radius;
            const float weight = t * t * (3.0f - 2.0f * t);
            feature.points[i] = {std::clamp(before.points[i].x + dx * weight, 0.0f, static_cast<float>(map.width)),
                                 std::clamp(before.points[i].y + dy * weight, 0.0f, static_cast<float>(map.height))};
        }
    }
    return true;
}

inline int insertGeneratedMapFeaturePoint(MapGeneratedFeature &feature, int segmentStartIndex,
                                          float segmentAmount)
{
    if (feature.locked || feature.points.size() < 2 || segmentStartIndex < 0)
        return -1;
    const int pointCount = static_cast<int>(feature.points.size());
    const int segmentCount = pointCount - 1 + (feature.closed ? 1 : 0);
    if (segmentStartIndex >= segmentCount)
        return -1;
    const int endIndex = (segmentStartIndex + 1) % pointCount;
    segmentAmount = std::clamp(segmentAmount, 0.05f, 0.95f);
    const MapFeaturePoint &start = feature.points[static_cast<std::size_t>(segmentStartIndex)];
    const MapFeaturePoint &end = feature.points[static_cast<std::size_t>(endIndex)];
    const MapFeaturePoint inserted{
        start.x + (end.x - start.x) * segmentAmount,
        start.y + (end.y - start.y) * segmentAmount};
    const int insertionIndex = segmentStartIndex + 1;
    feature.points.insert(feature.points.begin() + insertionIndex, inserted);
    return insertionIndex;
}

inline bool removeGeneratedMapFeaturePoint(MapGeneratedFeature &feature, int pointIndex)
{
    const std::size_t minimumPoints = feature.closed ? 3u : 2u;
    if (feature.locked || feature.points.size() <= minimumPoints || pointIndex < 0 ||
        pointIndex >= static_cast<int>(feature.points.size()))
        return false;
    feature.points.erase(feature.points.begin() + pointIndex);
    return true;
}

// An atlas river is drawn over the existing coast. Standing water must
// retain Water/None: marking it River tells the terrain renderer to recover
// land underneath the channel and would grow a peninsula into the ocean.
inline bool mapAuthoringPreservesStandingWater(const MapModel &map,const MapCellModel &cell,MapWayKind wayKind)
{
    return map.generation.continuousRendering &&
        (map.atlasLevel==MapLevel::World || map.atlasLevel==MapLevel::Region) &&
        (wayKind==MapWayKind::River || wayKind==MapWayKind::Canal) &&
        cell.kind==MapTileKind::Water && cell.wayKind==MapWayKind::None;
}

// Shared production implementation keeps river/road footprint repairs out of
// the public authoring header and avoids recompiling every editor for a fix.
void projectGeneratedWayFeatureToCells(MapModel &map,const MapGeneratedFeature &feature,bool erase,
                                      const std::vector<MapCellCoordinate> *affected=nullptr);
void reprojectGeneratedMapWayFeaturesAfterGeometryEdit(MapModel &map,const MapGeneratedFeature &before);

