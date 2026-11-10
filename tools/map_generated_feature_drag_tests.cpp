#include "Views/Editor/Widgets/Map/MapCreator/MapGeneratedFeatureDragInteraction.hpp"
#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapEditTransactions.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapObjectEditing.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapTerrainGroupEditing.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapAreaSelection.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapFogOverlay.hpp"
#include "Models/Map/MapBrushStrokePreview.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapRegionEditing.hpp"

#include <iostream>
#include <chrono>

namespace
{
int failures = 0;
void check(bool value, const char *message)
{ if (!value) { std::cerr << message << '\n'; ++failures; } }

// Feed real ImGui input events through the exact frame decision used by the
// editor. The small callbacks represent its undo capture and final projection;
// feature hit testing and geometry mutation use the actual authoring functions.
struct DragFixture
{
    MapModel map;
    MapModel undo;
    MapGeneratedFeature before;
    int selected = -1;
    int point = -1;
    int captures = 0;
    int finishes = 0;
    bool editing = false;
    bool updateOnRelease = false;
    static constexpr float cellSize = 20.0f;

    explicit DragFixture(MapGeneratedFeatureKind kind, bool locked = false)
    {
        map.width = 20; map.height = 16; resizeMapCells(map);
        MapGeneratedFeature feature;
        feature.id = "dragged"; feature.kind = kind; feature.locked = locked;
        feature.points = {{4.0f, 4.0f}};
        if (kind == MapGeneratedFeatureKind::Road) feature.points.push_back({12.0f, 4.0f});
        map.generation.graph.features.push_back(feature);
    }

    void frame()
    {
        ImGui::NewFrame();
        const auto mouse = ImGui::GetIO().MousePos;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const auto hit = hitTestGeneratedMapFeature(map, {mouse.x/cellSize, mouse.y/cellSize}, .55f);
            selected = hit.featureIndex;
            point = selected >= 0 && !map.generation.graph.features[selected].locked ? hit.pointIndex : -1;
        }
        if (point >= 0 && gm::mapui::mapCreatorShouldUpdateGeneratedFeatureDrag(editing))
        {
            if (!editing)
            {
                undo = map; before = map.generation.graph.features[selected]; ++captures;
                editing = true;
            }
            check(moveGeneratedMapFeaturePoint(map, selected, point, {mouse.x/cellSize, mouse.y/cellSize}),
                  "The actual authoring mutation rejected an unlocked dragged point");
            updateOnRelease |= ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        }
        if ((point >= 0 || editing) && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (editing)
            {
                finishGeneratedMapFeatureGeometryEdit(map, before);
                editing = false; ++finishes;
            }
            point = -1;
        }
        ImGui::EndFrame();
    }
};

void position(float x, float y) { ImGui::GetIO().AddMousePosEvent(x, y); }
void button(bool down) { ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, down); }
bool at(const DragFixture &fixture, float x, float y)
{
    const auto p = fixture.map.generation.graph.features.front().points.front();
    return p.x == x && p.y == y;
}
}

