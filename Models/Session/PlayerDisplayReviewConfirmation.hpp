#pragma once
#include "Models/Session/PlayerDisplayReviewTypes.hpp"
#include "Models/Session/PlayerReviewMeasurements.hpp"
#include <vector>

namespace PlayerDisplayReview
{
struct ImageBinding { std::string projectRelativePath,contentHash; };
// Owned confirmation data, not a render plan or publication authority. Every
// consumer still rebuilds the canonical projection and verifies reviewed bytes.
struct Confirmation
{
    bool eligible=false,hasMapPreview=false;
    std::string locale;
    std::uint64_t contentFingerprint=0;
    EntityId mapId;
    std::uint64_t mapRevision=0,mapContentHash=0;
    std::vector<ImageBinding> images;
};
inline Confirmation ownConfirmation(const PlayerDisplayReviewSnapshot &review,Measurements *measurements=nullptr)
{
    Measure ownership(measurements,Stage::ConfirmationOwnership);
    Confirmation value;
    value.eligible=review.eligible;value.hasMapPreview=review.hasMapPreview;
    value.locale=review.locale;value.contentFingerprint=review.contentFingerprint;
    if(review.hasMapPreview){
        value.mapId=review.playerMapPlan.mapId;value.mapRevision=review.playerMapPlan.mapRevision;
        value.mapContentHash=review.playerMapPlan.contentHash;
    }
    const auto add=[&](const std::string &path,const std::string &hash){
        if(!path.empty())value.images.push_back({path,hash});
    };
    // Preserve ordering and duplicates. Conflicting hashes are rejected by
    // the shared capture implementation rather than silently merged here.
    for(const auto &handout:review.playerHandouts)add(handout.projectRelativePath,handout.contentHash);
    if(review.hasMapPreview&&review.playerMapPlan.backgroundVisible){
        add(review.playerMapPlan.backgroundInteractiveProjectRelativePath,review.playerMapPlan.backgroundInteractiveContentHash);
        add(review.playerMapPlan.backgroundExportProjectRelativePath,review.playerMapPlan.backgroundExportContentHash);
    }
    return value;
}
inline Confirmation ownConfirmation(const Confirmation &review,Measurements *measurements=nullptr)
{
    Measure ownership(measurements,Stage::ConfirmationOwnership);
    return review;
}
inline bool matchesConfirmation(const PlayerDisplayReviewSnapshot &current,const Confirmation &review)
{
    return review.eligible&&review.contentFingerprint!=0&&current.eligible&&current.locale==review.locale&&
        current.contentFingerprint==review.contentFingerprint&&current.hasMapPreview==review.hasMapPreview&&
        (!review.hasMapPreview||(current.playerMapPlan.mapId==review.mapId&&
            current.playerMapPlan.mapRevision==review.mapRevision&&current.playerMapPlan.contentHash==review.mapContentHash));
}
}
