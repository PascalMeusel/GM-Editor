#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorOutputPreviewRendering.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapBrushRenderSnapshot.hpp"
#include "Models/Map/MapAuthoring.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

// Headless vector rendering deliberately has no GPU or texture backend. These
// atlas seams disable optional bitmap decoration; actual ImGui geometry and
// the production renderMap(plan) dispatch below remain unchanged.
bool hasAtlasTile(MapTileKind) { return false; }
ImTextureID getAtlasTileTextureId(MapTileKind) { return 0; }
ImTextureID getAtlasTileTextureId(MapTileKind, bool, bool, bool, bool, int, int) { return 0; }
ImTextureID getAtlasTileTextureId(const MapTileRenderContext &) { return 0; }
void getAtlasTileUV(MapTileKind, ImVec2 &, ImVec2 &) {}
void getAtlasTileUV(MapTileKind, ImVec2 &, ImVec2 &, bool, bool, bool, bool, int, int) {}
void getAtlasTileUV(const MapTileRenderContext &, ImVec2 &, ImVec2 &) {}
int getTerrainTransitionOverlayCount(const MapTileRenderContext &) { return 0; }
ImTextureID getTerrainTransitionOverlayTextureId(const MapTileRenderContext &, int) { return 0; }
void getTerrainTransitionOverlayUV(const MapTileRenderContext &, int, ImVec2 &, ImVec2 &) {}
bool hasIntegratedWayPrototypeTile(const MapWayRenderContext &) { return false; }
ImTextureID getIntegratedWayPrototypeTextureId() { return 0; }
void getIntegratedWayPrototypeUV(const MapWayRenderContext &, ImVec2 &, ImVec2 &) {}
bool hasRuntimeAtlasAsset(const std::string &, bool) { return false; }
bool getRuntimeAtlasAssetDrawData(const std::string &, MapRuntimeAtlasAssetDrawData &, bool) { return false; }
ImTextureID getRuntimeAtlasAssetTextureId(const std::string &, bool) { return 0; }
void getRuntimeAtlasAssetUV(const std::string &, ImVec2 &, ImVec2 &, bool) {}
ImTextureID getMapStandaloneTextureId(const std::filesystem::path &) { return 0; }

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

std::vector<ImDrawVert> vertices(const MapModel &map, const MapRenderPlan &plan)
{
    const MapModel source = filterMapForRenderPlan(map, plan);
    ImDrawList drawList(ImGui::GetDrawListSharedData());
    drawList._ResetForNewFrame();
    drawList.PushTexture(ImGui::GetIO().Fonts->TexRef);
    drawList.PushClipRect(ImVec2(0, 0), ImVec2(640, 480));
    // A reviewed background isolates vector overlays without substituting a
    // fake feature renderer or painting thousands of terrain vertices.
    renderMap(&drawList, ImVec2(20, 20), source, plan, 20.0f, true);
    if (drawList.VtxBuffer.empty())
        return {};
    return {drawList.VtxBuffer.begin(), drawList.VtxBuffer.end()};
}

std::vector<ImDrawVert> outputPreviewVertices(
    const MapModel &map, const MapPngExportOptions &options, float cellSize = 8.0f,
    bool backgroundDrawn = false)
{
    const auto plan = compileMapRenderPlan(map, mapPngExportRenderPlanRequest(options));
    const MapModel source = filterMapForRenderPlan(map, plan);
    ImDrawList drawList(ImGui::GetDrawListSharedData());
    drawList._ResetForNewFrame();
    drawList.PushTexture(ImGui::GetIO().Fonts->TexRef);
    drawList.PushClipRect(ImVec2(0, 0), ImVec2(640, 480));
    gm::mapui::drawMapCreatorOutputPreview(&drawList, ImVec2(20, 20), source, plan, cellSize, backgroundDrawn);
    if (drawList.VtxBuffer.empty())
        return {};
    return {drawList.VtxBuffer.begin(), drawList.VtxBuffer.end()};
}

bool traceFillCovers(const MapModel &map, const MapRenderPlan &plan, float mapX, float mapY)
{
    const MapModel source = filterMapForRenderPlan(map, plan);
    ImDrawList drawList(ImGui::GetDrawListSharedData());
    drawList._ResetForNewFrame();
    drawList.PushTexture(ImGui::GetIO().Fonts->TexRef);
    drawList.PushClipRect(ImVec2(0, 0), ImVec2(640, 480));
    renderMap(&drawList, ImVec2(20, 20), source, plan, 20.0f, true);
    const ImVec2 point(20 + mapX * 20, 20 + mapY * 20);
    const auto side = [](ImVec2 a, ImVec2 b, ImVec2 p) {
        return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    };
    constexpr ImU32 fill = IM_COL32(222, 174, 76, 28);
    for (int index = 0; index + 2 < drawList.IdxBuffer.Size; index += 3)
    {
        const auto &a = drawList.VtxBuffer[drawList.IdxBuffer[index]];
        const auto &b = drawList.VtxBuffer[drawList.IdxBuffer[index + 1]];
        const auto &c = drawList.VtxBuffer[drawList.IdxBuffer[index + 2]];
        if (a.col != fill || b.col != fill || c.col != fill)
            continue;
        const float ab = side(a.pos, b.pos, point), bc = side(b.pos, c.pos, point), ca = side(c.pos, a.pos, point);
        if ((ab >= 0 && bc >= 0 && ca >= 0) || (ab <= 0 && bc <= 0 && ca <= 0))
            return true;
    }
    return false;
}
}

