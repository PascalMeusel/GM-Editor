#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
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


void mapGenEnsureGenerationMetadata(MapModel &map, const MapGenerationOptions &options)
{
    if (options.generatorVersion >= 11)
    {
        map.generation.recipe.waterLevel = options.waterLevel;
        map.generation.recipe.mountainDensity = options.mountainDensity;
        map.generation.recipe.forestDensity = options.forestDensity;
        map.generation.recipe.settlementDensity = options.settlementDensity;
        map.generation.recipe.routeDensity = options.routeDensity;
        map.generation.recipe.tacticalDensity = options.tacticalDensity;
    }
    if (map.generation.enabled)
        return;
    map.generation.enabled = true;
    map.generation.continuousRendering =
        options.generatorVersion >= 4 || (options.generatorVersion >= 2 && options.scope != MapScope::Dungeon);
    map.generation.recipe.landform = options.landform;
    if (options.generatorVersion >= 11)
    {
        map.generation.recipe.waterLevel = options.waterLevel;
        map.generation.recipe.mountainDensity = options.mountainDensity;
        map.generation.recipe.forestDensity = options.forestDensity;
        map.generation.recipe.settlementDensity = options.settlementDensity;
        map.generation.recipe.routeDensity = options.routeDensity;
        map.generation.recipe.tacticalDensity = options.tacticalDensity;
    }
    map.generation.recipe.generatorVersion = std::max(1, options.generatorVersion);
    map.generation.recipe.recipeVersion = std::max(1, options.recipeVersion);
    map.generation.recipe.recipeId =
        options.caveNetwork ? "cave_network" : (options.scope == MapScope::Dungeon ? "stone_dungeon" : "generated_map");
    map.generation.recipe.styleId = options.styleId;
    const bool illustratedOverview =
        options.styleId == "illustrated_atlas" &&
        (options.generatorVersion >= 12 || options.scale == MapLevel::World || options.scale == MapLevel::Region);
    map.generation.recipe.styleVersion = illustratedOverview             ? (options.generatorVersion >= 12   ? 6
                                                                            : options.generatorVersion >= 10 ? 5
                                                                            : options.generatorVersion >= 9  ? 4
                                                                            : options.generatorVersion >= 8  ? 3
                                                                            : options.generatorVersion >= 7  ? 2
                                                                                                             : 1)
                                         : options.generatorVersion >= 4 ? std::max(3, options.styleVersion)
                                                                         : 2;
    map.generation.recipe.styleSnapshotId =
        illustratedOverview ? (options.generatorVersion >= 12   ? "illustrated_atlas_v6"
                               : options.generatorVersion >= 10 ? "illustrated_atlas_v5"
                               : options.generatorVersion >= 9  ? "illustrated_atlas_v4"
                               : options.generatorVersion >= 8  ? "illustrated_atlas_v3"
                               : options.generatorVersion >= 7  ? "illustrated_atlas_v2"
                                                                : "illustrated_atlas_v1")
        : options.generatorVersion >= 4
            ? (options.styleSnapshotId.empty() ? mapStyleCurrentProductionSnapshotId() : options.styleSnapshotId)
            : "";
    map.generation.recipe.scale = options.scale;
    map.generation.recipe.creationIntent = options.creationIntent;
    map.generation.recipe.sizePreset = options.sizePreset;
    map.generation.recipe.contentLanguage = options.contentLanguage;
    map.generation.recipe.nameCulture = options.nameCulture;
    map.generation.recipe.allowAdditionalNations = options.allowAdditionalNations;
    map.generation.recipe.purpose = options.purpose;
    map.generation.recipe.landscape = options.landscape;
    map.generation.recipe.quality = options.quality;
    map.generation.recipe.seed = options.seed;
    map.generation.recipe.settlementCount = std::max(0, options.requestedSettlements);
    map.generation.recipe.landmarkCount = std::max(0, options.requestedLandmarks);
    map.generation.recipe.edgeConnectionCount = std::max(0, options.edgeConnectionCount);
    map.generation.recipe.knownFacts = options.knownFacts;
    map.generation.recipe.requiredElements = options.requiredElements;
    map.generation.recipe.optionalElements = options.optionalElements;
    map.generation.recipe.excludedElements = options.excludedElements;
    map.generation.recipe.lockedFeatureIds = options.lockedFeatureIds;
    map.generation.recipe.constraints = options.constraints;
    map.generation.recipe.facts = options.facts;
    map.generation.graph.schemaVersion = 2;
}

