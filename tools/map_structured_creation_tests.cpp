#include "Models/Map/MapStructuredCreation.hpp"
#include "Models/Map/MapEditTransactions.hpp"
#include "Models/Session/PlayerDisplayReviewService.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorPersistencePolicy.hpp"
#include <iostream>

bool check(bool value,const char *message) { if(!value) std::cerr<<message<<'\n'; return value; }

int main()
{
    bool ok=true;
    {
        MapModel previous, generated;
        previous.metadata.customProperties["gm.note"] = "Preserve my annotation";
        generated.metadata.customProperties["cave.surface_entrances"] = "0,8;23,15";
        generated.metadata.customProperties["cave.composition_revision"] = "surface_access_1";
        retainMapAuthoredMetadata(generated, previous);
        ok &= check(generated.metadata.id == previous.metadata.id &&
            generated.metadata.customProperties.at("gm.note") == "Preserve my annotation" &&
            generated.metadata.customProperties.at("cave.surface_entrances") == "0,8;23,15",
            "Accepting a cave design must preserve identity and retain the generated surface openings");
        MapModel replacement;
        retainMapAuthoredMetadata(replacement, generated);
        ok &= check(!replacement.metadata.customProperties.count("cave.surface_entrances") &&
            replacement.metadata.customProperties.count("gm.note"),
            "Accepting another design must clear stale geometry annotations, preserving authored notes");
    }
    Campaign campaign;
    WorldPlaceModel established;
    established.name="Havenfall"; established.type=WorldPlaceType::Town;
    established.description="Keep authored details"; established.visibility=WorldObjectVisibility::Public;
    established.linkedMapId="original-world"; established.linkedFeatureId="original-marker";
    established.hasMapPosition=true; established.mapPosition={.25,.75};
    campaign.worldPlaces.push_back(established);
    MapSetupEntity city{EntityMetadata{"world_place"}.id,"Newhaven",false,false,{}};
    MapSetupEntity nation{EntityMetadata{"nation"}.id,"Orun",true,false,{}};
    city.nationId=nation.id;
    std::vector<MapSetupEntity> selected={{established.metadata.id,established.name,false,true,{}},city,nation};
    auto facts=mapSetupFacts(selected);
    auto danglingNation = facts;
    danglingNation[1].entityRefs[1] = "deleted-nation";
    ok&=check(!mapSetupReferencesValid(campaign,danglingNation),"A new city cannot retain a deleted nation reference");
    ok&=check(mapSetupReferencesValid(campaign,facts),"A new city may refer to a new nation in the same setup");
    ok&=check(facts.size()==3 && mapSetupEntities(facts).size()==3,"Selected names and stable IDs must roundtrip through typed facts");
    ok&=check(mapSetupNameKey("  Havenfall \t")=="havenfall","Name comparison trims input without changing campaign labels");
    MapModel map; map.name="Regional map"; map.width=30; map.height=20;
    map.generation.recipe.facts=facts;
    map.generation.recipe.landform=MapLandform::InlandLake;
    for(const auto &entry:selected)
    {
        MapGeneratedFeature f; f.id=entry.id+"-feature"; f.name=entry.name;
        f.kind=entry.nation?MapGeneratedFeatureKind::Nation:MapGeneratedFeatureKind::Settlement;
        f.points=entry.nation?std::vector<MapFeaturePoint>{{0,0},{15,0},{15,20},{0,20}}:std::vector<MapFeaturePoint>{{8,12}};
        map.generation.graph.features.push_back(f);
    }
    int editing=-1,selectedMap=-1;
    {
        MapCreatorCampaignSaveTransaction<Campaign> transaction(campaign,map,editing,selectedMap);
        ok&=check(applyMapSetupEntities(campaign,map),"Initial named geometry should bind successfully");
        ok&=check(!transaction.finish(false),"Injected persistence failure must fail");
    }
    ok&=check(campaign.worldPlaces.size()==1 && campaign.nations.empty(),"Failed save must not create campaign entities");
    ok&=check(map.generation.recipe.facts[1].sourceText=="map_setup:new","Failed save must retain reserved draft identity for retry");
    {
        MapCreatorCampaignSaveTransaction<Campaign> transaction(campaign,map,editing,selectedMap);
        ok&=check(applyMapSetupEntities(campaign,map) && transaction.finish(true),"Retry should commit the same identities");
    }
    ok&=check(campaign.worldPlaces.size()==2 && campaign.worldPlaces.back().metadata.id==city.id && campaign.nations.front().metadata.id==nation.id,"Retry must commit exactly one entity per reserved ID");
    ok&=check(campaign.worldPlaces.back().nationId==nation.id,"New city keeps its selected nation relationship");
    const auto &reused=campaign.worldPlaces.front();
    ok&=check(reused.linkedMapId=="original-world" && reused.mapPosition.x==.25 && reused.type==WorldPlaceType::Town && reused.description=="Keep authored details","Reusing a city must preserve its canonical location, type and authored details");
    ok&=check(!map.generation.graph.features[0].gmOnly && map.generation.graph.features[1].gmOnly && map.generation.graph.features[2].gmOnly,"Reused public visibility and new private defaults must reach generated geometry");
    map.name="Renamed region";
    ok&=check(applyMapSetupEntities(campaign,map) && campaign.worldPlaces.size()==2 && campaign.nations.front().territories.size()==1,"Repeated saves and map renames must not duplicate entities or territories");
    map.generation.graph.features[0].name="Harbour alias";
    ok&=check(mapSetupBindingForFeature(map,map.generation.graph.features[0]).entityId==established.metadata.id,"A local rename must retain the reused campaign city binding");
    map.atlasLevel=MapLevel::Region;
    ok&=check(PlayerDisplayReview::generatedFeatureIsPublic(campaign,map,map.generation.graph.features[0]),"Reused public city must be available in reviewed secondary maps");
    campaign.worldPlaces.front().visibility=WorldObjectVisibility::GmOnly;
    ok&=check(!PlayerDisplayReview::generatedFeatureIsPublic(campaign,map,map.generation.graph.features[0]),"Campaign privacy changes must immediately hide secondary map appearances");
    ok&=check(applyMapSetupEntities(campaign,map) && map.generation.graph.features[0].gmOnly && campaign.worldPlaces.front().name=="Havenfall","Saving a local alias must preserve authored city name and refresh privacy");
    auto ambiguous=map; ambiguous.generation.recipe.facts.push_back(ambiguous.generation.recipe.facts.front());
    ok&=check(!PlayerDisplayReview::generatedFeatureIsPublic(campaign,ambiguous,ambiguous.generation.graph.features[0]),"Ambiguous secondary bindings must not grant visibility");
    ok&=check(mapSetupFacts(mapSetupEntities(map.generation.recipe.facts))[0].entityRefs[2]==map.generation.graph.features[0].id,"Local geometry binding must survive setup roundtrip");
    campaign.worldPlaces.erase(campaign.worldPlaces.begin());
    const auto before=campaign.worldPlaces.size();
    ok&=check(!applyMapSetupEntities(campaign,map) && campaign.worldPlaces.size()==before,"Deleted existing selection must block, not silently recreate it");
    auto recipe=map.generation.recipe; recipe.waterLevel=42;
    ok&=check(!mapGenerationRecipeEqual(recipe,map.generation.recipe),"Changing a generation slider must participate in draft change detection");
    for(const auto shape:{MapLandform::Automatic,MapLandform::Islands,MapLandform::Continental,MapLandform::InlandLake,
                         MapLandform::SingleIsland,MapLandform::Coast,MapLandform::Peninsula})
        ok&=check(mapLandformFromToken(mapLandformToken(shape))==shape,"Landform tokens must roundtrip");
    ok&=check(mapLandformFromToken("unknown")==MapLandform::Automatic,"Unknown/legacy shape must safely preserve automatic mode");
    return ok?0:1;
}
