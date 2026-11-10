#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/MapGeneration.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
bool require(bool condition, const std::string &message)
{
    if (condition)
        return true;
    std::cerr << message << std::endl;
    return false;
}

bool nearlyEqual(float left, float right)
{
    return std::abs(left - right) < 0.001f;
}

bool sameCellContract(const MapCellModel &left, const MapCellModel &right)
{
    return left.kind == right.kind &&
           left.rotationQuarterTurns == right.rotationQuarterTurns &&
           left.rotationLocked == right.rotationLocked &&
           left.symbolVariantMode == right.symbolVariantMode &&
           left.wayKind == right.wayKind && left.movementCost == right.movementCost &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.lighting == right.lighting && left.fogged == right.fogged &&
           left.elevation == right.elevation &&
           left.movementBlockedMask == right.movementBlockedMask &&
           left.lineOfSightBlockedMask == right.lineOfSightBlockedMask &&
           left.movementOneWayBlockedMask == right.movementOneWayBlockedMask &&
           left.lineOfSightOneWayBlockedMask == right.lineOfSightOneWayBlockedMask;
}

bool sameMetadataContract(const EntityMetadata &left, const EntityMetadata &right)
{
    return left.id == right.id && left.createdAtUtc == right.createdAtUtc &&
           left.updatedAtUtc == right.updatedAtUtc && left.tags == right.tags &&
           left.customProperties == right.customProperties;
}

bool sameRegionContract(const MapRegionModel &left, const MapRegionModel &right)
{
    return sameMetadataContract(left.metadata, right.metadata) &&
           left.name == right.name && left.description == right.description &&
           left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height && left.movementCost == right.movementCost &&
           left.lighting == right.lighting &&
           left.difficultTerrain == right.difficultTerrain &&
           left.blocksLineOfSight == right.blocksLineOfSight &&
           left.gmOnly == right.gmOnly;
}

bool sameRouteContract(const MapRouteModel &left, const MapRouteModel &right)
{
    return sameMetadataContract(left.metadata, right.metadata) &&
           left.name == right.name && left.fromMapId == right.fromMapId &&
           left.toMapId == right.toMapId && left.startX == right.startX &&
           left.startY == right.startY && left.endX == right.endX &&
           left.endY == right.endY && nearlyEqual(left.distanceMiles, right.distanceMiles) &&
           left.terrain == right.terrain && left.notes == right.notes &&
           left.discovered == right.discovered;
}

MapModel makeMap(const std::string &id, const std::string &name, MapLevel level, std::uint64_t revision)
{
    MapModel map;
    map.metadata.id = id;
    map.name = name;
    map.atlasLevel = level;
    map.revision = revision;
    map.width = 100;
    map.height = 100;
    return map;
}

void linkTo(MapModel &child, const MapModel &parent, MapNormalizedRect rect)
{
    child.atlasPlacement.parentId = parent.metadata.id;
    child.atlasPlacement.parentRect = rect;
    child.atlasPlacement.parentRevision = parent.revision;
    child.atlasPlacement.sourceRevision = child.revision;
    child.atlasStatus = MapAtlasStatus::Linked;
}

bool hasIssue(const MapAtlasValidationResult &result, const std::string &code)
{
    for (const MapAtlasValidationIssue &issue : result.issues)
    {
        if (issue.code == code)
            return true;
    }
    return false;
}

bool testValidationHierarchyAndBreadcrumbs()
{
    MapModel world = makeMap("world", "The World", MapLevel::World, 4);
    MapModel region = makeMap("region", "Northlands", MapLevel::Region, 8);
    MapModel site = makeMap("site", "Old Keep", MapLevel::Site, 2);
    MapModel tactical = makeMap("tactical", "Gatehouse", MapLevel::Tactical, 1);
    linkTo(region, world, {0.1f, 0.2f, 0.5f, 0.4f});
    linkTo(site, region, {0.2f, 0.2f, 0.3f, 0.3f});
    linkTo(tactical, site, {0.0f, 0.0f, 1.0f, 1.0f});
    const std::vector<MapModel> maps{world, region, site, tactical};

    bool ok = true;
    ok &= require(MapAtlasService::validate(maps).valid, "A strictly deepening atlas hierarchy should validate");
    const std::vector<MapAtlasHierarchyNode> hierarchy = MapAtlasService::buildHierarchy(maps);
    ok &= require(hierarchy.size() == 1 && hierarchy.front().children.size() == 1 &&
                      hierarchy.front().children.front().children.size() == 1,
                  "Atlas hierarchy should recursively nest descendants");
    const std::vector<MapAtlasBreadcrumb> crumbs = MapAtlasService::breadcrumbs(maps, tactical.metadata.id);
    ok &= require(crumbs.size() == 4 && crumbs.front().mapId == "world" && crumbs.back().mapId == "tactical",
                  "Breadcrumbs should run from root to selected descendant");

    MapModel invalidLevel = makeMap("invalid-level", "Invalid", MapLevel::Region, 1);
    linkTo(invalidLevel, region, {0.0f, 0.0f, 0.5f, 0.5f});
    ok &= require(hasIssue(MapAtlasService::validate({world, region, invalidLevel}), "invalid_level"),
                  "A child at the same atlas level as its parent should be rejected");

    MapModel badRect = site;
    badRect.metadata.id = "bad-rect";
    badRect.atlasPlacement.parentRect = {0.8f, 0.8f, 0.4f, 0.4f};
    ok &= require(hasIssue(MapAtlasService::validate({world, region, badRect}), "invalid_parent_rect"),
                  "Placements outside the normalized parent rectangle should be rejected");

    MapModel cycleA = makeMap("cycle-a", "Cycle A", MapLevel::World, 1);
    MapModel cycleB = makeMap("cycle-b", "Cycle B", MapLevel::Region, 1);
    linkTo(cycleA, cycleB, {0.0f, 0.0f, 1.0f, 1.0f});
    linkTo(cycleB, cycleA, {0.0f, 0.0f, 1.0f, 1.0f});
    ok &= require(hasIssue(MapAtlasService::validate({cycleA, cycleB}), "cycle"),
                  "Atlas validation should detect parent cycles");

    MapModel legacyWorld = makeMap("legacy-world", "Legacy World", MapLevel::World, 1);
    legacyWorld.scope = MapScope::World;
    MapModel legacySite = makeMap("legacy-site", "Legacy Site", MapLevel::World, 1);
    legacySite.scope = MapScope::Local;
    legacySite.parentMapId = legacyWorld.metadata.id;
    const std::vector<MapModel> legacyMaps{legacyWorld, legacySite};
    ok &= require(MapAtlasService::validate(legacyMaps).valid,
                  "Legacy parentMapId hierarchies should validate using scope-derived atlas levels");
    const std::vector<MapAtlasHierarchyNode> legacyHierarchy = MapAtlasService::buildHierarchy(legacyMaps);
    ok &= require(legacyHierarchy.size() == 1 && legacyHierarchy.front().children.size() == 1 &&
                      legacyHierarchy.front().children.front().mapId == legacySite.metadata.id &&
                      legacyHierarchy.front().children.front().level == MapLevel::Site,
                  "Legacy child maps should remain visible in the recursive atlas hierarchy");
    const MapAtlasStaleReport unpositioned = MapAtlasService::detectStale(legacySite, legacyMaps);
    ok &= require(!unpositioned.stale && unpositioned.suggestedStatus == MapAtlasStatus::Unpositioned,
                  "Existing child maps without a normalized excerpt should be classified as unpositioned");
    ok &= require(MapAtlasService::levelFromLegacyScope(MapScope::Dungeon) == MapLevel::Site &&
                      MapAtlasService::kindFromLegacyScope(MapScope::Dungeon) == MapKind::Dungeon,
                  "Legacy MapScope helpers should derive atlas level and map kind consistently");
    return ok;
}

