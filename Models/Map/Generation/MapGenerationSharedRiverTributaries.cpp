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
#include "Models/Map/Generation/MapGenerationSharedRiverBuilder.hpp"

void MapGenerationSharedRiverBuilder::addTributaries()
{
    // A major continental river normally gathers tributaries. The old shared
    // river special case returned immediately after one isolated channel,
    // leaving the whole drainage basin visually and geographically empty.
    // Add one or two deterministic highland tributaries that terminate at the
    // established main stem. They remain secondary, unlocked features; the
    // required shared-river fact continues to refer to the single locked stem.
    const bool complexHighlandBasin = (options.landscape == MapLandscapeProfile::Highlands ||
                                       options.landscape == MapLandscapeProfile::MountainPass) &&
                                      options.mountainDensity >= 58 && std::min(map.width, map.height) >= 56;
    const int tributaryTarget = path.size() >= 16 ? (complexHighlandBasin ? 2 : 1) : 0;
    std::vector<MapCellCoordinate> tributarySources;
    for (int tributaryIndex = 0; tributaryIndex < tributaryTarget; ++tributaryIndex)
    {
        const float targetFraction = tributaryIndex == 0 ? 0.34f : 0.66f;
        const std::size_t targetIndex = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::lround(targetFraction * static_cast<float>(path.size() - 1))), 2,
            path.size() - 3);
        const MapCellCoordinate target = path[targetIndex];
        const MapCellCoordinate beforeTarget = path[targetIndex - 2];
        const MapCellCoordinate afterTarget = path[targetIndex + 2];
        const float downstreamX = static_cast<float>(afterTarget.x - beforeTarget.x);
        const float downstreamY = static_cast<float>(afterTarget.y - beforeTarget.y);
        const float downstreamLength =
            std::max(0.001f, std::sqrt(downstreamX * downstreamX + downstreamY * downstreamY));
        MapCellCoordinate tributarySource{-1, -1};
        float bestTributaryScore = -std::numeric_limits<float>::max();
        for (int y = 1; y < map.height - 1; ++y)
        {
            for (int x = 1; x < map.width - 1; ++x)
            {
                const int cellIndex = mapCellIndex(map, x, y);
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || cell->kind == MapTileKind::Water || cellIndex < 0 ||
                    coastDistanceField[static_cast<std::size_t>(cellIndex)] < sourceCoastClearance)
                    continue;
                int nearestMain = map.width + map.height;
                for (const MapCellCoordinate mainPoint : path)
                    nearestMain = std::min(nearestMain, std::abs(mainPoint.x - x) + std::abs(mainPoint.y - y));
                if (nearestMain < std::max(4, std::min(map.width, map.height) / 9))
                    continue;
                int nearestSource = map.width + map.height;
                for (const MapCellCoordinate priorSource : tributarySources)
                    nearestSource = std::min(nearestSource, std::abs(priorSource.x - x) + std::abs(priorSource.y - y));
                if (!tributarySources.empty() && nearestSource < std::min(map.width, map.height) / 3)
                    continue;
                const float sampledElevation = elevation[static_cast<std::size_t>(cellIndex)];
                const float targetDistance =
                    std::sqrt(static_cast<float>((target.x - x) * (target.x - x) + (target.y - y) * (target.y - y)));
                const float incomingX = static_cast<float>(target.x - x);
                const float incomingY = static_cast<float>(target.y - y);
                const float incomingLength = std::max(0.001f, std::sqrt(incomingX * incomingX + incomingY * incomingY));
                const float confluenceAlignment =
                    (incomingX * downstreamX + incomingY * downstreamY) / (incomingLength * downstreamLength);
                // Tributaries should merge in the downstream direction. A
                // perpendicular or opposing approach reads as a rail crossing
                // even if the raster cells technically touch.
                if (confluenceAlignment < 0.24f || confluenceAlignment > 0.93f)
                    continue;
                const bool sourceHighland = cell->kind == MapTileKind::Mountain || cell->kind == MapTileKind::Hill;
                if (!sourceHighland)
                    continue;
                const float score =
                    sampledElevation * 2.0f + static_cast<float>(nearestMain) * 0.055f - targetDistance * 0.012f +
                    (1.0f - std::abs(confluenceAlignment - 0.64f)) * 1.05f + 0.88f +
                    mapGenV2Hash01(options.seed + 3541u + static_cast<std::uint32_t>(tributaryIndex * 71), x, y) *
                        0.12f;
                if (score > bestTributaryScore)
                {
                    bestTributaryScore = score;
                    tributarySource = {x, y};
                }
            }
        }
        if (tributarySource.x < 0)
            continue;
        std::vector<MapCellCoordinate> tributary =
            mapGenV4FindDrainagePath(map, elevation, tributarySource, target,
                                     options.seed + 3557u + static_cast<std::uint32_t>(tributaryIndex * 83));
        const std::size_t minimumTributaryLength =
            static_cast<std::size_t>(std::max(7, std::min(map.width, map.height) / 6));
        if (tributary.size() < minimumTributaryLength)
            continue;
        // Join the first main/earlier river encountered and never draw through
        // the opposite bank. This also keeps the feature graph a drainage tree.
        for (std::size_t index = 1; index < tributary.size(); ++index)
        {
            const MapCellModel *cell = mapCellAt(map, tributary[index].x, tributary[index].y);
            if (cell && cell->wayKind == MapWayKind::River)
            {
                tributary.resize(index + 1);
                break;
            }
        }
        if (tributary.size() < minimumTributaryLength)
            continue;
        std::size_t joinedMainIndex = path.size();
        for (std::size_t mainIndex = 0; mainIndex < path.size(); ++mainIndex)
            if (path[mainIndex] == tributary.back())
            {
                joinedMainIndex = mainIndex;
                break;
            }
        if (joinedMainIndex == 0 || joinedMainIndex + 1 >= path.size())
            continue;
        const std::size_t incomingIndex = tributary.size() > 4 ? tributary.size() - 4 : 0;
        const float actualIncomingX = static_cast<float>(tributary.back().x - tributary[incomingIndex].x);
        const float actualIncomingY = static_cast<float>(tributary.back().y - tributary[incomingIndex].y);
        const float actualMainX = static_cast<float>(path[joinedMainIndex + 1].x - path[joinedMainIndex - 1].x);
        const float actualMainY = static_cast<float>(path[joinedMainIndex + 1].y - path[joinedMainIndex - 1].y);
        const float actualDivisor = std::sqrt((actualIncomingX * actualIncomingX + actualIncomingY * actualIncomingY) *
                                              (actualMainX * actualMainX + actualMainY * actualMainY));
        const float actualAlignment =
            actualDivisor <= 0.001f ? -1.0f
                                    : (actualIncomingX * actualMainX + actualIncomingY * actualMainY) / actualDivisor;
        if (actualAlignment < 0.24f || actualAlignment > 0.93f)
            continue;

        // Reject a branch that shadows the main stem for a substantial part
        // of its run. Such a branch is semantically connected but renders as
        // two parallel rails followed by an arrow-shaped merge (seed 930000).
        // The final approach is intentionally excluded: natural tributaries
        // are expected to align briefly immediately before the confluence.
        int parallelSamples = 0;
        int comparableSamples = 0;
        for (std::size_t sampleIndex = 1; sampleIndex + 4 < tributary.size(); ++sampleIndex)
        {
            const float tributaryX = static_cast<float>(tributary[sampleIndex + 1].x - tributary[sampleIndex - 1].x);
            const float tributaryY = static_cast<float>(tributary[sampleIndex + 1].y - tributary[sampleIndex - 1].y);
            const float tributaryLength = std::sqrt(tributaryX * tributaryX + tributaryY * tributaryY);
            if (tributaryLength <= 0.001f)
                continue;
            float nearestSquared = std::numeric_limits<float>::max();
            float nearestTangentX = 0.0f;
            float nearestTangentY = 0.0f;
            const float sampleX = static_cast<float>(tributary[sampleIndex].x);
            const float sampleY = static_cast<float>(tributary[sampleIndex].y);
            for (std::size_t mainIndex = 1; mainIndex < path.size(); ++mainIndex)
            {
                const float startX = static_cast<float>(path[mainIndex - 1].x);
                const float startY = static_cast<float>(path[mainIndex - 1].y);
                const float segmentX = static_cast<float>(path[mainIndex].x - path[mainIndex - 1].x);
                const float segmentY = static_cast<float>(path[mainIndex].y - path[mainIndex - 1].y);
                const float segmentLengthSquared = segmentX * segmentX + segmentY * segmentY;
                if (segmentLengthSquared <= 0.001f)
                    continue;
                const float amount = std::clamp(
                    ((sampleX - startX) * segmentX + (sampleY - startY) * segmentY) / segmentLengthSquared, 0.0f, 1.0f);
                const float deltaX = sampleX - (startX + segmentX * amount);
                const float deltaY = sampleY - (startY + segmentY * amount);
                const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
                if (distanceSquared >= nearestSquared)
                    continue;
                nearestSquared = distanceSquared;
                const float inverseLength = 1.0f / std::sqrt(segmentLengthSquared);
                nearestTangentX = segmentX * inverseLength;
                nearestTangentY = segmentY * inverseLength;
            }
            if (nearestSquared > 12.25f)
                continue;
            ++comparableSamples;
            const float tangentAlignment =
                std::abs((tributaryX * nearestTangentX + tributaryY * nearestTangentY) / tributaryLength);
            if (tangentAlignment > 0.80f)
                ++parallelSamples;
        }
        if (parallelSamples >= 3 && parallelSamples * 4 >= std::max(1, comparableSamples))
            continue;

        // A secondary river must contribute a readable watershed shape, not
        // merely a hairline ruler stroke from a nearby hill to the main stem.
        // If the drainage solver found an almost perfectly straight chord,
        // omitting that optional tributary produces the stronger world map.
        const float tributaryChordX = static_cast<float>(tributary.back().x - tributary.front().x);
        const float tributaryChordY = static_cast<float>(tributary.back().y - tributary.front().y);
        const float tributaryChordLength =
            std::sqrt(tributaryChordX * tributaryChordX + tributaryChordY * tributaryChordY);
        float maximumTributaryDeviation = 0.0f;
        if (tributaryChordLength > 0.001f)
            for (std::size_t index = 1; index + 1 < tributary.size(); ++index)
            {
                const float relativeX = static_cast<float>(tributary[index].x - tributary.front().x);
                const float relativeY = static_cast<float>(tributary[index].y - tributary.front().y);
                maximumTributaryDeviation = std::max(
                    maximumTributaryDeviation,
                    std::abs(tributaryChordX * relativeY - tributaryChordY * relativeX) / tributaryChordLength);
            }
        const float requiredTributaryDeviation = std::clamp(tributaryChordLength * 0.075f, 1.15f, 2.40f);
        if (maximumTributaryDeviation < requiredTributaryDeviation)
            continue;
        tributarySources.push_back(tributarySource);
        const MapCellModel *joinCell = mapCellAt(map, tributary.back().x, tributary.back().y);
        const int joinElevation = joinCell ? joinCell->elevation : 0;
        const int sampledElevation = std::clamp(
            static_cast<int>(std::lround(
                elevation[static_cast<std::size_t>(mapCellIndex(map, tributarySource.x, tributarySource.y))] * 20.0f)),
            1, 20);
        const int tributaryElevation = std::max(sampledElevation, joinElevation + 3);
        for (std::size_t index = 0; index + 1 < tributary.size(); ++index)
        {
            const MapCellCoordinate coordinate = tributary[index];
            mapGenSetRiverCell(map, coordinate.x, coordinate.y);
            if (MapCellModel *riverCell = mapCellAt(map, coordinate.x, coordinate.y))
            {
                const float progress = static_cast<float>(index) / static_cast<float>(tributary.size() - 1);
                riverCell->elevation = std::clamp(
                    static_cast<int>(std::lround(static_cast<float>(tributaryElevation) +
                                                 static_cast<float>(joinElevation - tributaryElevation) * progress)),
                    0, 20);
            }
        }
        mapGenV2AddPathFeature(
            map, MapGeneratedFeatureKind::River, mapGenV2FeatureId(options.seed, "river_tributary", tributaryIndex),
            "River tributary " + std::to_string(tributaryIndex + 1), "fantasy_neutral.water.creek", 1.02f, tributary);
        if (options.generatorVersion >= 5 && options.scale == MapLevel::World)
            mapGenV5SmoothWorldPathFeature(map.generation.graph.features.back(), 3);
    }
    mapGenFillDiagonalRiverCornerGaps(map);
}

