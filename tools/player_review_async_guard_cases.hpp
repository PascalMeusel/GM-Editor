// Controller API proof, separate from actual pending-button coverage. Rendering
// uses LocalPlayerDisplay's text-only path, never the private map preview.
void playerReviewPendingPublishedGuard(bool de)
{
    using namespace player_review_async_test;
    HeadlessPanel ui(de);ui.context(de?"Pending owner guard preserves published text DE":"Pending owner guard preserves published text EN");
    auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    EditorWindowController editor;Release release{gate};const auto source=playerReviewMemoFixture();
    require(editor.setCurrentCampaign(source),"Cannot set pending published guard fixture");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &owner=PlayerReviewUiProbe::owner(editor);
    auto &display=PlayerReviewUiProbe::display(editor);PlayerReviewSessionTestProbe::configure(owner,gated(gate,trace));
    const auto locale=PlayerDisplayReview::currentLocaleCode();
    require(owner.prepare(c,locale)&&editor.armLocalPlayerDisplayFromReview(owner.review())&&
        editor.publishLocalPlayerDisplay(owner.review()),"Cannot establish published-text positive control");
    ui.draw=[&]{require(!c.localPlayerDisplay.showMap&&!owner.review().playerMapPlan.backgroundVisible,
        "CPU published guard fixture must remain on text-only runtime path");display.update(nullptr);};ui.frame(2);
    require(localPlayerDisplayIsPublished(c.localPlayerDisplay)&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),
        "Published-text positive control is missing");
    const auto before=c.localPlayerDisplay;
    require(owner.requestPreparation(c,locale,9),"Cannot queue same-input published review refresh");entered(gate);
    require(!editor.armLocalPlayerDisplayFromReview(owner.review())&&!editor.publishLocalPlayerDisplay(owner.review()),
        "Pending owned review bypassed direct arm/publish guards");
    ui.frame(3);
    require(c.localPlayerDisplay.enabled==before.enabled&&c.localPlayerDisplay.blackout==before.blackout&&
        c.localPlayerDisplay.published==before.published&&
        c.localPlayerDisplay.publishedReviewFingerprint==before.publishedReviewFingerprint&&
        localPlayerDisplayIsPublished(c.localPlayerDisplay)&&contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),
        "Rejected pending confirmation blacked out or cleared existing approved display/cache");
    owner.clear();require(!gate->timedOut&&!gate->opened,"Clearing pending published review joined worker");
    gate->open();until([&]{return !owner.compileStats().running;},"Published guard worker did not drain");
    ui.draw=[]{};ui.frame();
}
void playerReviewReadyBeforeClose(bool de,int boundary)
{
    using namespace player_review_async_test;
    HeadlessPanel ui(de);ui.context("Completed review before owner boundary "+std::to_string(boundary)+(de?" DE":" EN"));
    auto gate=std::make_shared<Gate>();auto trace=std::make_shared<Trace>();
    PlayerDisplayReview::Measurements measurements;EditorWindowController editor;Release release{gate};
    const auto source=playerReviewMemoFixture();require(editor.setCurrentCampaign(source),"Cannot set close-order fixture");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &owner=PlayerReviewUiProbe::owner(editor);
    PlayerReviewSessionTestProbe::configure(owner,gated(gate,trace,PlayerReviewCompilePhase::Ready));owner.setMeasurements(&measurements);
    PlayerReviewUiProbe::open(editor);ui.draw=[&]{require(!owner.gmMap(),"CPU closed-owner fixture must never draw ready map");PlayerReviewUiProbe::draw(editor);};ui.frame(3);
    ui.clickVisible(de?"Spielerfenster öffnen…":"Open player window…");entered(gate);
    const auto builds=reviewCount(measurements,PlayerDisplayReview::Stage::Build);
    const auto assets=reviewCount(measurements,PlayerDisplayReview::Stage::PreviewAssets);
    gate->open();until([&]{return owner.compileStats().completed>0;},"Completed-before-close worker did not finish");
    // Completion exists, but no draw/poll has yet consumed it. Exercise actual
    // owner lifetime boundaries before the next production adoption opportunity.
    if(boundary==0)PlayerReviewUiProbe::close(editor);
    else if(boundary==1)c.presentationModeActive=true;
    else require(editor.setCurrentCampaign(c),"Cannot replace same-ID campaign before adoption");
    ui.frame(3);
    require(!owner.prepared()&&!owner.preparing()&&!owner.gmMap()&&
        reviewCount(measurements,PlayerDisplayReview::Stage::Build)==builds&&
        reviewCount(measurements,PlayerDisplayReview::Stage::PreviewAssets)==assets,
        "Retired owner performed main-thread canonical/assets adoption of completed worker");
    owner.setMeasurements(nullptr);
}
