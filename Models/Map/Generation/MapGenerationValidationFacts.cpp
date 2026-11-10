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

bool mapGenSemanticElementPresent(const MapModel &map, const std::string &key)
{
    if (key.empty())
        return false;
    if (key == "coast")
        return mapGenHasCoast(map);
    if (key == "island")
        return mapGenHasIsland(map);
    if (key == "lake")
        return mapGenHasWaterComponent(map, false);
    if (key == "sea")
        return mapGenHasWaterComponent(map, true);
    if (std::any_of(map.cells.begin(), map.cells.end(),
                    [&](const MapCellModel &cell) { return mapGenCellHasSemantic(cell, key); }))
        return true;
    return std::any_of(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                       [&](const MapGeneratedFeature &feature) {
                           if (key == "river")
                               return feature.kind == MapGeneratedFeatureKind::River;
                           if (key == "road")
                               return feature.kind == MapGeneratedFeatureKind::Road ||
                                      feature.kind == MapGeneratedFeatureKind::Trail;
                           if (key == "forest")
                               return feature.kind == MapGeneratedFeatureKind::Vegetation;
                           if (key == "mountain_range")
                               return feature.kind == MapGeneratedFeatureKind::MountainRange;
                           if (key == "water")
                               return feature.kind == MapGeneratedFeatureKind::WaterBody;
                           return false;
                       });
}

bool mapGenFeatureMatchesText(const MapGeneratedFeature &feature, const std::string &text)
{
    const std::string needle = mapGenNormalizedElementText(text);
    if (needle.empty())
        return false;
    return mapGenNormalizedElementText(feature.name).find(needle) != std::string::npos ||
           mapGenNormalizedElementText(feature.materialId).find(needle) != std::string::npos;
}

const MapGeneratedFeature *mapGenV5FindNationRelationFeature(const MapModel &map, const std::string &name)
{
    const std::string key = mapGenV5RelationNameKey(name);
    const auto found = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                    [&](const MapGeneratedFeature &feature) {
                                        const bool territory =
                                            feature.kind == MapGeneratedFeatureKind::Nation ||
                                            (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                                             feature.materialId.find("territory.") != std::string::npos);
                                        return territory && mapGenV5RelationNameKey(feature.name) == key;
                                    });
    return found == map.generation.graph.features.end() ? nullptr : &*found;
}

float mapGenV5PointSegmentDistanceSquared(const MapFeaturePoint &point, const MapFeaturePoint &start,
                                          const MapFeaturePoint &end)
{
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float amount =
        lengthSquared <= 0.000001f
            ? 0.0f
            : std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared, 0.0f, 1.0f);
    const float closestX = start.x + dx * amount;
    const float closestY = start.y + dy * amount;
    const float distanceX = point.x - closestX;
    const float distanceY = point.y - closestY;
    return distanceX * distanceX + distanceY * distanceY;
}

bool mapGenV5TerritoriesShareBorder(const MapModel &map, const MapGeneratedFeature &left,
                                    const MapGeneratedFeature &right)
{
    if (!left.closed || !right.closed || left.points.size() < 3 || right.points.size() < 3)
        return false;
    const float tolerance = std::max(0.20f, static_cast<float>(std::min(map.width, map.height)) * 0.008f);
    const float toleranceSquared = tolerance * tolerance;
    const float minimumSharedLength = std::max(0.8f, static_cast<float>(std::min(map.width, map.height)) * 0.025f);
    for (std::size_t leftIndex = 0; leftIndex < left.points.size(); ++leftIndex)
    {
        const MapFeaturePoint &leftStart = left.points[leftIndex];
        const MapFeaturePoint &leftEnd = left.points[(leftIndex + 1) % left.points.size()];
        const float leftDx = leftEnd.x - leftStart.x;
        const float leftDy = leftEnd.y - leftStart.y;
        if (std::sqrt(leftDx * leftDx + leftDy * leftDy) < minimumSharedLength)
            continue;
        const MapFeaturePoint midpoint{(leftStart.x + leftEnd.x) * 0.5f, (leftStart.y + leftEnd.y) * 0.5f};
        for (std::size_t rightIndex = 0; rightIndex < right.points.size(); ++rightIndex)
        {
            const MapFeaturePoint &rightStart = right.points[rightIndex];
            const MapFeaturePoint &rightEnd = right.points[(rightIndex + 1) % right.points.size()];
            const bool midpointShared =
                mapGenV5PointSegmentDistanceSquared(midpoint, rightStart, rightEnd) <= toleranceSquared;
            const bool endpointShared =
                mapGenV5PointSegmentDistanceSquared(leftStart, rightStart, rightEnd) <= toleranceSquared ||
                mapGenV5PointSegmentDistanceSquared(leftEnd, rightStart, rightEnd) <= toleranceSquared;
            if (midpointShared && endpointShared)
                return true;
        }
    }
    return false;
}

