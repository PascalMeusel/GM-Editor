#include "map_render_plan_workload.hpp"
#include "Models/Map/MapRenderPlanMeasurements.hpp"
#include "Models/Map/MapRenderPlanCache.hpp"
#include "map_render_memo_cases.hpp"
#include <iostream>

namespace {
using namespace render_plan_test;
void serializerSensitivity() {
    MapRenderPlan p;MapRenderPrimitive primitive;primitive.points={{0.f,2.f},{3.f,4.f}};
    gm::map_render::AtlasInkPrimitive ink;ink.points={{1.f,2.f},{3.f,4.f},{5.f,6.f}};
    ink.vertexColors={{1,2,3,4},{5,6,7,8},{9,10,11,12}};primitive.detailInk={ink};p.primitives={primitive};
    p.atlasInk=p.atlasWaterInk=p.atlasFeatureInk={ink};p.labelInkMask.covered={0,1,2,255};
    const auto baseline=snapshot(p);std::size_t checks=0;
    const auto changed=[&](const MapRenderPlan &q,const char *field){++checks;require(snapshot(q)!=baseline,std::string("Serializer omitted ")+field);};
#define CHANGE(field,value) {auto q=p;q.field=value;changed(q,#field);}
    CHANGE(mapId,"changed");CHANGE(mapRevision,9);CHANGE(level,MapLevel::Tactical);CHANGE(scope,MapScope::Dungeon);
    CHANGE(gridType,GridType::Hex);CHANGE(mapVisibility,MapVisibility::PlayersVisible);CHANGE(audience,MapRenderAudience::Player);
    CHANGE(locale,"de");CHANGE(styleId,"changed");CHANGE(styleSnapshotId,"changed");CHANGE(styleVersion,2);
    CHANGE(generationSeed,7);CHANGE(generatorVersion,12);CHANGE(recipeVersion,11);CHANGE(terrainLandscape,MapLandscapeProfile::Highlands);
    CHANGE(continuousRendering,true);CHANGE(layers,MapRenderPlanLabels);CHANGE(playerReleaseLayers,7);CHANGE(applyFog,false);
    CHANGE(showPoliticalOverlay,true);CHANGE(showGrid,true);CHANGE(backgroundVisible,true);
    CHANGE(backgroundInteractiveAssetId,"changed");CHANGE(backgroundInteractiveProjectRelativePath,"changed");
    CHANGE(backgroundInteractiveContentHash,"changed");CHANGE(backgroundExportAssetId,"changed");
    CHANGE(backgroundExportProjectRelativePath,"changed");CHANGE(backgroundExportContentHash,"changed");
    CHANGE(generatedFeatureIds,std::vector<std::string>{"a"});CHANGE(manualFeatureIds,std::vector<std::string>{"a"});
    CHANGE(regionIds,std::vector<std::string>{"a"});CHANGE(routeIds,std::vector<std::string>{"a"});CHANGE(tokenIds,std::vector<std::string>{"a"});
    CHANGE(primitives[0].kind,MapRenderPrimitiveKind::Route);CHANGE(primitives[0].layer,42);
    CHANGE(primitives[0].sourceId,"changed");CHANGE(primitives[0].assetRole,"changed");CHANGE(primitives[0].assetId,"changed");
    CHANGE(primitives[0].label,"changed");CHANGE(primitives[0].closed,true);CHANGE(primitives[0].points[0].x,-0.f);
    CHANGE(primitives[0].points[0].y,9.f);CHANGE(primitives[0].detailInk[0].strokeCells,2.f);
    CHANGE(labelInkMask.width,4);CHANGE(labelInkMask.height,3);CHANGE(labelInkMask.samplesPerCell,8.f);
    CHANGE(labelInkMask.covered[2],3);CHANGE(contentHash,123);
#undef CHANGE
    // Every ink member is checked in every storage location, including nested
    // detail ink. A hash-only comparison misses several of these on old recipes.
    for(int location=0;location<4;++location)for(int field=0;field<15;++field) {
        auto q=p;auto &values=location==0?q.atlasInk:location==1?q.atlasWaterInk:location==2?q.atlasFeatureInk:q.primitives[0].detailInk;
        auto &v=values[0];switch(field) {
        case 0:v.points[0].x=-1;break;case 1:v.points[0].y=-2;break;case 2:v.points.push_back({7,8});break;
        case 3:v.color.r=123;break;case 4:v.color.g=124;break;case 5:v.color.b=125;break;case 6:v.color.a=126;break;
        case 7:v.strokeCells=2;break;case 8:v.filled=!v.filled;break;case 9:v.minimumCellPixels=3;break;
        case 10:v.maximumCellPixels=4;break;case 11:v.vertexColors[0].r=15;break;case 12:v.vertexColors.push_back({1,2,3,4});break;
        case 13:std::swap(v.points[0],v.points[1]);break;case 14:values.push_back(v);break;
        }changed(q,"nested ink member/order/count");
    }
    auto q=p;q.generatedFeatureIds={"ab","c"};auto r=q;r.generatedFeatureIds={"a","bc"};
    require(snapshot(q)!=snapshot(r),"String vector boundaries collide");
    q=p;q.primitives[0].label=std::string("a\0b",3);r=q;r.primitives[0].label="a";
    require(snapshot(q)!=snapshot(r),"Embedded NUL string was truncated");
    std::uint32_t bits=0x7fc00001u;float first;std::memcpy(&first,&bits,4);++bits;float second;std::memcpy(&second,&bits,4);
    q=p;r=p;q.primitives[0].points[0].x=first;r.primitives[0].points[0].x=second;
    require(snapshot(q)!=snapshot(r),"Distinct float payloads normalized");
    std::cout<<"PASS full-field serializer sensitivity: "<<checks<<" mutations plus boundary/float controls\n";
}
bool has(const std::vector<EntityId> &ids,const EntityId &id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
void privacyAndRevocation() {
    const auto original=controlMap();MapRenderPlanRequest request;request.audience=MapRenderAudience::Player;request.locale="en";
    const auto player=compileMapRenderPlan(original,request);auto gmRequest=request;gmRequest.audience=MapRenderAudience::GameMaster;
    const auto gm=compileMapRenderPlan(original,gmRequest);
    require(has(gm.generatedFeatureIds,"private-road")&&snapshot(gm).find("PRIVATE_RENDER_CANARY")!=std::string::npos,"GM positive private control absent");
    require(has(player.generatedFeatureIds,"public-road")&&has(player.tokenIds,"public-token")&&
        snapshot(player).find("PRIVATE_RENDER_CANARY")==std::string::npos&&!has(player.tokenIds,"hidden-layer-token"),"Player projection/private canary failure");
    const auto filtered=filterMapForRenderPlan(original,player);
    require(filtered.features.size()==1&&filtered.features[0].gmNotes.empty()&&filtered.routes.size()==1&&filtered.routes[0].notes.empty(),"Filtered private notes leaked");
    const auto frozen=snapshot(player);auto changed=original;changed.playerRelease.waysAndRoutes=false;
    const auto revoked=compileMapRenderPlan(changed,request);
    require(!mapRenderPlanContainsGeneratedGeometry(revoked,"public-road")&&!mapRenderPlanContentMatchesMap(player,changed)&&snapshot(player)==frozen,"Consent revocation reused old geometry or mutated frozen plan");
    changed=original;changed.visibility=MapVisibility::GameMasterOnly;
    require(compileMapRenderPlan(changed,request).layers==MapRenderPlanFog&&!mapRenderPlanContentMatchesMap(player,changed),"Whole-map revocation failed");
    changed=original;mapCellAt(changed,11,5)->fogged=true;const auto fog=compileMapRenderPlan(changed,request);
    require(!has(fog.generatedFeatureIds,"policy-place")&&has(fog.generatedFeatureIds,"public-place"),"Fogged/public point controls failed");
    changed=original;managedBackground(changed);const auto background=compileMapRenderPlan(changed,request);
    require(background.backgroundVisible&&background.backgroundInteractiveAssetId=="proxy-asset"&&background.backgroundExportAssetId=="original-asset"&&
        background.backgroundInteractiveContentHash=="proxy-content-canary"&&background.backgroundExportContentHash=="original-content-canary","Managed background identities not bound");
    mapCellAt(changed,0,0)->fogged=true;require(!compileMapRenderPlan(changed,request).backgroundVisible,"Partial fog exposed full background");
    bool allowed=true;std::vector<std::string> calls;request.additionalGeneratedFeatureVisibility=[&](const auto &f){calls.push_back(f.id);return allowed||f.id!="policy-place";};
    MapRenderPlanCache cache;const auto old=cache.getOrCompile(original,request);const auto oldBytes=snapshot(*old);const auto firstCalls=calls;
    allowed=false;calls.clear();const auto fresh=cache.getOrCompile(original,request);
    require(has(old->generatedFeatureIds,"policy-place")&&!has(fresh->generatedFeatureIds,"policy-place")&&cache.compileCount()==2&&old!=fresh&&
        calls==firstCalls&&snapshot(*old)==oldBytes,"Captured callback policy or old-plan lifetime was cached incorrectly");
    require(!has(calls,"private-road")&&!has(calls,"private-place"),"Player callback was offered built-in hidden features");
    std::cout<<"PASS player consent/fog/background/callback revoke and frozen-plan controls\n";
}
void measuredParity() {
    const auto fixtures=makeFixtures(false);std::size_t count=0,details=0,terrain=0,water=0,features=0,masks=0;
    for(const auto &test:cases(fixtures)) {
        const auto map=source(fixtures,test);std::vector<std::string> ordinaryCalls,measuredCalls;
        const auto ordinary=compileMapRenderPlan(map,request(test,&ordinaryCalls));MapRenderPlanMeasurements measures;
        const auto measured=compileMapRenderPlanMeasured(map,request(test,&measuredCalls),measures);
        require(snapshot(ordinary)==snapshot(measured),"Measured/ordinary full-field mismatch: "+test.name);
        require(ordinaryCalls==measuredCalls,"Callback invocation sequence/count changed: "+test.name);
        require(measures.values[std::size_t(MapRenderPlanCompileStage::Total)].calls==1&&
            measures.values[std::size_t(MapRenderPlanCompileStage::ContentHash)].calls==1,"Measurement call scope mismatch: "+test.name);
        for(const auto &p:ordinary.primitives)details+=p.detailInk.size();terrain+=ordinary.atlasInk.size();water+=ordinary.atlasWaterInk.size();
        features+=ordinary.atlasFeatureInk.size();masks+=ordinary.labelInkMask.covered.size();++count;
    }
    require(details&&terrain&&water&&features&&masks,"Fixture workload did not positively populate all ink stores/mask");
    MapRenderPlanMeasurements accumulation;const auto map=controlMap();MapRenderPlanRequest r;
    compileMapRenderPlanMeasured(map,r,accumulation);compileMapRenderPlanMeasured(map,r,accumulation);
    require(accumulation.values[std::size_t(MapRenderPlanCompileStage::Total)].calls==2,"Caller-owned measurement did not accumulate");
    std::cout<<"PASS "<<count<<" production measured/unmeasured cases; detail/terrain/water/feature/mask bytes populated\n";
}
}
int main() {
    try {serializerSensitivity();privacyAndRevocation();measuredParity();memoParity();memoInputMutations();memoPolicyAndFallback();boundedWriterCompatibility();
        std::cout<<"PASS CPU render-plan parity; no windows, GL, audio or database\n";return 0;}
    catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
