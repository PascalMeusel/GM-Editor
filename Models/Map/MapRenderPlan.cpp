#include "Models/Map/MapRenderPlanInternal.hpp"
#include "Models/Map/MapRenderPlanMeasurements.hpp"
#include "Models/Map/MapRenderCompileMemoInternal.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapTravelInk.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapImportedReferencePolicy.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

using namespace MapRenderPlanInternal;

namespace
{
class CompileMeasure
{
    MapRenderPlanMeasurements::Sample *sample_ = nullptr;
    std::chrono::steady_clock::time_point started_{};

  public:
    CompileMeasure(MapRenderPlanMeasurements *sink, MapRenderPlanCompileStage stage)
    {
        if (sink)
        {
            sample_ = &sink->values[static_cast<std::size_t>(stage)];
            started_ = std::chrono::steady_clock::now();
        }
    }
    ~CompileMeasure() { finish(); }
    CompileMeasure(const CompileMeasure &) = delete;
    CompileMeasure &operator=(const CompileMeasure &) = delete;
    void finish()
    {
        if (!sample_)
            return;
        sample_->nanoseconds += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_).count());
        ++sample_->calls;
        sample_ = nullptr;
    }
};

MapRenderPlan compilePlan(const MapModel &map, const MapRenderPlanRequest &request,
                          MapRenderPlanMeasurements *measurements,
                          gm::map_render::MapRenderCompileMemo *memo = nullptr,
                          MapRenderCompileAttempt *cacheOnly = nullptr)
{
    using Stage = MapRenderPlanCompileStage;
    CompileMeasure total(measurements, Stage::Total);
    CompileMeasure selection(measurements, Stage::Selection);
    MapRenderPlan plan;
    plan.mapId = map.metadata.id;
    plan.mapRevision = map.revision;
    plan.level = map.atlasLevel;
    plan.scope = map.scope;
    plan.gridType = map.gridType;
    plan.mapVisibility = map.visibility;
    plan.audience = request.audience;
    plan.locale = request.locale;
    plan.styleSnapshotId = map.generation.recipe.styleSnapshotId;
    plan.styleId = map.generation.recipe.styleId;
    plan.styleVersion = map.generation.recipe.styleVersion;
    plan.recipeVersion = map.generation.recipe.recipeVersion;
    plan.generationSeed = map.generation.recipe.seed;
    plan.generatorVersion = map.generation.recipe.generatorVersion;
    plan.terrainLandscape = map.generation.recipe.landscape;
    plan.continuousRendering = map.generation.continuousRendering;
    // Immutable output plans contain campaign content only. Authoring helpers
    // stay in the interactive view and cannot be smuggled into player/export
    // output through a permissive layer mask.
    plan.layers = request.layers & MapRenderPlanAll;
    if (request.audience == MapRenderAudience::Player)
    {
        plan.playerReleaseLayers = mapPlayerReleasedRenderLayers(map);
        plan.layers &= plan.playerReleaseLayers;
        // Fog is a mandatory safety layer for every player plan, not an
        // optional presentation preference. mapPlayerReleasedRenderLayers()
        // always contains it, so requesting a narrow layer subset cannot
        // accidentally publish an unmasked map.
        plan.layers |= MapRenderPlanFog;
        // An unreleased map has no player drawing plan, even when a caller asks
        // for base geography. Keep the mandatory safety layer but compile no
        // symbols, terrain artwork or decorative coastline from its contents.
        if (map.visibility != MapVisibility::PlayersVisible)
            plan.layers = MapRenderPlanFog;
    }
    plan.applyFog = request.audience == MapRenderAudience::Player || request.applyFog;
    plan.showPoliticalOverlay = request.showPoliticalOverlay &&
                                (plan.layers & MapRenderPlanPolitical) != 0;
    plan.showGrid = request.showGrid && (plan.layers & MapRenderPlanGrid) != 0;
    const bool player = request.audience == MapRenderAudience::Player;
    const bool managedReferencePathsSafe = !map.importedReference.enabled ||
        MapImportedReferencePolicy::hasSafeManagedPaths(map.importedReference);
    plan.backgroundVisible = (plan.layers & MapRenderPlanBackground) != 0 &&
                             managedReferencePathsSafe &&
                             (map.importedReference.enabled
                                  ? (!player || (!map.importedReference.gmOnly &&
                                                 playerMapBackgroundIsFullyRevealed(map)))
                                  // Legacy paths have no persisted publication
                                  // consent or pixel-level safety metadata. They
                                  // remain visible to the GM but fail closed in
                                  // every player/export RenderPlan until adopted
                                  // as a reviewed managed reference.
                                  : (!player && !map.backgroundImagePath.empty()));
    if (plan.backgroundVisible)
    {
        if (map.importedReference.enabled)
        {
            const bool useWorkingProxy =
                map.importedReference.workingProxyGenerated &&
                !map.importedReference.workingProxyProjectRelativePath.empty();
            plan.backgroundInteractiveAssetId = useWorkingProxy
                                                    ? map.importedReference.workingProxyAssetId
                                                    : map.importedReference.assetId;
            plan.backgroundInteractiveProjectRelativePath = useWorkingProxy
                                                                 ? map.importedReference.workingProxyProjectRelativePath
                                                                 : map.importedReference.projectRelativePath;
            plan.backgroundInteractiveContentHash = useWorkingProxy
                                                        ? map.importedReference.workingProxyContentHash
                                                        : map.importedReference.contentHash;
            plan.backgroundExportAssetId = map.importedReference.assetId;
            plan.backgroundExportProjectRelativePath =
                map.importedReference.projectRelativePath;
            plan.backgroundExportContentHash = map.importedReference.contentHash;
        }
        else
        {
            // Unmanaged legacy backgrounds have no separate proxy or durable
            // asset hash. They remain GM-only, but still bind their path into
            // the plan so it cannot change between review and export.
            plan.backgroundInteractiveAssetId = map.backgroundImagePath;
            plan.backgroundInteractiveProjectRelativePath = map.backgroundImagePath;
            plan.backgroundExportAssetId = map.backgroundImagePath;
            plan.backgroundExportProjectRelativePath = map.backgroundImagePath;
        }
    }

    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (generatedFeatureLayerEnabled(feature, plan.layers) &&
            (!player || (!feature.gmOnly && feature.discovered)) &&
            (!player || !generatedFeatureUsesPointProjection(feature.kind) ||
             (!feature.points.empty() &&
              playerMapPositionIsRevealed(map, feature.points.front().x,
                                          feature.points.front().y))) &&
            (!request.additionalGeneratedFeatureVisibility || request.additionalGeneratedFeatureVisibility(feature)))
            plan.generatedFeatureIds.push_back(feature.id);
    if ((plan.layers & MapRenderPlanAnnotations) != 0)
    {
        for (const MapFeatureModel &feature : map.features)
            if (!player || (!feature.gmOnly && feature.discovered &&
                            playerMapCellIsRevealed(map, feature.x, feature.y)))
                plan.manualFeatureIds.push_back(feature.metadata.id);
        for (const MapRegionModel &region : map.regions)
            if (!player || !region.gmOnly)
                plan.regionIds.push_back(region.metadata.id);
    }
    if ((plan.layers & MapRenderPlanRoutes) != 0)
        for (const MapRouteModel &route : map.routes)
            if (!player || route.discovered)
                plan.routeIds.push_back(route.metadata.id);
    if ((plan.layers & MapRenderPlanTokens) != 0)
        for (const MapTokenModel &token : map.tokens)
            if (!player || (!token.hiddenFromPlayers &&
                            playerMapLayerIsVisible(map, token.layerId) &&
                            playerMapPositionIsRevealed(map, token.x, token.y)))
                plan.tokenIds.push_back(token.metadata.id);

    selection.finish();
    using MemoAccess = gm::map_render::MapRenderCompileMemoAccess;
    const auto bypasses = memo ? memo->stats().bypasses : 0;
    auto key = memo ? MemoAccess::key(*memo, map, request, plan)
                    : std::optional<gm::map_render::MapRenderCompileKey>{};
    const bool reused = key && MemoAccess::reuse(*memo, *key, plan);
    if (cacheOnly)
    {
        cacheOnly->status = reused ? MapRenderCompileStatus::Hit
            : (!key || memo->stats().bypasses != bypasses) ? MapRenderCompileStatus::Unavailable
                                                       : MapRenderCompileStatus::Miss;
        if (!reused) return {};
    }
    if (!reused && (gm::map_render::usesIllustratedAtlas(map) || map.generation.recipe.generatorVersion >= 6))
    {
        // Resolve against the reviewed audience, including its road consent.
        // Hidden roads must not leave visible gaps in the player's relief.
        // Filtering uses the selected IDs/layers, not primitives. Build once
        // from that filtered source rather than discard a first unfiltered set.
        CompileMeasure filtering(measurements, Stage::AudienceFilter);
        const MapModel visible = filterMapForRenderPlan(map, plan);
        filtering.finish();
        {
            CompileMeasure measure(measurements, Stage::FilteredPrimitives);
            plan.primitives = buildRenderPrimitives(visible, plan);
        }
        CompileMeasure features(measurements, Stage::FeatureInk);
        plan.atlasFeatureInk = gm::map_render::buildIllustratedAtlasFeatureInk(visible,
            (plan.layers & MapRenderPlanWays) != 0, (plan.layers & MapRenderPlanSymbols) != 0);
        if ((plan.layers & MapRenderPlanSymbols) != 0) {
            auto travel = gm::map_render::buildTravelPurposeInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), travel.begin(), travel.end());
            auto dungeonProps = gm::map_render::buildDungeonPropInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), dungeonProps.begin(), dungeonProps.end());
            auto caveSymbols = gm::map_render::buildNaturalCaveSymbolInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), caveSymbols.begin(), caveSymbols.end());
            auto civic = gm::map_render::buildSettlementCivicInk(visible);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), civic.begin(), civic.end());
            auto coverInk = gm::map_render::buildTacticalCoverInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), coverInk.begin(), coverInk.end());
            auto structures = gm::map_render::usesSettlementIllustrationV6(visible)
                ? gm::map_render::buildSettlementRoofInk(visible,plan.applyFog) : gm::map_render::buildTacticalStructureInk(visible);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), structures.begin(), structures.end());
        }
        if ((plan.layers & MapRenderPlanTerrain) != 0) {
            auto cavePools = gm::map_render::buildNaturalCavePoolInk(visible, plan.applyFog, (plan.layers & MapRenderPlanWater) != 0);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), cavePools.begin(), cavePools.end());
            auto caveInk = gm::map_render::buildNaturalCaveInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), caveInk.begin(), caveInk.end());
            auto relief = gm::map_render::usesIllustratedLocalV6(visible) ? std::vector<gm::map_render::AtlasInkPrimitive>{} : gm::map_render::buildTravelReliefInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), relief.begin(), relief.end());
            auto architecture = gm::map_render::buildDungeonArchitectureInk(visible, plan.applyFog);
            plan.atlasFeatureInk.insert(plan.atlasFeatureInk.end(), architecture.begin(), architecture.end());
        }
        features.finish();
        if ((plan.layers & MapRenderPlanWater) != 0)
        {
            CompileMeasure measure(measurements, Stage::WaterInk);
            plan.atlasWaterInk = gm::map_render::usesIllustratedLocalV6(visible)
                ? gm::map_render::buildLocalIllustrationWaterInk(visible,plan.applyFog) : gm::map_render::buildIllustratedAtlasWaterInk(visible);
        }
        if ((plan.layers & MapRenderPlanTerrain) != 0)
        {
            CompileMeasure preparation(measurements, Stage::TerrainPreparation);
            MapModel artwork = visible;
            auto &features = artwork.generation.graph.features;
            features.erase(std::remove_if(features.begin(), features.end(), [&](const auto &feature) {
                return (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail ||
                        feature.kind == MapGeneratedFeatureKind::River) &&
                    !mapRenderPlanContainsGeneratedGeometry(plan, feature.id);
            }), features.end());
            // Travel-route presentation may follow a bent network path.
            // Clear that reviewed path rather than its start/end chord.
            artwork.routes.clear();
            auto corridors = gm::map_render::illustratedAtlasRoadCorridors(artwork);
            for (const auto &primitive : plan.primitives)
            {
                if (primitive.kind == MapRenderPrimitiveKind::Route)
                    corridors.push_back({primitive.sourceId, primitive.points, .85f});
                else if (gm::map_render::usesIllustratedAtlasDetailed(visible) &&
                         primitive.kind == MapRenderPrimitiveKind::GeneratedGeometry && primitive.layer == MapRenderPlanWater)
                {
                    const auto river = std::find_if(visible.generation.graph.features.begin(), visible.generation.graph.features.end(),
                        [&](const auto &feature) { return feature.id == primitive.sourceId && feature.kind == MapGeneratedFeatureKind::River; });
                    if (river != visible.generation.graph.features.end())
                        corridors.push_back({primitive.sourceId, primitive.points, std::max(.62f, river->width * .12f + .24f)});
                }
            }
            preparation.finish();
            CompileMeasure terrain(measurements, Stage::TerrainInk);
            plan.atlasInk = gm::map_render::buildIllustratedAtlasInk(artwork, corridors);
            auto dungeonFloor = gm::map_render::buildDungeonFloorInk(artwork, plan.applyFog);
            plan.atlasInk.insert(plan.atlasInk.end(), dungeonFloor.begin(), dungeonFloor.end());
            auto caveFloor = gm::map_render::buildNaturalCaveFloorInk(artwork, plan.applyFog);
            plan.atlasInk.insert(plan.atlasInk.end(), caveFloor.begin(), caveFloor.end());
            auto local = gm::map_render::buildLocalIllustrationInk(artwork,plan.applyFog);
            plan.atlasInk.insert(plan.atlasInk.end(),local.begin(),local.end());
            auto settlementGround=gm::map_render::buildSettlementGroundInk(artwork,plan.applyFog);
            plan.atlasInk.insert(plan.atlasInk.end(),settlementGround.begin(),settlementGround.end());
            auto woodland = gm::map_render::usesIllustratedLocalV6(artwork) ? std::vector<gm::map_render::AtlasInkPrimitive>{} : gm::map_render::buildSettlementWoodlandInk(artwork);
            plan.atlasInk.insert(plan.atlasInk.end(), woodland.begin(), woodland.end());
            terrain.finish();
            CompileMeasure labels(measurements, Stage::LabelInkMask);
            plan.labelInkMask=gm::map_render::buildAtlasLabelInkMask(artwork,plan.atlasInk);
        }
    }
    else if (!reused)
    {
        CompileMeasure measure(measurements, Stage::InitialPrimitives);
        plan.primitives = buildRenderPrimitives(map, plan);
    }

    if (key && !reused)
        MemoAccess::retain(*memo, std::move(*key), plan);
    {
        CompileMeasure measure(measurements, Stage::ContentHash);
        plan.contentHash = compiledContentHash(map,plan);
    }
    return plan;
}
} // namespace

MapRenderPlan compileMapRenderPlan(const MapModel &map, const MapRenderPlanRequest &request)
{
    return compilePlan(map, request, nullptr);
}

MapRenderPlan compileMapRenderPlanMeasured(const MapModel &map,
                                          const MapRenderPlanRequest &request,
                                          MapRenderPlanMeasurements &measurements)
{
    return compilePlan(map, request, &measurements);
}

MapRenderPlan compileMapRenderPlanWithMemo(const MapModel &map,
    const MapRenderPlanRequest &request, gm::map_render::MapRenderCompileMemo &memo,
    MapRenderPlanMeasurements *measurements)
{
    return compilePlan(map, request, measurements, &memo);
}

MapRenderCompileAttempt tryCompileMapRenderPlanWithMemo(const MapModel &map,
    const MapRenderPlanRequest &request, gm::map_render::MapRenderCompileMemo &memo,
    MapRenderPlanMeasurements *measurements)
{
    MapRenderCompileAttempt result;
    auto plan = compilePlan(map, request, measurements, &memo, &result);
    if (result.status == MapRenderCompileStatus::Hit) result.plan = std::move(plan);
    return result;
}
