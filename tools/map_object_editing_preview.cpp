#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapPngExport.hpp"
#include "Models/Map/MapStrokeAuthoring.hpp"
#include "Models/Map/MapFogTexture.hpp"
#include "Models/Map/MapEditConsistency.hpp"
#include "Models/Media/RgbaImageLoader.hpp"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "extern/glfw/deps/stb_image_write.h"
#include "map_crossing_edit_fixture.hpp"
#include <filesystem>
#include <iostream>

static int riverEndingReview(const std::filesystem::path &root)
{
    std::filesystem::create_directories(root);
    constexpr int tile=280,columns=5;
    std::vector<unsigned char> sheet(tile*columns*tile*2*4,255);
    const char *names[]={"ground-source","mountain-source","inland-sink","sea-mouth","lake-mouth",
        "confluence","map-edge","angled-mouth","reverse-drawn-mouth","wide-mouth"};
    for(int example=0;example<10;++example)
    {
        auto map=crossingEditFixture();map.width=20;map.height=16;resizeMapCells(map);
        map.generation.graph.features.clear();map.generation.recipe.seed=91042;
        map.atlasLevel=example%2?MapLevel::Region:MapLevel::World;
        for(auto &cell:map.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;cell.fogged=false;}
        MapGeneratedFeature river;river.id="manual_stroke_ending-review";river.kind=MapGeneratedFeatureKind::River;
        river.width=example==9?4.f:1.6f;river.points={{6,6},{8,7},{11,8}};
        MapPngExportRect crop;crop.x=4;crop.y=4;crop.width=5;crop.height=5;
        if(example==1)applyMapTerrainBrush(map,mapBrushFootprint(map,5,5,2,true),MapTileKind::Mountain,0,false);
        if(example>=2){crop.x=8;crop.y=6;}
        if(example==3 || example>=7)
        {
            for(int y=0;y<map.height;++y)for(int x=10;x<map.width;++x)mapCellAt(map,x,y)->kind=MapTileKind::Water;
            river.points.back()={13,8};
        }
        if(example==4)
            applyMapTerrainBrush(map,mapBrushFootprint(map,12,8,7,true),MapTileKind::Water,0,false);
        if(example==5)
        {
            MapGeneratedFeature receiver=river;receiver.id="receiving-river";receiver.width=3;
            receiver.points={{11,0},{11,8},{13,16}};
            map.generation.graph.features.push_back(receiver);
        }
        if(example==6){river.points={{7,8},{14,8},{20,8}};crop.x=15;}
        if(example==7)river.points={{7,2},{8,5},{10.4f,10},{12,12}};
        if(example==8)std::reverse(river.points.begin(),river.points.end());
        map.generation.graph.features.push_back(river);
        MapRenderPlanRequest request;request.applyFog=false;request.locale="en";
        const auto plan=compileMapRenderPlan(map,request);
        std::string error;const auto path=root/(std::string(names[example])+".png");
        if(!exportMapPngRegion(map,path,plan,crop,56,&error)){std::cerr<<error;return 1;}
        LoadedRgbaImage image;if(!loadRgbaImage(path,image))return 1;
        for(int y=0;y<tile;++y)for(int x=0;x<tile;++x)for(int c=0;c<4;++c)
            sheet[(((example/columns)*tile+y)*tile*columns+(example%columns)*tile+x)*4+c]=image.pixels[(y*tile+x)*4+c];
    }
    if(!stbi_write_png((root/"endings-sheet.png").string().c_str(),tile*columns,tile*2,4,sheet.data(),tile*columns*4))return 1;
    std::cout<<"Reviewed export fixtures: ground, mountain, inland, sea, lake / confluence, edge, angled, reversed, wide.\n";
    return 0;
}

