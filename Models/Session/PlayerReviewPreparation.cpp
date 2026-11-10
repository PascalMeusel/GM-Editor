#include "Models/Session/PlayerReviewSessionInternal.hpp"
#include "Models/Session/PlayerDisplayReviewBuilder.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include <utility>

namespace
{
PlayerReviewCompileInput inputFor(const Campaign &campaign,const std::string &locale,bool measure)
{
    using namespace PlayerDisplayReview;
    PlayerReviewCompileInput input;input.measure=measure;
    const auto &display=campaign.localPlayerDisplay;
    const auto *map=(display.showMap||display.showSharedArt)&&!display.publicMapId.empty()?publicMap(campaign):nullptr;
    if(!map)return input;
    input.gmMap=std::make_shared<const MapModel>(*map);
    input.playerMap=std::make_shared<const MapModel>(playerSafeMapSource(*map));
    input.gmRequest=gmCompileRequest(*map,locale);
    input.playerRequest=playerCompileRequest(*input.playerMap,locale);
    for(const auto &feature:input.playerMap->generation.graph.features)
        input.playerPolicy.push_back(generatedFeatureIsPublic(campaign,*map,feature)?1:0);
    return input;
}
bool keysSupported(const PlayerReviewCompileInput &input,const PlayerReviewCompileOptions &options)
{
    if(!input.gmMap)return true;
    gm::map_render::MapRenderCompileMemo gm(options.limits),player(options.limits);
    if(tryCompileMapRenderPlanWithMemo(*input.gmMap,input.gmRequest,gm).status==MapRenderCompileStatus::Unavailable)return false;
    auto request=input.playerRequest;
    request.additionalGeneratedFeatureVisibility=[&input](const MapGeneratedFeature &feature){
        const auto index=static_cast<std::size_t>(&feature-input.playerMap->generation.graph.features.data());
        return index<input.playerPolicy.size()&&input.playerPolicy[index]!=0;
    };
    return tryCompileMapRenderPlanWithMemo(*input.playerMap,request,player).status!=MapRenderCompileStatus::Unavailable;
}
void mergeWorkerMeasurements(PlayerDisplayReview::Measurements *sink,const PlayerReviewCompileResult &result)
{
    if(!sink)return;
    using namespace PlayerDisplayReview;
    auto &origin=sink->values[static_cast<std::size_t>(Origin::PreparationWorker)];
    const auto add=[&](Stage stage,const MapRenderPlanMeasurements &source){
        const auto &total=source.values[static_cast<std::size_t>(MapRenderPlanCompileStage::Total)];
        auto &target=origin[static_cast<std::size_t>(stage)];
        target.count+=total.calls;target.milliseconds+=double(total.nanoseconds)/1000000.0;
    };
    add(Stage::GmCompile,result.gmMeasurements);add(Stage::PlayerCompile,result.playerMeasurements);
}
}