// Composite the actual ImGui triangle mesh at a subpixel sample. Tessellated
// surface interiors must stay opaque even along a long shared diagonal.
float inkOpacityAt(const ImDrawList &drawList, ImVec2 p)
{
    float alpha=0.f;
    const auto cross=[](ImVec2 a,ImVec2 b,ImVec2 c) {
        return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    };
    for(int i=0;i+2<drawList.IdxBuffer.Size;i+=3) {
        const auto &a=drawList.VtxBuffer[drawList.IdxBuffer[i]];
        const auto &b=drawList.VtxBuffer[drawList.IdxBuffer[i+1]];
        const auto &c=drawList.VtxBuffer[drawList.IdxBuffer[i+2]];
        const float area=cross(a.pos,b.pos,c.pos);
        if(std::abs(area)<.00001f)continue;
        const float wa=cross(b.pos,c.pos,p)/area,wb=cross(c.pos,a.pos,p)/area,wc=1.f-wa-wb;
        if(wa<0.f || wb<0.f || wc<0.f)continue;
        const float coverage=(wa*((a.col>>IM_COL32_A_SHIFT)&255)+wb*((b.col>>IM_COL32_A_SHIFT)&255)+wc*((c.col>>IM_COL32_A_SHIFT)&255))/255.f;
        alpha=coverage+alpha*(1.f-coverage);
    }
    return alpha;
}

bool atlasInkHasNoInternalFringes()
{
    bool ok=true;
    for(float scale : {2.f,4.f,16.f,42.f}) {
        ImDrawList drawList(ImGui::GetDrawListSharedData());
        drawList._ResetForNewFrame();
        drawList.PushTexture(ImGui::GetIO().Fonts->TexRef);
        drawList.PushClipRect({0,0},{2000,2000});
        drawList.Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;
        const auto flags=drawList.Flags;
        const std::vector<gm::map_render::AtlasInkPrimitive> ink{
            {{{0,0},{30,0},{30,3}},{139,135,111,255},.01f,true},
            {{{0,0},{30,3},{0,3}},{139,135,111,255},.01f,true}};
        drawMapIllustratedAtlasInk(&drawList,{20,20},scale,ink);
        for(int x=2;x<28;++x) for(float offset : {-.13f,.13f})
            ok &= require(inkOpacityAt(drawList,{20+x*scale,20+x*.1f*scale+offset})>.999f,
                          "Atlas tessellation exposes a transparent interior seam");
        ok &= require(drawList.Flags==flags,"Atlas fill changed caller's AA flags");
        const int fillEnd=drawList.VtxBuffer.Size;
        // Inspect geometric stroke AA; the texture-backed line path stores
        // fringe opacity in the font atlas rather than vertex alpha.
        drawList.Flags &= ~ImDrawListFlags_AntiAliasedLinesUseTex;
        drawMapIllustratedAtlasInk(&drawList,{20,20},scale,
            {{{{0,0},{30,3}},{80,90,95,255},.30f,false}});
        bool fringe=false;
        for(int i=fillEnd;i<drawList.VtxBuffer.Size;++i)
            fringe |= ((drawList.VtxBuffer[i].col>>IM_COL32_A_SHIFT)&255)==0;
        ok &= require(fringe,"Atlas silhouette strokes lost antialiasing");
    }
    return ok;
}

bool dungeonVoidIsFlat()
{
    MapModel map;
    map.width=12;map.height=9;map.scope=MapScope::Dungeon;
    map.creationIntent=MapCreationIntent::Dungeon;
    map.generation.recipe.generatorVersion=11;
    map.generation.continuousRendering=true;
    resizeMapCells(map);
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x)
        mapCellAt(map,x,y)->kind=(x+y)%2 ? MapTileKind::Wall : MapTileKind::Empty;
    ImDrawList drawList(ImGui::GetDrawListSharedData());
    drawList._ResetForNewFrame();
    drawList.PushTexture(ImGui::GetIO().Fonts->TexRef);
    drawList.PushClipRect({0,0},{640,480});
    drawMapContinuousTerrainBase(&drawList,{20,20},20.f,map);
    const ImU32 flat=IM_COL32(29,31,33,255),underlay=IM_COL32(74,96,53,255);
    bool sawVoid=false;
    for(const auto &v:drawList.VtxBuffer) {
        sawVoid |= v.col==flat;
        if(v.col!=flat && v.col!=underlay)
            return require(false,"Dungeon void contains varied cell shading or legacy decoration");
    }
    return require(sawVoid,"Dungeon void background was not drawn");
}

