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

MapGenerationQualityReport analyzeMapGenerationQuality(const MapModel &map)
{
    MapGenerationQualityReport report;
    const int traversableCount =
        mapGenCountMatchingCells(map, [](const MapCellModel &cell) { return mapGenQualityTraversable(cell); });
    const int largestTraversable =
        mapGenLargestComponentSize(map, [](const MapCellModel &cell) { return mapGenQualityTraversable(cell); }, true);
    report.traversableConnectivityPercent = traversableCount == 0 ? 0 : largestTraversable * 100 / traversableCount;

    const int wayCount =
        mapGenCountMatchingCells(map, [&](const MapCellModel &cell) { return mapGenQualityWayCell(cell, map.scope); });
    const int largestWay = mapGenLargestComponentSize(
        map, [&](const MapCellModel &cell) { return mapGenQualityWayCell(cell, map.scope); }, true);
    report.wayConnectivityPercent = wayCount == 0 ? 100 : largestWay * 100 / wayCount;
    const bool regionalIslands = map.generation.recipe.generatorVersion >= 11 && map.atlasLevel == MapLevel::Region &&
                                 map.generation.recipe.landform == MapLandform::Islands;
    if (map.generation.continuousRendering && (map.atlasLevel == MapLevel::World || regionalIslands))
    {
        // Separate continents intentionally have independent road networks.
        // At world scale, require every published road feature itself to be
        // continuous rather than treating an ocean between two valid national
        // networks as a broken road.
        int roadFeatures = 0;
        int continuousRoadFeatures = 0;
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail)
                continue;
            ++roadFeatures;
            bool continuous = feature.points.size() >= 2;
            for (std::size_t index = 1; continuous && index < feature.points.size(); ++index)
            {
                const float deltaX = feature.points[index].x - feature.points[index - 1].x;
                const float deltaY = feature.points[index].y - feature.points[index - 1].y;
                continuous = std::sqrt(deltaX * deltaX + deltaY * deltaY) <= 1.6f;
            }
            if (continuous)
                ++continuousRoadFeatures;
        }
        report.wayConnectivityPercent = roadFeatures == 0 ? 100 : continuousRoadFeatures * 100 / roadFeatures;
        if (map.generation.recipe.generatorVersion >= 5)
            report.wayConnectivityPercent =
                std::min(report.wayConnectivityPercent,
                         std::min(mapGenV5WorldRoadNetworkPercent(map), mapGenV5WorldRoadTerrainPercent(map)));
    }

    int structures = 0;
    int accessibleStructures = 0;
    for (int y = 0; y < map.height; ++y)
    {
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !mapGenIsStructureLike(cell->kind))
                continue;
            // Outdoor V2 treats clustered buildings and remote landmarks as
            // composition features. Only the settlement anchor must connect
            // to the generated travel network; individual stamps must not
            // attract a road through their footprint.
            if (map.generation.enabled && cell->kind != MapTileKind::Settlement)
                continue;
            ++structures;
            if (mapGenCellHasStructureApproach(map, x, y))
                ++accessibleStructures;
        }
    }
    report.structureAccessPercent = structures == 0 ? 100 : accessibleStructures * 100 / structures;
    report.coherentTerrainPercent = mapGenCoherentTerrainPercent(map);
    report.roadEfficiencyPercent = mapGenV2RoadEfficiencyPercent(map);
    report.hydrologyPercent = mapGenV2HydrologyPercent(map);
    report.bridgeValidityPercent = mapGenV2BridgeValidityPercent(map);
    report.settlementSuitabilityPercent = mapGenV2SettlementSuitabilityPercent(map);
    report.compositionPercent = mapGenV2CompositionPercent(map);
    report.visualHierarchyPercent = mapGenV4VisualHierarchyPercent(map);
    report.scaleCorrectnessPercent = mapGenV4ScaleCorrectnessPercent(map);
    if (map.generation.continuousRendering &&
        (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region ||
         (map.generation.recipe.generatorVersion >= 12 && map.generation.recipe.styleVersion == 6)) &&
        map.generation.recipe.styleId == "illustrated_atlas" &&
        ((map.generation.recipe.styleVersion == 1 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v1") ||
         (map.generation.recipe.styleVersion == 2 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v2") ||
         (map.generation.recipe.styleVersion == 3 && map.generation.recipe.styleSnapshotId == "illustrated_atlas_v3") ||
         ((map.generation.recipe.styleVersion == 4 &&
           map.generation.recipe.styleSnapshotId == "illustrated_atlas_v4") ||
          ((map.generation.recipe.styleVersion == 5 &&
            map.generation.recipe.styleSnapshotId == "illustrated_atlas_v5") ||
           (map.generation.recipe.styleVersion == 6 &&
            map.generation.recipe.styleSnapshotId == "illustrated_atlas_v6")))))
        // This exact procedural style supplies its complete primitive set.
        // Coverage measures renderer availability, not human art acceptance.
        report.assetCoveragePercent = 100;
    else if (map.generation.continuousRendering && map.generation.recipe.styleVersion >= 3)
        report.assetCoveragePercent = mapGenV4AssetCoveragePercent(map);
    else if (map.generation.continuousRendering)
        report.assetCoveragePercent = 75;
    else
    {
        const int targets = mapStyleContextualTargetAssetCount(defaultMapStyleId());
        const int accepted = mapStyleContextualAcceptedAssetCount(defaultMapStyleId());
        report.assetCoveragePercent = targets <= 0 ? 100 : std::clamp(accepted * 100 / targets, 0, 100);
    }

    report.criticalRouteConnected = true;
    if (!map.routes.empty())
    {
        const MapRouteModel &route = map.routes.front();
        report.criticalRouteConnected =
            mapGenCoordinatesConnected(map, {route.startX, route.startY}, {route.endX, route.endY}, true);
    }
    else if (map.scope == MapScope::Dungeon)
    {
        std::vector<MapCellCoordinate> stairs;
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (cell && cell->kind == MapTileKind::Stairs)
                    stairs.push_back({x, y});
            }
        }
        if (stairs.size() >= 2)
            report.criticalRouteConnected = mapGenCoordinatesConnected(map, stairs.front(), stairs.back(), false);
    }

    // Connectivity and access dominate because a beautiful but unusable map is
    // not a viable session map. Coherence breaks ties between playable maps.
    if (map.scope == MapScope::Dungeon)
    {
        report.score = report.traversableConnectivityPercent * 45 / 100 + report.wayConnectivityPercent * 20 / 100 +
                       report.coherentTerrainPercent * 10 / 100 + (report.criticalRouteConnected ? 25 : 0);
    }
    else if (!map.generation.continuousRendering)
    {
        report.score = report.traversableConnectivityPercent * 25 / 100 + report.wayConnectivityPercent * 30 / 100 +
                       report.structureAccessPercent * 25 / 100 + report.coherentTerrainPercent * 10 / 100 +
                       (report.criticalRouteConnected ? 10 : 0);
    }
    else
    {
        report.score = report.traversableConnectivityPercent * 12 / 100 + report.wayConnectivityPercent * 16 / 100 +
                       report.structureAccessPercent * 12 / 100 + report.coherentTerrainPercent * 8 / 100 +
                       report.roadEfficiencyPercent * 10 / 100 + report.hydrologyPercent * 10 / 100 +
                       report.bridgeValidityPercent * 9 / 100 + report.settlementSuitabilityPercent * 8 / 100 +
                       report.compositionPercent * 3 / 100 + report.visualHierarchyPercent * 3 / 100 +
                       report.scaleCorrectnessPercent * 2 / 100 + report.assetCoveragePercent * 3 / 100 +
                       (report.criticalRouteConnected ? 4 : 0);
    }
    auto addIssue = [&](const char *code, MapGenerationIssueSeverity severity, const char *message,
                        const char *action) {
        report.issues.push_back(MapGenerationIssue{code, severity, message, action});
        if (severity == MapGenerationIssueSeverity::Critical)
            ++report.criticalIssueCount;
    };
    if (map.generation.recipe.generatorVersion >= 10 && map.atlasLevel == MapLevel::World)
    {
        const int expected =
            std::clamp(map.generation.recipe.settlementCount, 0, std::max(1, map.width * map.height / 80));
        const int actual = static_cast<int>(std::count_if(
            map.generation.graph.features.begin(), map.generation.graph.features.end(), [](const auto &feature) {
                return feature.kind == MapGeneratedFeatureKind::Settlement && !feature.points.empty();
            }));
        if (actual < expected)
            addIssue("settlement.requested_count_missing", MapGenerationIssueSeverity::Critical,
                     "The requested number of places could not be placed on usable land.",
                     "Choose another design, reduce the place count, or make more land usable.");
    }
    if (!report.criticalRouteConnected)
        addIssue("route.disconnected", MapGenerationIssueSeverity::Critical, "The main route is interrupted.",
                 "Regenerate roads or move a route endpoint.");
    if (map.scope != MapScope::World && !regionalIslands && report.traversableConnectivityPercent < 80)
        addIssue("terrain.disconnected", MapGenerationIssueSeverity::Critical,
                 "The usable map surface is split into disconnected areas.",
                 "Regenerate terrain or add a valid crossing.");
    if (report.structureAccessPercent < 100)
        addIssue("settlement.no_access", MapGenerationIssueSeverity::Critical,
                 "At least one structure has no usable approach.",
                 "Connect the highlighted structure to the road network.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldRoadNetworkPercent(map) < 80)
        addIssue("road.world_network_too_small", MapGenerationIssueSeverity::Critical,
                 "The world road network is too short or does not connect the principal places.",
                 "Regenerate principal places and the main road network together.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldRoadTerrainPercent(map) < 80)
        addIssue(
            "road.world_terrain_conflict", MapGenerationIssueSeverity::Critical,
            "A world road runs through open water, follows the shoreline too closely, or overlaps a mountain range.",
            "Regenerate the road inland or create an explicit short bridge, ferry, or mountain pass.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        report.settlementSuitabilityPercent < 80)
        addIssue("settlement.nation_unrepresented", MapGenerationIssueSeverity::Critical,
                 "At least one named nation has no usable principal place.",
                 "Place or regenerate a town in the highlighted nation.");
    if (report.hydrologyPercent < 95)
    {
        if (mapGenV4RequestsSharedGreatRiver(map))
            addIssue(
                "river.shared_requirement", MapGenerationIssueSeverity::Critical,
                "The promised shared great river is not one continuous, prominent waterway through all named nations with a valid outlet.",
                "Regenerate the shared river while keeping the named nation facts locked.");
        else
            addIssue("river.disconnected", MapGenerationIssueSeverity::Critical,
                     "The main river is interrupted or has no valid outlet.",
                     "Regenerate the river while keeping settlements locked.");
    }
    if (report.bridgeValidityPercent < 100)
        addIssue("bridge.invalid", MapGenerationIssueSeverity::Critical,
                 "A bridge does not connect both banks correctly.", "Move the crossing or regenerate bridges.");
    if (report.roadEfficiencyPercent < 70)
        addIssue("road.detour", MapGenerationIssueSeverity::Warning, "The road network contains long detours.",
                 "Use automatic repair to shorten secondary roads.");
    if (report.compositionPercent < 55)
        addIssue("composition.empty", MapGenerationIssueSeverity::Warning,
                 "Large areas have no visual or playable point of interest.",
                 "Add or regenerate landmarks in empty regions.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldTerritoryLandAdjacencyPercent(map) < 80)
        addIssue("territory.crosses_open_ocean", MapGenerationIssueSeverity::Critical,
                 "A nation boundary extends across open ocean instead of following its land territory.",
                 "Rebuild political regions from the generated landmass.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldTerritoryNaturalismPercent(map) < 80)
        addIssue("territory.boundaries_too_geometric",
                 map.generation.recipe.generatorVersion >= 11 ? MapGenerationIssueSeverity::Warning
                                                              : MapGenerationIssueSeverity::Critical,
                 "The nation boundaries are dominated by long geometric rays instead of following the landscape.",
                 "Rebuild political regions along rivers, ridges and coastal bottlenecks.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldSettlementSpreadPercent(map) < 80)
        addIssue("composition.world_settlements_collinear", MapGenerationIssueSeverity::Critical,
                 "The principal places are compressed into one line and leave the nations unreadable.",
                 "Distribute principal places across their nations before rebuilding roads.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldMountainCompositionPercent(map) < 80)
        addIssue("terrain.world_mountains_wrong_scale", MapGenerationIssueSeverity::Critical,
                 "Mountain terrain is missing or occupies an implausibly large part of the world landmass.",
                 "Regenerate mountains as connected overview-scale ranges.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldCoastCompositionPercent(map) < 80)
        addIssue("terrain.world_coast_generic", MapGenerationIssueSeverity::Critical,
                 "The world coastline is too smooth, too fragmented, or lacks readable bays and peninsulas.",
                 "Regenerate the continental silhouette while keeping named places and facts locked.");
    if (map.generation.recipe.generatorVersion >= 5 && map.atlasLevel == MapLevel::World &&
        mapGenV5WorldBiomeCompositionPercent(map) < 80)
        addIssue("terrain.world_biomes_fragmented", MapGenerationIssueSeverity::Critical,
                 "Forest and open-land regions do not form readable overview-scale biomes.",
                 "Regenerate biome regions as a few connected geographic masses.");
    if (report.visualHierarchyPercent < 80)
        addIssue("composition.cluttered", MapGenerationIssueSeverity::Warning,
                 "Too many small details compete with the important map features.",
                 "Reduce stamps or regenerate this map for its current scale.");
    if (report.scaleCorrectnessPercent < 100)
        addIssue("art.wrong_scale", MapGenerationIssueSeverity::Critical,
                 "The map contains details that belong to a closer zoom level.",
                 "Remove tactical details or create a detail map for this area.");
    if (report.assetCoveragePercent < 100)
        addIssue("art.fallback",
                 map.generation.recipe.quality == MapGenerationQualityLevel::Polished
                     ? MapGenerationIssueSeverity::Critical
                     : MapGenerationIssueSeverity::Warning,
                 "Some visual roles still use fallback art.",
                 "Use an accepted production style or review the missing assets.");
    report.score = std::clamp(report.score, 0, 100);
    return report;
}

