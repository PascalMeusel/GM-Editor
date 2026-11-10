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

int mapGenV5WorldRiverCompositionPercent(const MapModel &map)
{
    if (map.generation.recipe.generatorVersion < 5 || map.atlasLevel != MapLevel::World)
        return 100;
    std::vector<const MapGeneratedFeature *> rivers;
    const MapGeneratedFeature *mainRiver = nullptr;
    int prominentLockedStems = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::River)
            continue;
        rivers.push_back(&feature);
        if (feature.locked && feature.width >= 1.35f)
        {
            mainRiver = &feature;
            ++prominentLockedStems;
        }
    }
    if (rivers.empty())
        return 100;
    if (!mapGenV4RequestsSharedGreatRiver(map))
        return 100;
    if (prominentLockedStems != 1 || !mainRiver || rivers.size() > 3)
        return 0;
    if (mainRiver->materialId.find("river") == std::string::npos || mainRiver->width < 1.35f)
        return 0;

    const auto beginsInVisibleHighland = [&](const MapGeneratedFeature &feature) {
        if (feature.points.empty())
            return false;
        const int sourceX = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, map.width - 1);
        const int sourceY = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, map.height - 1);
        int mountainCells = 0;
        int highlandCells = 0;
        for (int offsetY = -4; offsetY <= 4; ++offsetY)
            for (int offsetX = -4; offsetX <= 4; ++offsetX)
            {
                const MapCellModel *nearby = mapCellAt(map, sourceX + offsetX, sourceY + offsetY);
                if (!nearby)
                    continue;
                mountainCells += nearby->kind == MapTileKind::Mountain ? 1 : 0;
                highlandCells += nearby->kind == MapTileKind::Mountain || nearby->kind == MapTileKind::Hill ? 1 : 0;
            }
        // A single noisy hill pixel is not a legible source on an overview.
        // Accept a short ridge or a broader foothill context instead.
        return mountainCells >= 2 || highlandCells >= 4;
    };
    if (!beginsInVisibleHighland(*mainRiver))
        return 65;

    for (const MapGeneratedFeature *tributary : rivers)
    {
        if (tributary == mainRiver)
            continue;
        if (tributary->points.size() < 4)
            return 0;
        float tributaryLength = 0.0f;
        for (std::size_t index = 1; index < tributary->points.size(); ++index)
        {
            const float deltaX = tributary->points[index].x - tributary->points[index - 1].x;
            const float deltaY = tributary->points[index].y - tributary->points[index - 1].y;
            tributaryLength += std::sqrt(deltaX * deltaX + deltaY * deltaY);
        }
        if (tributaryLength < static_cast<float>(std::max(7, std::min(map.width, map.height) / 6)))
            return 0;
        const MapFeaturePoint &tributarySource = tributary->points.front();
        const MapFeaturePoint &join = tributary->points.back();
        const float chordX = join.x - tributarySource.x;
        const float chordY = join.y - tributarySource.y;
        const float chordLength = std::sqrt(chordX * chordX + chordY * chordY);
        float maximumDeviation = 0.0f;
        if (chordLength > 0.001f)
            for (std::size_t pointIndex = 1; pointIndex + 1 < tributary->points.size(); ++pointIndex)
            {
                const float relativeX = tributary->points[pointIndex].x - tributarySource.x;
                const float relativeY = tributary->points[pointIndex].y - tributarySource.y;
                maximumDeviation =
                    std::max(maximumDeviation, std::abs(chordX * relativeY - chordY * relativeX) / chordLength);
            }
        if (maximumDeviation < std::clamp(chordLength * 0.075f, 1.15f, 2.40f))
            return 55;
        float nearestSquared = std::numeric_limits<float>::max();
        float mainTangentX = 0.0f;
        float mainTangentY = 0.0f;
        for (std::size_t index = 1; index < mainRiver->points.size(); ++index)
        {
            const MapFeaturePoint &start = mainRiver->points[index - 1];
            const MapFeaturePoint &end = mainRiver->points[index];
            const float segmentX = end.x - start.x;
            const float segmentY = end.y - start.y;
            const float lengthSquared = segmentX * segmentX + segmentY * segmentY;
            if (lengthSquared <= 0.0001f)
                continue;
            const float amount =
                std::clamp(((join.x - start.x) * segmentX + (join.y - start.y) * segmentY) / lengthSquared, 0.0f, 1.0f);
            const float deltaX = join.x - (start.x + segmentX * amount);
            const float deltaY = join.y - (start.y + segmentY * amount);
            const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
            if (distanceSquared >= nearestSquared)
                continue;
            nearestSquared = distanceSquared;
            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
            mainTangentX = segmentX * inverseLength;
            mainTangentY = segmentY * inverseLength;
        }
        if (nearestSquared > 2.25f)
            return 0;

        const std::size_t incomingIndex = tributary->points.size() > 4 ? tributary->points.size() - 4 : 0;
        const float incomingX = join.x - tributary->points[incomingIndex].x;
        const float incomingY = join.y - tributary->points[incomingIndex].y;
        const float incomingLength = std::sqrt(incomingX * incomingX + incomingY * incomingY);
        const float confluenceAlignment =
            incomingLength <= 0.001f ? -1.0f : (incomingX * mainTangentX + incomingY * mainTangentY) / incomingLength;
        if (confluenceAlignment < 0.24f || confluenceAlignment > 0.93f)
            return 45;

        if (!beginsInVisibleHighland(*tributary))
            return 65;
        if (tributary->materialId.find("creek") == std::string::npos || tributary->width >= mainRiver->width * 0.72f)
            return 45;

        // A tributary may contact the main stem only at its confluence. Early
        // contact followed by a second crossing creates the three-rail shape
        // seen in the failed previews.
        int parallelSamples = 0;
        int comparableSamples = 0;
        for (std::size_t pointIndex = 0; pointIndex + 3 < tributary->points.size(); ++pointIndex)
        {
            for (const MapFeaturePoint &mainPoint : mainRiver->points)
            {
                const float deltaX = tributary->points[pointIndex].x - mainPoint.x;
                const float deltaY = tributary->points[pointIndex].y - mainPoint.y;
                if (deltaX * deltaX + deltaY * deltaY <= 1.44f)
                    return 0;
            }
            if (pointIndex == 0 || pointIndex + 1 >= tributary->points.size())
                continue;
            const float tributaryX = tributary->points[pointIndex + 1].x - tributary->points[pointIndex - 1].x;
            const float tributaryY = tributary->points[pointIndex + 1].y - tributary->points[pointIndex - 1].y;
            const float tributaryTangentLength = std::sqrt(tributaryX * tributaryX + tributaryY * tributaryY);
            if (tributaryTangentLength <= 0.001f)
                continue;
            float closestSquared = std::numeric_limits<float>::max();
            float closestTangentX = 0.0f;
            float closestTangentY = 0.0f;
            for (std::size_t mainIndex = 1; mainIndex < mainRiver->points.size(); ++mainIndex)
            {
                const MapFeaturePoint &start = mainRiver->points[mainIndex - 1];
                const MapFeaturePoint &end = mainRiver->points[mainIndex];
                const float segmentX = end.x - start.x;
                const float segmentY = end.y - start.y;
                const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;
                if (segmentLengthSquared <= 0.001f)
                    continue;
                const float amount = std::clamp(((tributary->points[pointIndex].x - start.x) * segmentX +
                                                 (tributary->points[pointIndex].y - start.y) * segmentY) /
                                                    segmentLengthSquared,
                                                0.0f, 1.0f);
                const float deltaX = tributary->points[pointIndex].x - (start.x + segmentX * amount);
                const float deltaY = tributary->points[pointIndex].y - (start.y + segmentY * amount);
                const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
                if (distanceSquared >= closestSquared)
                    continue;
                closestSquared = distanceSquared;
                const float inverseLength = 1.0f / std::sqrt(segmentLengthSquared);
                closestTangentX = segmentX * inverseLength;
                closestTangentY = segmentY * inverseLength;
            }
            if (closestSquared <= 12.25f)
            {
                ++comparableSamples;
                const float alignment =
                    std::abs((tributaryX * closestTangentX + tributaryY * closestTangentY) / tributaryTangentLength);
                parallelSamples += alignment > 0.80f ? 1 : 0;
            }
        }
        if (parallelSamples >= 2 && parallelSamples * 4 >= std::max(1, comparableSamples))
            return 35;
    }
    return 100;
}

