#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include <iostream>
#include <cmath>
#include <set>
namespace {
bool require(bool v,const char *msg){if(!v)std::cerr<<"FAIL "<<msg<<'\n';return v;}
MapGenerationOptions recipe(unsigned seed,int width=48,int height=34){MapGenerationOptions o;o.generatorVersion=10;o.seed=seed;o.width=width;o.height=height;o.scope=MapScope::Local;o.scale=MapLevel::Site;o.creationIntent=MapCreationIntent::Settlement;o.purpose=MapGenerationPurpose::SettlementEnvirons;o.settlementDensity=55;o.forestDensity=30;o.qualityAttempts=1;o.requestedLandmarks=3;return o;}
std::uint64_t fingerprint(const MapModel &map){std::uint64_t h=14695981039346656037ull;auto add=[&](std::uint64_t n){h=(h^n)*1099511628211ull;};for(auto c:map.cells){add(int(c.kind));add(int(c.wayKind));add(c.blocksLineOfSight);add(c.movementCost);}for(auto f:map.generation.graph.features){add(int(f.kind));for(auto c:f.name)add(static_cast<unsigned char>(c));for(auto p:f.points){add(std::lround(p.x*10000));add(std::lround(p.y*10000));}}return h;}
int buildings(const MapModel &map){return int(std::count_if(map.generation.graph.features.begin(),map.generation.graph.features.end(),[](const auto &f){return f.kind==MapGeneratedFeatureKind::BuildingGroup;}));}
}
int main(){bool ok=true;
 for(unsigned seed:{1911346733u,688522289u,295976336u,1567162392u,1399515144u}){
  auto o=recipe(seed,seed==688522289u?56:52,38);auto result=generateSemanticMapCandidateBatch(o,1);
  if(result.candidates.empty()) {std::cerr<<"Regression seed "<<seed<<" rejected\n";for(const auto &r:result.rejectedCandidates)for(const auto &i:r.quality.issues)std::cerr<<i.code<<'\n';}
  ok&=require(!result.candidates.empty(),"R02 river/coast/ring failures replay as eligible without relaxed gates");
 }
 for(unsigned i=0;i<60;++i){auto o=recipe(78217u+i*7919u,i%3==0?32:i%3==1?48:64,i%3==0?24:i%3==1?34:48);o.edgeConnectionCount=int(i%4)+1;o.forestDensity=int(i%3)*35;o.settlementDensity=20+int(i%5)*18;
  auto result=mapGenGenerateCandidate(o);const auto quality=analyzeMapGenerationQuality(result.map);auto validation=validateMapGeneration(result.map,quality);
  if(!validation.eligible || !mapGenQualityMeetsCandidateFloor(result.map,quality)){std::cerr<<"Sweep seed="<<o.seed<<" width="<<o.width<<" forest="<<o.forestDensity<<" density="<<o.settlementDensity<<" score="<<quality.score<<" roads="<<quality.wayConnectivityPercent<<" bridge="<<quality.bridgeValidityPercent<<" hierarchy="<<quality.visualHierarchyPercent<<'\n';for(auto issue:quality.issues)std::cerr<<issue.code<<'\n';}
  ok&=require(validation.eligible && mapGenQualityMeetsCandidateFloor(result.map,quality),"settlement seed/size/purpose sweep passes structural gates");
  ok&=require(buildings(result.map)>4,"settlement has usable housing fabric");
  const auto repeat=mapGenGenerateCandidate(o);ok&=require(fingerprint(result.map)==fingerprint(repeat.map),"generation and named geometry are deterministic");
  int edges=0;for(int edge=0;edge<4;++edge){bool road=false;for(auto f:result.map.generation.graph.features)if(f.kind==MapGeneratedFeatureKind::Road)for(auto p:f.points)road=road || (edge==0?p.x<1:edge==1?p.x>o.width-1:edge==2?p.y<1:p.y>o.height-1);edges+=road;}
  ok&=require(edges==std::min(o.edgeConnectionCount,(o.seed/17u)%6u==3?3:4),"external approaches honor the request and coastal edge");
  for(const auto &primitive:gm::map_render::buildTacticalStructureInk(result.map))for(auto p:primitive.points){const auto*c=mapCellAt(result.map,int(std::floor(p.x)),int(std::floor(p.y)));ok&=require(c&&c->kind==MapTileKind::Building,"settlement roof stays in actual blocked cells");}
 }
 auto low=recipe(717939956u,64,48);low.settlementDensity=10;auto high=low;high.settlementDensity=90;
 ok&=require(buildings(mapGenGenerateCandidate(high).map)>buildings(mapGenGenerateCandidate(low).map),"dense city contains more separate buildings than sparse town");
 auto german=recipe(1911346733u);german.contentLanguage="de";const auto germanMap=mapGenGenerateCandidate(german).map;
 ok&=require(std::any_of(germanMap.generation.graph.features.begin(),germanMap.generation.graph.features.end(),[](const auto &f){return f.name==u8"Alte Marktstra\u00dfe";}),"German generated street name uses correct UTF-8 sharp-s");
 ok&=require(germanMap.description.find(u8"Hausgrundst\u00fccken")!=std::string::npos,"German generated description uses correct UTF-8 umlaut");
 auto diagonalRiver=recipe(241910789u,52,38);const auto diagonalMap=mapGenGenerateCandidate(diagonalRiver).map;
 ok&=require(std::count_if(diagonalMap.generation.graph.features.begin(),diagonalMap.generation.graph.features.end(),[](const auto &f){return f.kind==MapGeneratedFeatureKind::Bridge;})==3,"one graph bridge per river crossing, without stacked per-row stamps");
 auto older=low;older.generatorVersion=9;auto legacy=mapGenGenerateCandidate(older).map;
 ok&=require(gm::map_render::buildTacticalStructureInk(legacy).empty() && gm::map_render::buildSettlementCivicInk(legacy).empty() && gm::map_render::buildSettlementWoodlandInk(legacy).empty(),"old saved settlement style remains unchanged");
 MapModel building;building.width=7;building.height=6;building.atlasLevel=MapLevel::Site;building.mapKind=MapKind::Settlement;building.generation.recipe.generatorVersion=10;building.generation.enabled=true;building.generation.continuousRendering=true;resizeMapCells(building);for(auto &c:building.cells)c.fogged=false;
 for(auto p:std::vector<MapCellCoordinate>{{1,1},{2,1},{1,2}})applyMapCellTerrainDefaults(*mapCellAt(building,p.x,p.y),MapTileKind::Building);
 MapGeneratedFeature hidden;hidden.id="secret_house";hidden.kind=MapGeneratedFeatureKind::BuildingGroup;hidden.gmOnly=true;hidden.points={{1.5f,1.5f}};building.generation.graph.features.push_back(hidden);
 building.visibility=MapVisibility::PlayersVisible;building.playerRelease.placesAndMarkers=true;
 MapRenderPlanRequest request;request.audience=MapRenderAudience::Player;request.layers=MapRenderPlanSymbols;request.applyFog=false;
 ok&=require(compileMapRenderPlan(building,request).atlasFeatureInk.empty(),"private site building hides its complete edited L footprint");
 applyMapCellTerrainDefaults(*mapCellAt(building,5,1),MapTileKind::Building);const auto plan=compileMapRenderPlan(building,request);
 ok&=require(!plan.atlasFeatureInk.empty(),"separate public site house survives hidden-building filtering");for(auto p:plan.atlasFeatureInk)for(auto q:p.points)ok&=require(q.x>=5.f,"no old private roof fragment survives");
 building.atlasLevel=MapLevel::Region;ok&=require(!gm::map_render::supportsFootprintRoofInk(building),"ordinary regional map does not inherit site rendering");
 std::cout<<(ok?"settlement_v10_tests passed\n":"settlement_v10_tests failed\n");return ok?0:1;
}
