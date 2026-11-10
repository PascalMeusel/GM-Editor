#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapGouacheAtlasInk.hpp"
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

namespace MapRenderPlanInternal
{
std::vector<MapRenderPrimitive> buildRenderPrimitives(const MapModel &map,
                                                      const MapRenderPlan &plan)
{
    std::vector<MapRenderPrimitive> primitives;
    const bool includeNames = (plan.layers & MapRenderPlanLabels) != 0;
    primitives.reserve(plan.generatedFeatureIds.size() * 2u +
                       plan.manualFeatureIds.size() + plan.regionIds.size() +
                       plan.routeIds.size() + plan.tokenIds.size() + 8u);

    const auto mapBounds = [&]() {
        return std::vector<MapFeaturePoint>{{0.0f, 0.0f},
                                            {static_cast<float>(map.width),
                                             static_cast<float>(map.height)}};
    };
    if (plan.backgroundVisible && (plan.layers & MapRenderPlanBackground) != 0)
    {
        MapRenderPrimitive primitive;
        primitive.kind = MapRenderPrimitiveKind::Background;
        primitive.layer = MapRenderPlanBackground;
        // sourceId identifies the immutable high-resolution source reviewed
        // for output, while assetId identifies the bounded proxy used by the
        // interactive renderer. Both identities are captured on the plan.
        primitive.sourceId = plan.backgroundExportAssetId.empty()
                                 ? map.metadata.id
                                 : plan.backgroundExportAssetId;
        primitive.assetRole = "background";
        primitive.assetId = plan.backgroundInteractiveAssetId;
        primitive.points = mapBounds();
        primitives.push_back(std::move(primitive));
    }

    // Generated content is grouped by the shared back-to-front layer order,
    // while retaining source order inside each layer for deterministic ties.
    constexpr std::uint32_t generatedLayers[] = {
        MapRenderPlanTerrain, MapRenderPlanWater, MapRenderPlanWays,
        MapRenderPlanSymbols, MapRenderPlanPolitical};
    for (const std::uint32_t layer : generatedLayers)
    {
        if ((plan.layers & layer) == 0 ||
            (layer == MapRenderPlanPolitical && !plan.showPoliticalOverlay))
            continue;
        if (layer == MapRenderPlanTerrain || layer == MapRenderPlanWater)
        {
            MapRenderPrimitive primitive;
            primitive.kind = layer == MapRenderPlanTerrain
                                 ? MapRenderPrimitiveKind::TerrainSurface
                                 : MapRenderPrimitiveKind::WaterSurface;
            primitive.layer = layer;
            primitive.sourceId = map.metadata.id;
            primitive.assetRole = layer == MapRenderPlanTerrain
                                      ? "terrain_material"
                                      : "water_material";
            primitive.assetId = plan.styleSnapshotId;
            primitive.points = mapBounds();
            primitives.push_back(std::move(primitive));
        }
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (!planContainsId(plan.generatedFeatureIds, feature.id) ||
                generatedFeatureContentLayer(feature) != layer)
                continue;
            MapRenderPrimitive primitive;
            primitive.kind = primitiveKindForLayer(layer);
            primitive.layer = layer;
            primitive.sourceId = feature.id;
            primitive.assetRole = generatedFeatureAssetRole(map, feature);
            primitive.assetId = feature.materialId;
            if (includeNames)
                primitive.label = gm::map_presentation::generatedName(feature.name,
                                                                       plan.locale);
            primitive.closed = feature.closed;
            primitive.points = mapFeaturePresentationPoints(map, feature);
            if (gm::map_render::usesTacticalRiverDetail(map) && feature.kind == MapGeneratedFeatureKind::River)
            {
                const float width = std::max(1.f,feature.width)*gm::map_render::atlasPresentationProfile(map).riverWidthCells;
                primitive.detailInk = gm::map_render::buildTacticalRiverDetailInk(primitive.points,width,map.generation.recipe.generatorVersion >= 9);
            }
            else if (feature.kind == MapGeneratedFeatureKind::River && gm::map_render::usesIllustratedAtlasRiverMesh(map))
                primitive.detailInk = gm::map_render::buildIllustratedAtlasRiverInk(map,feature);
            else if (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail)
                primitive.detailInk = gm::map_render::usesGouacheAtlasArtwork(map)
                    ? gm::map_render::buildGouacheAtlasRoadInk(map,feature)
                    : gm::map_render::buildIllustratedLocalRoadDetailInk(map,feature,primitive.points);
            primitives.push_back(std::move(primitive));
        }
    }

