#pragma once
#include "map_render_memo_mutations.hpp"
#include "Models/Map/MapRenderCompileMemo.hpp"
#include "Models/Map/MapRenderPlanMeasurements.hpp"
#include "Models/Map/MapPackageServiceInternal.hpp"
#include <cfenv>
#include <cmath>
#include <iostream>

namespace render_plan_test {
using CompileMemo=gm::map_render::MapRenderCompileMemo;
inline void memoParity() {
    const auto fixtures=makeFixtures(true);std::size_t checked=0;
    for(const auto &test:cases(fixtures)) {
        const auto map=source(fixtures,test);std::vector<std::string> ordinaryCalls,missCalls,hitCalls;
        const auto expected=snapshot(compileMapRenderPlan(map,request(test,&ordinaryCalls)));
        CompileMemo memo;
        const auto miss=compileMapRenderPlanWithMemo(map,request(test,&missCalls),memo);
        MapRenderPlanMeasurements measured;const auto hit=compileMapRenderPlanWithMemo(map,request(test,&hitCalls),memo,&measured);
        require(snapshot(miss)==expected&&snapshot(hit)==expected,"Memo complete output differs: "+test.name);
        require(ordinaryCalls==missCalls&&ordinaryCalls==hitCalls,"Memo callback order/count differs: "+test.name);
        require(memo.stats().misses==1&&memo.stats().hits==1&&memo.stats().bypasses==0&&!memo.empty(),
            "Case did not exercise an admitted memo HIT: "+test.name);
        for(std::size_t stage=0;stage<measured.values.size();++stage) {
            const auto id=static_cast<MapRenderPlanCompileStage>(stage);
            const bool required=id==MapRenderPlanCompileStage::Total||id==MapRenderPlanCompileStage::Selection||id==MapRenderPlanCompileStage::ContentHash;
            require(measured.values[stage].calls==(required?1u:0u),"Hit skipped fresh selection/hash or compiled geometry: "+test.name);
        }
        std::vector<std::string> cacheOnlyCalls;MapRenderPlanMeasurements cacheOnlyTiming;
        const auto attempt=tryCompileMapRenderPlanWithMemo(map,request(test,&cacheOnlyCalls),memo,&cacheOnlyTiming);
        require(attempt.status==MapRenderCompileStatus::Hit&&attempt.plan&&snapshot(*attempt.plan)==expected&&
            cacheOnlyCalls==ordinaryCalls,"Cache-only attempt changed full output/callback sequence: "+test.name);
        for(std::size_t stage=0;stage<cacheOnlyTiming.values.size();++stage) {
            const auto id=static_cast<MapRenderPlanCompileStage>(stage);
            const bool required=id==MapRenderPlanCompileStage::Total||id==MapRenderPlanCompileStage::Selection||id==MapRenderPlanCompileStage::ContentHash;
            require(cacheOnlyTiming.values[stage].calls==(required?1u:0u),"Cache-only hit compiled geometry or omitted fresh hash: "+test.name);
        }
        auto changed=map;changed.name+=" changed without revision";cacheOnlyCalls.clear();MapRenderPlanMeasurements missTiming;
        const auto missed=tryCompileMapRenderPlanWithMemo(changed,request(test,&cacheOnlyCalls),memo,&missTiming);
        require(missed.status==MapRenderCompileStatus::Miss&&!missed.plan&&cacheOnlyCalls==ordinaryCalls,
            "Cache-only source miss compiled/fabricated a result or skipped callbacks: "+test.name);
        for(std::size_t stage=0;stage<missTiming.values.size();++stage) {
            const auto id=static_cast<MapRenderPlanCompileStage>(stage);
            const bool required=id==MapRenderPlanCompileStage::Total||id==MapRenderPlanCompileStage::Selection;
            require(missTiming.values[stage].calls==(required?1u:0u),"Cache-only miss performed hidden geometry/hash work: "+test.name);
        }++checked;
    }
    require(checked==32,"Full memo parity workload no longer contains the expected32cases");
    CompileMemo unavailable({1,1});MapRenderPlanMeasurements unavailableTiming;const auto map=controlMap();
    const auto refused=tryCompileMapRenderPlanWithMemo(map,MapRenderPlanRequest{},unavailable,&unavailableTiming);
    require(refused.status==MapRenderCompileStatus::Unavailable&&!refused.plan&&unavailable.empty(),
        "Unavailable exact key fell through to foreground compilation");
    std::cout<<"PASS memo/uncached full-field parity and actual hits for "<<checked<<" cases\n";
}
inline void memoInputMutations() {
    auto original=controlMap();managedBackground(original);MapRenderPlanRequest request;request.locale="en";request.applyFog=false;
    std::size_t count=0;
    for(const auto &[label,change]:memoMutations()) {
        auto changed=original;change(changed);require(changed.revision==original.revision,"Mutation accidentally bumped revision: "+label);
        CompileMemo memo;compileMapRenderPlanWithMemo(original,request,memo);
        const auto expected=snapshot(compileMapRenderPlan(changed,request));
        const auto actual=compileMapRenderPlanWithMemo(changed,request,memo);
        require(memo.stats().misses==2&&memo.stats().hits==0,"Exact source key omitted "+label);
        require(snapshot(actual)==expected,"Mutated memo output differs: "+label);
        require(snapshot(compileMapRenderPlanWithMemo(changed,request,memo))==expected&&memo.stats().hits==1,
            "Mutated source did not become an exact hit: "+label);++count;
    }
    const auto pair=[&](MapModel a,MapModel b,const char *label) {
        CompileMemo memo;compileMapRenderPlanWithMemo(a,request,memo);
        const auto expected=snapshot(compileMapRenderPlan(b,request));
        require(snapshot(compileMapRenderPlanWithMemo(b,request,memo))==expected&&memo.stats().misses==2&&memo.stats().hits==0,
            std::string("Lossy input encoding: ")+label);
    };
    auto a=original,b=original;a.spatialReference.originX=0.f;b.spatialReference.originX=-0.f;pair(a,b,"signed zero");
    a=original;b=original;b.tokens[0].rotation=std::nextafter(a.tokens[0].rotation,100.f);pair(a,b,"adjacent float bits");
    a=original;b=original;std::uint32_t bits=0x7fc00001u;std::memcpy(&a.spatialReference.extentWidth,&bits,4);++bits;
    std::memcpy(&b.spatialReference.extentWidth,&bits,4);pair(a,b,"distinct quiet NaN payloads in inactive calibration");
    a=original;b=original;a.generation.recipe.landform=static_cast<MapLandform>(900);
    b.generation.recipe.landform=static_cast<MapLandform>(901);pair(a,b,"raw unknown landform enum");
    a=original;b=original;a.metadata.tags={"ab","c"};b.metadata.tags={"a","bc"};pair(a,b,"string vector boundaries");
    a=original;b=original;a.name=std::string("a\0b",3);b.name="a";pair(a,b,"embedded NUL");
    for(int change=0;change<7;++change) {
        CompileMemo memo;compileMapRenderPlanWithMemo(original,request,memo);auto other=request;
        switch(change) {case 0:other.locale="de";break;case 1:other.audience=MapRenderAudience::Player;break;
        case 2:other.layers=MapRenderPlanLabels;break;case 3:other.applyFog=true;break;
        case 4:other.showPoliticalOverlay=true;break;case 5:other.showGrid=true;break;
        case 6:other.additionalGeneratedFeatureVisibility=[](const auto &){return true;};break;}
        require(snapshot(compileMapRenderPlanWithMemo(original,other,memo))==snapshot(compileMapRenderPlan(original,other))&&
            memo.stats().misses==2&&memo.stats().hits==0,"Request scalar/predicate-presence omitted from exact key");
    }
    const int savedRounding=std::fegetround();
    struct RestoreRounding {int mode;~RestoreRounding(){std::fesetround(mode);}} restore{savedRounding};
    require(savedRounding!=-1&&std::fesetround(FE_TONEAREST)==0,"Cannot establish bounded floating-point context fixture");
    CompileMemo memo;compileMapRenderPlanWithMemo(original,request,memo);
    require(std::fesetround(FE_DOWNWARD)==0,"Cannot set supported floating-point rounding fixture");
    const auto expected=snapshot(compileMapRenderPlan(original,request));
    require(snapshot(compileMapRenderPlanWithMemo(original,request,memo))==expected&&memo.stats().misses==2,
        "Changed ambient floating-point context reused prior compilation");
    std::cout<<"PASS "<<count<<" no-revision input mutations, raw bits/enums/boundaries, request and FP context\n";
}
inline void memoPolicyAndFallback() {
    auto map=controlMap();MapRenderPlanRequest request;request.audience=MapRenderAudience::Player;
    bool allow=true,fail=false;std::vector<std::string> calls;
    request.additionalGeneratedFeatureVisibility=[&](const MapGeneratedFeature &feature){
        calls.push_back(feature.id);if(fail)throw std::runtime_error("policy failure canary");return allow||feature.id!="policy-place";
    };
    CompileMemo memo;const auto approved=compileMapRenderPlanWithMemo(map,request,memo);const auto expectedCalls=calls;
    require(!expectedCalls.empty()&&std::find(calls.begin(),calls.end(),"private-road")==calls.end(),"Invalid callback positive/privacy fixture");
    calls.clear();compileMapRenderPlanWithMemo(map,request,memo);
    require(memo.stats().hits==1&&calls==expectedCalls,"Hit skipped or duplicated current policy evaluation");
    allow=false;calls.clear();const auto denied=compileMapRenderPlanWithMemo(map,request,memo);
    require(memo.stats().misses==2&&calls==expectedCalls&&snapshot(denied)!=snapshot(approved),"Changed captured policy reused authorized plan");
    fail=true;calls.clear();bool threw=false;
    try{compileMapRenderPlanWithMemo(map,request,memo);}catch(const std::runtime_error &e){threw=std::string(e.what())=="policy failure canary";}
    require(threw&&calls.size()==1,"Policy exception was swallowed/retried or evaluated more than once");
    fail=false;calls.clear();const auto hits=memo.stats().hits;
    require(snapshot(compileMapRenderPlanWithMemo(map,request,memo))==snapshot(denied)&&memo.stats().hits==hits+1&&calls==expectedCalls,
        "Failed policy destroyed or replaced previously admitted entry");
    // Duplicate IDs and a stateful predicate must retain invocation order, not
    // memoize decisions by ID or invoke the predicate a second time on misses.
    map.generation.graph.features[2].id=map.generation.graph.features[0].id;
    for(int pass=0;pass<2;++pass) {
        std::size_t cursor=0;std::vector<std::string> ordinary,actual;
        request.additionalGeneratedFeatureVisibility=[&](const auto &f){ordinary.push_back(f.id);return (++cursor%2)!=0;};
        const auto expected=snapshot(compileMapRenderPlan(map,request));cursor=0;
        request.additionalGeneratedFeatureVisibility=[&](const auto &f){actual.push_back(f.id);return (++cursor%2)!=0;};
        require(snapshot(compileMapRenderPlanWithMemo(map,request,memo))==expected&&actual==ordinary,
            "Duplicate-ID/stateful callback sequence changed");
    }
    for(int limit=0;limit<2;++limit) {
        gm::map_render::MapRenderCompileMemoLimits limits;
        if(limit==0)limits.maximumKeyBytes=1;else limits.maximumDerivedBytes=1;
        CompileMemo bounded(limits);std::vector<std::string> ordinary,first,second;
        const Case test{"bounded","control","en",MapRenderAudience::Player,Variation::Full};
        const auto expected=snapshot(compileMapRenderPlan(map,render_plan_test::request(test,&ordinary)));
        require(snapshot(compileMapRenderPlanWithMemo(map,render_plan_test::request(test,&first),bounded))==expected&&
            snapshot(compileMapRenderPlanWithMemo(map,render_plan_test::request(test,&second),bounded))==expected&&
            first==ordinary&&second==ordinary&&bounded.empty()&&bounded.stats().hits==0&&bounded.stats().bypasses>=2,
            "Bounded admission fallback changed output/callbacks or retained oversized state");
    }
    gm::map_render::MapRenderCompileMemoLimits smallLimit;smallLimit.maximumDerivedBytes=4096;
    CompileMemo retained(smallLimit);MapModel small;small.width=small.height=1;small.cells.resize(1);
    MapRenderPlanRequest smallRequest;smallRequest.layers=0;
    const auto smallPlan=snapshot(compileMapRenderPlanWithMemo(small,smallRequest,retained));
    require(!retained.empty(),"Small retention positive control was not admitted");
    const auto full=controlMap();MapRenderPlanRequest fullRequest;
    require(snapshot(compileMapRenderPlanWithMemo(full,fullRequest,retained))==snapshot(compileMapRenderPlan(full,fullRequest))&&
        retained.stats().bypasses>0,"Oversized replacement did not take ordinary compile fallback");
    const auto retainedHits=retained.stats().hits;
    require(snapshot(compileMapRenderPlanWithMemo(small,smallRequest,retained))==smallPlan&&retained.stats().hits==retainedHits+1,
        "Failed larger admission discarded earlier usable memo entry");
    const auto old=snapshot(approved);memo.clear();require(memo.empty()&&snapshot(approved)==old,"Clear mutated returned immutable value");
    std::cout<<"PASS memo policy revocation, duplicate IDs, exceptions, admission fallback and clear\n";
}
inline void boundedWriterCompatibility() {
    using namespace MapPackageServiceInternal;
    auto map=controlMap();managedBackground(map);Writer ordinary;writeMap(ordinary,map);
    const auto expected=ordinary.data();require(!expected.empty(),"Empty package writer positive control");
    Writer exact(nullptr,expected.size());writeMap(exact,map);
    require(exact.data()==expected&&exact.data().capacity()<=expected.size(),"Bounded writer changed legacy map bytes or exceeded budget");
    bool refused=false;try{exact.u8(1);}catch(const std::length_error &){refused=true;}
    require(refused&&exact.data()==expected,"Exact-limit writer accepted an extra byte or changed finished bytes");
    for(const auto limit:{std::size_t{0},expected.size()-1}) {
        Writer bounded(nullptr,limit);refused=false;
        try{writeMap(bounded,map);}catch(const std::length_error &){refused=true;}
        require(refused&&bounded.data().size()<=limit&&bounded.data().capacity()<=limit,
            "Zero/short writer limit did not bound partial output capacity");
    }
    std::cout<<"PASS package writer legacy bytes, exact/zero/short aggregate limits\n";
}
} // namespace render_plan_test
