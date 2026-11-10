// Real CPU worker + public owner API. Gates bound scheduling deterministically;
// watchdogs detect deadlocks, not a machine-dependent latency acceptance gate.
void asyncReviewParity(const fs::path &root)
{
    using namespace player_review_async_test;
    auto c=fixture(root);auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    Measurements measurements;PlayerReviewSession owner(gated(gate,trace,PlayerReviewCompilePhase::BeforePlayer));Release release{gate};
    owner.setMeasurements(&measurements);require(owner.prepare(c,"en"),"Cannot seed retained async preview");
    const auto expected=owner.review();const auto priorGeneration=owner.generation();
    const auto h=expected.playerHandouts[0];auto frozen=owner.playerImage(h.projectRelativePath,h.contentHash);
    require(owner.requestPreparation(c,"en",41),"Cannot queue actual review worker");entered(gate);
    require(owner.preparing()&&!owner.prepared()&&!owner.knownCurrent(c,"en")&&owner.generation()==priorGeneration+1,
        "Queued preparation retained consent or did not advance generation exactly once");
    for(int frame=0;frame<20;++frame)require(!owner.pollPreparation(c,"en",41),"Held review was adopted");
    require(frozen.valid()&&read(frozen.path())=="APPROVED_IMAGE_BYTES"&&
        render_plan_test::snapshot(owner.review().playerMapPlan)==render_plan_test::snapshot(expected.playerMapPlan),
        "Pending refresh changed earlier frozen content");
    require(owner.compileStats().started==1&&owner.compileStats().running&&!owner.compileStats().pending,
        "Polling spawned duplicate review workers");
    gate->open();settle(owner,c,"en",41);
    require(owner.prepared()&&owner.knownCurrent(c,"en")&&owner.generation()==priorGeneration+1,
        "Current worker result failed to become ready exactly once");
    require(render_plan_test::snapshot(owner.review().gmMapPlan)==render_plan_test::snapshot(expected.gmMapPlan)&&
        render_plan_test::snapshot(owner.review().playerMapPlan)==render_plan_test::snapshot(expected.playerMapPlan)&&
        owner.review().contentFingerprint==expected.contentFingerprint,"Async compiler changed exact reviewed output");
    const auto accepted=owner.acceptedCompileStats();
    require(accepted.gm.hits>0&&accepted.player.hits>0,"Async adoption did not hit both exact worker memos");
    const auto events=trace->copy();require(events.size()==3,"Worker did not report three real compile phases");
    require(events[0].phase==PlayerReviewCompilePhase::BeforeGm&&events[1].phase==PlayerReviewCompilePhase::BeforePlayer&&
        events[2].phase==PlayerReviewCompilePhase::Ready,"Compile progress phases are out of order");
    for(const auto &event:events)require(event.thread!=std::this_thread::get_id()&&event.thread==events[0].thread,
        "Compile work ran on caller or multiple worker threads");
}
void asyncCancelAndLatest(const fs::path &root)
{
    using namespace player_review_async_test;
    for(bool clear:{false,true}) {
        auto c=fixture(root);auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
        PlayerReviewSession owner(gated(gate,trace));Release release{gate};
        require(owner.prepare(c,"en"),"Cannot seed cancellation preview");
        require(owner.requestPreparation(c,"en",1),"Cannot queue cancellation worker");entered(gate);
        const auto old=owner.preparationSerial();
        if(clear)owner.clear();else owner.cancelPreparation();
        require(!gate->timedOut&&!gate->opened&&!owner.preparing()&&!owner.prepared()&&owner.compileStats().running,
            "UI cancellation joined worker or retained authorization");
        require(clear?owner.review().playerHandouts.empty():owner.review().playerHandouts[0].body=="PUBLIC_SNAPSHOT_BODY",
            "Cancel/clear retained-state contract differs");
        c.handouts[0].body="QUEUED_B";require(owner.requestPreparation(c,"en",2),"Cannot queue B behind retired worker");
        const auto discardedB=owner.preparationSerial();c.handouts[0].body="LATEST_C";
        require(owner.requestPreparation(c,"en",3),"Cannot replace pending B with C");const auto latest=owner.preparationSerial();
        require(old!=discardedB&&discardedB!=latest&&owner.compileStats().started==1&&owner.compileStats().pending,
            "Queued context spawned concurrent workers or reused request identity");
        gate->open();settle(owner,c,"en",3);drain(owner,c,"en",3);
        require(owner.prepared()&&owner.review().playerHandouts[0].body=="LATEST_C"&&owner.compileStats().started==2,
            "Retired or intermediate result replaced latest requested review");
        const auto events=trace->copy();
        for(const auto &event:events)require(event.serial!=discardedB&&event.thread==events.front().thread,
            "Superseded queued job executed or a second worker was created");
    }
}
void asyncContextRevocation(const fs::path &root)
{
    using namespace player_review_async_test;
    for(int change=0;change<9;++change) {
        auto c=fixture(root);c.maps[0].atlasLevel=MapLevel::World;
        c.maps[0].playerRelease.placesAndMarkers=c.maps[0].playerRelease.labels=true;
        MapGeneratedFeature feature;feature.id="async-public-place";feature.name="ASYNC_PUBLIC_PLACE";
        feature.kind=MapGeneratedFeatureKind::Settlement;feature.discovered=true;feature.points={{2,2}};
        c.maps[0].generation.graph.features.push_back(feature);
        WorldPlaceModel place;place.name=feature.name;place.linkedMapId=c.maps[0].metadata.id;place.linkedFeatureId=feature.id;
        place.visibility=WorldObjectVisibility::Public;c.worldPlaces.push_back(place);
        auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
        PlayerReviewSession owner(gated(gate,trace,PlayerReviewCompilePhase::Ready));Release release{gate};
        require(owner.prepare(c,"en"),"Cannot seed context preview");const auto original=owner.review();
        const auto oldRevision=c.maps[0].revision;require(owner.requestPreparation(c,"en",17),"Cannot queue context worker");entered(gate);
        Campaign replacement=c;const Campaign *current=&c;std::string locale="en";std::uint64_t epoch=17;
        std::unique_ptr<DataRoot> changedRoot;
        if(change==0)c.maps[0].generation.graph.features.back().points[0].x=3;
        if(change==1)c.worldPlaces[0].visibility=WorldObjectVisibility::GmOnly;
        if(change==2)c.localPlayerDisplay.publicHandoutIds.clear();
        if(change==3)locale="de";
        if(change==4)++epoch;
        if(change==5)current=&replacement; // Same stable ID, different live owner.
        if(change==6)c.metadata.id=createEntityId("changed-async-campaign");
        if(change==7){fs::create_directories(root/"replacement-data-root");changedRoot=std::make_unique<DataRoot>((root/"replacement-data-root").u8string());}
        if(change==8){++c.maps[0].generation.recipe.generatorVersion;
            require(sourceFingerprint(c,"en",false,false)==sourceFingerprint(replacement,"en",false,false),
                "Exact-key miss fixture no longer bypasses the cheap invalidation key");}
        require(c.maps[0].revision==oldRevision,"Context mutation fixture unexpectedly changed revision");
        gate->open();settle(owner,*current,locale,epoch);drain(owner,*current,locale,epoch);
        require(!owner.prepared()&&!owner.foregroundRequired()&&!owner.knownCurrent(*current,locale)&&
            owner.review().contentFingerprint==original.contentFingerprint&&
            render_plan_test::snapshot(owner.review().playerMapPlan)==render_plan_test::snapshot(original.playerMapPlan),
            "Changed context adopted stale compile output or destroyed earlier preview");
        changedRoot.reset();c=replacement;owner.pollPreparation(c,"en",17);
        require(!owner.prepared(),"Reverting changed context silently restored review consent");
    }
}
void asyncAmbientAndReadyIdentity(const fs::path &root)
{
    using namespace player_review_async_test;
    auto c=fixture(root);const int originalRounding=std::fegetround();
    struct Restore {int mode;~Restore(){std::fesetround(mode);}} restore{originalRounding};
    auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    PlayerReviewSession owner(gated(gate,trace,PlayerReviewCompilePhase::Ready));Release release{gate};
    require(owner.requestPreparation(c,"en",11),"Cannot queue ambient mismatch worker");entered(gate);
    const int changed=originalRounding==FE_DOWNWARD?FE_UPWARD:FE_DOWNWARD;
    require(std::fesetround(changed)==0,"Host cannot exercise supported alternate rounding");
    gate->open();settle(owner,c,"en",11);
    require(!owner.prepared()&&owner.foregroundRequired(),"Worker/main FP mismatch silently compiled or authorized output");
    require(std::fesetround(originalRounding)==0&&owner.prepareInForeground(c,"en",11),"Explicit foreground ambient recovery failed");
    auto same=c;require(!owner.knownCurrent(same,"en")&&!owner.knownCurrent(c,"en"),"Ready same-ID object replacement retained consent");
    require(owner.prepare(c,"en"),"Cannot reset ready root fixture");
    {DataRoot different((root/"another-ready-root").u8string());require(!owner.knownCurrent(c,"en"),"Ready root replacement retained consent");}
    require(!owner.knownCurrent(c,"en"),"Restoring earlier root silently restored consent");
}
void asyncFailureAndForeground(const fs::path &root)
{
    using namespace player_review_async_test;
    for(int failure=0;failure<3;++failure) {
        auto c=fixture(root);PlayerReviewCompileOptions options;
        if(failure==0)options.phaseHook=[](std::uint64_t,PlayerReviewCompilePhase phase){
            if(phase==PlayerReviewCompilePhase::BeforePlayer)throw std::runtime_error("INJECTED_ASYNC_WORKER_FAILURE");};
        if(failure==1)options.limits.maximumKeyBytes=1;
        if(failure==2)options.limits.maximumDerivedBytes=1;
        PlayerReviewSession owner(options);require(owner.prepare(c,"en"),"Cannot seed fallback preview");
        const auto before=owner.review().contentFingerprint;require(owner.requestPreparation(c,"en",2),"Cannot request fallback fixture");
        settle(owner,c,"en",2);require(owner.foregroundRequired()&&!owner.prepared()&&
            owner.review().contentFingerprint==before,"Failed/admission-limited work lost old preview or automatically authorized foreground work");
        const auto completed=owner.compileStats().completed,started=owner.compileStats().started;
        for(int frame=0;frame<20;++frame)owner.pollPreparation(c,"en",2);
        require(owner.foregroundRequired()&&owner.compileStats().completed==completed&&owner.compileStats().started==started,
            "Idle polling retried failed worker or foreground compilation");
        require(owner.prepareInForeground(c,"en",2)&&owner.prepared()&&owner.knownCurrent(c,"en"),
            "Explicit ordinary foreground action failed to recover valid content");
    }
}
void asyncSynchronousTakeover(const fs::path &root)
{
    using namespace player_review_async_test;
    auto c=fixture(root);auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    PlayerReviewSession owner(gated(gate,trace,PlayerReviewCompilePhase::Ready));Release release{gate};
    require(owner.requestPreparation(c,"en",8),"Cannot queue synchronous takeover worker");entered(gate);
    c.handouts[0].body="EXPLICIT_SYNCHRONOUS_REVIEW";
    require(owner.prepare(c,"en")&&!gate->timedOut&&!gate->opened,"Synchronous prepare joined retired worker");
    const auto fingerprint=owner.review().contentFingerprint,generation=owner.generation();
    gate->open();drain(owner,c,"en",8);
    require(owner.prepared()&&owner.review().contentFingerprint==fingerprint&&owner.generation()==generation&&
        owner.review().playerHandouts[0].body=="EXPLICIT_SYNCHRONOUS_REVIEW","Late worker replaced explicit synchronous review");
}
void asyncMissingAsset(const fs::path &root)
{
    using namespace player_review_async_test;
    auto c=fixture(root);auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    PlayerReviewSession owner(gated(gate,trace,PlayerReviewCompilePhase::Ready));Release release{gate};
    require(owner.prepare(c,"en"),"Cannot seed missing-asset preview");const auto old=owner.review();
    const auto h=old.playerHandouts[0];const auto frozen=owner.playerImage(h.projectRelativePath,h.contentHash);
    require(owner.requestPreparation(c,"en",4),"Cannot queue asset worker");entered(gate);
    fs::remove(root/fs::u8path(h.projectRelativePath));gate->open();settle(owner,c,"en",4);
    require(!owner.prepared()&&owner.review().contentFingerprint==old.contentFingerprint&&
        frozen.valid()&&read(frozen.path())=="APPROVED_IMAGE_BYTES","Failed final asset check destroyed previous frozen bytes or authorized missing source");
}
void asyncFreshAssetBinding(const fs::path &root)
{
    using namespace player_review_async_test;
    auto c=fixture(root);auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    PlayerReviewSession owner(gated(gate,trace,PlayerReviewCompilePhase::Ready));Release release{gate};
    require(owner.prepare(c,"en"),"Cannot seed byte-replacement preview");const auto old=owner.review();
    const auto oldHandout=old.playerHandouts[0];const auto frozen=owner.playerImage(oldHandout.projectRelativePath,oldHandout.contentHash);
    const auto path=root/fs::u8path(oldHandout.projectRelativePath),sibling=path.parent_path()/"async-replacement.png";
    const auto modified=fs::last_write_time(path);require(owner.requestPreparation(c,"en",7),"Cannot queue replacement worker");entered(gate);
    write(sibling,"REPLACED_IMAGE_DATA!");fs::last_write_time(sibling,modified);fs::remove(path);fs::rename(sibling,path);
    require(fs::file_size(path)==fs::file_size(frozen.path())&&fs::last_write_time(path)==modified&&
        read(frozen.path())=="APPROVED_IMAGE_BYTES","Pending same-size/time fixture changed earlier frozen bytes");
    gate->open();settle(owner,c,"en",7);
    require(owner.prepared()&&owner.review().playerHandouts[0].contentHash!=oldHandout.contentHash,
        "Ready review reused old attachment hash after pending replacement");
    const auto freshHandout=owner.review().playerHandouts[0];const auto fresh=owner.playerImage(freshHandout.projectRelativePath,freshHandout.contentHash);
    require(fresh.valid()&&read(fresh.path())=="REPLACED_IMAGE_DATA!"&&
        isCurrent(c,owner.review(),nullptr,Origin::Unspecified,owner.compileMemo().get()),"Ready review did not bind its fresh canonical attachment bytes");
    write(path,"APPROVED_IMAGE_BYTES");fs::last_write_time(path,modified);int saves=0;
    require(armAndSave(c,owner.review(),[&](Campaign &){++saves;return true;},nullptr,owner.compileMemo().get()).status==TransactionStatus::ReviewChanged&&
        saves==0&&read(fresh.path())=="REPLACED_IMAGE_DATA!","Post-ready byte replacement bypassed actual canonical confirmation");
}
void asyncReviewCases(const fs::path &root)
{
    asyncReviewParity(root);asyncCancelAndLatest(root);asyncContextRevocation(root);
    asyncFailureAndForeground(root);asyncSynchronousTakeover(root);asyncMissingAsset(root);asyncFreshAssetBinding(root);
    asyncAmbientAndReadyIdentity(root);
}
