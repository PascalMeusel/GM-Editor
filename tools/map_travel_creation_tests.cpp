#include "Models/Map/MapTravelCreation.hpp"
#include "Models/Persistence/PersistenceSerialization.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace {
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::string signature(const MapModel &map){std::ostringstream out;for(const auto &c:map.cells)out<<int(c.kind)<<':'<<int(c.wayKind)<<':'<<c.elevation<<';';for(const auto &f:map.generation.graph.features){out<<f.id<<f.materialId;for(const auto&p:f.points)out<<p.x<<','<<p.y<<';';}return out.str();}
}
int main(){try {
 MapGenerationOptions options;options.creationIntent=MapCreationIntent::TravelScene;options.width=36;options.height=26;
 options.seed=71591;options.contentLanguage="de";options.nameCulture="neutral_fantasy";
 for(int biome=0;biome<6;++biome){
  const auto recipe=buildStandaloneTravelRecipe(options,biome%5,biome,2,false);
  require(recipe.schemaVersion==3 && recipe.width==36 && recipe.height==26 && recipe.seed==71591 &&
   recipe.contentLanguage=="de" && recipe.biome==kStandaloneTravelBiomes[biome],"adapter did not preserve user request");
  const auto batch=generateStandaloneTravelMapBatch(options,recipe,1);
  require(batch.succeeded() && batch.recommendedIndex==0,"structured travel choice produced no eligible composed candidate");
  const auto &map=batch.candidates[0].map;
  require(map.width==36 && map.height==26 && map.generation.recipe.generatorVersion==11 &&
   map.creationIntent==MapCreationIntent::TravelScene && map.parentMapId.empty() && map.atlasPlacement.parentId.empty() &&
   map.atlasStatus==MapAtlasStatus::Standalone,"standalone map dimensions/version/parent contract failed");
  require(map.generation.recipe.travelScenePurpose==biome%5 && map.generation.recipe.travelSceneBiome==recipe.biome &&
   map.generation.recipe.travelSceneSeed==71591 && map.visibility==MapVisibility::GameMasterOnly,
   "travel settings/requested seed/privacy were lost");
  require(std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[](const auto&f){return f.id.find("travel_focus")!=std::string::npos;}),
   "structured travel bypassed the reviewed purpose composer");
  if(!biome){const auto repeat=generateStandaloneTravelMapBatch(options,recipe,1);require(repeat.succeeded() && signature(repeat.candidates[0].map)==signature(map),"standalone request not deterministic");}
 }
 auto current=options;current.generatorVersion=12;
 for(int biome=0;biome<7;++biome){
  const auto currentRecipe=buildStandaloneTravelRecipe(current,biome%5,biome,2,false);
  require(currentRecipe.schemaVersion==4,"new authoring adapter still selects legacy travel art");
  const auto currentBatch=generateStandaloneTravelMapBatch(current,currentRecipe,1);
  require(currentBatch.succeeded(),"V12 structured travel choice rejected by production adapter");
  const auto &m=currentBatch.candidates.front().map;
  require(m.generation.recipe.generatorVersion==12 && m.generation.recipe.styleVersion==6 &&
          m.generation.recipe.styleSnapshotId=="illustrated_atlas_v6","new travel style lost between structured UI and generator");
 }
 auto recipe=buildStandaloneTravelRecipe(options,2,0,1,false);
 auto designs=generateStandaloneTravelMapBatch(options,recipe,3);require(designs.succeeded() && designs.candidates.size()==3,"three-design authoring batch failed");
 int progress=0;options.cancellationRequested=[&]{return progress>0;};options.progressCallback=[&](const auto&){++progress;};
 auto cancelled=generateStandaloneTravelMapBatch(options,recipe,3);require(cancelled.failure==MapGenerationBatchFailureCode::Cancelled && cancelled.candidates.empty() && cancelled.recommendedIndex==-1,"cancelled adapter exposed partial designs");
 options.cancellationRequested={};options.progressCallback={};recipe.width=81;
 auto invalid=generateStandaloneTravelMapBatch(options,recipe,1);require(invalid.failure==MapGenerationBatchFailureCode::InvalidRequest && invalid.candidates.empty(),"unsupported dimensions were silently changed");
 MapGenerationMetadata metadata;metadata.enabled=true;storeStandaloneTravelRecipe(metadata.recipe,recipe);
 const auto encoded=PersistenceSerialization::serializeMapGenerationMetadata(metadata);
 const auto restored=PersistenceSerialization::deserializeMapGenerationMetadata(encoded);
 require(restored.recipe.travelScenePurpose==2 && restored.recipe.travelSceneBiome=="Forest" &&
   restored.recipe.travelSceneSeed==71591,"Travel recipe did not survive actual persistence codec");
 auto historical=encoded;const auto lineEnd=historical.find('\n');
 auto cut=lineEnd;for(int field=0;field<5;++field)cut=historical.rfind('\x1f',cut-1);
 historical.erase(cut,lineEnd-cut);
 const auto old=PersistenceSerialization::deserializeMapGenerationMetadata(historical);
 require(old.recipe.travelScenePurpose==-1 && old.recipe.travelSceneBiome.empty(),"historical codec invented Travel choices");
 MapGenerationRecipe legacy;require(legacy.travelScenePurpose==-1,"legacy recipe must not opt into structured regeneration");
 std::cout<<"Structured Travel adapter contracts passed (six biomes, reviewed focus, standalone/privacy, dimensions/seed, three designs, repeat, cancel).\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