    if ((plan.layers & MapRenderPlanLabels) != 0)
    {
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (!feature.name.empty() &&
                planContainsId(plan.generatedFeatureIds, feature.id))
            {
                MapRenderPrimitive primitive;
                primitive.kind = MapRenderPrimitiveKind::Label;
                primitive.layer = MapRenderPlanLabels;
                primitive.sourceId = feature.id;
                primitive.assetRole = "label";
                primitive.label = gm::map_presentation::generatedName(feature.name,
                                                                       plan.locale);
                primitive.points = feature.points;
                primitives.push_back(std::move(primitive));
            }
        }
    }

    if ((plan.layers & MapRenderPlanAnnotations) != 0)
    {
        for (const MapRegionModel &region : map.regions)
        {
            if (!planContainsId(plan.regionIds, region.metadata.id))
                continue;
            MapRenderPrimitive primitive;
            primitive.kind = MapRenderPrimitiveKind::AnnotationRegion;
            primitive.layer = MapRenderPlanAnnotations;
            primitive.sourceId = region.metadata.id;
            primitive.assetRole = "annotation_region";
            if (includeNames)
                primitive.label = gm::map_presentation::generatedName(region.name,
                                                                       plan.locale);
            primitive.closed = true;
            primitive.points = {
                {static_cast<float>(region.x), static_cast<float>(region.y)},
                {static_cast<float>(region.x + region.width), static_cast<float>(region.y)},
                {static_cast<float>(region.x + region.width), static_cast<float>(region.y + region.height)},
                {static_cast<float>(region.x), static_cast<float>(region.y + region.height)}};
            primitives.push_back(std::move(primitive));
        }
        for (const MapFeatureModel &feature : map.features)
        {
            if (!planContainsId(plan.manualFeatureIds, feature.metadata.id))
                continue;
            MapRenderPrimitive primitive;
            primitive.kind = MapRenderPrimitiveKind::AnnotationPoint;
            primitive.layer = MapRenderPlanAnnotations;
            primitive.sourceId = feature.metadata.id;
            primitive.assetRole = "annotation";
            if (includeNames)
                primitive.label = gm::map_presentation::generatedName(feature.name,
                                                                       plan.locale);
            primitive.points = {{static_cast<float>(feature.x) + 0.5f,
                                 static_cast<float>(feature.y) + 0.5f}};
            primitives.push_back(std::move(primitive));
        }
    }

    if ((plan.layers & MapRenderPlanRoutes) != 0)
    {
        for (const MapRouteModel &route : map.routes)
        {
            if (!planContainsId(plan.routeIds, route.metadata.id))
                continue;
            gm::map_route_presentation::BuildOptions routeOptions;
            if (plan.audience == MapRenderAudience::Player)
            {
                routeOptions.allowedGeneratedFeatureIds = &plan.generatedFeatureIds;
                // A legacy raster network cannot distinguish public from
                // private source features. Derive public route geometry from
                // reviewed graph features or terrain/direct fallbacks only.
                routeOptions.includeRasterNetwork = false;
            }
            const gm::map_route_presentation::Path presentationPath =
                gm::map_route_presentation::build(map, route, routeOptions);
            MapRenderPrimitive primitive;
            primitive.kind = MapRenderPrimitiveKind::Route;
            primitive.layer = MapRenderPlanRoutes;
            primitive.sourceId = route.metadata.id;
            primitive.assetRole = std::string(
                gm::map_route_presentation::assetRole(presentationPath.source));
            if (includeNames)
                primitive.label = gm::map_presentation::generatedName(route.name,
                                                                       plan.locale);
            primitive.points = gm::map_render::atlasRouteEntrancePoints(map,presentationPath.points);
            primitives.push_back(std::move(primitive));
        }
    }

    if ((plan.layers & MapRenderPlanTokens) != 0)
    {
        for (const MapTokenModel &token : map.tokens)
        {
            if (!planContainsId(plan.tokenIds, token.metadata.id))
                continue;
            MapRenderPrimitive primitive;
            primitive.kind = MapRenderPrimitiveKind::Token;
            primitive.layer = MapRenderPlanTokens;
            primitive.sourceId = token.metadata.id;
            primitive.assetRole = "token";
            primitive.assetId = token.assetId;
            if (includeNames)
                primitive.label = token.name;
            primitive.points = {{token.x + 0.5f, token.y + 0.5f}};
            primitives.push_back(std::move(primitive));
        }
    }

    if (plan.applyFog && (plan.layers & MapRenderPlanFog) != 0)
    {
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const std::size_t index = static_cast<std::size_t>(y * map.width + x);
                // A missing semantic cell is unknown content and therefore
                // fogged. Player/public output must fail closed rather than
                // revealing a renderer fallback through an incomplete grid.
                if (index < map.cells.size() && !map.cells[index].fogged)
                    continue;
                MapRenderPrimitive primitive;
                primitive.kind = MapRenderPrimitiveKind::FogCell;
                primitive.layer = MapRenderPlanFog;
                primitive.sourceId = map.metadata.id + ":fog:" +
                                     std::to_string(x) + ":" + std::to_string(y);
                primitive.assetRole = "fog";
                primitive.closed = true;
                primitive.points = {
                    {static_cast<float>(x), static_cast<float>(y)},
                    {static_cast<float>(x + 1), static_cast<float>(y + 1)}};
                primitives.push_back(std::move(primitive));
            }
    }

    if (plan.showGrid && (plan.layers & MapRenderPlanGrid) != 0)
    {
        MapRenderPrimitive primitive;
        primitive.kind = MapRenderPrimitiveKind::Grid;
        primitive.layer = MapRenderPlanGrid;
        primitive.sourceId = map.metadata.id;
        primitive.assetRole = "grid";
        primitive.points = mapBounds();
        primitives.push_back(std::move(primitive));
    }
    return primitives;
}
}