int main()
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = ImVec2(640,480); io.DeltaTime = 1.0f/60.0f;
    io.ConfigInputTrickleEventQueue = true;
    unsigned char *pixels = nullptr; int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    for (const auto kind : {MapGeneratedFeatureKind::Settlement, MapGeneratedFeatureKind::Road})
    {
        DragFixture fast(kind);
        position(80,80); fast.frame();
        // All three actions arrive between rendered frames. ImGui trickles
        // the press first, then the moved position plus release together.
        button(true); position(120,140); button(false);
        fast.frame();
        check(fast.point == 0 && fast.captures == 0 && at(fast,4,4), "Fast drag press was not armed at its original feature");
        fast.frame();
        check(fast.updateOnRelease && at(fast,6,7) && fast.captures == 1 && fast.finishes == 1 && !fast.editing,
              "Press/move/release between frames lost its final endpoint or split the undo transaction");
        check(fast.undo.generation.graph.features.front().points.front().x == 4 &&
                  fast.undo.generation.graph.features.front().points.front().y == 4,
              "Fast drag captured its undo snapshot after moving");
        const MapModel after = fast.map;
        fast.map = fast.undo;
        check(at(fast,4,4), "Undo did not restore the pressed endpoint");
        fast.map = after;
        check(at(fast,6,7), "Redo did not restore the released endpoint");

        DragFixture held(kind);
        position(80,80); held.frame(); button(true); held.frame();
        position(100,100); held.frame();
        check(at(held,5,5) && held.captures == 1 && held.finishes == 0, "Held drag must update before release");
        position(160,120); button(false); held.frame();
        check(at(held,8,6) && held.updateOnRelease && held.captures == 1 && held.finishes == 1,
              "Held drag discarded movement delivered with the release or captured two undo steps");
    }

    DragFixture click(MapGeneratedFeatureKind::Settlement);
    position(80,80); click.frame(); button(true); click.frame(); button(false); click.frame();
    check(at(click,4,4) && click.captures == 0 && click.finishes == 0, "A plain selection click must not create a geometry edit");
    DragFixture locked(MapGeneratedFeatureKind::Settlement,true);
    position(80,80); locked.frame(); button(true); position(120,140); button(false); locked.frame(); locked.frame();
    check(at(locked,4,4) && locked.captures == 0 && locked.finishes == 0, "A fast drag moved a locked feature");
    DragFixture returning(MapGeneratedFeatureKind::Road);
    position(80,80); returning.frame(); button(true); returning.frame(); position(140,100); returning.frame();
    position(80,80); button(false); returning.frame();
    check(at(returning,4,4) && returning.captures == 1 && returning.finishes == 1,
          "Returning to the press position on release must apply that final coordinate");
    // Region drags share the editor's release-aware input decision. Their
    // previews must not mutate gameplay regions or rebuild the map while held.
    for (const float zoom : {8.f,26.f,78.f})
    {
        using namespace gm::mapui;
        RegionBounds saved{2,3,4,3}, preview=saved, undo=saved;
        MapFeaturePoint start;
        bool armed=false, dragging=false;
        int commits=0;
        auto frame=[&]() {
            ImGui::NewFrame();
            const auto mouse=ImGui::GetIO().MousePos;
            const MapFeaturePoint p{mouse.x/zoom,mouse.y/zoom};
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { armed=true; start=p; }
            if(armed && mapCreatorShouldUpdateGeneratedFeatureDrag(dragging))
            { dragging=true; preview=regionDragBounds(saved,RegionRight|RegionBottom,start,p,20,16); }
            if(armed && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                if(!(preview==saved)) { undo=saved; saved=preview; ++commits; }
                armed=dragging=false;
            }
            ImGui::EndFrame();
        };
        position(6*zoom,6*zoom); frame();
        button(true); frame();
        position(8*zoom,8*zoom); frame();
        check(saved==RegionBounds{2,3,4,3},"Region preview mutated committed bounds before release");
        // Deliver the final move and release together, outside the old rectangle.
        position(10*zoom,11*zoom); button(false); frame(); frame();
        check(saved==RegionBounds{2,3,8,8} && commits==1,
              "Region resize lost final release coordinates or made multiple undo edits");
        const auto redo=saved; saved=undo;
        check(saved==RegionBounds{2,3,4,3},"Region undo did not restore its original rectangle");
        saved=redo;
        check(saved==RegionBounds{2,3,8,8},"Region redo did not restore the resized rectangle");
    }
    ImGui::DestroyContext();

    {
        using namespace gm::mapui;
        const RegionBounds region{2,3,6,5};
        check(regionHandleAt(region,{8,8},.3f)==(RegionRight|RegionBottom),"Region corner needs both resize axes");
        check(regionHandleAt(region,{5,3},.3f)==RegionTop,"Region top edge needs vertical resize");
        check(regionHandleAt(region,{5,5},.3f)==RegionMove,"Region interior should move its bounds");
        check(regionHandleAt(region,{15,5},.3f)==RegionNone,"Region stole a click outside its bounds");
        check(regionDragBounds(region,RegionLeft|RegionTop,{2,3},{-4,-6},20,16)==RegionBounds{0,0,8,8},
              "Region expansion should preserve opposite corner and stop at map edge");
        check(regionDragBounds(region,RegionLeft|RegionTop,{2,3},{12,14},20,16)==RegionBounds{7,7,1,1},
              "Region must not invert or shrink below one cell");
        check(regionDragBounds(region,RegionMove,{5,5},{40,40},20,16)==RegionBounds{14,11,6,5},
              "Moving regions should preserve size at map boundaries");
        check(regionDragBounds({},RegionCreate,{9.2f,10.7f},{2.1f,3.4f},20,16)==RegionBounds{2,3,8,8},
              "Reverse region creation should enclose the whole drawn area");
        check(regionDragBounds({},RegionCreate,{19.8f,15.8f},{25,30},20,16)==RegionBounds{19,15,1,1},
              "Region creation outside the canvas must stay within the map");
        MapRegionModel model; model.name="Secret marsh"; model.gmOnly=true; model.movementCost=4;
        model.blocksLineOfSight=true; model.description="Keep this metadata";
        region.apply(model);
        check(model.gmOnly && model.movementCost==4 && model.blocksLineOfSight && model.name=="Secret marsh" &&
            model.description=="Keep this metadata", "Resizing a region changed its gameplay or privacy settings");
    }

    MapModel atlas;
    atlas.width = 24; atlas.height = 20; resizeMapCells(atlas);
    for (auto &cell : atlas.cells) cell.kind = MapTileKind::Empty;
    for (int y = 15; y < atlas.height; ++y)
        for (int x = 0; x < atlas.width; ++x) mapCellAt(atlas,x,y)->kind = MapTileKind::Water;
    MapGeneratedFeature town;
    town.id = "town"; town.kind = MapGeneratedFeatureKind::Settlement; town.points = {{4.5f,4.5f}};
    MapGeneratedFeature road;
    road.id = "approach"; road.kind = MapGeneratedFeatureKind::Road;
    for (int i=0;i<=120;++i) road.points.push_back({4.5f+i*.1f,4.5f});
    MapGeneratedFeature river;
    river.id = "unrelated-mouth"; river.kind = MapGeneratedFeatureKind::River;
    river.points = {{20.5f,3.5f},{20.5f,18.5f}};
    atlas.generation.graph.features = {town,road,river};
    projectGeneratedPointFeatureClassificationToCells(atlas,town);
    projectGeneratedWayFeatureToCells(atlas,road,false);
    const MapModel original=atlas;
    moveGeneratedMapFeaturePoint(atlas,0,0,{6.25f,6.75f});
    finishGeneratedMapFeatureGeometryEdit(atlas,town);
    check(mapCellAt(atlas,4,4)->kind==MapTileKind::Empty && mapCellAt(atlas,6,6)->kind==MapTileKind::Settlement,
          "Moving a place must transfer its semantic symbol footprint");
    check(atlas.generation.graph.features[1].points.front().x==6.25f && atlas.generation.graph.features[1].points.front().y==6.75f,
          "An unlocked road ending at the town must follow its moved entrance");
    for (int y=0;y<atlas.height;++y) for (int x=18;x<atlas.width;++x)
        check(mapGenerationCellEqual(*mapCellAt(atlas,x,y),*mapCellAt(original,x,y)),
              "Moving a town unexpectedly changed a remote river/coast cell");

    atlas=original;
    atlas.generation.graph.features[1].locked = true;
    mapCellAt(atlas,4,4)->fogged = true;
    const auto oldCell = *mapCellAt(atlas,4,4);
    moveGeneratedMapFeaturePoint(atlas,0,0,{6.25f,6.75f});
    finishGeneratedMapFeatureGeometryEdit(atlas,town);
    check(atlas.generation.graph.features[1].points.front().x == road.points.front().x &&
              atlas.generation.graph.features[1].points.front().y == road.points.front().y,
          "Moving a town must not move a locked road entrance");
    check(mapCellAt(atlas,4,4)->fogged == oldCell.fogged,
          "Moving a point symbol must preserve authored fog");
    atlas=original;
    auto secondTown = town; secondTown.id = "shared-cell";
    atlas.generation.graph.features.push_back(secondTown);
    moveGeneratedMapFeaturePoint(atlas,0,0,{6.25f,6.75f});
    finishGeneratedMapFeatureGeometryEdit(atlas,town);
    check(mapCellAt(atlas,4,4)->kind == MapTileKind::Settlement,
          "Moving one place must preserve another place sharing the old cell");

    atlas = original;
    MapGeneratedFeature branch;
    branch.id = "joined-approach"; branch.kind = MapGeneratedFeatureKind::Road;
    branch.points = {{6.5f,4.5f},{6.5f,1.5f}};
    auto nearby = branch; nearby.id = "nearby-unconnected";
    nearby.points.front().y += .3f;
    auto fixedBranch = branch; fixedBranch.id = "locked-branch"; fixedBranch.locked = true;
    atlas.generation.graph.features.insert(atlas.generation.graph.features.end(), {branch,nearby,fixedBranch});
    moveGeneratedMapFeaturePoint(atlas,0,0,{6.25f,6.75f});
    finishGeneratedMapFeatureGeometryEdit(atlas,town);
    check(mapFeaturePointDistanceSquared(atlas.generation.graph.features[3].points.front(),
                                        atlas.generation.graph.features[1].points[20]) < .000001f,
          "Moving a town must preserve a branch joined inside its reshaped approach");
    check(mapFeaturePointDistanceSquared(atlas.generation.graph.features[4].points.front(),nearby.points.front()) < .000001f &&
              mapFeaturePointDistanceSquared(atlas.generation.graph.features[5].points.front(),fixedBranch.points.front()) < .000001f,
          "Junction repair must not move a nearby disconnected road or a locked branch");
    check(mapFeaturePointDistanceSquared(atlas.generation.graph.features[3].points.back(),branch.points.back()) < .000001f,
          "Repairing a junction must keep the branch's remote endpoint fixed");

    atlas = original;
    branch.points.front() = {10.5f,4.5f};
    atlas.generation.graph.features.push_back(branch);
    moveGeneratedMapFeatureSection(atlas,1,60,{10.5f,6.5f},road);
    finishGeneratedMapFeatureGeometryEdit(atlas,road);
    check(mapFeaturePointDistanceSquared(atlas.generation.graph.features[3].points.front(),
                                        atlas.generation.graph.features[1].points[60]) < .000001f,
          "Dragging a road section must preserve its branch junction too");

    atlas=original;
    const auto handles=generatedMapFeatureEditHandles(road);
    check(handles.size()<15 && handles.front()==0 && handles.back()==120,
          "Dense generated roads must expose spaced handles and both endpoints");
    moveGeneratedMapFeatureSection(atlas,1,60,{10.5f,6.5f},road);
    const auto edited=atlas.generation.graph.features[1];
    check(edited.points[59].y>6.4f && edited.points[61].y>6.4f && edited.points.front().y==4.5f && edited.points.back().y==4.5f,
          "Road edits must move a smooth local section without breaking fixed ends");
    moveGeneratedMapFeatureSection(atlas,1,60,{10.5f,6.5f},road);
    check(mapGenerationFeatureEqual(edited,atlas.generation.graph.features[1]),
          "Repeating a drag frame must not accumulate deformation");
    MapModel buildings;
    buildings.width=16; buildings.height=12; resizeMapCells(buildings);
    buildings.generation.recipe.generatorVersion=10;
    MapGeneratedFeature house;
    house.id="private-house"; house.kind=MapGeneratedFeatureKind::BuildingGroup;
    house.points={{2.5f,2.5f}}; house.gmOnly=true;
    buildings.generation.graph.features.push_back(house);
    for (const auto &p:std::vector<MapCellCoordinate>{{2,2},{3,2},{2,3}})
        applyMapCellTerrainDefaults(*mapCellAt(buildings,p.x,p.y),MapTileKind::Building);
    mapCellAt(buildings,7,5)->fogged=true;
    mapCellAt(buildings,7,5)->lighting=MapLightingMode::Darkness;
    mapCellAt(buildings,7,5)->elevation=4;
    const auto buildingOriginal=buildings;
    moveGeneratedMapFeaturePoint(buildings,0,0,{7.8f,5.2f});
    check(finishGeneratedMapFeatureGeometryEdit(buildings,house),"Building footprint move rejected valid ground");
    check(mapCellAt(buildings,2,2)->kind!=MapTileKind::Building && !mapCellAt(buildings,2,2)->blocksLineOfSight &&
          mapCellAt(buildings,3,2)->kind!=MapTileKind::Building && mapCellAt(buildings,2,3)->kind!=MapTileKind::Building,
          "Building move left part of the old L-shaped footprint or blockers behind");
    check(mapCellAt(buildings,7,5)->kind==MapTileKind::Building && mapCellAt(buildings,8,5)->kind==MapTileKind::Building &&
          mapCellAt(buildings,7,6)->kind==MapTileKind::Building && mapCellAt(buildings,8,6)->kind!=MapTileKind::Building,
          "Building move changed its actual L-shaped footprint");
    check(mapCellAt(buildings,7,5)->fogged && mapCellAt(buildings,7,5)->lighting==MapLightingMode::Darkness &&
          mapCellAt(buildings,7,5)->elevation==4 && buildings.generation.graph.features[0].gmOnly,
          "Building move changed destination fog/lighting/elevation or object privacy");
    for (int failureCase=0;failureCase<3;++failureCase)
    {
        buildings=buildingOriginal;
        if (failureCase==0) mapCellAt(buildings,8,5)->wayKind=MapWayKind::DirtRoad;
        if (failureCase==1) mapCellAt(buildings,8,5)->kind=MapTileKind::Water;
        const auto beforeInvalid=buildings;
        moveGeneratedMapFeaturePoint(buildings,0,0,failureCase==2?MapFeaturePoint{15.5f,11.5f}:MapFeaturePoint{7.5f,5.5f});
        check(!finishGeneratedMapFeatureGeometryEdit(buildings,house),"Invalid building destination was accepted");
        check(mapGenerationFeatureEqual(buildings.generation.graph.features[0],house) &&
              mapCellAt(buildings,2,2)->kind==MapTileKind::Building && mapCellAt(buildings,3,2)->kind==MapTileKind::Building &&
              mapCellAt(buildings,8,5)->kind==mapCellAt(beforeInvalid,8,5)->kind,
              "Rejected building move partially changed geometry or terrain");
    }
    // Object-mode interaction preserves the whole shape and the mouse-down
    // offset, including drags against map edges and repeated preview frames.
    atlas = original;
    check(gm::mapui::moveMapObject(atlas, 1, road, {3, 2}), "Whole route move was rejected");
    for (std::size_t i = 0; i < road.points.size(); ++i)
        check(mapFeaturePointDistanceSquared(atlas.generation.graph.features[1].points[i],
              {road.points[i].x+3, road.points[i].y+2}) < .000001f,
              "Whole move reshaped a route instead of translating it");
    const auto wholePreview = atlas.generation.graph.features[1];
    gm::mapui::moveMapObject(atlas,1,road,{3,2});
    check(mapGenerationFeatureEqual(wholePreview,atlas.generation.graph.features[1]),
          "Repeated whole-object preview accumulated movement");
    gm::mapui::moveMapObject(atlas,1,road,{100,100});
    const auto edgeRoute = atlas.generation.graph.features[1];
    check(std::abs((edgeRoute.points.back().x-edgeRoute.points.front().x) -
          (road.points.back().x-road.points.front().x)) < .00001f,
          "Map-edge clamp squashed the moved object");
    atlas.generation.recipe.lockedFeatureIds.push_back(road.id);
    check(!gm::mapui::moveMapObject(atlas,1,road,{1,1}) &&
          mapGenerationFeatureEqual(edgeRoute,atlas.generation.graph.features[1]),
          "Whole-object move ignored a recipe lock");

    MapGeneratedFeature polygon = house;
    check(gm::mapui::hitTestMapObject(buildingOriginal,{3.8f,2.8f},.15f,-1,
          gm::mapui::MapObjectEditMode::Move).featureIndex==0,
          "Clicking the non-anchor wing of a tactical building should select its owner");
    check(gm::mapui::hitTestMapObject(buildingOriginal,{3.8f,3.8f},.15f,-1,
          gm::mapui::MapObjectEditMode::Move).featureIndex==-1,
          "The empty corner of an L-shaped roof should not select the building");
    polygon.points={{2,2},{5,2},{5,5},{2,5}}; polygon.closed=true;
    buildings=buildingOriginal; buildings.generation.graph.features[0]=polygon;
    const auto interiorHit=gm::mapui::hitTestMapObject(buildings,{3.5f,3.5f},.2f,-1,
        gm::mapui::MapObjectEditMode::Move);
    check(interiorHit.featureIndex==0 && interiorHit.pointIndex==-1,
          "A click inside a building missed its selectable footprint");
    check(gm::mapui::hitTestMapObject(buildings,{6.5f,6.5f},.2f,-1,
          gm::mapui::MapObjectEditMode::Move).featureIndex==-1,
          "Building selection extends outside its actual polygon");
    check(!gm::mapui::mapObjectCanReshape(polygon),
          "Building exposes unsupported corner deformation");
    gm::mapui::moveMapObject(buildings,0,polygon,{4.7f,2.1f});
    check(buildings.generation.graph.features[0].points[0].x==7 &&
          buildings.generation.graph.features[0].points[2].x==10,
          "Building preview does not translate all corners on its cell grid");
    atlas=original;
    check(gm::mapui::hitTestMapObject(atlas,town.points.front(),.55f,1,
          gm::mapui::MapObjectEditMode::Reshape).featureIndex==1,
          "Selected road handle lost to a town drawn over its endpoint");
    check(gm::mapui::hitTestMapObject(atlas,town.points.front(),.55f,-1,
          gm::mapui::MapObjectEditMode::Move).featureIndex==0,
          "Normal selection should still prefer the place over a road endpoint");
    // Exercise the real render-plan compiler/cache through a long drag. Dirty
    // state changes on every frame, but only the canvas freezes its base art.
    // Release must compile the moved geometry, and undo must compile the old one.
    {
        MapModel preview = atlas;
        preview.width = 80; preview.height = 80; resizeMapCells(preview);
        preview.generation.continuousRendering = true;
        preview.generation.recipe.styleId = "illustrated_atlas";
        preview.generation.recipe.styleSnapshotId = "illustrated_atlas_v5";
        preview.generation.recipe.styleVersion = 5;
        const MapModel beforeDragMap = preview;
        MapRenderPlanRequest request;
        request.applyFog = false;
        request.layers = MapRenderPlanTerrain | MapRenderPlanWater | MapRenderPlanWays |
                         MapRenderPlanSymbols | MapRenderPlanLabels;
        MapRenderPlanCache cache;
        std::uint64_t serial = 100;
        const auto initial = cache.getOrCompile(preview, request, serial, 12.0f);
        const auto before = preview.generation.graph.features[1];
        const auto startedAt = serial;
        const auto begin = std::chrono::steady_clock::now();
        for (int frame = 1; frame <= 120; ++frame)
        {
            check(gm::mapui::moveMapObject(preview, 1, before, {frame * .01f, 1.0f}),
                  "Performance fixture must move the road on each frame");
            ++serial;
            const auto cached = cache.getOrCompile(preview, request,
                gm::mapui::mapObjectCanvasVersion(serial, true, startedAt), 12.0f);
            check(cached == initial, "Dragging must reuse base illustration rather than compiling per frame");
        }
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        check(cache.compileCount() == 1, "120 drag frames must cause zero additional illustration builds");
        finishGeneratedMapFeatureGeometryEdit(preview, before);
        ++serial;
        const auto released = cache.getOrCompile(preview, request,
            gm::mapui::mapObjectCanvasVersion(serial, false, startedAt), 12.0f);
        check(released != initial && cache.compileCount() == 2 && released->contentHash != initial->contentHash,
              "Release must rebuild the finished road and terrain exactly once");
        const auto *geometry = mapRenderPlanGeneratedGeometry(*released, before.id);
        check(geometry && mapFeaturePointDistanceSquared(geometry->back(), before.points.back()) > .1f,
              "Released render plan must contain the moved road, not frozen geometry");
        preview = beforeDragMap;
        const auto undone = cache.getOrCompile(preview, request,
            gm::mapui::mapObjectCanvasVersion(++serial, false, startedAt), 12.0f);
        check(undone->contentHash == initial->contentHash && cache.compileCount() == 3,
              "Undo after release must restore the original illustration");
        std::cout << "80x80 map: 120 move/cache frames in " << elapsed
                  << " ms; zero per-frame illustration builds, one release build\n";
        MapRenderPlanCache oldCache;
        const auto oldBegin = std::chrono::steady_clock::now();
        for (int frame = 1; frame <= 8; ++frame)
        {
            gm::mapui::moveMapObject(preview, 1, before, {frame * .01f, 1.0f});
            oldCache.getOrCompile(preview, request, ++serial, 12.0f);
        }
        std::cout << "Previous invalidation path: " << oldCache.compileCount()
                  << " illustration builds for 8 updates in "
                  << std::chrono::duration<double, std::milli>(
                         std::chrono::steady_clock::now() - oldBegin).count() << " ms\n";
    }
    {
        MapModel network=original;
        const auto before=network.generation.graph.features[1];
        gm::mapui::moveMapObject(network,1,before,{2,2});
        finishGeneratedMapFeatureGeometryEdit(network,before);
        check(mapFeaturePointDistanceSquared(network.generation.graph.features[1].points.front(),town.points.front())<.0001f,
              "Moving a connected road must preserve its city entrance");
        MapGeneratedFeature destination=town; destination.id="destination"; destination.points={{19,8}};
        network.generation.graph.features.push_back(destination);
        auto roadBefore=network.generation.graph.features[1];
        moveGeneratedMapFeatureSection(network,1,120,{18.6f,8.2f},roadBefore);
        finishGeneratedMapFeatureGeometryEdit(network,roadBefore);
        check(mapFeaturePointDistanceSquared(network.generation.graph.features[1].points.back(),{19,8})<.0001f,
              "A nearby loose road end must snap to its new city");
        network=original;
        MapGeneratedFeature loose=road; loose.id="loose"; loose.points={{9,7},{9,10}};
        network.generation.graph.features.push_back(loose);
        moveGeneratedMapFeaturePoint(network,3,0,{9,4.8f});
        finishGeneratedMapFeatureGeometryEdit(network,loose);
        check(std::abs(network.generation.graph.features[3].points.front().y-4.5f)<.0001f,
              "Road endpoints should snap to the middle of another road");
        network=original;
        MapGeneratedFeature newTown=town; newTown.id="new-town";newTown.points={{10,10}};
        network.generation.graph.features.push_back(newTown);
        moveGeneratedMapFeaturePoint(network,3,0,{10,5.5f});
        finishGeneratedMapFeatureGeometryEdit(network,newTown);
        check(network.generation.graph.features.size()==5 &&
              mapFeaturePointDistanceSquared(network.generation.graph.features.back().points.front(),{10,4.5f})<.0001f,
              "Placing an unconnected town beside a road should add an approach without moving the through-road");
        auto movingTown=network.generation.graph.features[3];
        moveGeneratedMapFeaturePoint(network,3,0,town.points.front());
        check(!finishGeneratedMapFeatureGeometryEdit(network,movingTown) &&
              mapFeaturePointDistanceSquared(network.generation.graph.features[3].points.front(),movingTown.points.front())<.0001f,
              "Cities must not be dropped on top of each other");
        network=original;
        newTown.points={{10.5f,4.5f}};
        network.generation.graph.features.push_back(newTown);
        MapRouteModel travel;travel.startX=10;travel.startY=4;travel.endX=16;travel.endY=4;
        network.routes.push_back(travel);
        moveGeneratedMapFeaturePoint(network,3,0,{10.5f,7.5f});
        finishGeneratedMapFeatureGeometryEdit(network,newTown);
        check(network.routes.front().startX==10 && network.routes.front().startY==7,
              "A travel route endpoint must follow a moved city");
        check(mapFeaturePointDistanceSquared(nearestMapRoadPoint(network.generation.graph.features[1],{10.5f,7.5f}),{10.5f,7.5f})<.0001f,
              "A city placed on a through-road must keep that connection when moved again");
        network=original;
        moveGeneratedMapFeaturePoint(network,0,0,{5,17});
        check(!finishGeneratedMapFeatureGeometryEdit(network,town) &&
              mapFeaturePointDistanceSquared(network.generation.graph.features[0].points.front(),town.points.front())<.0001f,
              "A city move into water must be rejected without moving its roads");
    }
    {
        MapModel terrain;terrain.width=20;terrain.height=16;resizeMapCells(terrain);
        for(auto p:{MapCellCoordinate{2,2},{3,2},{3,3}}) {
            applyMapCellTerrainDefaults(*mapCellAt(terrain,p.x,p.y),MapTileKind::Forest);
            mapCellAt(terrain,p.x,p.y)->elevation=3;
        }
        applyMapCellTerrainDefaults(*mapCellAt(terrain,15,10),MapTileKind::Forest);
        auto group=gm::mapui::selectTerrainGroup(terrain,2,2);
        check(group.cells.size()==3,"Terrain selection must group the patch and exclude remote trees");
        check(group.sourceCellCount==3 && !group.isPartial(),
              "A directly selected patch must report its full connected cell count");
        mapCellAt(terrain,7,5)->fogged=true;
        mapCellAt(terrain,7,5)->lighting=MapLightingMode::Darkness;
        const auto terrainUndo=terrain;
        check(gm::mapui::moveTerrainGroup(terrain,group,5,3),"Forest area move rejected clear land");
        check(mapCellAt(terrain,2,2)->kind==MapTileKind::Empty && mapCellAt(terrain,7,5)->kind==MapTileKind::Forest &&
              mapCellAt(terrain,8,6)->kind==MapTileKind::Forest && mapCellAt(terrain,15,10)->kind==MapTileKind::Forest,
              "Group move must preserve the whole patch and leave remote trees in place");
        check(mapCellAt(terrain,7,5)->fogged && mapCellAt(terrain,7,5)->lighting==MapLightingMode::Darkness &&
              mapCellAt(terrain,7,5)->elevation==3,"Terrain movement must preserve destination secrecy and transfer relief");
        terrain=terrainUndo;mapCellAt(terrain,7,5)->kind=MapTileKind::Water;
        check(!gm::mapui::moveTerrainGroup(terrain,group,5,3) && mapCellAt(terrain,2,2)->kind==MapTileKind::Forest,
              "Invalid terrain destination must reject the whole transaction");
        terrain=terrainUndo;mapCellAt(terrain,7,5)->wayKind=MapWayKind::DirtRoad;
        check(!gm::mapui::moveTerrainGroup(terrain,group,5,3),"Terrain must not bury a destination road");
        terrain=terrainUndo;
        check(gm::mapui::moveTerrainGroup(terrain,group,1,0) && mapCellAt(terrain,4,3)->kind==MapTileKind::Forest,
              "Overlapping terrain move must read from an immutable source snapshot");
        terrain=terrainUndo;
        mapCellAt(terrain,2,2)->kind=MapTileKind::Mountain;
        mapCellAt(terrain,3,2)->kind=MapTileKind::Hill;
        mapCellAt(terrain,3,3)->kind=MapTileKind::Mountain;
        group=gm::mapui::selectTerrainGroup(terrain,2,2);
        check(group.cells.size()==3 && gm::mapui::moveTerrainGroup(terrain,group,5,3) &&
              mapCellAt(terrain,8,5)->kind==MapTileKind::Hill && mapCellAt(terrain,8,6)->kind==MapTileKind::Mountain,
              "A mountain area must move its connected foothills and retain their terrain types");
    }
    {
        auto entranceMap=original;
        entranceMap.generation.recipe.styleId="illustrated_atlas";
        entranceMap.generation.recipe.styleVersion=5;
        entranceMap.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        const auto entranceInk=gm::map_render::buildAtlasCityEntrances(entranceMap);
        check(!entranceInk.empty(),"A connected city needs shared entrance artwork");
        const auto route=gm::map_render::atlasRouteEntrancePoints(entranceMap,{{4.5f,4.5f},{12,4.5f}});
        check(route.size()==2 && std::abs(route.front().x-5.45f)<.001f,
              "Travel overlays must stop at city entrances instead of crossing the symbol");
        auto hiddenCity=entranceMap;
        hiddenCity.generation.graph.features.erase(hiddenCity.generation.graph.features.begin());
        const auto uncut=gm::map_render::atlasRouteEntrancePoints(hiddenCity,{{4.5f,4.5f},{12,4.5f}});
        check(uncut.front().x==4.5f,"A filtered-out city must not leave a gap in a public route");
        entranceMap.generation.graph.features.erase(entranceMap.generation.graph.features.begin()+1);
        check(gm::map_render::buildAtlasCityEntrances(entranceMap).empty(),
              "No road may leave a phantom gate when filtered out of player output");
    }
    {
        MapModel alpine;alpine.width=32;alpine.height=32;resizeMapCells(alpine);
        alpine.atlasLevel=MapLevel::Region;alpine.generation.continuousRendering=true;
        auto &recipe=alpine.generation.recipe;recipe.recipeVersion=7;recipe.generatorVersion=12;
        recipe.styleId="illustrated_atlas";recipe.styleVersion=6;recipe.styleSnapshotId="illustrated_atlas_v6";
        for(auto &cell:alpine.cells)cell.kind=MapTileKind::Mountain;
        for(auto kind:{MapGeneratedFeatureKind::Road,MapGeneratedFeatureKind::River})
            for(int angle=0;angle<8;++angle)for(float width:{.8f,2.f,4.f}) {
                const float a=angle*3.14159265f/4.f;
                MapGeneratedFeature f;f.id="alpine-crossing";f.kind=kind;f.width=width;
                f.points={{16-10*std::cos(a),16-10*std::sin(a)},{16+10*std::cos(a),16+10*std::sin(a)}};
                alpine.generation.graph.features={f};
                std::vector<gm::map_geometry::MapRoadCorridor> corridors{{f.id,f.points,.8f}};
                const auto ink=gm::map_render::buildAtlasMountainCrossingInk(alpine,corridors);
                check(!ink.empty(),"Mountain crossing assets missing for direction/width");
                for(const auto &piece:ink)for(auto p:piece.points) {
                    check(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.y>=0 && p.x<32 && p.y<32,
                          "Crossing asset escaped map or produced nonfinite geometry");
                    check(gm::map_geometry::roadPointSegmentDistanceSquared(p,f.points[0],f.points[1])>.08f*.08f,
                          "Rock artwork blocks the open channel");
                }
                check(gm::map_render::buildAtlasMountainCrossingInk(alpine,{}).empty(),"Hidden route left crossing artwork");
                const auto foothills=gm::map_render::buildAtlasValleyReliefInk(alpine,corridors);
                float nearestLeft=100.f,nearestRight=100.f;
                for(const auto &piece:foothills)for(auto p:piece.points) {
                    const float side=-(p.x-16)*std::sin(a)+(p.y-16)*std::cos(a);
                    (side<0?nearestLeft:nearestRight)=std::min(side<0?nearestLeft:nearestRight,std::abs(side));
                    check(std::abs(side)>.08f,"Integrated foothill blocks the channel");
                }
                const float bank=kind==MapGeneratedFeatureKind::River?std::max(.20f,width*.12f+.10f):std::max(.19f,width*.15f+.10f);
                check(nearestLeft<=bank+.16f && nearestRight<=bank+.16f,
                      "Upright mountain foothills remain detached from the crossing banks");
                auto blocked=corridors;
                blocked.push_back({"other-crossing",{{0,16},{31,16}},100.f});
                check(gm::map_render::buildAtlasMountainCrossingInk(alpine,blocked).empty(),
                      "Rejected crossing banks left detached wet highlights");
                check(gm::map_render::buildAtlasValleyReliefInk(alpine,blocked).empty(),
                      "Integrated foothills cover another crossing");
                const auto valley=gm::map_render::buildAtlasMountainLandformInk(alpine,corridors);
                check(!valley.empty(),"Natural mountain slopes missing for direction/width");
                float nearSlope=100.f,farSlope=0.f;
                for(const auto &piece:valley) {
                    if(piece.filled)check(gm::map_render::atlasSilhouetteClearsRoads(piece.points,{{f.id,f.points,bank}},.02f),
                        "Valley face crosses the open road or river");
                    for(auto p:piece.points) {
                        check(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.y>=0 && p.x<32 && p.y<32,
                            "Natural mountain slope escaped the map");
                        const float distance=std::sqrt(gm::map_geometry::roadPointSegmentDistanceSquared(p,f.points[0],f.points[1]));
                        nearSlope=std::min(nearSlope,distance);farSlope=std::max(farSlope,distance);
                    }
                }
                check(nearSlope<bank+.7f && farSlope>bank+1.5f,"Valley slopes do not connect the floor to the mountain feet");
                check(gm::map_render::buildAtlasMountainLandformInk(alpine,{}).empty(),"Hidden route left natural slopes");
                check(gm::map_render::buildAtlasMountainLandformInk(alpine,blocked).empty(),"Natural slopes cover another crossing");
                auto water=alpine;for(auto &cell:water.cells)cell.kind=MapTileKind::Water;
                check(gm::map_render::buildAtlasMountainLandformInk(water,corridors).empty(),"Natural slopes appear in water");
                auto bent=f;bent.points.insert(bent.points.begin()+1,{16.f-std::sin(a)*3.f,16.f+std::cos(a)*3.f});
                alpine.generation.graph.features={bent};
                const std::vector<gm::map_geometry::MapRoadCorridor> bendCorridors{{f.id,bent.points,.8f}};
                const auto bendInk=gm::map_render::buildAtlasMountainLandformInk(alpine,bendCorridors);
                check(!bendInk.empty(),"Bent valley lost all mountain slopes");
                for(const auto &piece:bendInk)if(piece.filled) {
                    check(gm::map_render::atlasSilhouetteClearsRoads(piece.points,{{f.id,bent.points,bank}},.02f),
                        "Mountain slope folds into a bend's open channel");
                    check(piece.vertexColors.size()==3,"Valley slope lost interpolated ground blending");
                }
                const auto repeated=gm::map_render::buildAtlasMountainLandformInk(alpine,bendCorridors);
                check(repeated.size()==bendInk.size(),"Same valley produced different ink counts");
                for(std::size_t k=0;k<std::min(repeated.size(),bendInk.size());++k)
                    for(std::size_t j=0;j<bendInk[k].points.size();++j)
                        check(repeated[k].points[j].x==bendInk[k].points[j].x && repeated[k].points[j].y==bendInk[k].points[j].y,
                            "Same valley produced different artwork geometry");
            }
    }
    {
        MapModel alpine;alpine.width=32;alpine.height=32;resizeMapCells(alpine);
        alpine.atlasLevel=MapLevel::Region;alpine.generation.recipe.recipeVersion=9;
        for(auto &cell:alpine.cells)cell.kind=MapTileKind::Mountain;
        for(int angle=0;angle<8;++angle)for(float width:{.8f,2.f,4.f}) {
            const float a=angle*3.14159265f/4.f,tx=std::cos(a),ty=std::sin(a);
            gm::map_render::AtlasMountainFootprint mountain;
            mountain.base={16-ty*2.5f,16+tx*2.5f};mountain.width=.8f;mountain.height=1.4f;mountain.main=true;mountain.variant=137;
            mountain.outline={{mountain.base.x-.8f,mountain.base.y},{mountain.base.x,mountain.base.y-1.4f},
                              {mountain.base.x+.8f,mountain.base.y}};
            std::vector<gm::map_render::AtlasInkPrimitive> roadInk;
            for(auto kind:{MapGeneratedFeatureKind::Road,MapGeneratedFeatureKind::River}) {
                MapGeneratedFeature f;f.id="anchored-crossing";f.kind=kind;f.width=width;
                f.points={{16-10*tx,16-10*ty},{16+10*tx,16+10*ty}};alpine.generation.graph.features={f};
                const std::vector<gm::map_geometry::MapRoadCorridor> corridors{{f.id,f.points,.8f}};
                const auto ink=gm::map_render::buildAtlasMountainButtressInk(alpine,corridors,{mountain});
                check(!ink.empty(),"Mountain-rooted artwork missing for direction/width");
                const float edge=kind==MapGeneratedFeatureKind::River?std::max(.20f,width*.12f+.10f):std::max(.19f,width*.15f+.10f);
                bool rooted=false;
                for(const auto &piece:ink) {
                    if(piece.filled)check(gm::map_render::atlasSilhouetteClearsRoads(piece.points,{{f.id,f.points,edge}},.02f),
                        "Mountain-rooted rock face covers the open crossing");
                    for(auto p:piece.points) {
                        check(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.y>=0 && p.x<32 && p.y<32,
                              "Mountain-rooted artwork escaped the map");
                        rooted=rooted || gm::map_render::atlasPointInsidePolygon(mountain.outline,p);
                    }
                }
                check(rooted,"Crossing rocks do not touch the visible mountain footprint");
                check(gm::map_render::buildAtlasMountainButtressInk(alpine,corridors,{}).empty(),"Removed mountain left orphan rock slopes");
                check(gm::map_render::buildAtlasMountainButtressInk(alpine,{}, {mountain}).empty(),"Hidden crossing left orphan rock slopes");
                auto removed=alpine;removed.generation.graph.features.clear();
                check(gm::map_render::buildAtlasMountainButtressInk(removed,corridors,{mountain}).empty(),"Removed route left mountain crossing artwork");
                auto water=alpine;for(auto &cell:water.cells)cell.kind=MapTileKind::Water;
                check(gm::map_render::buildAtlasMountainButtressInk(water,corridors,{mountain}).empty(),"Anchored rocks appeared in open water");
                auto blocked=corridors;blocked.push_back({"junction",{{0,16},{31,16}},100.f});
                check(gm::map_render::buildAtlasMountainButtressInk(alpine,blocked,{mountain}).empty(),"Anchored rocks cover another crossing");
                const auto repeat=gm::map_render::buildAtlasMountainButtressInk(alpine,corridors,{mountain});
                check(repeat.size()==ink.size(),"Anchored artwork is not deterministic");
                for(std::size_t k=0;k<std::min(repeat.size(),ink.size());++k)
                    for(std::size_t j=0;j<ink[k].points.size();++j)
                        check(repeat[k].points[j].x==ink[k].points[j].x && repeat[k].points[j].y==ink[k].points[j].y,
                              "Repeated mountain-rooted artwork changed geometry");
                auto bentMap=alpine;auto bent=f;
                bent.points.insert(bent.points.begin()+1,{16.f-ty*.4f,16.f+tx*.4f});bentMap.generation.graph.features={bent};
                const auto bentInk=gm::map_render::buildAtlasMountainButtressInk(bentMap,{{f.id,bent.points,.8f}},{mountain});
                check(!bentInk.empty(),"Bent crossing lost all mountain-rooted artwork");
                for(const auto &piece:bentInk)if(piece.filled)
                    check(gm::map_render::atlasSilhouetteClearsRoads(piece.points,{{f.id,bent.points,edge}},.02f),
                          "Rock face blocks a bent crossing");
                if(kind==MapGeneratedFeatureKind::Road)roadInk=ink;
                else {
                    bool different=ink.size()!=roadInk.size();
                    for(std::size_t k=0;k<std::min(ink.size(),roadInk.size());++k)
                        different=different || ink[k].color.r!=roadInk[k].color.r || ink[k].color.b!=roadInk[k].color.b;
                    check(different,"Road cutting and eroded river gorge use identical rock artwork");
                }
            }
        }
    }
    {
        const auto box=gm::mapui::mapSelectionBox({8,8},{4,4});
        MapGeneratedFeature across; across.kind=MapGeneratedFeatureKind::Road;across.points={{0,6},{12,6}};
        check(gm::mapui::selectionBoxIntersectsFeature(box,across),"Box selection must find a crossing road with both endpoints outside");
        across.points={{0,3},{12,3}};
        check(!gm::mapui::selectionBoxIntersectsFeature(box,across),"Box selection must reject nearby lines outside the rectangle");
        MapGeneratedFeature building;building.closed=true;building.kind=MapGeneratedFeatureKind::BuildingGroup;
        building.points={{1,1},{12,1},{12,12},{1,12}};
        check(gm::mapui::selectionBoxIntersectsFeature(box,building),"A box inside a building should find its footprint");
        MapModel selectionMap;selectionMap.width=16;selectionMap.height=16;resizeMapCells(selectionMap);
        across.id="crossing";across.points={{0,6},{12,6}};selectionMap.generation.graph.features.push_back(across);
        for(int x=2;x<12;++x) mapCellAt(selectionMap,x,7)->kind=MapTileKind::Forest;
        const auto candidates=gm::mapui::collectMapAreaSelection(selectionMap,box);
        check(candidates.size()==2 && candidates[0].category==2 && candidates[1].category==3,
              "Area picker must list the road and one forest group, not individual trees");
        const auto &forest=candidates.back();
        check(forest.terrain.cells.size()==4 && forest.terrain.contains(4,7) &&
              !forest.terrain.contains(3,7) && !forest.terrain.contains(8,7),
              "Choosing a terrain candidate must retain only the boxed part of the forest");
        check(forest.terrain.sourceCellCount==10 && forest.terrain.isPartial(),
              "Boxed terrain feedback must distinguish four selected cells from the ten-cell source patch");
        const auto wholeCandidates=gm::mapui::collectMapAreaSelection(selectionMap,{0,0,16,16});
        check(wholeCandidates.back().terrain.cells.size()==10 && !wholeCandidates.back().terrain.isPartial(),
              "A box enclosing the whole patch must report a full selection");
        auto selectedItems=candidates;
        check(gm::mapui::selectedAreaCount(selectedItems)==2,"Box must immediately select road and terrain together");
        auto combinedMap=selectionMap;auto combinedItems=candidates;
        projectGeneratedWayFeatureToCells(combinedMap,combinedMap.generation.graph.features[0],false);
        check(gm::mapui::moveAreaSelection(combinedMap,combinedItems,0,-1),"Moving forest into vacated selected road footprint should succeed");
        check(mapCellAt(combinedMap,4,6)->kind==MapTileKind::Forest && combinedMap.generation.graph.features[0].points[0].y==5,
              "Combined way/terrain move lost one selected element");
        selectedItems[0].selected=false;
        check(gm::mapui::selectedAreaCount(selectedItems)==1,"Sidebar exclusion must preserve other selections");
        auto movedMap=selectionMap;
        check(gm::mapui::moveAreaSelection(movedMap,selectedItems,0,3),"Selected forest movement failed");
        check(movedMap.generation.graph.features[0].points[0].y==6,"Deselected road moved with forest");
        auto invalidItems=candidates;auto invalidMap=selectionMap;
        check(!gm::mapui::moveAreaSelection(invalidMap,invalidItems,100,0),"Out-of-bounds group move should fail atomically");
        check(mapCellAt(invalidMap,4,7)->kind==MapTileKind::Forest,"Rejected group move mutated document");
        selectionMap.generation.graph.features.clear();
        for(auto &cell:selectionMap.cells)if(cell.kind==MapTileKind::Empty)cell.kind=MapTileKind::Dirt;
        const auto selected=forest.terrain;
        const auto unchanged=selectionMap;
        check(gm::mapui::moveTerrainGroup(selectionMap,selected,0,3),"Moving a boxed forest portion failed");
        const auto moved=gm::mapui::translatedTerrainSelection(selectionMap,selected,0,3);
        check(moved.cells.size()==4 && moved.contains(4,10) && !moved.contains(3,10),
              "Selection must keep its exact footprint after moving rather than flood-filling again");
        check(moved.sourceCellCount==10 && moved.isPartial(),
              "Moving a partial selection must preserve the scope shown before dragging");
        for(int x=2;x<12;++x)
            check(mapCellAt(selectionMap,x,7)->kind==((x>=4 && x<8)?MapTileKind::Empty:MapTileKind::Forest),
                  "Moving a portion changed unselected forest cells");
        selectionMap=unchanged;
        check(mapCellAt(selectionMap,4,7)->kind==MapTileKind::Forest &&
              mapCellAt(selectionMap,4,10)->kind==MapTileKind::Dirt,
              "Restoring the edit snapshot must undo only the moved terrain");
    }
    {
        MapModel fog = original;
        fog.width=80; fog.height=80; resizeMapCells(fog);
        fog.generation.continuousRendering=true;
        fog.generation.recipe.styleId="illustrated_atlas";
        fog.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        fog.generation.recipe.styleVersion=5;
        for(auto &cell:fog.cells) cell.fogged=false;
        const auto undo=fog;
        MapRenderPlanRequest request; request.applyFog=false;
        MapRenderPlanCache cache;
        std::uint64_t serial=700, beforeStroke=serial;
        const auto art=cache.getOrCompile(fog,request,serial,12);
        const auto begin=std::chrono::steady_clock::now();
        for(int frame=0;frame<120;++frame)
        {
            for(const auto &p:mapBrushFootprint(fog,frame%80,frame/2%80,12,true))
                mapCellAt(fog,p.x,p.y)->fogged=true;
            ++serial;
            check(cache.getOrCompile(fog,request,
                gm::mapui::mapObjectCanvasVersion(serial,true,beforeStroke),12)==art,
                "Fog stroke must reuse the fog-free GM illustration");
            int covered=0;
            gm::mapui::visitEditorFog(fog,0,0,79,79,
                [&](int a,int b,int c,int d){covered+=(c-a)*(d-b);},
                [](int,int,int,int){},true);
            check(covered==std::count_if(fog.cells.begin(),fog.cells.end(),
                [](const auto &cell){return cell.fogged;}),"Fog overlay must follow live cells during the stroke");
        }
        std::cout<<"80x80 fog: 120 brush/cache/overlay updates in "
            <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()
            <<" ms; no illustration rebuild during stroke\n";
        check(cache.compileCount()==1,"Fog painting rebuilt expensive terrain during stroke");
        cache.getOrCompile(fog,request,gm::mapui::mapObjectCanvasVersion(++serial,false,beforeStroke),12);
        check(cache.compileCount()==2,"Fog release must refresh final draft once");
        fog=undo;
        const auto restored=cache.getOrCompile(fog,request,++serial,12);
        check(restored->contentHash==art->contentHash,"Fog undo must restore original render state");
        for(auto &cell:fog.cells) cell.fogged=true;
        int spans=0,edges=0,coverage=0;
        auto fill=[&](int a,int b,int c,int d){++spans;coverage+=(c-a)*(d-b);};
        auto edge=[&](int,int,int,int){++edges;};
        gm::mapui::visitEditorFog(fog,0,0,79,79,fill,edge,true);
        check(spans==80 && edges==4 && coverage==6400,
            "Solid fog must batch rows and draw only four exterior edges, without cell seams");
        mapCellAt(fog,40,40)->fogged=false;
        spans=edges=coverage=0;
        gm::mapui::visitEditorFog(fog,0,0,79,79,fill,edge,true);
        check(coverage==6399 && edges==8,"Revealed hole must remain unshaded with its own boundary");
        spans=edges=coverage=0;
        gm::mapui::visitEditorFog(fog,10,10,19,19,fill,edge,true);
        check(coverage==100 && edges==0,"Viewport clipping must not invent a fog boundary");
    }
    for(const auto terrain:{MapTileKind::Forest,MapTileKind::Mountain,MapTileKind::Water})
    {
        MapModel painted=original;painted.width=80;painted.height=80;resizeMapCells(painted);
        painted.generation.continuousRendering=true;
        painted.generation.recipe.styleId="illustrated_atlas";
        painted.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        painted.generation.recipe.styleVersion=5;
        for(auto &cell:painted.cells) cell.fogged=true;
        const auto undo=painted;
        MapBrushStrokePreview stroke;stroke.begin(painted);
        MapRenderPlanRequest request;request.applyFog=false;
        MapRenderPlanCache cache;std::uint64_t serial=900;const auto started=serial;
        const auto initial=cache.getOrCompile(painted,request,serial,12);
        const auto begin=std::chrono::steady_clock::now();
        for(int frame=0;frame<120;++frame)
        {
            const auto footprint=mapBrushFootprint(painted,frame%70+5,frame/2%70+5,24,true);
            applyMapTerrainBrush(painted,footprint,terrain,0,false);
            stroke.record(footprint);++serial;
            check(cache.getOrCompile(painted,request,
                gm::mapui::mapObjectCanvasVersion(serial,true,started),12)==initial,
                "Terrain/water painting rebuilt the illustration during a stroke");
            int area=0;
            stroke.visitSpans(0,0,79,79,[&](int x0,int y0,int x1,int y1){area+=(x1-x0)*(y1-y0);});
            check(area==std::count(stroke.touched.begin(),stroke.touched.end(),1),
                "Stroke preview lost or duplicated painted coverage");
        }
        std::cout<<"80x80 "<<mapTileKindLabel(terrain)<<": 120 size-24 brush/cache/preview updates in "
            <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<" ms\n";
        check(cache.compileCount()==1,"Brush updates must keep dirty tracking separate from GM art");
        const auto finished=cache.getOrCompile(painted,request,
            gm::mapui::mapObjectCanvasVersion(++serial,false,started),12);
        check(cache.compileCount()==2 && finished->contentHash!=initial->contentHash,
            "Release must compile the actual painted terrain exactly once");
        const auto redo=painted;
        painted=undo;
        check(cache.getOrCompile(painted,request,++serial,12)->contentHash==initial->contentHash,
            "Undo must restore original terrain/water");
        painted=redo;
        check(cache.getOrCompile(painted,request,++serial,12)->contentHash==finished->contentHash,
            "Redo must restore the finished terrain/water stroke");
        check(std::all_of(painted.cells.begin(),painted.cells.end(),[](const auto &c){return c.fogged;}),
            "Painting terrain/water must not reveal hidden cells");
        stroke.clear();int stale=0;
        stroke.visitSpans(0,0,79,79,[&](int,int,int,int){++stale;});
        check(stale==0,"Ending a stroke must remove its temporary preview");
        stroke.begin(painted);stroke.record({{-1,0},{80,80},{2,2},{2,2}});
        int clipped=0;
        stroke.visitSpans(0,0,3,3,[&](int a,int b,int c,int d){clipped+=(c-a)*(d-b);});
        check(clipped==1,"Preview must clip edges and deduplicate overlapping stamps");
    }
    {
        auto cached=original, uncached=original;
        const auto protection=mapProtectedWayBrushCells(cached);
        for(const bool erase:{false,true})
        for(int frame=0;frame<12;++frame)
        {
            const auto footprint=mapBrushFootprint(cached,frame+2,5,8,true);
            applyMapWayBrush(cached,footprint,MapWayKind::River,erase,&protection);
            applyMapWayBrush(uncached,footprint,MapWayKind::River,erase);
        }
        for(std::size_t i=0;i<cached.cells.size();++i)
            check(mapGenerationCellEqual(cached.cells[i],uncached.cells[i]),
                "Cached river/road protection changed authoring semantics");
    }
    {
        MapModel curved;
        curved.width=32; curved.height=24; curved.cells.resize(32*24);
        for(auto &cell:curved.cells) cell.kind=MapTileKind::Dirt;
        curved.atlasLevel=MapLevel::Region;
        curved.generation.enabled=true; curved.generation.continuousRendering=true;
        auto &recipe=curved.generation.recipe;
        recipe.generatorVersion=12; recipe.seed=712345; recipe.styleId="illustrated_atlas";
        recipe.styleVersion=6; recipe.styleSnapshotId="illustrated_atlas_v6";
        MapGeneratedFeature river;
        river.id="curved-river"; river.kind=MapGeneratedFeatureKind::River;
        river.width=.7f; river.points={{5,10},{11,14},{21,16},{29,22}};
        curved.generation.graph.features={river};
        MapRenderPlanRequest request; request.applyFog=false; request.layers=MapRenderPlanWater;
        const auto plan=compileMapRenderPlan(curved,request);
        int checked=0, rawMisses=0;
        for(const auto &primitive:plan.primitives) if(primitive.sourceId==river.id)
            for(const auto point:primitive.points)
            {
                ++checked;
                if(hitTestGeneratedMapFeature(curved,point,.18f).featureIndex<0) ++rawMisses;
                const auto hit=gm::mapui::hitTestMapObject(curved,point,.18f,-1,gm::mapui::MapObjectEditMode::Move);
                check(hit.featureIndex==0,"Every rendered curved river point must select the river");
                check(hit.segmentStartIndex==-1,"Rendered segments must never become raw insertion indices");
                check(gm::mapui::hitTestMapObject(curved,point,.18f,-1,gm::mapui::MapObjectEditMode::Move,&plan).featureIndex==0,
                      "Cached displayed geometry must select the same river");
            }
        check(checked>20 && rawMisses>0,"Curved picking regression must exercise the original mismatch");
        const auto handle=gm::mapui::hitTestMapObject(curved,river.points[1],.18f,0,gm::mapui::MapObjectEditMode::Reshape);
        check(handle.featureIndex==0 && handle.pointIndex==1,"Reshape must retain source control handles");
        MapGeneratedFeature distant;distant.id="new-object";distant.kind=MapGeneratedFeatureKind::Landmark;distant.points={{1,1}};
        curved.generation.graph.features.insert(curved.generation.graph.features.begin(),distant);
        const auto shown=mapFeaturePresentationPoints(curved,river);
        for(auto p:shown) {
            const auto members=gm::mapui::collectMapAreaSelection(curved,{p.x-.02f,p.y-.02f,p.x+.02f,p.y+.02f});
            check(std::any_of(members.begin(),members.end(),[](const auto &i){return i.kind==gm::mapui::AreaObjectKind::Generated && i.index==1;}),
                  "Box selection missed the visible curved river");
        }
        check(gm::mapui::hitTestMapObject(curved,shown[shown.size()/2],.18f,-1,gm::mapui::MapObjectEditMode::Move,&plan).featureIndex==1,
              "Displayed geometry must resolve current indices through stable IDs");
    }
    return failures == 0 ? 0 : 1;
}