bool mapGenV5RelativeFactIsSatisfied(const MapModel &map, const MapGenerationFact &fact)
{
    if (fact.entityRefs.size() < 2)
        return false;
    const std::string relation = mapGenV5RelationKindKey(fact);
    if (relation == "borders")
    {
        const MapGeneratedFeature *left = mapGenV5FindNationRelationFeature(map, fact.entityRefs[0]);
        const MapGeneratedFeature *right = mapGenV5FindNationRelationFeature(map, fact.entityRefs[1]);
        return left && right && left != right && mapGenV5TerritoriesShareBorder(map, *left, *right);
    }

    const MapGeneratedFeature *subject = mapGenV5FindRelationFeature(map, fact.entityRefs[0]);
    const MapGeneratedFeature *object = mapGenV5FindRelationFeature(map, fact.entityRefs[1]);
    MapFeaturePoint subjectCenter;
    MapFeaturePoint objectCenter;
    if (!subject || !object || subject == object || !mapGenV5RelationFeatureCenter(*subject, subjectCenter) ||
        !mapGenV5RelationFeatureCenter(*object, objectCenter))
        return false;
    const float horizontalTolerance = std::max(1.0f, static_cast<float>(map.width) * 0.06f);
    const float verticalTolerance = std::max(1.0f, static_cast<float>(map.height) * 0.06f);
    if (relation == "north_of")
        return subjectCenter.y + verticalTolerance <= objectCenter.y;
    if (relation == "south_of")
        return subjectCenter.y >= objectCenter.y + verticalTolerance;
    if (relation == "east_of")
        return subjectCenter.x >= objectCenter.x + horizontalTolerance;
    if (relation == "west_of")
        return subjectCenter.x + horizontalTolerance <= objectCenter.x;
    if (relation != "between" || fact.entityRefs.size() != 3 ||
        (subject->kind != MapGeneratedFeatureKind::Settlement && subject->kind != MapGeneratedFeatureKind::Landmark))
        return false;

    const MapGeneratedFeature *secondObject = mapGenV5FindRelationFeature(map, fact.entityRefs[2]);
    MapFeaturePoint secondObjectCenter;
    if (!secondObject || secondObject == subject || secondObject == object ||
        !mapGenV5RelationFeatureCenter(*secondObject, secondObjectCenter))
        return false;
    const float segmentX = secondObjectCenter.x - objectCenter.x;
    const float segmentY = secondObjectCenter.y - objectCenter.y;
    const float lengthSquared = segmentX * segmentX + segmentY * segmentY;
    if (lengthSquared <= 1.0f)
        return false;
    const float projection =
        ((subjectCenter.x - objectCenter.x) * segmentX + (subjectCenter.y - objectCenter.y) * segmentY) / lengthSquared;
    const float maximumDistance =
        std::max(2.0f, std::sqrt(static_cast<float>(map.width * map.width + map.height * map.height)) * 0.08f);
    return projection >= 0.10f && projection <= 0.90f &&
           mapGenV5PointSegmentDistanceSquared(subjectCenter, objectCenter, secondObjectCenter) <=
               maximumDistance * maximumDistance;
}