bool overviewRiverHasNoSegmentFringes()
{
    bool ok=true;
    for(float scale:{8.f,16.f,32.f,64.f,128.f})
        ok &= require(scale/mapAtlasCoastSubdivisions(scale)<=4.f,
            "Zoomed coast must not interpolate its sharp edge across large visible triangles");
    ok &= require(mapAtlasCoastSubdivisions(1024.f)==32,"Coast tessellation must remain bounded at extreme zoom");
    MapModel map;map.width=18;map.height=12;map.atlasLevel=MapLevel::World;
    map.generation.continuousRendering=true;resizeMapCells(map);
    map.generation.recipe.styleId="illustrated_atlas";map.generation.recipe.styleVersion=5;
    map.generation.recipe.styleSnapshotId="illustrated_atlas_v5";map.generation.recipe.generatorVersion=11;
    MapGeneratedFeature river;river.id="native_continuous_river";river.kind=MapGeneratedFeatureKind::River;
    river.points={{2,3},{4,3},{6,7},{10,8},{14,8}};river.width=2.f;
    map.generation.graph.features={river};
    MapRenderPlanRequest request;request.layers=MapRenderPlanWater;request.applyFog=false;
    const auto plan=compileMapRenderPlan(map,request);
    const auto points=gm::map_render::illustratedAtlasFinalRiverPoints(map,river);
    const float radius=river.width*gm::map_render::atlasPresentationProfile(map).riverWidthCells*.5f;
    for(float scale:{16.f,48.f,128.f})
    {
        ImDrawList drawList(ImGui::GetDrawListSharedData());drawList._ResetForNewFrame();
        drawList.PushTexture(ImGui::GetIO().Fonts->TexRef);drawList.PushClipRect({0,0},{4000,3000});
        drawList.Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;
        renderMap(&drawList,{20,20},map,plan,scale,true);
        for(std::size_t i=2;i+2<points.size();++i)
        {
            const auto a=points[i-1],b=points[i];
            const float dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);
            if(len<.002f)continue;
            for(float t:{.02f,.5f,.98f})for(float side:{-.55f,0.f,.55f})
            {
                // Headwaters and inland sinks deliberately taper. Keep
                // checking their centre for cracks, without requiring the
                // old full-width bank to exist outside the new silhouette.
                const MapFeaturePoint centre{a.x+dx*t,a.y+dy*t};
                const bool endReach=std::hypot(centre.x-points.front().x,centre.y-points.front().y)<1.f ||
                    std::hypot(centre.x-points.back().x,centre.y-points.back().y)<1.f;
                if(endReach && side!=0.f)continue;
                const float x=a.x+dx*t-dy/len*radius*side,y=a.y+dy*t+dx/len*radius*side;
                ok &= require(inkOpacityAt(drawList,{20+x*scale,20+y*scale})>.995f,
                    "Native river renderer exposes an antialiased crosswise segment fringe");
            }
        }
    }
    return ok;
}

bool dungeonPerimeterPreservesOpenings()
{
    MapModel map;
    map.width=8;map.height=5;map.scope=MapScope::Dungeon;
    map.creationIntent=MapCreationIntent::Dungeon;
    map.generation.recipe.generatorVersion=11;
    map.generation.continuousRendering=true;
    resizeMapCells(map);
    for(auto &cell:map.cells) cell.kind=MapTileKind::Wall;
    for(int x=2;x<=5;++x) mapCellAt(map,x,2)->kind=x==4 ? MapTileKind::Door : MapTileKind::Room;
    ImDrawList boundary(ImGui::GetDrawListSharedData());
    boundary._ResetForNewFrame();
    boundary.PushTexture(ImGui::GetIO().Fonts->TexRef);
    boundary.PushClipRect({0,0},{640,480});
    // Use geometric AA so headless opacity sampling sees the line fringe.
    boundary.Flags &= ~ImDrawListFlags_AntiAliasedLinesUseTex;
    drawMapDungeonMasonryBoundary(&boundary,{20,20},20.f,map);
    bool ok=require(inkOpacityAt(boundary,{70,60.13f})>.99f &&
                    inkOpacityAt(boundary,{60.13f,70})>.99f,
                    "Dungeon exposed floor perimeter has no wall");
    ok &= require(inkOpacityAt(boundary,{80.13f,70})==0.f &&
                  inkOpacityAt(boundary,{100.13f,70})==0.f &&
                  inkOpacityAt(boundary,{120.13f,70})==0.f,
                  "Dungeon wall blocks shared floor or an open doorway");
    ImDrawList composed(ImGui::GetDrawListSharedData());
    composed._ResetForNewFrame();
    composed.PushTexture(ImGui::GetIO().Fonts->TexRef);
    composed.PushClipRect({0,0},{640,480});
    drawMapContinuousTerrainBase(&composed,{20,20},20.f,map);
    bool hasWall=false;
    for(const auto &v:composed.VtxBuffer) hasWall |= v.col==IM_COL32(130,118,94,255);
    ok &= require(hasWall,"Native terrain dispatch omitted dungeon masonry perimeter");
    map.generation.recipe.generatorVersion=10;
    const int oldSize=boundary.VtxBuffer.Size;
    drawMapDungeonMasonryBoundary(&boundary,{20,20},20.f,map);
    ok &= require(boundary.VtxBuffer.Size==oldSize,"New masonry pass changed a legacy dungeon");
    return ok;
}

