#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapWorldLandform.hpp"
#include "Models/Map/MapOverviewLabelLod.hpp"
#include <iostream>
#include <stdexcept>
static void check(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
int main(){try{
for(unsigned seed=1;seed<=100;++seed)for(auto form:{MapLandform::Continental,MapLandform::InlandLake,MapLandform::Islands}){
  int land=0,water=0;
  for(int y=0;y<50;++y)for(int x=0;x<70;++x){float score=mapWorldLandformScore(form,seed,(x+.5f)/35.f-1.f,(y+.5f)/25.f-1.f,50);land+=score>=0;water+=score<0;}
  check(land>0,"landform has no land");check(form==MapLandform::Continental ? water==0 : water>0,"water mask does not match selected type");
  if(form!=MapLandform::Islands)for(int i=0;i<51;++i)for(float side:{-1.f,1.f})
    check(mapWorldLandformScore(form,seed,side,i/25.f-1.f,100)>0 && mapWorldLandformScore(form,seed,i/25.f-1.f,side,100)>0,"inland boundary reaches ocean");
  if(form==MapLandform::Islands)for(int i=0;i<51;++i)for(float side:{-1.f,1.f})
    check(mapWorldLandformScore(form,seed,side,i/25.f-1.f,0)<0 && mapWorldLandformScore(form,seed,i/25.f-1.f,side,0)<0,"island touches map frame");
}
// Every new regional basin remains enclosed even at maximum water. The
// optional mask cannot change the default/legacy shape contract.
// New choices must keep their defining topology at both ends of the slider.
// Compare connected land against all land rather than accepting scattered
// islands that merely have a plausible coverage percentage.
for(unsigned seed=1;seed<=100;++seed)for(auto form:{MapLandform::SingleIsland,MapLandform::Coast,MapLandform::Peninsula}){
  int previousLand=10000;
  for(int waterLevel:{0,50,100}){
    constexpr int side=64;
    std::vector<bool> land(side*side),seen(side*side,false);
    int count=0,boundaryLand=0;
    for(int y=0;y<side;++y)for(int x=0;x<side;++x){
      const float nx=(x+.5f)*2.f/side-1.f,ny=(y+.5f)*2.f/side-1.f;
      const float score=mapWorldLandformScore(form,seed,nx,ny,waterLevel);
      check(std::isfinite(score)&&score==mapWorldLandformScore(form,seed,nx,ny,waterLevel),"new landform is not deterministic");
      land[y*side+x]=score>=0;count+=land[y*side+x]?1:0;
      if(x==0||x==side-1||y==0||y==side-1)boundaryLand+=land[y*side+x]?1:0;
    }
    check(count>side*side/6&&count<side*side*3/4,"new landform has unusable coverage");
    check(count<=previousLand,"more sea unexpectedly adds land");previousLand=count;
    if(form==MapLandform::SingleIsland)check(boundaryLand==0,"single island reaches the frame");
    else check(boundaryLand>=side&&boundaryLand<side*3,"coast/peninsula loses its mainland or sea edge");
    const auto first=std::find(land.begin(),land.end(),true);
    std::vector<int> pending={static_cast<int>(std::distance(land.begin(),first))};seen[pending.front()]=true;
    for(std::size_t i=0;i<pending.size();++i){const int x=pending[i]%side,y=pending[i]/side;
      for(const auto d:std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}){const int nx=x+d.first,ny=y+d.second;
        if(nx>=0&&ny>=0&&nx<side&&ny<side){const int n=ny*side+nx;if(land[n]&&!seen[n]){seen[n]=true;pending.push_back(n);}}}
    }
    check(static_cast<int>(pending.size())==count,"new mainland shape is disconnected");
  }
}
for(unsigned seed=1;seed<=100;++seed){
  for(int i=0;i<51;++i)for(float side:{-1.f,1.f})
    check(mapWorldLandformScore(MapLandform::InlandLake,seed,side,i/25.f-1.f,100,true)>0 &&
          mapWorldLandformScore(MapLandform::InlandLake,seed,i/25.f-1.f,side,100,true)>0,"regional basin opens into ocean");
  check(mapWorldLandformScore(MapLandform::InlandLake,seed,0,0,50,true)<0,"regional basin loses its main lake");
}
MapCreationRequest request;request.generatorVersion=11;request.landform=MapLandform::InlandLake;request.waterLevel=81;request.forestDensity=72;request.mountainDensity=23;request.routeDensity=0;
auto options=mapGenerationOptionsFromCreationRequest(request);check(options.landform==request.landform&&options.waterLevel==81&&options.forestDensity==72&&options.mountainDensity==23&&options.routeDensity==0,"request controls not transferred");
request.generatorVersion=10;check(mapGenerationOptionsFromCreationRequest(request).waterLevel==20,"legacy request defaults changed");
options.generatorVersion=11;options.width=48;options.height=34;options.scope=MapScope::World;options.scale=MapLevel::World;options.styleId="illustrated_atlas";options.qualityAttempts=24;options.seed=543;options.landform=MapLandform::Continental;options.waterLevel=0;options.mountainDensity=0;options.forestDensity=0;options.routeDensity=0;options.requestedSettlements=5;
auto batch=generateSemanticMapCandidateBatch(options,1);check(batch.succeeded(),"zero controls must produce a valid inland map");
const auto &zero=batch.candidates.front().map;
auto compact=zero;compact.width=72;compact.height=48;
check(gm::map_render::overviewLabelBoxFitsMap(compact,0.f,0.f,72.f,48.f),"canvas-sized label incorrectly rejected");
check(!gm::map_render::overviewLabelBoxFitsMap(compact,71.f,4.f,74.f,6.f),"right-edge label leaks outside map");
check(!gm::map_render::overviewLabelBoxFitsMap(compact,-1.f,4.f,3.f,6.f),"left-edge label leaks outside map");
check(gm::map_render::overviewTerritoryLabelFontSize(compact,309.f/72.f,30.f,220.f)==0.f,"tiny overview keeps dominant nation label");
const float boundedLabel=gm::map_render::overviewTerritoryLabelFontSize(compact,6.f,30.f,220.f);
check(boundedLabel>=11.f&&boundedLabel<17.f,"nation label exceeds compact width budget");
check(gm::map_render::overviewTerritoryLabelFontSize(compact,6.f,30.f,600.f)==0.f,"long label shrinks below readability");
check(gm::map_render::overviewTerritoryLabelFontSize(compact,20.f,30.f,220.f)==30.f,"full-size nation label unnecessarily changed");
compact.generation.recipe.generatorVersion=10;check(gm::map_render::overviewTerritoryLabelFontSize(compact,4.f,30.f,220.f)==30.f,"historical label rendering changed");
check(gm::map_render::overviewLabelBoxFitsMap(compact,-1.f,0.f,74.f,50.f),"historical label placement changed");
check(mapGenV11RoadBoundaryAnchor(zero,{0,10}).x<.02f&&mapGenV11RoadBoundaryAnchor(zero,{47,10}).x>47.98f,"road exit stops before frame");
check(mapGenV11RoadBoundaryAnchor(zero,{10,33}).y>33.98f&&mapGenV11RoadBoundaryAnchor(zero,{10,10}).x==10.5f,"boundary anchoring moved interior road");
MapGeneratedFeature shortRoad;shortRoad.kind=MapGeneratedFeatureKind::Road;shortRoad.points={{10.f,10.f},{15.f,10.f}};
mapGenV7FitOverviewRoadPathFeature(zero,shortRoad);check(shortRoad.points.front().x==10.f&&shortRoad.points.front().y==10.f&&shortRoad.points.back().x==15.f&&shortRoad.points.back().y==10.f,"road fitting moved endpoints");
bool curved=false;for(std::size_t i=1;i<shortRoad.points.size();++i){curved=curved||std::abs(shortRoad.points[i].y-10.f)>.05f;check(mapGenV7RoadChordIsSafe(zero,shortRoad.points[i-1],shortRoad.points[i]),"short road fitting left safe terrain");}check(curved,"medium road remains an artificial straight chord");
MapFeatureGraph junctionGraph;MapGeneratedFeature trunk;trunk.id="trunk";trunk.kind=MapGeneratedFeatureKind::Road;trunk.points={{5.f,10.f},{20.f,10.f}};junctionGraph.features.push_back(trunk);
MapGeneratedFeature branch;branch.id="branch";branch.kind=MapGeneratedFeatureKind::Road;branch.points={{10.f,15.f},{11.f,12.f},{12.f,10.55f},{15.f,9.9f},{18.f,10.f}};
mapGenV7JoinFirstExistingRoad(branch,junctionGraph,nullptr,&zero);check(branch.points.size()==3&&std::abs(branch.points.back().x-12.f)<.01f&&branch.points.back().y==10.f,"near-junction leaves a thin road lens");
for(const auto &cell:zero.cells)check(cell.kind!=MapTileKind::Forest&&cell.kind!=MapTileKind::Mountain&&cell.kind!=MapTileKind::Hill&&cell.kind!=MapTileKind::Water&&!mapGenWayKindIsRoadLike(cell.wayKind),"zero control leaked terrain or roads");
check(zero.generation.recipe.waterLevel==0&&zero.generation.recipe.routeDensity==0,"control recipe lost values");
auto broken=zero;for(auto &c:broken.cells)applyMapCellTerrainDefaults(c,MapTileKind::Water);check(mapGenV5WorldCoastCompositionPercent(broken)<80,"invalid all-ocean inland map accepted");
auto geometric=zero;
for(int n=0;n<2;++n){MapGeneratedFeature territory;territory.kind=MapGeneratedFeatureKind::TerrainRegion;territory.materialId="fantasy_neutral.territory."+std::to_string(n);territory.name=n?"East":"West";territory.closed=true;float left=n?24.f:0.f,right=n?48.f:24.f;territory.points={{left,0.f},{right,0.f},{right,34.f},{left,34.f}};geometric.generation.graph.features.push_back(territory);}
auto geometryQuality=analyzeMapGenerationQuality(geometric);bool warned=false;
for(const auto&i:geometryQuality.issues)if(i.code=="territory.boundaries_too_geometric"){check(i.severity==MapGenerationIssueSeverity::Warning,"V11 aesthetic border incorrectly blocks generation");warned=true;}
check(warned,"aesthetic border diagnostic disappeared");
check(mapGenV5WorldTerritoryGeographyPercent(geometric)==100,"aesthetic warning still fails structural geography floor");
geometric.generation.recipe.generatorVersion=10;bool legacyCritical=false;for(const auto&i:analyzeMapGenerationQuality(geometric).issues)if(i.code=="territory.boundaries_too_geometric")legacyCritical=i.severity==MapGenerationIssueSeverity::Critical;check(legacyCritical,"legacy border gate changed");
for(auto scale:{MapLevel::World,MapLevel::Region})for(auto form:{MapLandform::Continental,MapLandform::Islands,MapLandform::InlandLake,MapLandform::SingleIsland,MapLandform::Coast,MapLandform::Peninsula}){
  options.scale=scale;options.scope=scale==MapLevel::World?MapScope::World:MapScope::Local;
  options.landform=form;options.waterLevel=55;options.mountainDensity=25;options.forestDensity=40;options.routeDensity=30;options.requestedSettlements=6;options.facts.clear();options.constraints.clear();options.nationNames={"Arven","Beloria"};
  for(const std::string name:{"Arven","Beloria"}){MapGenerationFact fact;fact.id=name;fact.label=name;fact.value=name;fact.type=MapGenerationFactType::Nation;fact.sourceText="map_setup:existing";fact.entityRefs={"nation_"+name};fact.strength=MapGenerationFactStrength::Must;options.facts.push_back(fact);}
  for(const std::string name:{"Havenfall","Stoneford"}){MapGenerationFact fact;fact.id=name;fact.label=name;fact.value=name;fact.type=MapGenerationFactType::Settlement;fact.sourceText="map_setup:existing";fact.entityRefs={"city_"+name,name=="Havenfall"?"nation_Arven":"nation_Beloria"};fact.strength=MapGenerationFactStrength::Must;options.facts.push_back(fact);MapGenerationConstraint c;c.id=name;c.label=name;c.featureKind=MapGeneratedFeatureKind::Settlement;c.required=true;c.locked=false;options.constraints.push_back(c);}
  options.seed=10877231;auto named=generateSemanticMapCandidateBatch(options,1);check(named.succeeded(),"named landform request not eligible");for(const auto &f:options.facts)check(mapGenFactIsSatisfied(named.candidates.front().map,f),"named fact or authored nation binding lost");
  auto misplaced=named.candidates.front().map;
  for(auto &feature:misplaced.generation.graph.features)if(feature.kind==MapGeneratedFeatureKind::Settlement&&feature.name=="Havenfall")feature.points.front()={-100.f,-100.f};
  check(!mapGenFactIsSatisfied(misplaced,options.facts[2]),"misplaced authored nation city accepted");
  auto unselected=options.facts;unselected.erase(unselected.begin());
  check(mapGenV11AuthoredSettlementNationName(unselected,"Havenfall").empty(),"unselected nation imposed a territory constraint");
}
std::cout<<"1200 mask cases, request versioning, zero controls, invalid inland rejection and 12 named World/Region contracts with authored nation bindings passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
