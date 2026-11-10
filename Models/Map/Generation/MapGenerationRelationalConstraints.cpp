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

std::vector<MapGenerationConstraint> mapGenV5BuildRelationalPointConstraints(const MapModel &map,
                                                                             const MapGenerationOptions &options)
{
    std::vector<MapGenerationConstraint> constraints = options.constraints;
    std::vector<bool> authoredLock;
    authoredLock.reserve(constraints.size() + options.facts.size());
    for (const MapGenerationConstraint &constraint : constraints)
    {
        // Brief-created coordinates are only deterministic suggestions. Their
        // lock preserves the named feature after placement, while a relation
        // may still choose its initial position. Atlas/manual anchors use
        // non-brief IDs and remain immovable.
        const bool generatedBriefPlacement = constraint.id.rfind("brief_", 0) == 0;
        authoredLock.push_back(constraint.locked && !generatedBriefPlacement);
    }

    const auto findPointConstraint = [&](const std::string &name) -> int {
        const std::string key = mapGenV5RelationNameKey(name);
        for (std::size_t index = 0; index < constraints.size(); ++index)
        {
            if ((constraints[index].featureKind == MapGeneratedFeatureKind::Settlement ||
                 constraints[index].featureKind == MapGeneratedFeatureKind::Landmark) &&
                mapGenV5RelationNameKey(constraints[index].label) == key)
                return static_cast<int>(index);
        }
        return -1;
    };
    const auto addDeclaredPoint = [&](const MapGenerationFact &fact) {
        if (fact.type != MapGenerationFactType::Settlement && fact.type != MapGenerationFactType::Capital)
            return;
        const std::string name = !fact.label.empty() ? fact.label : fact.value;
        if (name.empty() || findPointConstraint(name) >= 0)
            return;
        const std::uint32_t hash = mapGenV5RelationTextHash(mapGenV5RelationNameKey(name));
        MapGenerationConstraint constraint;
        constraint.id = fact.id.empty() ? "relation_place_" + std::to_string(hash) : fact.id;
        constraint.label = name;
        constraint.featureKind = MapGeneratedFeatureKind::Settlement;
        constraint.x = static_cast<float>(map.width) * (0.20f + mapGenV2Hash01(options.seed ^ hash, 3, 7) * 0.60f);
        constraint.y =
            static_cast<float>(map.height) * (0.20f + mapGenV2Hash01(options.seed ^ (hash + 911u), 11, 5) * 0.60f);
        constraint.required = fact.strength == MapGenerationFactStrength::Must;
        constraint.locked = constraint.required;
        constraints.push_back(std::move(constraint));
        // A point synthesized from a fact may be moved by the relationship
        // solver before it becomes locked in the generated map.
        authoredLock.push_back(false);
    };
    for (const MapGenerationFact &fact : options.facts)
        addDeclaredPoint(fact);

    const auto graphPoint = [&](const std::string &name, MapFeaturePoint &point) {
        const MapGeneratedFeature *feature = mapGenV5FindRelationFeature(map, name);
        return feature && mapGenV5RelationFeatureCenter(*feature, point);
    };
    const auto resolvedPoint = [&](const std::string &name, MapFeaturePoint &point, int &constraintIndex) {
        constraintIndex = findPointConstraint(name);
        if (constraintIndex >= 0)
        {
            point = {constraints[static_cast<std::size_t>(constraintIndex)].x,
                     constraints[static_cast<std::size_t>(constraintIndex)].y};
            return true;
        }
        return graphPoint(name, point);
    };
    const auto storePoint = [&](int index, MapFeaturePoint point) {
        if (index < 0 || authoredLock[static_cast<std::size_t>(index)])
            return;
        constraints[static_cast<std::size_t>(index)].x =
            std::clamp(point.x, 2.0f, std::max(2.0f, static_cast<float>(map.width) - 2.0f));
        constraints[static_cast<std::size_t>(index)].y =
            std::clamp(point.y, 2.0f, std::max(2.0f, static_cast<float>(map.height) - 2.0f));
    };

    const float horizontalMargin = std::max(3.0f, static_cast<float>(map.width) * 0.16f);
    const float verticalMargin = std::max(3.0f, static_cast<float>(map.height) * 0.16f);
    for (int pass = 0; pass < 12; ++pass)
    {
        for (const MapGenerationFact &fact : options.facts)
        {
            if (fact.type != MapGenerationFactType::RelativePosition || fact.entityRefs.size() < 2)
                continue;
            const std::string relation = mapGenV5RelationKindKey(fact);
            MapFeaturePoint subject;
            MapFeaturePoint object;
            int subjectConstraint = -1;
            int objectConstraint = -1;
            if (!resolvedPoint(fact.entityRefs[0], subject, subjectConstraint) ||
                !resolvedPoint(fact.entityRefs[1], object, objectConstraint))
                continue;
            if (relation == "between")
            {
                if (fact.entityRefs.size() != 3 || subjectConstraint < 0)
                    continue;
                MapFeaturePoint secondObject;
                int secondObjectConstraint = -1;
                if (!resolvedPoint(fact.entityRefs[2], secondObject, secondObjectConstraint))
                    continue;
                storePoint(subjectConstraint, {(object.x + secondObject.x) * 0.5f, (object.y + secondObject.y) * 0.5f});
                continue;
            }
            if (relation == "borders")
                continue;

            const bool vertical = relation == "north_of" || relation == "south_of";
            const bool subjectBefore = relation == "north_of" || relation == "west_of";
            if ((!vertical && relation != "east_of" && relation != "west_of") ||
                (vertical && relation != "north_of" && relation != "south_of"))
                continue;
            const float margin = vertical ? verticalMargin : horizontalMargin;
            float subjectAxis = vertical ? subject.y : subject.x;
            float objectAxis = vertical ? object.y : object.x;
            const bool satisfied =
                subjectBefore ? subjectAxis + margin <= objectAxis : subjectAxis >= objectAxis + margin;
            if (satisfied)
                continue;
            if (subjectConstraint >= 0 && !authoredLock[static_cast<std::size_t>(subjectConstraint)])
            {
                subjectAxis = subjectBefore ? objectAxis - margin : objectAxis + margin;
                if (vertical)
                    subject.y = subjectAxis;
                else
                    subject.x = subjectAxis;
                storePoint(subjectConstraint, subject);
            }
            else if (objectConstraint >= 0 && !authoredLock[static_cast<std::size_t>(objectConstraint)])
            {
                objectAxis = subjectBefore ? subjectAxis + margin : subjectAxis - margin;
                if (vertical)
                    object.y = objectAxis;
                else
                    object.x = objectAxis;
                storePoint(objectConstraint, object);
            }
        }
    }
    return constraints;
}

