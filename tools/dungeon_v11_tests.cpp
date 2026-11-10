#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include <iostream>
#include <chrono>
bool requireDungeon(bool v,const char *m){if(!v)std::cerr<<m<<'\n';return v;}
int main(){
 bool ok=true;MapGenerationOptions o;o.generatorVersion=11;o.scope=MapScope::Dungeon;o.scale=MapLevel::Tactical;o.creationIntent=MapCreationIntent::Dungeon;o.caveNetwork=false;o.qualityAttempts=1;o.routeDensity=65;o.tacticalDensity=50;
 int doorsTested=0;
 for(unsigned seed=1;seed<=60;++seed){
  o.seed=seed*9719;o.width=18+(seed%7)*9;o.height=18+(seed%5)*8;
  auto a=mapGenGenerateCandidate(o).map,b=mapGenGenerateCandidate(o).map;
  std::vector<MapCellCoordinate> stairs;
  const auto wall=[&](int x,int y){auto*c=mapCellAt(a,x,y);return !c||c->kind==MapTileKind::Wall||c->kind==MapTileKind::Empty;};
  for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x){auto*c=mapCellAt(a,x,y);auto*d=mapCellAt(b,x,y);
   ok&=requireDungeon(c->kind==d->kind&&c->wayKind==d->wayKind,"Non deterministic dungeon");
   if(c->kind==MapTileKind::Stairs)stairs.push_back({x,y});
   if(c->kind==MapTileKind::Door){++doorsTested;for(auto d:std::vector<MapCellCoordinate>{{1,0},{0,1}}){auto*n=mapCellAt(a,x+d.x,y+d.y);ok&=requireDungeon(!n||n->kind!=MapTileKind::Door,"Adjacent redundant doors");}const bool n=wall(x,y-1),s=wall(x,y+1),w=wall(x-1,y),e=wall(x+1,y);ok&=requireDungeon((n&&s&&!w&&!e)||(w&&e&&!n&&!s),"Door floats in an open room/junction or blocks a blind wall");}
   if(c->kind==MapTileKind::Cover||c->kind==MapTileKind::Rubble||c->kind==MapTileKind::Hazard||c->kind==MapTileKind::Pit)
    for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox){auto*n=mapCellAt(a,x+ox,y+oy);ok&=requireDungeon(n&&n->kind!=MapTileKind::Wall&&n->kind!=MapTileKind::Empty&&n->kind!=MapTileKind::Door&&n->kind!=MapTileKind::Stairs,"Prop intrudes on wall or threshold");}
  }
  ok&=requireDungeon(stairs.size()==2,"Expected distinct entrance/exit stairs");
  if(stairs.size()==2)ok&=requireDungeon(mapGenCoordinatesConnected(a,stairs[0],stairs[1],false),"Disconnected dungeon stair route");
  auto floor=gm::map_render::buildDungeonFloorInk(a,false);ok&=requireDungeon(!floor.empty(),"Missing new floor");
  for(auto&p:floor)if(p.filled){float x=0,y=0;for(auto&q:p.points){x+=q.x;y+=q.y;}x/=p.points.size();y/=p.points.size();ok&=requireDungeon(!wall(static_cast<int>(x),static_cast<int>(y)),"Floor art fills semantic wall");}
  for(auto&c:a.cells)c.fogged=true;
  ok&=requireDungeon(gm::map_render::buildDungeonFloorInk(a,true).empty()&&gm::map_render::buildDungeonPropInk(a,true).empty(),"Fog leaks dungeon art");
  a.generation.recipe.generatorVersion=10;
  ok&=requireDungeon(gm::map_render::buildDungeonFloorInk(a,false).empty()&&gm::map_render::buildDungeonPropInk(a,false).empty(),"Legacy dungeon received generation11 art");
 }
 o.width=80;o.height=56;o.seed=12345;auto large=mapGenGenerateCandidate(o).map;MapRenderPlanRequest request;request.applyFog=false;request.layers=MapRenderPlanAll;
 const auto start=std::chrono::steady_clock::now();const auto plan=compileMapRenderPlan(large,request);const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"Doors checked="<<doorsTested<<"; 80x56 plan ms="<<ms<<" ink="<<plan.atlasInk.size()+plan.atlasFeatureInk.size()<<'\n';
 std::cout<<(ok?"Dungeon generation11 regressions passed\n":"Dungeon generation11 regressions FAILED\n");return ok?0:1;
}
