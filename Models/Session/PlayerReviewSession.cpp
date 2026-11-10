#include "Models/Session/PlayerReviewSessionInternal.hpp"
#include "Models/Session/PlayerDisplayReviewBuilder.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include <new>
#include <utility>

PlayerReviewSession::PlayerReviewSession()=default;
PlayerReviewSession::PlayerReviewSession(PlayerReviewCompileOptions options):compileOptions_(std::move(options)){}
PlayerReviewSession::~PlayerReviewSession()=default;
void PlayerReviewSession::clear(){cancelPreparation();state_.reset();stale_=false;error_.clear();}
bool PlayerReviewSession::prepared() const
{return state_&&state_->review.eligible&&error_.empty()&&!preparationVeto_&&preparationStatus()==PlayerReviewPreparationStatus::Idle;}

bool PlayerReviewSession::prepare(const Campaign &campaign,const std::string &locale)
{
    using namespace PlayerDisplayReview;
    if(preparation_)
    {
        if(preparation_->worker)preparation_->worker->cancel();
        preparation_->status=PlayerReviewPreparationStatus::Idle;preparation_->campaign=nullptr;preparation_->fallback.reset();
    }
    try
    {
        auto next=std::make_unique<State>();
        // The optional optimization must not make an otherwise valid review
        // unavailable when its ownership allocation cannot be satisfied.
        try { next->compileMemo=std::make_shared<gm::map_render::MapRenderCompileMemo>(); }
        catch(const std::bad_alloc &) {}
        next->campaignId=campaign.metadata.id;next->display=campaign.localPlayerDisplay;
        next->review=build(campaign,locale,BuildPurpose::Comparison,measurements_,Origin::Open,next->compileMemo.get());
        return finishPreparedState(campaign,locale,std::move(next),true);
    }
    catch(const std::exception &e){error_=e.what();stale_=true;return false;}
}

bool PlayerReviewSession::finishPreparedState(const Campaign &campaign,const std::string &locale,
    std::unique_ptr<State> next,bool advanceGeneration)
{
    using namespace PlayerDisplayReview;
    try
    {
        if(!next->review.eligible)
        {
            if(!state_)state_=std::move(next);
            stale_=true;error_="The selected content cannot be reviewed.";return false;
        }
        const auto *map=next->review.hasMapPreview?publicMap(campaign):nullptr;
        if(map)
        {
            Measure filter(measurements_,Stage::PreviewFilter,Origin::Open);
            next->gmMap=filterMapForRenderPlan(*map,next->review.gmMapPlan);
            next->playerMap=filterMapForRenderPlan(playerSafeMapSource(*map),next->review.playerMapPlan);
            next->maps=true;
        }
        {
            Measure assets(measurements_,Stage::PreviewAssets,Origin::Open);
            if(!next->playerAssets.capture(next->review))
            {error_=next->playerAssets.error();stale_=true;return false;}
            // This adapter exists only inside the private GM owner. It must
            // never be passed to a detached player controller or transaction.
            PlayerDisplayReviewSnapshot privateArt;
            privateArt.eligible=true;privateArt.hasMapPreview=next->maps;
            // Asset capture consumes only these byte-bound background fields;
            // copying the private geometry/ink here adds no validation.
            const auto &gm=next->review.gmMapPlan;
            auto &background=privateArt.playerMapPlan;
            background.backgroundVisible=gm.backgroundVisible;
            background.backgroundInteractiveProjectRelativePath=gm.backgroundInteractiveProjectRelativePath;
            background.backgroundInteractiveContentHash=gm.backgroundInteractiveContentHash;
            background.backgroundExportProjectRelativePath=gm.backgroundExportProjectRelativePath;
            background.backgroundExportContentHash=gm.backgroundExportContentHash;
            if(!next->gmAssets.capture(privateArt))
            {
                // Legacy unhashed or unavailable private art is visibly omitted;
                // no preview can fall back to a mutable original path.
                next->gmArtOmitted=privateArt.playerMapPlan.backgroundVisible;
                next->review.gmMapPlan.backgroundVisible=false;
            }
        }
        next->sourceKey=sourceFingerprint(campaign,locale,false,false);
        next->campaign=&campaign;next->root=ApplicationPaths::writableRoot();
        state_=std::move(next);stale_=false;preparationVeto_=false;error_.clear();
        if(advanceGeneration)++generation_;
        return true;
    }
    catch(const std::exception &e){error_=e.what();stale_=true;return false;}
}

bool PlayerReviewSession::knownCurrent(const Campaign &campaign,const std::string &locale)
{
    using namespace PlayerDisplayReview;
    Measure timing(measurements_,Stage::Invalidation,Origin::Runtime);
    if(!prepared()||stale_)return false;
    // Metadata-only checks do not start a verifier worker and do not retire
    // earlier preview bytes on staleness. Confirm still checks actual bytes.
    try
    {
        stale_=state_->campaign!=&campaign||ApplicationPaths::writableRoot()!=state_->root||
            campaign.metadata.id!=state_->campaignId||locale!=state_->review.locale||
            sourceFingerprint(campaign,locale,false,false)!=state_->sourceKey||
            !state_->playerAssets.sourcesCurrent(false)||
            (!state_->gmArtOmitted&&!state_->gmAssets.sourcesCurrent(false));
    }
    catch(...){stale_=true;}
    return !stale_;
}
const PlayerDisplayReviewSnapshot &PlayerReviewSession::review() const
{static const PlayerDisplayReviewSnapshot empty;return state_?state_->review:empty;}
const LocalPlayerDisplayModel &PlayerReviewSession::display() const
{static const LocalPlayerDisplayModel empty;return state_?state_->display:empty;}
const MapModel *PlayerReviewSession::gmMap() const{return state_&&state_->maps?&state_->gmMap:nullptr;}
const MapModel *PlayerReviewSession::playerMap() const{return state_&&state_->maps?&state_->playerMap:nullptr;}
FrozenImageAsset PlayerReviewSession::gmBackground() const
{
    if(!state_)return {};
    const auto &p=state_->review.gmMapPlan;
    return state_->gmAssets.image(p.backgroundInteractiveProjectRelativePath,p.backgroundInteractiveContentHash);
}
FrozenImageAsset PlayerReviewSession::playerImage(const std::string &path,const std::string &hash) const
{return state_?state_->playerAssets.image(path,hash):FrozenImageAsset{};}
bool PlayerReviewSession::gmArtOmitted() const{return state_&&state_->gmArtOmitted;}
std::shared_ptr<gm::map_render::MapRenderCompileMemo> PlayerReviewSession::compileMemo() const
{return state_?state_->compileMemo:nullptr;}
