#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapTravelInk.hpp"
#include "MapRenderPlan.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapFeaturePresentationGeometry.hpp"

#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapRoutePresentation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "Models/Map/MapRenderPlanInternal.hpp"

using namespace MapRenderPlanInternal;

std::uint32_t mapPlayerReleasedRenderLayers(const MapModel &map)
{
    std::uint32_t layers = MapRenderPlanBackground | MapRenderPlanTerrain |
                           MapRenderPlanWater | MapRenderPlanFog;
    if (map.playerRelease.politicalBorders)
        layers |= MapRenderPlanPolitical;
    if (map.playerRelease.waysAndRoutes)
        layers |= MapRenderPlanWays | MapRenderPlanRoutes;
    if (map.playerRelease.placesAndMarkers)
        layers |= MapRenderPlanSymbols;
    if (map.playerRelease.labels)
        layers |= MapRenderPlanLabels;
    if (map.playerRelease.tokensAndAnnotations)
        layers |= MapRenderPlanTokens | MapRenderPlanAnnotations;
    if (map.atlasLevel == MapLevel::Tactical && map.gridType == GridType::Square)
        layers |= MapRenderPlanGrid;
    return layers;
}

MapModel filterMapForRenderPlan(const MapModel &map, const MapRenderPlan &plan)
{
    MapModel filtered = map;
    if (plan.audience == MapRenderAudience::Player)
        suppressPlayerRasterLinearProjections(filtered);
    suppressUnreviewedPointProjections(filtered, map, plan);
    suppressUnreviewedMountainProjections(filtered, map, plan);
    const auto removeMissing = [](auto &values, const auto &ids, const auto &idOf) {
        values.erase(std::remove_if(values.begin(), values.end(), [&](const auto &value) {
            return std::find(ids.begin(), ids.end(), idOf(value)) == ids.end();
        }), values.end());
    };
    removeMissing(filtered.generation.graph.features, plan.generatedFeatureIds,
                  [](const MapGeneratedFeature &feature) { return feature.id; });
    removeMissing(filtered.features, plan.manualFeatureIds,
                  [](const MapFeatureModel &feature) { return feature.metadata.id; });
    removeMissing(filtered.regions, plan.regionIds,
                  [](const MapRegionModel &region) { return region.metadata.id; });
    removeMissing(filtered.routes, plan.routeIds,
                  [](const MapRouteModel &route) { return route.metadata.id; });
    removeMissing(filtered.tokens, plan.tokenIds,
                  [](const MapTokenModel &token) { return token.metadata.id; });
    const bool waysEnabled = (plan.layers & MapRenderPlanWays) != 0;
    const bool symbolsEnabled = (plan.layers & MapRenderPlanSymbols) != 0;
    for (MapCellModel &cell : filtered.cells)
    {
        // Roads and bridges are synchronized raster projections of the
        // feature graph. Strip them for every audience when the reviewed plan
        // omits Ways; otherwise a GM labels-only PNG can leak a brown road
        // through the terrain material even though no road primitive exists.
        if (!waysEnabled)
        {
            if (cell.wayKind == MapWayKind::Trail ||
                cell.wayKind == MapWayKind::DirtRoad ||
                cell.wayKind == MapWayKind::StoneRoad)
                cell.wayKind = MapWayKind::None;
            if (cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Bridge)
            {
                cell.kind = MapTileKind::Empty;
                cell.wayKind = MapWayKind::None;
            }
        }
        if (!symbolsEnabled &&
            (cell.kind == MapTileKind::Settlement ||
             cell.kind == MapTileKind::Building ||
             cell.kind == MapTileKind::Landmark))
        {
            cell.kind = cell.kind==MapTileKind::Building?suppressedBuildingUnderlay(map):MapTileKind::Empty;
            cell.wayKind = MapWayKind::None;
        }
    }
    if (plan.audience == MapRenderAudience::Player &&
        (plan.layers & MapRenderPlanLabels) == 0)
    {
        filtered.name.clear();
        for (MapGeneratedFeature &feature : filtered.generation.graph.features)
            feature.name.clear();
        for (MapFeatureModel &feature : filtered.features)
            feature.name.clear();
        // Tactical region names currently carry their semantic subtype
        // (Party Start, Objective, ...). Render primitives keep their label
        // empty while this internal classifier remains available to backends.
        for (MapRouteModel &route : filtered.routes)
            route.name.clear();
        for (MapTokenModel &token : filtered.tokens)
            token.name.clear();
    }
    if (plan.audience == MapRenderAudience::Player)
    {
        for (MapFeatureModel &feature : filtered.features)
        {
            feature.gmNotes.clear();
            feature.linkedEntityId.clear();
            feature.targetMapId.clear();
            feature.difficultyClass = 0;
        }
        for (MapRouteModel &route : filtered.routes)
            route.notes.clear();
        for (MapTokenModel &token : filtered.tokens)
        {
            token.linkedEntityType.clear();
            token.linkedEntityId.clear();
        }
    }
    if (!plan.backgroundVisible)
    {
        filtered.backgroundImagePath.clear();
        filtered.importedReference.enabled = false;
    }
    if (!plan.applyFog)
        for (MapCellModel &cell : filtered.cells) cell.fogged = false;
    return filtered;
}