int main(int argc,char **argv)
{
    if(argc==3 && std::string(argv[1])=="--edit-repair-review")
    {
        const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);
        auto map=crossingEditFixture();map.width=30;map.height=20;resizeMapCells(map);
        map.generation.graph.features.clear();
        for(auto &cell:map.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;cell.fogged=false;}
        MapGeneratedFeature river;river.id="manual_stroke_cross-lake";river.kind=MapGeneratedFeatureKind::River;
        river.width=3;river.points={{2,8},{5,8},{8,10},{12,11},{16,10},{20,11},{26,8}};
        MapGeneratedFeature road;road.id="cross-lake-road";road.kind=MapGeneratedFeatureKind::Road;
        road.width=.82f;road.points={{3.5f,5.5f},{16.5f,8.5f},{26.5f,15.5f}};
        MapGeneratedFeature town;town.id="flooded-place";town.kind=MapGeneratedFeatureKind::Settlement;
        town.name="Havenfall";town.points={{16.5f,8.5f}};
        map.generation.graph.features={river,road,town};
        projectGeneratedWayFeatureToCells(map,river,false);projectGeneratedWayFeatureToCells(map,road,false);
        projectGeneratedPointFeatureClassificationToCells(map,town);
        for(int x=4;x<27;++x)for(int y=15;y<17;++y)applyMapCellTerrainDefaults(*mapCellAt(map,x,y),MapTileKind::Mountain);
        for(int x=2;x<13;++x)for(int y=2;y<5;++y)applyMapCellTerrainDefaults(*mapCellAt(map,x,y),MapTileKind::Forest);
        applyMapTerrainBrush(map,mapBrushFootprint(map,16,10,9,true),MapTileKind::Water,0,false);
        mapCellAt(map,24,4)->kind=MapTileKind::Water;
        MapRenderPlanRequest request;request.applyFog=false;request.locale="en";std::string error;
        if(!exportMapPng(map,root/"painted-before-repair.png",compileMapRenderPlan(map,request),36,&error))return 1;
        const auto conflicts=inspectMapEditConflicts(map);
        if(!repairMapEditConflict(map,town.id)||!repairMapEditConflict(map,road.id)){std::cerr<<"Repair failed";return 1;}
        for(const auto &issue:conflicts)if(issue.waterFragment)repairMapEditConflict(map,issue.featureId);
        if(!exportMapPng(map,root/"painted-after-repair.png",compileMapRenderPlan(map,request),36,&error))return 1;
        if(!exportMapPngRegion(map,root/"river-through-lake.png",compileMapRenderPlan(map,request),{9,6,14,10},56,&error))return 1;
        return 0;
    }
    if(argc==3 && std::string(argv[1])=="--mountain-pass-review")
    {
        const std::filesystem::path root=argv[2];std::filesystem::create_directories(root);
        for(int example=0;example<3;++example)
        {
            auto map=crossingEditFixture();map.width=24;map.height=18;resizeMapCells(map);
            map.generation.graph.features.clear();
            for(auto &cell:map.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;cell.fogged=false;}
            for(int x=4;x<20;++x)for(int y=7;y<10;++y)
                applyMapCellTerrainDefaults(*mapCellAt(map,x,y),MapTileKind::Mountain);
            MapRenderPlanRequest request;request.applyFog=false;
            std::string error;
            if(example==0 && !exportMapPng(map,root/"before-road.png",compileMapRenderPlan(map,request),36,&error))return 1;
            const std::vector<MapFeaturePoint> path=example==0?std::vector<MapFeaturePoint>{{12,2},{12,16}}:
                example==1?std::vector<MapFeaturePoint>{{5,2},{9,6},{14,11},{18,16}}:
                           std::vector<MapFeaturePoint>{{3,13},{8,9},{12,8},{16,6},{21,3}};
            appendContinuousMapWayStroke(map,path,example==2?MapWayKind::Trail:MapWayKind::DirtRoad,example==1?2:1);
            const auto plan=compileMapRenderPlan(map,request);
            if(!exportMapPng(map,root/("pass-"+std::to_string(example)+".png"),plan,36,&error)){std::cerr<<error;return 1;}
            if(!exportMapPngRegion(map,root/("detail-"+std::to_string(example)+".png"),plan,{8,5,9,7},64,&error)){std::cerr<<error;return 1;}
        }
        return 0;
    }
    if(argc==3 && std::string(argv[1])=="--river-review")return riverEndingReview(argv[2]);
    if(argc!=2) return 2;
    {
        MapModel shore;shore.width=20;shore.height=12;resizeMapCells(shore);
        shore.atlasLevel=MapLevel::World;shore.generation.recipe.generatorVersion=11;
        for(int x=0;x<shore.width;++x) mapCellAt(shore,x,0)->kind=MapTileKind::Water;
        if(mapGenV5WorldRoadSegmentIsSafe(shore,{3,1.2f},{16,1.2f}) ||
           !mapGenV5WorldRoadSegmentIsSafe(shore,{3,2.1f},{16,2.1f}))
        {std::cerr<<"Road smoothing did not preserve coastal padding\n";return 1;}
        MapGeneratedFeature harbor;harbor.kind=MapGeneratedFeatureKind::Settlement;harbor.points={{3,1.2f}};
        shore.generation.graph.features.push_back(harbor);
        if(!mapGenV5WorldRoadSegmentIsSafe(shore,{3,1.2f},{3,2.5f}))
        {std::cerr<<"Padding blocked a short harbor entrance\n";return 1;}
    }
    const std::filesystem::path root=argv[1]; std::filesystem::create_directories(root);
    MapGenerationOptions options;
    options.generatorVersion=11;options.styleId="illustrated_atlas";
    options.styleVersion=5;options.styleSnapshotId="illustrated_atlas_v5";
    options.scale=MapLevel::World;options.creationIntent=MapCreationIntent::WorldOverview;
    options.width=48;options.height=32;options.seed=91042;
    options.requestedSettlements=4;options.contentLanguage="en";
    auto result=generateSemanticMap(options);
    auto &map=result.map;
    MapRenderPlanRequest request;request.applyFog=false;request.locale="en";
    const auto plan=compileMapRenderPlan(map,request);
    std::string error;
    if(!exportMapPng(map,root/"connections-world.png",plan,24,&error)) {std::cerr<<error;return 1;}
    {
        auto mist=map;
        for(int y=0;y<mist.height;++y)for(int x=0;x<mist.width;++x)
            mapCellAt(mist,x,y)->fogged=x>20+static_cast<int>(4*std::sin(y*.25f));
        auto fogRequest=request;fogRequest.applyFog=true;fogRequest.showGrid=false;
        if(!exportMapPng(mist,root/"fog-player-preview.png",compileMapRenderPlan(mist,fogRequest),24,&error))
        {std::cerr<<error;return 1;}
        LoadedRgbaImage gm;
        if(!loadRgbaImage(root/"connections-world.png",gm))return 1;
        for(int py=0;py<gm.height;++py)for(int px=0;px<gm.width;++px)
        {
            if(!mapCellAt(mist,px/24,py/24)->fogged)continue;
            const auto c=gm::map_render::fogTextureAt((px+.5f)/24,(py+.5f)/24,true);
            const auto offset=(static_cast<std::size_t>(py)*gm.width+px)*4;
            for(int channel=0;channel<3;++channel)
                gm.pixels[offset+channel]=static_cast<unsigned char>(std::lround(gm.pixels[offset+channel]*(1-71.f/255)+c[channel]*71.f/255));
        }
        if(!stbi_write_png((root/"fog-gm-preview.png").string().c_str(),gm.width,gm.height,4,gm.pixels.data(),gm.width*4))return 1;
    }
    for(const auto &city:map.generation.graph.features)
    {
        if(city.kind!=MapGeneratedFeatureKind::Settlement || city.points.empty()) continue;
        MapPngExportRect rect;
        rect.x=std::clamp(static_cast<int>(city.points[0].x)-4,0,map.width-8);
        rect.y=std::clamp(static_cast<int>(city.points[0].y)-4,0,map.height-8);
        rect.width=8;rect.height=8;
        if(!exportMapPngRegion(map,root/"city-entrance-detail.png",plan,rect,80,&error)) {std::cerr<<error;return 1;}
        break;
    }
    for(bool reversed:{false,true})
    {
        MapModel coast;coast.width=30;coast.height=20;resizeMapCells(coast);
        coast.atlasLevel=MapLevel::World;coast.generation.continuousRendering=true;
        coast.generation.recipe=map.generation.recipe;coast.generation.recipe.generatorVersion=12;
        for(int y=0;y<coast.height;++y)for(int x=0;x<coast.width;++x)
        {
            auto *cell=mapCellAt(coast,x,y);cell->kind=x>=18?MapTileKind::Water:MapTileKind::Dirt;cell->fogged=false;
        }
        std::vector<MapFeaturePoint> path{{9.f,4.f},{13.f,8.f},{16.f,11.f},{25.5f,15.5f}};
        if(reversed)std::reverse(path.begin(),path.end());
        appendContinuousMapWayStroke(coast,path,MapWayKind::River,3);
        projectGeneratedWayFeatureToCells(coast,coast.generation.graph.features.back(),false);
        const auto coastPlan=compileMapRenderPlan(coast,request);
        if(!exportMapPng(coast,root/(reversed?"river-from-sea.png":"river-to-sea.png"),coastPlan,28,&error))
        {std::cerr<<error;return 1;}
    }
    {
        MapModel painted;painted.width=30;painted.height=20;resizeMapCells(painted);
        painted.atlasLevel=MapLevel::World;painted.generation.continuousRendering=true;
        painted.generation.recipe=map.generation.recipe;
        for(auto &cell:painted.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        applyMapTerrainBrush(painted,mapBrushFootprint(painted,5,7,1),MapTileKind::Mountain,0,false);
        for(int x=13;x<21;++x)
            applyMapTerrainBrush(painted,mapBrushFootprint(painted,x,10+(x-13)/3,5,true),MapTileKind::Mountain,0,false);
        const auto paintedPlan=compileMapRenderPlan(painted,request);
        if(!exportMapPng(painted,root/"painted-mountains.png",paintedPlan,28,&error))
        {std::cerr<<error;return 1;}
    }
    {
        auto crossing=crossingEditFixture();
        const auto before=crossing.generation.graph.features.front();
        for(auto &p:crossing.generation.graph.features.front().points)p.x+=7.f;
        finishGeneratedMapFeatureGeometryEdit(crossing,before);
        const auto crossingPlan=compileMapRenderPlan(crossing,request);
        if(!exportMapPng(crossing,root/"moved-road-river-crossings.png",crossingPlan,28,&error))
        {std::cerr<<error;return 1;}
    }
    for(int setting=0;setting<4;++setting)
    {
        MapModel riverMap;riverMap.width=24;riverMap.height=18;resizeMapCells(riverMap);
        riverMap.atlasLevel=MapLevel::World;riverMap.generation.continuousRendering=true;
        riverMap.generation.recipe=map.generation.recipe;
        for(auto &cell:riverMap.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        if(setting==1)applyMapTerrainBrush(riverMap,mapBrushFootprint(riverMap,5,5,3,true),MapTileKind::Mountain,0,false);
        if(setting==2)for(int y=0;y<riverMap.height;++y)for(int x=18;x<riverMap.width;++x)mapCellAt(riverMap,x,y)->kind=MapTileKind::Water;
        if(setting==3)applyMapTerrainBrush(riverMap,mapBrushFootprint(riverMap,18,12,7,true),MapTileKind::Water,0,false);
        MapGeneratedFeature river;river.id="river-art-preview";river.kind=MapGeneratedFeatureKind::River;
        river.width=2.5f;river.points={{5.5f,6.5f},{8.f,8.f},{12.f,7.5f},{14.f,10.f},{19.f,12.f}};
        riverMap.generation.graph.features.push_back(river);
        // Retain standing-water underlay just as an authored waterway does.
        riverMap.generation.graph.features.front().id="manual_stroke_preview";
        projectGeneratedWayFeatureToCells(riverMap,riverMap.generation.graph.features.front(),false);
        const auto riverPlan=compileMapRenderPlan(riverMap,request);
        const char *name[]={"river-spring-marsh.png","river-mountain-source.png","river-sea-mouth.png","river-lake-mouth.png"};
        if(!exportMapPng(riverMap,root/name[setting],riverPlan,32,&error)){std::cerr<<error;return 1;}
        if(setting<2)
        {
            MapPngExportRect sourceCrop;sourceCrop.x=3;sourceCrop.y=4;sourceCrop.width=6;sourceCrop.height=5;
            if(!exportMapPngRegion(riverMap,root/(setting==0?"spring-detail.png":"mountain-spring-detail.png"),
                riverPlan,sourceCrop,80,&error)){std::cerr<<error;return 1;}
        }
    }
    {
        auto junction=crossingEditFixture();
        junction.generation.graph.features.clear();
        for(auto &cell:junction.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;}
        MapGeneratedFeature main;main.id="main-road";main.kind=MapGeneratedFeatureKind::Road;
        main.points={{16,1},{16,27}};main.width=1.5f;
        junction.generation.graph.features.push_back(main);
        appendContinuousMapWayStroke(junction,{{15.4f,5},{10,5},{6,8},{5,12},{7,16},{11,19},{15.4f,21}},MapWayKind::DirtRoad,2);
        MapGeneratedFeature river;river.id="crossing-river";river.kind=MapGeneratedFeatureKind::River;
        river.points={{2,13},{10,13},{18,15},{24,17}};river.width=2;
        junction.generation.graph.features.push_back(river);
        MapGeneratedFeature stale;stale.id="manual_bridge_2_2";stale.kind=MapGeneratedFeatureKind::Bridge;stale.points={{3,3},{6,3}};
        junction.generation.graph.features.push_back(stale);
        const auto junctionPlan=compileMapRenderPlan(junction,request);
        if(!exportMapPng(junction,root/"road-junctions-and-bridges.png",junctionPlan,32,&error)){std::cerr<<error;return 1;}
        MapPngExportRect joinRect;joinRect.x=14;joinRect.y=3;joinRect.width=4;joinRect.height=4;
        if(!exportMapPngRegion(junction,root/"road-junction-detail.png",junctionPlan,joinRect,96,&error)){std::cerr<<error;return 1;}
    }
    for(int example=0;example<6;++example)
    {
        auto bridgeMap=crossingEditFixture();
        bridgeMap.width=16;bridgeMap.height=16;resizeMapCells(bridgeMap);
        bridgeMap.generation.graph.features.clear();
        for(auto &cell:bridgeMap.cells){cell.kind=MapTileKind::Dirt;cell.wayKind=MapWayKind::None;cell.fogged=false;}
        const float angle=(example%3)*.55f;
        MapGeneratedFeature road;road.id="bridge-review-road";road.kind=MapGeneratedFeatureKind::Road;
        road.width=example<3?1.f:2.f;
        road.points={{8.f-7.f*std::cos(angle),8.f-7.f*std::sin(angle)},
                     {8.f+7.f*std::cos(angle),8.f+7.f*std::sin(angle)}};
        MapGeneratedFeature river;river.id="bridge-review-river";river.kind=MapGeneratedFeatureKind::River;
        river.width=example<3?2.f:4.f;river.points={{8,0},{8,16}};
        bridgeMap.generation.graph.features={road,river};
        const auto bridgePlan=compileMapRenderPlan(bridgeMap,request);
        MapPngExportRect crop;crop.x=5;crop.y=5;crop.width=6;crop.height=6;
        if(!exportMapPngRegion(bridgeMap,root/("bridge-detail-"+std::to_string(example)+".png"),bridgePlan,crop,80,&error))
        {std::cerr<<error;return 1;}
        if(!exportMapPng(bridgeMap,root/("bridge-fit-"+std::to_string(example)+".png"),bridgePlan,24,&error))
        {std::cerr<<error;return 1;}
    }
    std::cout<<"Generated world seed 91042 and exported city/river/mountain/crossing rendering fixtures.\n";
}
