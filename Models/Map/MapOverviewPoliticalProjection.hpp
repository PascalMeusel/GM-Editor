#pragma once

#include "Models/Map/MapOverviewGroundPolicy.hpp"

namespace gm::map_render
{
inline std::vector<MapTileKind> buildOverviewGroundProjection(const MapModel &map)
{
    std::vector<MapTileKind> result;
    if (map.width <= 0 || map.height <= 0)
        return result;
    result.resize(static_cast<std::size_t>(map.width) *
                  static_cast<std::size_t>(map.height));
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            result[static_cast<std::size_t>(mapCellIndex(map, x, y))] =
                overviewGroundKindForRender(map, x, y);
    return result;
}

inline MapTileKind overviewGroundKindFromProjection(
    const MapModel &map,
    const std::vector<MapTileKind> &projection,
    int x,
    int y)
{
    if (isMapCellInside(map, x, y) &&
        projection.size() == static_cast<std::size_t>(map.width) *
                                 static_cast<std::size_t>(map.height))
        return projection[static_cast<std::size_t>(mapCellIndex(map, x, y))];
    return overviewGroundKindForRender(map, x, y);
}

// The broad atlas coast kernel can erase an entire one-cell paint stroke.
// Preserve only narrow land/water cores (opposite neighbours agree against
// the centre). Ordinary coast edges/corners keep their existing smooth field.
inline bool overviewHasThinTerrainCore(const MapModel &map,int x,int y)
{
    if (!overviewUsesAtlasTerrain(map) || map.generation.recipe.generatorVersion<11 ||
        map.generation.recipe.styleId!="illustrated_atlas" ||
        !((map.generation.recipe.styleVersion==5 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v5") ||
          (map.generation.recipe.styleVersion==6 && map.generation.recipe.styleSnapshotId=="illustrated_atlas_v6")) ||
        !isMapCellInside(map,x,y)) return false;
    const auto *cell=mapCellAt(map,x,y);
    if (!cell || cell->wayKind==MapWayKind::River || cell->wayKind==MapWayKind::Canal || cell->kind==MapTileKind::Bridge) return false;
    const bool land=overviewGroundKindIsLand(overviewGroundKindForRender(map,x,y));
    const auto opposite=[&](int nx,int ny) {
        return isMapCellInside(map,nx,ny) &&
            overviewGroundKindIsLand(overviewGroundKindForRender(map,nx,ny))!=land;
    };
    return (opposite(x-1,y)&&opposite(x+1,y)) || (opposite(x,y-1)&&opposite(x,y+1));
}

inline bool overviewTerritoryPolygonContainsPoint(
    const MapGeneratedFeature &feature,
    float x,
    float y)
{
    if (!feature.closed || feature.points.size() < 3u)
        return false;
    bool inside = false;
    for (std::size_t current = 0u, previous = feature.points.size() - 1u;
         current < feature.points.size(); previous = current++)
    {
        const MapFeaturePoint &a = feature.points[current];
        const MapFeaturePoint &b = feature.points[previous];
        if ((a.y > y) == (b.y > y))
            continue;
        const float denominator = b.y - a.y;
        if (std::abs(denominator) <= 0.00001f)
            continue;
        const float intersectionX =
            (b.x - a.x) * (y - a.y) / denominator + a.x;
        if (x < intersectionX)
            inside = !inside;
    }
    return inside;
}

inline OverviewPoliticalOwnerProjection buildOverviewPoliticalOwnerProjection(
    const MapModel &map)
{
    OverviewPoliticalOwnerProjection projection;
    projection.width = map.width;
    projection.height = map.height;
    if (map.atlasLevel != MapLevel::World || map.width <= 0 || map.height <= 0)
        return projection;

    std::vector<const MapGeneratedFeature *> territories;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
            feature.closed && feature.points.size() >= 3u &&
            feature.materialId.find("territory.") != std::string::npos)
            territories.push_back(&feature);
    }
    std::sort(territories.begin(), territories.end(),
              [](const MapGeneratedFeature *first,
                 const MapGeneratedFeature *second) {
                  return first->id < second->id;
              });
    projection.territoryCount = static_cast<int>(territories.size());
    projection.owners.assign(static_cast<std::size_t>(map.width) *
                                 static_cast<std::size_t>(map.height),
                             -1);
    if (territories.size() < 2u)
        return projection;

