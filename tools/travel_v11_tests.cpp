#include "Models/Map/TravelSceneGenerator.hpp"
#include "Models/Map/MapTravelInk.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include <iostream>
#include <sstream>

static std::string fingerprint(const MapModel &map) {
    std::ostringstream out;
    for(const auto &c:map.cells)out<<static_cast<int>(c.kind)<<':'<<static_cast<int>(c.wayKind)<<':'<<c.elevation<<';';
    for(const auto &f:map.generation.graph.features){out<<f.id<<f.materialId;for(const auto&p:f.points)out<<p.x<<','<<p.y<<';';}
    return out.str();
}
int main() {
    bool ok=true;
    auto require=[&](bool value,const char *message){if(!value){std::cerr<<message<<'\n';ok=false;}};
    MapModel source;source.metadata.id="travel_v11_fixed_source";source.atlasLevel=MapLevel::World;
    TravelSceneRecipe request;request.schemaVersion=3;request.width=36;request.height=26;request.routeId="road";
    for(int purpose=0;purpose<5;++purpose) {
        request.purpose=static_cast<TravelScenePurpose>(purpose);request.biome=purpose==3?"mountain pass":purpose==1?"highland":"forest";request.seed=6407u+purpose*79u;
        const auto maps=TravelSceneGenerator::generate(source,nullptr,request,1);
        require(!maps.empty(),"New travel purpose produced no eligible scene");if(maps.empty())continue;
        const auto &m=maps.front().localMap;
        require(m.width==36 && m.height==26 && m.generation.recipe.generatorVersion==11,"New recipe dimensions/version lost");
        require(validateMapGeneration(m,analyzeMapGenerationQuality(m)).eligible,"Composed scene fails validation");
        if(purpose==1)require(m.generation.recipe.landscape==MapLandscapeProfile::Highlands,"English highland biome falls back to river valley");
        require(!gm::map_render::buildTravelPurposeInk(m,false).empty(),"Purpose missing shared ink");
        for(int y=0;y<m.height;++y)for(int x=0;x<m.width;++x){const auto *c=mapCellAt(m,x,y);if(mapGenWayKindIsRoadLike(c->wayKind)){if(c->kind==MapTileKind::Mountain || c->movementCost>=99)std::cerr<<"purpose "<<purpose<<" cell "<<x<<","<<y<<" kind "<<static_cast<int>(c->kind)<<" cost "<<c->movementCost<<"\n"; require(c->kind!=MapTileKind::Mountain && c->movementCost<99,"Road intersects blocked rock");}}
        if(purpose==3)require(!gm::map_render::buildTravelReliefInk(m,false).empty(),"Pass has no visible rock relief");
        if(purpose==3)for(const auto&f:m.generation.graph.features)if(f.materialId=="fantasy_neutral.travel.obstacle"){
            const int x=static_cast<int>(f.points.front().x),y=static_cast<int>(f.points.front().y);int shoulders=0;
            for(int oy=-5;oy<=5;++oy)for(int ox=-5;ox<=5;++ox){const auto*c=mapCellAt(m,x+ox,y+oy);if(c && c->kind==MapTileKind::Mountain)++shoulders;}
            require(shoulders>=4,"Pass obstacle has no nearby rock shoulders");
        }
        if(purpose==0){auto again=TravelSceneGenerator::generate(source,nullptr,request,1);require(!again.empty() && fingerprint(m)==fingerprint(again.front().localMap),"Recipe not deterministic");}
    }
    // R24 quiet-lake put the campfire in the pond although its anchor was dry.
    source.metadata.id="preview_travel_source_1456000461";
    request.seed=1456000461u;request.routeId="preview_route";request.biome="lake";request.purpose=TravelScenePurpose::Quiet;request.width=40;request.height=32;
    const auto lakes=TravelSceneGenerator::generate(source,nullptr,request,1);
    require(!lakes.empty(),"Lake camp regression produced no scene");
    if(!lakes.empty())for(const auto &f:lakes.front().localMap.generation.graph.features)if(f.materialId=="fantasy_neutral.travel.rest") {
        const auto &m=lakes.front().localMap;const int x=static_cast<int>(f.points.front().x),y=static_cast<int>(f.points.front().y);
        for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){const auto*c=mapCellAt(m,x+ox,y+oy);require(c && !mapGenRiverInfluenceCell(c) && c->kind!=MapTileKind::Bridge,"Lake camp art footprint overlaps water");}
    }
    // V12 illustration is an explicit new recipe; legacy assertions below remain.
    request.schemaVersion=4;request.width=36;request.height=26;request.biome="forest";
    for(int purpose=0;purpose<5;++purpose) {
        request.purpose=static_cast<TravelScenePurpose>(purpose);request.seed=99071u+purpose*7919u;
        const auto maps=TravelSceneGenerator::generateStandalone(request,1);
        require(!maps.empty(),"V12 travel purpose produces no candidate");if(maps.empty())continue;
        const auto &m=maps.front().localMap;
        require(m.generation.recipe.generatorVersion==12 && m.generation.recipe.styleVersion==6 &&
                m.generation.recipe.styleId=="illustrated_atlas" && m.generation.recipe.styleSnapshotId=="illustrated_atlas_v6","V12 travel style pin lost");
        require(validateMapGeneration(m,analyzeMapGenerationQuality(m)).eligible,"V12 travel semantic validation failed");
        int furnishings=0;
        for(const auto &f:m.generation.graph.features)if(f.id.find("::travel_furnishing_")!=std::string::npos) {
            ++furnishings;require(!f.points.empty(),"Furnishing has no anchor");if(f.points.empty())continue;
            const auto *c=mapCellAt(m,int(f.points.front().x),int(f.points.front().y));
            require(c && c->kind==MapTileKind::Cover && c->wayKind==MapWayKind::None && !c->blocksLineOfSight,"Furnishing blocks route or has no gameplay cover");
        }
        require(furnishings>0,"V12 purpose has no supporting furniture");
        const auto repeat=TravelSceneGenerator::generateStandalone(request,1);
        require(!repeat.empty() && fingerprint(m)==fingerprint(repeat.front().localMap),"V12 travel is nondeterministic");
    }
    for(const char *biome:{"coast","lake"}) {
        request.schemaVersion=4;request.biome=biome;request.purpose=TravelScenePurpose::Quiet;request.seed=602097u;
        const auto maps=TravelSceneGenerator::generateStandalone(request,1);
        require(!maps.empty(),"V12 water composition has no eligible scene");if(maps.empty())continue;
        const auto &m=maps.front().localMap;int water=0,edgeWater=0;
        for(int y=0;y<m.height;++y)for(int x=0;x<m.width;++x) {
            const auto *c=mapCellAt(m,x,y);if(c->kind!=MapTileKind::Water)continue;
            ++water;edgeWater+=x==0 || y==0 || x==m.width-1 || y==m.height-1;
            require(c->wayKind==MapWayKind::None,"V12 water overwrote a road");
        }
        require(water>m.width*m.height/25,"Lake/coast request remains only a puddle");
        require(std::string(biome)=="coast"?edgeWater>=m.width:edgeWater==0,"Coast must touch edge; inland lake must not");
    }
    request.schemaVersion=4;request.biome="river";request.purpose=TravelScenePurpose::Obstacle;request.seed=1604499718u;
    for(const auto &required:std::vector<std::vector<std::string>>{{},{"Watchtower","Old shrine"}}) {
        request.requiredElements=required;
        const auto maps=TravelSceneGenerator::generateStandalone(request,1);
        require(!maps.empty(),"V12 river cannot preserve requested anchors");if(maps.empty())continue;
        const auto &m=maps.front().localMap;int bridges=0,rivers=0;
        for(const auto &f:m.generation.graph.features) {
            rivers+=f.kind==MapGeneratedFeatureKind::River;bridges+=f.kind==MapGeneratedFeatureKind::Bridge;
            if(f.locked)for(const auto &p:f.points){const auto *c=mapCellAt(m,int(p.x),int(p.y));require(c && c->kind!=MapTileKind::Water && c->kind!=MapTileKind::Bridge,"River erased a locked point");}
        }
        require(rivers>0 && bridges>0,"Explicit river lacks through-channel or real crossing");
        require(validateMapGeneration(m,analyzeMapGenerationQuality(m)).eligible,"V12 river fails hydrology/route validation");
    }
    request.requiredElements.clear();
    request.schemaVersion=2;request.width=24;request.height=18;request.biome="forest";request.purpose=TravelScenePurpose::Quiet;
    const auto old=TravelSceneGenerator::generate(source,nullptr,request,1);
    require(!old.empty() && old.front().localMap.generation.recipe.generatorVersion==5 && old.front().localMap.width==64 && old.front().localMap.height==48,"Legacy dimensions/version changed");
    if(!old.empty())require(gm::map_render::buildTravelPurposeInk(old.front().localMap,false).empty(),"New art leaked into legacy recipe");

    TravelSceneRecipe standaloneRequest;
    standaloneRequest.schemaVersion=3;standaloneRequest.width=24;standaloneRequest.height=18;
    standaloneRequest.biome="forest";standaloneRequest.seed=9137;
    int progressCalls=0;
    auto standalone=TravelSceneGenerator::generateStandalone(standaloneRequest,1,{},[&](const MapGenerationProgress &){++progressCalls;});
    require(!standalone.empty() && progressCalls>0,"Standalone travel failed or lost progress");
    if(!standalone.empty()) {
        const auto &m=standalone.front().localMap;
        require(m.parentMapId.empty() && m.atlasPlacement.parentId.empty() && m.atlasStatus==MapAtlasStatus::Standalone,"Standalone travel persisted a fake parent");
        require(m.width==24 && m.height==18 && m.generation.recipe.generatorVersion==11,"Standalone travel lost current recipe");
        auto repeated=TravelSceneGenerator::generateStandalone(standaloneRequest,1);
        require(!repeated.empty() && fingerprint(m)==fingerprint(repeated.front().localMap),"Standalone travel not deterministic");
    }
    require(TravelSceneGenerator::generateStandalone(standaloneRequest,1,[]{return true;}).empty(),"Standalone ignores cancellation");
    bool cancelDuringProgress=false;
    require(TravelSceneGenerator::generateStandalone(standaloneRequest,1,[&]{return cancelDuringProgress;},[&](const MapGenerationProgress &){cancelDuringProgress=true;}).empty(),"Travel cancellation leaks partial candidates");

    MapModel secret;secret.width=12;secret.height=12;resizeMapCells(secret);secret.creationIntent=MapCreationIntent::TravelScene;secret.atlasLevel=MapLevel::Site;secret.generation.recipe.generatorVersion=11;secret.visibility=MapVisibility::PlayersVisible;
    for(auto &c:secret.cells){c.fogged=false;c.kind=MapTileKind::Dirt;}
    MapGeneratedFeature focus;focus.id="secret_camp";focus.kind=MapGeneratedFeatureKind::Landmark;focus.points={{6.5f,6.5f}};focus.materialId="fantasy_neutral.travel.rest";focus.gmOnly=true;focus.discovered=true;secret.generation.graph.features.push_back(focus);mapCellAt(secret,6,6)->kind=MapTileKind::Landmark;
    MapRenderPlanRequest render;render.layers=MapRenderPlanSymbols;render.applyFog=false;
    require(!compileMapRenderPlan(secret,render).atlasFeatureInk.empty(),"GM preview missing camp");
    render.audience=MapRenderAudience::Player;
    require(compileMapRenderPlan(secret,render).atlasFeatureInk.empty(),"Private travel purpose leaks into player render plan");
    secret.generation.graph.features.front().gmOnly=false;secret.playerRelease.placesAndMarkers=true;
    require(!compileMapRenderPlan(secret,render).atlasFeatureInk.empty(),"Reviewed public camp missing");
    mapCellAt(secret,6,6)->fogged=true;render.applyFog=true;
    require(compileMapRenderPlan(secret,render).atlasFeatureInk.empty(),"Unrevealed travel purpose leaks through fog");
    secret.generation.recipe.generatorVersion=12;secret.generation.recipe.styleId="illustrated_atlas";
    secret.generation.recipe.styleVersion=6;secret.generation.recipe.styleSnapshotId="illustrated_atlas_v6";
    secret.generation.graph.features.clear();
    MapGeneratedFeature shelter;shelter.id="shelter";shelter.kind=MapGeneratedFeatureKind::TacticalSetpiece;
    shelter.materialId="fantasy_neutral.travel.shelter";shelter.width=2.f;shelter.points={{3.5f,3.5f},{4.5f,3.5f},{3.5f,4.5f},{4.5f,4.5f}};
    for(auto p:shelter.points){auto *c=mapCellAt(secret,int(p.x),int(p.y));c->kind=MapTileKind::Cover;c->fogged=false;}
    secret.generation.graph.features.push_back(shelter);
    require(!gm::map_render::buildTravelPurposeInk(secret,true).empty(),"Revealed shelter has no illustration");
    mapCellAt(secret,4,4)->fogged=true;
    require(gm::map_render::buildTravelPurposeInk(secret,true).empty(),"Shelter reveals a hidden non-anchor footprint cell");
    mapCellAt(secret,4,4)->fogged=false;secret.generation.recipe.styleVersion=5;
    require(gm::map_render::buildTravelPurposeInk(secret,false).empty(),"V6 shelter appears in historical style");
    if(ok)std::cout<<"Travel v11 dimensions, purposes, deterministic generation, pass access, legacy gating and player privacy passed\n";
    return ok?0:1;
}