bool testCoordinateTransformsAndExtraction()
{
    MapModel parent = makeMap("parent", "Parent", MapLevel::World, 7);
    MapGeneratedFeature road;
    road.id = "road-main";
    road.kind = MapGeneratedFeatureKind::Road;
    road.name = "King's Road";
    road.points = {{10.0f, 50.0f}, {90.0f, 50.0f}};
    parent.generation.graph.features.push_back(road);
    MapGeneratedFeature settlement;
    settlement.id = "settlement-center";
    settlement.kind = MapGeneratedFeatureKind::Settlement;
    settlement.points = {{50.0f, 50.0f}};
    parent.generation.graph.features.push_back(settlement);
    MapRegionModel parentRegion;
    parentRegion.metadata.id = "region-center";
    parentRegion.name = "Central March";
    parentRegion.x = 30;
    parentRegion.y = 30;
    parentRegion.width = 40;
    parentRegion.height = 40;
    parent.regions.push_back(parentRegion);
    MapRouteModel parentRoute;
    parentRoute.metadata.id = "route-center";
    parentRoute.name = "Pilgrim Road";
    parentRoute.startX = 0;
    parentRoute.startY = 50;
    parentRoute.endX = 100;
    parentRoute.endY = 50;
    parentRoute.distanceMiles = 100.0f;
    parent.routes.push_back(parentRoute);

    MapAtlasPlacement placement;
    placement.parentId = parent.metadata.id;
    placement.parentRect = {0.25f, 0.25f, 0.5f, 0.5f};
    placement.parentRevision = parent.revision;

    bool ok = true;
    const MapAtlasPoint parentPoint = MapAtlasService::childToParentNormalized(placement, {0.5f, 0.5f});
    ok &= require(nearlyEqual(parentPoint.x, 0.5f) && nearlyEqual(parentPoint.y, 0.5f),
                  "Child-to-parent normalized transformation should honor placement scale and offset");
    const std::optional<MapAtlasPoint> childPoint = MapAtlasService::parentToChildNormalized(placement, parentPoint);
    ok &= require(childPoint && nearlyEqual(childPoint->x, 0.5f) && nearlyEqual(childPoint->y, 0.5f),
                  "Parent-to-child transformation should invert the placement transform");
    ok &= require(!MapAtlasService::parentToChildNormalized(placement, {0.1f, 0.1f}),
                  "Parent points outside the placement should not transform into child coordinates");

    const MapAtlasExtractionResult extraction = MapAtlasService::extractFromParent(parent, placement, 20, 20);
    ok &= require(extraction.valid && extraction.graph.features.size() == 2,
                  "Feature extraction should include clipped lines and contained point features");
    const MapGeneratedFeature &clippedRoad = extraction.graph.features.front();
    ok &= require(clippedRoad.points.size() == 2 && nearlyEqual(clippedRoad.points.front().x, 0.0f) && nearlyEqual(clippedRoad.points.back().x, 20.0f),
                  "Cross-boundary line features should clip to the child map bounds");
    ok &= require(clippedRoad.lineage.sourceMapId == parent.metadata.id && clippedRoad.lineage.sourceFeatureId == road.id &&
                      clippedRoad.lineage.sourceRevision == parent.revision,
                  "Extracted features should retain stable source lineage");
    ok &= require(extraction.boundaryConstraints.size() == 2,
                  "A line crossing the excerpt should create entry and exit boundary constraints");
    ok &= require(extraction.generationConstraints.size() == 2 && extraction.generationConstraints.front().required &&
                      extraction.generationConstraints.front().locked,
                  "Boundary crossings should be directly usable as locked generation constraints");
    ok &= require(extraction.regions.size() == 1 && extraction.regions.front().x == 2 &&
                      extraction.regions.front().y == 2 && extraction.regions.front().width == 16 &&
                      extraction.regions.front().height == 16 &&
                      extraction.regions.front().metadata.id == "region-center@atlas_region" &&
                      extraction.regions.front().metadata.customProperties.at("atlas.source_map_id") ==
                          parent.metadata.id,
                  "Region extraction must clip, transform and mark inherited child regions");
    ok &= require(extraction.routes.size() == 1 && extraction.routes.front().startX == 0 &&
                      extraction.routes.front().endX == 19 && extraction.routes.front().startY == 10 &&
                      extraction.routes.front().endY == 10 &&
                      nearlyEqual(extraction.routes.front().distanceMiles, 50.0f) &&
                      extraction.routes.front().metadata.id == "route-center@atlas_route",
                  "Route extraction must clip, transform and mark inherited child routes");
    return ok;
}

