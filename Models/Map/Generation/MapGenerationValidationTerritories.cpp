#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
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

int mapGenV5WorldTerritoryStraightRayPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 0;

    const float minimumSpan = std::max(5.0f, static_cast<float>(std::min(map.width, map.height)) * 0.16f);
    const float maximumWalk = std::max(minimumSpan * 1.5f, static_cast<float>(std::min(map.width, map.height)) * 0.72f);
    std::vector<const MapGeneratedFeature *> territoryFeatures;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::TerrainRegion && feature.closed && feature.points.size() >= 3 &&
            feature.materialId.rfind("fantasy_neutral.territory.", 0) == 0)
            territoryFeatures.push_back(&feature);
    const auto pointSegmentDistanceSquared = [](const MapFeaturePoint &point, const MapFeaturePoint &first,
                                                const MapFeaturePoint &second) {
        const float segmentX = second.x - first.x;
        const float segmentY = second.y - first.y;
        const float lengthSquared = segmentX * segmentX + segmentY * segmentY;
        const float amount =
            lengthSquared <= 0.0001f
                ? 0.0f
                : std::clamp(((point.x - first.x) * segmentX + (point.y - first.y) * segmentY) / lengthSquared, 0.0f,
                             1.0f);
        const float deltaX = point.x - (first.x + segmentX * amount);
        const float deltaY = point.y - (first.y + segmentY * amount);
        return deltaX * deltaX + deltaY * deltaY;
    };
    float totalBoundaryLength = 0.0f;
    float longStraightLength = 0.0f;
    for (const MapGeneratedFeature *featurePointer : territoryFeatures)
    {
        const MapGeneratedFeature &feature = *featurePointer;
        const std::size_t count = feature.points.size();
        std::vector<float> edgeLengths(count, 0.0f);
        std::vector<unsigned char> internalEdge(count, 0);
        std::vector<unsigned char> naturalBarrierEdge(count, 0);
        std::vector<unsigned char> straightEdge(count, 0);
        for (std::size_t index = 0; index < count; ++index)
        {
            const MapFeaturePoint &first = feature.points[index];
            const MapFeaturePoint &second = feature.points[(index + 1) % count];
            const float deltaX = second.x - first.x;
            const float deltaY = second.y - first.y;
            edgeLengths[index] = std::sqrt(deltaX * deltaX + deltaY * deltaY);
            const MapFeaturePoint middle{(first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f};
            for (const MapGeneratedFeature *other : territoryFeatures)
            {
                if (other == featurePointer)
                    continue;
                for (std::size_t otherIndex = 0; otherIndex < other->points.size(); ++otherIndex)
                {
                    if (pointSegmentDistanceSquared(middle, other->points[otherIndex],
                                                    other->points[(otherIndex + 1) % other->points.size()]) <= 0.04f)
                    {
                        internalEdge[index] = 1;
                        break;
                    }
                }
                if (internalEdge[index])
                    break;
            }
            if (internalEdge[index])
            {
                totalBoundaryLength += edgeLengths[index];
                const int middleX = static_cast<int>(std::floor(middle.x));
                const int middleY = static_cast<int>(std::floor(middle.y));
                for (int offsetY = -1; !naturalBarrierEdge[index] && offsetY <= 1; ++offsetY)
                    for (int offsetX = -1; offsetX <= 1; ++offsetX)
                    {
                        const MapCellModel *cell = mapCellAt(map, middleX + offsetX, middleY + offsetY);
                        naturalBarrierEdge[index] =
                            cell && (cell->kind == MapTileKind::Mountain || cell->wayKind == MapWayKind::River ||
                                     cell->wayKind == MapWayKind::Canal);
                        if (naturalBarrierEdge[index])
                            break;
                    }
            }
        }

        // Only edges shared by two territories are political borders. Outer
        // coastline may legitimately contain a long beach or cliff and must
        // not make the administrative-naturalism gate fail. Grid-traced
        // Euclidean Voronoi rays alternate one-cell horizontal and vertical
        // steps, so inspect whole shared windows rather than individual edges.
        for (std::size_t start = 0; start < count; ++start)
        {
            if (!internalEdge[start] || naturalBarrierEdge[start])
                continue;
            float walked = 0.0f;
            for (std::size_t span = 1; span < count; ++span)
            {
                const std::size_t edgeIndex = (start + span - 1) % count;
                if (!internalEdge[edgeIndex] || naturalBarrierEdge[edgeIndex])
                    break;
                walked += edgeLengths[edgeIndex];
                const bool stopAfterWindow = walked > maximumWalk;
                const std::size_t end = (start + span) % count;
                const MapFeaturePoint &first = feature.points[start];
                const MapFeaturePoint &last = feature.points[end];
                const float chordX = last.x - first.x;
                const float chordY = last.y - first.y;
                const float chordLength = std::sqrt(chordX * chordX + chordY * chordY);
                if (chordLength < minimumSpan)
                {
                    if (stopAfterWindow)
                        break;
                    continue;
                }
                const float allowedDeviation = std::max(0.82f, chordLength * 0.035f);
                float maximumDeviation = 0.0f;
                for (std::size_t middle = 1; middle < span; ++middle)
                {
                    const MapFeaturePoint &point = feature.points[(start + middle) % count];
                    const float cross = std::abs(chordX * (first.y - point.y) - (first.x - point.x) * chordY);
                    maximumDeviation = std::max(maximumDeviation, cross / std::max(0.001f, chordLength));
                }
                if (maximumDeviation > allowedDeviation)
                {
                    if (stopAfterWindow)
                        break;
                    continue;
                }
                for (std::size_t marked = 0; marked < span; ++marked)
                    straightEdge[(start + marked) % count] = 1;
                if (stopAfterWindow)
                    break;
            }
        }
        for (std::size_t index = 0; index < count; ++index)
            if (internalEdge[index] && !naturalBarrierEdge[index] && straightEdge[index])
                longStraightLength += edgeLengths[index];
    }
    if (territoryFeatures.size() < 2)
        return 0;
    if (totalBoundaryLength <= 0.001f)
        return map.generation.recipe.generatorVersion >= 11 ? 0 : 100;
    const float straightShare = longStraightLength / totalBoundaryLength;
    return std::clamp(static_cast<int>(std::lround(straightShare * 100.0f)), 0, 100);
}

