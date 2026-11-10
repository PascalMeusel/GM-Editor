#pragma once
#include "map_render_plan_snapshot.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"
#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/TacticalMapGenerator.hpp"
#include <map>
#include <stdexcept>

namespace render_plan_test {
inline void require(bool value,const std::string &message) { if(!value)throw std::runtime_error(message); }
using Fixtures=std::map<std::string,MapModel>;
inline void identify(EntityMetadata &metadata,const std::string &id) {
    metadata.id=id;metadata.createdAtUtc=metadata.updatedAtUtc="2026-10-04T00:00:00Z";
}
inline MapModel controlMap() {
    MapModel map;identify(map.metadata,"render-control");map.name="Public Haven / Hafen";
    map.width=16;map.height=12;map.scope=MapScope::Local;map.atlasLevel=MapLevel::Tactical;
    map.creationIntent=MapCreationIntent::TacticalEncounter;map.visibility=MapVisibility::PlayersVisible;
    map.revision=17;map.playerRelease={true,true,true,true,true};resizeMapCells(map);
    for(auto &cell:map.cells)cell.fogged=false;
    for(int y=2;y<10;++y){mapCellAt(map,7,y)->kind=MapTileKind::Water;mapCellAt(map,7,y)->wayKind=MapWayKind::River;}
    for(int x=1;x<13;++x)mapCellAt(map,x,3)->wayKind=MapWayKind::DirtRoad;
    mapCellAt(map,3,7)->kind=MapTileKind::Forest;mapCellAt(map,11,7)->kind=MapTileKind::Mountain;
    auto &recipe=map.generation.recipe;recipe.seed=612345;recipe.generatorVersion=12;
    recipe.recipeVersion=kCurrentMapIllustrationRecipe;recipe.styleId="illustrated_atlas";recipe.styleVersion=6;
    recipe.scale=MapLevel::Tactical;recipe.styleSnapshotId.clear();map.generation.continuousRendering=true;
    const auto feature=[&](const char *id,MapGeneratedFeatureKind kind,std::vector<MapFeaturePoint> points,bool hidden=false) {
        MapGeneratedFeature f;f.id=id;f.name=hidden?"PRIVATE_RENDER_CANARY":id;f.kind=kind;
        f.points=std::move(points);f.width=.85f;f.discovered=true;f.gmOnly=hidden;map.generation.graph.features.push_back(f);
    };
    feature("public-road",MapGeneratedFeatureKind::Road,{{1,3},{6,3},{12,3}});
    feature("private-road",MapGeneratedFeatureKind::Road,{{1,9},{12,9}},true);
    feature("public-river",MapGeneratedFeatureKind::River,{{7,2},{7,6},{8,10}});
    feature("public-place",MapGeneratedFeatureKind::Settlement,{{3,5}});
    feature("private-place",MapGeneratedFeatureKind::Landmark,{{13,8}},true);
    feature("policy-place",MapGeneratedFeatureKind::Landmark,{{11,5}});
    MapLayerModel layer;identify(layer.metadata,"visible-layer");layer.name="Base";map.layers.push_back(layer);
    identify(layer.metadata,"hidden-layer");layer.visible=false;map.layers.push_back(layer);
    MapTokenModel token;identify(token.metadata,"public-token");token.name="Public token";token.layerId="visible-layer";
    token.x=2.25f;token.y=5.5f;token.rotation=12.25f;token.size=1.5f;map.tokens.push_back(token);
    identify(token.metadata,"private-token");token.hiddenFromPlayers=true;token.name="PRIVATE_RENDER_CANARY";map.tokens.push_back(token);
    identify(token.metadata,"hidden-layer-token");token.hiddenFromPlayers=false;token.layerId="hidden-layer";map.tokens.push_back(token);
    MapFeatureModel pin;identify(pin.metadata,"public-pin");pin.name="Public pin";pin.discovered=true;pin.x=4;pin.y=5;
    pin.gmNotes="PRIVATE_RENDER_CANARY";map.features.push_back(pin);
    identify(pin.metadata,"private-pin");pin.name="PRIVATE_RENDER_CANARY";pin.gmOnly=true;map.features.push_back(pin);
    MapRegionModel region;identify(region.metadata,"public-region");region.name="Public region";region.x=2;region.y=6;region.width=3;region.height=2;map.regions.push_back(region);
    identify(region.metadata,"private-region");region.gmOnly=true;region.name="PRIVATE_RENDER_CANARY";map.regions.push_back(region);
    MapRouteModel route;identify(route.metadata,"public-route");route.name="Public route";route.fromMapId=map.metadata.id;
    route.startX=1;route.startY=4;route.endX=12;route.endY=4;route.notes="PRIVATE_RENDER_CANARY";map.routes.push_back(route);
    identify(route.metadata,"private-route");route.discovered=false;route.name="PRIVATE_RENDER_CANARY";map.routes.push_back(route);
    return map;
}
inline void managedBackground(MapModel &map) {
    auto &r=map.importedReference;r.enabled=true;r.gmOnly=false;
    r.assetId="original-asset";r.projectRelativePath="Resources/Campaigns/render/Assets/Maps/original.png";
    r.contentHash="original-content-canary";r.sourceFileName="original.png";r.sourceWidth=2048;r.sourceHeight=1536;
    r.workingProxyGenerated=true;r.workingProxyAssetId="proxy-asset";
    r.workingProxyProjectRelativePath="Resources/Campaigns/render/Assets/Maps/proxy.png";
    r.workingProxyContentHash="proxy-content-canary";r.workingProxyWidth=512;r.workingProxyHeight=384;
    r.crop={.1f,.2f,.7f,.6f};r.rotationDegrees=90;map.backgroundImageOpacity=.625f;
}
inline MapModel generated(const std::string &name,bool large=false) {
    MapGenerationOptions o;o.width=large?80:28;o.height=large?80:22;o.seed=612345;o.qualityAttempts=1;
    o.generatorVersion=12;o.recipeVersion=kCurrentMapIllustrationRecipe;o.styleId="illustrated_atlas";o.contentLanguage="en";
    o.scale=MapLevel::Region;o.scope=MapScope::Local;o.creationIntent=MapCreationIntent::RegionExploration;
    if(name=="world"||large){o.generatorVersion=11;o.scale=MapLevel::World;o.scope=MapScope::World;o.creationIntent=MapCreationIntent::WorldOverview;}
    if(name=="settlement"){o.scale=MapLevel::Site;o.creationIntent=MapCreationIntent::Settlement;o.purpose=MapGenerationPurpose::SettlementEnvirons;o.settlementDensity=60;}
    if(name=="travel"){o.scale=MapLevel::Tactical;o.creationIntent=MapCreationIntent::TravelScene;o.purpose=MapGenerationPurpose::TravelArea;}
    if(name=="dungeon"||name=="cave") {o.generatorVersion=11;o.scope=MapScope::Dungeon;o.scale=MapLevel::Tactical;
        o.creationIntent=name=="cave"?MapCreationIntent::Cave:MapCreationIntent::Dungeon;o.caveNetwork=name=="cave";}
    // The 80x80 recipe deliberately matches the native player performance
    // source (one quality attempt, seed612345); it is generated only at prepare.
    MapModel map=large?generateSemanticMap(o).map:mapGenGenerateCandidate(o).map;
    identify(map.metadata,"render-"+name);map.name="Render fixture "+name;map.visibility=MapVisibility::PlayersVisible;
    map.playerRelease={true,true,true,true,true};for(auto &cell:map.cells)cell.fogged=false;
    if(large) {
        map.name="Public 80x80 world";MapGeneratedFeature privateRoad;privateRoad.id="native-private-review-road";
        privateRoad.name="GM_ONLY_REVIEW_CANARY";privateRoad.kind=MapGeneratedFeatureKind::Road;
        privateRoad.gmOnly=true;privateRoad.width=2;privateRoad.points={{10,10},{30,30}};
        map.generation.graph.features.push_back(privateRoad);
    }
    require(map.width==o.width&&map.height==o.height&&!map.cells.empty(),"Generated fixture dimensions changed: "+name);
    return map;
}
inline Fixtures makeFixtures(bool includeLarge) {
    Fixtures maps;maps.emplace("control",controlMap());auto classic=controlMap();
    classic.generation.recipe.generatorVersion=5;classic.generation.recipe.recipeVersion=3;
    classic.generation.recipe.styleId="fantasy_neutral";classic.generation.recipe.styleVersion=3;classic.generation.continuousRendering=false;
    identify(classic.metadata,"render-classic");maps.emplace("classic",std::move(classic));
    for(const std::string name:{"world","region","settlement","travel","dungeon","cave"})maps.emplace(name,generated(name));
    TacticalMapRecipe recipe;recipe.width=24;recipe.height=18;recipe.seed=612345;recipe.generatorVersion=12;
    recipe.illustrationRecipeVersion=kCurrentMapIllustrationRecipe;recipe.mapTemplate=TacticalMapTemplate::RiverCrossing;
    recipe.quality=MapGenerationQualityLevel::Draft;const auto candidates=TacticalMapGenerator::generate(recipe,nullptr,1);
    require(!candidates.empty(),"No tactical fixture generated");auto tactical=candidates.front().map;
    identify(tactical.metadata,"render-tactical");tactical.visibility=MapVisibility::PlayersVisible;
    tactical.playerRelease={true,true,true,true,true};for(auto &cell:tactical.cells)cell.fogged=false;maps.emplace("tactical",std::move(tactical));
    if(includeLarge)maps.emplace("world80",generated("world80",true));return maps;
}
enum class Variation { Full, Labels, Terrain, NoWays, Fog, PrivateMap, Background, UnsafeBackground, PolicyDeny, Grid };
struct Case {
    std::string name,fixture,locale="en";MapRenderAudience audience=MapRenderAudience::GameMaster;Variation variation=Variation::Full;
};
inline std::vector<Case> cases(const Fixtures &fixtures) {
    std::vector<Case> result;
    for(const auto &[name,ignored]:fixtures)for(const auto audience:{MapRenderAudience::GameMaster,MapRenderAudience::Player})
        result.push_back({name+(audience==MapRenderAudience::Player?"-player-en":"-gm-en"),name,"en",audience});
    for(const auto audience:{MapRenderAudience::GameMaster,MapRenderAudience::Player})
        result.push_back({audience==MapRenderAudience::Player?"control-player-de":"control-gm-de","control","de",audience});
    for(const auto &item:std::vector<std::pair<const char*,Variation>>{{"labels",Variation::Labels},{"terrain",Variation::Terrain},
        {"ways-revoked",Variation::NoWays},{"fog",Variation::Fog},{"private-map",Variation::PrivateMap},{"background",Variation::Background},
        {"unsafe-background",Variation::UnsafeBackground},{"policy-deny",Variation::PolicyDeny},{"grid",Variation::Grid}})
        result.push_back({std::string("control-player-")+item.first,"control","en",MapRenderAudience::Player,item.second});
    result.push_back({"control-gm-background","control","de",MapRenderAudience::GameMaster,Variation::Background});return result;
}
inline MapModel source(const Fixtures &fixtures,const Case &test) {
    auto map=fixtures.at(test.fixture);
    if(test.variation==Variation::NoWays)map.playerRelease.waysAndRoutes=false;
    if(test.variation==Variation::Fog)for(int y=0;y<map.height;++y)for(int x=map.width/2;x<map.width;++x)mapCellAt(map,x,y)->fogged=true;
    if(test.variation==Variation::PrivateMap)map.visibility=MapVisibility::GameMasterOnly;
    if(test.variation==Variation::Background||test.variation==Variation::UnsafeBackground){managedBackground(map);
        if(test.variation==Variation::UnsafeBackground)map.importedReference.projectRelativePath="../unreviewed.png";}
    return map;
}
inline MapRenderPlanRequest request(const Case &test,std::vector<std::string> *calls=nullptr) {
    MapRenderPlanRequest r;r.audience=test.audience;r.locale=test.locale;r.applyFog=test.audience==MapRenderAudience::Player;r.showPoliticalOverlay=true;
    if(test.variation==Variation::Labels)r.layers=MapRenderPlanLabels;
    if(test.variation==Variation::Terrain)r.layers=MapRenderPlanTerrain;
    r.showGrid=test.variation==Variation::Grid;
    if(calls||test.variation==Variation::PolicyDeny)r.additionalGeneratedFeatureVisibility=[calls,deny=test.variation==Variation::PolicyDeny](const auto &f){
        if(calls)calls->push_back(f.id);return !deny||f.id!="policy-place";
    };
    return r;
}
} // namespace render_plan_test
