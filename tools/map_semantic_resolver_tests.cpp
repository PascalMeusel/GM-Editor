#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapRuntimeVisualAssets.hpp"
#include "Models/Map/MapStyleRegistry.hpp"
#include "Models/Map/MapTileSemantics.hpp"

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

bool hasRelationshipMask(const MapTileSemanticResolution &resolution, MapTileKind neighborKind, int mask)
{
    for (const MapTileNeighborRelationship &relationship : resolution.neighborRelationships)
    {
        if (relationship.neighborKind == neighborKind && relationship.mask == mask)
            return true;
    }
    return false;
}

bool hasTransitionOverlay(const MapTileSemanticResolution &resolution, const std::string &transitionKey, int mask)
{
    for (const MapTileResolvedTransitionOverlay &overlay : resolution.transitionOverlays)
    {
        if (overlay.transitionKey == transitionKey && overlay.mask == mask)
            return true;
    }
    return false;
}

bool containsText(const std::string &text, const std::string &needle)
{
    return text.find(needle) != std::string::npos;
}

MapTileRenderContext contextWithOneNeighbor(MapTileKind owner, MapTileKind neighbor, MapTileNeighborSlot slot)
{
    MapCellModel neighborCell;
    applyMapCellTerrainDefaults(neighborCell, neighbor);

    const MapCellModel *north = slot == MapTileNeighborSlot::North ? &neighborCell : nullptr;
    const MapCellModel *east = slot == MapTileNeighborSlot::East ? &neighborCell : nullptr;
    const MapCellModel *south = slot == MapTileNeighborSlot::South ? &neighborCell : nullptr;
    const MapCellModel *west = slot == MapTileNeighborSlot::West ? &neighborCell : nullptr;
    return makeMapTileRenderContext(owner, 0, 10, 10, north, east, south, west);
}

MapTileRenderContext contextForMapCell(const MapModel &map, int x, int y)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    return makeMapTileRenderContext(
        cell,
        x,
        y,
        mapCellAt(map, x, y - 1),
        mapCellAt(map, x + 1, y),
        mapCellAt(map, x, y + 1),
        mapCellAt(map, x - 1, y));
}

MapWayRenderContext wayContextForMapCell(const MapModel &map, int x, int y)
{
    return makeMapWayRenderContext(
        mapCellAt(map, x, y),
        x,
        y,
        mapCellAt(map, x, y - 1),
        mapCellAt(map, x + 1, y),
        mapCellAt(map, x, y + 1),
        mapCellAt(map, x - 1, y),
        mapCellAt(map, x + 1, y - 1),
        mapCellAt(map, x + 1, y + 1),
        mapCellAt(map, x - 1, y + 1),
        mapCellAt(map, x - 1, y - 1));
}

bool testCardinalMasks()
{
    bool ok = true;
    ok &= require(mapTileCardinalMask(true, false, false, false) == 1, "North mask should be 1");
    ok &= require(mapTileCardinalMask(false, true, false, false) == 2, "East mask should be 2");
    ok &= require(mapTileCardinalMask(false, false, true, false) == 4, "South mask should be 4");
    ok &= require(mapTileCardinalMask(false, false, false, true) == 8, "West mask should be 8");
    ok &= require(mapTileCardinalMask(true, true, true, true) == 15, "All-direction mask should be 15");
    return ok;
}

bool testContextualMountainRangeSemantics()
{
    MapModel map;
    map.width = 7;
    map.height = 5;
    resizeMapCells(map);

    for (int x = 1; x <= 5; ++x)
    {
        MapCellModel *cell = mapCellAt(map, x, 2);
        if (cell)
            applyMapCellTerrainDefaults(*cell, MapTileKind::Mountain);
    }

    const MapTileRenderContext leftEndContext = contextForMapCell(map, 1, 2);
    const MapTileRenderContext middleContext = contextForMapCell(map, 3, 2);
    const MapTileRenderContext rightEndContext = contextForMapCell(map, 5, 2);

    const MapTileSemanticResolution leftEnd = resolveMapTileSemantics(leftEndContext);
    const MapTileSemanticResolution middle = resolveMapTileSemantics(middleContext);
    const MapTileSemanticResolution rightEnd = resolveMapTileSemantics(rightEndContext);

    bool ok = true;
    ok &= require(mapCellAt(map, 3, 2)->kind == MapTileKind::Mountain, "Mountain range paint should preserve Mountain semantic kind");
    ok &= require(leftEnd.sameFamilyMask == 2, "Left end of east-west mountain range should connect east only");
    ok &= require(middle.sameFamilyMask == 10, "Middle of east-west mountain range should connect east and west");
    ok &= require(rightEnd.sameFamilyMask == 8, "Right end of east-west mountain range should connect west only");
    ok &= require(leftEnd.contextualSymbolVariant.enabled, "Mountain should use contextual symbolic variants");
    ok &= require(middle.contextualSymbolVariant.owner == MapTileKind::Mountain, "Mountain contextual variant should keep Mountain ownership");
    ok &= require(middle.contextualSymbolVariant.sameFamilyMask == middle.sameFamilyMask, "Mountain contextual variant should be driven by same-family mask");
    ok &= require(contextualSymbolCellIndex(leftEndContext, 22, mapTileContextualSymbolVariantCount(MapTileKind::Mountain)) == 0, "Connected mountain end should choose the range-end variant");
    ok &= require(contextualSymbolCellIndex(middleContext, 22, mapTileContextualSymbolVariantCount(MapTileKind::Mountain)) == 1, "Connected mountain middle should choose the connected-range variant");
    ok &= require(contextualSymbolCellIndex(rightEndContext, 22, mapTileContextualSymbolVariantCount(MapTileKind::Mountain)) == 0, "Connected mountain end should choose the range-end variant");
    return ok;
}

bool testIndividualMountainVariantMode()
{
    MapModel map;
    map.width = 5;
    map.height = 3;
    resizeMapCells(map);

    for (int x = 1; x <= 3; ++x)
    {
        MapCellModel *cell = mapCellAt(map, x, 1);
        if (cell)
        {
            applyMapCellTerrainDefaults(*cell, MapTileKind::Mountain);
            cell->symbolVariantMode = MapSymbolVariantMode::Individual;
        }
    }

    const MapTileSemanticResolution middle = resolveMapTileSemantics(contextForMapCell(map, 2, 1));
    bool ok = true;
    ok &= require(middle.sameFamilyMask == 10, "Individual mountain mode should preserve same-family mask for transitions");
    ok &= require(middle.contextualSymbolVariant.sameFamilyMask == 0, "Individual mountain mode should ignore adjacency for symbolic shape selection");
    ok &= require(contextualSymbolCellIndex(contextForMapCell(map, 2, 1), 22, mapTileContextualSymbolVariantCount(MapTileKind::Mountain)) <= 1, "Individual mountain mode should still choose a valid mountain cell");
    return ok;
}

