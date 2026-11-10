#include "Models/Map/MapStrokeAuthoring.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapStrokePresentation.hpp"
#include "Models/Map/MapStrokeWidths.hpp"
#include "Models/Map/MapEditConsistency.hpp"
#include "map_crossing_edit_fixture.hpp"
#include <iostream>

int main()
{
    int failures = 0;
    const auto check = [&](bool ok, const char *message) {
        if (!ok) { ++failures; std::cerr << message << '\n'; }
    };
    {
        MapModel map;map.width=24;map.height=24;resizeMapCells(map);
        map.atlasLevel=MapLevel::Region;map.generation.continuousRendering=true;
        map.generation.recipe.generatorVersion=12;
        for(auto &c:map.cells)c.kind=MapTileKind::Dirt;
        MapGeneratedFeature river;river.id="diagonal";river.kind=MapGeneratedFeatureKind::River;
        for(int i=3;i<15;++i) {
            river.points.push_back({i+.5f,i+.5f});
            auto &c=*mapCellAt(map,i,i);c.kind=MapTileKind::Water;c.wayKind=MapWayKind::River;
            if(i>3){mapCellAt(map,i-1,i)->kind=MapTileKind::Water;mapCellAt(map,i,i-1)->kind=MapTileKind::Water;}
        }
        mapCellAt(map,2,18)->kind=MapTileKind::Water; // real, isolated pond
        for(int x=0;x<24;++x)mapCellAt(map,x,22)->kind=MapTileKind::Water;
        MapGeneratedFeature road;road.id="diagonal-crossing-road";road.kind=MapGeneratedFeatureKind::Road;
        road.points={{1,9.5f},{22,9.5f}};road.width=.8f;
        map.generation.graph.features={river,road};projectGeneratedWayFeatureToCells(map,road,false);
        for(auto &p:map.generation.graph.features[0].points)p.x+=6;
        reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map,river);
        for(int i=4;i<15;++i)check(mapCellAt(map,i-1,i)->kind!=MapTileKind::Water &&
            mapCellAt(map,i,i-1)->kind!=MapTileKind::Water,"Moving diagonal river left circular shoulder lakes");
        check(mapCellAt(map,9,9)->kind!=MapTileKind::Bridge,"Moved river left phantom bridge terrain at old road crossing");
        check(mapCellAt(map,2,18)->kind==MapTileKind::Water && mapCellAt(map,8,22)->kind==MapTileKind::Water,
              "River move deleted genuine standing water");
    }
    const auto fixture = [] {
        MapModel map;
        map.width = 20; map.height = 12; resizeMapCells(map);
        for (auto &cell : map.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
        map.generation.continuousRendering = true;
        return map;
    };
    auto map = fixture();
    auto atlas = fixture();
    atlas.atlasLevel = MapLevel::World;
    atlas.generation.recipe.generatorVersion = 12;
    atlas.generation.recipe.styleId = "illustrated_atlas";
    atlas.generation.recipe.styleVersion = 5;
    atlas.generation.recipe.styleSnapshotId = "illustrated_atlas_v5";
    const auto worldStroke = gm::map_render::manualWayStrokeDisplayWidth(atlas, MapWayKind::DirtRoad, 3, 16.0f);
    {
        MapStrokeWidths widths;
        widths.setting(MapWayKind::River)=8;
        check(widths.value(MapWayKind::DirtRoad)==1 && widths.value(MapWayKind::Trail)==1,
              "Widening a waterway must not widen the next road or trail");
        widths.setting(MapWayKind::DirtRoad)=3;
        check(widths.value(MapWayKind::River)==8,"Road and water widths must remain independent");
        widths.road=24;
        check(widths.value(MapWayKind::DirtRoad)==3,"Ordinary road drawing must reject an area-brush-sized width");
        widths.road=1;
        auto switched=atlas;
        // Reproduce a large terrain stamp followed by a road beside water.
        applyMapTerrainBrush(switched,mapBrushFootprint(switched,10,6,24,true),MapTileKind::Dirt,0,false);
        for(int x=0;x<switched.width;++x) mapCellAt(switched,x,1)->kind=MapTileKind::Water;
        const auto before=switched;
        for(int x=4;x<=14;++x)
            applyMapWayBrush(switched,mapBrushFootprint(switched,x,6,widths.value(MapWayKind::DirtRoad)),MapWayKind::DirtRoad,false);
        appendContinuousMapWayStroke(switched,{{4.5f,6.5f},{14.5f,6.5f}},MapWayKind::DirtRoad,widths.value(MapWayKind::DirtRoad));
        const auto &drawn=switched.generation.graph.features.back();
        check(std::abs(drawn.width-.82f)<.001f,"A large terrain stamp leaked into the committed road width");
        projectGeneratedWayFeatureToCells(switched,drawn,false);
        rebuildContinuousMapWayFeaturesFromCells(switched);
        for(int x=0;x<switched.width;++x)
            check(mapCellAt(switched,x,1)->kind==mapCellAt(before,x,1)->kind &&
                  mapCellAt(switched,x,1)->wayKind==mapCellAt(before,x,1)->wayKind &&
                  mapCellAt(switched,x,1)->fogged==mapCellAt(before,x,1)->fogged,
                  "Creating a road beside water painted stray bridges into the unrelated water");
        const auto preview=gm::map_render::manualWayStrokeDisplayWidth(switched,MapWayKind::DirtRoad,widths.value(MapWayKind::DirtRoad),16);
        const auto finalWidth=gm::map_render::continuousMapWayDisplayWidth(switched,switched.generation.graph.features.front(),16);
        check(preview.outerPixels<5 && std::abs(preview.outerPixels-finalWidth.outerPixels)<.001f,
              "Default road preview/committed widths must remain narrow and agree");
    }
    check(worldStroke.outerPixels > 7.8f && worldStroke.outerPixels < 8.1f,
          "World road cursor predicts the 48-pixel gameplay brush instead of its 8-pixel atlas stroke");
    const auto zoomedStroke = gm::map_render::manualWayStrokeDisplayWidth(atlas, MapWayKind::DirtRoad, 3, 32.0f);
    check(std::abs(zoomedStroke.outerPixels - worldStroke.outerPixels * 2.0f) < .001f,
          "World stroke cursor does not track canvas zoom");
    const auto waterStroke = gm::map_render::manualWayStrokeDisplayWidth(atlas, MapWayKind::River, 3, 16.0f);
    check(waterStroke.tapers && waterStroke.outerPixels > 9.6f && waterStroke.outerPixels < 9.8f,
          "Atlas river preview ignores mesh fringe or river scale");
    for (const auto level : {MapLevel::World, MapLevel::Region, MapLevel::Site, MapLevel::Tactical})
        for (const auto way : {MapWayKind::DirtRoad, MapWayKind::StoneRoad, MapWayKind::Trail, MapWayKind::River, MapWayKind::Canal})
        {
            auto family = atlas; family.atlasLevel = level;
            if (level == MapLevel::Site || level == MapLevel::Tactical)
            {
                family.scope = MapScope::Local;
                family.generation.recipe.styleVersion = 6;
                family.generation.recipe.styleSnapshotId = "illustrated_atlas_v6";
            }
            appendContinuousMapWayStroke(family, {{2.5f, 3.5f}, {12.5f, 3.5f}}, way, 3);
            const auto actual = gm::map_render::continuousMapWayDisplayWidth(family, family.generation.graph.features.back(), 16.0f);
            const auto preview = gm::map_render::manualWayStrokeDisplayWidth(family, way, 3, 16.0f);
            check(std::isfinite(preview.outerPixels) && preview.outerPixels >= preview.surfacePixels &&
                  std::abs(actual.outerPixels - preview.outerPixels) < .001f,
                  "Manual cursor width and committed feature disagree for a map family/way kind");
        }
    check(appendContinuousMapWayStroke(map, {{1.25f, 4.25f}, {3.75f, 4.5f}, {12.5f, 4.5f}},
                                      MapWayKind::DirtRoad, 3), "Road gesture was rejected");
    check(map.generation.graph.features.size() == 1, "One road gesture produced a cell-edge lattice");
    auto &road = map.generation.graph.features.front();
    check(road.points.size() == 3 && road.points.front().x == 1.25f &&
          std::abs(road.width - 2.46f) < 0.001f, "Stroke lost raw coordinates or brush width");
    road.name = "Old trade road"; road.gmOnly = true; road.discovered = false;
    road.lineage.sourceFeatureId = "parent_road"; road.lineage.sourceRevision = 17;
    const auto originalId = road.id;
    projectGeneratedWayFeatureToCells(map, road, false);
    const std::vector<MapCellCoordinate> cut{{6, 4}, {7, 4}};
    for (const auto cell : cut) mapCellAt(map, cell.x, cell.y)->wayKind = MapWayKind::None;
    check(eraseContinuousMapWayStrokes(map, cut), "Sparse centreline was not clipped by eraser");
    check(map.generation.graph.features.size() == 2, "Middle erasure did not split the drawn road");
    check(map.generation.graph.features.front().id == originalId, "First road fragment lost its identity");
    for (const auto &fragment : map.generation.graph.features)
        check(fragment.name == "Old trade road" && fragment.gmOnly && !fragment.discovered &&
              fragment.lineage.sourceFeatureId == "parent_road" && fragment.lineage.sourceRevision == 17,
              "Erasing a stroke discarded author metadata");
    check(mapCellAt(map, 6, 4)->wayKind == MapWayKind::None &&
          mapCellAt(map, 7, 4)->wayKind == MapWayKind::None, "Fragment endpoints repainted erased cells");
    check(!eraseContinuousMapWayStrokes(map, cut), "Repeating an erasure cut surviving fragments again");

    auto clicked = fixture();
    check(appendContinuousMapWayStroke(clicked, {{4.25f, 4.5f}, {4.25f, 4.5f}}, MapWayKind::Trail, 2),
          "Single click was rejected");
    check(clicked.generation.graph.features.front().points.size() == 2 &&
          clicked.generation.graph.features.front().points.front().x <
          clicked.generation.graph.features.front().points.back().x,
          "Single click has no visible segment");
    check(std::abs(clicked.generation.graph.features.front().width - 1.1f) < 0.001f,
          "Trail does not use the requested width");
    check(!appendContinuousMapWayStroke(clicked, {}, MapWayKind::River, 1), "Empty gesture created a feature");

    auto broad = fixture();
    for (int x = 2; x <= 14; ++x)
        applyMapWayBrush(broad, mapBrushFootprint(broad, x, 5, 3), MapWayKind::DirtRoad, false);
    appendContinuousMapWayStroke(broad, {{2.5f, 5.5f}, {14.5f, 5.5f}}, MapWayKind::DirtRoad, 3);
    rebuildContinuousMapWayFeaturesFromCells(broad);
    check(broad.generation.graph.features.size() == 1 &&
          mapGeneratedFeatureIsManualStroke(broad.generation.graph.features.front()),
          "Reconciliation turned a broad continuous stroke into a grid lattice");
    std::vector<MapCellCoordinate> narrowErase;
    for (int x = 6; x <= 10; ++x) narrowErase.push_back({x, 5});
    applyMapWayBrush(broad, narrowErase, MapWayKind::DirtRoad, true);
    eraseContinuousMapWayStrokes(broad, narrowErase);
    rebuildContinuousMapWayFeaturesFromCells(broad);
    check(std::all_of(broad.generation.graph.features.begin(), broad.generation.graph.features.end(),
                      [](const auto &feature) { return mapGeneratedFeatureIsManualStroke(feature); }),
          "Narrow eraser left a lattice around a removed broad stroke section");
    auto legacy = fixture();
    auto offCenter = fixture();
    offCenter.width = 72; offCenter.height = 48; resizeMapCells(offCenter);
    // Exact native repro cursor coordinates. These yield snapped paint centres
    // (39,12),(40,13),... while the raw line reaches x40 only near y14.
    for (const auto centre : std::vector<MapCellCoordinate>{{39,12},{40,13},{40,14},{41,15},{41,16},{42,17}})
        applyMapWayBrush(offCenter, mapBrushFootprint(offCenter, centre.x, centre.y, 3), MapWayKind::River, false);
    appendContinuousMapWayStroke(offCenter, {{39.2123566f,12.5714283f},{42.1776047f,17.4517384f}}, MapWayKind::River, 3);
    rebuildContinuousMapWayFeaturesFromCells(offCenter);
    check(offCenter.generation.graph.features.size() == 1 &&
          mapGeneratedFeatureIsManualStroke(offCenter.generation.graph.features.front()),
          "Off-centre river stroke created blue grid-derived shoulder spokes");
    for (int x = 2; x <= 14; ++x) mapCellAt(legacy, x, 5)->wayKind = MapWayKind::DirtRoad;
    rebuildContinuousMapWayFeaturesFromCells(legacy);
    applyMapWayBrush(legacy, {{7, 5}}, MapWayKind::DirtRoad, true);
    eraseContinuousMapWayStrokes(legacy, {{7, 5}});
    rebuildContinuousMapWayFeaturesFromCells(legacy);
    check(mapCellAt(legacy, 7, 5)->wayKind == MapWayKind::None,
          "Continuous eraser resurrected an old grid-derived road before reconciliation");

    auto crossing = fixture();
    MapGeneratedFeature river;
    river.id = "generated_river"; river.kind = MapGeneratedFeatureKind::River;
    river.materialId = "fantasy_neutral.water.river"; river.name = "Preserved river";
    river.points = {{8.5f, 1.5f}, {8.5f, 10.5f}};
    crossing.generation.graph.features.push_back(river);
    projectGeneratedWayFeatureToCells(crossing, river, false);
    appendContinuousMapWayStroke(crossing, {{1.5f, 5.5f}, {17.5f, 5.5f}}, MapWayKind::DirtRoad, 1);
    projectGeneratedWayFeatureToCells(crossing, crossing.generation.graph.features.back(), false);
    check(mapCellAt(crossing, 8, 5)->kind == MapTileKind::Bridge, "Road projection lost river crossing");
    mapCellAt(crossing, 8, 5)->wayKind = MapWayKind::None;
    eraseContinuousMapWayStrokes(crossing, {{8, 5}});
    check(mapCellAt(crossing, 8, 5)->wayKind == MapWayKind::River &&
          mapCellAt(crossing, 8, 5)->kind == MapTileKind::Water,
          "Erasing a road crossing did not restore surviving river semantics");
    check(crossing.generation.graph.features.front().id == "generated_river" &&
          crossing.generation.graph.features.front().name == "Preserved river" &&
          crossing.generation.graph.features.front().points.size() == 2,
          "Road erasure changed generated river identity or geometry");

    auto locked = fixture();
    appendContinuousMapWayStroke(locked, {{2.5f, 3.5f}, {14.5f, 3.5f}}, MapWayKind::StoneRoad, 1);
    locked.generation.recipe.lockedFeatureIds.push_back(locked.generation.graph.features.front().id);
    check(!eraseContinuousMapWayStrokes(locked, {{7, 3}}) && locked.generation.graph.features.size() == 1 &&
          locked.generation.graph.features.front().points.size() == 2,
          "Eraser cut a recipe-locked manual road");

    auto removed = fixture();
    appendContinuousMapWayStroke(removed, {{4.3f, 4.5f}, {4.7f, 4.5f}}, MapWayKind::DirtRoad, 1);
    check(eraseContinuousMapWayStrokes(removed, {{4, 4}}) && removed.generation.graph.features.empty(),
          "Erasing an entire short stroke left an invisible feature");
    auto water = fixture();
    appendContinuousMapWayStroke(water, {{2.5f, 4.5f}, {14.5f, 4.5f}}, MapWayKind::River, 1);
    projectGeneratedWayFeatureToCells(water, water.generation.graph.features.front(), false);
    auto *erasedWater = mapCellAt(water, 7, 4);
    erasedWater->movementBlockedMask = 3;
    applyMapWayBrush(water,{{7,4}},MapWayKind::River,true);
    eraseContinuousMapWayStrokes(water, {{7, 4}});
    check(erasedWater->kind == MapTileKind::Dirt && erasedWater->wayKind == MapWayKind::None &&
          erasedWater->movementBlockedMask == 3 && erasedWater->movementCost == mapTileKindDefaultMovementCost(MapTileKind::Dirt),
          "Erased river kept water terrain/costs or discarded authored edge masks");
    for(const auto level:{MapLevel::World,MapLevel::Region})for(const auto way:{MapWayKind::River,MapWayKind::Canal})for(int diagonal:{-1,0,1})
    {
        auto shore=fixture();shore.atlasLevel=level;
        for(int y=0;y<shore.height;++y)for(int x=0;x<shore.width;++x)
            if(x>=10+diagonal*(y-5))shore.cells[y*shore.width+x].kind=MapTileKind::Water;
        const auto before=shore;
        for(int x=5;x<=15;++x)applyMapWayBrush(shore,mapBrushFootprint(shore,x,5,3),way,false);
        appendContinuousMapWayStroke(shore,{{5.5f,5.5f},{15.5f,5.5f}},way,3);
        projectGeneratedWayFeatureToCells(shore,shore.generation.graph.features.back(),false);
        rebuildContinuousMapWayFeaturesFromCells(shore);
        const auto oceanUnchanged=[&] {
            const auto ground=gm::map_render::buildOverviewGroundProjection(shore);
            for(int y=0;y<shore.height;++y)for(int x=0;x<shore.width;++x)
                if(before.cells[y*shore.width+x].kind==MapTileKind::Water && (shore.cells[y*shore.width+x].kind!=MapTileKind::Water ||
                   shore.cells[y*shore.width+x].wayKind!=MapWayKind::None ||
                   ground[y*shore.width+x]!=MapTileKind::Water)) return false;
            return true;
        };
        check(oceanUnchanged(),"Drawing an atlas river into the sea must not turn standing-water cells or their rendered underlay into land");
        check(shore.generation.graph.features.size()==1 && shore.generation.graph.features.front().points.back().x==15.5f &&
              mapCellAt(shore,6,5)->wayKind==way,"Preserving the ocean must retain the requested stroke and its inland gameplay channel");
        auto oldFeature=shore.generation.graph.features.front();
        shore.generation.graph.features.front().points.back().y=7.5f;
        reprojectGeneratedMapWayFeaturesAfterGeometryEdit(shore,oldFeature);
        check(oceanUnchanged(),"Moving a drawn river endpoint offshore must preserve standing water");
        std::vector<MapCellCoordinate> erased;
        for(int y=3;y<=9;++y)for(int x=10;x<=17;++x)erased.push_back({x,y});
        applyMapWayBrush(shore,erased,way,true);eraseContinuousMapWayStrokes(shore,erased);
        check(oceanUnchanged(),"Erasing the offshore portion of a drawn river must not fill the ocean with land");
        auto lakePaint=before;
        applyMapTerrainBrush(lakePaint,{{5,5}},MapTileKind::Water,0,false);
        check(mapCellAt(lakePaint,5,5)->kind==MapTileKind::Water,"Water Area terrain painting must remain independent of river underlay preservation");
    }
    {
        auto cached=fixture();
        mapCellAt(cached,10,5)->kind=MapTileKind::Water;
        for(const auto p:std::vector<MapCellCoordinate>{{10,4},{9,5}}) {
            mapCellAt(cached,p.x,p.y)->kind=MapTileKind::Water;
            mapCellAt(cached,p.x,p.y)->wayKind=MapWayKind::River;
        }
        appendContinuousMapWayStroke(cached,{{8.5f,5.5f},{12.5f,5.5f}},MapWayKind::River,1);
        const auto originalHash=gm::map_render::overviewTerrainContentHash(cached);
        auto moved=cached;
        for(auto &p:moved.generation.graph.features.front().points)p.y=9.5f;
        check(gm::map_render::overviewGroundKindForRender(cached,10,5)==MapTileKind::Water &&
              gm::map_render::overviewGroundKindForRender(moved,10,5)!=MapTileKind::Water &&
              originalHash!=gm::map_render::overviewTerrainContentHash(moved),
              "Moving manual river geometry without changing cells must invalidate its shore terrain cache");
        auto wider=moved;wider.generation.graph.features.front().width=8.f;
        check(gm::map_render::overviewGroundKindForRender(wider,10,5)==MapTileKind::Water &&
              gm::map_render::overviewTerrainContentHash(moved)!=gm::map_render::overviewTerrainContentHash(wider),
              "Manual river width must invalidate terrain when its standing-water corridor changes");
        auto renamed=cached;renamed.generation.graph.features.front().name="A harmless label edit";
        check(originalHash==gm::map_render::overviewTerrainContentHash(renamed),
              "Unrelated river labels must not invalidate terrain geometry");
    }
    for(const auto level:{MapLevel::World,MapLevel::Region})
    for(const auto way:{MapWayKind::River,MapWayKind::Canal})
    for(bool reverse:{false,true})
    {
        auto coast=atlas;coast.atlasLevel=level;
        for(int y=0;y<coast.height;++y)for(int x=10;x<coast.width;++x)
            mapCellAt(coast,x,y)->kind=MapTileKind::Water;
        std::vector<MapFeaturePoint> path{{5.5f,3.5f},{7.f,5.5f},{15.5f,5.5f}};
        if(reverse) std::reverse(path.begin(),path.end());
        appendContinuousMapWayStroke(coast,path,way,3);
        const auto original=coast.generation.graph.features.back();
        projectGeneratedWayFeatureToCells(coast,original,false);
        auto resolved=gm::map_render::illustratedAtlasUntrimmedRiverPoints(coast,original);
        const auto mouth=gm::map_render::trimIllustratedAtlasRiverMouth(coast,resolved,true);
        check(mouth.joinsWater && mouth.shore.x>9.f && mouth.shore.x<11.f && resolved.front().x<9.f,
              "A river drawn from water toward land must resolve the same land-to-water mouth");
        const auto ink=gm::map_render::buildIllustratedAtlasRiverInk(coast,original);
        check(!ink.empty(),"Bidirectional manual river lost its shared artwork");
        for(const auto &primitive:ink)for(const auto &point:primitive.points)
            check(point.x<=mouth.shore.x+1.81f,"A manually drawn waterway extends beyond its bounded shore transition");
        check(coast.generation.graph.features.back().points.front().x==path.front().x &&
              coast.generation.graph.features.back().points.back().x==path.back().x,
              "Resolving a river mouth must not reorder the saved authoring controls");
    }
    for(const auto level:{MapLevel::World,MapLevel::Region})
    for(const int row:{4,5})
    {
        auto painted=atlas;painted.atlasLevel=level;
        const auto countMarks=[](const auto &ridges) {
            std::size_t count=0;for(const auto &ridge:ridges)count+=ridge.marks.size();return count;
        };
        check(gm::map_render::buildOverviewRidgePaths(painted).empty(),"Plain terrain acquired phantom mountains");
        const auto footprint=mapBrushFootprint(painted,5,row,1);
        applyMapTerrainBrush(painted,footprint,MapTileKind::Mountain,0,false);
        check(countMarks(gm::map_render::buildOverviewRidgePaths(painted))>0,
              "A single mountain brush click must produce visible relief on either grid row parity");
        check(painted.generation.graph.features.empty(),"Rendering painted mountains must not create saved generated objects");
        applyMapTerrainBrush(painted,footprint,MapTileKind::Dirt,0,false);
        check(gm::map_render::buildOverviewRidgePaths(painted).empty(),"Repainting mountain terrain left orphaned relief");
        MapGeneratedFeature existing;existing.id="existing-range";existing.kind=MapGeneratedFeatureKind::MountainRange;
        existing.points={{2.f,3.f},{16.f,3.f}};existing.width=2;
        painted.generation.graph.features.push_back(existing);
        const auto originalMarks=countMarks(gm::map_render::buildOverviewRidgePaths(painted));
        applyMapTerrainBrush(painted,mapBrushFootprint(painted,7,3,1),MapTileKind::Mountain,0,false);
        check(countMarks(gm::map_render::buildOverviewRidgePaths(painted))==originalMarks,
              "Mountain cells represented by a generated range must not duplicate its peaks");
        const auto patch=mapBrushFootprint(painted,10,9,3,true);
        applyMapTerrainBrush(painted,patch,MapTileKind::Mountain,0,false);
        check(countMarks(gm::map_render::buildOverviewRidgePaths(painted))>originalMarks,
              "An existing generated range must not hide a new painted mountain area elsewhere");
        applyMapTerrainBrush(painted,patch,MapTileKind::Water,0,false);
        check(countMarks(gm::map_render::buildOverviewRidgePaths(painted))==originalMarks,
              "Painting water over authored mountains must remove their derived peaks");
    }
    for(const auto level:{MapLevel::World,MapLevel::Region})
    for(bool reversedOrder:{false,true})
    {
        auto crossing=crossingEditFixture();crossing.atlasLevel=level;
        if(reversedOrder)std::reverse(crossing.generation.graph.features.begin(),crossing.generation.graph.features.end());
        auto &road=crossing.generation.graph.features[reversedOrder?1:0];
        const auto before=road;
        check(gm::map_render::illustratedAtlasCrossingBridges(crossing).empty(),"Separate ways gained a phantom bridge");
        for(auto &p:road.points)p.x+=7.f;
        check(finishGeneratedMapFeatureGeometryEdit(crossing,before),"Road geometry commit failed");
        const auto bridges=gm::map_render::illustratedAtlasCrossingBridges(crossing);
        check(bridges.size()==2,"Moving a road over a winding river must resolve both crossings");
        check(crossing.generation.graph.features.size()==2,"Derived bridges must not add saved objects");
        for(const auto &cell:crossing.cells)if(cell.kind==MapTileKind::Bridge)
            check(cell.wayKind==MapWayKind::DirtRoad,"River insertion order overwrote road crossing semantics");
        for(int y=0;y<crossing.height;++y)for(int x=0;x<crossing.width;++x)
            check(gm::map_render::overviewGroundKindForRender(crossing,x,y)!=MapTileKind::Water,
                  "Moving the road exposed a river projection cell as a round pond");
        const auto moved=road;road=before;
        check(finishGeneratedMapFeatureGeometryEdit(crossing,moved),"Moving the road away failed");
        check(gm::map_render::illustratedAtlasCrossingBridges(crossing).empty(),"Moving the road away left automatic bridges behind");
        mapCellAt(crossing,21,3)->kind=MapTileKind::Water;
        check(gm::map_render::overviewGroundKindForRender(crossing,21,3)==MapTileKind::Water,
              "Suppressing river projections removed a genuine isolated pond");
    }
    {
        auto joined=atlas;
        MapGeneratedFeature main;main.id="main-road";main.kind=MapGeneratedFeatureKind::Road;
        main.points={{12,1},{12,11}};joined.generation.graph.features={main};
        check(appendContinuousMapWayStroke(joined,{{11.4f,3},{6,3},{4,5},{6,8},{11.3f,9}},MapWayKind::DirtRoad,1),
              "Appending a connected hand-drawn road failed");
        const auto &branch=joined.generation.graph.features.back();
        check(branch.points.front().x==12 && branch.points.back().x==12,
              "Both near road endpoints must snap onto the receiving road");
        const auto curve=gm::map_render::overviewFinalRoadCurvePoints(joined,branch);
        check(curve.size()>branch.points.size() && curve.front().x==12 && curve.back().x==12,
              "Hand-drawn road smoothing must preserve snapped junctions");
        const auto roadInk=gm::map_render::illustratedAtlasRoadInkPoints(joined,branch);
        check(roadInk.front().x<12 && roadInk.back().x<12 && branch.points.front().x==12 && branch.points.back().x==12,
              "Round road caps must end inside the junction without changing its semantic connections");
        auto locked=branch;locked.locked=true;
        check(gm::map_render::overviewFinalRoadCurvePoints(joined,locked).size()==locked.points.size(),
              "Locked road controls must retain their exact geometry");
        auto hostMap=atlas;
        MapGeneratedFeature host=main;host.id="manual_stroke_host";host.points={{2,2},{10,2},{10,10}};
        MapGeneratedFeature incoming=main;incoming.id="incoming";incoming.points={{9.5f,2},{15,6}};
        hostMap.generation.graph.features={host,incoming};
        const auto hostCurve=gm::map_render::overviewFinalRoadCurvePoints(hostMap,host);
        check(std::any_of(hostCurve.begin(),hostCurve.end(),[](auto p){return std::abs(p.x-9.5f)<.00001f && std::abs(p.y-2.f)<.00001f;}),
              "Smoothing a receiving manual road must keep an incoming junction fixed");
        hostMap.generation.recipe.lockedFeatureIds.push_back(host.id);
        check(gm::map_render::overviewFinalRoadCurvePoints(hostMap,host).size()==host.points.size(),
              "Recipe-locked hand-drawn roads must also retain exact controls");
        auto distantMap=atlas;distantMap.generation.graph.features={main};
        appendContinuousMapWayStroke(distantMap,{{2,3},{6,4},{8,6}},MapWayKind::DirtRoad,1);
        check(distantMap.generation.graph.features.back().points.back().x==8,"Distant road ends must not jump to another road");
        MapGeneratedFeature stale;stale.id="manual_bridge_6_6";stale.kind=MapGeneratedFeatureKind::Bridge;
        stale.points={{5,6},{7,6}};joined.generation.graph.features.push_back(stale);
        check(gm::map_render::illustratedAtlasCrossingBridges(joined).empty(),
              "Old grid bridge stamps must not float on dry ground in atlas artwork");
    }
    {
        auto pass=atlas;pass.generation.graph.features.clear();
        for(auto &cell:pass.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;}
        for(int x=4;x<16;++x)for(int y=5;y<8;++y)
            applyMapCellTerrainDefaults(*mapCellAt(pass,x,y),MapTileKind::Mountain);
        const auto original=pass;
        std::vector<MapCellCoordinate> stroke;
        for(int y=1;y<11;++y)stroke.push_back({10,y});
        applyMapWayBrush(pass,stroke,MapWayKind::DirtRoad,false);
        appendContinuousMapWayStroke(pass,{{10,1},{10,11}},MapWayKind::DirtRoad,1);
        const auto retainedMountains=[&](const MapModel &value){
            for(std::size_t i=0;i<value.cells.size();++i)
                if(value.cells[i].kind!=original.cells[i].kind)return false;
            return true;
        };
        check(retainedMountains(pass),"Painting a mountain pass must preserve underlying mountain terrain");
        const auto passInk=[&](const MapModel &value){return gm::map_render::buildIllustratedAtlasMountainPassInk(
            value,gm::map_render::illustratedAtlasRoadCorridors(value));};
        check(!passInk(pass).empty(),"A road through mountain terrain needs rock-cut pass artwork");
        check(pass.generation.graph.features.size()==1,"Pass artwork must not create saved mountain/wall entities");
        const auto beforeMove=pass;
        const auto roadBefore=pass.generation.graph.features.back();
        for(auto &p:pass.generation.graph.features.back().points)p.x+=8.f;
        finishGeneratedMapFeatureGeometryEdit(pass,roadBefore);
        check(retainedMountains(pass) && passInk(pass).empty(),"Moving a road out of mountains must remove its pass without erasing terrain");
        pass=beforeMove;
        check(!passInk(pass).empty(),"Restoring an undo snapshot must restore derived mountain-pass art");
        projectGeneratedWayFeatureToCells(pass,pass.generation.graph.features.back(),true);
        pass.generation.graph.features.clear();
        check(retainedMountains(pass) && passInk(pass).empty(),"Erasing the road must restore uninterrupted mountains without stale pass artwork");
    }
    {
        auto edited=atlas;edited.generation.graph.features.clear();
        for(auto &cell:edited.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;}
        MapGeneratedFeature river;river.id="edited-river";river.kind=MapGeneratedFeatureKind::River;
        river.points={{2,6},{18,6}};river.width=1.2f;
        edited.generation.graph.features.push_back(river);projectGeneratedWayFeatureToCells(edited,river,false);
        applyMapTerrainBrush(edited,mapBrushFootprint(edited,10,6,5,true),MapTileKind::Water,0,false);
        check(mapCellAt(edited,10,6)->kind==MapTileKind::Water && mapCellAt(edited,10,6)->wayKind==MapWayKind::None,
              "Lake painting must clear stale river underlay markers");
        MapGeneratedFeature city;city.id="submerged-city";city.name="Harbor";city.kind=MapGeneratedFeatureKind::Settlement;city.points={{10.5f,6.5f}};
        MapGeneratedFeature road;road.id="cross-lake-road";road.kind=MapGeneratedFeatureKind::Road;road.points={{2.5f,6.5f},{17.5f,6.5f}};road.width=.82f;
        edited.generation.graph.features.push_back(city);edited.generation.graph.features.push_back(road);
        projectGeneratedWayFeatureToCells(edited,road,false);
        check(mapCellAt(edited,10,6)->kind==MapTileKind::Water && mapCellAt(edited,10,6)->wayKind==MapWayKind::None,
              "Road projection must not raise a bridge island across a lake");
        const auto problems=inspectMapEditConflicts(edited);
        check(problems.size()==2,"Flooded place and road must both be reviewable");
        projectGeneratedWayFeatureToCells(edited,river,false);
        check(mapCellAt(edited,10,6)->wayKind==MapWayKind::None,
              "Reprojecting a generated river must not punch a stale channel back through a painted lake");
        const auto glyphInk=gm::map_render::buildIllustratedAtlasFeatureInk(edited,false,true);
        check(!glyphInk.empty(),"A graph settlement painted over with water must retain its visible place glyph");
        const auto waterInk=gm::map_render::buildIllustratedAtlasRiverInk(edited,river);
        bool foundSubmerged=false,opaqueSubmerged=false;
        for(const auto &primitive:waterInk)
            for(std::size_t i=0;i<primitive.points.size() && i<primitive.vertexColors.size();++i)
                if(std::abs(primitive.points[i].x-10.f)<.25f && std::abs(primitive.points[i].y-6.f)<.25f)
                {foundSubmerged=true;opaqueSubmerged|=primitive.vertexColors[i].a>8;}
        check(foundSubmerged && !opaqueSubmerged,"River ink inside a lake must dissolve instead of overlaying the water as a pipe");
        check(repairMapEditConflict(edited,city.id),"Flooded city should move to nearest suitable land on request");
        check(repairMapEditConflict(edited,road.id),"A road around a lake should have a land repair");
        check(inspectMapEditConflicts(edited).empty(),"Repairs should resolve both conflicts without losing feature IDs");
        auto movedRiver=atlas;movedRiver.generation.graph.features={river};
        projectGeneratedWayFeatureToCells(movedRiver,river,false);
        for(auto &p:movedRiver.generation.graph.features.front().points)p.y+=3.f;
        finishGeneratedMapFeatureGeometryEdit(movedRiver,river);
        check(mapCellAt(movedRiver,5,6)->kind!=MapTileKind::Water && mapCellAt(movedRiver,5,9)->wayKind==MapWayKind::River,
              "Moving a river must restore its old channel ground rather than leave a water-only ghost");
        auto locked=atlas;locked.generation.graph.features={city};
        applyMapTerrainBrush(locked,mapBrushFootprint(locked,10,6,5,true),MapTileKind::Water,0,false);
        locked.generation.recipe.lockedFeatureIds.push_back(city.id);
        check(!repairMapEditConflict(locked,city.id) && locked.generation.graph.features.front().points.front().x==10.5f,
              "Conflict repair must respect locked places");
        locked.generation.recipe.lockedFeatureIds.clear();
        auto attached=road;attached.id="locked-approach";attached.points.back()=city.points.front();
        locked.generation.graph.features.push_back(attached);locked.generation.recipe.lockedFeatureIds.push_back(attached.id);
        check(!repairMapEditConflict(locked,city.id),"Moving a flooded town must not disconnect its locked approach");
        auto sea=atlas;sea.generation.graph.features={road};for(auto &cell:sea.cells){cell.kind=MapTileKind::Water;cell.wayKind=MapWayKind::None;}
        check(!repairMapEditConflict(sea,road.id) && sea.generation.graph.features.front().points.size()==2,
              "An impossible land route must leave the map unchanged");
        auto patch=atlas;patch.generation.graph.features.clear();for(auto &cell:patch.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;}
        mapCellAt(patch,7,5)->kind=MapTileKind::Water;
        const auto patches=inspectMapEditConflicts(patch);
        check(patches.size()==1 && patches.front().waterFragment,"Detached water patches should be offered for review");
        if(!patches.empty())check(repairMapEditConflict(patch,patches.front().featureId) && mapCellAt(patch,7,5)->kind==MapTileKind::Dirt,
              "Explicit water-patch removal should restore adjacent ground");
        mapCellAt(patch,7,5)->kind=MapTileKind::Ice;
        check(inspectMapEditConflicts(patch).empty(),"Ice must not be offered as a removable water fragment");
        auto junction=atlas;junction.generation.graph.features.clear();
        auto host=road;host.id="junction-host";host.points={{10,1},{10,11}};
        auto branch=road;branch.id="junction-branch";branch.points={{2,5},{8,5},{10.3f,5},{9.8f,5}};
        junction.generation.graph.features={host,branch};connectEditedMapRoad(junction,1,branch);
        check(junction.generation.graph.features[1].points.back().x==10.f &&
              junction.generation.graph.features[1].points[2].x<10.f,
              "Snapping a short backwards road hook should produce a clean host-road approach");
    }
    if (!failures) std::cout << "Continuous stroke authoring checks passed\n";
    return failures ? 1 : 0;
}
