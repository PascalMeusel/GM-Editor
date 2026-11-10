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
#include "Models/Map/Generation/MapGenerationOverworldBuilder.hpp"

void MapGenerationOverworldBuilder::traceHydrology()
{
    // Atlas anchors must be present before hydrology, settlement selection and
    // route planning. In particular, new drainage can join an inherited river
    // and roads can connect to an inherited place instead of discovering both
    // only in the final merge.
    mapGenInstallPreservedFeatures(map, options);

    mapGenReportProgress(options, 3, totalSteps, "Tracing rivers");
    river.clear();
    if (options.waterLevel > 5 || sharedGreatRiver)
        river = options.generatorVersion >= 4 ? mapGenV4BuildHydrology(map, options, elevation)
                                              : mapGenV2BuildRiver(map, options, elevation, rng);

    // Nations are laid out only after lakes and drainage exist. V5 clips their
    // editable outlines to real land, so building them before hydrology would
    // leave political fill beneath lakes and river mouths. Shared-river nation
    // centers additionally follow the actual finished drainage spine.
    mapGenV2BuildNationTerritories(map, options);

    effectiveConstraints =
        options.generatorVersion >= 5 ? mapGenV5BuildRelationalPointConstraints(map, options) : options.constraints;
    map.generation.recipe.constraints = effectiveConstraints;
    map.generation.recipe.settlementCount = std::max(
        map.generation.recipe.settlementCount,
        static_cast<int>(std::count_if(effectiveConstraints.begin(), effectiveConstraints.end(),
                                       [](const MapGenerationConstraint &constraint) {
                                           return constraint.featureKind == MapGeneratedFeatureKind::Settlement;
                                       })));
    map.generation.recipe.landmarkCount =
        std::max(map.generation.recipe.landmarkCount,
                 static_cast<int>(std::count_if(effectiveConstraints.begin(), effectiveConstraints.end(),
                                                [](const MapGenerationConstraint &constraint) {
                                                    return constraint.featureKind == MapGeneratedFeatureKind::Landmark;
                                                })));
}