bool testStaleDetectionAndPromotionPreview()
{
    MapModel parent = makeMap("world", "World", MapLevel::World, 5);
    MapModel child = makeMap("region", "Region", MapLevel::Region, 3);
    child.width = 20;
    child.height = 20;
    linkTo(child, parent, {0.25f, 0.25f, 0.5f, 0.5f});
    MapGeneratedFeature road;
    road.id = "road";
    road.kind = MapGeneratedFeatureKind::Road;
    road.points = {{0.0f, 10.0f}, {20.0f, 10.0f}};
    child.generation.graph.features.push_back(road);

    bool ok = true;
    const MapAtlasStaleReport current = MapAtlasService::detectStale(child, {parent, child});
    ok &= require(!current.stale && current.suggestedStatus == MapAtlasStatus::Linked,
                  "Matching parent and source revisions should produce a current atlas link");
    MapModel changedParent = parent;
    changedParent.revision++;
    const MapAtlasStaleReport parentStale = MapAtlasService::detectStale(child, {changedParent, child});
    ok &= require(parentStale.stale && parentStale.parentChanged && parentStale.suggestedStatus == MapAtlasStatus::Stale,
                  "Parent revision changes should mark the atlas link stale");
    MapModel changedChild = child;
    changedChild.revision++;
    const MapAtlasStaleReport sourceStale = MapAtlasService::detectStale(changedChild, {parent, changedChild});
    ok &= require(sourceStale.stale && sourceStale.sourceChanged,
                  "Source revision changes should mark the atlas link stale");

    const std::size_t parentFeatureCount = parent.generation.graph.features.size();
    const MapAtlasPromotionPreview preview = MapAtlasService::previewPromotion(child, parent);
    ok &= require(preview.valid && preview.promotedFeatures.size() == 1,
                  "Promotion preview should produce transformed features without applying them");
    ok &= require(preview.expectedSourceContentFingerprint != 0 &&
                      preview.expectedParentContentFingerprint != 0 &&
                      MapAtlasService::promotionPreviewIsCurrent(child, parent, preview),
                  "Promotion preview should bind source and parent content, not revisions alone");
    ok &= require(parent.generation.graph.features.size() == parentFeatureCount,
                  "Promotion preview must not mutate the parent feature graph");
    ok &= require(nearlyEqual(preview.promotedFeatures.front().points.front().x, 25.0f) &&
                      nearlyEqual(preview.promotedFeatures.front().points.back().x, 75.0f) &&
                      nearlyEqual(preview.promotedFeatures.front().points.front().y, 50.0f),
                  "Promotion preview should transform child feature coordinates into parent space");

    MapModel sameRevisionChild = child;
    sameRevisionChild.generation.graph.features.front().points.front().x += 1.0f;
    ok &= require(!MapAtlasService::promotionPreviewIsCurrent(sameRevisionChild, parent, preview),
                  "Same-revision source edits must invalidate a promotion preview");
    MapModel sameRevisionParent = parent;
    sameRevisionParent.regions.push_back(MapRegionModel{});
    sameRevisionParent.regions.back().metadata.id = "same-revision-region";
    ok &= require(!MapAtlasService::promotionPreviewIsCurrent(child, sameRevisionParent, preview),
                  "Same-revision parent edits must invalidate a promotion preview");

    MapGeneratedFeature conflict = preview.promotedFeatures.front();
    parent.generation.graph.features.push_back(conflict);
    const MapAtlasPromotionPreview conflictPreview = MapAtlasService::previewPromotion(child, parent);
    ok &= require(!conflictPreview.valid && conflictPreview.conflicts.size() == 1,
                  "Promotion preview should report stable-identity conflicts explicitly");
    return ok;
}