bool testWayMasksAndDirtyCells()
{
    MapModel map;
    map.width = 5;
    map.height = 5;
    resizeMapCells(map);

    for (const MapCellCoordinate &coordinate : {MapCellCoordinate{2, 2}, MapCellCoordinate{2, 1}, MapCellCoordinate{3, 2}, MapCellCoordinate{3, 3}})
    {
        MapCellModel *cell = mapCellAt(map, coordinate.x, coordinate.y);
        if (cell)
            cell->wayKind = MapWayKind::DirtRoad;
    }
    MapCellModel *riverCell = mapCellAt(map, 1, 2);
    if (riverCell)
        riverCell->wayKind = MapWayKind::River;

    const MapWayRenderContext context = wayContextForMapCell(map, 2, 2);
    const int mask = mapWayEightNeighborMask(context);

    const std::vector<MapCellCoordinate> footprint = {MapCellCoordinate{2, 2}};
    const std::vector<MapCellCoordinate> dirtyCells = mapWayDirtyCellsForFootprint(map, footprint);

    bool ok = true;
    ok &= require((mask & 1) != 0, "Way mask should include north connection bit 1");
    ok &= require((mask & 4) != 0, "Way mask should include east connection bit 4");
    ok &= require((mask & 8) != 0, "Way mask should include south-east connection bit 8");
    ok &= require((mask & 64) == 0, "Way mask should not connect road to river west neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 1, 1), "Way dirty ring should include diagonals");
    ok &= require(mapCellCoordinateInList(dirtyCells, 3, 3), "Way dirty ring should include south-east neighbor");
    return ok;
}

bool testGrassMountainReciprocalRelationships()
{
    struct Case
    {
        MapTileNeighborSlot grassNeighborSlot;
        int grassMask = 0;
        MapTileNeighborSlot mountainNeighborSlot;
        int mountainMask = 0;
    };

    const std::vector<Case> cases = {
        {MapTileNeighborSlot::North, 1, MapTileNeighborSlot::South, 4},
        {MapTileNeighborSlot::East, 2, MapTileNeighborSlot::West, 8},
        {MapTileNeighborSlot::South, 4, MapTileNeighborSlot::North, 1},
        {MapTileNeighborSlot::West, 8, MapTileNeighborSlot::East, 2}};

    bool ok = true;
    for (const Case &testCase : cases)
    {
        const MapTileSemanticResolution grassResolution =
            resolveMapTileSemantics(contextWithOneNeighbor(MapTileKind::Empty, MapTileKind::Mountain, testCase.grassNeighborSlot));
        ok &= require(
            hasRelationshipMask(grassResolution, MapTileKind::Mountain, testCase.grassMask),
            "Grass owner did not record mountain relationship with expected cardinal mask");

        const MapTileSemanticResolution mountainResolution =
            resolveMapTileSemantics(contextWithOneNeighbor(MapTileKind::Mountain, MapTileKind::Empty, testCase.mountainNeighborSlot));
        ok &= require(
            hasRelationshipMask(mountainResolution, MapTileKind::Empty, testCase.mountainMask),
            "Mountain owner did not record reciprocal grass relationship with expected cardinal mask");
    }

    return ok;
}

bool testConfiguredTransitionOverlays()
{
    bool ok = true;
    const MapTileSemanticResolution grassRockResolution =
        resolveMapTileSemantics(contextWithOneNeighbor(MapTileKind::Empty, MapTileKind::Mountain, MapTileNeighborSlot::West));
    ok &= require(hasTransitionOverlay(grassRockResolution, "grass_open_to_rock", 8), "Grass-to-rock overlay should resolve on west mask 8");

    const MapTileSemanticResolution sameGrassResolution =
        resolveMapTileSemantics(contextWithOneNeighbor(MapTileKind::Empty, MapTileKind::Empty, MapTileNeighborSlot::West));
    ok &= require(sameGrassResolution.transitionOverlays.empty(), "Same-family neighbor should not leave a stale transition overlay");
    ok &= require(sameGrassResolution.neighborRelationships.empty(), "Same-family neighbor should not record a different-terrain relationship");

    const MapTileSemanticResolution waterSandResolution =
        resolveMapTileSemantics(contextWithOneNeighbor(MapTileKind::Water, MapTileKind::Dirt, MapTileNeighborSlot::North));
    ok &= require(hasTransitionOverlay(waterSandResolution, "water_to_sand", 1), "Water-to-sand overlay should resolve on north mask 1");
    return ok;
}

bool testRoadBridgeConnectivity()
{
    MapCellModel road;
    MapCellModel bridge;
    MapCellModel water;
    applyMapCellTerrainDefaults(road, MapTileKind::Road);
    applyMapCellTerrainDefaults(bridge, MapTileKind::Bridge);
    applyMapCellTerrainDefaults(water, MapTileKind::Water);

    const MapTileRenderContext context = makeMapTileRenderContext(
        MapTileKind::Road,
        0,
        4,
        4,
        &road,
        &bridge,
        &water,
        nullptr);
    return require(connectedMapTileMask(context) == 3, "Road should connect to road north and bridge east only");
}

bool testAutomaticTileRotation()
{
    MapCellModel road;
    MapCellModel bridge;
    MapCellModel wall;
    applyMapCellTerrainDefaults(road, MapTileKind::Road);
    applyMapCellTerrainDefaults(bridge, MapTileKind::Bridge);
    applyMapCellTerrainDefaults(wall, MapTileKind::Wall);

    MapTileRenderContext bridgeContext = makeMapTileRenderContext(
        MapTileKind::Bridge,
        0,
        4,
        4,
        nullptr,
        &road,
        nullptr,
        &road);

    MapTileRenderContext lockedBridgeContext = bridgeContext;
    lockedBridgeContext.rotationLocked = true;

    MapTileRenderContext unlockedManualBridgeContext = bridgeContext;
    unlockedManualBridgeContext.rotationQuarterTurns = 2;

    const MapTileRenderContext doorContext = makeMapTileRenderContext(
        MapTileKind::Door,
        0,
        4,
        4,
        nullptr,
        &wall,
        nullptr,
        &wall);

    bool ok = true;
    ok &= require(automaticMapTileRotation(bridgeContext) == 1, "Bridge should auto-rotate horizontally for east/west road neighbors");
    ok &= require(automaticMapTileRotation(lockedBridgeContext) == 0, "Rotation lock should preserve manual zero rotation");
    ok &= require(automaticMapTileRotation(unlockedManualBridgeContext) == 1, "Auto rotation should ignore stored manual rotation unless rotation is locked");
    ok &= require(automaticMapTileRotation(doorContext) == 1, "Door should auto-rotate horizontally when east/west neighbors are blocking walls");
    return ok;
}

bool testBrushDirtyCells()
{
    MapModel map;
    map.width = 4;
    map.height = 4;
    resizeMapCells(map);

    const std::vector<MapCellCoordinate> footprint = mapBrushFootprint(map, 1, 1, 1);
    const std::vector<MapCellCoordinate> dirtyCells = mapTerrainDirtyCellsForFootprint(map, footprint);

    bool ok = true;
    ok &= require(footprint.size() == 1, "Brush size 1 should produce one footprint cell");
    ok &= require(mapCellCoordinateInList(dirtyCells, 1, 1), "Dirty cells should include painted cell");
    ok &= require(mapCellCoordinateInList(dirtyCells, 1, 0), "Dirty cells should include north neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 2, 1), "Dirty cells should include east neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 1, 2), "Dirty cells should include south neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 0, 1), "Dirty cells should include west neighbor");
    ok &= require(!mapCellCoordinateInList(dirtyCells, 0, 0), "Terrain dirty ring should not include diagonal cells");

    const MapModel preview = previewMapTerrainBrush(map, footprint, MapTileKind::Mountain, 0, false);
    const MapCellModel *originalCell = mapCellAt(map, 1, 1);
    const MapCellModel *previewCell = mapCellAt(preview, 1, 1);
    ok &= require(originalCell && originalCell->kind == MapTileKind::Empty, "Preview brush should not mutate the original map");
    ok &= require(previewCell && previewCell->kind == MapTileKind::Mountain, "Preview brush should paint the scratch map");
    return ok;
}

bool testBuiltInStyleRegistry()
{
    bool ok = true;
    ok &= require(findMapStylePack(defaultMapStyleId()) != nullptr, "Default Fantasy Neutral style should be registered");
    ok &= require(mapStyleSupportsTileKind(defaultMapStyleId(), MapTileKind::Mountain), "Default style should support current map tile kinds");
    ok &= require(!mapStyleSupportsTileKind("sci_fi_frontier", MapTileKind::Mountain), "Planned styles should report incomplete coverage until mapped");
    ok &= require(mapStyleContextualTargetAssetCount(defaultMapStyleId()) == 590, "Default style should expose the contextual v2 work-order target count");
    ok &= require(mapStyleContextualAcceptedAssetCount(defaultMapStyleId()) == 92, "Default contextual v2 coverage should expose the accepted provisional grass-road and grass-trail slices");
    ok &= require(mapStyleContextualCandidateAssetCount(defaultMapStyleId()) == 582, "Default contextual v2 coverage should keep the remaining candidate-slot coverage separate from accepted runtime art");
    ok &= require(mapStyleContextualCompletionPercent(defaultMapStyleId()) == 15, "Default contextual v2 coverage should report the accepted provisional slice percentage");
    ok &= require(mapStyleContextualAssetCoverage().size() == 12, "Default contextual coverage should load the twelve v2 production work orders from manifests");
    const MapStylePackedRuntimeAsset *packedCandidate = findMapStylePackedRuntimeAsset("fantasy_neutral.world.base.grass_open.soft_meadow_edge_locked_retry.v209");
    ok &= require(packedCandidate && packedCandidate->status == "candidate", "Packed candidate preview manifest should expose current v2 candidate atlas records");
    ok &= require(packedCandidate && !mapStylePackedRuntimeAssetDrawable(*packedCandidate, false), "Packed candidates should not be drawable by default runtime asset lookup");
    ok &= require(packedCandidate && mapStylePackedRuntimeAssetDrawable(*packedCandidate, true), "Packed candidates should be drawable only for explicit candidate preview");
    ok &= require(packedCandidate && !mapStyleResolveExistingPath(packedCandidate->atlasPath).empty(), "Packed atlas records should resolve to an existing atlas page");
    ok &= require(mapStyleIntegratedWayOwnerKey(MapWayKind::DirtRoad, MapTileKind::Empty) == "way_dirt_road:grass_open", "Dirt road over empty outdoor ground should map to the grass road work-order key");
    ok &= require(mapStyleIntegratedWayOwnerKey(MapWayKind::Trail, MapTileKind::Empty) == "way_trail_narrow:grass_open", "Trail over empty outdoor ground should map to the narrow-trail-on-grass work-order key");
    ok &= require(mapStyleIntegratedWayOwnerKey(MapWayKind::Trail, MapTileKind::Dirt) == "way_trail_narrow:dirt", "Trail over dirt ground should map to the narrow-trail-on-dirt work-order key");
    ok &= require(mapStyleIntegratedWayOwnerKey(MapWayKind::River, MapTileKind::Water) == "way_river_stream:water_shallow", "River over shallow water should map to the river-stream-on-water work-order key");
    ok &= require(mapStyleIntegratedWayOwnerKey(MapWayKind::StoneRoad, MapTileKind::Stone) == "way_stone_road:stone_paved", "Stone road over stone should map to the paved-stone work-order key");
    ok &= require(mapStyleIntegratedWayOwnerKey(MapWayKind::StoneRoad, MapTileKind::Road) == "way_stone_road:stone_paved", "Stone road over generic road terrain should map to the paved-stone work-order key");
    ok &= require(mapStyleBridgeSequenceOwnerKey(MapWayKind::StoneRoad, MapTileKind::Water) == "bridge_stone:water_shallow_or_canal", "Stone-road water crossing should map to the stone bridge sequence key");
    ok &= require(mapStyleComponentRoleOwnerKey(MapTileKind::Mountain, MapSymbolVariantMode::Continuous) == "mountain_range_rocky", "Continuous mountains should map to rocky mountain range component coverage");
    ok &= require(mapStyleStructureContextOwnerKey(MapTileKind::Settlement) == "village_cluster", "Settlement terrain should map to the village-cluster structure context matrix");
    ok &= require(mapStyleStructureContextOwnerKey(MapTileKind::Building) == "settlement_house", "Building terrain should map to the settlement-house structure context matrix");
    ok &= require(mapStyleStructureContextOwnerKey(MapTileKind::Landmark) == "guard_tower", "Landmark terrain should map to the guard-tower structure context matrix");

    MapStyleContextualAssetRequest roadRequest;
    roadRequest.assetClass = MapStyleContextualAssetClass::IntegratedWayGround;
    roadRequest.owner = mapStyleIntegratedWayOwnerKey(MapWayKind::DirtRoad, MapTileKind::Empty);
    roadRequest.semanticContext = "way_family=way_dirt_road; ground=grass_open";
    const MapStyleContextualAssetResolution roadResolution = resolveMapStyleContextualAsset(roadRequest);
    const MapStyleContextualAssetCoverage *roadCoverage = findMapStyleContextualAssetCoverage(roadRequest.styleId, roadRequest.assetClass, roadRequest.owner);
    ok &= require(roadCoverage && roadCoverage->sourceManifest == "ways.json", "Road contextual coverage should come from the v2 ways manifest");
    ok &= require(roadResolution.workOrderId == "fn_v2_way_dirt_road_on_grass_open", "Road contextual asset lookup should bind to the dirt-road-on-grass work order");
    ok &= require(roadResolution.hasExactAcceptedAsset, "Road contextual asset lookup should expose accepted road-on-grass art");
    ok &= require(roadResolution.hasCandidateAsset, "Road contextual asset lookup should count active candidate variants");
    ok &= require(roadResolution.candidateAssetCount == 88, "Road contextual asset lookup should count only active candidate variants and ignore needs-repaint prototype attempts");
    ok &= require(!roadResolution.usedFallback, "Road contextual asset lookup should use accepted road art without falling back");
    ok &= require(roadResolution.fallbackReason.empty(), "Road contextual accepted lookup should not report a fallback reason");
    ok &= require(containsText(roadResolution.selectedAssetId, "way_dirt_road_on_grass_open"), "Road contextual runtime lookup should select accepted road-on-grass art");
    ok &= require(mapStyleContextualDrawableAtlasAssetId(roadResolution, false) == roadResolution.selectedAssetId, "Road contextual accepted lookup should expose a drawable atlas asset in accepted-only mode");
    ok &= require(mapStyleContextualDrawableAtlasAssetId(roadResolution, true) == roadResolution.selectedAssetId, "Road contextual preview should keep accepted art as the selected drawable atlas asset");
    const MapStylePackedRuntimeAsset *packedRoadAccepted = findMapStylePackedRuntimeAsset(roadResolution.selectedAssetId);
    ok &= require(packedRoadAccepted && packedRoadAccepted->status == "accepted", "Road contextual accepted IDs should resolve to packed runtime atlas assets");
    ok &= require(packedRoadAccepted && mapStylePackedRuntimeAssetDrawable(*packedRoadAccepted, false), "Road contextual accepted IDs should be drawable in accepted-only runtime mode");

    MapStyleContextualAssetRequest maskedRoadRequest = roadRequest;
    maskedRoadRequest.semanticContext = "way_family=way_dirt_road; ground=grass_open; mask=68";
    const MapStyleContextualAssetResolution maskedRoadResolution = resolveMapStyleContextualAsset(maskedRoadRequest);
    ok &= require(maskedRoadResolution.hasExactAcceptedAsset, "Road contextual lookup should find accepted art for the requested accepted mask slot");
    ok &= require(containsText(maskedRoadResolution.selectedAssetId, ".mask068."), "Road contextual runtime lookup should select accepted art from the requested mask slot");

    MapStyleContextualAssetRequest dirtRoadRequest = roadRequest;
    dirtRoadRequest.owner = mapStyleIntegratedWayOwnerKey(MapWayKind::DirtRoad, MapTileKind::Dirt);
    dirtRoadRequest.semanticContext = "way_family=way_dirt_road; ground=dirt; mask=60";
    const MapStyleContextualAssetResolution dirtRoadResolution = resolveMapStyleContextualAsset(dirtRoadRequest);
    ok &= require(dirtRoadResolution.workOrderId == "fn_v2_way_dirt_road_on_dirt", "Dirt-road-on-dirt contextual asset lookup should bind to the dirt ground work order");
    ok &= require(dirtRoadResolution.hasCandidateAsset, "Dirt-road-on-dirt contextual lookup should find active candidate variants");
    ok &= require(dirtRoadResolution.candidateAssetCount == 92, "Dirt-road-on-dirt contextual lookup should include the runtime-discovered mask060 candidates");
    ok &= require(containsText(dirtRoadResolution.candidatePreviewAssetId, ".mask060."), "Dirt-road-on-dirt contextual preview lookup should select from the requested runtime-discovered mask slot");

    MapStyleContextualAssetRequest stoneRoadRequest = roadRequest;
    stoneRoadRequest.owner = mapStyleIntegratedWayOwnerKey(MapWayKind::StoneRoad, MapTileKind::Road);
    stoneRoadRequest.semanticContext = "way_family=way_stone_road; ground=stone_paved; mask=56";
    const MapStyleContextualAssetResolution stoneRoadResolution = resolveMapStyleContextualAsset(stoneRoadRequest);
    ok &= require(stoneRoadResolution.workOrderId == "fn_v2_way_stone_road_on_stone_paved", "Stone-road contextual asset lookup should bind to the paved-stone work order");
    ok &= require(stoneRoadResolution.hasCandidateAsset, "Stone-road contextual lookup should find active candidate variants");
    ok &= require(stoneRoadResolution.candidateAssetCount == 96, "Stone-road contextual lookup should include the runtime-discovered mask024 and mask056 candidates");
    ok &= require(containsText(stoneRoadResolution.candidatePreviewAssetId, ".mask056."), "Stone-road contextual preview lookup should select from the requested runtime-discovered mask slot");

    MapStyleContextualAssetRequest trailRequest;
    trailRequest.assetClass = MapStyleContextualAssetClass::IntegratedWayGround;
    trailRequest.owner = mapStyleIntegratedWayOwnerKey(MapWayKind::Trail, MapTileKind::Empty);
    trailRequest.semanticContext = "way_family=way_trail_narrow; ground=grass_open; mask=17";
    const MapStyleContextualAssetResolution trailResolution = resolveMapStyleContextualAsset(trailRequest);
    ok &= require(trailResolution.workOrderId == "fn_v2_way_trail_narrow_on_grass_open", "Trail contextual asset lookup should bind to the narrow-trail-on-grass work order");
    ok &= require(trailResolution.hasExactAcceptedAsset, "Trail contextual asset lookup should expose accepted trail-on-grass art");
    ok &= require(!trailResolution.hasCandidateAsset, "Trail contextual asset lookup should not count superseded trail-on-grass attempts as active candidates");
    ok &= require(trailResolution.acceptedAssetCount == 44, "Trail contextual asset lookup should count the accepted trail-on-grass runtime variants");
    ok &= require(trailResolution.candidateAssetCount == 0, "Trail contextual asset lookup should report zero remaining active trail-on-grass candidates after promotion");
    ok &= require(!trailResolution.usedFallback, "Trail contextual asset lookup should use accepted trail art without falling back");
    ok &= require(trailResolution.fallbackReason.empty(), "Trail contextual accepted lookup should not report fallback metadata");
    ok &= require(containsText(trailResolution.selectedAssetId, "way_trail_narrow_on_grass_open"), "Trail contextual runtime lookup should select accepted trail-on-grass art");
    ok &= require(containsText(trailResolution.selectedAssetId, ".mask017."), "Trail contextual runtime lookup should select from the requested mask slot");
    const MapStylePackedRuntimeAsset *packedTrailAccepted = findMapStylePackedRuntimeAsset(trailResolution.selectedAssetId);
    ok &= require(packedTrailAccepted && packedTrailAccepted->status == "accepted", "Trail contextual accepted IDs should resolve to packed runtime atlas assets");
    ok &= require(packedTrailAccepted && mapStylePackedRuntimeAssetDrawable(*packedTrailAccepted, false), "Trail contextual accepted IDs should draw in accepted-only runtime mode");

    MapStyleContextualAssetRequest trailDirtRequest = trailRequest;
    trailDirtRequest.owner = mapStyleIntegratedWayOwnerKey(MapWayKind::Trail, MapTileKind::Dirt);
    trailDirtRequest.semanticContext = "way_family=way_trail_narrow; ground=dirt; mask=68";
    const MapStyleContextualAssetResolution trailDirtResolution = resolveMapStyleContextualAsset(trailDirtRequest);
    ok &= require(trailDirtResolution.workOrderId == "fn_v2_way_trail_narrow_on_dirt", "Trail-on-dirt contextual asset lookup should bind to the dirt trail work order");
    ok &= require(trailDirtResolution.hasCandidateAsset, "Trail-on-dirt contextual asset lookup should find active candidate variants");
    ok &= require(trailDirtResolution.candidateAssetCount == 44, "Trail-on-dirt contextual asset lookup should count the trail-on-dirt candidate variants");
    ok &= require(containsText(trailDirtResolution.candidatePreviewAssetId, "way_trail_narrow_on_dirt"), "Trail-on-dirt contextual candidate preview should expose a deterministic candidate asset ID");
    ok &= require(containsText(trailDirtResolution.candidatePreviewAssetId, ".mask068."), "Trail-on-dirt contextual preview lookup should select from the requested mask slot");

    MapStyleContextualAssetRequest towerRequest;
    towerRequest.assetClass = MapStyleContextualAssetClass::StructureContextMatrix;
    towerRequest.owner = mapStyleStructureContextOwnerKey(MapTileKind::Landmark);
    towerRequest.semanticContext = "structure_family=guard_tower; ground=cave_floor; entrance=cave_tunnel; orientation=s";
    const MapStyleContextualAssetResolution towerResolution = resolveMapStyleContextualAsset(towerRequest);
    const MapStyleContextualAssetCoverage *towerCoverage = findMapStyleContextualAssetCoverage(towerRequest.styleId, towerRequest.assetClass, towerRequest.owner);
    ok &= require(towerCoverage && towerCoverage->sourceManifest == "taxonomy.json", "Guard tower contextual coverage should come from the v2 taxonomy manifest");
    ok &= require(towerResolution.workOrderId == "fn_v2_guard_tower_ground_entrance_matrix", "Guard tower contextual asset lookup should bind to the ground/entrance matrix work order");
    ok &= require(towerResolution.hasCandidateAsset, "Guard tower contextual asset lookup should find candidate structure-context art");
    ok &= require(containsText(towerResolution.candidatePreviewAssetId, ".cave_threshold.v401"), "Guard tower contextual preview lookup should select from the requested ground/entrance/orientation slot");
    ok &= require(towerResolution.qaLabel == "tower_cave_grass_halo", "Guard tower contextual fallback should carry the tower ground-context QA label");

    MapStyleContextualAssetRequest houseRequest = towerRequest;
    houseRequest.owner = mapStyleStructureContextOwnerKey(MapTileKind::Building);
    houseRequest.semanticContext = "structure_family=settlement_house; ground=grass_open; entrance=way_dirt_road; orientation=e";
    const MapStyleContextualAssetResolution houseResolution = resolveMapStyleContextualAsset(houseRequest);
    ok &= require(houseResolution.workOrderId == "fn_v2_settlement_house_ground_entrance_matrix", "Building contextual asset lookup should bind to the settlement-house work order");
    ok &= require(houseResolution.hasCandidateAsset, "Building contextual asset lookup should find candidate house art");
    ok &= require(containsText(houseResolution.candidatePreviewAssetId, "settlement_house.entrance_e"), "Building contextual preview should select from the house entrance slot");

    MapStyleContextualAssetRequest villageRequest = towerRequest;
    villageRequest.owner = mapStyleStructureContextOwnerKey(MapTileKind::Settlement);
    villageRequest.semanticContext = "structure_family=village_cluster; ground=grass_open; entrance=way_dirt_road; orientation=s";
    const MapStyleContextualAssetResolution villageResolution = resolveMapStyleContextualAsset(villageRequest);
    ok &= require(villageResolution.workOrderId == "fn_v2_village_cluster_ground_entrance_matrix", "Settlement contextual asset lookup should bind to the village-cluster work order");
    ok &= require(villageResolution.hasCandidateAsset, "Settlement contextual asset lookup should find candidate village art");
    ok &= require(containsText(villageResolution.candidatePreviewAssetId, "village_cluster.entrance_s"), "Settlement contextual preview should select from the village entrance slot");

    MapStyleContextualAssetRequest missingTowerSlotRequest = towerRequest;
    missingTowerSlotRequest.semanticContext = "structure_family=guard_tower; ground=water_shallow; entrance=none; orientation=none";
    const MapStyleContextualAssetResolution missingTowerSlotResolution = resolveMapStyleContextualAsset(missingTowerSlotRequest);
    ok &= require(!missingTowerSlotResolution.hasCandidateAsset, "Guard tower contextual lookup should not preview mismatched ground/entrance candidate art");
    ok &= require(missingTowerSlotResolution.candidatePreviewAssetId.empty(), "Guard tower contextual lookup should leave missing-slot previews empty");
    ok &= require(missingTowerSlotResolution.fallbackReason == "missing_contextual_asset_slot", "Guard tower contextual missing slots should report the specific structure-context slot gap");

    MapStyleContextualAssetRequest missingRequest;
    missingRequest.assetClass = MapStyleContextualAssetClass::BridgeSequence;
    missingRequest.owner = "bridge_crystal:lava";
    const MapStyleContextualAssetResolution missingResolution = resolveMapStyleContextualAsset(missingRequest);
    ok &= require(missingResolution.workOrderId.empty(), "Unknown contextual owner should not invent a work order");
    ok &= require(missingResolution.qaLabel == "style_family_fallback", "Unknown contextual owner should be a style-family fallback");
    ok &= require(missingResolution.fallbackReason == "missing_contextual_work_order", "Unknown contextual owner should explain missing work-order coverage");
    return ok;
}

bool testRuntimeVisualStyleAssetResolution()
{
    MapCellModel grassRoad;
    applyMapCellTerrainDefaults(grassRoad, MapTileKind::Empty);
    grassRoad.wayKind = MapWayKind::DirtRoad;
    MapCellModel grassRoadNeighbor = grassRoad;
    const MapWayRenderContext roadContext = makeMapWayRenderContext(&grassRoad, 4, 4, &grassRoadNeighbor, &grassRoadNeighbor, nullptr, nullptr);
    const MapStyleContextualAssetRequest roadRequest = mapStyleContextualAssetRequestForWayVisual(roadContext);
    const MapStyleContextualAssetResolution roadResolution = resolveMapStyleContextualAssetForWayVisual(roadContext);

    MapCellModel bridgeCell;
    applyMapCellTerrainDefaults(bridgeCell, MapTileKind::Water);
    bridgeCell.wayKind = MapWayKind::DirtRoad;
    MapCellModel westApproach;
    applyMapCellTerrainDefaults(westApproach, MapTileKind::Empty);
    westApproach.wayKind = MapWayKind::DirtRoad;
    MapCellModel eastApproach = westApproach;
    const MapWayRenderContext bridgeContext = makeMapWayRenderContext(&bridgeCell, 5, 4, nullptr, &eastApproach, nullptr, &westApproach);
    const MapStyleContextualAssetRequest bridgeRequest = mapStyleContextualAssetRequestForWayVisual(bridgeContext);
    const MapStyleContextualAssetResolution bridgeResolution = resolveMapStyleContextualAssetForWayVisual(bridgeContext);
    MapCellModel bridgeNeighbor = bridgeCell;
    const MapWayRenderContext bridgeThresholdContext = makeMapWayRenderContext(&bridgeCell, 6, 4, nullptr, &bridgeNeighbor, nullptr, &westApproach);
    const MapStyleContextualAssetRequest bridgeThresholdRequest = mapStyleContextualAssetRequestForWayVisual(bridgeThresholdContext);
    const MapStyleContextualAssetResolution bridgeThresholdResolution = resolveMapStyleContextualAssetForWayVisual(bridgeThresholdContext);
    const MapWayRenderContext bridgeApproachContext = makeMapWayRenderContext(&westApproach, 4, 4, nullptr, &bridgeCell, nullptr, nullptr);
    const MapStyleContextualAssetRequest bridgeApproachRequest = mapStyleContextualAssetRequestForWayVisual(bridgeApproachContext);
    const MapStyleContextualAssetResolution bridgeApproachResolution = resolveMapStyleContextualAssetForWayVisual(bridgeApproachContext);

    MapCellModel mountain;
    applyMapCellTerrainDefaults(mountain, MapTileKind::Mountain);
    mountain.symbolVariantMode = MapSymbolVariantMode::Continuous;
    MapCellModel mountainWest = mountain;
    MapCellModel mountainEast = mountain;
    const MapTileRenderContext mountainContext = makeMapTileRenderContext(&mountain, 3, 2, nullptr, &mountainEast, nullptr, &mountainWest);
    const MapStyleContextualAssetRequest mountainRequest = mapStyleContextualAssetRequestForTileVisual(mountainContext);
    const MapStyleContextualAssetResolution mountainResolution = resolveMapStyleContextualAssetForTileVisual(mountainContext);

    MapCellModel tower;
    applyMapCellTerrainDefaults(tower, MapTileKind::Landmark);
    MapCellModel caveEntrance;
    applyMapCellTerrainDefaults(caveEntrance, MapTileKind::Cave);
    caveEntrance.wayKind = MapWayKind::CaveTunnel;
    const MapTileRenderContext towerContext = makeMapTileRenderContext(&tower, 2, 2, &caveEntrance, nullptr, nullptr, nullptr);
    const MapStyleContextualAssetRequest towerRequest = mapStyleContextualAssetRequestForTileVisual(towerContext);
    const MapStyleContextualAssetResolution towerResolution = resolveMapStyleContextualAssetForTileVisual(towerContext);
    MapCellModel dungeonEntrance;
    applyMapCellTerrainDefaults(dungeonEntrance, MapTileKind::Room);
    dungeonEntrance.wayKind = MapWayKind::DungeonCorridor;
    const MapStyleContextualAssetRequest dungeonTowerRequest =
        mapStyleContextualAssetRequestForTileVisual(makeMapTileRenderContext(&tower, 2, 3, nullptr, &dungeonEntrance, nullptr, nullptr));
    MapCellModel towerBridge;
    applyMapCellTerrainDefaults(towerBridge, MapTileKind::Bridge);
    towerBridge.wayKind = MapWayKind::DirtRoad;
    const MapStyleContextualAssetRequest bridgeheadTowerRequest =
        mapStyleContextualAssetRequestForTileVisual(makeMapTileRenderContext(&tower, 2, 4, nullptr, nullptr, &towerBridge, nullptr));
    MapCellModel stoneRoadEntrance;
    applyMapCellTerrainDefaults(stoneRoadEntrance, MapTileKind::Road);
    stoneRoadEntrance.wayKind = MapWayKind::StoneRoad;
    const MapStyleContextualAssetRequest stoneRoadTowerRequest =
        mapStyleContextualAssetRequestForTileVisual(makeMapTileRenderContext(&tower, 2, 6, nullptr, &stoneRoadEntrance, nullptr, nullptr));
    MapCellModel dirtRoadEntrance;
    applyMapCellTerrainDefaults(dirtRoadEntrance, MapTileKind::Road);
    dirtRoadEntrance.wayKind = MapWayKind::DirtRoad;
    const MapStyleContextualAssetRequest mixedRoadTowerRequest =
        mapStyleContextualAssetRequestForTileVisual(makeMapTileRenderContext(&tower, 2, 7, &dirtRoadEntrance, &stoneRoadEntrance, nullptr, nullptr));
    MapCellModel riverEntrance;
    applyMapCellTerrainDefaults(riverEntrance, MapTileKind::Water);
    riverEntrance.wayKind = MapWayKind::River;
    const MapStyleContextualAssetRequest riverTowerRequest =
        mapStyleContextualAssetRequestForTileVisual(makeMapTileRenderContext(&tower, 2, 5, nullptr, nullptr, nullptr, &riverEntrance));

    MapModel visualMap;
    visualMap.width = 3;
    visualMap.height = 3;
    resizeMapCells(visualMap);
    MapCellModel *visualRoad = mapCellAt(visualMap, 1, 1);
    if (visualRoad)
    {
        applyMapCellTerrainDefaults(*visualRoad, MapTileKind::Empty);
        visualRoad->wayKind = MapWayKind::DirtRoad;
    }
    MapCellModel *visualRoadNorth = mapCellAt(visualMap, 1, 0);
    if (visualRoadNorth)
    {
        applyMapCellTerrainDefaults(*visualRoadNorth, MapTileKind::Empty);
        visualRoadNorth->wayKind = MapWayKind::DirtRoad;
    }
    MapCellModel *visualRoadEast = mapCellAt(visualMap, 2, 1);
    if (visualRoadEast)
    {
        applyMapCellTerrainDefaults(*visualRoadEast, MapTileKind::Empty);
        visualRoadEast->wayKind = MapWayKind::DirtRoad;
    }
    const std::vector<MapStyleContextualAssetResolution> cellVisualResolutions =
        resolveMapStyleContextualAssetsForCellVisual(visualMap, 1, 1);

    const MapStyleContextualAssetResolution missingStyleResolution =
        resolveMapStyleContextualAssetForWayVisual(roadContext, "missing_style");

    bool ok = true;
    ok &= require(roadRequest.owner == "way_dirt_road:grass_open", "Runtime road visual should request the grass integrated-road owner key");
    ok &= require(containsText(roadRequest.semanticContext, "ground=grass_open"), "Runtime road visual should preserve the terrain ground context");
    ok &= require(containsText(roadRequest.semanticContext, "mask=5"), "Runtime road visual should request the eight-neighbor road mask");
    ok &= require(roadResolution.workOrderId == "fn_v2_way_dirt_road_on_grass_open", "Runtime road visual should resolve to the dirt-road-on-grass work order");
    ok &= require(containsText(roadResolution.selectedAssetId, "way_dirt_road_on_grass_open"), "Runtime road visual should select accepted road-on-grass atlas art");
    ok &= require(containsText(roadResolution.selectedAssetId, ".mask005."), "Runtime road visual should select accepted art matching the requested way mask");
    ok &= require(roadResolution.fallbackReason.empty(), "Runtime road visual should not report fallback when accepted art exists");
    const MapStylePackedRuntimeAsset *packedRoadAccepted = findMapStylePackedRuntimeAsset(roadResolution.selectedAssetId);
    ok &= require(packedRoadAccepted && packedRoadAccepted->assetClass == "way_segment", "Runtime road visual accepted IDs should be packed for runtime drawing");
    ok &= require(packedRoadAccepted && mapStylePackedRuntimeAssetDrawable(*packedRoadAccepted, false), "Runtime road visual accepted IDs should draw in accepted-only runtime mode");
    ok &= require(roadResolution.hasExactAcceptedAsset && !roadResolution.usedFallback, "Packed accepted road assets should be treated as production runtime art");

    ok &= require(bridgeRequest.assetClass == MapStyleContextualAssetClass::BridgeSequence, "Runtime bridge visual should request bridge sequence coverage");
    ok &= require(bridgeRequest.owner == "bridge_wood_simple:water_shallow", "Runtime bridge visual should resolve the wood bridge owner key");
    ok &= require(containsText(bridgeRequest.semanticContext, "approach_way=way_dirt_road"), "Runtime bridge visual should record approach way context");
    ok &= require(containsText(bridgeRequest.semanticContext, "span_position=mid_02"), "Runtime bridge visual should infer a middle span position for one-cell bridges with opposing approaches");
    ok &= require(containsText(bridgeResolution.candidatePreviewAssetId, ".mid_02."), "Runtime bridge visual should filter one-cell bridge candidate previews by inferred middle span position");
    ok &= require(bridgeResolution.qaLabel == "bridge_middle_repetition", "Runtime bridge visual should carry the bridge QA fallback label");
    ok &= require(containsText(bridgeThresholdRequest.semanticContext, "span_position=threshold_a"), "Runtime bridge visual should infer threshold span position when a bridge starts from a dry approach");
    ok &= require(containsText(bridgeThresholdResolution.candidatePreviewAssetId, ".threshold_a."), "Runtime bridge visual should filter bridge candidate previews by inferred span position");
    ok &= require(bridgeApproachRequest.assetClass == MapStyleContextualAssetClass::BridgeSequence, "Runtime dry bridge approach visual should request bridge sequence coverage");
    ok &= require(containsText(bridgeApproachRequest.semanticContext, "span_position=approach_a"), "Runtime dry bridge approach visual should infer bridge approach sequence position");
    ok &= require(containsText(bridgeApproachRequest.semanticContext, "approach_ground=grass_open"), "Runtime dry bridge approach visual should preserve the approach ground context");
    ok &= require(containsText(bridgeApproachResolution.candidatePreviewAssetId, ".approach_a."), "Runtime dry bridge approach visual should filter candidate previews by bridge approach role");

    ok &= require(mountainRequest.assetClass == MapStyleContextualAssetClass::ComponentRoleSet, "Runtime mountain visual should request component role coverage");
    ok &= require(mountainRequest.owner == "mountain_range_rocky", "Runtime continuous mountain visual should resolve the mountain range owner key");
    ok &= require(containsText(mountainRequest.semanticContext, "same_family_mask=10"), "Runtime mountain visual should record its connected range mask");
    ok &= require(containsText(mountainRequest.semanticContext, "component_role=spine"), "Runtime mountain visual should infer a straight range role from opposing neighbors");
    ok &= require(mountainResolution.hasCandidateAsset, "Runtime mountain visual should find candidate component-role art");
    ok &= require(containsText(mountainResolution.candidatePreviewAssetId, ".spine."), "Runtime mountain visual should filter candidate previews by inferred component role");
    ok &= require(mountainResolution.fallbackReason == "candidate_not_runtime_accepted", "Runtime mountain visual should explain candidate component-role art is not accepted yet");

    ok &= require(towerRequest.assetClass == MapStyleContextualAssetClass::StructureContextMatrix, "Runtime structure visual should request structure context coverage");
    ok &= require(towerRequest.owner == "guard_tower", "Runtime structure visual should resolve the guard tower owner key");
    ok &= require(containsText(towerRequest.semanticContext, "ground=cave_floor"), "Runtime structure visual should record cave ground context");
    ok &= require(containsText(towerRequest.semanticContext, "entrance=cave_tunnel"), "Runtime structure visual should record cave-tunnel entrance context");
    ok &= require(containsText(towerRequest.semanticContext, "orientation=n"), "Runtime structure visual should record cave-tunnel entrance orientation");
    ok &= require(containsText(dungeonTowerRequest.semanticContext, "entrance=dungeon_corridor"), "Runtime structure visual should use taxonomy dungeon-corridor entrance tokens");
    ok &= require(containsText(dungeonTowerRequest.semanticContext, "orientation=e"), "Runtime structure visual should record dungeon-corridor entrance orientation");
    ok &= require(containsText(bridgeheadTowerRequest.semanticContext, "entrance=way_wood_bridge"), "Runtime structure visual should use taxonomy bridgehead entrance tokens");
    ok &= require(containsText(bridgeheadTowerRequest.semanticContext, "orientation=s"), "Runtime structure visual should record bridgehead entrance orientation");
    ok &= require(containsText(stoneRoadTowerRequest.semanticContext, "ground=stone_paved"), "Runtime structure visual should use stone-paved ground for stone-road entrances");
    ok &= require(containsText(stoneRoadTowerRequest.semanticContext, "entrance=way_stone_road"), "Runtime structure visual should preserve stone-road entrance context");
    ok &= require(containsText(mixedRoadTowerRequest.semanticContext, "ground=dirt"), "Runtime structure visual should bind ground to the selected entrance road when mixed road contexts touch a structure");
    ok &= require(containsText(mixedRoadTowerRequest.semanticContext, "entrance=way_dirt_road"), "Runtime structure visual should preserve the selected mixed-road entrance context");
    ok &= require(containsText(mixedRoadTowerRequest.semanticContext, "orientation=n"), "Runtime structure visual should keep the selected mixed-road entrance orientation");
    ok &= require(containsText(riverTowerRequest.semanticContext, "ground=water_shallow"), "Runtime waterfront structure visual should still expose unsupported water ground context");
    ok &= require(containsText(riverTowerRequest.semanticContext, "entrance=none"), "Runtime waterfront structure visual should not treat rivers as tower entrances");
    ok &= require(containsText(riverTowerRequest.semanticContext, "orientation=none"), "Runtime waterfront structure visual should leave orientation empty when no entrance exists");
    ok &= require(towerResolution.hasCandidateAsset, "Runtime structure visual should find candidate guard-tower context art");
    ok &= require(containsText(towerResolution.candidatePreviewAssetId, ".cave_threshold.v402"), "Runtime structure visual should filter guard-tower previews by entrance orientation");
    ok &= require(towerResolution.fallbackReason == "candidate_not_runtime_accepted", "Runtime structure visual should explain candidate structure art is not accepted yet");
    ok &= require(towerResolution.selectedAssetId == "runtime_v1:structure_symbol", "Runtime structure visual should name the structure fallback asset");
    ok &= require(cellVisualResolutions.size() == 1, "Runtime cell visual asset list should include one road layer for a road-on-grass cell");
    ok &= require(!cellVisualResolutions.empty() && cellVisualResolutions.front().fallbackReason.empty(), "Runtime cell visual asset list should not report road-art fallback metadata when accepted art exists");
    ok &= require(!cellVisualResolutions.empty() && containsText(cellVisualResolutions.front().selectedAssetId, ".mask005."), "Runtime cell visual asset list should keep the mask-matched accepted runtime ID");
    ok &= require(missingStyleResolution.selectedAssetId == "style_registry:missing_style", "Runtime visual lookup should name a missing style registry fallback");
    ok &= require(missingStyleResolution.fallbackReason == "missing_style_pack", "Runtime visual lookup should explain missing style pack coverage");
    return ok;
}

bool testBattlefieldBrushDirtyCells()
{
    EncounterModel battlefield;
    battlefield.battlefieldWidth = 4;
    battlefield.battlefieldHeight = 4;
    normalizeEncounter(battlefield);

    const std::vector<MapCellCoordinate> footprint = battlefieldBrushFootprint(battlefield, 2, 2, 1);
    const std::vector<MapCellCoordinate> dirtyCells = battlefieldTerrainDirtyCellsForFootprint(battlefield, footprint);

    bool ok = true;
    ok &= require(footprint.size() == 1, "Battlefield brush size 1 should produce one footprint cell");
    ok &= require(mapCellCoordinateInList(dirtyCells, 2, 2), "Battlefield dirty cells should include painted cell");
    ok &= require(mapCellCoordinateInList(dirtyCells, 2, 1), "Battlefield dirty cells should include north neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 3, 2), "Battlefield dirty cells should include east neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 2, 3), "Battlefield dirty cells should include south neighbor");
    ok &= require(mapCellCoordinateInList(dirtyCells, 1, 2), "Battlefield dirty cells should include west neighbor");

    const EncounterModel preview = previewBattlefieldTerrainBrush(battlefield, footprint, MapTileKind::Lava);
    const MapCellModel *originalCell = encounterCellAt(battlefield, 2, 2);
    const MapCellModel *previewCell = encounterCellAt(preview, 2, 2);
    ok &= require(originalCell && originalCell->kind == MapTileKind::Empty, "Battlefield preview should not mutate the original encounter");
    ok &= require(previewCell && previewCell->kind == MapTileKind::Lava, "Battlefield preview should paint the scratch encounter");
    return ok;
}

