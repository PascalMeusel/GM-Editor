#include "Models/Map/MapRenderPlanInternal.hpp"

namespace MapRenderPlanInternal
{
std::uint64_t compiledContentHash(const MapModel &map,const MapRenderPlan &plan)
{
    std::uint64_t hash = 14695981039346656037ull;
    hashString(hash, plan.mapId);
    hashValue(hash, plan.mapRevision);
    hashValue(hash, plan.level);
    hashValue(hash, plan.scope);
    hashValue(hash, plan.gridType);
    hashValue(hash, plan.mapVisibility);
    hashValue(hash, plan.audience);
    hashString(hash, plan.locale);
    hashString(hash, plan.styleId);
    hashString(hash, plan.styleSnapshotId);
    hashValue(hash, plan.styleVersion);
    hashValue(hash, plan.generationSeed);
    hashValue(hash, plan.generatorVersion);
    if (plan.recipeVersion >= 4) hashValue(hash, plan.recipeVersion);
    if (gm::map_render::usesIllustratedAtlasDetailed(map))
        hashValue(hash, plan.terrainLandscape);
    hashValue(hash, plan.continuousRendering);
    hashValue(hash, plan.layers);
    hashValue(hash, plan.playerReleaseLayers);
    hashValue(hash, plan.applyFog);
    hashValue(hash, plan.showPoliticalOverlay);
    hashValue(hash, plan.showGrid);
    hashValue(hash, plan.backgroundVisible);
    const auto hashInkPresentation=[&](const gm::map_render::AtlasInkPrimitive &ink) {
        // Recipe 10 introduces graduated foliage and soft road shoulders.
        // Bind their presentation to the reviewed content just as geometry is
        // bound. Older recipe hashes remain byte-for-byte compatible.
        if(plan.recipeVersion<10)return;
        hashValue(hash,ink.minimumCellPixels);hashValue(hash,ink.maximumCellPixels);
        hashValue(hash,ink.vertexColors.size());
        for(const auto c:ink.vertexColors) {
            hashValue(hash,c.r);hashValue(hash,c.g);hashValue(hash,c.b);hashValue(hash,c.a);
        }
    };
    hashString(hash, plan.backgroundInteractiveAssetId);
    hashString(hash, plan.backgroundInteractiveProjectRelativePath);
    hashString(hash, plan.backgroundInteractiveContentHash);
    hashString(hash, plan.backgroundExportAssetId);
    hashString(hash, plan.backgroundExportProjectRelativePath);
    hashString(hash, plan.backgroundExportContentHash);
    if ((plan.layers & MapRenderPlanLabels) != 0)
        hashString(hash, map.name);
    for (const MapRenderPrimitive &primitive : plan.primitives)
    {
        hashValue(hash, primitive.kind);
        hashValue(hash, primitive.layer);
        hashString(hash, primitive.sourceId);
        hashString(hash, primitive.assetRole);
        hashString(hash, primitive.assetId);
        hashString(hash, primitive.label);
        hashValue(hash, primitive.closed);
        for (const MapFeaturePoint &point : primitive.points)
        {
            hashValue(hash, point.x);
            hashValue(hash, point.y);
        }
    }
    for (const auto &primitive : plan.primitives)
        if (!primitive.detailInk.empty())
        {
            hashString(hash,"tactical_river_detail_v8");
            for (const auto &ink:primitive.detailInk)
            {
                hashInkPresentation(ink);
                hashValue(hash,ink.filled); hashValue(hash,ink.strokeCells);
                hashValue(hash,ink.color.r); hashValue(hash,ink.color.g); hashValue(hash,ink.color.b); hashValue(hash,ink.color.a);
                for (const auto p:ink.points) { hashValue(hash,p.x); hashValue(hash,p.y); }
                if (!ink.vertexColors.empty()) {
                    hashString(hash,"continuous_vertex_colors"); hashValue(hash,ink.vertexColors.size());
                    for (const auto c:ink.vertexColors) {hashValue(hash,c.r);hashValue(hash,c.g);hashValue(hash,c.b);hashValue(hash,c.a);}
                }
            }
        }
    for (const auto &ink : plan.atlasInk)
    {
        hashInkPresentation(ink);
        hashValue(hash, ink.filled); hashValue(hash, ink.strokeCells);
        hashValue(hash, ink.color.r); hashValue(hash, ink.color.g);
        hashValue(hash, ink.color.b); hashValue(hash, ink.color.a);
        for (const auto p : ink.points) { hashValue(hash, p.x); hashValue(hash, p.y); }
    }
    if (!plan.atlasFeatureInk.empty())
    {
        hashString(hash,"illustrated_atlas_features_v3");
        for (const auto &ink:plan.atlasFeatureInk)
        {
            hashInkPresentation(ink);
            hashValue(hash,ink.filled); hashValue(hash,ink.strokeCells);
            hashValue(hash,ink.color.r); hashValue(hash,ink.color.g); hashValue(hash,ink.color.b); hashValue(hash,ink.color.a);
            for (const auto p:ink.points) { hashValue(hash,p.x); hashValue(hash,p.y); }
        }
    }
    if (!plan.atlasWaterInk.empty())
    {
        hashString(hash, "illustrated_atlas_water_v2");
        hashValue(hash, plan.atlasWaterInk.size());
        for (const auto &ink : plan.atlasWaterInk)
        {
            hashInkPresentation(ink);
            hashValue(hash, ink.filled); hashValue(hash, ink.strokeCells);
            hashValue(hash, ink.color.r); hashValue(hash, ink.color.g);
            hashValue(hash, ink.color.b); hashValue(hash, ink.color.a);
            for (const auto p : ink.points) { hashValue(hash, p.x); hashValue(hash, p.y); }
        }
    }
    hashValue(hash, map.width);
    hashValue(hash, map.height);
    if (plan.backgroundVisible)
    {
        hashValue(hash, map.backgroundImageOpacity);
        hashValue(hash, map.importedReference.crop.x);
        hashValue(hash, map.importedReference.crop.y);
        hashValue(hash, map.importedReference.crop.width);
        hashValue(hash, map.importedReference.crop.height);
        hashValue(hash, map.importedReference.rotationDegrees);
    }
    for (const EntityId &id : plan.generatedFeatureIds) hashString(hash, id);
    for (const EntityId &id : plan.manualFeatureIds) hashString(hash, id);
    for (const EntityId &id : plan.regionIds) hashString(hash, id);
    for (const EntityId &id : plan.routeIds) hashString(hash, id);
    for (const EntityId &id : plan.tokenIds) hashString(hash, id);
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (!containsId(plan.generatedFeatureIds, feature.id))
            continue;
        hashString(hash, feature.id);
        hashValue(hash, feature.kind);
        if ((plan.layers & MapRenderPlanLabels) != 0)
            hashString(hash, feature.name);
        hashString(hash, feature.materialId);
        hashValue(hash, feature.width);
        hashValue(hash, feature.closed);
        hashValue(hash, feature.locked);
        hashValue(hash, feature.discovered);
        hashValue(hash, feature.gmOnly);
        hashString(hash, feature.lineage.sourceMapId);
        hashString(hash, feature.lineage.sourceFeatureId);
        hashValue(hash, feature.lineage.sourceRevision);
        for (const MapFeaturePoint &point : feature.points)
        {
            hashValue(hash, point.x);
            hashValue(hash, point.y);
        }
    }
    for (const MapCellModel &cell : map.cells)
    {
        hashValue(hash, cell.kind);
        hashValue(hash, cell.wayKind);
        hashValue(hash, cell.rotationQuarterTurns);
        hashValue(hash, cell.rotationLocked);
        hashValue(hash, cell.symbolVariantMode);
        hashValue(hash, cell.elevation);
        hashValue(hash, cell.movementCost);
        hashValue(hash, cell.difficultTerrain);
        hashValue(hash, cell.blocksLineOfSight);
        hashValue(hash, cell.lighting);
        hashValue(hash, cell.movementBlockedMask);
        hashValue(hash, cell.lineOfSightBlockedMask);
        hashValue(hash, cell.movementOneWayBlockedMask);
        hashValue(hash, cell.lineOfSightOneWayBlockedMask);
        if (plan.applyFog) hashValue(hash, cell.fogged);
    }
    for (const MapFeatureModel &feature : map.features)
    {
        if (!containsId(plan.manualFeatureIds, feature.metadata.id)) continue;
        hashString(hash, feature.metadata.id); hashValue(hash, feature.kind);
        if ((plan.layers & MapRenderPlanLabels) != 0) hashString(hash, feature.name);
        hashString(hash, feature.description);
        hashValue(hash, feature.x); hashValue(hash, feature.y);
    }
    for (const MapRegionModel &region : map.regions)
    {
        if (!containsId(plan.regionIds, region.metadata.id)) continue;
        hashString(hash, region.metadata.id);
        // Tactical start/objective names also select marker geometry and
        // colors, so a hidden text label does not make this field inert.
        hashString(hash, region.name);
        hashValue(hash, region.x); hashValue(hash, region.y); hashValue(hash, region.width); hashValue(hash, region.height);
    }
    for (const MapRouteModel &route : map.routes)
    {
        if (!containsId(plan.routeIds, route.metadata.id)) continue;
        hashString(hash, route.metadata.id);
        if ((plan.layers & MapRenderPlanLabels) != 0) hashString(hash, route.name);
        hashValue(hash, route.startX); hashValue(hash, route.startY); hashValue(hash, route.endX); hashValue(hash, route.endY);
    }
    for (const MapTokenModel &token : map.tokens)
    {
        if (!containsId(plan.tokenIds, token.metadata.id)) continue;
        hashString(hash, token.metadata.id);
        if ((plan.layers & MapRenderPlanLabels) != 0) hashString(hash, token.name);
        hashValue(hash, token.kind);
        hashValue(hash, token.x); hashValue(hash, token.y); hashValue(hash, token.rotation); hashValue(hash, token.size);
    }
    return hash;
}
} // namespace MapRenderPlanInternal
