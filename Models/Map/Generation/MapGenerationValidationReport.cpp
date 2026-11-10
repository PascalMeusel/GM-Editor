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

MapGenerationFactValidation mapGenValidateFactWithEvidence(const MapModel &map, const MapGenerationFact &fact)
{
    MapGenerationFactValidation result;
    result.factId = fact.id;
    result.strength = fact.strength;
    const std::string label = !fact.label.empty() ? fact.label : fact.value;
    const bool satisfied = mapGenFactIsSatisfied(map, fact);
    result.fulfillment =
        satisfied
            ? MapGenerationFactFulfillment::Fulfilled
            : (fact.strength == MapGenerationFactStrength::Must
                   ? MapGenerationFactFulfillment::Unsatisfied
                   : (fact.strength == MapGenerationFactStrength::Prefer ? MapGenerationFactFulfillment::Adjusted
                                                                         : MapGenerationFactFulfillment::NotEvaluated));
    if (!satisfied)
        result.issueCode =
            fact.strength == MapGenerationFactStrength::Must
                ? "fact.must_unsatisfied"
                : (fact.strength == MapGenerationFactStrength::Prefer ? "fact.prefer_adjusted" : std::string{});

    const auto featureRepresentsFact = [&](const MapGeneratedFeature &feature) {
        const bool textMatches = label.empty() || mapGenFeatureMatchesText(feature, label);
        switch (fact.type)
        {
        case MapGenerationFactType::Nation:
            return textMatches && (feature.kind == MapGeneratedFeatureKind::Nation ||
                                   (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                                    feature.materialId.find("territory.") != std::string::npos));
        case MapGenerationFactType::NationCount:
            return feature.kind == MapGeneratedFeatureKind::Nation ||
                   (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                    feature.materialId.find("territory.") != std::string::npos);
        case MapGenerationFactType::Settlement:
        case MapGenerationFactType::Capital:
            return textMatches && feature.kind == MapGeneratedFeatureKind::Settlement;
        case MapGenerationFactType::River:
        case MapGenerationFactType::SharedRiver:
            return feature.kind == MapGeneratedFeatureKind::River &&
                   (fact.type == MapGenerationFactType::SharedRiver || textMatches);
        case MapGenerationFactType::WaterBody:
            return textMatches && (feature.kind == MapGeneratedFeatureKind::WaterBody ||
                                   feature.kind == MapGeneratedFeatureKind::TerrainRegion);
        case MapGenerationFactType::Coast:
        case MapGenerationFactType::Island:
            return textMatches && feature.kind == MapGeneratedFeatureKind::Coastline;
        case MapGenerationFactType::MountainRange:
            return textMatches && feature.kind == MapGeneratedFeatureKind::MountainRange;
        case MapGenerationFactType::Forest:
            return textMatches && feature.kind == MapGeneratedFeatureKind::Vegetation;
        case MapGenerationFactType::Road:
            return textMatches &&
                   (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail);
        case MapGenerationFactType::IncludedElement:
        case MapGenerationFactType::ExcludedElement:
            return mapGenFeatureMatchesText(feature, label);
        case MapGenerationFactType::RelativePosition: {
            const bool namedParticipant =
                std::any_of(fact.entityRefs.begin(), fact.entityRefs.end(), [&](const std::string &reference) {
                    return mapGenV5RelationNameKey(feature.name) == mapGenV5RelationNameKey(reference);
                });
            if (!namedParticipant)
                return false;
            if (mapGenV5RelationKindKey(fact) == "borders")
                return feature.kind == MapGeneratedFeatureKind::Nation ||
                       (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                        feature.materialId.find("territory.") != std::string::npos);
            return feature.kind == MapGeneratedFeatureKind::Settlement ||
                   feature.kind == MapGeneratedFeatureKind::Landmark ||
                   feature.kind == MapGeneratedFeatureKind::Nation ||
                   (feature.kind == MapGeneratedFeatureKind::TerrainRegion &&
                    feature.materialId.find("territory.") != std::string::npos);
        }
        case MapGenerationFactType::Note:
        case MapGenerationFactType::Region:
            return false;
        }
        return false;
    };
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!featureRepresentsFact(feature))
            continue;
        result.affectedFeatureIds.push_back(feature.id);
        if (!feature.points.empty() && result.positions.size() < 16)
            result.positions.push_back(feature.points.front());
    }

    // Older or intentionally raster-only maps still provide a useful focus
    // point for material facts even when no graph feature exists.
    if (result.positions.empty() && satisfied)
    {
        for (int y = 0; y < map.height && result.positions.empty(); ++y)
            for (int x = 0; x < map.width && result.positions.empty(); ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell)
                    continue;
                const bool matchesCell =
                    ((fact.type == MapGenerationFactType::Coast || fact.type == MapGenerationFactType::Island ||
                      fact.type == MapGenerationFactType::WaterBody) &&
                     cell->kind == MapTileKind::Water) ||
                    (fact.type == MapGenerationFactType::MountainRange && cell->kind == MapTileKind::Mountain) ||
                    (fact.type == MapGenerationFactType::Forest && cell->kind == MapTileKind::Forest);
                if (matchesCell)
                    result.positions.push_back({static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f});
            }
    }
    return result;
}