bool testTacticalMapAuthoring()
{
    MapModel map;
    map.width = 4;
    map.height = 4;
    resizeMapCells(map);

    const std::vector<MapCellCoordinate> elevationFootprint = mapBrushFootprint(map, 1, 1, 2);
    applyMapElevationBrush(map, elevationFootprint, 3);

    bool ok = true;
    ok &= require(mapCellAt(map, 0, 0)->elevation == 3 && mapCellAt(map, 1, 1)->elevation == 3,
                  "Elevation brush should set an exact elevation across its footprint");

    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::Wall);
    ok &= require((mapCellAt(map, 1, 1)->movementBlockedMask & 2) != 0 &&
                      (mapCellAt(map, 2, 1)->movementBlockedMask & 8) != 0,
                  "Wall preset should paint reciprocal movement blockers");
    ok &= require((mapCellAt(map, 1, 1)->lineOfSightBlockedMask & 2) != 0 &&
                      (mapCellAt(map, 2, 1)->lineOfSightBlockedMask & 8) != 0,
                  "Wall preset should paint reciprocal line-of-sight blockers");

    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::Window);
    ok &= require((mapCellAt(map, 1, 1)->movementBlockedMask & 2) != 0 &&
                      (mapCellAt(map, 1, 1)->lineOfSightBlockedMask & 2) == 0,
                  "Window preset should block movement while leaving sight open");

    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::TreeTrunk);
    ok &= require((mapCellAt(map, 1, 1)->movementBlockedMask & 2) != 0 &&
                      (mapCellAt(map, 1, 1)->lineOfSightBlockedMask & 2) != 0,
                  "Tree trunk preset should block movement and line of sight");

    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::CustomBlocker, true, false, true);
    ok &= require((mapCellAt(map, 1, 1)->movementOneWayBlockedMask & 2) != 0 &&
                      (mapCellAt(map, 2, 1)->movementBlockedMask & 8) == 0,
                  "One-way blocker should be stored only on the outgoing owner edge");

    EncounterModel encounter;
    encounter.battlefieldWidth = map.width;
    encounter.battlefieldHeight = map.height;
    encounter.battlefieldCells = map.cells;
    ok &= require(encounterMovementStepBlocked(encounter, 1, 1, 2, 1),
                  "One-way blocker should prevent movement in its arrow direction");
    ok &= require(!encounterMovementStepBlocked(encounter, 2, 1, 1, 1),
                  "One-way blocker should permit movement in the reverse direction");

    const int startingElevation = mapCellAt(map, 1, 1)->elevation;
    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::StairsUp);
    ok &= require(mapCellAt(map, 2, 1)->elevation == startingElevation + 1,
                  "Stairs preset should raise the far cell by one elevation step");
    ok &= require((mapCellAt(map, 1, 1)->movementBlockedMask & 2) == 0 &&
                      (mapCellAt(map, 1, 1)->movementOneWayBlockedMask & 2) == 0,
                  "Stairs preset should clear blockers from its traversable edge");

    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::Ledge);
    ok &= require((mapCellAt(map, 1, 1)->movementOneWayBlockedMask & 2) != 0,
                  "Ledge preset should create an uphill one-way movement blocker");
    applyMapTacticalPreset(map, 1, 1, 1, MapTacticalPreset::ClearEdge);
    ok &= require((mapCellAt(map, 1, 1)->movementBlockedMask & 2) == 0 &&
                      (mapCellAt(map, 1, 1)->movementOneWayBlockedMask & 2) == 0 &&
                      (mapCellAt(map, 2, 1)->movementBlockedMask & 8) == 0,
                  "Clear Edge should remove symmetric and one-way blocker metadata");
    return ok;
}
}

int main()
{
    bool ok = true;
    ok &= testCardinalMasks();
    ok &= testContextualMountainRangeSemantics();
    ok &= testIndividualMountainVariantMode();
    ok &= testWayMasksAndDirtyCells();
    ok &= testGrassMountainReciprocalRelationships();
    ok &= testConfiguredTransitionOverlays();
    ok &= testRoadBridgeConnectivity();
    ok &= testAutomaticTileRotation();
    ok &= testBrushDirtyCells();
    ok &= testBuiltInStyleRegistry();
    ok &= testRuntimeVisualStyleAssetResolution();
    ok &= testBattlefieldBrushDirtyCells();
    ok &= testTacticalMapAuthoring();
    return ok ? 0 : 1;
}