bool PlayerReviewSession::preparing() const
{return preparationStatus()==PlayerReviewPreparationStatus::Preparing;}
PlayerReviewPreparationStatus PlayerReviewSession::preparationStatus() const
{return preparation_?preparation_->status:PlayerReviewPreparationStatus::Idle;}
bool PlayerReviewSession::foregroundRequired() const
{
    return preparation_&&preparation_->campaign&&
        (preparation_->status==PlayerReviewPreparationStatus::ForegroundRequired||
         preparation_->status==PlayerReviewPreparationStatus::Failed);
}
std::uint64_t PlayerReviewSession::preparationSerial() const
{return preparation_?preparation_->serial:0;}
PlayerReviewCompileStats PlayerReviewSession::compileStats() const
{return preparation_&&preparation_->worker?preparation_->worker->stats():PlayerReviewCompileStats{};}
PlayerReviewAcceptedCompileStats PlayerReviewSession::acceptedCompileStats() const
{return preparation_?preparation_->accepted:PlayerReviewAcceptedCompileStats{};}
void PlayerReviewSession::cancelPreparation()
{
    if(preparation_)
    {
        if(preparation_->worker)preparation_->worker->cancel();
        preparation_->campaign=nullptr;
        preparation_->status=PlayerReviewPreparationStatus::Idle;preparation_->fallback.reset();
    }
    stale_=true;preparationVeto_=true;error_.clear();++generation_;
}
void PlayerReviewSession::rejectPreparation(const std::string &message)
{
    if(preparation_)
    {
        if(preparation_->worker)preparation_->worker->cancel();
        preparation_->campaign=nullptr;
        preparation_->status=PlayerReviewPreparationStatus::Failed;preparation_->fallback.reset();
    }
    stale_=true;preparationVeto_=true;error_=message;
}
void PlayerReviewSession::invalidate()
{
    if(preparing()||foregroundRequired())rejectPreparation("The review was invalidated. Request a new review.");
    stale_=true;
}
bool PlayerReviewSession::preparationMatches(const Campaign &campaign,const std::string &locale,std::uint64_t epoch) const
{
    return preparation_&&preparation_->campaign==&campaign&&preparation_->campaignId==campaign.metadata.id&&
        preparation_->locale==locale&&preparation_->contextEpoch==epoch&&
        preparation_->root==ApplicationPaths::writableRoot()&&!campaign.presentationModeActive;
}
bool PlayerReviewSession::requestPreparation(const Campaign &campaign,const std::string &locale,std::uint64_t epoch)
{
    cancelPreparation();
    try
    {
        if(campaign.presentationModeActive){error_="Return to GM mode to prepare a review.";return false;}
        if(!preparation_)preparation_=std::make_unique<Preparation>();
        auto &p=*preparation_;
        p.campaign=&campaign;p.campaignId=campaign.metadata.id;p.locale=locale;p.contextEpoch=epoch;
        p.root=ApplicationPaths::writableRoot();p.environment=playerReviewCompileEnvironment();
        p.sourceKey=PlayerDisplayReview::sourceFingerprint(campaign,locale,false,false);
        p.accepted={};
        auto input=inputFor(campaign,locale,measurements_!=nullptr);
        if(!p.environment||!keysSupported(input,compileOptions_))
        {
            p.status=PlayerReviewPreparationStatus::ForegroundRequired;
            error_="This map requires explicit foreground preparation, which may pause the editor.";
            return true; // Accepted request; explicit fallback is immediately available.
        }
        if(!p.worker)p.worker=std::make_unique<PlayerReviewCompileWorker>(compileOptions_);
        p.serial=p.worker->request(std::move(input));p.status=PlayerReviewPreparationStatus::Preparing;
        return true;
    }
    catch(const std::exception &e)
    {if(preparation_)preparation_->status=PlayerReviewPreparationStatus::Failed;error_=e.what();return false;}
}
bool PlayerReviewSession::pollPreparation(const Campaign &campaign,const std::string &locale,std::uint64_t epoch)
{
    if(!preparing())return false;
    try
    {
        if(!preparationMatches(campaign,locale,epoch))
        {rejectPreparation("The review context changed. Request a new review.");return false;}
        auto result=preparation_->worker->takeCompletion();
        if(!result)return false;
        auto &p=*preparation_;
        if(result->serial!=p.serial)return false;
        // This cheap key can only veto. Exact fresh map selection/key matching
        // below remains mandatory; no revision or fingerprint authorizes reuse.
        if(p.sourceKey!=PlayerDisplayReview::sourceFingerprint(campaign,locale,false,false))
        {rejectPreparation("The selected content changed. Request a new review.");return false;}
        // Background durations are separate from the main Build interval.
        mergeWorkerMeasurements(measurements_,*result);
        if(result->foregroundRequired||!result->error.empty()||!result->environment||
           result->environment!=p.environment||result->environment!=playerReviewCompileEnvironment())
        {
            p.status=result->error.empty()?PlayerReviewPreparationStatus::ForegroundRequired:PlayerReviewPreparationStatus::Failed;
            error_=result->error.empty()?"This map requires explicit foreground preparation, which may pause the editor.":result->error;
            p.fallback=std::move(result);return false;
        }
        auto next=std::make_unique<State>();next->campaignId=campaign.metadata.id;next->display=campaign.localPlayerDisplay;
        next->compileMemo=result->player;
        MapRenderCompileStatus status=MapRenderCompileStatus::Hit;
        next->review=PlayerDisplayReview::build(campaign,locale,PlayerDisplayReview::BuildPurpose::Comparison,
            measurements_,PlayerDisplayReview::Origin::Open,result->player.get(),result->gm.get(),true,&status);
        if(status!=MapRenderCompileStatus::Hit)
        {
            if(status==MapRenderCompileStatus::Miss)
            {rejectPreparation("The map or its public visibility changed. Request a new review.");return false;}
            p.status=PlayerReviewPreparationStatus::ForegroundRequired;p.fallback=std::move(result);
            error_="Prepared geometry could not be reused. Explicit foreground preparation may pause the editor.";return false;
        }
        const PlayerReviewAcceptedCompileStats accepted{
            result->gm?result->gm->stats():gm::map_render::MapRenderCompileMemoStats{},
            result->player?result->player->stats():gm::map_render::MapRenderCompileMemoStats{}};
        if(!finishPreparedState(campaign,locale,std::move(next),false))
        {p.status=PlayerReviewPreparationStatus::Failed;p.campaign=nullptr;return false;}
        p.status=PlayerReviewPreparationStatus::Idle;p.campaign=nullptr;p.accepted=accepted;
        return true;
    }
    catch(const std::exception &e){rejectPreparation(e.what());return false;}
}
bool PlayerReviewSession::prepareInForeground(const Campaign &campaign,const std::string &locale,std::uint64_t epoch)
{
    if(!foregroundRequired())return false;
    try
    {
        if(!preparationMatches(campaign,locale,epoch))
        {rejectPreparation("The review context changed. Request a new review.");return false;}
        auto &p=*preparation_;
        if(p.sourceKey!=PlayerDisplayReview::sourceFingerprint(campaign,locale,false,false))
        {rejectPreparation("The selected content changed. Request a new review.");return false;}
        auto next=std::make_unique<State>();next->campaignId=campaign.metadata.id;next->display=campaign.localPlayerDisplay;
        next->compileMemo=p.fallback?p.fallback->player:nullptr;
        auto *gm=p.fallback?p.fallback->gm.get():nullptr;
        next->review=PlayerDisplayReview::build(campaign,locale,PlayerDisplayReview::BuildPurpose::Comparison,
            measurements_,PlayerDisplayReview::Origin::Open,next->compileMemo.get(),gm);
        if(!finishPreparedState(campaign,locale,std::move(next),false))
        {p.status=PlayerReviewPreparationStatus::Failed;p.campaign=nullptr;return false;}
        p.status=PlayerReviewPreparationStatus::Idle;p.campaign=nullptr;p.fallback.reset();return true;
    }
    catch(const std::exception &e){error_=e.what();stale_=true;return false;}
}