int mapGenV5WorldTerritoryNaturalismPercent(const MapModel &map)
{
    const int straightRayPercent = mapGenV5WorldTerritoryStraightRayPercent(map);
    if (straightRayPercent >= 48)
        return 48;
    if (straightRayPercent >= 38)
        return 68;
    return 100;
}

int mapGenV5WorldTerritoryLandAdjacencyPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 100;
    int samples = 0;
    int landAdjacent = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::TerrainRegion || !feature.closed ||
            feature.materialId.rfind("fantasy_neutral.territory.", 0) != 0)
            continue;
        for (std::size_t pointIndex = 0; pointIndex < feature.points.size(); ++pointIndex)
        {
            const MapFeaturePoint &start = feature.points[pointIndex];
            const MapFeaturePoint &end = feature.points[(pointIndex + 1) % feature.points.size()];
            const float dx = end.x - start.x;
            const float dy = end.y - start.y;
            const int steps = std::max(1, static_cast<int>(std::ceil(std::sqrt(dx * dx + dy * dy))));
            for (int step = 0; step < steps; ++step)
            {
                const float amount = static_cast<float>(step) / static_cast<float>(steps);
                const int x = static_cast<int>(std::floor(start.x + dx * amount));
                const int y = static_cast<int>(std::floor(start.y + dy * amount));
                bool nearLand = false;
                for (int offsetY = -1; !nearLand && offsetY <= 1; ++offsetY)
                    for (int offsetX = -1; offsetX <= 1; ++offsetX)
                    {
                        const MapCellModel *cell = mapCellAt(map, x + offsetX, y + offsetY);
                        nearLand = nearLand || (cell && cell->kind != MapTileKind::Water);
                    }
                ++samples;
                landAdjacent += nearLand ? 1 : 0;
            }
        }
    }
    return samples == 0 ? 100 : landAdjacent * 100 / samples;
}

int mapGenV5WorldTerritoryGeographyPercent(const MapModel &map)
{
    // V11 separates usable territorial geography from its aesthetic review.
    // Preserve the naturalism diagnostic without letting it silently fail the
    // aggregate composition floor after being reported as a warning.
    if (map.generation.recipe.generatorVersion >= 11)
        return mapGenV5WorldTerritoryLandAdjacencyPercent(map);
    return std::min(mapGenV5WorldTerritoryLandAdjacencyPercent(map), mapGenV5WorldTerritoryNaturalismPercent(map));
}

int mapGenV5WorldSettlementSpreadPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 100;
    std::vector<MapFeaturePoint> points;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::Settlement && !feature.points.empty())
            points.push_back(feature.points.front());
    if (points.size() < 3)
        return 100;

    std::size_t first = 0;
    std::size_t second = 1;
    float longestSquared = 0.0f;
    for (std::size_t left = 0; left < points.size(); ++left)
        for (std::size_t right = left + 1; right < points.size(); ++right)
        {
            const float dx = points[right].x - points[left].x;
            const float dy = points[right].y - points[left].y;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared > longestSquared)
            {
                longestSquared = distanceSquared;
                first = left;
                second = right;
            }
        }
    if (longestSquared <= 0.001f)
        return 0;
    const float length = std::sqrt(longestSquared);
    float maximumPerpendicularDistance = 0.0f;
    for (const MapFeaturePoint &point : points)
    {
        const float cross = std::abs((points[second].x - points[first].x) * (points[first].y - point.y) -
                                     (points[first].x - point.x) * (points[second].y - points[first].y));
        maximumPerpendicularDistance = std::max(maximumPerpendicularDistance, cross / length);
    }
    const float desiredSpread = std::max(2.0f, static_cast<float>(std::min(map.width, map.height)) * 0.10f);
    return std::clamp(static_cast<int>(std::lround(maximumPerpendicularDistance / desiredSpread * 100.0f)), 0, 100);
}

