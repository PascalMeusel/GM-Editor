std::uint64_t reviewCount(const PlayerDisplayReview::Measurements &m,PlayerDisplayReview::Stage stage)
{std::uint64_t result=0;for(const auto &row:m.values)result+=row[std::size_t(stage)].count;return result;}
Campaign playerReviewTextFixture()
{
    auto c=reviewedUiCampaign(false);auto &d=c.localPlayerDisplay;
    d.mapOnly=d.showMap=d.showSharedArt=d.showInitiative=d.showVisibleNpcs=d.showParty=false;
    d.showPublicHandouts=true;HandoutModel h;h.title="FROZEN_PUBLIC_TITLE";h.body="FROZEN_PUBLIC_TEXT";
    c.handouts.push_back(h);d.publicHandoutIds={h.metadata.id};return c;
}
void playerReviewGmUi(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);
    ui.context(std::string("Frozen legacy player review")+(de?" DE":" EN")+(compact?" compact":" normal"));
    auto c=playerReviewTextFixture();GMScreenController gm(c,nullptr);PlayerDisplayReview::Measurements measurements;
    auto &owner=PlayerReviewUiProbe::owner(gm);owner.setMeasurements(&measurements);
    ui.draw=[&]{PlayerReviewUiProbe::draw(gm);};ui.frame(3);
    ui.clickVisible(de?"Spielerfenster öffnen…":"Open player window…");
    ui.waitReview(owner);
    require(owner.prepared()&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),"Actual legacy popup lacks captured public text");
    const auto generation=owner.generation(),builds=reviewCount(measurements,PlayerDisplayReview::Stage::Build);
    ui.frame(300);require(owner.generation()==generation&&reviewCount(measurements,PlayerDisplayReview::Stage::Build)==builds,
        "Legacy popup rebuilt review during 300 unchanged frames");
    c.handouts[0].body="CHANGED_LIVE_TEXT";ui.frame(3);
    require(owner.stale()&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT")&&!contains(ui.rendered,"CHANGED_LIVE_TEXT"),
        "Legacy stale preview consumed live text");
    ui.clickVisible(de?"Vorschau aktualisieren":"Refresh review");ui.waitReview(owner);ui.frame(3);
    require(owner.generation()==generation+1&&contains(ui.rendered,"CHANGED_LIVE_TEXT")&&reviewCount(measurements,PlayerDisplayReview::Stage::Build)==builds+1,
        "Explicit legacy refresh did not capture exactly once");
    ui.clickVisible(gm::localization::trUi("Cancel"));require(!owner.prepared()&&!ui.popupOpen()&&!c.localPlayerDisplay.enabled,"Cancel retained preview or armed display");
    ui.clickVisible(de?"Spielerfenster öffnen…":"Open player window…");
    ui.waitReview(owner);
    ui.clickVisible(de?"Spielerfenster öffnen":"Open player window");
    require(c.localPlayerDisplay.enabled&&c.localPlayerDisplay.blackout&&!owner.prepared()&&PlayerReviewUiProbe::restore(gm),
        "Legacy arm changed its deferred-save/blackout behavior or lost previous selection");
    gm.resetDrafts();require(!PlayerReviewUiProbe::restore(gm),"Legacy campaign reset retained previous-campaign restore selection");
    c.localPlayerDisplay.enabled=false;ui.frame(3);ui.clickVisible(de?"Spielerfenster öffnen…":"Open player window…");
    c.presentationModeActive=true;ui.frame(3);
    require(!owner.prepared()&&!contains(ui.rendered,"CHANGED_LIVE_TEXT"),"Legacy private review survived presentation suppression");
}
