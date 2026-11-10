#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include <iostream>
#include <chrono>
bool check(bool v,const char *m){if(!v)std::cerr<<m<<'\n';return v;}
int main(){
 bool ok=true;MapGenerationOptions o;o.generatorVersion=11;o.scope=MapScope::Dungeon;o.scale=MapLevel::Tactical;o.creationIntent=MapCreationIntent::Cave;o.caveNetwork=true;o.qualityAttempts=1;o.routeDensity=65;o.waterLevel=55;o.tacticalDensity=50;
 for(unsigned seed=1;seed<=50;++seed){
  o.seed=seed*9719;o.width=18+(seed%7)*9;o.height=18+(seed%5)*8;
  auto a=mapGenGenerateCandidate(o).map,b=mapGenGenerateCandidate(o).map;
  std::vector<MapCellCoordinate>stairs;int chambers=0,tunnels=0;
  for(const auto &f:a.generation.graph.features){chambers+=f.materialId=="fantasy_neutral.cave.chamber";tunnels+=f.materialId=="fantasy_neutral.cave.tunnel";}
  ok&=check(chambers>=4,"Not enough cave chambers");
  if(chambers>4)ok&=check(tunnels>=chambers,"No alternate approach at high route density");
  for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x){
   const auto *c=mapCellAt(a,x,y),*d=mapCellAt(b,x,y);
   ok&=check(c->kind==d->kind&&c->wayKind==d->wayKind,"Non deterministic cave cells");
   if(c->kind==MapTileKind::Stairs)stairs.push_back({x,y});
   if(c->kind==MapTileKind::Cover||c->kind==MapTileKind::Rubble||c->kind==MapTileKind::Hazard||c->kind==MapTileKind::Pit)
    for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){const auto *n=mapCellAt(a,x+ox,y+oy);ok&=check(n&&n->kind!=MapTileKind::Wall&&n->kind!=MapTileKind::Empty&&n->kind!=MapTileKind::Stairs,"Prop lacks interior margin");}
  }
  ok&=check(stairs.size()==2,"Cave must retain exactly two distinct stairs");
  if(stairs.size()==2)ok&=check(mapGenCoordinatesConnected(a,stairs[0],stairs[1],false),"Cave stairs disconnected");
  ok&=check(!gm::map_render::buildNaturalCaveInk(a,false).empty(),"Missing cave rock geometry");
  for(auto &c:a.cells)c.fogged=true;
  ok&=check(gm::map_render::buildNaturalCaveInk(a,true).empty(),"Fogged cave boundary leaks");
  ok&=check(gm::map_render::buildNaturalCaveFloorInk(a,true).empty(),"Fogged cave floor leaks");
  ok&=check(gm::map_render::buildNaturalCavePoolInk(a,true).empty(),"Fogged cave water leaks");
  ok&=check(gm::map_render::buildNaturalCaveSymbolInk(a,true).empty(),"Fogged cave props leak");
  a.generation.recipe.generatorVersion=10;ok&=check(gm::map_render::buildNaturalCaveInk(a,false).empty(),"Legacy cave got new art");
 }
 o.seed=7744;o.width=48;o.height=36;o.tacticalDensity=0;o.waterLevel=0;o.routeDensity=0;auto dry=mapGenGenerateCandidate(o).map;
 for(const auto &c:dry.cells)ok&=check(c.kind!=MapTileKind::Water,"Zero-water cave contains generated water");
 int chambers=0,tunnels=0;for(const auto &f:dry.generation.graph.features){chambers+=f.materialId=="fantasy_neutral.cave.chamber";tunnels+=f.materialId=="fantasy_neutral.cave.tunnel";}
 ok&=check(tunnels==chambers-1,"Zero-loop cave has extra passages");
 MapModel ring;ring.scope=MapScope::Dungeon;ring.creationIntent=MapCreationIntent::Cave;ring.generation.recipe.generatorVersion=11;ring.width=16;ring.height=16;resizeMapCells(ring);mapGenClear(ring,MapTileKind::Wall);
 for(int y=3;y<13;++y)for(int x=3;x<13;++x)mapGenSetCell(ring,x,y,MapTileKind::Cave);
 for(int y=6;y<9;++y)for(int x=6;x<9;++x)mapGenSetCell(ring,x,y,MapTileKind::Wall);
 const auto contours=gm::map_render::naturalCaveContours(ring,false);int outer=0,islands=0;double outerArea=0,islandArea=0;
 for(const auto &p:contours){const float a=gm::map_render::naturalCaveContourArea(p);if(a>0){++outer;outerArea+=a;}else{++islands;islandArea-=a;}}
 ok&=check(outer==1&&islands==1,"Continuous cave contours lost a rock island");
 double filledFloor=0,filledVoid=0;
 for(const auto &p:gm::map_render::buildNaturalCaveFloorInk(ring,false))if(p.filled&&p.points.size()==3){double a=std::abs(gm::map_render::naturalCaveContourArea(p.points));if(p.color.r==139)filledFloor+=a;else if(p.color.r==31)filledVoid+=a;}
 ok&=check(std::abs(filledFloor-outerArea)<.01,"Continuous floor triangulation omitted geometry");
 ok&=check(std::abs(filledVoid-256.-islandArea)<.01,"Continuous floor filled in an impassable rock island");
 o.width=80;o.height=56;o.waterLevel=70;o.tacticalDensity=45;o.routeDensity=65;auto large=mapGenGenerateCandidate(o).map;
 ok&=check(large.generation.recipe.waterLevel==70&&large.generation.recipe.routeDensity==65&&large.generation.recipe.tacticalDensity==45,"Generation controls not persisted in cave recipe");
 MapRenderPlanRequest request;request.applyFog=false;request.layers=MapRenderPlanTerrain;
 const auto terrainOnly=compileMapRenderPlan(large,request);
 for(const auto &p:terrainOnly.atlasFeatureInk)ok&=check(!(p.color.r==63&&p.color.g==118&&p.color.b==119),"Hidden Water layer leaked cave pools");
 request.layers=MapRenderPlanAll;const auto start=std::chrono::steady_clock::now();const auto complete=compileMapRenderPlan(large,request);
 const auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"80x56 shared render-plan compile milliseconds="<<millis<<" primitives="<<complete.atlasInk.size()+complete.atlasFeatureInk.size()<<'\n';
 std::cout<<(ok?"Cave generation 11 regressions passed\n":"Cave generation 11 regressions FAILED\n");return ok?0:1;
}