bool testExplicitRebase()
{
    MapModel parent = makeMap("world-rebase", "World", MapLevel::World, 2);
    MapGeneratedFeature road;
    road.id = "road-main";
    road.kind = MapGeneratedFeatureKind::Road;
    road.materialId = "fantasy_neutral.road.dirt";
    road.points = {{0.0f, 50.0f}, {100.0f, 50.0f}};
    parent.generation.graph.features.push_back(road);
    MapGeneratedFeature landmark;
    landmark.id = "watchtower";
    landmark.kind = MapGeneratedFeatureKind::Landmark;
    landmark.name = "Old Watchtower";
    landmark.points = {{70.0f, 30.0f}};
    parent.generation.graph.features.push_back(landmark);
    MapRegionModel parentRegion;
    parentRegion.metadata.id = "north-march";
    parentRegion.name = "North March";
    parentRegion.x = 10;
    parentRegion.y = 10;
    parentRegion.width = 30;
    parentRegion.height = 20;
    parent.regions.push_back(parentRegion);
    MapRouteModel parentRoute;
    parentRoute.metadata.id = "trade-route";
    parentRoute.name = "Trade Route";
    parentRoute.startX = 0;
    parentRoute.startY = 20;
    parentRoute.endX = 100;
    parentRoute.endY = 20;
    parentRoute.distanceMiles = 80.0f;
    parent.routes.push_back(parentRoute);

    MapModel child = makeMap("region-rebase", "Region", MapLevel::Region, 4);
    child.width = 50;
    child.height = 50;
    resizeMapCells(child);
    linkTo(child, parent, {0.0f, 0.0f, 1.0f, 1.0f});
    const MapAtlasExtractionResult initial = MapAtlasService::extractFromParent(parent, child.atlasPlacement, child.width, child.height);
    child.generation.graph = initial.graph;
    child.regions = initial.regions;
    child.routes = initial.routes;
    child.generation.recipe.constraints = initial.generationConstraints;
    for (const MapGeneratedFeature &feature : initial.graph.features)
    {
        child.generation.recipe.lockedFeatureIds.push_back(feature.id);
        projectGeneratedWayFeatureToCells(child, feature, false);
        projectGeneratedPointFeatureClassificationToCells(child, feature);
    }
    MapGeneratedFeature local;
    local.id = "local-place";
    local.kind = MapGeneratedFeatureKind::Settlement;
    local.points = {{10.0f, 10.0f}};
    child.generation.graph.features.push_back(local);
    MapRegionModel localRegion;
    localRegion.metadata.id = "local-region";
    localRegion.name = "GM region";
    child.regions.push_back(localRegion);
    MapRouteModel localRoute;
    localRoute.metadata.id = "local-route";
    localRoute.name = "GM shortcut";
    child.routes.push_back(localRoute);
    MapTokenModel token;
    token.metadata.id = "token-preserved";
    token.name = "Hidden patrol";
    token.hiddenFromPlayers = true;
    child.tokens.push_back(token);
    MapFeatureModel note;
    note.metadata.id = "note-preserved";
    note.gmNotes = "Secret ford";
    note.gmOnly = true;
    child.features.push_back(note);
    MapCellModel *oldRoadAuthoredCell = mapCellAt(child, 20, 25);
    oldRoadAuthoredCell->fogged = true;
    oldRoadAuthoredCell->movementCost = 7;
    oldRoadAuthoredCell->difficultTerrain = true;
    oldRoadAuthoredCell->lighting = MapLightingMode::Darkness;
    oldRoadAuthoredCell->movementBlockedMask = 3;
    oldRoadAuthoredCell->lineOfSightBlockedMask = 5;
    const MapCellModel oldRoadBefore = *oldRoadAuthoredCell;
    MapCellModel *outsideCell = mapCellAt(child, 49, 49);
    outsideCell->kind = MapTileKind::Snow;
    outsideCell->wayKind = MapWayKind::Trail;
    outsideCell->rotationQuarterTurns = 3;
    outsideCell->rotationLocked = true;
    outsideCell->fogged = true;
    outsideCell->elevation = 9;
    outsideCell->lineOfSightOneWayBlockedMask = 8;
    const MapCellModel outsideBefore = *outsideCell;

    parent.revision = 3;
    parent.generation.graph.features.front().points[0].y = 70.0f;
    parent.generation.graph.features.front().points[1].y = 70.0f;
    parent.generation.graph.features[1].points.front() = {75.0f, 35.0f};
    parent.regions.front().name = "Expanded North March";
    parent.regions.front().x = 20;
    parent.regions.front().width = 40;
    parent.routes.front().startY = 80;
    parent.routes.front().endY = 80;
    MapGeneratedFeature river;
    river.id = "river-new";
    river.kind = MapGeneratedFeatureKind::River;
    river.materialId = "fantasy_neutral.water.river";
    river.points = {{30.0f, 0.0f}, {30.0f, 100.0f}};
    parent.generation.graph.features.push_back(river);
    MapGeneratedFeature bridge;
    bridge.id = "bridge-new";
    bridge.kind = MapGeneratedFeatureKind::Bridge;
    bridge.materialId = "fantasy_neutral.bridge.stone";
    bridge.points = {{29.0f, 70.0f}, {31.0f, 70.0f}};
    parent.generation.graph.features.push_back(bridge);

    bool ok = true;
    const MapAtlasRebasePreview preview = MapAtlasService::previewRebase(child, parent);
    ok &= require(preview.valid && preview.items.size() == 6 &&
                      preview.expectedChildContentFingerprint != 0 &&
                      preview.expectedParentContentFingerprint != 0 &&
                      std::count_if(preview.items.begin(), preview.items.end(), [](const MapAtlasRebaseItem &item) {
                          return item.kind == MapAtlasRebaseItemKind::Region;
                      }) == 1 &&
                      std::count_if(preview.items.begin(), preview.items.end(), [](const MapAtlasRebaseItem &item) {
                          return item.kind == MapAtlasRebaseItemKind::Route;
                      }) == 1 &&
                      !preview.affectedRasterCellIndices.empty(),
                  "Rebase preview should bind and expose every graph, region, route and raster representation");
    std::vector<MapAtlasRebaseDecision> decisions;
    for (const MapAtlasRebaseItem &item : preview.items)
        if (item.requiresDecision)
            decisions.push_back({item.id, item.sourceEntityId == "watchtower"
                                              ? MapAtlasRebaseResolution::Detach
                                              : MapAtlasRebaseResolution::UseParent});
    const MapAtlasRebaseResult result = MapAtlasService::applyRebase(child, parent, preview, decisions);
    ok &= require(result.applied && result.map.revision == child.revision + 1 &&
                      result.map.atlasPlacement.parentRevision == parent.revision &&
                      result.map.atlasPlacement.sourceRevision == result.map.revision &&
                      !result.synchronizedRasterCellIndices.empty() && child.revision == 4,
                  "Applying a rebase should atomically advance the child and atlas revisions");
    const auto detached = std::find_if(result.map.generation.graph.features.begin(), result.map.generation.graph.features.end(), [](const MapGeneratedFeature &feature) {
        return feature.id.find("watchtower") != std::string::npos;
    });
    ok &= require(detached != result.map.generation.graph.features.end() && detached->lineage.sourceMapId.empty() && !detached->locked,
                  "Detaching should preserve local geometry while removing lineage and its lock");
    ok &= require(std::any_of(result.map.generation.graph.features.begin(), result.map.generation.graph.features.end(), [](const MapGeneratedFeature &feature) {
                      return feature.lineage.sourceFeatureId == "river-new";
                  }),
                  "Using the parent should add newly inherited features");
    const MapCellModel *oldRoadAfter = mapCellAt(result.map, 20, 25);
    const MapCellModel *newRoadAfter = mapCellAt(result.map, 20, 35);
    const MapCellModel *newRiverAfter = mapCellAt(result.map, 15, 10);
    const MapCellModel *crossingAfter = mapCellAt(result.map, 15, 35);
    ok &= require(oldRoadAfter && oldRoadAfter->wayKind == MapWayKind::None &&
                      oldRoadAfter->fogged == oldRoadBefore.fogged &&
                      oldRoadAfter->movementCost == oldRoadBefore.movementCost &&
                      oldRoadAfter->difficultTerrain == oldRoadBefore.difficultTerrain &&
                      oldRoadAfter->lighting == oldRoadBefore.lighting &&
                      oldRoadAfter->movementBlockedMask == oldRoadBefore.movementBlockedMask &&
                      oldRoadAfter->lineOfSightBlockedMask == oldRoadBefore.lineOfSightBlockedMask &&
                      newRoadAfter && newRoadAfter->wayKind == MapWayKind::DirtRoad &&
                      newRiverAfter && newRiverAfter->kind == MapTileKind::Water &&
                      newRiverAfter->wayKind == MapWayKind::River && crossingAfter &&
                      crossingAfter->kind == MapTileKind::Bridge,
                  "Rebase must clear stale raster geometry, project roads/rivers/bridges and preserve authored cell state");
    ok &= require(sameCellContract(outsideBefore, *mapCellAt(result.map, 49, 49)),
                  "Rebase must leave every raster cell outside its impact set byte-equivalent");
    ok &= require(result.map.regions.size() == 2 &&
                      std::any_of(result.map.regions.begin(), result.map.regions.end(), [](const MapRegionModel &region) {
                          return region.metadata.id == "local-region" && region.name == "GM region";
                      }) &&
                      std::any_of(result.map.regions.begin(), result.map.regions.end(), [](const MapRegionModel &region) {
                          return region.metadata.id == "north-march@atlas_region" &&
                                 region.name == "Expanded North March";
                      }) &&
                      result.map.routes.size() == 2 &&
                      std::any_of(result.map.routes.begin(), result.map.routes.end(), [](const MapRouteModel &route) {
                          return route.metadata.id == "trade-route@atlas_route" && route.startY == 40;
                      }),
                  "Rebase must replace inherited regions/routes while retaining local child entries");
    ok &= require(result.map.tokens.size() == 1 &&
                      result.map.tokens.front().metadata.id == "token-preserved" &&
                      result.map.features.size() == 1 &&
                      result.map.features.front().gmNotes == "Secret ford",
                  "Atlas synchronization must preserve tokens and GM annotations exactly");
    ok &= require(std::any_of(result.map.generation.graph.features.begin(), result.map.generation.graph.features.end(), [](const MapGeneratedFeature &feature) {
                      return feature.id == "local-place";
                  }),
                  "A rebase must leave local features untouched");

    MapModel rollbackChild = child;
    const std::uint64_t rollbackRevision = rollbackChild.revision;
    const std::vector<MapCellModel> rollbackCells = rollbackChild.cells;
    const std::vector<MapGeneratedFeature> rollbackGraph = rollbackChild.generation.graph.features;
    const MapAtlasRebaseResult missingDecision =
        MapAtlasService::applyRebaseAtomically(rollbackChild, parent, preview, {});
    ok &= require(!missingDecision.applied && !missingDecision.errors.empty() &&
                      rollbackChild.revision == rollbackRevision &&
                      rollbackChild.cells.size() == rollbackCells.size() &&
                      std::equal(rollbackChild.cells.begin(), rollbackChild.cells.end(),
                                 rollbackCells.begin(), sameCellContract) &&
                      rollbackChild.generation.graph.features.size() == rollbackGraph.size(),
                  "A failed in-place rebase must roll back without changing graph, raster or revision");

    MapModel sameRevisionChangedParent = parent;
    sameRevisionChangedParent.generation.graph.features.front().points.front().x += 3.0f;
    const MapAtlasRebaseResult staleContent = MapAtlasService::applyRebaseAtomically(
        rollbackChild, sameRevisionChangedParent, preview, decisions);
    ok &= require(!staleContent.applied && rollbackChild.revision == rollbackRevision &&
                      std::equal(rollbackChild.cells.begin(), rollbackChild.cells.end(),
                                 rollbackCells.begin(), sameCellContract),
                  "Content fingerprints must reject same-revision parent edits and preserve the child");

    MapModel committedChild = child;
    const MapAtlasRebaseResult committed = MapAtlasService::applyRebaseAtomically(
        committedChild, parent, preview, decisions);
    ok &= require(committed.applied && committedChild.revision == child.revision + 1 &&
                      committedChild.atlasPlacement.parentRevision == parent.revision,
                  "The service-level atomic apply must swap in a fully validated rebase candidate");
    return ok;
}

