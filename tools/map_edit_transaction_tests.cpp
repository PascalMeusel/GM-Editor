#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapEditTransactions.hpp"
#include "Models/Session/PlayerSafeDataFilter.hpp"

#include <iostream>

namespace
{
bool require(bool value, const char *message)
{
    if (!value) std::cerr << message << '\n';
    return value;
}

MapGeneratedFeature pointFeature(const std::string &id, MapGeneratedFeatureKind kind, float x, float y, bool locked = false)
{
    MapGeneratedFeature feature;
    feature.id = id;
    feature.kind = kind;
    feature.points = {{x, y}};
    feature.locked = locked;
    return feature;
}

MapGeneratedFeature lineFeature(const std::string &id, MapGeneratedFeatureKind kind,
                                std::initializer_list<MapFeaturePoint> points)
{
    MapGeneratedFeature feature;
    feature.id = id;
    feature.kind = kind;
    feature.points.assign(points.begin(), points.end());
    return feature;
}

bool sameCell(const MapCellModel &left, const MapCellModel &right)
{
    return left.kind == right.kind &&
           left.rotationQuarterTurns == right.rotationQuarterTurns &&
           left.rotationLocked == right.rotationLocked &&
           left.symbolVariantMode == right.symbolVariantMode &&
           left.wayKind == right.wayKind &&
           left.movementCost == right.movementCost &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.lighting == right.lighting &&
           left.fogged == right.fogged &&
           left.elevation == right.elevation &&
           left.movementBlockedMask == right.movementBlockedMask &&
           left.lineOfSightBlockedMask == right.lineOfSightBlockedMask &&
           left.movementOneWayBlockedMask == right.movementOneWayBlockedMask &&
           left.lineOfSightOneWayBlockedMask == right.lineOfSightOneWayBlockedMask;
}

bool sameGeneratedFeature(const MapGeneratedFeature &left, const MapGeneratedFeature &right)
{
    if (left.id != right.id || left.kind != right.kind || left.name != right.name ||
        left.materialId != right.materialId || left.width != right.width ||
        left.closed != right.closed || left.locked != right.locked ||
        left.discovered != right.discovered || left.gmOnly != right.gmOnly ||
        left.points.size() != right.points.size() ||
        left.lineage.sourceMapId != right.lineage.sourceMapId ||
        left.lineage.sourceFeatureId != right.lineage.sourceFeatureId ||
        left.lineage.sourceRevision != right.lineage.sourceRevision)
        return false;
    for (std::size_t index = 0; index < left.points.size(); ++index)
        if (left.points[index].x != right.points[index].x ||
            left.points[index].y != right.points[index].y)
            return false;
    return true;
}
}