bool mapGenFeatureSatisfiesRequirement(const MapFeatureGraph &graph, const std::string &id, const std::string &label)
{
    return std::any_of(graph.features.begin(), graph.features.end(), [&](const MapGeneratedFeature &feature) {
        return (!id.empty() && feature.id == id) || (!label.empty() && feature.name == label);
    });
}

bool mapGenPlaceRequiredFeature(MapModel &map, const MapGenerationOptions &options, const std::string &id,
                                const std::string &label, MapGeneratedFeatureKind kind, float requestedX,
                                float requestedY, bool locked, int salt)
{
    if (mapGenFeatureSatisfiesRequirement(map.generation.graph, id, label))
        return true;
    const int originX = std::clamp(static_cast<int>(std::round(requestedX)), 1, std::max(1, map.width - 2));
    const int originY = std::clamp(static_cast<int>(std::round(requestedY)), 1, std::max(1, map.height - 2));
    MapCellCoordinate target{-1, -1};
    const int maximumRadius = std::max(map.width, map.height);
    for (int radius = 0; radius <= maximumRadius && target.x < 0; ++radius)
    {
        for (int offsetY = -radius; offsetY <= radius && target.x < 0; ++offsetY)
        {
            for (int offsetX = -radius; offsetX <= radius; ++offsetX)
            {
                if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                    continue;
                const int x = originX + offsetX;
                const int y = originY + offsetY;
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || cell->movementCost >= 99 || cell->kind == MapTileKind::Water ||
                    cell->kind == MapTileKind::Bridge || cell->kind == MapTileKind::Building ||
                    cell->kind == MapTileKind::Pit || cell->kind == MapTileKind::Chasm ||
                    cell->kind == MapTileKind::Lava || mapGenRiverInfluenceCell(cell) ||
                    cell->wayKind != MapWayKind::None || mapGenIsStructureLike(cell->kind))
                    continue;
                target = {x, y};
                break;
            }
        }
    }
    // A required campaign fact must not disappear merely because an extreme
    // world recipe leaves only steep (movement-blocking) land around it.
    // Prefer ordinary traversable ground above, then allow a non-water,
    // non-route land cell and turn that single cell into the requested
    // landmark/settlement. Rivers, roads and existing structures stay intact.
    for (int radius = 0; radius <= maximumRadius && target.x < 0; ++radius)
    {
        for (int offsetY = -radius; offsetY <= radius && target.x < 0; ++offsetY)
        {
            for (int offsetX = -radius; offsetX <= radius; ++offsetX)
            {
                if (radius > 0 && std::abs(offsetX) != radius && std::abs(offsetY) != radius)
                    continue;
                const int x = originX + offsetX;
                const int y = originY + offsetY;
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Bridge ||
                    cell->kind == MapTileKind::Building || cell->kind == MapTileKind::Pit ||
                    cell->kind == MapTileKind::Chasm || cell->kind == MapTileKind::Lava ||
                    mapGenRiverInfluenceCell(cell) || cell->wayKind != MapWayKind::None ||
                    mapGenIsStructureLike(cell->kind))
                    continue;
                target = {x, y};
                break;
            }
        }
    }
    if (target.x < 0)
        return false;
    const MapTileKind tileKind =
        kind == MapGeneratedFeatureKind::Settlement ? MapTileKind::Settlement : MapTileKind::Landmark;
    mapGenSetCell(map, target.x, target.y, tileKind);
    MapGeneratedFeature feature;
    feature.id = id.empty() ? mapGenV2FeatureId(options.seed, "required", salt) : id;
    feature.kind = kind;
    feature.name = label.empty() ? "Required feature" : label;
    feature.materialId = kind == MapGeneratedFeatureKind::Settlement ? "fantasy_neutral.settlement.required"
                                                                     : "fantasy_neutral.landmark.required";
    feature.locked = locked;
    feature.points.push_back({target.x + 0.5f, target.y + 0.5f});
    map.generation.graph.features.push_back(std::move(feature));
    return true;
}

