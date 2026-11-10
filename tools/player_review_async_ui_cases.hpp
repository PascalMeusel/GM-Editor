// Actual production Editor/GM controls in CPU ImGui. Ready map previews allocate
// GL fog textures, so these cases draw only pending maps or ready TEXT reviews.
// Gates pause only the real CPU compiler; no native/backend/audio is started.
void playerReviewAsyncPendingUi(bool de,bool retained)
{
    using namespace player_review_async_test;
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);
    ui.context(std::string("Async review pending/cancel 27px ")+(de?"DE":"EN")+(retained?" retained":" initial"));
    auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    EditorWindowController editor;Release release{gate};auto source=playerReviewMemoFixture();
    if(retained)source.localPlayerDisplay.showSharedArt=false;
    require(editor.setCurrentCampaign(source),"Cannot install async popup campaign");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &owner=PlayerReviewUiProbe::owner(editor);
    auto options=gated(gate,trace,retained?PlayerReviewCompilePhase::BeforePlayer:PlayerReviewCompilePhase::BeforeGm);
    PlayerReviewSessionTestProbe::configure(owner,options);
    PlayerReviewUiProbe::open(editor);ui.draw=[&]{require(!owner.gmMap(),"CPU pending fixture must never draw a ready map");PlayerReviewUiProbe::draw(editor);};ui.frame(3);
    const auto *open=de?"Spielerfenster öffnen…":"Open player window…";
    const auto *confirm=de?"Spielerfenster öffnen":"Open player window";
    const auto *refresh=de?"Vorschau aktualisieren":"Refresh review";
    const auto *cancel=gm::localization::trUi("Cancel");
    ui.clickVisible(open);
    if(retained) {
        ui.waitReview(owner);require(owner.prepared()&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),"Async popup ready positive control absent");
        c.handouts[0].body="ASYNC_NEW_PUBLIC_TEXT";c.localPlayerDisplay.showSharedArt=true;ui.frame(2);
        // Keyboard Refresh disables its own button while the real map worker
        // is held. The later exact Tab/Space Cancel must remain reachable.
        ui.step("Keyboard Refresh before held compilation");ui.reviewTabTo(refresh);ui.reviewKey(ImGuiKey_Space);
    }
    entered(gate);ui.frame(3);
    require(owner.preparing()&&!owner.prepared()&&contains(ui.rendered,de?"wird vorbereitet":"Preparing"),
        "Actual popup omitted pending state or remained confirmable");
    if(retained) {
        require(owner.review().playerHandouts.size()==1&&owner.review().playerHandouts[0].body=="FROZEN_PUBLIC_TEXT",
            "Pending refresh changed the owner's immutable handout snapshot");
        const auto publicText=ui.reviewPublicText(cancel);
        if(!contains(publicText,"FROZEN_PUBLIC_TEXT"))throw std::runtime_error(
            "Pending popup public-review section omitted the retained text\nsection:\n"+publicText+"\nframe:\n"+ui.rendered);
        if(contains(publicText,"ASYNC_NEW_PUBLIC_TEXT"))throw std::runtime_error(
            "Pending popup public-review section leaked changed live text\nsection:\n"+publicText+"\nframe:\n"+ui.rendered);
    }
    ui.reviewBounds(confirm,cancel,27.f);ui.reviewState(confirm,true);ui.reviewState(refresh,true);ui.reviewState(cancel,false);
    const auto serial=owner.preparationSerial(),started=owner.compileStats().started;
    ui.frame(20);require(owner.preparationSerial()==serial&&owner.compileStats().started==started,
        "Pending popup frames queued more jobs");
    if(retained)require(!editor.armLocalPlayerDisplayFromReview(owner.review())&&!c.localPlayerDisplay.enabled,
        "Direct owner confirmation bypassed pending-state guard");
    // Genuine keyboard navigation reaches the always-visible Cancel footer.
    const auto beforeTab=ui.reviewInputState(cancel);ui.reviewTabTo(cancel);
    const auto afterTab=ui.reviewInputState(cancel);ui.reviewKey(ImGuiKey_Space);
    const auto assertCancel=[&](bool condition,const char *message){if(condition)return;
        const auto stats=owner.compileStats();std::ostringstream out;out<<message
            <<"\nBefore Tab: "<<beforeTab<<"\nAfter Tab: "<<afterTab<<"\nAfter Space: "<<ui.reviewInputState(cancel)
            <<"\nprepared="<<owner.prepared()<<" preparing="<<owner.preparing()<<" stale="<<owner.stale()
            <<" status="<<static_cast<int>(owner.preparationStatus())<<" serial="<<owner.preparationSerial()
            <<" gate_opened="<<gate->opened.load()<<" gate_timeout="<<gate->timedOut.load()
            <<" started="<<stats.started<<" completed="<<stats.completed<<" discarded="<<stats.discarded
            <<" running="<<stats.running<<" pending="<<stats.pending
            <<" display_enabled="<<c.localPlayerDisplay.enabled<<" display_published="<<c.localPlayerDisplay.published;
        throw std::runtime_error(out.str());};
    assertCancel(!ui.popupOpen(),"Keyboard Cancel did not close the actual review popup");
    assertCancel(!owner.preparing(),"Keyboard Cancel left preparation active");
    assertCancel(!owner.prepared(),"Keyboard Cancel retained an authorized review");
    assertCancel(!gate->opened&&!gate->timedOut,"Keyboard Cancel did not return while its worker barrier remained held");
    assertCancel(!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published,"Keyboard Cancel changed display flags");
    gate->open();until([&]{ui.frame();return !owner.compileStats().running;},"Canceled popup worker did not finish");ui.frame(3);
    require(!ui.popupOpen()&&!owner.prepared()&&!c.localPlayerDisplay.enabled,"Late completion reopened or authorized canceled popup");
    const auto saved=CampaignRepository::loadAllCampaigns();
    const auto found=std::find_if(saved.begin(),saved.end(),[&](const Campaign &row){return row.metadata.id==c.metadata.id;});
    require(found!=saved.end()&&!found->localPlayerDisplay.enabled&&!found->localPlayerDisplay.published,
        "Pending/Cancel unexpectedly persisted output flags");
}
void playerReviewAsyncForegroundUi(bool de,bool legacy)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);
    ui.context(std::string("Explicit async foreground 27px ")+(de?"DE":"EN")+(legacy?" GM":" Editor"));
    EditorWindowController editor;auto source=playerReviewMemoFixture();require(editor.setCurrentCampaign(source),"Cannot install foreground UI fixture");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &gm=PlayerReviewUiProbe::gm(editor);
    auto &owner=legacy?PlayerReviewUiProbe::owner(gm):PlayerReviewUiProbe::owner(editor);
    PlayerReviewCompileOptions options;options.limits.maximumDerivedBytes=1;PlayerReviewSessionTestProbe::configure(owner,options);
    PlayerReviewUiProbe::open(editor);ui.draw=[&]{require(c.presentationModeActive||!owner.gmMap(),"CPU fallback fixture must suppress its ready map before drawing");
        if(legacy)PlayerReviewUiProbe::draw(gm);else PlayerReviewUiProbe::draw(editor);};ui.frame(3);
    ui.clickVisible(de?"Spielerfenster öffnen…":"Open player window…");ui.waitReview(owner);
    const char *confirm=de?"Spielerfenster öffnen":"Open player window";
    const char *cancel=gm::localization::trUi("Cancel");
    require(owner.foregroundRequired()&&!owner.prepared(),"Budget-limited actual review did not offer explicit fallback");
    ui.reviewBounds(confirm,cancel,27.f);ui.reviewState(confirm,true);
    const auto jobs=owner.compileStats().started;ui.frame(20);
    require(owner.foregroundRequired()&&owner.compileStats().started==jobs&&!c.localPlayerDisplay.enabled,
        "Foreground review started automatically");
    ui.reviewClickRelease(de?"Jetzt vorbereiten (kann pausieren)":"Prepare now (may pause)");
    require(owner.prepared()&&!owner.foregroundRequired()&&owner.review().playerHandouts[0].body=="FROZEN_PUBLIC_TEXT",
        "Actual foreground action did not install correct immutable content");
    // Successful button dispatch is covered; ready MAP drawing is deliberately
    // left to native GL coverage. Next production frame must gate it entirely.
    c.presentationModeActive=true;ui.frame(2);
    require(!owner.prepared()&&!ui.popupOpen()&&!c.localPlayerDisplay.enabled&&!contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),
        "Presentation suppression retained foreground map review or changed output");
}
void playerReviewCompactKeyboard(bool de,bool legacy)
{
    HeadlessPanel ui(de,ImVec2(960,600),1.5f,18.f);
    ui.context(std::string("Review body/footer Tab 27px ")+(de?"DE":"EN")+(legacy?" GM":" Editor"));
    EditorWindowController editor;auto source=playerReviewTextFixture();source.localPlayerDisplay.showMap=true;
    source.localPlayerDisplay.publicMapId="missing-review-map";
    require(editor.setCurrentCampaign(source),"Cannot install repair keyboard fixture");
    auto &gm=PlayerReviewUiProbe::gm(editor);auto &owner=legacy?PlayerReviewUiProbe::owner(gm):PlayerReviewUiProbe::owner(editor);
    std::ostringstream drawStates;const char *phase="open";
    const auto step=[&](const char *next){phase=next;ui.step(next);};
    const char *repair=de?"Ohne Karte oder Kartenbild fortfahren":"Continue without map or artwork";
    PlayerReviewUiProbe::open(editor);ui.draw=[&]{require(!owner.gmMap(),"CPU keyboard fixture unexpectedly acquired a ready map");
        if(legacy)PlayerReviewUiProbe::draw(gm);else PlayerReviewUiProbe::draw(editor);
        drawStates<<"\nphase="<<phase<<" "<<ui.reviewInputState(repair)<<ui.reviewBodyState()
            <<" prepared="<<owner.prepared()<<" preparing="<<owner.preparing()<<" foreground="<<owner.foregroundRequired()
            <<" status="<<static_cast<int>(owner.preparationStatus())<<" issue="<<static_cast<int>(owner.review().issue)
            <<" eligible="<<owner.review().eligible<<" error='"<<owner.error()<<"'";
    };ui.frame(3);
    ui.clickVisible(de?"Spielerfenster öffnen…":"Open player window…");ui.waitReview(owner);
    const char *confirm=de?"Spielerfenster öffnen":"Open player window";
    const char *cancel=gm::localization::trUi("Cancel");
    ui.reviewBounds(confirm,cancel,27.f);ui.reviewState(confirm,true);
    try {
        step("Tab to repair");ui.reviewTabTo(repair);step("Tab body to Cancel before repair");ui.reviewTabTo(cancel);
        step("ShiftTab to repair");ui.reviewTabTo(repair,true);step("Space repair");ui.reviewKey(ImGuiKey_Space);
        step("Wait for repaired text review");ui.waitReview(owner);
        require(owner.prepared()&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),"Real keyboard map repair did not prepare valid text");
        step("Ready footer geometry/state");ui.reviewBounds(confirm,cancel,27.f);ui.reviewState(confirm,false);
        step("Tab to Cancel after repair control disappears");ui.reviewTabTo(cancel);
        step("Space ready Cancel");ui.reviewKey(ImGuiKey_Space);
        require(!ui.popupOpen()&&!owner.prepared(),"Body-to-footer keyboard route could not close ready review");
    } catch(...) {std::cerr<<"Keyboard review draw states:"<<drawStates.str()<<"\nLast rendered frame:\n"<<ui.rendered<<'\n';throw;}
}
