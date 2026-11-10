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

std::string mapGenV5RelationNameKey(std::string value)
{
    std::string normalized;
    normalized.reserve(value.size());
    bool pendingSpace = false;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if (byte < 128 && std::isalnum(byte) != 0)
        {
            if (pendingSpace && !normalized.empty())
                normalized.push_back(' ');
            normalized.push_back(static_cast<char>(std::tolower(byte)));
            pendingSpace = false;
        }
        else if (byte >= 128)
        {
            if (pendingSpace && !normalized.empty())
                normalized.push_back(' ');
            normalized.push_back(static_cast<char>(byte));
            pendingSpace = false;
        }
        else
            pendingSpace = true;
    }
    return normalized;
}

std::string mapGenV5RelationKindKey(const MapGenerationFact &fact)
{
    std::string value = fact.value;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character < 128 ? static_cast<char>(std::tolower(character)) : static_cast<char>(character);
    });
    for (char &character : value)
        if (character == ' ' || character == '-')
            character = '_';
    return value;
}

int mapGenV5NamedNationIndex(const MapGenerationOptions &options, const std::string &name)
{
    const std::string key = mapGenV5RelationNameKey(name);
    for (std::size_t index = 0; index < options.nationNames.size(); ++index)
        if (mapGenV5RelationNameKey(options.nationNames[index]) == key)
            return static_cast<int>(index);
    return -1;
}

void mapGenV5ApplyNationRelations(std::vector<MapFeaturePoint> &centers, const MapGenerationOptions &options, int width,
                                  int height)
{
    if (centers.size() != options.nationNames.size() || centers.empty())
        return;
    const float horizontalMargin = std::max(3.0f, static_cast<float>(width) * 0.20f);
    const float verticalMargin = std::max(3.0f, static_cast<float>(height) * 0.20f);
    const float borderDistance = std::max(3.0f, static_cast<float>(std::min(width, height)) * 0.22f);
    const auto clampCenter = [&](MapFeaturePoint &point) {
        point.x = std::clamp(point.x, 1.5f, std::max(1.5f, static_cast<float>(width) - 1.5f));
        point.y = std::clamp(point.y, 1.5f, std::max(1.5f, static_cast<float>(height) - 1.5f));
    };
    const auto enforceAxis = [&](float &subject, float &object, float margin, bool subjectBefore, float minimum,
                                 float maximum) {
        const bool satisfied = subjectBefore ? subject + margin <= object : subject >= object + margin;
        if (satisfied)
            return;
        const float midpoint = (subject + object) * 0.5f;
        if (subjectBefore)
        {
            subject = midpoint - margin * 0.5f;
            object = midpoint + margin * 0.5f;
        }
        else
        {
            subject = midpoint + margin * 0.5f;
            object = midpoint - margin * 0.5f;
        }
        subject = std::clamp(subject, minimum, maximum);
        object = std::clamp(object, minimum, maximum);
    };

    // Repeating the constraints in declaration order is a deterministic small
    // relaxation solver. Contradictory cycles remain unsatisfied and are later
    // rejected by hard validation rather than being silently weakened.
    for (int pass = 0; pass < 16; ++pass)
    {
        for (const MapGenerationFact &fact : options.facts)
        {
            if (fact.type != MapGenerationFactType::RelativePosition || fact.entityRefs.size() < 2)
                continue;
            const int subjectIndex = mapGenV5NamedNationIndex(options, fact.entityRefs[0]);
            const int objectIndex = mapGenV5NamedNationIndex(options, fact.entityRefs[1]);
            if (subjectIndex < 0 || objectIndex < 0 || subjectIndex == objectIndex)
                continue;
            MapFeaturePoint &subject = centers[static_cast<std::size_t>(subjectIndex)];
            MapFeaturePoint &object = centers[static_cast<std::size_t>(objectIndex)];
            const std::string relation = mapGenV5RelationKindKey(fact);
            if (relation == "north_of")
                enforceAxis(subject.y, object.y, verticalMargin, true, 1.5f, static_cast<float>(height) - 1.5f);
            else if (relation == "south_of")
                enforceAxis(subject.y, object.y, verticalMargin, false, 1.5f, static_cast<float>(height) - 1.5f);
            else if (relation == "east_of")
                enforceAxis(subject.x, object.x, horizontalMargin, false, 1.5f, static_cast<float>(width) - 1.5f);
            else if (relation == "west_of")
                enforceAxis(subject.x, object.x, horizontalMargin, true, 1.5f, static_cast<float>(width) - 1.5f);
            else if (relation == "borders")
            {
                const float dx = subject.x - object.x;
                const float dy = subject.y - object.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance > borderDistance && distance > 0.001f)
                {
                    const float midpointX = (subject.x + object.x) * 0.5f;
                    const float midpointY = (subject.y + object.y) * 0.5f;
                    const float halfDistance = borderDistance * 0.5f;
                    subject.x = midpointX + dx / distance * halfDistance;
                    subject.y = midpointY + dy / distance * halfDistance;
                    object.x = midpointX - dx / distance * halfDistance;
                    object.y = midpointY - dy / distance * halfDistance;
                }
            }
            clampCenter(subject);
            clampCenter(object);
        }
    }
}

MapFeaturePoint mapGenV5SnapNationCenterToLand(const MapModel &map, MapFeaturePoint requested)
{
    const int requestedX = std::clamp(static_cast<int>(std::floor(requested.x)), 0, map.width - 1);
    const int requestedY = std::clamp(static_cast<int>(std::floor(requested.y)), 0, map.height - 1);
    const int maximumRadius = std::max(map.width, map.height);
    MapCellCoordinate best{-1, -1};
    float bestScore = std::numeric_limits<float>::max();
    for (int radius = 0; radius <= maximumRadius && best.x < 0; ++radius)
    {
        for (int offsetY = -radius; offsetY <= radius; ++offsetY)
        {
            for (int offsetX = -radius; offsetX <= radius; ++offsetX)
            {
                if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                    continue;
                const int x = requestedX + offsetX;
                const int y = requestedY + offsetY;
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Lava ||
                    cell->kind == MapTileKind::Pit || cell->kind == MapTileKind::Chasm)
                    continue;
                const float dx = static_cast<float>(x) + 0.5f - requested.x;
                const float dy = static_cast<float>(y) + 0.5f - requested.y;
                const float score = dx * dx + dy * dy;
                if (score < bestScore)
                {
                    bestScore = score;
                    best = {x, y};
                }
            }
        }
    }
    return best.x < 0 ? requested
                      : MapFeaturePoint{static_cast<float>(best.x) + 0.5f, static_cast<float>(best.y) + 0.5f};
}