MapGenerationValidationReport validateMapGeneration(const MapModel &map, const MapGenerationQualityReport &quality)
{
    MapGenerationValidationReport validation;
    const bool enforceHardContract = map.generation.recipe.generatorVersion >= 4;
    for (const MapGenerationIssue &issue : quality.issues)
    {
        if (enforceHardContract && issue.severity == MapGenerationIssueSeverity::Critical)
            validation.blockers.push_back(issue);
        else
            validation.warnings.push_back(issue);
    }
    const auto addBlocker = [&](const char *code, const std::string &message, const char *action) {
        const bool duplicate =
            std::any_of(validation.blockers.begin(), validation.blockers.end(), [&](const MapGenerationIssue &issue) {
                return issue.code == code && issue.message == message;
            });
        if (!duplicate)
            validation.blockers.push_back({code, MapGenerationIssueSeverity::Critical, message, action});
    };
    for (const MapGenerationConstraint &constraint : map.generation.recipe.constraints)
    {
        if (mapGenIsAtlasBoundaryConstraint(constraint))
            continue;
        if (enforceHardContract && constraint.required &&
            !mapGenFeatureSatisfiesRequirement(map.generation.graph, constraint.id, constraint.label))
            addBlocker("requirement.constraint_missing", constraint.label,
                       "Regenerate the requested feature or edit the required fact.");
    }
    for (const std::string &required : map.generation.recipe.requiredElements)
    {
        if (!enforceHardContract || required.empty())
            continue;
        const bool present = std::any_of(
            map.generation.graph.features.begin(), map.generation.graph.features.end(),
            [&](const MapGeneratedFeature &feature) { return mapGenFeatureMatchesText(feature, required); });
        if (!present)
            addBlocker("requirement.element_missing", required,
                       "Regenerate the requested feature or edit the required fact.");
    }
    for (const MapGenerationFact &fact : map.generation.recipe.facts)
    {
        MapGenerationFactValidation factValidation = mapGenValidateFactWithEvidence(map, fact);
        validation.facts.push_back(factValidation);
        if (enforceHardContract && factValidation.fulfillment == MapGenerationFactFulfillment::Unsatisfied)
            addBlocker("fact.must_unsatisfied", fact.id.empty() ? fact.label : fact.id,
                       "Regenerate the map or edit the conflicting required fact.");
        else if (factValidation.fulfillment == MapGenerationFactFulfillment::Adjusted)
            validation.warnings.push_back({"fact.prefer_adjusted", MapGenerationIssueSeverity::Warning,
                                           fact.id.empty() ? fact.label : fact.id,
                                           "Review the adjusted preference before accepting this design."});
    }
    validation.criticalIssueCount = static_cast<int>(validation.blockers.size());
    validation.eligible = validation.blockers.empty();
    return validation;
}

bool mapGenQualityMeetsCandidateFloor(const MapModel &map, const MapGenerationQualityReport &quality)
{
    // Legacy generators remain inspectable for old campaigns. The production
    // V4/V5 path only publishes designs that satisfy the agreed readiness
    // floor; quality is not allowed to compensate for a weak category.
    if (map.generation.recipe.generatorVersion < 4)
        return true;
    if (quality.score < 85 || quality.wayConnectivityPercent < 80 || quality.coherentTerrainPercent < 80 ||
        quality.roadEfficiencyPercent < 80 || quality.hydrologyPercent < 80 || quality.bridgeValidityPercent < 80 ||
        quality.settlementSuitabilityPercent < 80 || quality.compositionPercent < 80 ||
        quality.visualHierarchyPercent < 80 || quality.scaleCorrectnessPercent < 80 ||
        quality.assetCoveragePercent < 80)
        return false;
    if (map.atlasLevel != MapLevel::World &&
        !(map.generation.recipe.generatorVersion >= 11 && map.atlasLevel == MapLevel::Region &&
          map.generation.recipe.landform == MapLandform::Islands) &&
        quality.traversableConnectivityPercent < 80)
        return false;
    if ((map.atlasLevel == MapLevel::Site || map.atlasLevel == MapLevel::Tactical) &&
        quality.structureAccessPercent < 80)
        return false;
    return true;
}