int main(int argc,char **argv)
{
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640, 480);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char *fontPixels = nullptr;
    int fontWidth = 0, fontHeight = 0;
    io.Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
    ImGui::NewFrame();

    if(argc>1 && std::string(argv[1])=="--benchmark-local") {
        MapModel dense;dense.width=80;dense.height=80;dense.scope=MapScope::Local;dense.atlasLevel=MapLevel::Tactical;
        dense.generation.enabled=true;dense.generation.continuousRendering=true;
        dense.generation.recipe.generatorVersion=12;dense.generation.recipe.styleId="illustrated_atlas";
        dense.generation.recipe.styleVersion=6;dense.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        dense.generation.recipe.recipeVersion=argc>2?std::stoi(argv[2]):9;
        resizeMapCells(dense);for(auto &cell:dense.cells){cell.kind=MapTileKind::Forest;cell.fogged=false;}
        MapRenderPlanRequest request;request.applyFog=false;
        const auto begin=std::chrono::steady_clock::now();const auto plan=compileMapRenderPlan(dense,request);
        const double planMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        for(float scale:{2.5f,8.f,16.f}) {
            ImDrawList draw(ImGui::GetDrawListSharedData());std::vector<double> times;
            for(int frame=0;frame<4;++frame){draw._ResetForNewFrame();draw.PushTexture(io.Fonts->TexRef);draw.PushClipRect({0,0},{1600,1600});
                const auto start=std::chrono::steady_clock::now();renderMap(&draw,{0,0},dense,plan,scale,false);
                if(frame)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
            std::sort(times.begin(),times.end());
            std::cout<<"{\"fixture\":\"80x80 all-forest\",\"recipe_version\":"<<dense.generation.recipe.recipeVersion<<",\"plan_ms\":"<<planMs<<",\"cell_pixels\":"<<scale
                <<",\"median_native_cpu_ms\":"<<times[1]<<",\"ink_primitives\":"<<plan.atlasInk.size()<<",\"vertices\":"<<draw.VtxBuffer.Size<<"}\n";
        }
        ImGui::EndFrame();ImGui::DestroyContext();return 0;
    }

    MapModel map;
    map.metadata.id = "headless_geometry_consent";
    map.width = 24;
    map.height = 16;
    map.atlasLevel = MapLevel::World;
    map.visibility = MapVisibility::PlayersVisible;
    map.generation.continuousRendering = true;
    map.generation.recipe.styleVersion = 0;
    resizeMapCells(map);
    for (auto &cell : map.cells)
    {
        cell.kind = MapTileKind::Dirt;
        cell.fogged = false;
    }
    MapGeneratedFeature river;
    river.id = "public_river";
    river.kind = MapGeneratedFeatureKind::River;
    river.points = {{3.0f, 5.0f}, {8.0f, 6.0f}, {15.0f, 9.0f}};
    river.discovered = true;
    river.gmOnly = false;
    map.generation.graph.features = {river};
    MapRenderPlanRequest request;
    request.audience = MapRenderAudience::Player;
    request.locale = "en";
    const auto riverPlan = compileMapRenderPlan(map, request);
    bool ok = require((riverPlan.layers & MapRenderPlanWater) != 0 &&
                          (riverPlan.layers & MapRenderPlanWays) == 0 &&
                          mapRenderPlanContainsGeneratedGeometry(riverPlan, river.id) &&
                          !vertices(map, riverPlan).empty(),
                      "Base Water must draw real river vertices when Ways and Political are both withheld");
    map.generation.graph.features.front().gmOnly = true;
    ok &= require(vertices(map, compileMapRenderPlan(map, request)).empty(),
                  "A GM-only river must remain absent from the Water pass");
    map.generation.graph.features.front().gmOnly = false;
    map.generation.graph.features.front().discovered = false;
    ok &= require(vertices(map, compileMapRenderPlan(map, request)).empty(),
                  "An undiscovered river must remain absent from the Water pass");

    MapGeneratedFeature road = river;
    road.id = "named_road";
    road.kind = MapGeneratedFeatureKind::Road;
    road.name = "Released road name";
    map.generation.graph.features = {road};
    map.playerRelease.labels = true;
    map.playerRelease.politicalBorders = true;
    request.showPoliticalOverlay = true;
    const auto labelPlan = compileMapRenderPlan(map, request);
    const bool releasedLabel = std::any_of(labelPlan.primitives.begin(), labelPlan.primitives.end(), [&](const auto &primitive) {
        return primitive.kind == MapRenderPrimitiveKind::Label && primitive.sourceId == road.id && primitive.label == road.name;
    });
    ok &= require(releasedLabel && mapRenderPlanContainsGeneratedFeature(labelPlan, road.id) &&
                          !mapRenderPlanContainsGeneratedGeometry(labelPlan, road.id) &&
                          vertices(map, labelPlan).empty(),
                      "A released road label must not acquire line geometry from the Water or Political render pass");
    map.playerRelease.waysAndRoutes = true;
    const auto roadPlan = compileMapRenderPlan(map, request);
    ok &= require(mapRenderPlanContainsGeneratedGeometry(roadPlan, road.id) && !vertices(map, roadPlan).empty(),
                  "The same road must render real line vertices when Ways is deliberately released");

    MapGeneratedFeature bridge = road;
    bridge.id = "named_bridge";
    bridge.kind = MapGeneratedFeatureKind::Bridge;
    bridge.name = "Released bridge name";
    bridge.points = {{9, 6}, {10, 7}};
    map.generation.graph.features = {bridge};
    map.playerRelease.waysAndRoutes = false;
    const auto bridgeLabelPlan = compileMapRenderPlan(map, request);
    ok &= require(mapRenderPlanContainsGeneratedFeature(bridgeLabelPlan, bridge.id) &&
                          std::any_of(bridgeLabelPlan.primitives.begin(), bridgeLabelPlan.primitives.end(), [&](const auto &primitive) {
                              return primitive.kind == MapRenderPrimitiveKind::Label && primitive.sourceId == bridge.id;
                          }) &&
                          !mapRenderPlanContainsGeneratedGeometry(bridgeLabelPlan, bridge.id) &&
                          vertices(map, bridgeLabelPlan).empty(),
                      "A released bridge label must not draw its unreleased deck or planks through another content pass");
    map.playerRelease.waysAndRoutes = true;
    const auto bridgePlan = compileMapRenderPlan(map, request);
    ok &= require(mapRenderPlanContainsGeneratedGeometry(bridgePlan, bridge.id) && !vertices(map, bridgePlan).empty(),
                  "The same bridge must render deck/plank vertices when Ways is released");

    // District labels use the feature pass too, but their tint requires the
    // Symbols primitive. Narrowing geometry must retain the allowed text.
    map.atlasLevel = MapLevel::Site;
    map.playerRelease.waysAndRoutes = false;
    MapGeneratedFeature district = road;
    district.id = "district";
    district.kind = MapGeneratedFeatureKind::District;
    district.name = "Market";
    district.closed = true;
    district.points = {{3, 3}, {10, 3}, {10, 10}, {3, 10}};
    map.generation.graph.features = {district};
    const auto districtPlan = compileMapRenderPlan(map, request);
    const auto districtVertices = vertices(map, districtPlan);
    const ImU32 districtTint = IM_COL32(186, 151, 94, 10);
    ok &= require(!districtVertices.empty() && !mapRenderPlanContainsGeneratedGeometry(districtPlan, district.id) &&
                          std::none_of(districtVertices.begin(), districtVertices.end(), [&](const auto &vertex) { return vertex.col == districtTint; }),
                      "A labels-only district must retain text vertices without its unreleased polygon tint");
    MapGeneratedFeature trace;
    trace.id = "concave_trace";
    trace.kind = MapGeneratedFeatureKind::TerrainRegion;
    trace.materialId = "trace.terrain_area";
    trace.closed = true;
    trace.discovered = true;
    trace.gmOnly = false;
    trace.points = {{1, 1}, {10, 1}, {10, 8}, {7, 8}, {7, 4}, {4, 4}, {4, 8}, {1, 8}};
    map.generation.graph.features = {trace};
    MapRenderPlanRequest traceRequest;
    traceRequest.layers = MapRenderPlanTerrain;
    auto tracePlan = compileMapRenderPlan(map, traceRequest);
    ok &= require(traceFillCovers(map, tracePlan, 2.5f, 6.5f) &&
                      !traceFillCovers(map, tracePlan, 5.5f, 6.5f),
                  "A concave terrain trace must fill its interior without triangles across the notch");
    std::reverse(map.generation.graph.features.front().points.begin(), map.generation.graph.features.front().points.end());
    // Explicitly closing the authored ring must not introduce a degenerate ear.
    map.generation.graph.features.front().points.push_back(map.generation.graph.features.front().points.front());
    tracePlan = compileMapRenderPlan(map, traceRequest);
    ok &= require(traceFillCovers(map, tracePlan, 2.5f, 6.5f) &&
                      !traceFillCovers(map, tracePlan, 5.5f, 6.5f),
                  "Reverse-wound/explicitly closed terrain traces must retain the same concave fill");
    traceRequest.layers = MapRenderPlanWater | MapRenderPlanLabels;
    map.generation.graph.features.front().name = "Trace name only";
    tracePlan = compileMapRenderPlan(map, traceRequest);
    ok &= require(!mapRenderPlanContainsGeneratedGeometry(tracePlan, trace.id) &&
                      !traceFillCovers(map, tracePlan, 2.5f, 6.5f),
                  "A trace retained for its label must not draw a disabled Terrain fill");
    traceRequest.layers = MapRenderPlanTerrain;
    traceRequest.audience = MapRenderAudience::Player;
    map.generation.graph.features.front().gmOnly = true;
    ok &= require(!traceFillCovers(map, compileMapRenderPlan(map, traceRequest), 2.5f, 6.5f),
                  "A GM-only traced terrain area must not reach player geometry");

    MapModel legacy;
    legacy.width = 2;
    legacy.height = 1;
    legacy.visibility = MapVisibility::PlayersVisible;
    legacy.generation.continuousRendering = false;
    resizeMapCells(legacy);
    legacy.cells[0].kind = MapTileKind::Dirt;
    legacy.cells[1].kind = MapTileKind::Water;
    legacy.cells[0].fogged = false;
    legacy.cells[1].fogged = true;
    MapPngExportOptions outputOptions;
    outputOptions.applyFog = false;
    outputOptions.showLabels = false;
    ok &= require(!outputPreviewVertices(legacy, outputOptions).empty(),
                  "A supported legacy raster map must produce real cell vertices in Export preview even with no graph");
    ok &= require(outputPreviewVertices(legacy, outputOptions, 8.0f, true).empty(),
                  "Legacy raster fallback must not paint over a reviewed authoritative image background");
    outputOptions.applyFog = true;
    outputOptions.showGrid = true;
    const auto fogAndGrid = outputPreviewVertices(legacy, outputOptions);
    constexpr ImU32 fogColor = IM_COL32(24, 28, 32, 255);
    {
        MapModel covered;covered.width=80;covered.height=80;resizeMapCells(covered);
        for(auto &cell:covered.cells)cell.fogged=true;
        ImDrawList cloud(ImGui::GetDrawListSharedData());
        const auto begin=std::chrono::steady_clock::now();
        for(int frame=0;frame<120;++frame)
        {
            cloud._ResetForNewFrame();cloud.PushTexture(ImGui::GetIO().Fonts->TexRef);cloud.PushClipRect({0,0},{4000,4000});
            drawOpaqueMapFog(&cloud,static_cast<ImTextureID>(123),{0,0},20,covered,0,0,79,79);
        }
        ok &= require(cloud.VtxBuffer.Size==80*8,"Solid cloud fog should batch rows, not add geometry per texel or cell");
        ok &= require(cloud.CmdBuffer.Size<=3,"Cloud fog must batch texture draws instead of switching textures every row");
        ok &= require(std::all_of(cloud.VtxBuffer.begin(),cloud.VtxBuffer.end(),[](const auto &v){return (v.col>>24)==255;}),
            "Player cloud mesh must be opaque, including its fallback layer");
        std::cout<<"80x80 cloud fog: 120 complete draw-list updates in "
            <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()<<" ms\n";
        const auto &texture=gm::map_render::fogTexturePixels(false);
        auto low=texture[0][0],high=low;
        for(const auto &p:texture){low=std::min(low,p[0]);high=std::max(high,p[0]);
            ok &= require(p[3]==255,"A fog texel would reveal concealed content");}
        ok &= require(high-low>25,"Cloud texture lost its visible tonal variation");
        ok &= require(gm::map_render::fogTextureAt(.25f,1.5f)==gm::map_render::fogTextureAt(24.25f,25.5f),
            "Cloud texture should repeat seamlessly at map-anchored coordinates");
    }
    constexpr ImU32 gridColor = IM_COL32(48, 54, 48, 120);
    int lastFog = -1, lastGrid = -1;
    for (int index = 0; index < static_cast<int>(fogAndGrid.size()); ++index)
    {
        if (fogAndGrid[index].col == fogColor) lastFog = index;
        if (fogAndGrid[index].col == gridColor) lastGrid = index;
    }
    ok &= require(lastFog >= 0 && lastGrid > lastFog,
                  "Export preview must draw selected grid vertices after opaque fog, including below the editor's 10px grid threshold");
    outputOptions.applyFog = false;
    const auto gmNoFog = outputPreviewVertices(legacy, outputOptions);
    ok &= require(std::none_of(gmNoFog.begin(), gmNoFog.end(), [&](const auto &vertex) { return vertex.col == fogColor; }),
                  "Turning GM Apply fog off must remove the actual preview mask");
    outputOptions.playerSafe = true;
    const auto playerForcedFog = outputPreviewVertices(legacy, outputOptions);
    ok &= require(std::any_of(playerForcedFog.begin(), playerForcedFog.end(), [&](const auto &vertex) { return vertex.col == fogColor; }),
                  "Player Export preview must still draw the actual opaque mask when applyFog=false is requested");
    {
    MapModel labels;
    labels.width = 24; labels.height = 16; labels.atlasLevel = MapLevel::World;
    labels.generation.continuousRendering = true;
    resizeMapCells(labels);
    for (auto &cell : labels.cells) cell.kind = MapTileKind::Dirt;
    MapGeneratedFeature nation;
    nation.id = "nation-a"; nation.name = "Orun";
    nation.kind = MapGeneratedFeatureKind::TerrainRegion;
    nation.materialId = "territory.orun"; nation.closed = true;
    nation.points = {{5, 3}, {17, 3}, {17, 13}, {5, 13}};
    auto overlappingNation = nation;
    overlappingNation.id = "nation-b"; overlappingNation.name = "Skeld";
    MapGeneratedFeature town;
    town.id = "town"; town.name = "Havenfall";
    town.kind = MapGeneratedFeatureKind::Settlement;
    town.points = {{11, 8}};
    labels.generation.graph.features = {nation, overlappingNation, town};
    ImDrawList labelDrawList(ImGui::GetDrawListSharedData());
    labelDrawList._ResetForNewFrame();
    labelDrawList.PushTexture(ImGui::GetIO().Fonts->TexRef);
    labelDrawList.PushClipRect(ImVec2(0, 0), ImVec2(640, 480));
    std::vector<ImVec4> occupiedLabels;
    drawMapContinuousFeatureGraph(&labelDrawList, ImVec2(20, 20), 20, labels,
                                  false, {}, false, true, "en", {}, nullptr, &occupiedLabels);
    ok &= require(occupiedLabels.size() == 1,
                  "Coincident nation labels must not be painted over one another");
    drawMapContinuousSymbols(&labelDrawList, ImVec2(20, 20), 20, labels,
                             false, false, true, {}, "en", &occupiedLabels);
    ok &= require(occupiedLabels.size() == 2,
                  "A nearby settlement must choose a readable position outside the nation label");
    for (std::size_t i = 0; i < occupiedLabels.size(); ++i)
        for (std::size_t j = i + 1; j < occupiedLabels.size(); ++j)
        {
            const auto a = occupiedLabels[i], b = occupiedLabels[j];
            ok &= require(!(a.x < b.z && a.z > b.x && a.y < b.w && a.w > b.y),
                          "Nation and settlement labels overlap across rendering passes");
        }
    } // ImDrawList must release its shared data before destroying the context.
    // Diagonal river raster helpers are Water/None. Player sanitization must
    // recover the same atlas ground before it strips river provenance, rather
    // than inventing tiny lakes beside the smooth public river.
    for (const auto level : {MapLevel::World, MapLevel::Region})
    {
        MapModel coast;
        coast.width = 40; coast.height = 30; coast.atlasLevel = level;
        coast.visibility = MapVisibility::PlayersVisible;
        coast.generation.continuousRendering = true;
        coast.generation.recipe.generatorVersion = 11;
        coast.generation.recipe.styleId = "illustrated_atlas";
        coast.generation.recipe.styleSnapshotId = "illustrated_atlas_v5";
        coast.generation.recipe.styleVersion = 5;
        resizeMapCells(coast);
        for (int y = 0; y < coast.height; ++y)
            for (int x = 0; x < coast.width; ++x)
            {
                auto &cell = *mapCellAt(coast, x, y);
                cell.kind = (x == 0 || y == 0 || x == coast.width - 1 || y == coast.height - 1)
                    ? MapTileKind::Water : MapTileKind::Dirt;
                cell.fogged = false;
            }
        for (const auto point : {MapFeaturePoint{5, 5}, MapFeaturePoint{6, 6}, MapFeaturePoint{7, 7}})
        {
            auto &cell = *mapCellAt(coast, static_cast<int>(point.x), static_cast<int>(point.y));
            cell.kind = MapTileKind::Water; cell.wayKind = MapWayKind::River;
        }
        for (const auto point : {MapFeaturePoint{6, 5}, MapFeaturePoint{5, 6}, MapFeaturePoint{7, 6}, MapFeaturePoint{6, 7}})
            mapCellAt(coast, static_cast<int>(point.x), static_cast<int>(point.y))->kind = MapTileKind::Water;
        for (int y = 18; y <= 21; ++y)
            for (int x = 23; x <= 27; ++x)
                mapCellAt(coast, x, y)->kind = MapTileKind::Water;
        MapGeneratedFeature stream;
        stream.id = "reviewed-river"; stream.kind = MapGeneratedFeatureKind::River;
        stream.points = {{5.5f, 5.5f}, {6.5f, 6.5f}, {7.5f, 7.5f}};
        stream.gmOnly = false; stream.discovered = true;
        coast.generation.graph.features.push_back(stream);
        MapRenderPlanRequest gmRequest; gmRequest.audience = MapRenderAudience::GameMaster;
        MapRenderPlanRequest playerRequest; playerRequest.audience = MapRenderAudience::Player;
        const auto gmPlan = compileMapRenderPlan(coast, gmRequest);
        const auto playerPlan = compileMapRenderPlan(coast, playerRequest);
        const auto sanitized = filterMapForRenderPlan(coast, playerPlan);
        for (int y = 0; y < coast.height; ++y)
            for (int x = 0; x < coast.width; ++x)
                ok &= require(gm::map_render::overviewGroundKindIsLand(gm::map_render::overviewGroundKindForRender(coast, x, y)) ==
                              gm::map_render::overviewGroundKindIsLand(gm::map_render::overviewGroundKindForRender(sanitized, x, y)),
                              "Player terrain invented water/land while removing diagonal river raster projections");
        ok &= require(mapCellAt(sanitized, 25, 20)->kind == MapTileKind::Water &&
                      mapCellAt(sanitized, 0, 12)->kind == MapTileKind::Water,
                      "Sanitization must retain actual lake and ocean water");
        ok &= require(gmPlan.atlasWaterInk.size() == playerPlan.atlasWaterInk.size(),
                      "GM/player coast ink should have the same topology for identical revealed terrain");
        if (gmPlan.atlasWaterInk.size() == playerPlan.atlasWaterInk.size())
            for (std::size_t i = 0; i < gmPlan.atlasWaterInk.size(); ++i)
            {
                const auto &a = gmPlan.atlasWaterInk[i].points;
                const auto &b = playerPlan.atlasWaterInk[i].points;
                ok &= require(a.size() == b.size(), "GM/player shoreline ink point count differs");
                if (a.size() == b.size())
                    for (std::size_t j = 0; j < a.size(); ++j)
                        ok &= require(a[j].x == b[j].x && a[j].y == b[j].y,
                                      "GM/player shoreline ink geometry differs");
            }
        coast.generation.graph.features[0].gmOnly = true;
        const auto privatePlan = compileMapRenderPlan(coast, playerRequest);
        const auto privateSafe = filterMapForRenderPlan(coast, privatePlan);
        ok &= require(!mapRenderPlanContainsGeneratedGeometry(privatePlan, stream.id) && privateSafe.generation.graph.features.empty(),
                      "Ground recovery must not restore a private river primitive");
        for (const auto &cell : privateSafe.cells)
            ok &= require(cell.wayKind == MapWayKind::None, "Ground recovery leaked a raster river flag");
        mapCellAt(coast, 6, 5)->fogged = true;
        const auto fogPlan = compileMapRenderPlan(coast, playerRequest);
        ok &= require(fogPlan.applyFog && mapCellAt(filterMapForRenderPlan(coast, fogPlan), 6, 5)->fogged,
                      "Ground recovery must preserve mandatory player fog");
    }
    for (const auto level : {MapLevel::World, MapLevel::Region})
    {
        MapModel relief;
        relief.width = 30; relief.height = 20; relief.atlasLevel = level;
        relief.visibility = MapVisibility::PlayersVisible;
        relief.generation.continuousRendering = true;
        relief.generation.recipe.generatorVersion = 12;
        relief.generation.recipe.styleId = "illustrated_atlas";
        relief.generation.recipe.styleVersion = 5;
        relief.generation.recipe.styleSnapshotId = "illustrated_atlas_v5";
        resizeMapCells(relief);
        for (auto &cell : relief.cells) { cell.kind = MapTileKind::Dirt; cell.fogged = false; }
        for (int y = 3; y <= 7; ++y) for (int x = 3; x <= 12; ++x)
            mapCellAt(relief, x, y)->kind = MapTileKind::Mountain;
        MapGeneratedFeature hidden;
        hidden.id = "private-range"; hidden.kind = MapGeneratedFeatureKind::MountainRange;
        hidden.points = {{4, 5}, {11, 5}}; hidden.gmOnly = true;
        relief.generation.graph.features.push_back(hidden);
        MapRenderPlanRequest player; player.audience = MapRenderAudience::Player;
        const auto filtered = filterMapForRenderPlan(relief, compileMapRenderPlan(relief, player));
        ok &= require(gm::map_render::buildOverviewRidgePaths(filtered).empty(),
                      "Hidden generated mountains must not reappear as cell-derived painted relief");
        mapCellAt(relief, 23, 14)->kind = MapTileKind::Mountain;
        const auto painted = filterMapForRenderPlan(relief, compileMapRenderPlan(relief, player));
        ok &= require(!gm::map_render::buildOverviewRidgePaths(painted).empty() &&
                      mapCellAt(painted, 23, 14)->kind == MapTileKind::Mountain &&
                      mapCellAt(relief, 5, 5)->kind == MapTileKind::Mountain,
                      "Filtering private relief must preserve separate painted mountains and source data");
        relief.generation.graph.features.front().gmOnly = false;
        relief.generation.graph.features.front().discovered = true;
        const auto reviewed = filterMapForRenderPlan(relief, compileMapRenderPlan(relief, player));
        ok &= require(mapCellAt(reviewed, 5, 5)->kind == MapTileKind::Mountain,
                      "Reviewed mountain terrain must survive player filtering");
    }
    {
        MapModel crossing;
        crossing.width=20; crossing.height=16; resizeMapCells(crossing);
        crossing.visibility=MapVisibility::PlayersVisible;
        crossing.playerRelease.waysAndRoutes=true;
        crossing.atlasLevel=MapLevel::World; crossing.generation.continuousRendering=true;
        crossing.generation.recipe.generatorVersion=12;
        crossing.generation.recipe.styleId="illustrated_atlas";
        crossing.generation.recipe.styleVersion=5;
        crossing.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
        for(auto &cell:crossing.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        MapGeneratedFeature road;road.id="public-road";road.kind=MapGeneratedFeatureKind::Road;
        road.points={{2,8},{18,8}};
        MapGeneratedFeature river;river.id="public-river";river.kind=MapGeneratedFeatureKind::River;
        river.points={{10,1},{10,15}};
        crossing.generation.graph.features={road,river};
        MapRenderPlanRequest player;player.audience=MapRenderAudience::Player;
        auto reviewed=filterMapForRenderPlan(crossing,compileMapRenderPlan(crossing,player));
        ok &= require(gm::map_render::illustratedAtlasCrossingBridges(reviewed).size()==1,
                      "Public road/river crossing must have a bridge in player geometry");
        for(int hidden:{0,1})
        {
            crossing.generation.graph.features[hidden].gmOnly=true;
            reviewed=filterMapForRenderPlan(crossing,compileMapRenderPlan(crossing,player));
            ok &= require(gm::map_render::illustratedAtlasCrossingBridges(reviewed).empty(),
                          "An inferred bridge must not reveal a private road or river");
            crossing.generation.graph.features[hidden].gmOnly=false;
        }
        auto explicitBridge=gm::map_render::illustratedAtlasCrossingBridges(crossing).front();
        explicitBridge.id="authored-bridge";
        crossing.generation.graph.features.push_back(explicitBridge);
        ok &= require(gm::map_render::illustratedAtlasCrossingBridges(crossing).size()==1,
                      "An existing bridge must not receive duplicate inferred artwork");
    }
    for(const auto material:{MapTileKind::Forest,MapTileKind::Mountain,MapTileKind::Water})
    for(const float brushCellSize:{12.f,36.4f})
    {
        MapModel painted;painted.width=80;painted.height=80;painted.atlasLevel=MapLevel::World;
        painted.generation.enabled=true;painted.generation.continuousRendering=true;
        painted.generation.recipe.generatorVersion=12;painted.generation.recipe.styleId="illustrated_atlas";
        painted.generation.recipe.styleVersion=6;painted.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
        resizeMapCells(painted);
        for(auto &cell:painted.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        for(int y=0;y<80;++y)for(int x=0;x<12;++x)mapCellAt(painted,x,y)->kind=MapTileKind::Water;
        const auto initial=painted;
        MapRenderPlanRequest brushRequest;brushRequest.applyFog=false;
        const auto brushPlan=compileMapRenderPlan(painted,brushRequest);
        MapBrushRenderSnapshot snapshot;snapshot.begin(painted);
        const auto initialKey=mapAtlasTerrainCache(snapshot.source(painted)).key;
        double milliseconds[2]{};
        for(int stable=0;stable<2;++stable)
        {
            painted=initial;
            ImDrawList drawing(ImGui::GetDrawListSharedData());
            for(int frame=-1;frame<6;++frame)
            {
                drawing._ResetForNewFrame();drawing.Flags|=ImDrawListFlags_AllowVtxOffset;
                drawing.PushTexture(io.Fonts->TexRef);drawing.PushClipRect({0,0},{960,720});
                if(frame>=0)applyMapTerrainBrush(painted,mapBrushFootprint(painted,16+frame*3,18+frame,10,true),material,0,false);
                const auto &source=stable ? snapshot.source(painted) : painted;
                const auto began=std::chrono::steady_clock::now();
                renderMap(&drawing,{0,0},source,brushPlan,brushCellSize);
                if(frame>=0)milliseconds[stable]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
                if(stable)ok &= require(mapAtlasTerrainCache(source).key==initialKey,
                    "Painting invalidated native ground/coast cache despite a stable render plan");
            }
        }
        ok &= require(mapCellAt(painted,31,23)->kind==material &&
            mapCellAt(snapshot.source(painted),31,23)->kind==MapTileKind::Dirt,
            "Brush preview must keep live changes separate from the cached GM illustration");
        std::cout<<"80x80 "<<mapTileKindLabel(material)<<" at "<<brushCellSize<<" px/cell, 6 native draw frames: old "
            <<milliseconds[0]<<" ms, snapshot "<<milliseconds[1]<<" ms\n";
        snapshot.clear();
        ok &= require(&snapshot.source(painted)==&painted && mapAtlasTerrainCache(painted).key!=initialKey,
            "Releasing the brush must return native drawing to the edited terrain");
        snapshot.begin(initial);auto other=painted;other.metadata.id="different-map";
        ok &= require(&snapshot.source(other)==&other,"A brush snapshot leaked into another map");
    }
    ok &= atlasInkHasNoInternalFringes();
    ok &= overviewRiverHasNoSegmentFringes();
    ok &= dungeonVoidIsFlat();
    ok &= dungeonPerimeterPreservesOpenings();
    ImGui::EndFrame();
    ImGui::DestroyContext();
    return ok ? 0 : 1;
}
