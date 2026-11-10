struct PlayerReviewSubscription
{
    int id;
    explicit PlayerReviewSubscription(ProjectEventBus::Handler f):id(ProjectEventBus::getInstance().subscribe(std::move(f))){}
    ~PlayerReviewSubscription(){ProjectEventBus::getInstance().unsubscribe(id);}
};
void playerReviewEditorUi(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);
    ui.context(std::string("Frozen Editor player review")+(de?" DE":" EN")+(compact?" compact":" normal"));
    EditorWindowController editor;auto fixture=playerReviewTextFixture();editor.setCurrentCampaign(fixture);
    auto &c=PlayerReviewUiProbe::campaign(editor);PlayerDisplayReview::Measurements measurements;
    auto &owner=PlayerReviewUiProbe::owner(editor);owner.setMeasurements(&measurements);
    PlayerReviewUiProbe::open(editor);ui.draw=[&]{PlayerReviewUiProbe::draw(editor);};ui.frame(3);
    const auto open=de?"Spielerfenster öffnen…":"Open player window…";
    const auto confirm=de?"Spielerfenster öffnen":"Open player window";
    ui.clickVisible(open);ui.waitReview(owner);require(owner.prepared()&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),"Actual Editor review omitted captured public text");
    const auto generation=owner.generation(),builds=reviewCount(measurements,PlayerDisplayReview::Stage::Build);
    ui.frame(300);require(owner.generation()==generation&&reviewCount(measurements,PlayerDisplayReview::Stage::Build)==builds,
        "Actual Editor popup rebuilt on unchanged frames");
    c.handouts[0].body="NEW_PUBLIC_BODY";ui.frame(3);
    require(owner.stale()&&owner.review().playerHandouts[0].body=="FROZEN_PUBLIC_TEXT", "Editor stale popup changed captured text");
    ui.clickVisible(de?"Vorschau aktualisieren":"Refresh review");ui.waitReview(owner);ui.frame(3);
    require(owner.generation()==generation+1&&owner.review().playerHandouts[0].body=="NEW_PUBLIC_BODY", "Editor refresh did not capture exactly once");
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_player_review BEFORE INSERT ON handouts BEGIN SELECT RAISE(ABORT, 'injected review save failure'); END;");
    ui.clickVisible(confirm);
    require(ui.popupOpen()&&owner.prepared()&&!owner.stale()&&!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published&&
        contains(ui.rendered,de?"konnte nicht gespeichert":"could not be saved"),"Failed actual arm lost review, hid failure or enabled output");
    sql.exec("DROP TRIGGER reject_player_review;");ui.clickVisible(confirm);
    require(!ui.popupOpen()&&!owner.prepared()&&c.localPlayerDisplay.enabled&&c.localPlayerDisplay.blackout&&PlayerReviewUiProbe::restore(editor),
        "Actual arm retry failed or discarded useful previous selection");
    ui.clickVisible(gm::localization::trUi("Show to players"));ui.waitReview(owner);ui.clickVisible(gm::localization::trUi("Show to players"));
    require(!ui.popupOpen()&&!owner.prepared()&&localPlayerDisplayIsPublished(c.localPlayerDisplay)&&
        reviewCount(measurements,PlayerDisplayReview::Stage::Installation)==1,"Actual reviewed publication/install path did not succeed");
    editor.blackoutLocalPlayerDisplayOutput();
    // Render only the production GM panel: no detached/native window is created.
    c.localPlayerDisplay.enabled=false;ui.frame(3);ui.clickVisible(open);
    editor.discardUnsavedChanges("review UI explicit discard");ui.frame(3);
    require(!owner.prepared()&&!ui.popupOpen()&&!PlayerReviewUiProbe::restore(editor),"Discard retained review or previous selections");
    PlayerReviewUiProbe::open(editor);ui.frame(3);ui.clickVisible(open);
    editor.setCurrentCampaign(c);ui.frame(3);
    require(!owner.prepared()&&!PlayerReviewUiProbe::restore(editor),"Same-ID replacement retained popup consent/restore");
    PlayerReviewUiProbe::open(editor);ui.frame(3);ui.clickVisible(open);
    c.presentationModeActive=true;ui.frame(3);require(!owner.prepared()&&!contains(ui.rendered,"NEW_PUBLIC_BODY"),"Private Editor review survived presentation suppression");
    c.presentationModeActive=false;editor.setCurrentCampaign(fixture);PlayerReviewUiProbe::open(editor);ui.frame(3);
    for(int reset=0;reset<5;++reset)
    {
        c.localPlayerDisplay.enabled=false;ui.frame(3);ui.clickVisible(open);ui.waitReview(owner);ui.clickVisible(confirm);
        ui.clickVisible(gm::localization::trUi("Show to players"));ui.waitReview(owner);bool called=false;
        const auto originalId=c.metadata.id;
        {
            PlayerReviewSubscription subscription([&](const ProjectEvent &event){
                if(called||event.type!=ProjectEventType::CampaignSaved||event.campaignId!=originalId)return;
                called=true;
                if(reset==0)owner.clear();
                else if(reset==3){const auto selected=c.localPlayerDisplay.publicHandoutIds;c.localPlayerDisplay.publicHandoutIds.clear();
                    owner.prepare(c,PlayerDisplayReview::currentLocaleCode());c.localPlayerDisplay.publicHandoutIds=selected;}
                else if(reset==4){const auto text=c.handouts[0].body;c.handouts[0].body="TEMPORARY_INVALIDATION";
                    owner.knownCurrent(c,PlayerDisplayReview::currentLocaleCode());c.handouts[0].body=text;}
                else {auto replacement=c;if(reset==2){replacement.metadata.id=createEntityId("callback-campaign");replacement.name="CALLBACK_REPLACEMENT";}
                    editor.setCurrentCampaign(std::move(replacement));}
            });
            ui.clickVisible(gm::localization::trUi("Show to players"));
        }
        require(called&&!c.localPlayerDisplay.published&&(!owner.prepared()||owner.stale()),
            "Save subscriber owner/campaign reset reapplied retired review");
        require(!contains(editor.shortcutStatusMessage(),de?"konnte nicht gespeichert":"could not be saved"),
            "Committed but revoked save was misreported as failed persistence");
        if(reset==2)require(c.metadata.id!=originalId&&c.name=="CALLBACK_REPLACEMENT","Reviewed staged campaign overwrote callback-loaded campaign");
        editor.setCurrentCampaign(fixture);PlayerReviewUiProbe::open(editor);ui.frame(3);
    }
    owner.setMeasurements(nullptr);PlayerReviewUiProbe::close(editor);ui.frame(3);
}
