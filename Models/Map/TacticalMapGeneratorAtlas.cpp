#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "Models/Map/TacticalMapGeneratorInternal.hpp"

namespace TacticalMapGeneratorInternal
{
void installTacticalPreservedFeatures(MapModel &map, const TacticalMapRecipe &recipe)
{
    if (!recipe.preservedFeatures.empty())
    for (auto &feature : map.generation.graph.features)
    {
        if (feature.locked || (feature.kind != MapGeneratedFeatureKind::Road &&
                               feature.kind != MapGeneratedFeatureKind::Trail) || feature.points.size() < 2u)
            continue;
        // Long diagonal template segments can span several independent atlas
        // crossings. Short exact subsegments let projection and bridge
        // validation inspect the actual crossing, rather than its distant
        // bounding-box corner. Parent controls and locks remain unchanged.
        std::vector<MapFeaturePoint> points{feature.points.front()};
        for (std::size_t index = 1; index < feature.points.size(); ++index)
        {
            const auto &start = feature.points[index - 1];
            const auto &end = feature.points[index];
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const int steps = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy) / 1.5f)));
            for (int step = 1; step <= steps; ++step)
            {
                const float amount = static_cast<float>(step) / steps;
                points.push_back({start.x + dx * amount, start.y + dy * amount});
            }
        }
        feature.points = std::move(points);
    }
    mapGenInstallPreservedFeatures(map, recipe.preservedFeatures);
    // The template's own routes already exist when parent geometry is
    // installed. Reproject every road after inherited rivers so their real
    // intersections become traversable bridge cells instead of severing both
    // tactical lanes.
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::Road ||
            feature.kind == MapGeneratedFeatureKind::Trail)
            projectGeneratedWayFeatureToCells(map, feature, false);
}

void connectTacticalPreservedRoutes(MapModel &map, const TacticalMapRecipe &recipe)
{
    int connectorIndex = 0;
    const auto isPreservedId = [&](const std::string &id) {
        return std::any_of(recipe.preservedFeatures.begin(), recipe.preservedFeatures.end(),
                           [&](const MapGeneratedFeature &feature) {
                               return feature.id == id;
                           });
    };
    for (const MapGeneratedFeature &preserved : recipe.preservedFeatures)
    {
        if ((preserved.kind != MapGeneratedFeatureKind::Road &&
             preserved.kind != MapGeneratedFeatureKind::Trail) ||
            preserved.points.empty())
            continue;
        float bestDistance = std::numeric_limits<float>::max();
        MapFeaturePoint inheritedPoint = preserved.points.front();
        MapFeaturePoint generatedPoint = inheritedPoint;
        for (const MapFeaturePoint &sourcePoint : preserved.points)
            for (const MapGeneratedFeature &generated : map.generation.graph.features)
            {
                if ((generated.kind != MapGeneratedFeatureKind::Road &&
                     generated.kind != MapGeneratedFeatureKind::Trail) ||
                    generated.points.empty() || isPreservedId(generated.id))
                    continue;
                for (const MapFeaturePoint &targetPoint : generated.points)
                {
                    const float dx = targetPoint.x - sourcePoint.x;
                    const float dy = targetPoint.y - sourcePoint.y;
                    const float distance = dx * dx + dy * dy;
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        inheritedPoint = sourcePoint;
                        generatedPoint = targetPoint;
                    }
                }
            }
        if (!std::isfinite(bestDistance) || bestDistance <= 2.25f)
            continue;
        const Point start{
            std::clamp(static_cast<int>(std::floor(inheritedPoint.x)), 0, map.width - 1),
            std::clamp(static_cast<int>(std::floor(inheritedPoint.y)), 0, map.height - 1)};
        const Point end{
            std::clamp(static_cast<int>(std::floor(generatedPoint.x)), 0, map.width - 1),
            std::clamp(static_cast<int>(std::floor(generatedPoint.y)), 0, map.height - 1)};
        const MapWayKind way = mapGeneratedFeatureWayKind(preserved);
        carveRoute(map, "atlas_connector_" + std::to_string(connectorIndex++),
                   "Inherited route connection", {start, end}, 0,
                   MapTileKind::Dirt, way, true);
    }
}

