#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
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


bool mapGenV2NearestFeatureTangent(const MapFeatureGraph &graph, MapGeneratedFeatureKind kind, float x, float y,
                                   float maximumDistance, float &tangentX, float &tangentY)
{
    float bestDistanceSquared = maximumDistance * maximumDistance;
    bool found = false;
    for (const MapGeneratedFeature &feature : graph.features)
    {
        if (feature.kind != kind || feature.points.size() < 2)
            continue;
        for (std::size_t pointIndex = 1; pointIndex < feature.points.size(); ++pointIndex)
        {
            const MapFeaturePoint &start = feature.points[pointIndex - 1];
            const MapFeaturePoint &end = feature.points[pointIndex];
            const float segmentX = end.x - start.x;
            const float segmentY = end.y - start.y;
            const float lengthSquared = segmentX * segmentX + segmentY * segmentY;
            if (lengthSquared <= 0.0001f)
                continue;
            const float projection =
                std::clamp(((x - start.x) * segmentX + (y - start.y) * segmentY) / lengthSquared, 0.0f, 1.0f);
            const float nearestX = start.x + segmentX * projection;
            const float nearestY = start.y + segmentY * projection;
            const float deltaX = x - nearestX;
            const float deltaY = y - nearestY;
            const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
            if (distanceSquared >= bestDistanceSquared)
                continue;
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            bestDistanceSquared = distanceSquared;
            tangentX = segmentX * inverseLength;
            tangentY = segmentY * inverseLength;
            found = true;
        }
    }
    return found;
}
