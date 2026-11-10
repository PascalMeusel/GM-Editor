#pragma once
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include <algorithm>

namespace SessionPlan
{
struct StartReview
{
    bool valid = false, create = false;
    EntityId campaignId, sessionId;
    std::string title;
};
inline StartReview reviewStart(const Campaign &campaign, const std::string &newTitle)
{
    if (SessionControl::activeSession(campaign)) return {};
    if (const auto *next = nextSession(campaign))
        return {true, false, campaign.metadata.id, next->metadata.id, next->title};
    SessionModel fresh;
    return {true, true, campaign.metadata.id, fresh.metadata.id, newTitle};
}
inline bool currentStartReview(const Campaign &campaign, const StartReview &review)
{
    if (!review.valid || review.campaignId != campaign.metadata.id || review.sessionId.empty() ||
        SessionControl::activeSession(campaign)) return false;
    const auto *next = nextSession(campaign);
    if (review.create)
        return !next && !findSession(campaign, review.sessionId) &&
            std::find(campaign.sessionIds.begin(), campaign.sessionIds.end(), review.sessionId) == campaign.sessionIds.end();
    return next && next->metadata.id == review.sessionId && next->title == review.title &&
        next->startedAtUtc.empty() && next->endedAtUtc.empty() && !next->liveModeActive &&
        std::find(next->metadata.tags.begin(), next->metadata.tags.end(), "archived") == next->metadata.tags.end();
}
}