bool testRegionAndRouteRebaseDecisions()
{
    MapModel parent = makeMap("world-collection-rebase", "World", MapLevel::World, 1);
    MapRegionModel sourceRegion;
    sourceRegion.metadata.id = "source-region";
    sourceRegion.metadata.tags = {"frontier"};
    sourceRegion.metadata.customProperties["campaign.note"] = "parent-region-note";
    sourceRegion.name = "Northern March";
    sourceRegion.description = "Cold uplands";
    sourceRegion.x = 10;
    sourceRegion.y = 12;
    sourceRegion.width = 35;
    sourceRegion.height = 24;
    sourceRegion.movementCost = 2;
    parent.regions.push_back(sourceRegion);

    MapRouteModel sourceRoute;
    sourceRoute.metadata.id = "source-route";
    sourceRoute.metadata.tags = {"trade"};
    sourceRoute.metadata.customProperties["campaign.note"] = "parent-route-note";
    sourceRoute.name = "Amber Road";
    sourceRoute.startX = 0;
    sourceRoute.startY = 40;
    sourceRoute.endX = 100;
    sourceRoute.endY = 40;
    sourceRoute.distanceMiles = 120.0f;
    sourceRoute.terrain = "upland";
    sourceRoute.notes = "Old milestones";
    parent.routes.push_back(sourceRoute);

    MapModel child = makeMap("region-collection-rebase", "Region", MapLevel::Region, 5);
    child.width = 50;
    child.height = 50;
    resizeMapCells(child);
    linkTo(child, parent, {0.0f, 0.0f, 1.0f, 1.0f});
    const MapAtlasExtractionResult initial =
        MapAtlasService::extractFromParent(parent, child.atlasPlacement, child.width, child.height);
    child.regions = initial.regions;
    child.routes = initial.routes;

    // Both inherited entries were changed locally through ordinary map editing.
    child.regions.front().name = "GM-shaped Northern March";
    child.regions.front().x = 3;
    child.regions.front().width = 17;
    child.regions.front().metadata.tags.push_back("locally-shaped");
    child.regions.front().metadata.customProperties["gm.override"] = "keep this";
    child.routes.front().name = "GM Amber Road";
    child.routes.front().startY = 9;
    child.routes.front().endY = 13;
    child.routes.front().notes = "Secret local ford";
    child.routes.front().metadata.tags.push_back("locally-rerouted");
    child.routes.front().metadata.customProperties["gm.override"] = "keep this too";

    MapRegionModel unrelatedRegion;
    unrelatedRegion.metadata.id = "local-region-unrelated";
    unrelatedRegion.metadata.tags = {"gm-only"};
    unrelatedRegion.metadata.customProperties["stable"] = "region";
    unrelatedRegion.name = "Local Grove";
    unrelatedRegion.x = 31;
    unrelatedRegion.y = 32;
    unrelatedRegion.width = 7;
    unrelatedRegion.height = 8;
    unrelatedRegion.gmOnly = true;
    child.regions.push_back(unrelatedRegion);
    MapRouteModel unrelatedRoute;
    unrelatedRoute.metadata.id = "local-route-unrelated";
    unrelatedRoute.metadata.tags = {"gm-only"};
    unrelatedRoute.metadata.customProperties["stable"] = "route";
    unrelatedRoute.name = "Local Footpath";
    unrelatedRoute.startX = 30;
    unrelatedRoute.startY = 30;
    unrelatedRoute.endX = 35;
    unrelatedRoute.endY = 37;
    unrelatedRoute.notes = "Never inherited";
    child.routes.push_back(unrelatedRoute);

    const MapRegionModel locallyEditedRegion = child.regions.front();
    const MapRouteModel locallyEditedRoute = child.routes.front();

    // The parent independently changes the same semantic objects.
    parent.revision = 2;
    parent.regions.front().name = "Expanded Northern March";
    parent.regions.front().x = 20;
    parent.regions.front().width = 45;
    parent.regions.front().description = "The new northern frontier";
    parent.routes.front().name = "Royal Amber Road";
    parent.routes.front().startY = 76;
    parent.routes.front().endY = 76;
    parent.routes.front().notes = "Rebuilt by the crown";

    const MapAtlasRebasePreview preview = MapAtlasService::previewRebase(child, parent);
    const auto regionItem = std::find_if(preview.items.begin(), preview.items.end(), [](const MapAtlasRebaseItem &item) {
        return item.kind == MapAtlasRebaseItemKind::Region;
    });
    const auto routeItem = std::find_if(preview.items.begin(), preview.items.end(), [](const MapAtlasRebaseItem &item) {
        return item.kind == MapAtlasRebaseItemKind::Route;
    });

    bool ok = true;
    ok &= require(preview.valid && preview.items.size() == 2 &&
                      regionItem != preview.items.end() && routeItem != preview.items.end() &&
                      regionItem->requiresDecision && routeItem->requiresDecision &&
                      regionItem->change == MapAtlasRebaseChange::ChangedByParent &&
                      routeItem->change == MapAtlasRebaseChange::ChangedByParent &&
                      regionItem->sourceEntityId == "source-region" &&
                      routeItem->sourceEntityId == "source-route",
                  "Locally edited inherited regions and routes must appear as explicit rebase decisions");
    if (regionItem == preview.items.end() || routeItem == preview.items.end())
        return false;

    const std::vector<MapRegionModel> originalRegions = child.regions;
    const std::vector<MapRouteModel> originalRoutes = child.routes;
    const std::uint64_t originalRevision = child.revision;
    MapModel missingDecisionChild = child;
    const MapAtlasRebaseResult missingDecision = MapAtlasService::applyRebaseAtomically(
        missingDecisionChild, parent, preview,
        {{regionItem->id, MapAtlasRebaseResolution::UseParent}});
    ok &= require(!missingDecision.applied && missingDecisionChild.revision == originalRevision &&
                      missingDecisionChild.regions.size() == originalRegions.size() &&
                      std::equal(missingDecisionChild.regions.begin(), missingDecisionChild.regions.end(),
                                 originalRegions.begin(), sameRegionContract) &&
                      missingDecisionChild.routes.size() == originalRoutes.size() &&
                      std::equal(missingDecisionChild.routes.begin(), missingDecisionChild.routes.end(),
                                 originalRoutes.begin(), sameRouteContract),
                  "A missing region or route decision must reject the complete rebase without mutation");

    const auto findRegion = [](const MapModel &map, const EntityId &id) -> const MapRegionModel * {
        const auto found = std::find_if(map.regions.begin(), map.regions.end(), [&](const MapRegionModel &entry) {
            return entry.metadata.id == id;
        });
        return found == map.regions.end() ? nullptr : &*found;
    };
    const auto findRoute = [](const MapModel &map, const EntityId &id) -> const MapRouteModel * {
        const auto found = std::find_if(map.routes.begin(), map.routes.end(), [&](const MapRouteModel &entry) {
            return entry.metadata.id == id;
        });
        return found == map.routes.end() ? nullptr : &*found;
    };
    const auto decisionsFor = [&](MapAtlasRebaseResolution regionResolution,
                                  MapAtlasRebaseResolution routeResolution) {
        return std::vector<MapAtlasRebaseDecision>{{regionItem->id, regionResolution},
                                                   {routeItem->id, routeResolution}};
    };

    const MapAtlasRebaseResult useParent = MapAtlasService::applyRebase(
        child, parent, preview,
        decisionsFor(MapAtlasRebaseResolution::UseParent,
                     MapAtlasRebaseResolution::UseParent));
    const MapRegionModel *parentRegionResult =
        useParent.applied ? findRegion(useParent.map, locallyEditedRegion.metadata.id) : nullptr;
    const MapRouteModel *parentRouteResult =
        useParent.applied ? findRoute(useParent.map, locallyEditedRoute.metadata.id) : nullptr;
    ok &= require(useParent.applied && parentRegionResult && parentRouteResult &&
                      regionItem->parentRegion && routeItem->parentRoute &&
                      sameRegionContract(*parentRegionResult, *regionItem->parentRegion) &&
                      sameRouteContract(*parentRouteResult, *routeItem->parentRoute),
                  "Use parent must apply the current inherited region and route including geometry and lineage");

    const MapAtlasRebaseResult keepLocal = MapAtlasService::applyRebase(
        child, parent, preview,
        decisionsFor(MapAtlasRebaseResolution::KeepLocal,
                     MapAtlasRebaseResolution::KeepLocal));
    const MapRegionModel *keptRegion =
        keepLocal.applied ? findRegion(keepLocal.map, locallyEditedRegion.metadata.id) : nullptr;
    const MapRouteModel *keptRoute =
        keepLocal.applied ? findRoute(keepLocal.map, locallyEditedRoute.metadata.id) : nullptr;
    ok &= require(keepLocal.applied && keptRegion && keptRoute &&
                      sameRegionContract(*keptRegion, locallyEditedRegion) &&
                      sameRouteContract(*keptRoute, locallyEditedRoute),
                  "Keep local must preserve exact region and route IDs, geometry, metadata and atlas lineage");

    const MapAtlasRebaseResult detach = MapAtlasService::applyRebase(
        child, parent, preview,
        decisionsFor(MapAtlasRebaseResolution::Detach,
                     MapAtlasRebaseResolution::Detach));
    const MapRegionModel *detachedRegion =
        detach.applied ? findRegion(detach.map, locallyEditedRegion.metadata.id) : nullptr;
    const MapRouteModel *detachedRoute =
        detach.applied ? findRoute(detach.map, locallyEditedRoute.metadata.id) : nullptr;
    MapRegionModel expectedDetachedRegion = locallyEditedRegion;
    MapRouteModel expectedDetachedRoute = locallyEditedRoute;
    const auto clearAtlasLineage = [](EntityMetadata &metadata) {
        metadata.customProperties.erase("atlas.source_map_id");
        metadata.customProperties.erase("atlas.source_entity_id");
        metadata.customProperties.erase("atlas.source_revision");
        metadata.customProperties.erase("atlas.source_kind");
    };
    clearAtlasLineage(expectedDetachedRegion.metadata);
    clearAtlasLineage(expectedDetachedRoute.metadata);
    ok &= require(detach.applied && detachedRegion && detachedRoute &&
                      sameRegionContract(*detachedRegion, expectedDetachedRegion) &&
                      sameRouteContract(*detachedRoute, expectedDetachedRoute),
                  "Detach must preserve local IDs and geometry while removing only atlas lineage");

    for (const MapAtlasRebaseResult *result : {&useParent, &keepLocal, &detach})
    {
        const MapRegionModel *stableRegion = findRegion(result->map, unrelatedRegion.metadata.id);
        const MapRouteModel *stableRoute = findRoute(result->map, unrelatedRoute.metadata.id);
        ok &= require(stableRegion && stableRoute &&
                          sameRegionContract(*stableRegion, unrelatedRegion) &&
                          sameRouteContract(*stableRoute, unrelatedRoute),
                      "Every rebase resolution must preserve unrelated region and route data exactly");
    }
    return ok;
}