void mapGenApplyRequiredFeatures(MapModel &map, const MapGenerationOptions &options)
{
    int salt = 0;
    for (const MapGenerationConstraint &constraint : options.constraints)
    {
        if (!constraint.required || mapGenIsAtlasBoundaryConstraint(constraint))
            continue;
        // The legacy placement hook can only place point features. Treating a
        // line endpoint as a generic landmark corrupts inherited rivers and
        // roads; those are installed through preservedFeatures instead.
        if (constraint.featureKind != MapGeneratedFeatureKind::Settlement &&
            constraint.featureKind != MapGeneratedFeatureKind::Landmark)
            continue;
        mapGenPlaceRequiredFeature(map, options, constraint.id, constraint.label, constraint.featureKind, constraint.x,
                                   constraint.y, constraint.locked, salt++);
    }
    for (std::size_t index = 0; index < options.requiredElements.size(); ++index)
    {
        const std::string &label = options.requiredElements[index];
        if (label.empty() || mapGenFeatureSatisfiesRequirement(map.generation.graph, {}, label))
            continue;
        const float fraction = static_cast<float>(index + 1) / static_cast<float>(options.requiredElements.size() + 1);
        mapGenPlaceRequiredFeature(
            map, options, "required_element_" + std::to_string(index), label, MapGeneratedFeatureKind::Landmark,
            2.0f + fraction * std::max(1, map.width - 4),
            static_cast<float>(map.height) * (0.38f + 0.19f * static_cast<float>((index + 1) % 2)), true, salt++);
    }
}

void mapGenApplyFactsToOptions(MapGenerationOptions &options)
{
    const auto addUnique = [](std::vector<std::string> &values, const std::string &value) {
        if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end())
            values.push_back(value);
    };
    for (const MapGenerationFact &fact : options.facts)
    {
        const std::string value = !fact.value.empty() ? fact.value : fact.label;
        const std::string key = mapGenSemanticElementKey(value);
        if (fact.type == MapGenerationFactType::Nation)
        {
            const std::string nationName = !fact.label.empty() ? fact.label : fact.value;
            const std::string normalizedNation = mapGenNormalizedElementText(nationName);
            if (!nationName.empty() &&
                std::none_of(options.nationNames.begin(), options.nationNames.end(), [&](const std::string &existing) {
                    return mapGenNormalizedElementText(existing) == normalizedNation;
                }))
                options.nationNames.push_back(nationName);
        }
        if (fact.type == MapGenerationFactType::ExcludedElement)
        {
            addUnique(options.excludedElements, key.empty() ? value : key);
            if (key == "river")
                options.waterLevel = 0;
            else if (key == "road")
            {
                options.routeDensity = 0;
                options.roadNetwork = MapRoadNetworkMode::None;
            }
            else if (key == "forest")
                options.forestDensity = 0;
            else if (key == "mountain_range")
                options.mountainDensity = 0;
            else if (key == "lake")
            {
                if (options.landscape == MapLandscapeProfile::LakeCountry)
                    options.landscape = MapLandscapeProfile::RiverValley;
                options.waterLevel = std::min(options.waterLevel, 60);
            }
            continue;
        }
        if (fact.strength == MapGenerationFactStrength::Automatic)
            continue;
        const bool required = fact.strength == MapGenerationFactStrength::Must;
        if (fact.type == MapGenerationFactType::Forest || key == "forest")
            options.forestDensity = std::max(options.forestDensity, required ? 58 : 42);
        else if (fact.type == MapGenerationFactType::MountainRange || key == "mountain_range")
        {
            options.mountainDensity = std::max(options.mountainDensity, required ? 58 : 42);
            if (required)
                options.mountainPattern = MapMountainPattern::Ranges;
        }
        else if (fact.type == MapGenerationFactType::River || fact.type == MapGenerationFactType::SharedRiver ||
                 key == "river")
            options.waterLevel = std::max(options.waterLevel, required ? 38 : 20);
        else if (key == "lake")
        {
            options.waterLevel = std::max(options.waterLevel, required ? 72 : 62);
            options.landscape = MapLandscapeProfile::LakeCountry;
        }
        else if (fact.type == MapGenerationFactType::Road || key == "road")
        {
            options.routeDensity = std::max(options.routeDensity, required ? 38 : 24);
            if (options.roadNetwork == MapRoadNetworkMode::None)
                options.roadNetwork = required ? MapRoadNetworkMode::Connected : MapRoadNetworkMode::Sparse;
        }
    }
}

