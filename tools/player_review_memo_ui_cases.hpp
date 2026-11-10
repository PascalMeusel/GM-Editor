// CPU controller integration, deliberately separate from the existing actual
// button cases. No native platform backend; the visible positive control uses
// text only, while an untextured map exercises authoritative compilation.
Campaign playerReviewMemoFixture()
{
    auto c=playerReviewTextFixture();MapModel map;map.name="MEMO_PUBLIC_MAP";
    map.width=map.height=4;map.cells.resize(16);map.atlasLevel=MapLevel::Tactical;
    map.visibility=MapVisibility::PlayersVisible;map.generation.recipe.styleVersion=1;
    map.generation.recipe.styleSnapshotId.clear();
    c.localPlayerDisplay.publicMapId=map.metadata.id;c.maps.push_back(map);
    // Request the map contract, but display only the text: no background image
    // exists, so runtime rendering cannot enter texture/decode/GL paths.
    c.localPlayerDisplay.showSharedArt=true;c.localPlayerDisplay.showMap=false;
    c.localPlayerDisplay.playerTextScale=1.f;return c;
}
#include "player_publication_copy_ui_cases.hpp"
void playerReviewMemoIntegration(bool de)
{
    HeadlessPanel ui(de);ui.context(de?"Memo controller API DE":"Memo controller API EN");
    EditorWindowController editor;const auto fixture=playerReviewMemoFixture();
    require(editor.setCurrentCampaign(fixture),"Cannot set synthetic memo campaign");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &owner=PlayerReviewUiProbe::owner(editor);
    auto &display=PlayerReviewUiProbe::display(editor);PlayerDisplayReview::Measurements measurements;
    PlayerReviewUiProbe::runtimeMeasurements(editor,&measurements);owner.setMeasurements(&measurements);
    const auto prepare=[&]{require(owner.prepare(c,PlayerDisplayReview::currentLocaleCode()),"Cannot prepare map memo owner");
        auto memo=owner.compileMemo();require(memo&&!memo->empty(),"Review has no admitted map seed");return memo;};
    auto memo=prepare();auto hits=memo->stats().hits;
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_memo_save BEFORE INSERT ON handouts BEGIN SELECT RAISE(ABORT, 'memo save failure'); END;");
    require(!editor.armLocalPlayerDisplayFromReview(owner.review(),&measurements)&&memo->stats().hits==hits+1&&
        !c.localPlayerDisplay.enabled&&owner.prepared(),"Memo hit bypassed real SQL failure or lost retryable review");
    sql.exec("DROP TRIGGER reject_memo_save;");hits=memo->stats().hits;
    require(editor.armLocalPlayerDisplayFromReview(owner.review(),&measurements)&&memo->stats().hits==hits+2&&
        c.localPlayerDisplay.blackout,"Arm retry did not hit both authoritative boundaries");
    ui.draw=[&]{display.update(nullptr);};const auto builds=reviewCount(measurements,PlayerDisplayReview::Stage::Build);
    ui.frame(5);require(reviewCount(measurements,PlayerDisplayReview::Stage::Build)==builds&&
        !localPlayerDisplayIsPublished(c.localPlayerDisplay),"Armed black runtime rebuilt map content");
    owner.clear();memo=prepare();hits=memo->stats().hits;
    require(editor.publishLocalPlayerDisplay(owner.review(),&measurements)&&memo->stats().hits==hits+3&&
        localPlayerDisplayIsPublished(c.localPlayerDisplay),"Publish did not hit pre/post/install canonical compilation");
    ui.frame(3);require(contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),"Published runtime positive text control absent");
    const auto selections=c.localPlayerDisplay.publicHandoutIds;
    const auto beforeBlack=reviewCount(measurements,PlayerDisplayReview::Stage::Build);
    c.localPlayerDisplay.publicHandoutIds.clear();ui.frame(2);c.localPlayerDisplay.publicHandoutIds=selections;ui.frame(2);
    require(c.localPlayerDisplay.blackout&&!c.localPlayerDisplay.published&&
        reviewCount(measurements,PlayerDisplayReview::Stage::Build)==beforeBlack&&!contains(ui.rendered,"FROZEN_PUBLIC_TEXT"),
        "Reverting staged payload revived output or compiled during blackout");
    // A completed persistence subscriber can retire the popup/campaign while
    // its memo is held. Reuse must not override the existing epoch guards.
    for(int reset=0;reset<3;++reset) {
        require(editor.setCurrentCampaign(fixture),"Cannot reset callback fixture");memo=prepare();
        require(editor.armLocalPlayerDisplayFromReview(owner.review()),"Cannot arm callback fixture");
        owner.clear();memo=prepare();hits=memo->stats().hits;bool called=false;
        const auto id=c.metadata.id;
        {
            PlayerReviewSubscription listener([&](const ProjectEvent &event){
                if(called||event.type!=ProjectEventType::CampaignSaved||event.campaignId!=id)return;called=true;
                if(reset==0)owner.clear();else {auto replacement=c;
                    if(reset==2){replacement.metadata.id=createEntityId("memo-switch");replacement.name="NEW_MEMO_CAMPAIGN";}
                    require(editor.setCurrentCampaign(std::move(replacement)),"Cannot replace campaign in save callback");}
            });
            require(!editor.publishLocalPlayerDisplay(owner.review()),"Retired owner/campaign published through held memo");
        }
        require(called&&memo->stats().hits==hits+1&&!c.localPlayerDisplay.published&&!owner.prepared(),
            "Callback fixture missed cache hit or retained revoked output");
        if(reset==2)require(c.metadata.id!=id&&c.name=="NEW_MEMO_CAMPAIGN","Old staged campaign replaced new context");
    }
    // Installation revalidates even when derived map compilation hits. It may
    // reject a public text edit independently of unchanged geometry.
    require(editor.setCurrentCampaign(fixture),"Cannot reset installation fixture");memo=prepare();
    const auto review=owner.review();require(editor.armLocalPlayerDisplayFromReview(owner.review()),"Cannot arm installation fixture");
    require(PlayerDisplayReview::publish(c,review,memo.get()),"Cannot establish installation positive publication flags");
    c.handouts[0].body="CHANGED_BEFORE_INSTALL";hits=memo->stats().hits;
    require(!display.publicationCommitted(review,&measurements,memo.get())&&memo->stats().hits==hits+1&&
        c.localPlayerDisplay.blackout&&!c.localPlayerDisplay.published,"Installation memo bypassed canonical payload rejection");
    owner.clear();PlayerReviewUiProbe::runtimeMeasurements(editor,nullptr);owner.setMeasurements(nullptr);
    ui.draw=[]{};ui.frame();
}