bool mapGenFactIsSatisfied(const MapModel &map, const MapGenerationFact &fact)
{
    const auto hasFeature = [&](MapGeneratedFeatureKind kind, const std::string &label) {
        return std::any_of(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                           [&](const MapGeneratedFeature &feature) {
                               return feature.kind == kind &&
                                      (label.empty() || mapGenFeatureMatchesText(feature, label));
                           });
    };
    const auto featureCount = [&](const std::function<bool(const MapGeneratedFeature &)> &predicate) {
        return static_cast<int>(
            std::count_if(map.generation.graph.features.begin(), map.generation.graph.features.end(), predicate));
    };
    const std::string label = !fact.label.empty() ? fact.label : fact.value;
    const std::string semanticKey = mapGenSemanticElementKey(!fact.value.empty() ? fact.value : label);
    switch (fact.type)
    {
    case MapGenerationFactType::Nation:
        return std::any_of(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                           [&](const MapGeneratedFeature &feature) {
                               const bool territory = feature.kind == MapGeneratedFeatureKind::Nation ||
                                                      (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                                                       feature.materialId.find("territory.") != std::string::npos);
                               return territory && mapGenFeatureMatchesText(feature, label);
                           });
    case MapGenerationFactType::NationCount: {
        int expected = 0;
        try
        {
            expected = std::stoi(fact.value.empty() ? fact.label : fact.value);
        }
        catch (...)
        {
            return false;
        }
        const int actual = featureCount([](const MapGeneratedFeature &feature) {
            return feature.kind == MapGeneratedFeatureKind::Nation ||
                   (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                    feature.materialId.find("territory.") != std::string::npos);
        });
        return actual == expected;
    }
    case MapGenerationFactType::Settlement:
    case MapGenerationFactType::Capital: {
        if (!hasFeature(MapGeneratedFeatureKind::Settlement, label))
            return false;
        const auto nation = map.generation.recipe.generatorVersion >= 11
                                ? mapGenV11AuthoredSettlementNationName(map.generation.recipe.facts, label)
                                : std::string{};
        if (nation.empty())
            return true;
        for (const auto &city : map.generation.graph.features)
            if (city.kind == MapGeneratedFeatureKind::Settlement && !city.points.empty() &&
                mapGenV5RelationNameKey(city.name) == mapGenV5RelationNameKey(label))
                for (const auto &territory : map.generation.graph.features)
                    if (territory.closed && territory.kind == MapGeneratedFeatureKind::TerrainRegion &&
                        territory.materialId.rfind("fantasy_neutral.territory.", 0) == 0 &&
                        mapGenV5RelationNameKey(territory.name) == mapGenV5RelationNameKey(nation) &&
                        mapGenV5PointInsideTerritory(city.points.front(), territory))
                        return true;
        return false;
    }
    case MapGenerationFactType::River:
        if (semanticKey == "river")
            return mapGenSemanticElementPresent(map, semanticKey);
        return hasFeature(MapGeneratedFeatureKind::River, label) ||
               (label.empty() && hasFeature(MapGeneratedFeatureKind::River, {}));
    case MapGenerationFactType::SharedRiver:
        return mapGenV4SharedGreatRiverIsValid(map);
    case MapGenerationFactType::WaterBody:
        if (!semanticKey.empty())
            return mapGenSemanticElementPresent(map, semanticKey);
        return hasFeature(MapGeneratedFeatureKind::WaterBody, label) ||
               hasFeature(MapGeneratedFeatureKind::TerrainRegion, label);
    case MapGenerationFactType::Coast:
        return mapGenHasCoast(map);
    case MapGenerationFactType::Island:
        return mapGenHasIsland(map);
    case MapGenerationFactType::MountainRange:
        if (semanticKey == "mountain_range")
            return mapGenSemanticElementPresent(map, semanticKey);
        return hasFeature(MapGeneratedFeatureKind::MountainRange, label) ||
               mapGenCountMatchingCells(
                   map, [](const MapCellModel &cell) { return cell.kind == MapTileKind::Mountain; }) > 0;
    case MapGenerationFactType::Forest:
        if (semanticKey == "forest")
            return mapGenSemanticElementPresent(map, semanticKey);
        return hasFeature(MapGeneratedFeatureKind::Vegetation, label) ||
               mapGenCountMatchingCells(map,
                                        [](const MapCellModel &cell) { return cell.kind == MapTileKind::Forest; }) > 0;
    case MapGenerationFactType::Road:
        if (semanticKey == "road")
            return mapGenSemanticElementPresent(map, semanticKey);
        return hasFeature(MapGeneratedFeatureKind::Road, label) || hasFeature(MapGeneratedFeatureKind::Trail, label);
    case MapGenerationFactType::IncludedElement:
        if (!semanticKey.empty())
            return mapGenSemanticElementPresent(map, semanticKey);
        return std::any_of(
            map.generation.graph.features.begin(), map.generation.graph.features.end(),
            [&](const MapGeneratedFeature &feature) { return mapGenFeatureMatchesText(feature, label); });
    case MapGenerationFactType::ExcludedElement:
        if (!semanticKey.empty())
            return !mapGenSemanticElementPresent(map, semanticKey);
        return std::none_of(
            map.generation.graph.features.begin(), map.generation.graph.features.end(),
            [&](const MapGeneratedFeature &feature) { return mapGenFeatureMatchesText(feature, label); });
    case MapGenerationFactType::Note:
    case MapGenerationFactType::Region:
        // These facts need a more specific normalized constraint before they
        // can become a hard gate. Leaving an untyped note as Must therefore
        // fails closed instead of pretending the prose was implemented.
        return false;
    case MapGenerationFactType::RelativePosition:
        return mapGenV5RelativeFactIsSatisfied(map, fact);
    }
    return false;
}