bool sameFeatureContract(const MapGeneratedFeature &left, const MapGeneratedFeature &right)
{
    if (left.id != right.id || left.kind != right.kind || left.name != right.name ||
        left.materialId != right.materialId || !nearlyEqual(left.width, right.width) ||
        left.closed != right.closed || left.locked != right.locked ||
        left.discovered != right.discovered || left.gmOnly != right.gmOnly ||
        left.lineage.sourceMapId != right.lineage.sourceMapId ||
        left.lineage.sourceFeatureId != right.lineage.sourceFeatureId ||
        left.lineage.sourceRevision != right.lineage.sourceRevision ||
        left.points.size() != right.points.size())
        return false;
    for (std::size_t index = 0; index < left.points.size(); ++index)
        if (!nearlyEqual(left.points[index].x, right.points[index].x) ||
            !nearlyEqual(left.points[index].y, right.points[index].y))
            return false;
    return true;
}

bool testInheritedAnchorsParticipateBeforeGeneration()
{
    MapModel parent = makeMap("atlas-source", "Atlas Source", MapLevel::World, 12);
    MapGeneratedFeature river;
    river.id = "great-river";
    river.kind = MapGeneratedFeatureKind::River;
    river.name = "Great River";
    river.materialId = "fantasy_neutral.water.river";
    river.width = 1.4f;
    river.points = {{50.0f, 0.0f}, {48.0f, 50.0f}, {50.0f, 100.0f}};
    parent.generation.graph.features.push_back(river);
    MapGeneratedFeature road;
    road.id = "kings-road";
    road.kind = MapGeneratedFeatureKind::Road;
    road.name = "King's Road";
    road.materialId = "fantasy_neutral.road.dirt";
    road.width = 0.8f;
    road.points = {{0.0f, 20.0f}, {100.0f, 20.0f}};
    parent.generation.graph.features.push_back(road);
    MapGeneratedFeature place;
    place.id = "old-harbor";
    place.kind = MapGeneratedFeatureKind::Settlement;
    place.name = "Old Harbor";
    place.materialId = "fantasy_neutral.settlement.town";
    place.points = {{14.0f, 64.0f}};
    parent.generation.graph.features.push_back(place);

    MapAtlasPlacement placement;
    placement.parentId = parent.metadata.id;
    placement.parentRect = {0.0f, 0.0f, 1.0f, 1.0f};
    placement.parentRevision = parent.revision;
    const MapAtlasExtractionResult inherited =
        MapAtlasService::extractFromParent(parent, placement, 64, 48);

    MapGenerationOptions options;
    options.generatorVersion = 5;
    options.scope = MapScope::World;
    options.scale = MapLevel::Region;
    options.creationIntent = MapCreationIntent::RegionExploration;
    options.width = 64;
    options.height = 48;
    options.seed = 94017u;
    options.requestedSettlements = 2;
    options.requestedLandmarks = 0;
    options.edgeConnectionCount = 0;
    options.waterLevel = 28;
    options.preservedFeatures = inherited.graph.features;
    options.constraints = inherited.generationConstraints;

    const MapGenerationResult generated = mapGenGenerateCandidate(options);
    bool ok = true;
    for (const MapGeneratedFeature &expected : inherited.graph.features)
    {
        const auto actual = std::find_if(generated.map.generation.graph.features.begin(),
                                         generated.map.generation.graph.features.end(),
                                         [&](const MapGeneratedFeature &feature) {
                                             return feature.id == expected.id;
                                         });
        ok &= require(actual != generated.map.generation.graph.features.end() &&
                          sameFeatureContract(*actual, expected),
                      "Pre-generation atlas anchors must retain exact identity, geometry, lineage and lock state");
        ok &= require(std::count_if(generated.map.generation.graph.features.begin(),
                                    generated.map.generation.graph.features.end(),
                                    [&](const MapGeneratedFeature &feature) {
                                        return feature.id == expected.id;
                                    }) == 1,
                      "Preserved atlas anchors must not be duplicated by generator repair passes");
    }

    const auto inheritedPlace = std::find_if(inherited.graph.features.begin(), inherited.graph.features.end(),
                                              [](const MapGeneratedFeature &feature) {
                                                  return feature.kind == MapGeneratedFeatureKind::Settlement;
                                              });
    ok &= require(inheritedPlace != inherited.graph.features.end(),
                  "Atlas test fixture should contain an inherited settlement");
    if (inheritedPlace != inherited.graph.features.end())
    {
        const MapFeaturePoint point = inheritedPlace->points.front();
        const bool generatedRouteUsesPlace = std::any_of(
            generated.map.generation.graph.features.begin(),
            generated.map.generation.graph.features.end(),
            [&](const MapGeneratedFeature &feature) {
                if ((feature.kind != MapGeneratedFeatureKind::Road &&
                     feature.kind != MapGeneratedFeatureKind::Trail) ||
                    feature.id == road.id || feature.points.empty())
                    return false;
                const auto touches = [&](const MapFeaturePoint &candidate) {
                    return std::abs(candidate.x - point.x) <= 0.51f &&
                           std::abs(candidate.y - point.y) <= 0.51f;
                };
                return touches(feature.points.front()) || touches(feature.points.back());
            });
        ok &= require(generatedRouteUsesPlace,
                      "Route planning must use an inherited settlement as a network node before final merge");
        const MapCellModel *cell = mapCellAt(
            generated.map, static_cast<int>(std::floor(point.x)),
            static_cast<int>(std::floor(point.y)));
        ok &= require(cell && cell->kind == MapTileKind::Settlement,
                      "Inherited places must project into the semantic raster");
    }

    const bool fabricatedEdgeLandmark = std::any_of(
        generated.map.generation.graph.features.begin(),
        generated.map.generation.graph.features.end(),
        [&](const MapGeneratedFeature &feature) {
            return std::any_of(inherited.generationConstraints.begin(), inherited.generationConstraints.end(),
                               [&](const MapGenerationConstraint &constraint) {
                                   return feature.id == constraint.id;
                               });
        });
    ok &= require(!fabricatedEdgeLandmark,
                  "Atlas edge constraints must not be converted into point landmarks");
    MapGenerationQualityReport quality = analyzeMapGenerationQuality(generated.map);
    const MapGenerationValidationReport validation = validateMapGeneration(generated.map, quality);
    ok &= require(std::none_of(validation.blockers.begin(), validation.blockers.end(),
                               [](const MapGenerationIssue &issue) {
                                   return issue.code == "requirement.constraint_missing";
                               }),
                  "Satisfied inherited boundary topology must not create a false constraint blocker");
    return ok;
}