    const std::vector<MapTileKind> ground = buildOverviewGroundProjection(map);
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            if (!overviewGroundKindIsLand(overviewGroundKindFromProjection(
                    map, ground, x, y)))
                continue;
            const float centerX = static_cast<float>(x) + 0.5f;
            const float centerY = static_cast<float>(y) + 0.5f;
            for (std::size_t territoryIndex = 0u;
                 territoryIndex < territories.size(); ++territoryIndex)
            {
                if (!overviewTerritoryPolygonContainsPoint(
                        *territories[territoryIndex], centerX, centerY))
                    continue;
                projection.owners[static_cast<std::size_t>(mapCellIndex(map, x, y))] =
                    static_cast<int>(territoryIndex);
                break;
            }
        }
    }

    // Clipped polygons can leave one-cell cracks along complex coasts or at a
    // three-way junction. Letting owner=-1 survive inside a connected landmass
    // would make an internal border stop in open terrain. Fill only through
    // cardinally connected land from the nearest known owner; an unclaimed
    // island without any territory seed deliberately remains unowned.
    std::queue<std::pair<int, int>> frontier;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (projection.owners[static_cast<std::size_t>(mapCellIndex(map, x, y))] >= 0)
                frontier.emplace(x, y);
    constexpr int cardinal[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    while (!frontier.empty())
    {
        const auto [x, y] = frontier.front();
        frontier.pop();
        const int owner = projection.owners[
            static_cast<std::size_t>(mapCellIndex(map, x, y))];
        for (const auto &offset : cardinal)
        {
            const int neighborX = x + offset[0];
            const int neighborY = y + offset[1];
            if (!isMapCellInside(map, neighborX, neighborY))
                continue;
            const std::size_t neighborIndex = static_cast<std::size_t>(
                mapCellIndex(map, neighborX, neighborY));
            if (projection.owners[neighborIndex] >= 0 ||
                !overviewGroundKindIsLand(overviewGroundKindFromProjection(
                    map, ground, neighborX, neighborY)))
                continue;
            projection.owners[neighborIndex] = owner;
            frontier.emplace(neighborX, neighborY);
        }
    }
    return projection;
}

inline bool overviewPoliticalBorderEndpointIsAnchored(
    const OverviewPoliticalOwnerProjection &projection,
    const MapFeaturePoint &point)
{
    if (projection.width <= 0 || projection.height <= 0 ||
        projection.owners.size() <
            static_cast<std::size_t>(projection.width) *
                static_cast<std::size_t>(projection.height))
        return false;
    constexpr float epsilon = 0.04f;
    if (point.x <= epsilon || point.y <= epsilon ||
        point.x >= static_cast<float>(projection.width) - epsilon ||
        point.y >= static_cast<float>(projection.height) - epsilon)
        return true;

    const int vertexX = static_cast<int>(std::lround(point.x));
    const int vertexY = static_cast<int>(std::lround(point.y));
    int distinctOwners[4] = {-2, -2, -2, -2};
    int distinctCount = 0;
    bool touchesUnowned = false;
    for (int offsetY = -1; offsetY <= 0; ++offsetY)
    {
        for (int offsetX = -1; offsetX <= 0; ++offsetX)
        {
            const int x = vertexX + offsetX;
            const int y = vertexY + offsetY;
            if (x < 0 || y < 0 || x >= projection.width || y >= projection.height)
            {
                touchesUnowned = true;
                continue;
            }
            const int owner = projection.owners[static_cast<std::size_t>(
                y * projection.width + x)];
            if (owner < 0)
            {
                touchesUnowned = true;
                continue;
            }
            bool known = false;
            for (int index = 0; index < distinctCount; ++index)
                if (distinctOwners[index] == owner)
                    known = true;
            if (!known && distinctCount < 4)
                distinctOwners[distinctCount++] = owner;
        }
    }
    // An open internal border may end only at a coastline/map edge or at a
    // true three-territory junction.  This rejects the short free "worms"
    // created by incomplete polygon traces while keeping legitimate borders.
    return touchesUnowned || distinctCount >= 3;
}

} // namespace gm::map_render
