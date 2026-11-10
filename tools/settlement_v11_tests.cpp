#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include <iostream>
#include <chrono>
bool demandSettlement(bool value,const char* message){if(!value)std::cerr<<message<<'\n';return value;}
int main(){
 bool ok=true;int accesses=0;MapGenerationOptions o;o.generatorVersion=11;o.scope=MapScope::Local;o.scale=MapLevel::Site;o.creationIntent=MapCreationIntent::Settlement;o.purpose=MapGenerationPurpose::SettlementEnvirons;o.qualityAttempts=1;o.settlementDensity=60;o.forestDensity=25;
 for(unsigned seed=1;seed<=48;++seed){
  o.seed=seed*9719;o.width=24+(seed%6)*8;o.height=20+(seed%4)*7;
  auto a=mapGenGenerateCandidate(o).map,b=mapGenGenerateCandidate(o).map;
  for(std::size_t i=0;i<a.cells.size();++i)ok&=demandSettlement(a.cells[i].kind==b.cells[i].kind&&a.cells[i].wayKind==b.cells[i].wayKind,"Settlement not deterministic");
  for(const auto &f:a.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Road)
   for(auto p:f.points){const auto*c=mapCellAt(a,int(p.x),int(p.y));ok&=demandSettlement(c&&c->kind!=MapTileKind::Wall,"A town wall blocks a generated street");}
  for(const auto &f:a.generation.graph.features){
   if(f.id.find("house_access")==std::string::npos)continue;++accesses;
   ok&=demandSettlement(f.kind==MapGeneratedFeatureKind::Trail&&f.points.size()>=2,"Missing access geometry");
   for(std::size_t j=1;j<f.points.size();++j){auto p=f.points[j];auto*c=mapCellAt(a,int(p.x),int(p.y));ok&=demandSettlement(c&&c->kind!=MapTileKind::Building&&c->kind!=MapTileKind::Wall&&c->kind!=MapTileKind::Water&&c->kind!=MapTileKind::Bridge,"House approach crosses blocked ground");}
   auto end=f.points.back();auto*c=mapCellAt(a,int(end.x),int(end.y));ok&=demandSettlement(c&&mapGenWayKindIsRoadLike(c->wayKind),"House approach ends away from street");
  }
  auto legacy=o;legacy.generatorVersion=10;auto old=mapGenGenerateCandidate(legacy).map;
  for(const auto& f:old.generation.graph.features)ok&=demandSettlement(f.id.find("house_access")==std::string::npos,"Legacy map receives new house approach");
 }
 for(unsigned archetype=0;archetype<6;++archetype){
  auto low=o;low.seed=archetype*17+1;low.width=48;low.height=34;low.routeDensity=0;low.forestDensity=0;
  auto high=low;high.routeDensity=100;auto a=mapGenGenerateCandidate(low).map,b=mapGenGenerateCandidate(high).map;
  auto countRoads=[](const MapModel&m){int n=0;for(const auto&f:m.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Road)++n;return n;};
  ok&=demandSettlement(countRoads(a)>=2&&countRoads(b)>countRoads(a),"Street complexity does not change extra streets");
  auto searched=low;searched.qualityAttempts=6;auto chosen=generateSemanticMap(searched).map;
  ok&=demandSettlement((chosen.generation.recipe.seed/17u)%6u==archetype,"Quality search changes settlement layout family");
  for(const auto &c:a.cells)ok&=demandSettlement(c.kind!=MapTileKind::Forest,"Zero woodland ignored");
  if(archetype==4){int walls=0;for(const auto&c:b.cells)if(c.kind==MapTileKind::Wall)++walls;ok&=demandSettlement(walls>25,"Fortified recipe lacks walls");}
  if(archetype==2)for(const auto&f:b.generation.graph.features)if(f.name=="Crafts Lane")
   for(std::size_t j=1;j<f.points.size();++j)ok&=demandSettlement(f.points[j].x>=f.points[j-1].x-.01f,"River lane folds back at bank junction");
  if(archetype==2){int bridges=0;for(const auto&f:b.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Bridge)++bridges;ok&=demandSettlement(bridges<=3,"Every secondary lane adds a redundant bridge");}

 }
 {
  auto civic=o;civic.seed=69;civic.width=48;civic.height=34;civic.routeDensity=65;
  auto named=mapGenGenerateCandidate(civic).map,renamed=named;
  for(auto&f:renamed.generation.graph.features) f.name="User renamed building";
  auto first=gm::map_render::buildTacticalStructureInk(named),second=gm::map_render::buildTacticalStructureInk(renamed);
  ok&=demandSettlement(first.size()==second.size(),"Renaming changes building roof geometry");
  for(std::size_t i=0;i<std::min(first.size(),second.size());++i){
   ok&=demandSettlement(first[i].points.size()==second[i].points.size(),"Renaming changes roof vertices");
   for(std::size_t j=0;j<std::min(first[i].points.size(),second[i].points.size());++j)
    ok&=demandSettlement(first[i].points[j].x==second[i].points[j].x&&first[i].points[j].y==second[i].points[j].y,"Renaming moves roof vertices");
  }
  for(auto&c:renamed.cells)c.fogged=true;
  renamed.generation.graph.features.clear();
  auto hidden=gm::map_render::buildSettlementCivicInk(renamed);
  ok&=demandSettlement(hidden.empty(),"Hidden settlement walls emit ink");
 }
 {
  auto small=o;small.seed=35;small.width=24;small.height=20;small.routeDensity=100;
  auto village=mapGenGenerateCandidate(small).map;int bridges=0;
  for(const auto&f:village.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Bridge)++bridges;
  ok&=demandSettlement(bridges==1,"Tiny river village gets redundant crossings");
 }
 {
  auto compact=o;compact.seed=1734851478u;compact.width=20;compact.height=17;compact.settlementDensity=35;compact.forestDensity=15;compact.routeDensity=65;
  auto town=mapGenGenerateCandidate(compact).map;int houses=0;
  for(const auto&f:town.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::BuildingGroup)++houses;
  ok&=demandSettlement(houses>=5,"Compact fortified layout has only two houses");
 }
 ok&=demandSettlement(accesses>100,"House approaches missing");
 o.width=80;o.height=56;o.seed=98765;auto a=mapGenGenerateCandidate(o).map;MapRenderPlanRequest r;r.applyFog=false;r.layers=MapRenderPlanAll;auto start=std::chrono::steady_clock::now();auto plan=compileMapRenderPlan(a,r);auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"Accesses checked="<<accesses<<"; 80x56 render-plan ms="<<ms<<'\n'<<(ok?"Settlement v11 tests passed\n":"Settlement v11 tests FAILED\n");return ok?0:1;
}