bool testSettlementPlanningProtectsInheritedRoadGeometry()
{
    MapGeneratedFeature inheritedRoad;
    inheritedRoad.id = "atlas-road@atlas_0";
    inheritedRoad.kind = MapGeneratedFeatureKind::Road;
    inheritedRoad.name = "Inherited Road";
    inheritedRoad.materialId = "fantasy_neutral.road.dirt";
    inheritedRoad.locked = true;
    inheritedRoad.lineage = {"parent-region", "atlas-road", 9};
    // A dense serpentine fixture intentionally covers every potential building
    // row. If this road were merged only afterwards, generated buildings would
    // remain underneath it in the raster.
    for (int y = 0; y < 36; ++y)
    {
        if ((y & 1) == 0)
        {
            inheritedRoad.points.push_back({0.5f, static_cast<float>(y) + 0.5f});
            inheritedRoad.points.push_back({47.5f, static_cast<float>(y) + 0.5f});
        }
        else
        {
            inheritedRoad.points.push_back({47.5f, static_cast<float>(y) + 0.5f});
            inheritedRoad.points.push_back({0.5f, static_cast<float>(y) + 0.5f});
        }
    }

    MapGenerationOptions options;
    options.generatorVersion = 5;
    options.scope = MapScope::Local;
    options.scale = MapLevel::Site;
    options.creationIntent = MapCreationIntent::Settlement;
    options.purpose = MapGenerationPurpose::SettlementEnvirons;
    options.width = 48;
    options.height = 36;
    options.seed = 103u;
    options.preservedFeatures = {inheritedRoad};
    const MapGenerationResult generated = mapGenGenerateCandidate(options);

    bool ok = true;
    ok &= require(std::none_of(generated.map.cells.begin(), generated.map.cells.end(),
                               [](const MapCellModel &cell) {
                                   return cell.kind == MapTileKind::Building &&
                                          mapGenWayKindIsRoadLike(cell.wayKind);
                               }),
                  "Settlement structures must be planned around inherited roads, not left underneath a final merge");
    const auto restored = std::find_if(generated.map.generation.graph.features.begin(),
                                       generated.map.generation.graph.features.end(),
                                       [&](const MapGeneratedFeature &feature) {
                                           return feature.id == inheritedRoad.id;
                                       });
    ok &= require(restored != generated.map.generation.graph.features.end() &&
                      sameFeatureContract(*restored, inheritedRoad),
                  "Settlement generation must restore the exact inherited road contract");
    return ok;
}
}