void MapGenerationOverworldBuilder::placeSettlements()
{
    mapGenReportProgress(options, 4, totalSteps, "Placing settlements");
    settlements.clear();
    settlementConstraints.clear();
    for (const MapGeneratedFeature &feature : options.preservedFeatures)
    {
        if (feature.kind != MapGeneratedFeatureKind::Settlement || feature.points.empty())
            continue;
        const MapCellCoordinate inherited{
            std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, map.width - 1),
            std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, map.height - 1)};
        if (std::find(settlements.begin(), settlements.end(), inherited) == settlements.end())
        {
            settlements.push_back(inherited);
            settlementConstraints.push_back(nullptr);
        }
    }
    const std::size_t preservedSettlementCount = settlements.size();
    int requestedSettlementTarget = map.generation.recipe.settlementCount;
    if (options.generatorVersion >= 5 && options.scale == MapLevel::World && options.nationNames.size() >= 3)
    {
        // One icon per nation only restates the prompt and leaves the road
        // graph as the same sparse two-edge V on every world. Add one
        // generated secondary centre so the editor fulfils its promise to
        // complete an underspecified setting while preserving every named
        // place as the higher-priority locked fact.
        requestedSettlementTarget =
            std::max(requestedSettlementTarget, static_cast<int>(options.nationNames.size()) + 1);
        map.generation.recipe.settlementCount = requestedSettlementTarget;
    }
    const int settlementTarget = std::clamp(requestedSettlementTarget, 0, std::max(1, map.width * map.height / 80));
    for (const MapGenerationConstraint &constraint : effectiveConstraints)
    {
        if (constraint.featureKind != MapGeneratedFeatureKind::Settlement)
            continue;
        MapCellCoordinate requested{std::clamp(static_cast<int>(std::floor(constraint.x)), 1, map.width - 2),
                                    std::clamp(static_cast<int>(std::floor(constraint.y)), 1, map.height - 2)};
        MapCellCoordinate fixed{-1, -1};
        const std::string requiredNation = options.generatorVersion >= 11
                                               ? mapGenV11AuthoredSettlementNationName(options.facts, constraint.label)
                                               : std::string{};
        const auto allowedTerritory = [&](int x, int y) {
            if (requiredNation.empty())
                return true;
            for (const auto &territory : map.generation.graph.features)
                if (territory.kind == MapGeneratedFeatureKind::TerrainRegion && territory.closed &&
                    territory.materialId.rfind("fantasy_neutral.territory.", 0) == 0 &&
                    mapGenV5RelationNameKey(territory.name) == mapGenV5RelationNameKey(requiredNation) &&
                    mapGenV5PointInsideTerritory({x + .5f, y + .5f}, territory))
                    return true;
            return false;
        };
        if (options.generatorVersion >= 5 && (options.scale == MapLevel::World || structuredRegion) &&
            (constraint.id == "brief_capital" || (options.generatorVersion >= 11 && !constraint.locked)))
            fixed = mapGenV5ChooseWorldSettlement(map, settlements, options.seed + 4621u, 0, requiredNation);
        const int maximumRadius = std::max(map.width, map.height);
        for (int radius = 0; radius <= maximumRadius && fixed.x < 0; ++radius)
        {
            for (int offsetY = -radius; offsetY <= radius && fixed.x < 0; ++offsetY)
            {
                for (int offsetX = -radius; offsetX <= radius; ++offsetX)
                {
                    if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                        continue;
                    const int x = requested.x + offsetX;
                    const int y = requested.y + offsetY;
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
                        cell->kind == MapTileKind::Bridge || mapGenRiverInfluenceCell(cell) ||
                        cell->wayKind != MapWayKind::None)
                        continue;
                    if (!allowedTerritory(x, y))
                        continue;
                    fixed = {x, y};
                    break;
                }
            }
        }
        if (fixed.x >= 0 && std::find(settlements.begin(), settlements.end(), fixed) == settlements.end())
        {
            settlements.push_back(fixed);
            settlementConstraints.push_back(&constraint);
        }
    }
    int automaticSettlementIndex = 0;
    static constexpr float sharedSettlementFractions[] = {0.18f, 0.82f, 0.32f, 0.68f, 0.10f, 0.90f, 0.42f, 0.58f};
    while (static_cast<int>(settlements.size()) < settlementTarget)
    {
        const float sharedFraction =
            sharedSettlementFractions[static_cast<std::size_t>(automaticSettlementIndex) %
                                      (sizeof(sharedSettlementFractions) / sizeof(sharedSettlementFractions[0]))];
        MapCellCoordinate chosen =
            options.generatorVersion >= 5 && (options.scale == MapLevel::World || structuredRegion)
                ? mapGenV5ChooseWorldSettlement(map, settlements, options.seed, automaticSettlementIndex)
                : (sharedGreatRiver
                       ? mapGenV4ChooseSharedRiverSettlement(map, river, settlements, automaticSettlementIndex,
                                                             settlementTarget, options.seed, sharedFraction)
                       : mapGenV2ChooseSettlement(map, settlements, rng));
        if (chosen.x < 0 && sharedGreatRiver)
            chosen = mapGenV2ChooseSettlement(map, settlements, rng);
        if (chosen.x < 0)
            break;
        settlements.push_back(chosen);
        settlementConstraints.push_back(nullptr);
        ++automaticSettlementIndex;
    }
    for (int i = 0; i < static_cast<int>(settlements.size()); ++i)
    {
        if (static_cast<std::size_t>(i) < preservedSettlementCount)
            continue;
        mapGenV2PaintSettlement(map, settlements[static_cast<std::size_t>(i)], i, rng,
                                options.scale == MapLevel::Site || options.scale == MapLevel::Tactical);
        if (i < static_cast<int>(settlementConstraints.size()) && settlementConstraints[static_cast<std::size_t>(i)] &&
            !map.generation.graph.features.empty())
        {
            const MapGenerationConstraint &constraint = *settlementConstraints[static_cast<std::size_t>(i)];
            MapGeneratedFeature &feature = map.generation.graph.features.back();
            feature.id = constraint.id.empty() ? feature.id : constraint.id;
            feature.name = constraint.label.empty() ? feature.name : constraint.label;
            feature.locked = constraint.locked;
        }
    }
}
