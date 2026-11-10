#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <set>

namespace {
bool require(bool value,const char *message) { if(!value) std::cerr<<"FAIL "<<message<<'\n';return value; }
std::uint64_t hashMap(const MapModel &map) {
    std::uint64_t h=14695981039346656037ull;
    auto add=[&](std::uint64_t v) {h=(h^v)*1099511628211ull;};
    for(const auto &c:map.cells) {add(static_cast<int>(c.kind));add(static_cast<int>(c.wayKind));add(c.elevation);add(c.movementCost);add(c.blocksLineOfSight);}
    for(const auto &f:map.generation.graph.features) {add(static_cast<int>(f.kind));for(auto p:f.points) {add(std::lround(p.x*10000));add(std::lround(p.y*10000));}}
    return h;
}
}
int main(int argc,char **argv) {
    if(argc>1 && (std::string(argv[1])=="--bench" || std::string(argv[1])=="--bench-v11")) {
        for(auto type:{TacticalMapTemplate::ForestRoad,TacticalMapTemplate::RiverCrossing,TacticalMapTemplate::CitySquare}) {
            TacticalMapRecipe recipe;recipe.generatorVersion=std::string(argv[1])=="--bench-v11"?11:10;recipe.width=80;recipe.height=80;
            recipe.seed=1983376322u;recipe.mapTemplate=type;
            const auto start=std::chrono::steady_clock::now();
            const auto candidates=TacticalMapGenerator::generate(recipe,nullptr,3);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout<<"template="<<static_cast<int>(type)<<" size=80x80 requested_candidates=3 actual="<<candidates.size()<<" seconds="<<seconds<<" attempts="<<(candidates.empty()?0:candidates.front().quality.candidatesEvaluated)<<std::endl;
            if(candidates.empty() || candidates.front().quality.candidatesEvaluated!=24) return 1;
        }
        return 0;
    }
    bool ok=true;
    MapModel coverArt;coverArt.width=4;coverArt.height=4;coverArt.atlasLevel=MapLevel::Tactical;
    coverArt.generation.recipe.generatorVersion=11;coverArt.generation.recipe.creationIntent=MapCreationIntent::TacticalEncounter;resizeMapCells(coverArt);
    applyMapCellTerrainDefaults(*mapCellAt(coverArt,1,1),MapTileKind::Cover);
    const auto coverInk=gm::map_render::buildTacticalCoverInk(coverArt,false);
    ok&=require(!coverInk.empty(),"v11 cover has shared crisp geometry");
    for(const auto &primitive:coverInk) for(const auto &point:primitive.points)
        ok&=require(point.x>=1.f && point.x<=2.f && point.y>=1.f && point.y<=2.f,"cover art stays inside semantic cell");
    mapCellAt(coverArt,1,1)->fogged=true;
    ok&=require(gm::map_render::buildTacticalCoverInk(coverArt,true).empty(),"fogged cover has no ink leak");
    mapCellAt(coverArt,1,1)->fogged=false;coverArt.visibility=MapVisibility::PlayersVisible;
    coverArt.generation.enabled=true;coverArt.playerRelease.placesAndMarkers=false;
    MapRenderPlanRequest coverRequest;coverRequest.layers=MapRenderPlanSymbols;coverRequest.applyFog=false;coverRequest.audience=MapRenderAudience::Player;
    ok&=require(compileMapRenderPlan(coverArt,coverRequest).atlasFeatureInk.empty(),"unreleased cover has no player ink");
    coverArt.playerRelease.placesAndMarkers=true;
    MapGeneratedFeature hiddenCover;hiddenCover.id="private-cover";hiddenCover.kind=MapGeneratedFeatureKind::TacticalSetpiece;
    hiddenCover.gmOnly=true;hiddenCover.points={{1.5f,1.5f}};coverArt.generation.graph.features.push_back(hiddenCover);
    ok&=require(compileMapRenderPlan(coverArt,coverRequest).atlasFeatureInk.empty(),"private tactical cover feature has no player ink");
    coverArt.generation.recipe.generatorVersion=10;
    ok&=require(gm::map_render::buildTacticalCoverInk(coverArt,false).empty(),"legacy cover rendering unchanged");
    MapModel underground;underground.scope=MapScope::Dungeon;underground.creationIntent=MapCreationIntent::TacticalEncounter;
    underground.generation.recipe.generatorVersion=11;underground.metadata.customProperties["tactical.template_kind"]="6";
    ok&=require(gm::map_render::usesNaturalCaveInk(underground) && !gm::map_render::usesDungeonMasonryInk(underground),"tactical cave selects natural contour style only");
    underground.metadata.customProperties["tactical.template_kind"]="7";
    ok&=require(gm::map_render::usesDungeonMasonryInk(underground) && !gm::map_render::usesNaturalCaveInk(underground),"tactical dungeon selects masonry style only");
    underground.generation.recipe.generatorVersion=10;
    ok&=require(!gm::map_render::usesNaturalCaveInk(underground) && !gm::map_render::usesDungeonMasonryInk(underground),"legacy tactical underground styles remain unchanged");
    MapGenerationOptions zeroCover; zeroCover.generatorVersion=11;zeroCover.tacticalDensity=0;
    ok&=require(TacticalMapGenerator::recipeFromGenerationOptions(zeroCover).coverDensity==0,"v11 explicit zero cover remains zero");
    zeroCover.generatorVersion=10;
    ok&=require(TacticalMapGenerator::recipeFromGenerationOptions(zeroCover).coverDensity==24,"legacy zero cover retains historical default");

    TacticalMapRecipe cancelled;cancelled.generatorVersion=10;cancelled.width=32;cancelled.height=24;
    cancelled.quality=MapGenerationQualityLevel::Draft;
    cancelled.cancellationRequested=[] {return true;};
    ok&=require(TacticalMapGenerator::generate(cancelled,nullptr,3).empty(),"already-cancelled generation cannot publish candidates");
    int cancellationChecks=0;
    cancelled.cancellationRequested=[&] {return ++cancellationChecks>=3;};
    auto cancellationStart=std::chrono::steady_clock::now();
    ok&=require(TacticalMapGenerator::generate(cancelled,nullptr,3).empty() && cancellationChecks>=3,"semantic search stops when cancellation arrives between attempts");
    bool compositionStarted=false;
    cancelled.cancellationRequested=[&] {return compositionStarted;};
    cancelled.progressCallback=[&](const MapGenerationProgress &progress) {if(progress.stage=="Placing tactical features") compositionStarted=true;};
    ok&=require(TacticalMapGenerator::generate(cancelled,nullptr,3).empty() && compositionStarted,"composition cancellation cannot publish a partial candidate set");
    std::cout<<"Cancellation scenarios seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-cancellationStart).count()<<std::endl;
    MapModel building;building.width=6;building.height=5;building.atlasLevel=MapLevel::Tactical;
    building.generation.recipe.generatorVersion=10;building.generation.continuousRendering=true;
    building.generation.enabled=true;resizeMapCells(building);
    for(auto &c:building.cells) c.fogged=false;
    for(auto p:std::vector<MapCellCoordinate>{{1,1},{2,1},{1,2}}) applyMapCellTerrainDefaults(*mapCellAt(building,p.x,p.y),MapTileKind::Building);
    const auto roof=gm::map_render::buildTacticalStructureInk(building);
    ok&=require(!roof.empty(),"v10 building has roof ink");
    for(const auto &primitive:roof) for(auto point:primitive.points) {
        const auto *cell=mapCellAt(building,static_cast<int>(std::floor(point.x)),static_cast<int>(std::floor(point.y)));
        ok&=require(cell && cell->kind==MapTileKind::Building,"edited L roof stays inside actual blocked footprint");
    }
    building.generation.recipe.generatorVersion=9;
    ok&=require(gm::map_render::buildTacticalStructureInk(building).empty(),"legacy renderer unchanged");
    MapGeneratedFeature crossing;crossing.width=.65f;crossing.materialId="fantasy_neutral.tactical.bridge.stone_causeway";
    ok&=require(gm::map_render::tacticalBridgeStampWidth(building,crossing)==1.75f,"legacy bridge stamp width unchanged");
    building.generation.recipe.generatorVersion=10;
    ok&=require(gm::map_render::tacticalBridgeStampWidth(building,crossing)==1.f,"narrow stone crossing cannot render as a wide wagon bridge");
    MapRenderPlanRequest request;request.layers=MapRenderPlanSymbols;request.applyFog=false;
    auto gm=compileMapRenderPlan(building,request);
    ok&=require(gm.atlasFeatureInk.size()==roof.size(),"GM/export plan owns the complete structure geometry");
    request.audience=MapRenderAudience::Player;building.visibility=MapVisibility::PlayersVisible;
    building.playerRelease.placesAndMarkers=false;
    ok&=require(compileMapRenderPlan(building,request).atlasFeatureInk.empty(),"unreleased player buildings have no roof leak");
    building.playerRelease.placesAndMarkers=true;
    const auto player=compileMapRenderPlan(building,request);
    ok&=require(player.atlasFeatureInk.size()==roof.size(),"reviewed public building geometry matches GM/export");
    auto privateMap=building;
    MapGeneratedFeature hidden;hidden.id="private-building";hidden.kind=MapGeneratedFeatureKind::BuildingGroup;
    hidden.gmOnly=true;hidden.points={{1.5f,1.5f}};privateMap.generation.graph.features.push_back(hidden);
    ok&=require(compileMapRenderPlan(privateMap,request).atlasFeatureInk.empty(),"private building suppression covers the full L footprint");
    applyMapCellTerrainDefaults(*mapCellAt(privateMap,4,1),MapTileKind::Building);
    const auto publicOnly=compileMapRenderPlan(privateMap,request);
    ok&=require(!publicOnly.atlasFeatureInk.empty(),"a separate public building remains visible");
    for(const auto &primitive:publicOnly.atlasFeatureInk) for(auto point:primitive.points)
        ok&=require(point.x>=4.f && point.x<=5.f,"private footprint cannot survive through neighboring roof cells");
    request.layers=MapRenderPlanTerrain;
    ok&=require(compileMapRenderPlan(building,request).atlasFeatureInk.empty(),"terrain-only plan does not leak hidden symbol structures");
    MapModel grouped; grouped.width=20; grouped.height=20; grouped.atlasLevel=MapLevel::Tactical;
    grouped.generation.recipe.generatorVersion=10; resizeMapCells(grouped);
    MapRegionModel focus;focus.name="Objective 1";focus.width=3;focus.height=3;grouped.regions.push_back(focus);
    for(int y=0;y<5;++y) for(int x=0;x<20;++x) mapCellAt(grouped,x,y)->kind=MapTileKind::Building;
    auto scattered=grouped; for(auto &cell:scattered.cells) cell.kind=MapTileKind::Empty;
    for(int y=0;y<20;y+=2) for(int x=0;x<20;x+=2) mapCellAt(scattered,x,y)->kind=MapTileKind::Building;
    ok&=require(mapGenV4VisualHierarchyPercent(grouped)>mapGenV4VisualHierarchyPercent(scattered),"one coherent footprint differs from one hundred scattered stamps");
    auto legacy=grouped;legacy.generation.recipe.generatorVersion=9;
    ok&=require(mapGenV4VisualHierarchyPercent(legacy)==mapGenV4VisualHierarchyPercent(scattered),"legacy per-cell detail metric remains unchanged");
    const int compositionVersion=argc>1 && std::string(argv[1])=="--v12"?12:argc>1 && std::string(argv[1])=="--v11"?11:10;
    for(int t=0;t<8;++t) {
        TacticalMapRecipe recipe;recipe.generatorVersion=compositionVersion;recipe.mapTemplate=static_cast<TacticalMapTemplate>(t);
        recipe.width=24;recipe.height=18;recipe.seed=1579013541u+static_cast<unsigned>(t)*7919u;recipe.quality=MapGenerationQualityLevel::Draft;
        std::cout<<"Checking tactical template "<<t<<std::endl;
        auto maps=TacticalMapGenerator::generate(recipe,nullptr,1);
        ok&=require(maps.size()==1,"every selected-version tactical template generates a small encounter");
        if(maps.empty()) continue;
        ok&=require(maps.front().readinessIssues.empty(),"candidate retains route, spawn, objective and footprint checks");
        if(compositionVersion>=12 && recipe.mapTemplate==TacticalMapTemplate::VillageStreet) {
            const auto &features=maps.front().map.generation.graph.features;
            const MapGeneratedFeature *mainRoad=nullptr,*lane=nullptr;
            for(const auto &feature:features) {
                if(feature.id.find("village_main")!=std::string::npos)mainRoad=&feature;
                if(feature.id.find("village_lane")!=std::string::npos)lane=&feature;
            }
            ok&=require(mainRoad && lane && mainRoad->points.size()>1 && lane->points.size()>1,"v12 village retains both route geometries");
            if(mainRoad && lane && mainRoad->points.size()>1 && lane->points.size()>1) {
                const auto nearPoint=[](auto a,auto b){return std::hypot(a.x-b.x,a.y-b.y)<.01f;};
                const auto joins=[&](auto p){return std::any_of(mainRoad->points.begin(),mainRoad->points.end(),[&](auto q){return nearPoint(p,q);});};
                ok&=require(joins(lane->points.front()) && joins(lane->points.back()),"v12 village bypass joins both main-road anchors exactly");
            }
        }
        auto repeat=TacticalMapGenerator::generate(recipe,nullptr,1);
        ok&=require(!repeat.empty() && hashMap(repeat.front().map)==hashMap(maps.front().map),"same tactical recipe preserves terrain/LOS/geometry determinism");
    }
    TacticalMapRecipe villageFrontage;villageFrontage.generatorVersion=11;villageFrontage.mapTemplate=TacticalMapTemplate::VillageStreet;
    if(compositionVersion>=12) {
        TacticalMapRecipe compact;compact.generatorVersion=12;compact.mapTemplate=TacticalMapTemplate::VillageStreet;
        compact.width=24;compact.height=18;compact.seed=89020148u;compact.coverDensity=0;compact.objectiveCount=2;compact.minimumCreatureFootprint=2;
        const auto maps=TacticalMapGenerator::generate(compact,nullptr,1);
        ok&=require(!maps.empty(),"compact v12 village frontage regression generates");
        if(!maps.empty()) {
            int buildings=0;for(const auto &c:maps.front().map.cells)buildings+=c.kind==MapTileKind::Building;
            ok&=require(buildings>=16,"compact village retains meaningful built frontage after safe-placement rejection");
        }
    }
    villageFrontage.width=24;villageFrontage.height=18;villageFrontage.seed=1480700753u;
    villageFrontage.coverDensity=0;villageFrontage.objectiveCount=2;villageFrontage.minimumCreatureFootprint=2;
    const auto villageFrontages=TacticalMapGenerator::generate(villageFrontage,nullptr,3);
    ok&=require(!villageFrontages.empty(),"small village frontage regression generates");
    for(const auto &candidate:villageFrontages) {
        int southern=0;
        for(int y=candidate.map.height/2+4;y<candidate.map.height;++y) for(int x=0;x<candidate.map.width;++x)
            southern+=mapCellAt(candidate.map,x,y)->kind==MapTileKind::Building;
        ok&=require(southern>=4,"small village keeps at least one southern cottage after row omissions");
    }
    TacticalMapRecipe cityFrontage;cityFrontage.generatorVersion=11;cityFrontage.mapTemplate=TacticalMapTemplate::CitySquare;
    cityFrontage.width=24;cityFrontage.height=18;cityFrontage.seed=1015134254u;
    cityFrontage.coverDensity=0;cityFrontage.objectiveCount=3;cityFrontage.minimumCreatureFootprint=3;
    const auto frontageMaps=TacticalMapGenerator::generate(cityFrontage,nullptr,3);
    ok&=require(!frontageMaps.empty(),"small city frontage regression generates");
    for(const auto &candidate:frontageMaps) {
        int west=0,east=0;
        for(int y=0;y<candidate.map.height;++y) for(int x=0;x<candidate.map.width;++x)
            if(mapCellAt(candidate.map,x,y)->kind==MapTileKind::Building) (x<candidate.map.width/2?west:east)++;
        ok&=require(west>=6 && east>=6,"small square keeps frontage on both sides after seeded setbacks");
    }
    TacticalMapRecipe caveRegression;caveRegression.generatorVersion=10;caveRegression.mapTemplate=TacticalMapTemplate::Cave;
    caveRegression.width=32;caveRegression.height=24;caveRegression.seed=1463840099u;
    caveRegression.coverDensity=24;caveRegression.objectiveCount=2;caveRegression.minimumCreatureFootprint=3;
    const auto caveMaps=TacticalMapGenerator::generate(caveRegression,nullptr,1);
    ok&=require(!caveMaps.empty() && caveMaps.front().readinessIssues.empty(),"R19 cave keeps both edge route mouths open after asymmetric gallery placement");
    caveRegression.width=48;caveRegression.height=36;caveRegression.seed=727883700u;
    caveRegression.coverDensity=40;caveRegression.objectiveCount=3;caveRegression.minimumCreatureFootprint=1;
    const auto fragmentMaps=TacticalMapGenerator::generate(caveRegression,nullptr,1);
    ok&=require(!fragmentMaps.empty(),"R29 cave fragment regression produces a candidate");
    if(!fragmentMaps.empty()) {
        const auto &cave=fragmentMaps.front().map;
        const auto rockAt=[&](int x,int y) {const auto *cell=mapCellAt(cave,x,y);return cell && cell->kind==MapTileKind::Wall;};
        int unsupported=0;
        for(int y=1;y<cave.height-1;++y) for(int x=1;x<cave.width-1;++x) {
            if(!rockAt(x,y)) continue;
            bool supported=false;
            for(int dy=-1;dy<=0;++dy) for(int dx=-1;dx<=0;++dx)
                supported|=rockAt(x+dx,y+dy) && rockAt(x+dx+1,y+dy)
                        && rockAt(x+dx,y+dy+1) && rockAt(x+dx+1,y+dy+1);
            unsupported+=!supported;
        }
        ok&=require(unsupported==0,"R29 cave removes thin isolated wall ribbons while preserving supported rock masses");
    }
    if(compositionVersion>=12) {
        for(const auto selectedTemplate:{TacticalMapTemplate::VillageStreet,TacticalMapTemplate::RiverCrossing}) {
            std::set<int> boundaryExitCounts;
            for(unsigned seed=1;seed<=12;++seed) {
                TacticalMapRecipe varied;varied.generatorVersion=12;varied.illustrationRecipeVersion=6;
                varied.width=32;varied.height=24;varied.seed=seed*7919u;varied.mapTemplate=selectedTemplate;
                varied.quality=MapGenerationQualityLevel::Draft;
                const auto candidates=TacticalMapGenerator::generate(varied,nullptr,1);
                ok&=require(!candidates.empty() && candidates.front().readinessIssues.empty(),
                    "recipe 6 layout alternatives retain playable routes, deployment and objectives");
                if(candidates.empty())continue;
                const auto &map=candidates.front().map;
                int exits=0;
                for(const auto &feature:map.generation.graph.features) {
                    const auto key=selectedTemplate==TacticalMapTemplate::VillageStreet?"village_lane":"crossing_route_1";
                    if(feature.id.find(key)==std::string::npos || feature.points.size()<2)continue;
                    for(const auto p:{feature.points.front(),feature.points.back()})
                        exits+=p.x<=.01f || p.y<=.01f || p.x>=map.width-.01f || p.y>=map.height-.01f;
                }
                boundaryExitCounts.insert(exits);
                if(seed<=2) {
                    const auto repeat=TacticalMapGenerator::generate(varied,nullptr,1);
                    ok&=require(!repeat.empty() && hashMap(map)==hashMap(repeat.front().map),
                        "recipe 6 route alternatives reproduce their saved seed");
                }
            }
            ok&=require(boundaryExitCounts.size()>=2,"recipe 6 changes route relationships rather than only jittering a repeated loop");
        }
    }
    TacticalMapRecipe large;large.generatorVersion=compositionVersion;large.mapTemplate=TacticalMapTemplate::ForestRoad;
    large.width=80;large.height=80;large.seed=1983376322u;large.quality=MapGenerationQualityLevel::Draft;
    const auto maps=TacticalMapGenerator::generate(large,nullptr,1);
    ok&=require(!maps.empty() && maps.front().map.width==80 && maps.front().map.height==80,"selected version supports actual 80x80 tactical maps");
    large.generatorVersion=11;large.mapTemplate=TacticalMapTemplate::RiverCrossing;large.minimumCreatureFootprint=3;
    const auto windingCrossings=TacticalMapGenerator::generate(large,nullptr,1);
    ok&=require(!windingCrossings.empty() && windingCrossings.front().readinessIssues.empty(),"80x80 curved inland approaches retain bridge and 3x3 deployment connectivity");
    std::cout<<(ok?"tactical_v10_tests passed\n":"tactical_v10_tests failed\n");return ok?0:1;
}
