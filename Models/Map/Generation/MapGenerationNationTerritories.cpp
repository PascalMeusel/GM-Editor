#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
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

void mapGenV2BuildNationTerritories(MapModel &map, const MapGenerationOptions &options)
{
    if (options.nationNames.empty())
        return;

    std::vector<MapFeaturePoint> centers;
    centers.reserve(options.nationNames.size());
    constexpr float pi = 3.14159265358979323846f;
    const MapGeneratedFeature *sharedRiver = nullptr;
    if (mapGenV4RequestsSharedGreatRiver(options))
    {
        const auto found = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                        [](const MapGeneratedFeature &feature) {
                                            return feature.kind == MapGeneratedFeatureKind::River && feature.locked &&
                                                   feature.points.size() >= 2;
                                        });
        if (found != map.generation.graph.features.end())
            sharedRiver = &*found;
    }
    if (sharedRiver)
    {
        // The river defines access, not a one-cell-wide political corridor.
        // Place national centers well into alternating hinterlands so their
        // territories occupy readable parts of the landmass while each still
        // meets the common main stem near its longitudinal section.
        const float bankOffset = std::max(3.0f, static_cast<float>(std::min(map.width, map.height)) * 0.15f);
        for (std::size_t index = 0; index < options.nationNames.size(); ++index)
        {
            const float fraction = (static_cast<float>(index) + 0.5f) / static_cast<float>(options.nationNames.size());
            const std::size_t pointIndex = std::min(
                sharedRiver->points.size() - 1,
                static_cast<std::size_t>(std::lround(fraction * static_cast<float>(sharedRiver->points.size() - 1))));
            const std::size_t beforeIndex = pointIndex > 0 ? pointIndex - 1 : pointIndex;
            const std::size_t afterIndex = std::min(sharedRiver->points.size() - 1, pointIndex + 1);
            const MapFeaturePoint &point = sharedRiver->points[pointIndex];
            const float tangentX = sharedRiver->points[afterIndex].x - sharedRiver->points[beforeIndex].x;
            const float tangentY = sharedRiver->points[afterIndex].y - sharedRiver->points[beforeIndex].y;
            const float tangentLength = std::max(0.001f, std::sqrt(tangentX * tangentX + tangentY * tangentY));
            const float side = index % 2 == 0 ? -1.0f : 1.0f;
            centers.push_back({std::clamp(point.x - tangentY / tangentLength * bankOffset * side, 1.5f,
                                          static_cast<float>(map.width) - 1.5f),
                               std::clamp(point.y + tangentX / tangentLength * bankOffset * side, 1.5f,
                                          static_cast<float>(map.height) - 1.5f)});
        }
    }
    else
    {
        const float phase = mapGenV2Hash01(options.seed + 701u, 2, 3) * pi * 2.0f;
        for (std::size_t index = 0; index < options.nationNames.size(); ++index)
        {
            const float angle =
                phase + static_cast<float>(index) / static_cast<float>(options.nationNames.size()) * pi * 2.0f;
            const float radialVariation =
                0.78f + mapGenV2Hash01(options.seed + 719u, static_cast<int>(index), 5) * 0.22f;
            centers.push_back({map.width * (0.5f + std::cos(angle) * 0.27f * radialVariation),
                               map.height * (0.5f + std::sin(angle) * 0.25f * radialVariation)});
        }
    }
    mapGenV5ApplyNationRelations(centers, options, map.width, map.height);
    if (options.generatorVersion >= 5 &&
        (map.atlasLevel == MapLevel::World || (options.generatorVersion >= 11 && map.atlasLevel == MapLevel::Region &&
                                               options.landform != MapLandform::Automatic)))
        for (MapFeaturePoint &center : centers)
            center = mapGenV5SnapNationCenterToLand(map, center);

    const auto clipToCloserHalfPlane = [](const std::vector<MapFeaturePoint> &polygon, const MapFeaturePoint &own,
                                          const MapFeaturePoint &other) {
        std::vector<MapFeaturePoint> clipped;
        if (polygon.empty())
            return clipped;
        const float normalX = other.x - own.x;
        const float normalY = other.y - own.y;
        const float limit = (other.x * other.x + other.y * other.y - own.x * own.x - own.y * own.y) * 0.5f;
        const auto distance = [&](const MapFeaturePoint &point) {
            return point.x * normalX + point.y * normalY - limit;
        };
        for (std::size_t index = 0; index < polygon.size(); ++index)
        {
            const MapFeaturePoint current = polygon[index];
            const MapFeaturePoint next = polygon[(index + 1) % polygon.size()];
            const float currentDistance = distance(current);
            const float nextDistance = distance(next);
            const bool currentInside = currentDistance <= 0.0f;
            const bool nextInside = nextDistance <= 0.0f;
            if (currentInside)
                clipped.push_back(current);
            if (currentInside != nextInside)
            {
                const float denominator = currentDistance - nextDistance;
                const float amount = std::abs(denominator) < 0.00001f ? 0.0f : currentDistance / denominator;
                clipped.push_back(
                    {current.x + (next.x - current.x) * amount, current.y + (next.y - current.y) * amount});
            }
        }
        return clipped;
    };

    for (std::size_t nationIndex = 0; nationIndex < options.nationNames.size(); ++nationIndex)
    {
        std::vector<MapFeaturePoint> polygon = {
            {0.35f, 0.35f},
            {static_cast<float>(map.width) - 0.35f, 0.35f},
            {static_cast<float>(map.width) - 0.35f, static_cast<float>(map.height) - 0.35f},
            {0.35f, static_cast<float>(map.height) - 0.35f}};
        for (std::size_t otherIndex = 0; otherIndex < centers.size() && polygon.size() >= 3; ++otherIndex)
        {
            if (otherIndex != nationIndex)
                polygon = clipToCloserHalfPlane(polygon, centers[nationIndex], centers[otherIndex]);
        }
        if (polygon.size() < 3)
            continue;
        if (options.generatorVersion >= 5 && (map.atlasLevel == MapLevel::World ||
                                              (options.generatorVersion >= 11 && map.atlasLevel == MapLevel::Region &&
                                               options.landform != MapLandform::Automatic)))
        {
            std::vector<MapFeaturePoint> landBoundary = mapGenV5TraceNationLandBoundary(map, centers, nationIndex);
            // Never fall back to the rectangular Euclidean half-plane at
            // World scale. A failed land expansion is an invalid candidate and
            // must be retried, not rendered as a nation crossing open ocean.
            if (landBoundary.size() < 3)
                continue;
            polygon = std::move(landBoundary);
        }
        MapGeneratedFeature territory;
        territory.id = mapGenV2FeatureId(options.seed, "territory", static_cast<int>(nationIndex));
        territory.kind = MapGeneratedFeatureKind::TerrainRegion;
        territory.name = options.nationNames[nationIndex];
        territory.materialId = "fantasy_neutral.territory." + std::to_string(nationIndex % 6);
        territory.closed = true;
        territory.locked = true;
        territory.points = std::move(polygon);
        map.generation.graph.features.push_back(std::move(territory));
    }
}

bool mapGenV5PointInsideTerritory(const MapFeaturePoint &point, const MapGeneratedFeature &territory)
{
    if (!territory.closed || territory.points.size() < 3)
        return false;
    bool inside = false;
    for (std::size_t index = 0, previous = territory.points.size() - 1; index < territory.points.size();
         previous = index++)
    {
        const MapFeaturePoint &a = territory.points[index];
        const MapFeaturePoint &b = territory.points[previous];
        if ((a.y > point.y) == (b.y > point.y))
            continue;
        const float intersectionX = (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
        if (point.x < intersectionX)
            inside = !inside;
    }
    return inside;
}

