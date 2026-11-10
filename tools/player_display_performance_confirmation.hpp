#pragma once
#include "player_display_performance_durability.hpp"

namespace player_perf
{
// Appended after the existing Editor Cancel/retirement observation. Every
// action below uses the parent's observed item and queued mouse input helper.
struct ButtonConfirmation
{
    enum class Step { Open, PrepareArm, Arm, Armed, ArmReceipt, OpenPublish, PreparePublish,
        Publish, Pixels, PublishReceipt, Blackout, Black, Close, Closed, CloseReceipt, Done };
    Step step=Step::Open; int frames=0;
    bool noSnapshots=false,releasePending=false,armPixels=false,publishPixels=false,blackPixels=false;
    ImTextureID sentinel=0,publishedTexture=0;
    std::uint64_t initialPayload=0;
    std::string initialDescription,beforeDescription,beforeBackup,beforeRecovery;
    bool beforeEnabled=false;
    std::size_t buttonIndex=0,armIndex=0,publishIndex=0,blackIndex=0;
    Clock::time_point releaseAt{},armAt{},publishAt{},blackAt{},started=Clock::now();
    std::optional<DurabilityReceipt> receipt;
    Step releasedStep=Step::Open;
    template<class Popup> ButtonConfirmation(Popup &p,bool disabledSnapshots,ImTextureID ordinary)
        :noSnapshots(disabledSnapshots),sentinel(ordinary)
    {
        const auto &c=p.editor.currentCampaign();initialDescription=c.description;
        initialPayload=localPlayerDisplayPayloadFingerprint(c.localPlayerDisplay);
        require(!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published,"Button walkthrough must begin disabled");
    }
    bool complete()const{return step==Step::Done;}
    const char *label()const
    {
        switch(step){
        case Step::PrepareArm:case Step::PreparePublish:return "editor_button_preparing";
        case Step::Arm:return "editor_button_arm_input";
        case Step::Armed:return "editor_button_black_window_control";
        case Step::Publish:return "editor_button_publish_input";
        case Step::Pixels:return "editor_button_first_pixels_control";
        case Step::ArmReceipt:case Step::PublishReceipt:case Step::CloseReceipt:return "button_snapshot_readback_control";
        case Step::Blackout:case Step::Black:return "editor_button_blackout_control";
        case Step::Close:case Step::Closed:return "editor_button_close_control";
        default:return "editor_button_review_control";
        }
    }
    void next(Step value){step=value;frames=0;}
    template<class Popup> bool click(Popup &p,const char *caption,const char *action,std::uint64_t frame)
    {
        if(p.clickStep==2){
            const auto &c=p.editor.currentCampaign();const auto &save=ProjectSaveService::getInstance().state();
            beforeDescription=c.description;beforeEnabled=c.localPlayerDisplay.enabled;
            beforeBackup=save.lastBackupPath;beforeRecovery=save.lastRecoverySnapshotPath;
            releaseAt=Clock::now();releasedStep=step;releasePending=true;buttonIndex=p.metrics.buttons.size();
            p.metrics.buttons.push_back({action,caption,p.clickId,frame});
            p.metrics.buttons.back().beforeReviewStages=p.metrics.reviewStages;
        }
        return p.click(caption);
    }
    template<class Popup> void verifyReceipt(Popup &p)
    {
        if(!noSnapshots){require(receipt.has_value(),"Successful UI save has no captured durability receipt");
            p.metrics.time("button_snapshot_readback_outside_input",-1,[&]{receipt->verify(p.fixture,p.metrics);});receipt.reset();}
    }
    template<class Popup> void before(Popup &p,std::uint64_t frame)
    {
        require(Clock::now()-started<std::chrono::seconds(45),"Actual successful-button walkthrough exceeded45seconds");
        switch(step){
        case Step::Open:if(click(p,"Open player window…","editor_open_review",frame))next(Step::PrepareArm);break;
        case Step::Arm:if(click(p,"Open player window","editor_arm",frame))next(Step::Armed);break;
        case Step::ArmReceipt:verifyReceipt(p);next(Step::OpenPublish);break;
        case Step::OpenPublish:if(click(p,"Show to players","editor_publish_review",frame))next(Step::PreparePublish);break;
        case Step::Publish:if(click(p,"Show to players","editor_publish",frame))next(Step::Pixels);break;
        case Step::PublishReceipt:verifyReceipt(p);requireCounts(1,"successful button display after collection");next(Step::Blackout);break;
        case Step::Blackout:if(click(p,"Blackout","editor_blackout",frame))next(Step::Black);break;
        case Step::Close:if(click(p,"Close player window","editor_close",frame))next(Step::Closed);break;
        case Step::CloseReceipt:verifyReceipt(p);next(Step::Done);break;
        default:break;
        }
    }
    template<class Popup> void captureSave(Popup &p,const char *reason,bool afterEnabled)
    {
        require(!receipt,"A later button save overtook its prior snapshot verification");
        const auto state=ProjectSaveService::getInstance().state();
        require(state.campaignId==p.fixture.campaign.metadata.id&&state.lastReason==reason&&state.lastError.empty()&&
            !state.lastSavedAtUtc.empty(),"Actual button did not report its expected successful durable save");
        if(!noSnapshots){
            require(state.lastBackupPath!=beforeBackup&&state.lastRecoverySnapshotPath!=beforeRecovery,
                "Actual button reused earlier save snapshot paths");
            receipt=DurabilityReceipt{state,beforeDescription,p.editor.currentCampaign().description,
                p.metrics.buttons[buttonIndex].action,beforeEnabled,-1,afterEnabled};
        }
    }
    template<class Popup> void response(Popup &p,std::uint64_t frame,double renderMs)
    {
        if(!releasePending)return;releasePending=false;const auto observed=Clock::now();
        auto &b=p.metrics.buttons[buttonIndex];b.afterReviewStages=p.metrics.reviewStages;
        const auto &d=p.editor.currentCampaign().localPlayerDisplay;
        switch(releasedStep){
        case Step::Open:case Step::OpenPublish:
            require(p.owner().preparing()||p.owner().prepared(),"Actual launcher did not start its owned review");break;
        case Step::Arm:
            require(d.enabled&&d.blackout&&!d.published&&!p.owner().prepared(),"Actual Open player window did not arm black and retire review");
            captureSave(p,"enable reviewed local player display",true);armIndex=buttonIndex;armAt=releaseAt;break;
        case Step::Publish:
            require(localPlayerDisplayIsPublished(d)&&PlayerReviewUiProbe::publicationProof(PlayerReviewUiProbe::display(p.editor))&&
                !p.owner().prepared(),"Actual Show to players did not install approved output and retire review");
            require(PlayerReviewUiProbe::approved(PlayerReviewUiProbe::display(p.editor)).playerHandouts.at(0).contentHash==p.fixture.replacementHash,
                "Actual publication installed an unreviewed handout hash");
            captureSave(p,"publish reviewed local player display",true);publishIndex=buttonIndex;publishAt=releaseAt;break;
        case Step::Blackout:
            require(d.enabled&&d.blackout&&!d.published&&!PlayerReviewUiProbe::publicationProof(PlayerReviewUiProbe::display(p.editor)),
                "Actual Blackout retained approved output");blackIndex=buttonIndex;blackAt=releaseAt;break;
        case Step::Close:
            require(!d.enabled&&!d.published,"Actual Close player window retained enabled output");
            captureSave(p,"disable local player display",false);break;
        default:throw std::runtime_error("Unexpected successful-button response phase");
        }
        b.applied=true;b.responseFrame=frame;b.releaseToResponse=milliseconds(observed-releaseAt);b.inputFrameRender=renderMs;
    }
    static bool playerReady()
    {
        const auto *w=playerWindow();return w&&w->Active&&!w->Hidden&&!w->SkipItems&&w->Viewport&&
            w->Viewport!=ImGui::GetMainViewport()&&w->Viewport->PlatformHandle&&w->Viewport->DrawData&&w->Viewport->DrawData->Valid;
    }
    template<class Popup> void recordPixels(Popup &p,const char *action,std::size_t index,Clock::time_point released,
        std::uint64_t frame,double renderMs,Clock::time_point observed)
    {p.metrics.pixels.push_back({action,p.metrics.buttons[index].inputFrame,frame,milliseconds(observed-released),renderMs});}
    template<class Popup> void after(Popup &p,std::uint64_t frame,double renderMs)
    {
        ++frames;response(p,frame,renderMs);
        if(step==Step::PrepareArm||step==Step::PreparePublish){
            require(frames<600,"Actual successful-button preparation did not finish");
            if(p.owner().preparing())return;
            require(p.owner().prepared()&&!p.owner().stale()&&p.owner().review().eligible&&
                p.owner().review().playerHandouts.size()==1&&p.owner().review().playerHandouts[0].contentHash==p.fixture.replacementHash,
                "Actual successful-button review did not prepare the changed source");
            next(step==Step::PrepareArm?Step::Arm:Step::Publish);return;
        }
        const auto &d=p.editor.currentCampaign().localPlayerDisplay;
        if((step==Step::Arm||step==Step::Armed)&&d.enabled&&!armPixels&&playerReady()){
            const auto observed=Clock::now();blackoutPixels();armPixels=true;
            recordPixels(p,"editor_arm_black",armIndex,armAt,frame,renderMs,observed);
        }
        if(step==Step::Armed&&armPixels){require(!ImGui::GetTopMostPopupModal(),"Arm left its modal open");next(Step::ArmReceipt);}
        if((step==Step::Publish||step==Step::Pixels)&&localPlayerDisplayIsPublished(d)&&!publishPixels&&playerReady()){
            const auto draw=player_perf::imageDraw();if(draw.texture){const auto observed=Clock::now();
                positivePixels(draw,GL_FRONT,1);mapPixels(p.metrics);publishedTexture=draw.texture;publishPixels=true;
                recordPixels(p,"editor_publish_changed_image",publishIndex,publishAt,frame,renderMs,observed);}
        }
        if(step==Step::Pixels&&publishPixels){require(!ImGui::GetTopMostPopupModal(),"Publish left its modal open");next(Step::PublishReceipt);}
        if((step==Step::Blackout||step==Step::Black)&&d.blackout&&!d.published&&!blackPixels&&playerReady()&&
            !player_perf::imageDraw(publishedTexture).texture){
            const auto observed=Clock::now();blackoutPixels();
            blackPixels=true;recordPixels(p,"editor_blackout_black",blackIndex,blackAt,frame,renderMs,observed);}
        if(step==Step::Black&&blackPixels&&frames>=2){requireCounts(0,"button blackout after collection");
            require(!textureAlive(publishedTexture)&&textureAlive(sentinel),"Button blackout retired wrong texture");next(Step::Close);}
        if(step==Step::Closed&&frames>=4){
            requireCounts(0,"button close after collection");const auto *w=playerWindow();
            require((!w||!w->Active||w->Hidden)&&!d.enabled&&!d.published&&!p.owner().prepared()&&!ImGui::GetTopMostPopupModal()&&
                textureAlive(sentinel)&&localPlayerDisplayPayloadFingerprint(d)==initialPayload&&
                p.editor.currentCampaign().description==initialDescription,"Button cleanup changed selections/canary or retained output");
            p.metrics.checks.push_back("Actual Editor Open/Show/Blackout/Close controls: black opening, changed detached GL_FRONT image quadrants plus map, retired output, preserved selections/description/sentinel");
            next(Step::CloseReceipt);
        }
    }
};
}