bool mapRenderPlanContainsGeneratedFeature(const MapRenderPlan &plan, const EntityId &id)
{
    return containsId(plan.generatedFeatureIds, id);
}

bool mapRenderPlanMatchesMap(const MapRenderPlan &plan, const MapModel &map)
{
    return plan.mapId == map.metadata.id && plan.mapRevision == map.revision &&
           plan.level == map.atlasLevel && plan.scope == map.scope &&
           plan.gridType == map.gridType && plan.mapVisibility == map.visibility &&
           plan.styleId == map.generation.recipe.styleId &&
           plan.styleSnapshotId == map.generation.recipe.styleSnapshotId &&
           plan.styleVersion == map.generation.recipe.styleVersion &&
           plan.generationSeed == map.generation.recipe.seed &&
           plan.generatorVersion == map.generation.recipe.generatorVersion &&
           plan.recipeVersion == map.generation.recipe.recipeVersion &&
           (!gm::map_render::usesIllustratedAtlasDetailed(map) || plan.terrainLandscape == map.generation.recipe.landscape) &&
           plan.continuousRendering == map.generation.continuousRendering &&
           (plan.audience != MapRenderAudience::Player ||
            plan.playerReleaseLayers == mapPlayerReleasedRenderLayers(map));
}

bool mapRenderPlanContentMatchesMap(const MapRenderPlan &plan, const MapModel &map)
{
    if (!mapRenderPlanMatchesMap(plan, map))
        return false;

    MapRenderPlanRequest request;
    request.audience = plan.audience;
    request.locale = plan.locale;
    request.layers = plan.layers;
    request.applyFog = plan.applyFog;
    request.showPoliticalOverlay = plan.showPoliticalOverlay;
    request.showGrid = plan.showGrid;
    // Recreate the already reviewed generated-feature allow-list. This keeps
    // a campaign-level visibility predicate fail-closed without requiring the
    // exporter to retain an arbitrary callback past review time.
    request.additionalGeneratedFeatureVisibility = [&plan](const MapGeneratedFeature &feature) {
        return planContainsId(plan.generatedFeatureIds, feature.id);
    };
    const MapRenderPlan current = compileMapRenderPlan(map, request);
    return current.contentHash == plan.contentHash;
}

std::uint64_t mapRenderPlanCacheKey(const MapRenderPlan &plan,
                                    int lod,
                                    int chunkX,
                                    int chunkY)
{
    std::uint64_t hash = 14695981039346656037ull;
    hashString(hash, plan.mapId);
    hashValue(hash, plan.mapRevision);
    hashValue(hash, plan.level);
    hashValue(hash, plan.scope);
    hashValue(hash, plan.gridType);
    hashValue(hash, plan.contentHash);
    hashValue(hash, plan.mapVisibility);
    hashValue(hash, plan.audience);
    hashString(hash, plan.locale);
    hashString(hash, plan.styleId);
    hashString(hash, plan.styleSnapshotId);
    hashValue(hash, plan.styleVersion);
    hashValue(hash, plan.generationSeed);
    hashValue(hash, plan.generatorVersion);
    if (plan.recipeVersion >= 4) hashValue(hash, plan.recipeVersion);
    hashValue(hash, plan.continuousRendering);
    hashValue(hash, plan.layers);
    hashValue(hash, plan.playerReleaseLayers);
    hashValue(hash, plan.applyFog);
    hashValue(hash, plan.showPoliticalOverlay);
    hashValue(hash, plan.showGrid);
    hashValue(hash, lod);
    hashValue(hash, chunkX);
    hashValue(hash, chunkY);
    return hash;
}
