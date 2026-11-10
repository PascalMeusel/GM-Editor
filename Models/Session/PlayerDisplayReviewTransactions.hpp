#pragma once

#include "Models/Session/PlayerDisplayReviewBuilder.hpp"
#include "Models/Session/PlayerDisplayReviewConfirmation.hpp"

namespace PlayerDisplayReview
{
inline bool isCurrent(const Campaign &campaign, const Confirmation &review,
    Measurements *measurements = nullptr, Origin origin = Origin::Unspecified,
    MapRenderCompileMemo *compileMemo = nullptr)
{
    if (!review.eligible || review.contentFingerprint == 0)
        return false;
    const PlayerDisplayReviewSnapshot current = build(campaign, review.locale, BuildPurpose::PlayerOnly, measurements, origin, compileMemo);
    return matchesConfirmation(current,review);
}

inline bool isCurrent(const Campaign &campaign,const PlayerDisplayReviewSnapshot &review,
    Measurements *measurements=nullptr,Origin origin=Origin::Unspecified,MapRenderCompileMemo *compileMemo=nullptr)
{ return isCurrent(campaign,ownConfirmation(review,measurements),measurements,origin,compileMemo); }

inline bool arm(Campaign &campaign, const Confirmation &review, MapRenderCompileMemo *compileMemo = nullptr)
{
    if (!isCurrent(campaign, review, nullptr, Origin::Unspecified, compileMemo))
        return false;
    armLocalPlayerDisplay(campaign.localPlayerDisplay);
    campaign.localPlayerDisplay.publishedReviewFingerprint = 0;
    return true;
}

inline bool publish(Campaign &campaign, const Confirmation &review, MapRenderCompileMemo *compileMemo = nullptr)
{
    if (!isCurrent(campaign, review, nullptr, Origin::Unspecified, compileMemo) || !publishLocalPlayerDisplay(campaign.localPlayerDisplay))
        return false;
    campaign.localPlayerDisplay.publishedReviewFingerprint = review.contentFingerprint;
    return true;
}

inline bool arm(Campaign &campaign,const PlayerDisplayReviewSnapshot &review,MapRenderCompileMemo *compileMemo=nullptr)
{ return arm(campaign,ownConfirmation(review),compileMemo); }
inline bool publish(Campaign &campaign,const PlayerDisplayReviewSnapshot &review,MapRenderCompileMemo *compileMemo=nullptr)
{ return publish(campaign,ownConfirmation(review),compileMemo); }

enum class TransactionStatus { Applied, ReviewChanged, DisplayNotArmed, PersistenceFailed };
struct TransactionResult
{
    TransactionStatus status = TransactionStatus::PersistenceFailed;
    bool applied() const { return status == TransactionStatus::Applied; }
};
using Persist = std::function<bool(Campaign &)>;

namespace detail
{
template<class Review>
inline TransactionResult commitReviewedStateOwned(Campaign &campaign,
                                               const Review &sourceReview,
                                               const Persist &persist, bool publishing, Measurements *measurements = nullptr,
                                               MapRenderCompileMemo *compileMemo = nullptr)
{
    const auto failed = [&](TransactionStatus status) {
        // A failed publication attempt may always remove output, never reveal it.
        // Payload selections stay intact so an unchanged review remains retryable.
        blackoutLocalPlayerDisplay(campaign.localPlayerDisplay);
        return TransactionResult{status};
    };
    try
    {
        // External callers get the same bounded reuse within this operation.
        // No cache can authorize a review or outlive a caller-owned operation.
        MapRenderCompileMemo localMemo;
        if (!compileMemo) compileMemo = &localMemo;
        // Save subscribers may close/reset the originating popup. Own the
        // reviewed data before invoking callbacks or replacing campaign storage.
        const Confirmation review = ownConfirmation(sourceReview,measurements);
        { Measure timing(measurements, Stage::PreValidation, Origin::PreSave);
          if (!isCurrent(campaign, review, measurements, Origin::PreSave, compileMemo)) return failed(TransactionStatus::ReviewChanged); }
        if (publishing && !campaign.localPlayerDisplay.enabled) return failed(TransactionStatus::DisplayNotArmed);
        if (!persist) return failed(TransactionStatus::PersistenceFailed);
        Campaign staged = [&]{ Measure timing(measurements, Stage::CampaignCopy); return campaign; }();
        if (publishing)
        {
            if (!publishLocalPlayerDisplay(staged.localPlayerDisplay)) return failed(TransactionStatus::DisplayNotArmed);
            staged.localPlayerDisplay.publishedReviewFingerprint = review.contentFingerprint;
        }
        else armLocalPlayerDisplay(staged.localPlayerDisplay);
        staged.localPlayerDisplay.metadata.touch();
        staged.metadata.touch();
        { Measure timing(measurements, Stage::Persist);
          if (!persist(staged)) return failed(TransactionStatus::PersistenceFailed); }
        // Managed files or the save callback may change a source while saving.
        // Such content requires a fresh review before any device can display it.
        { Measure timing(measurements, Stage::PostValidation, Origin::PostSave);
          if (!isCurrent(staged, review, measurements, Origin::PostSave, compileMemo)) return failed(TransactionStatus::ReviewChanged); }
        { Measure timing(measurements, Stage::Commit); campaign = std::move(staged); }
        return {TransactionStatus::Applied};
    }
    catch (...) { return failed(TransactionStatus::PersistenceFailed); }
}
} // namespace detail

inline TransactionResult commitReviewedState(Campaign &campaign,const Confirmation &review,
    const Persist &persist,bool publishing,Measurements *measurements=nullptr,MapRenderCompileMemo *compileMemo=nullptr)
{ return detail::commitReviewedStateOwned(campaign,review,persist,publishing,measurements,compileMemo); }
inline TransactionResult commitReviewedState(Campaign &campaign,const PlayerDisplayReviewSnapshot &review,
    const Persist &persist,bool publishing,Measurements *measurements=nullptr,MapRenderCompileMemo *compileMemo=nullptr)
{ return detail::commitReviewedStateOwned(campaign,review,persist,publishing,measurements,compileMemo); }

// These operations replace campaign storage only after a successful durable
// save. Callers must reacquire entity pointers and perform device work afterwards.
inline TransactionResult armAndSave(Campaign &campaign,const Confirmation &review,const Persist &persist,
    Measurements *measurements=nullptr,MapRenderCompileMemo *compileMemo=nullptr)
{ return commitReviewedState(campaign,review,persist,false,measurements,compileMemo); }
inline TransactionResult publishAndSave(Campaign &campaign,const Confirmation &review,const Persist &persist,
    Measurements *measurements=nullptr,MapRenderCompileMemo *compileMemo=nullptr)
{ return commitReviewedState(campaign,review,persist,true,measurements,compileMemo); }

inline TransactionResult armAndSave(Campaign &campaign, const PlayerDisplayReviewSnapshot &review, const Persist &persist,
    Measurements *measurements = nullptr, MapRenderCompileMemo *compileMemo = nullptr)
{ return commitReviewedState(campaign, review, persist, false, measurements, compileMemo); }

inline TransactionResult publishAndSave(Campaign &campaign, const PlayerDisplayReviewSnapshot &review, const Persist &persist,
    Measurements *measurements = nullptr, MapRenderCompileMemo *compileMemo = nullptr)
{ return commitReviewedState(campaign, review, persist, true, measurements, compileMemo); }

inline bool publishedStateIsCurrent(const Campaign &campaign, const std::string &locale = currentLocaleCode())
{
    if (!localPlayerDisplayIsPublished(campaign.localPlayerDisplay) ||
        campaign.localPlayerDisplay.publishedReviewFingerprint == 0)
        return false;
    const PlayerDisplayReviewSnapshot current = build(campaign, locale, BuildPurpose::PlayerOnly);
    return current.eligible && current.contentFingerprint == campaign.localPlayerDisplay.publishedReviewFingerprint;
}
} // namespace PlayerDisplayReview