bool testPrimaryWorldMap()
{
    MapModel region = makeMap("region", "Region", MapLevel::Region, 1);
    MapModel firstWorld = makeMap("world-a", "First World", MapLevel::World, 1);
    MapModel secondWorld = makeMap("world-b", "Second World", MapLevel::World, 1);
    std::vector<MapModel> maps{region, firstWorld, secondWorld};
    bool ok = true;
    ok &= require(MapAtlasService::primaryWorldMap(maps) == &maps[1],
                  "A legacy world root should anchor an atlas without a main-map marker");
    maps[2].isMainMap = true;
    ok &= require(MapAtlasService::primaryWorldMap(maps) == &maps[2],
                  "The explicitly designated main world should take precedence");
    maps.erase(maps.begin() + 1, maps.end());
    ok &= require(MapAtlasService::primaryWorldMap(maps) == nullptr,
                  "An atlas without a world map should have no world anchor");
    return ok;
}

int main()
{
    bool ok = true;
    ok &= testPrimaryWorldMap();
    ok &= testValidationHierarchyAndBreadcrumbs();
    ok &= testCoordinateTransformsAndExtraction();
    ok &= testStaleDetectionAndPromotionPreview();
    ok &= testExplicitRebase();
    ok &= testRegionAndRouteRebaseDecisions();
    ok &= testInheritedAnchorsParticipateBeforeGeneration();
    ok &= testSettlementPlanningProtectsInheritedRoadGeometry();
    return ok ? 0 : 1;
}
