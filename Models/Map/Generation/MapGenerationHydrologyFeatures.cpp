#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"

void mapGenV2AddPathFeature(MapModel &map, MapGeneratedFeatureKind kind, const std::string &id, const std::string &name,
                            const std::string &material, float width, const std::vector<MapCellCoordinate> &cells)
{
    if (cells.empty())
        return;
    MapGeneratedFeature feature;
    feature.id = id;
    feature.kind = kind;
    feature.name = name;
    feature.materialId = material;
    feature.width = width;
    feature.points.reserve(cells.size());
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        feature.points.push_back({static_cast<float>(cells[i].x) + 0.5f, static_cast<float>(cells[i].y) + 0.5f});
    }
    map.generation.graph.features.push_back(std::move(feature));
}

bool mapGenV6RegionJoinFirstRiverConfluence(const MapModel &map, std::vector<MapCellCoordinate> &path,
                                            MapFeaturePoint &confluence)
{
    if (map.generation.recipe.generatorVersion < 6 ||
        (map.atlasLevel != MapLevel::Region &&
         !(map.generation.recipe.generatorVersion >= 8 && map.atlasLevel == MapLevel::World)))
        return false;
    for (std::size_t index = 1; index < path.size(); ++index)
    {
        const MapFeaturePoint a{path[index - 1].x + .5f, path[index - 1].y + .5f};
        const MapFeaturePoint b{path[index].x + .5f, path[index].y + .5f};
        float firstAmount = 2.f;
        MapFeaturePoint firstJoin;
        for (const auto &river : map.generation.graph.features)
        {
            if (river.kind != MapGeneratedFeatureKind::River)
                continue;
            for (std::size_t segment = 1; segment < river.points.size(); ++segment)
            {
                const auto c = river.points[segment - 1], d = river.points[segment];
                if (gm::map_geometry::roadSegmentDistanceSquared(a, b, c, d) > .000001f)
                    continue;
                const float rx = b.x - a.x, ry = b.y - a.y;
                const float sx = d.x - c.x, sy = d.y - c.y;
                const float denominator = rx * sy - ry * sx;
                const float amount = std::abs(denominator) > .000001f
                                         ? std::clamp(((c.x - a.x) * sy - (c.y - a.y) * sx) / denominator, 0.f, 1.f)
                                         : std::clamp(std::min((c.x - a.x) * rx + (c.y - a.y) * ry,
                                                               (d.x - a.x) * rx + (d.y - a.y) * ry) /
                                                          std::max(.000001f, rx * rx + ry * ry),
                                                      0.f, 1.f);
                if (amount >= firstAmount)
                    continue;
                firstAmount = amount;
                firstJoin = {a.x + rx * amount, a.y + ry * amount};
                // Generated rivers use adjacent cell centres. Join one of the
                // existing centres before an X-shaped diagonal intersection,
                // preserving one semantic confluence and its exact elevation.
                // A long inherited segment instead keeps the exact contact.
                const float cDistance = (c.x - a.x) * (c.x - a.x) + (c.y - a.y) * (c.y - a.y);
                const float dDistance = (d.x - a.x) * (d.x - a.x) + (d.y - a.y) * (d.y - a.y);
                if (std::min(cDistance, dDistance) <= 2.0001f)
                    firstJoin = dDistance <= cDistance ? d : c;
            }
        }
        if (firstAmount > 1.f)
            continue;
        const MapCellCoordinate join{static_cast<int>(std::floor(firstJoin.x)),
                                     static_cast<int>(std::floor(firstJoin.y))};
        if (!isMapCellInside(map, join.x, join.y))
            continue;
        path.resize(index);
        if (path.empty() || !(path.back() == join))
            path.push_back(join);
        confluence = firstJoin;
        return true;
    }
    return false;
}

void mapGenV5SmoothWorldPathFeature(MapGeneratedFeature &feature, int passes)
{
    if (feature.points.size() < 4)
        return;
    // The raster path is the gameplay projection and must stay on traversable
    // cells. The overview feature is its continuous cartographic geometry: a
    // short low-pass removes the 45/90-degree staircase without changing
    // either endpoint or moving the curve by more than a fraction of a cell.
    std::vector<MapFeaturePoint> smoothed = feature.points;
    for (int pass = 0; pass < passes; ++pass)
    {
        std::vector<MapFeaturePoint> next = smoothed;
        for (std::size_t index = 1; index + 1 < smoothed.size(); ++index)
        {
            next[index].x = smoothed[index - 1].x * 0.22f + smoothed[index].x * 0.56f + smoothed[index + 1].x * 0.22f;
            next[index].y = smoothed[index - 1].y * 0.22f + smoothed[index].y * 0.56f + smoothed[index + 1].y * 0.22f;
        }
        smoothed.swap(next);
    }

    feature.points = std::move(smoothed);
}