bool mapGenApplyExcludedElements(MapGenerationResult &result, const MapGenerationOptions &options)
{
    if (options.excludedElements.empty())
        return false;
    bool changed = false;
    auto &features = result.map.generation.graph.features;
    const auto firstRemoved = std::remove_if(features.begin(), features.end(), [&](const MapGeneratedFeature &feature) {
        bool excluded = mapGenExcludedElementMatches(feature.name, options.excludedElements) ||
                        mapGenExcludedElementMatches(feature.materialId, options.excludedElements);
        for (const std::string &entry : options.excludedElements)
        {
            const std::string key = mapGenCanonicalExcludedSemanticKey(entry);
            excluded = excluded || (key == "river" && feature.kind == MapGeneratedFeatureKind::River) ||
                       (key == "road" && (feature.kind == MapGeneratedFeatureKind::Road ||
                                          feature.kind == MapGeneratedFeatureKind::Trail)) ||
                       (key == "forest" && feature.kind == MapGeneratedFeatureKind::Vegetation) ||
                       (key == "mountain_range" && feature.kind == MapGeneratedFeatureKind::MountainRange) ||
                       ((key == "water" || key == "sea" || key == "coast") &&
                        (feature.kind == MapGeneratedFeatureKind::WaterBody ||
                         feature.kind == MapGeneratedFeatureKind::Coastline));
        }
        changed = changed || excluded;
        return excluded;
    });
    features.erase(firstRemoved, features.end());

    for (int y = 0; y < result.map.height; ++y)
    {
        for (int x = 0; x < result.map.width; ++x)
        {
            MapCellModel &cell = result.map.cells[static_cast<std::size_t>(mapCellIndex(result.map, x, y))];
            const bool excludedWay =
                cell.wayKind != MapWayKind::None &&
                mapGenExcludedElementMatches(mapWayKindLabel(cell.wayKind), options.excludedElements);
            const bool excludedTile =
                mapGenExcludedElementMatches(mapTileKindLabel(cell.kind), options.excludedElements);
            std::string excludedSemanticKey;
            for (const std::string &entry : options.excludedElements)
            {
                const std::string key = mapGenCanonicalExcludedSemanticKey(entry);
                if (!key.empty() && mapGenCellHasSemantic(cell, key))
                {
                    excludedSemanticKey = key;
                    break;
                }
            }
            const bool excludedSemantic = !excludedSemanticKey.empty();
            const bool excludedWaterTerrain =
                cell.kind == MapTileKind::Water &&
                std::any_of(
                    options.excludedElements.begin(), options.excludedElements.end(), [&](const std::string &entry) {
                        const std::string key = mapGenCanonicalExcludedSemanticKey(entry);
                        return key == "river" || key == "water" || key == "sea" || key == "lake" || key == "coast";
                    });
            if (!excludedWay && !excludedTile && !excludedSemantic)
                continue;
            changed = true;

            // Removing a road family from a river crossing must reveal the
            // river underneath, not punch an empty zero-elevation hole into
            // the hydrology path.
            if (cell.kind == MapTileKind::Bridge && (excludedSemanticKey == "road" || excludedWay || excludedTile))
            {
                const int previousElevation = cell.elevation;
                const bool onRiver =
                    std::any_of(features.begin(), features.end(), [&](const MapGeneratedFeature &feature) {
                        if (feature.kind != MapGeneratedFeatureKind::River)
                            return false;
                        return std::any_of(feature.points.begin(), feature.points.end(),
                                           [&](const MapFeaturePoint &point) {
                                               return static_cast<int>(std::floor(point.x)) == x &&
                                                      static_cast<int>(std::floor(point.y)) == y;
                                           });
                    });
                applyMapCellTerrainDefaults(cell, onRiver ? MapTileKind::Water : MapTileKind::Empty);
                cell.elevation = previousElevation;
                cell.wayKind = onRiver ? MapWayKind::River : MapWayKind::None;
                continue;
            }
            if (excludedTile || excludedWaterTerrain || cell.kind == MapTileKind::Road ||
                cell.kind == MapTileKind::Bridge || (cell.kind == MapTileKind::Water && excludedWay))
                applyMapCellTerrainDefaults(cell, MapTileKind::Empty);
            if (excludedWay || excludedTile || excludedSemantic)
                cell.wayKind = MapWayKind::None;
        }
    }
    if (changed)
        result.warnings.push_back("Excluded recipe elements were removed before quality validation.");
    return changed;
}