MapGenerationValidationReport validateMapGenerationRequest(const MapGenerationOptions &options)
{
    MapGenerationValidationReport validation;
    for (const MapGenerationFact &fact : options.facts)
    {
        MapGenerationFactValidation pending;
        pending.factId = fact.id;
        pending.strength = fact.strength;
        pending.fulfillment = MapGenerationFactFulfillment::NotEvaluated;
        validation.facts.push_back(std::move(pending));
    }
    const auto rejectFact = [&](const std::string &factId, const char *issueCode) {
        const auto found =
            std::find_if(validation.facts.begin(), validation.facts.end(),
                         [&](const MapGenerationFactValidation &entry) { return entry.factId == factId; });
        if (found != validation.facts.end())
        {
            found->fulfillment = MapGenerationFactFulfillment::Unsatisfied;
            found->issueCode = issueCode;
        }
    };
    const auto addBlocker = [&](const char *code, const std::string &detail, const char *action) {
        validation.blockers.push_back({code, MapGenerationIssueSeverity::Critical, detail, action});
    };
    for (const std::string &required : options.requiredElements)
        if (!required.empty() && mapGenExcludedElementMatches(required, options.excludedElements))
            addBlocker("request.include_exclude_conflict", required,
                       "Keep the element required or excluded, but not both.");
    for (const MapGenerationConstraint &constraint : options.constraints)
        if (!mapGenIsAtlasBoundaryConstraint(constraint) && constraint.required &&
            mapGenExcludedElementMatches(constraint.label, options.excludedElements))
            addBlocker("request.constraint_excluded", constraint.label,
                       "Remove the exclusion or make the placement optional.");

    std::vector<std::string> declaredEntities;
    std::vector<std::string> declaredNations;
    std::vector<std::string> declaredPointEntities;
    const auto declareEntity = [&](std::vector<std::string> &target, const std::string &name) {
        const std::string normalized = mapGenNormalizedElementText(name);
        if (!normalized.empty() && std::find(target.begin(), target.end(), normalized) == target.end())
            target.push_back(normalized);
        if (!normalized.empty() &&
            std::find(declaredEntities.begin(), declaredEntities.end(), normalized) == declaredEntities.end())
            declaredEntities.push_back(normalized);
    };
    for (const std::string &nation : options.nationNames)
        declareEntity(declaredNations, nation);
    for (const MapGenerationFact &fact : options.facts)
    {
        const std::string name = !fact.label.empty() ? fact.label : fact.value;
        if (fact.type == MapGenerationFactType::Nation)
            declareEntity(declaredNations, name);
        else if (fact.type == MapGenerationFactType::Settlement || fact.type == MapGenerationFactType::Capital)
            declareEntity(declaredPointEntities, name);
    }
    for (const MapGenerationConstraint &constraint : options.constraints)
        if (constraint.featureKind == MapGeneratedFeatureKind::Settlement ||
            constraint.featureKind == MapGeneratedFeatureKind::Landmark)
            declareEntity(declaredPointEntities, constraint.label);

    const auto entityDeclared = [&](const std::vector<std::string> &set, const std::string &name) {
        const std::string normalized = mapGenNormalizedElementText(name);
        return !normalized.empty() && std::find(set.begin(), set.end(), normalized) != set.end();
    };
    for (const MapGenerationFact &fact : options.facts)
    {
        if (fact.type != MapGenerationFactType::RelativePosition)
            continue;
        const std::string relation = mapGenNormalizedElementText(fact.value);
        const bool relationKnown = relation == "north of" || relation == "south of" || relation == "east of" ||
                                   relation == "west of" || relation == "borders" || relation == "between";
        const std::size_t expectedReferences = relation == "between" ? 3u : 2u;
        const bool referencesValid =
            fact.entityRefs.size() == expectedReferences &&
            std::all_of(fact.entityRefs.begin(), fact.entityRefs.end(),
                        [&](const std::string &reference) { return entityDeclared(declaredEntities, reference); });
        const bool relationTypesValid =
            (relation != "borders" || (referencesValid && entityDeclared(declaredNations, fact.entityRefs[0]) &&
                                       entityDeclared(declaredNations, fact.entityRefs[1]))) &&
            (relation != "between" || (referencesValid && entityDeclared(declaredPointEntities, fact.entityRefs[0])));
        if (!relationKnown || !referencesValid || !relationTypesValid)
        {
            const std::string detail = fact.id.empty() ? fact.sourceText : fact.id;
            if (fact.strength == MapGenerationFactStrength::Must)
            {
                addBlocker("request.relative_relation_invalid", detail,
                           "Review the relationship and identify every referenced place.");
                rejectFact(fact.id, "request.relative_relation_invalid");
            }
            else
            {
                validation.warnings.push_back({"request.relative_relation_invalid", MapGenerationIssueSeverity::Warning,
                                               detail,
                                               "Review the relationship or keep it as context for manual placement."});
                const auto pending =
                    std::find_if(validation.facts.begin(), validation.facts.end(),
                                 [&](const MapGenerationFactValidation &entry) { return entry.factId == fact.id; });
                if (pending != validation.facts.end())
                {
                    pending->fulfillment = fact.strength == MapGenerationFactStrength::Prefer
                                               ? MapGenerationFactFulfillment::Adjusted
                                               : MapGenerationFactFulfillment::NotEvaluated;
                    pending->issueCode = "request.relative_relation_invalid";
                }
            }
        }
    }

    std::vector<std::string> namedNations;
    int requiredNationCount = -1;
    for (const MapGenerationFact &fact : options.facts)
    {
        if (fact.strength != MapGenerationFactStrength::Must)
            continue;
        if (fact.type == MapGenerationFactType::Note)
        {
            addBlocker("request.must_note_untyped", fact.id.empty() ? fact.sourceText : fact.id,
                       "Confirm the note as a typed fact before generation.");
            rejectFact(fact.id, "request.must_note_untyped");
        }
        else if (fact.type == MapGenerationFactType::Nation && !fact.label.empty())
        {
            const std::string normalized = mapGenNormalizedElementText(fact.label);
            if (std::find(namedNations.begin(), namedNations.end(), normalized) == namedNations.end())
                namedNations.push_back(normalized);
        }
        else if (fact.type == MapGenerationFactType::NationCount)
        {
            try
            {
                requiredNationCount = std::stoi(fact.value.empty() ? fact.label : fact.value);
            }
            catch (...)
            {
                addBlocker("request.nation_count_invalid", fact.value.empty() ? fact.label : fact.value,
                           "Enter a whole number of nations.");
                rejectFact(fact.id, "request.nation_count_invalid");
            }
        }
    }
    if (requiredNationCount >= 0 && requiredNationCount < static_cast<int>(namedNations.size()))
    {
        addBlocker("request.nation_count_conflict", std::to_string(requiredNationCount),
                   "Increase the nation count or remove one of the named nations.");
        for (const MapGenerationFact &fact : options.facts)
            if (fact.type == MapGenerationFactType::NationCount || fact.type == MapGenerationFactType::Nation)
                rejectFact(fact.id, "request.nation_count_conflict");
    }

    for (const MapGenerationFact &included : options.facts)
    {
        if (included.strength != MapGenerationFactStrength::Must ||
            included.type == MapGenerationFactType::ExcludedElement || included.type == MapGenerationFactType::Note)
            continue;
        const std::string includedText = !included.label.empty() ? included.label : included.value;
        const std::string includedKey =
            mapGenSemanticElementKey(!included.value.empty() ? included.value : includedText);
        for (const MapGenerationFact &excluded : options.facts)
        {
            if (excluded.strength != MapGenerationFactStrength::Must ||
                excluded.type != MapGenerationFactType::ExcludedElement)
                continue;
            const std::string excludedText = !excluded.label.empty() ? excluded.label : excluded.value;
            const std::string excludedKey =
                mapGenSemanticElementKey(!excluded.value.empty() ? excluded.value : excludedText);
            if (!includedText.empty() &&
                ((!includedKey.empty() && includedKey == excludedKey) ||
                 mapGenNormalizedElementText(includedText) == mapGenNormalizedElementText(excludedText)))
            {
                addBlocker("request.include_exclude_conflict", includedText,
                           "Keep the element required or excluded, but not both.");
                rejectFact(included.id, "request.include_exclude_conflict");
                rejectFact(excluded.id, "request.include_exclude_conflict");
            }
        }
    }
    validation.criticalIssueCount = static_cast<int>(validation.blockers.size());
    validation.eligible = validation.blockers.empty();
    return validation;
}