int main()
{
    MapModel current;
    current.metadata.id = "patch_map";
    current.width = 20;
    current.height = 20;
    current.revision = 5;
    resizeMapCells(current);
    current.cells[0].fogged = true;
    current.description = "GM notes stay untouched";
    current.playerRelease.waysAndRoutes = true;
    current.atlasPlacement.parentId = "parent_world";
    current.atlasPlacement.parentRevision = 12;
    current.parentMapId = "parent_world";
    current.importedReference.enabled = true;
    current.importedReference.assetId = "managed_reference";
    current.importedReference.projectRelativePath = "maps/reference.png";
    current.importedReference.contentHash = "fnv1a64:reference";
    MapLayerModel layer;
    layer.metadata.id = "layer_keep";
    layer.name = "GM layer";
    layer.locked = true;
    current.layers.push_back(layer);
    MapTokenModel token;
    token.metadata.id = "token_keep";
    token.name = "Hidden scout";
    token.x = 4.25f;
    token.y = 6.75f;
    token.hiddenFromPlayers = true;
    current.tokens.push_back(token);
    MapRegionModel region;
    region.metadata.id = "region_keep";
    region.name = "Protected region";
    region.gmOnly = true;
    current.regions.push_back(region);
    MapFeatureModel annotation;
    annotation.metadata.id = "annotation_keep";
    annotation.name = "Secret door note";
    annotation.gmNotes = "Do not reveal";
    annotation.gmOnly = true;
    current.features.push_back(annotation);
    MapRouteModel route;
    route.metadata.id = "route_keep";
    route.name = "Old road";
    route.notes = "Ambush after dusk";
    current.routes.push_back(route);
    current.generation.graph.features = {
        pointFeature("road_replace", MapGeneratedFeatureKind::Road, 5.0f, 5.0f),
        pointFeature("river_keep", MapGeneratedFeatureKind::River, 5.0f, 5.0f),
        pointFeature("locked_road", MapGeneratedFeatureKind::Road, 6.0f, 6.0f, true),
        pointFeature("outside_road", MapGeneratedFeatureKind::Road, 17.0f, 17.0f)};

    MapModel replacement = current;
    replacement.generation.graph.features = {
        pointFeature("new_road", MapGeneratedFeatureKind::Road, 7.0f, 7.0f),
        pointFeature("new_outside", MapGeneratedFeatureKind::Road, 18.0f, 18.0f)};

    MapGenerationSelection selection;
    selection.useArea = true;
    selection.normalizedArea = {0.0f, 0.0f, 0.5f, 0.5f};
    selection.featureKinds = {MapGeneratedFeatureKind::Road};
    const MapGenerationPatch patch = buildMapGenerationPatch(current, replacement, selection);
    bool ok = require(patch.valid, "Patch should be valid");
    const auto roundBrush = mapBrushFootprint(current, 10, 10, 5, true);
    const auto squareBrush = mapBrushFootprint(current, 10, 10, 5);
    ok &= require(roundBrush.size() == 21 && squareBrush.size() == 25,
                  "Round brush excludes corners; legacy square footprint stays compatible");
    ok &= require(mapBrushFootprint(current, 10, 10, 1, true).size() == 1 &&
                      mapBrushFootprint(current, 0, 0, 5, true).size() == 8,
                  "Round brush must retain single-cell stamps and clip at map boundaries");
    MapModel brushed = current;
    applyMapTerrainBrush(brushed, roundBrush, MapTileKind::Forest, 0, false);
    ok &= require(mapCellAt(brushed, 8, 8)->kind == mapCellAt(current, 8, 8)->kind &&
                      mapCellAt(brushed, 10, 8)->kind == MapTileKind::Forest,
                  "Painted terrain must match the displayed circular footprint exactly");

    {
        MapModel hidden;hidden.width=4;hidden.height=4;resizeMapCells(hidden);
        hidden.visibility=MapVisibility::PlayersVisible;
        hidden.cells[0].kind=MapTileKind::Wall;hidden.cells[0].fogged=true;hidden.cells[0].lighting=MapLightingMode::Darkness;
        hidden.cells[1].kind=MapTileKind::Forest;hidden.cells[1].fogged=false;hidden.cells[1].lighting=MapLightingMode::Dim;
        MapTokenModel token;token.name="Fog-protected ambusher";token.x=.5f;token.y=.5f;token.hiddenFromPlayers=false;
        hidden.tokens.push_back(token);
        MapFeatureModel note;note.name="Fog-protected clue";note.x=0;note.y=0;note.gmOnly=false;note.discovered=true;
        hidden.features.push_back(note);
        ok &= require(buildPlayerSafeMapTokens(hidden).empty() && buildPlayerSafeMapFeatures(hidden).empty(),
            "Terrain erasure privacy fixture must initially hide its fog-protected content");
        const std::vector<MapCellCoordinate> footprint{{0,0},{1,0}};
        const auto preview=previewMapTerrainBrush(hidden,footprint,MapTileKind::Empty,0,true);
        ok &= require(preview.cells[0].kind==MapTileKind::Empty && preview.cells[0].fogged &&
            buildPlayerSafeMapTokens(preview).empty() && buildPlayerSafeMapFeatures(preview).empty(),
            "Terrain eraser preview must not reveal tokens or clues protected by fog");
        applyMapTerrainBrush(hidden,footprint,MapTileKind::Empty,0,true);
        ok &= require(hidden.cells[0].kind==MapTileKind::Empty && hidden.cells[1].kind==MapTileKind::Empty &&
            hidden.cells[0].fogged && !hidden.cells[1].fogged &&
            hidden.cells[0].lighting==MapLightingMode::Darkness && hidden.cells[1].lighting==MapLightingMode::Dim &&
            buildPlayerSafeMapTokens(hidden).empty() && buildPlayerSafeMapFeatures(hidden).empty(),
            "Committed terrain erasure must preserve each cell's fog and lighting without exposing player content");
    }

    ok &= require(patch.preservationVerified && mapGenerationPatchPreservesContract(patch),
                  "The canonical patch must expose and verify its preservation contract");
    ok &= require(patch.targetMapId == "patch_map" && patch.sourceRevision == 5 &&
                      patch.impact.useArea && patch.impact.featureKinds == selection.featureKinds,
                  "The canonical patch must bind its target revision and exact impact selection");
    ok &= require(patch.preservation.completeContents.size() == 14 &&
                      mapGenerationContainsPreservedContent(
                          patch.preservation, MapGenerationPreservedContent::Tokens) &&
                      mapGenerationContainsPreservedContent(
                          patch.preservation, MapGenerationPreservedContent::Fog) &&
                      patch.preservation.layerIds == std::vector<EntityId>{"layer_keep"} &&
                      patch.preservation.tokenIds == std::vector<EntityId>{"token_keep"} &&
                      patch.preservation.regionIds == std::vector<EntityId>{"region_keep"} &&
                      patch.preservation.annotationIds == std::vector<EntityId>{"annotation_keep"} &&
                      patch.preservation.routeIds == std::vector<EntityId>{"route_keep"},
                  "The preservation set must explicitly enumerate every authored collection");
    ok &= require(patch.preservation.generatedFeatureIds ==
                      std::vector<std::string>({"river_keep", "locked_road", "outside_road"}),
                  "The preservation set must explicitly enumerate locked and out-of-impact feature IDs");
    const int outsideContractIndex = mapCellIndex(current, 17, 17);
    ok &= require(outsideContractIndex >= 0 &&
                      std::binary_search(patch.preservation.byteIdenticalCellIndices.begin(),
                                         patch.preservation.byteIdenticalCellIndices.end(),
                                         static_cast<std::size_t>(outsideContractIndex)) &&
                      patch.preservation.authoredCellStateIndices.size() == current.cells.size(),
                  "Outside cells and authored state must be explicit preservation promises");
    ok &= require(patch.removedFeatureIds.size() == 1 && patch.removedFeatureIds.front() == "road_replace",
                  "Only selected, unlocked road should be removed");
    ok &= require(patch.preservedLockedFeatureIds.size() == 1, "Locked road should be preserved");
    ok &= require(patch.after.tokens.size() == 1 && patch.after.description == current.description && patch.after.cells[0].fogged,
                  "Tokens, notes and fog must survive partial regeneration");
    ok &= require(mapGenerationVectorEqual(current.layers, patch.after.layers, mapGenerationLayerEqual) &&
                      mapGenerationVectorEqual(current.tokens, patch.after.tokens, mapGenerationTokenEqual) &&
                      mapGenerationVectorEqual(current.regions, patch.after.regions, mapGenerationRegionEqual) &&
                      mapGenerationVectorEqual(current.features, patch.after.features, mapGenerationAnnotationEqual) &&
                      mapGenerationVectorEqual(current.routes, patch.after.routes, mapGenerationRouteEqual) &&
                      mapGenerationAtlasLineageEqual(current, patch.after) &&
                      mapGenerationImportedReferenceEqual(current.importedReference,
                                                          patch.after.importedReference),
                  "Partial regeneration must preserve tokens, GM notes, atlas state and imported art byte-for-byte");
    ok &= require(patch.after.revision == 6, "Patch should advance revision exactly once");

    MapModel staleTarget = current;
    staleTarget.tokens.front().x += 1.0f; // Deliberately do not advance the revision.
    MapEditHistory staleHistory;
    ok &= require(!staleHistory.apply(staleTarget, patch, "Stale regeneration") &&
                      !staleHistory.canUndo() && staleTarget.tokens.front().x == token.x + 1.0f,
                  "Atomic application must reject same-revision maps whose content changed after preview");

    MapEditHistory history;
    MapModel edited = current;
    ok &= require(history.apply(edited, patch, "Regenerate roads"), "Patch apply should succeed");
    ok &= require(history.canUndo() && history.undo(edited) && !history.canUndo() &&
                      edited.generation.graph.features.size() == 4,
                  "Undo should restore the complete prior map");
    ok &= require(history.canRedo() && history.redo(edited) && edited.revision == 6,
                  "Redo should restore the generated patch");

    MapGenerationSelection nonFiniteSelection = selection;
    nonFiniteSelection.normalizedArea.x = std::numeric_limits<float>::quiet_NaN();
    const MapGenerationPatch nonFinitePatch =
        buildMapGenerationPatch(current, replacement, nonFiniteSelection);
    ok &= require(!nonFinitePatch.valid &&
                      nonFinitePatch.errorCode == "map.regeneration.invalid_selection",
                  "Non-finite area coordinates must fail before float-to-grid conversion");

    MapModel polygonCurrent = current;
    MapGeneratedFeature enclosingNation = lineFeature(
        "nation_border", MapGeneratedFeatureKind::Nation,
        {{1.0f, 1.0f}, {19.0f, 1.0f}, {19.0f, 19.0f}, {1.0f, 19.0f}});
    enclosingNation.closed = true;
    polygonCurrent.generation.graph.features = {enclosingNation};
    MapModel polygonReplacement = polygonCurrent;
    polygonReplacement.generation.graph.features.clear();
    MapGenerationSelection nationCenter;
    nationCenter.useArea = true;
    nationCenter.normalizedArea = {0.25f, 0.25f, 0.5f, 0.5f};
    nationCenter.featureKinds = {MapGeneratedFeatureKind::Nation};
    nationCenter.replaceTerrainProjection = true;
    const MapGenerationPatch unsafePolygonPatch =
        buildMapGenerationPatch(polygonCurrent, polygonReplacement, nationCenter);
    ok &= require(!unsafePolygonPatch.valid &&
                      unsafePolygonPatch.errorCode ==
                          "map.regeneration.closed_feature_crosses_selection" &&
                      mapGenerationMapsEqual(unsafePolygonPatch.before, unsafePolygonPatch.after),
                  "Area regeneration must fail closed instead of deleting an enclosing polygon outside the impact set");

    MapModel crossingCurrent = current;
    crossingCurrent.generation.graph.features = {
        lineFeature("crossing_road", MapGeneratedFeatureKind::Road, {{1.0f, 10.0f}, {19.0f, 10.0f}})};
    MapModel crossingReplacement = crossingCurrent;
    crossingReplacement.generation.graph.features = {
        lineFeature("replacement_road", MapGeneratedFeatureKind::Road, {{0.0f, 8.0f}, {20.0f, 12.0f}})};
    MapGenerationSelection center;
    center.useArea = true;
    center.normalizedArea = {0.25f, 0.25f, 0.5f, 0.5f};
    center.featureKinds = {MapGeneratedFeatureKind::Road};
    const MapGenerationPatchPreview crossingPatch = buildMapGenerationPatch(crossingCurrent, crossingReplacement, center);
    ok &= require(crossingPatch.valid && crossingPatch.removedFeatureIds.size() == 1,
                  "A line crossing the area with both endpoints outside must be selected");
    const auto oldFirst = std::find_if(crossingPatch.after.generation.graph.features.begin(),
                                       crossingPatch.after.generation.graph.features.end(),
                                       [](const MapGeneratedFeature &feature) { return feature.id == "crossing_road"; });
    const auto oldSecond = std::find_if(crossingPatch.after.generation.graph.features.begin(),
                                        crossingPatch.after.generation.graph.features.end(),
                                        [](const MapGeneratedFeature &feature) { return feature.id == "crossing_road__outside_2"; });
    ok &= require(oldFirst != crossingPatch.after.generation.graph.features.end() &&
                      oldSecond != crossingPatch.after.generation.graph.features.end() &&
                      oldFirst->points.back().x == 5.0f && oldSecond->points.front().x == 15.0f,
                  "Outside line segments and deterministic boundary anchors must survive regeneration");
    const auto newInside = std::find_if(crossingPatch.after.generation.graph.features.begin(),
                                        crossingPatch.after.generation.graph.features.end(),
                                        [](const MapGeneratedFeature &feature) { return feature.id == "replacement_road"; });
    ok &= require(newInside != crossingPatch.after.generation.graph.features.end() &&
                      newInside->points.front().x == 5.0f && newInside->points.back().x == 15.0f,
                  "Replacement curve geometry must be clipped to the selected area");

    // A generated line may carry the same stable ID as the preserved outside
    // segment it replaces.  The inside child ID must remain deterministic;
    // wall-clock entity IDs made identical preview requests produce different
    // diffs and broke reproducible regeneration.
    MapModel duplicateReplacement = crossingCurrent;
    duplicateReplacement.generation.graph.features = {
        lineFeature("crossing_road", MapGeneratedFeatureKind::Road, {{0.0f, 9.0f}, {20.0f, 11.0f}})};
    const MapGenerationPatchPreview deterministicPatchA =
        buildMapGenerationPatch(crossingCurrent, duplicateReplacement, center);
    const MapGenerationPatchPreview deterministicPatchB =
        buildMapGenerationPatch(crossingCurrent, duplicateReplacement, center);
    ok &= require(deterministicPatchA.valid && deterministicPatchB.valid &&
                      deterministicPatchA.addedFeatureIds == deterministicPatchB.addedFeatureIds &&
                      deterministicPatchA.addedFeatureIds.size() == 1 &&
                      deterministicPatchA.addedFeatureIds.front() ==
                          "crossing_road__regenerated_6_1",
                  "Identical partial regeneration previews must use deterministic child IDs");
    std::vector<std::string> deterministicIdsA;
    std::vector<std::string> deterministicIdsB;
    for (const MapGeneratedFeature &feature : deterministicPatchA.after.generation.graph.features)
        deterministicIdsA.push_back(feature.id);
    for (const MapGeneratedFeature &feature : deterministicPatchB.after.generation.graph.features)
        deterministicIdsB.push_back(feature.id);
    ok &= require(deterministicIdsA == deterministicIdsB,
                  "Repeated partial regeneration must preserve the complete feature identity set");

    MapModel projectedCurrent = current;
    MapModel projectedReplacement = current;
    MapCellModel *currentRoad = mapCellAt(projectedCurrent, 4, 4);
    MapCellModel *replacementRoad = mapCellAt(projectedReplacement, 4, 4);
    currentRoad->wayKind = MapWayKind::DirtRoad;
    currentRoad->fogged = true;
    currentRoad->movementBlockedMask = 7;
    replacementRoad->wayKind = MapWayKind::StoneRoad;
    replacementRoad->fogged = false;
    replacementRoad->movementBlockedMask = 0;
    MapGenerationSelection roadProjection;
    roadProjection.useArea = true;
    roadProjection.normalizedArea = {0.0f, 0.0f, 0.5f, 0.5f};
    roadProjection.featureKinds = {MapGeneratedFeatureKind::Road, MapGeneratedFeatureKind::Trail};
    roadProjection.replaceWayProjection = true;
    const MapGenerationPatchPreview roadProjectionPatch =
        buildMapGenerationPatch(projectedCurrent, projectedReplacement, roadProjection);
    const MapCellModel *projectedRoad = mapCellAt(roadProjectionPatch.after, 4, 4);
    ok &= require(projectedRoad && projectedRoad->wayKind == MapWayKind::StoneRoad,
                  "Selected road projection must follow the replacement");
    ok &= require(projectedRoad && projectedRoad->fogged && projectedRoad->movementBlockedMask == 7,
                  "Road projection must not overwrite fog or tactical authoring state");

    MapCellModel *currentTerrain = mapCellAt(projectedCurrent, 3, 3);
    MapCellModel *replacementTerrain = mapCellAt(projectedReplacement, 3, 3);
    currentTerrain->kind = MapTileKind::Forest;
    currentTerrain->fogged = true;
    currentTerrain->lineOfSightBlockedMask = 5;
    replacementTerrain->kind = MapTileKind::Water;
    replacementTerrain->elevation = -2;
    replacementTerrain->fogged = false;
    replacementTerrain->lineOfSightBlockedMask = 0;
    MapCellModel *currentOutsideCell = mapCellAt(projectedCurrent, 15, 15);
    MapCellModel *replacementOutsideCell = mapCellAt(projectedReplacement, 15, 15);
    currentOutsideCell->kind = MapTileKind::Mountain;
    currentOutsideCell->rotationQuarterTurns = 3;
    currentOutsideCell->rotationLocked = true;
    currentOutsideCell->symbolVariantMode = MapSymbolVariantMode::Individual;
    currentOutsideCell->wayKind = MapWayKind::Trail;
    currentOutsideCell->movementCost = 8;
    currentOutsideCell->difficultTerrain = true;
    currentOutsideCell->blocksLineOfSight = true;
    currentOutsideCell->lighting = MapLightingMode::Darkness;
    currentOutsideCell->fogged = true;
    currentOutsideCell->elevation = 6;
    currentOutsideCell->movementBlockedMask = 1;
    currentOutsideCell->lineOfSightBlockedMask = 2;
    currentOutsideCell->movementOneWayBlockedMask = 4;
    currentOutsideCell->lineOfSightOneWayBlockedMask = 8;
    *replacementOutsideCell = MapCellModel{};
    replacementOutsideCell->kind = MapTileKind::Water;
    replacementOutsideCell->wayKind = MapWayKind::River;
    MapGenerationSelection terrainProjection;
    terrainProjection.useArea = true;
    terrainProjection.normalizedArea = {0.0f, 0.0f, 0.5f, 0.5f};
    terrainProjection.featureKinds = {MapGeneratedFeatureKind::TerrainRegion,
                                      MapGeneratedFeatureKind::WaterBody};
    terrainProjection.replaceTerrainProjection = true;
    const MapGenerationPatchPreview terrainProjectionPatch =
        buildMapGenerationPatch(projectedCurrent, projectedReplacement, terrainProjection);
    const MapCellModel *projectedTerrain = mapCellAt(terrainProjectionPatch.after, 3, 3);
    ok &= require(projectedTerrain && projectedTerrain->kind == MapTileKind::Water &&
                      projectedTerrain->elevation == -2,
                  "Selected biome projection must follow generated terrain");
    ok &= require(projectedTerrain && projectedTerrain->fogged &&
                      projectedTerrain->lineOfSightBlockedMask == 5,
                  "Biome projection must preserve fog and line-of-sight authoring state");
    const MapCellModel *outsideBefore = mapCellAt(projectedCurrent, 15, 15);
    const MapCellModel *outsideAfter = mapCellAt(terrainProjectionPatch.after, 15, 15);
    ok &= require(outsideBefore && outsideAfter && sameCell(*outsideBefore, *outsideAfter),
                  "Every field outside the selected rectangle must remain byte-equivalent");

    MapModel protectedCurrent;
    protectedCurrent.metadata.id = "protected_projection";
    protectedCurrent.width = 12;
    protectedCurrent.height = 12;
    resizeMapCells(protectedCurrent);
    MapGeneratedFeature lockedProjection = lineFeature(
        "locked_projection", MapGeneratedFeatureKind::Road,
        {{1.5f, 2.5f}, {8.5f, 2.5f}});
    lockedProjection.locked = true;
    lockedProjection.name = "Protected King's Road";
    lockedProjection.materialId = "fantasy_neutral.road.stone";
    lockedProjection.lineage.sourceMapId = "parent_world";
    lockedProjection.lineage.sourceFeatureId = "parent_road";
    lockedProjection.lineage.sourceRevision = 41;
    MapGeneratedFeature preservedProjection = lineFeature(
        "preserved_river", MapGeneratedFeatureKind::River,
        {{1.5f, 5.5f}, {8.5f, 5.5f}});
    preservedProjection.materialId = "fantasy_neutral.water.river";
    protectedCurrent.generation.graph.features = {lockedProjection, preservedProjection};
    for (const MapGeneratedFeature &feature : protectedCurrent.generation.graph.features)
        projectGeneratedWayFeatureToCells(protectedCurrent, feature, false);
    MapCellModel *lockedProjectionCell = mapCellAt(protectedCurrent, 4, 2);
    MapCellModel *preservedProjectionCell = mapCellAt(protectedCurrent, 4, 5);
    lockedProjectionCell->rotationQuarterTurns = 3;
    lockedProjectionCell->rotationLocked = true;
    lockedProjectionCell->symbolVariantMode = MapSymbolVariantMode::Individual;
    lockedProjectionCell->movementCost = 7;
    lockedProjectionCell->difficultTerrain = true;
    lockedProjectionCell->blocksLineOfSight = true;
    lockedProjectionCell->lighting = MapLightingMode::Dim;
    lockedProjectionCell->fogged = true;
    lockedProjectionCell->elevation = 4;
    lockedProjectionCell->movementBlockedMask = 3;
    lockedProjectionCell->lineOfSightBlockedMask = 5;
    lockedProjectionCell->movementOneWayBlockedMask = 6;
    lockedProjectionCell->lineOfSightOneWayBlockedMask = 9;
    preservedProjectionCell->rotationQuarterTurns = 2;
    preservedProjectionCell->rotationLocked = true;
    preservedProjectionCell->fogged = true;
    preservedProjectionCell->elevation = -3;
    const MapCellModel lockedProjectionBefore = *lockedProjectionCell;
    const MapCellModel preservedProjectionBefore = *preservedProjectionCell;

    MapModel protectedReplacement = protectedCurrent;
    protectedReplacement.generation.graph.features = {
        lineFeature("replacement_road", MapGeneratedFeatureKind::Road,
                    {{1.5f, 8.5f}, {8.5f, 8.5f}})};
    MapCellModel *replacementLockedCell = mapCellAt(protectedReplacement, 4, 2);
    MapCellModel *replacementPreservedCell = mapCellAt(protectedReplacement, 4, 5);
    *replacementLockedCell = MapCellModel{};
    replacementLockedCell->kind = MapTileKind::Water;
    replacementLockedCell->wayKind = MapWayKind::River;
    replacementLockedCell->elevation = -8;
    *replacementPreservedCell = MapCellModel{};
    replacementPreservedCell->kind = MapTileKind::Dirt;
    replacementPreservedCell->wayKind = MapWayKind::StoneRoad;

    MapGenerationSelection protectedSelection;
    protectedSelection.useArea = true;
    protectedSelection.normalizedArea = {0.0f, 0.0f, 0.8f, 0.8f};
    protectedSelection.featureKinds = {MapGeneratedFeatureKind::Road};
    protectedSelection.replaceTerrainProjection = true;
    protectedSelection.replaceWayProjection = true;
    protectedSelection.replacePlaceProjection = true;
    const MapGenerationPatchPreview protectedPatch = buildMapGenerationPatch(
        protectedCurrent, protectedReplacement, protectedSelection);
    const MapCellModel *lockedProjectionAfter = mapCellAt(protectedPatch.after, 4, 2);
    const MapCellModel *preservedProjectionAfter = mapCellAt(protectedPatch.after, 4, 5);
    ok &= require(protectedPatch.valid && lockedProjectionAfter &&
                      sameCell(lockedProjectionBefore, *lockedProjectionAfter),
                  "Partial regeneration must leave every field of a locked feature cell byte-equivalent");
    ok &= require(preservedProjectionAfter &&
                      sameCell(preservedProjectionBefore, *preservedProjectionAfter),
                  "Partial regeneration must not repaint through an out-of-family preserved feature");
    const MapGeneratedFeature *lockedFeatureAfter = nullptr;
    for (const MapGeneratedFeature &feature : protectedPatch.after.generation.graph.features)
        if (feature.id == lockedProjection.id)
            lockedFeatureAfter = &feature;
    ok &= require(lockedFeatureAfter && sameGeneratedFeature(*lockedFeatureAfter, lockedProjection),
                  "Partial regeneration must retain locked feature identity, metadata and lineage exactly");

    MapModel reconciledWays;
    reconciledWays.width = 12;
    reconciledWays.height = 9;
    resizeMapCells(reconciledWays);
    reconciledWays.generation.continuousRendering = true;
    MapGeneratedFeature generatedRoad = lineFeature(
        "generated_road", MapGeneratedFeatureKind::Road,
        {{1.5f, 1.5f}, {9.5f, 1.5f}});
    generatedRoad.name = "The Amber Road";
    generatedRoad.materialId = "fantasy_neutral.road.stone";
    generatedRoad.lineage.sourceMapId = "region_parent";
    generatedRoad.lineage.sourceFeatureId = "road_anchor";
    generatedRoad.lineage.sourceRevision = 22;
    MapGeneratedFeature lockedRiver = lineFeature(
        "locked_river", MapGeneratedFeatureKind::River,
        {{1.5f, 7.5f}, {9.5f, 7.5f}});
    lockedRiver.name = "River Anchor";
    lockedRiver.materialId = "fantasy_neutral.water.river";
    lockedRiver.locked = true;
    lockedRiver.lineage.sourceMapId = "world_parent";
    lockedRiver.lineage.sourceFeatureId = "river_anchor";
    lockedRiver.lineage.sourceRevision = 33;
    MapGeneratedFeature existingManual = lineFeature(
        "manual_way_legacy_stable", MapGeneratedFeatureKind::Road,
        {{1.5f, 4.5f}, {2.5f, 4.5f}});
    existingManual.name = "GM-renamed farm lane";
    existingManual.materialId = "fantasy_neutral.road.dirt";
    existingManual.lineage.sourceMapId = "manual_parent";
    existingManual.lineage.sourceFeatureId = "manual_source";
    existingManual.lineage.sourceRevision = 7;
    reconciledWays.generation.graph.features = {generatedRoad, lockedRiver, existingManual};
    projectGeneratedWayFeatureToCells(reconciledWays, generatedRoad, false);
    projectGeneratedWayFeatureToCells(reconciledWays, lockedRiver, false);
    for (int x = 1; x <= 3; ++x)
        mapCellAt(reconciledWays, x, 4)->wayKind = MapWayKind::DirtRoad;

    rebuildContinuousMapWayFeaturesFromCells(reconciledWays);
    ok &= require(reconciledWays.generation.graph.features.size() == 4,
                  "Grid reconciliation should add only the missing local manual segment");
    ok &= require(sameGeneratedFeature(reconciledWays.generation.graph.features[0], generatedRoad) &&
                      sameGeneratedFeature(reconciledWays.generation.graph.features[1], lockedRiver) &&
                      sameGeneratedFeature(reconciledWays.generation.graph.features[2], existingManual),
                  "Explicit grid rebuild must preserve generated and unchanged manual feature objects exactly");
    const std::string addedManualId = reconciledWays.generation.graph.features[3].id;
    rebuildContinuousMapWayFeaturesFromCells(reconciledWays);
    ok &= require(reconciledWays.generation.graph.features.size() == 4 &&
                      reconciledWays.generation.graph.features[3].id == addedManualId &&
                      sameGeneratedFeature(reconciledWays.generation.graph.features[0], generatedRoad) &&
                      sameGeneratedFeature(reconciledWays.generation.graph.features[1], lockedRiver) &&
                      sameGeneratedFeature(reconciledWays.generation.graph.features[2], existingManual),
                  "Repeated grid reconciliation must be deterministic and retain stable identities");

    const std::vector<MapCellCoordinate> protectedBrush = {{4, 1}, {4, 7}, {1, 4}};
    applyMapWayBrush(reconciledWays, protectedBrush, MapWayKind::DirtRoad, true);
    ok &= require(mapCellAt(reconciledWays, 4, 1)->wayKind == MapWayKind::StoneRoad &&
                      mapCellAt(reconciledWays, 4, 7)->wayKind == MapWayKind::River &&
                      mapCellAt(reconciledWays, 1, 4)->wayKind == MapWayKind::None,
                  "Way brushing must protect generated and locked graph footprints while editing manual cells");
    rebuildContinuousMapWayFeaturesFromCells(reconciledWays);
    const auto retainedStableManual = std::find_if(
        reconciledWays.generation.graph.features.begin(),
        reconciledWays.generation.graph.features.end(),
        [&](const MapGeneratedFeature &feature) {
            return feature.id == existingManual.id;
        });
    ok &= require(retainedStableManual == reconciledWays.generation.graph.features.end(),
                  "Erasing an affected manual segment must remove only that segment from the graph");
    ok &= require(sameGeneratedFeature(reconciledWays.generation.graph.features[0], generatedRoad) &&
                      sameGeneratedFeature(reconciledWays.generation.graph.features[1], lockedRiver),
                  "A local manual edit must not change generated or locked feature identity and lineage");

    MapModel overlappingSymbols;
    overlappingSymbols.width = 72;
    overlappingSymbols.height = 48;
    overlappingSymbols.generation.graph.features = {
        pointFeature("lindenreach", MapGeneratedFeatureKind::Settlement, 10.0f, 10.0f),
        pointFeature("watchtower", MapGeneratedFeatureKind::Landmark, 20.0f, 20.0f),
        lineFeature("road_connection", MapGeneratedFeatureKind::Road,
                    {{10.0f, 10.0f}, {30.0f, 10.0f}}),
        lineFeature("landmark_trail", MapGeneratedFeatureKind::Trail,
                    {{20.0f, 20.0f}, {25.0f, 20.0f}})};
    const auto selectedId = [&](MapFeaturePoint position, float radius, bool pointsOnly = false) {
        const MapGeneratedFeatureHit hit = hitTestGeneratedMapFeature(overlappingSymbols, position, radius, pointsOnly);
        return hit.featureIndex < 0 ? std::string{} :
                   overlappingSymbols.generation.graph.features[static_cast<std::size_t>(hit.featureIndex)].id;
    };
    for (float cellSizePixels : {8.0f, 16.0f, 48.0f})
    {
        const float hitRadius = std::clamp(11.0f / cellSizePixels, 0.18f, 1.15f);
        const float nearSymbol = hitRadius * 0.3f;
        ok &= require(selectedId({10.0f, 10.0f}, hitRadius) == "lindenreach" &&
                          selectedId({10.0f + nearSymbol, 10.0f}, hitRadius) == "lindenreach" &&
                          selectedId({20.0f, 20.0f}, hitRadius) == "watchtower",
                      "Selecting atlas place symbols must outrank overlapping road/trail endpoints at fit and detail scales");
        ok &= require(selectedId({18.0f, 10.1f}, hitRadius) == "road_connection" &&
                          selectedId({30.0f, 10.0f}, hitRadius) == "road_connection" &&
                          selectedId({40.0f, 40.0f}, hitRadius).empty(),
                      "Symbol priority must retain road segment/control-point picking outside the symbol radius and empty-space misses");
    }
    ok &= require(selectedId({10.0f, 10.0f}, 0.75f, true) == "road_connection",
                  "Explicit points-only geometry picking must retain its existing vertex selection behavior");
    std::reverse(overlappingSymbols.generation.graph.features.begin(), overlappingSymbols.generation.graph.features.end());
    ok &= require(selectedId({10.0f, 10.0f}, 0.75f) == "lindenreach" &&
                      selectedId({20.0f, 20.0f}, 0.75f) == "watchtower",
                  "A place symbol's priority over its connected way must not depend on graph feature ordering");
    overlappingSymbols.generation.graph.features.push_back(
        pointFeature("nearby_landmark", MapGeneratedFeatureKind::Landmark, 10.4f, 10.0f));
    ok &= require(selectedId({10.1f, 10.0f}, 0.75f) == "lindenreach" &&
                      selectedId({10.35f, 10.0f}, 0.75f) == "nearby_landmark",
                  "Overlapping point symbols must still select the nearest symbol instead of the last graph entry");

    MapModel directEdit;
    directEdit.width = 20;
    directEdit.height = 12;
    resizeMapCells(directEdit);
    directEdit.generation.graph.features = {
        lineFeature("editable_river", MapGeneratedFeatureKind::River,
                    {{1.0f, 3.0f}, {10.0f, 3.0f}, {18.0f, 6.0f}}),
        lineFeature("locked_road", MapGeneratedFeatureKind::Road,
                    {{2.0f, 9.0f}, {17.0f, 9.0f}})};
    directEdit.generation.graph.features[1].locked = true;
    directEdit.generation.graph.features[0].name = "Silver Run";
    directEdit.generation.graph.features[0].lineage.sourceMapId = "world_parent";
    directEdit.generation.graph.features[0].lineage.sourceFeatureId = "river_source";
    directEdit.generation.graph.features[0].lineage.sourceRevision = 17;
    const MapGeneratedFeatureHit pointHit = hitTestGeneratedMapFeature(
        directEdit, {10.1f, 3.1f}, 0.5f);
    ok &= require(pointHit.featureIndex == 0 && pointHit.pointIndex == 1,
                  "Direct feature hit testing should prioritize a nearby control point");
    const MapGeneratedFeatureHit segmentHit = hitTestGeneratedMapFeature(
        directEdit, {5.0f, 3.4f}, 0.75f);
    ok &= require(segmentHit.featureIndex == 0 && segmentHit.pointIndex < 0 &&
                      segmentHit.segmentStartIndex == 0,
                  "Direct feature hit testing should locate the nearest curve segment");
    const int insertedPoint = insertGeneratedMapFeaturePoint(
        directEdit.generation.graph.features[0], segmentHit.segmentStartIndex,
        segmentHit.segmentAmount);
    ok &= require(insertedPoint == 1 &&
                      directEdit.generation.graph.features[0].points.size() == 4,
                  "Double-click editing should be able to split a generated curve segment");
    ok &= require(moveGeneratedMapFeaturePoint(directEdit, 0, insertedPoint,
                                               {-4.0f, 99.0f}) &&
                      directEdit.generation.graph.features[0].points[1].x == 0.0f &&
                      directEdit.generation.graph.features[0].points[1].y == 12.0f,
                  "Dragged generated control points should remain inside the map");
    ok &= require(removeGeneratedMapFeaturePoint(
                      directEdit.generation.graph.features[0], insertedPoint) &&
                      directEdit.generation.graph.features[0].points.size() == 3,
                  "An inserted generated control point should be removable without deleting the feature");
    ok &= require(!moveGeneratedMapFeaturePoint(directEdit, 1, 0, {4.0f, 4.0f}) &&
                      !removeGeneratedMapFeaturePoint(directEdit.generation.graph.features[1], 0),
                  "Locked inherited features must reject direct geometry edits");

    for (const MapGeneratedFeature &feature : directEdit.generation.graph.features)
        projectGeneratedWayFeatureToCells(directEdit, feature, false);
    const MapGeneratedFeature riverBeforeDrag = directEdit.generation.graph.features[0];
    ok &= require(moveGeneratedMapFeaturePoint(directEdit, 0, 1, {10.0f, 8.0f}),
                  "An editable generated river control point should move");
    reprojectGeneratedMapWayFeaturesAfterGeometryEdit(directEdit, riverBeforeDrag);
    const MapCellModel *oldRiverCell = mapCellAt(directEdit, 5, 3);
    const MapCellModel *newRiverCell = mapCellAt(directEdit, 5, 5);
    const MapCellModel *preservedRoadCell = mapCellAt(directEdit, 5, 9);
    const MapGeneratedFeature &riverAfterDrag = directEdit.generation.graph.features[0];
    ok &= require(oldRiverCell && oldRiverCell->wayKind == MapWayKind::None &&
                      newRiverCell && newRiverCell->wayKind == MapWayKind::River,
                  "A direct geometry edit must erase the old raster way and project the new curve");
    ok &= require(preservedRoadCell && preservedRoadCell->wayKind == MapWayKind::DirtRoad,
                  "Reprojection must replay unaffected generated roads after another feature moves");
    ok &= require(riverAfterDrag.id == "editable_river" && riverAfterDrag.name == "Silver Run" &&
                      riverAfterDrag.lineage.sourceMapId == "world_parent" &&
                      riverAfterDrag.lineage.sourceFeatureId == "river_source" &&
                      riverAfterDrag.lineage.sourceRevision == 17 &&
                      directEdit.generation.graph.features[1].id == "locked_road" &&
                      directEdit.generation.graph.features[1].locked,
                  "Reprojecting edited ways must preserve IDs, names, locks and atlas lineage");

    MapModel featureProperties;
    featureProperties.width = 16;
    featureProperties.height = 12;
    resizeMapCells(featureProperties);
    featureProperties.generation.graph.features = {
        lineFeature("road_profile", MapGeneratedFeatureKind::Road,
                    {{1.0f, 1.0f}, {12.0f, 1.0f}}),
        lineFeature("river_profile", MapGeneratedFeatureKind::River,
                    {{1.0f, 3.0f}, {12.0f, 3.0f}}),
        pointFeature("place_profile", MapGeneratedFeatureKind::Settlement, 4.0f, 6.0f),
        pointFeature("setpiece_profile", MapGeneratedFeatureKind::TacticalSetpiece, 8.0f, 8.0f),
        lineFeature("locked_profile", MapGeneratedFeatureKind::Road,
                    {{1.0f, 10.0f}, {12.0f, 10.0f}})};
    MapGeneratedFeature &profileRoad = featureProperties.generation.graph.features[0];
    profileRoad.name = "Old King's Road";
    profileRoad.materialId = "fantasy_neutral.road.dirt";
    profileRoad.width = 0.82f;
    profileRoad.lineage.sourceMapId = "region_parent";
    profileRoad.lineage.sourceFeatureId = "road_parent";
    profileRoad.lineage.sourceRevision = 9;
    featureProperties.generation.graph.features[1].materialId = "fantasy_neutral.water.river";
    featureProperties.generation.graph.features[2].materialId = "fantasy_neutral.settlement.city";
    featureProperties.generation.graph.features[3].materialId = "fantasy_neutral.tactical.cover";
    featureProperties.generation.graph.features[4].materialId = "fantasy_neutral.road.dirt";
    featureProperties.generation.graph.features[4].locked = true;

    const auto roadClassifications =
        mapGeneratedFeatureClassificationOptions(MapGeneratedFeatureKind::Road);
    const auto placeClassifications =
        mapGeneratedFeatureClassificationOptions(MapGeneratedFeatureKind::Settlement);
    ok &= require(roadClassifications.size() == 2 &&
                      roadClassifications[0].kind == MapGeneratedFeatureKind::Trail &&
                      roadClassifications[1].kind == MapGeneratedFeatureKind::Road &&
                      placeClassifications.size() == 2 &&
                      placeClassifications[0].kind == MapGeneratedFeatureKind::Settlement &&
                      placeClassifications[1].kind == MapGeneratedFeatureKind::Landmark,
                  "Generated feature classification choices must stay inside compatible families");
    ok &= require(!applyGeneratedMapFeatureClassification(
                      featureProperties, 0, MapGeneratedFeatureKind::River) &&
                      profileRoad.kind == MapGeneratedFeatureKind::Road,
                  "The feature inspector must reject classifications from another feature family");

    for (const MapGeneratedFeature &feature : featureProperties.generation.graph.features)
        projectGeneratedWayFeatureToCells(featureProperties, feature, false);
    MapCellModel *roadProfileCell = mapCellAt(featureProperties, 6, 1);
    MapCellModel *riverProfileCell = mapCellAt(featureProperties, 6, 3);
    MapCellModel *placeProfileCell = mapCellAt(featureProperties, 4, 6);
    MapCellModel *setpieceProfileCell = mapCellAt(featureProperties, 8, 8);
    roadProfileCell->fogged = true;
    roadProfileCell->movementBlockedMask = 3;
    riverProfileCell->fogged = true;
    riverProfileCell->lineOfSightBlockedMask = 5;
    placeProfileCell->fogged = true;
    placeProfileCell->movementOneWayBlockedMask = 6;
    setpieceProfileCell->fogged = true;
    setpieceProfileCell->lineOfSightOneWayBlockedMask = 9;

    ok &= require(applyGeneratedMapFeatureAppearance(
                      featureProperties, 0, "fantasy_neutral.road.stone"),
                  "A validated road display profile should be applied");
    roadProfileCell = mapCellAt(featureProperties, 6, 1);
    ok &= require(profileRoad.materialId == "fantasy_neutral.road.stone" &&
                      std::abs(profileRoad.width - 1.08f) < 0.001f &&
                      roadProfileCell && roadProfileCell->wayKind == MapWayKind::StoneRoad,
                  "Road profile changes must update both graph appearance and raster semantics");
    ok &= require(roadProfileCell && roadProfileCell->fogged &&
                      roadProfileCell->movementBlockedMask == 3,
                  "Road profile changes must preserve authored fog and movement edges");
    ok &= require(!applyGeneratedMapFeatureAppearance(
                      featureProperties, 0, "unvalidated.internal.material") &&
                      profileRoad.materialId == "fantasy_neutral.road.stone",
                  "The inspector must reject free-form or unknown material IDs");

    ok &= require(applyGeneratedMapFeatureClassification(
                      featureProperties, 0, MapGeneratedFeatureKind::Trail),
                  "A road should be reclassifiable as a trail");
    roadProfileCell = mapCellAt(featureProperties, 6, 1);
    ok &= require(profileRoad.kind == MapGeneratedFeatureKind::Trail &&
                      profileRoad.materialId == "fantasy_neutral.road.trail" &&
                      std::abs(profileRoad.width - 0.55f) < 0.001f &&
                      roadProfileCell && roadProfileCell->wayKind == MapWayKind::Trail,
                  "Road-to-trail classification must choose a valid profile and rebuild the raster way");
    ok &= require(profileRoad.id == "road_profile" && profileRoad.name == "Old King's Road" &&
                      profileRoad.lineage.sourceMapId == "region_parent" &&
                      profileRoad.lineage.sourceFeatureId == "road_parent" &&
                      profileRoad.lineage.sourceRevision == 9,
                  "Classification changes must preserve IDs, names and atlas lineage");

    ok &= require(applyGeneratedMapFeatureAppearance(
                      featureProperties, 1, "fantasy_neutral.water.canal"),
                  "A validated canal profile should be applied to a waterway");
    riverProfileCell = mapCellAt(featureProperties, 6, 3);
    ok &= require(riverProfileCell && riverProfileCell->kind == MapTileKind::Water &&
                      riverProfileCell->wayKind == MapWayKind::Canal &&
                      riverProfileCell->fogged &&
                      riverProfileCell->lineOfSightBlockedMask == 5,
                  "Canal profiles must retain water semantics and authored visibility edges");

    ok &= require(applyGeneratedMapFeatureClassification(
                      featureProperties, 2, MapGeneratedFeatureKind::Landmark),
                  "A settlement marker should be reclassifiable as a landmark");
    placeProfileCell = mapCellAt(featureProperties, 4, 6);
    const MapGeneratedFeature &profilePlace = featureProperties.generation.graph.features[2];
    ok &= require(profilePlace.id == "place_profile" &&
                      profilePlace.kind == MapGeneratedFeatureKind::Landmark &&
                      profilePlace.materialId == "fantasy_neutral.landmark.discovery" &&
                      placeProfileCell && placeProfileCell->kind == MapTileKind::Landmark &&
                      placeProfileCell->fogged &&
                      placeProfileCell->movementOneWayBlockedMask == 6,
                  "Point classification must update the cell without dropping authored state or identity");

    ok &= require(applyGeneratedMapFeatureAppearance(
                      featureProperties, 3, "fantasy_neutral.tactical.rubble"),
                  "A validated tactical setpiece profile should be applied");
    setpieceProfileCell = mapCellAt(featureProperties, 8, 8);
    ok &= require(setpieceProfileCell && setpieceProfileCell->kind == MapTileKind::Rubble &&
                      setpieceProfileCell->fogged &&
                      setpieceProfileCell->lineOfSightOneWayBlockedMask == 9,
                  "Tactical display profiles must update the raster role without dropping authored state");

    ok &= require(!applyGeneratedMapFeatureAppearance(
                      featureProperties, 4, "fantasy_neutral.road.stone") &&
                      !applyGeneratedMapFeatureClassification(
                          featureProperties, 4, MapGeneratedFeatureKind::Trail) &&
                      featureProperties.generation.graph.features[4].materialId ==
                          "fantasy_neutral.road.dirt" &&
                      featureProperties.generation.graph.features[4].kind ==
                          MapGeneratedFeatureKind::Road,
                  "Locked and inherited features must reject profile and classification changes");
    return ok ? 0 : 1;
}