void publishTacticalInheritedCrossings(MapModel &map)
{
    int crossingIndex = 0;
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind != MapTileKind::Bridge ||
                !mapGenWayKindIsRoadLike(cell->wayKind))
                continue;
            if ((mapCellAt(map, x - 1, y) && mapCellAt(map, x - 1, y)->kind == MapTileKind::Bridge) ||
                (mapCellAt(map, x, y - 1) && mapCellAt(map, x, y - 1)->kind == MapTileKind::Bridge))
                continue;
            const float centerX = static_cast<float>(x) + 0.5f;
            const float centerY = static_cast<float>(y) + 0.5f;
            float existingBridgeX = 0.0f;
            float existingBridgeY = 0.0f;
            if (mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Bridge,
                                             centerX, centerY, 1.75f,
                                             existingBridgeX, existingBridgeY))
                continue;
            float roadX = 1.0f;
            float roadY = 0.0f;
            if (!mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Road,
                                              centerX, centerY, 2.5f, roadX, roadY))
                mapGenV2NearestFeatureTangent(map.generation.graph, MapGeneratedFeatureKind::Trail,
                                              centerX, centerY, 2.5f, roadX, roadY);
            const float length = std::max(0.001f, std::sqrt(roadX * roadX + roadY * roadY));
            roadX /= length;
            roadY /= length;
            MapGeneratedFeature bridge;
            bridge.id = map.metadata.id + "::inherited_crossing::" +
                        std::to_string(crossingIndex++);
            bridge.kind = MapGeneratedFeatureKind::Bridge;
            bridge.name = "Atlas crossing";
            bridge.materialId = cell->wayKind == MapWayKind::StoneRoad
                                    ? "fantasy_neutral.bridge.stone"
                                    : "fantasy_neutral.bridge.wood";
            bridge.width = 1.0f;
            bridge.points = {{centerX - roadX * 0.72f, centerY - roadY * 0.72f},
                             {centerX + roadX * 0.72f, centerY + roadY * 0.72f}};
            map.generation.graph.features.push_back(std::move(bridge));
        }
}

std::array<Point, 2> tacticalDeploymentPreferences(const MapModel &map,
                                                   const TacticalMapRecipe &recipe)
{
    std::vector<Point> entries;
    for (const MapGeneratedFeature &feature : recipe.preservedFeatures)
    {
        if ((feature.kind != MapGeneratedFeatureKind::Road &&
             feature.kind != MapGeneratedFeatureKind::Trail) ||
            feature.points.empty())
            continue;
        for (const MapFeaturePoint &point : {feature.points.front(), feature.points.back()})
        {
            const Point cell{
                std::clamp(static_cast<int>(std::floor(point.x)), 0, map.width - 1),
                std::clamp(static_cast<int>(std::floor(point.y)), 0, map.height - 1)};
            const bool boundary = cell.x <= 1 || cell.y <= 1 ||
                                  cell.x >= map.width - 2 || cell.y >= map.height - 2;
            if (boundary && std::find(entries.begin(), entries.end(), cell) == entries.end())
                entries.push_back(cell);
        }
    }
    std::array<Point, 2> preferred{{{2, map.height / 2},
                                     {map.width - 3, map.height / 2}}};
    if (entries.size() < 2)
        return preferred;

    int bestDistance = -1;
    for (std::size_t first = 0; first < entries.size(); ++first)
        for (std::size_t second = first + 1; second < entries.size(); ++second)
        {
            const int dx = entries[first].x - entries[second].x;
            const int dy = entries[first].y - entries[second].y;
            const int distance = dx * dx + dy * dy;
            if (distance > bestDistance)
            {
                preferred = {entries[first], entries[second]};
                bestDistance = distance;
            }
        }
    if (preferred[0].x > preferred[1].x ||
        (preferred[0].x == preferred[1].x && preferred[0].y > preferred[1].y))
        std::swap(preferred[0], preferred[1]);
    return preferred;
}

bool tacticalPreservedFeatureMatches(const MapModel &map,
                                     const MapGeneratedFeature &expected)
{
    constexpr float epsilon = 0.001f;
    const auto actual = std::find_if(map.generation.graph.features.begin(),
                                     map.generation.graph.features.end(),
                                     [&](const MapGeneratedFeature &feature) {
                                         return feature.id == expected.id;
                                     });
    if (actual == map.generation.graph.features.end() || actual->kind != expected.kind ||
        actual->name != expected.name || actual->materialId != expected.materialId ||
        std::abs(actual->width - expected.width) > epsilon ||
        actual->closed != expected.closed || actual->locked != expected.locked ||
        actual->discovered != expected.discovered || actual->gmOnly != expected.gmOnly ||
        actual->lineage.sourceMapId != expected.lineage.sourceMapId ||
        actual->lineage.sourceFeatureId != expected.lineage.sourceFeatureId ||
        actual->lineage.sourceRevision != expected.lineage.sourceRevision ||
        actual->points.size() != expected.points.size())
        return false;
    for (std::size_t index = 0; index < expected.points.size(); ++index)
        if (std::abs(actual->points[index].x - expected.points[index].x) > epsilon ||
            std::abs(actual->points[index].y - expected.points[index].y) > epsilon)
            return false;
    return true;
}
}
