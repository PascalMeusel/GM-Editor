#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include <iostream>

bool check(bool value,const char *message) { if(!value) std::cerr << message << '\n'; return value; }
bool sameGraph(const MapFeatureGraph &a,const MapFeatureGraph &b) {
 if(a.features.size()!=b.features.size())return false;
 for(std::size_t i=0;i<a.features.size();++i) {
  const auto &x=a.features[i],&y=b.features[i];
  if(x.id!=y.id || x.kind!=y.kind || x.materialId!=y.materialId || x.closed!=y.closed || x.points.size()!=y.points.size())return false;
  for(std::size_t j=0;j<x.points.size();++j) if(x.points[j].x!=y.points[j].x || x.points[j].y!=y.points[j].y)return false;
 }
 return true;
}
int main() {
 bool ok=true; MapGenerationOptions o;
 o.generatorVersion=10;o.scope=MapScope::Dungeon;o.scale=MapLevel::Tactical;o.creationIntent=MapCreationIntent::Dungeon;
 o.width=48;o.height=36;o.qualityAttempts=1;o.tacticalDensity=35;o.routeDensity=45;
 for(unsigned seed:{1u,9u,772u,592271770u,775699284u,803646864u,2030771426u}) {
  o.seed=seed;
  auto a=mapGenGenerateCandidate(o).map, b=mapGenGenerateCandidate(o).map;
  ok&=check(sameGraph(a.generation.graph,b.generation.graph),"V10 dungeon graph is not deterministic");
  ok&=check(a.cells.size()==b.cells.size(),"Repeated grid dimensions differ");
  std::vector<MapCellCoordinate> stairs;
  int rooms=0,corridors=0,shaped=0;
  for(const auto &f:a.generation.graph.features) {
   if(f.materialId=="fantasy_neutral.dungeon.room") {++rooms;if(f.points.size()>4)++shaped;}
   if(f.materialId=="fantasy_neutral.dungeon.corridor")++corridors;
  }
  ok&=check(rooms>8,"Large dungeon still constrained by old eight-room cap");
  ok&=check(corridors>=rooms,"V10 nonzero route density should add alternate approach loops");
  for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x) {
   const auto &cell=*mapCellAt(a,x,y); const auto &other=*mapCellAt(b,x,y);
   ok&=check(cell.kind==other.kind && cell.wayKind==other.wayKind,"Repeated dungeon terrain differs");
   if(cell.kind==MapTileKind::Cover || cell.kind==MapTileKind::Rubble || cell.kind==MapTileKind::Hazard || cell.kind==MapTileKind::Pit) {
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
     const auto *n=mapCellAt(a,x+dx,y+dy);
     ok&=check(n && n->kind!=MapTileKind::Wall && n->kind!=MapTileKind::Empty && n->kind!=MapTileKind::Door && n->kind!=MapTileKind::Stairs,"Dungeon prop has no floor margin or obstructs an entrance");
    }
   }
   if(cell.kind==MapTileKind::Stairs) stairs.push_back({x,y});
   if(cell.kind==MapTileKind::Door) {
    int approaches=0;
    for(auto d: {MapCellCoordinate{1,0},{-1,0},{0,1},{0,-1}}) {
     const auto *n=mapCellAt(a,x+d.x,y+d.y);
     if(n && n->movementCost<99 && n->wayKind==MapWayKind::DungeonCorridor) ++approaches;
    }
    ok&=check(approaches>=2,"Door is detached from the actual corridor traversal");
   }
  }
  ok&=check(stairs.size()==2,"Dungeon must preserve two distinct stairs");
  if(stairs.size()==2) ok&=check(mapGenCoordinatesConnected(a,stairs[0],stairs[1],false),"Dungeon stairs are disconnected");
  ok&=check(!gm::map_render::buildDungeonArchitectureInk(a,false).empty(),"New door/stair ink missing");
  for(auto &c:a.cells)c.fogged=true;
  ok&=check(gm::map_render::buildDungeonArchitectureInk(a,true).empty(),"Unrevealed dungeon architecture leaks ink");
  a.generation.recipe.generatorVersion=9;
  ok&=check(gm::map_render::buildDungeonArchitectureInk(a,false).empty(),"Historical dungeon received new artwork");
 }
 o.seed=772;o.routeDensity=0;auto tree=mapGenGenerateCandidate(o).map;int rooms=0,corridors=0;
 for(const auto &f:tree.generation.graph.features) {rooms+=f.materialId=="fantasy_neutral.dungeon.room";corridors+=f.materialId=="fantasy_neutral.dungeon.corridor";}
 ok&=check(corridors==rooms-1,"Zero route density unexpectedly added optional loops");
 o.generatorVersion=9;o.routeDensity=45;auto old=mapGenGenerateCandidate(o).map;rooms=0;corridors=0;
 for(const auto &f:old.generation.graph.features) {rooms+=f.materialId=="fantasy_neutral.dungeon.room";corridors+=f.materialId=="fantasy_neutral.dungeon.corridor";}
 ok&=check(rooms<=8 && corridors==rooms-1,"Historical Dungeon room/corridor contract changed");
 std::cout<<(ok?"Dungeon V10 regressions passed\n":"Dungeon V10 regressions FAILED\n");return ok?0:1;
}
