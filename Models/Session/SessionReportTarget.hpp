#pragma once
#include "Models/Session/SessionStartReview.hpp"

namespace SessionReport
{
struct Target
{
    bool valid = false, completed = false;
    EntityId campaignId, sessionId;
    std::string title;
};
inline bool available(const SessionModel &session)
{ return std::find(session.metadata.tags.begin(), session.metadata.tags.end(), "archived") == session.metadata.tags.end(); }
inline bool completed(const SessionModel &session)
{ return available(session) && !session.endedAtUtc.empty() && !session.liveModeActive; }
inline const SessionModel *latestCompleted(const Campaign &campaign)
{
    const SessionModel *latest = nullptr;
    for (const auto &session : campaign.sessions) if (completed(session) &&
        (!latest || session.endedAtUtc > latest->endedAtUtc ||
        (session.endedAtUtc == latest->endedAtUtc && session.metadata.id > latest->metadata.id))) latest = &session;
    return latest;
}
inline const SessionModel *resolve(const Campaign &campaign, const EntityId &explicitCompleted = {})
{
    if (!explicitCompleted.empty()) {
        const auto *session = SessionPlan::findSession(campaign, explicitCompleted);
        return session && completed(*session) ? session : nullptr;
    }
    if (const auto *active = SessionControl::activeSession(campaign)) if (available(*active)) return active;
    return latestCompleted(campaign);
}
inline Target review(const Campaign &campaign, const EntityId &explicitCompleted = {})
{
    const auto *session = resolve(campaign, explicitCompleted);
    return session ? Target{true, completed(*session), campaign.metadata.id, session->metadata.id, session->title} : Target{};
}
inline const SessionModel *validate(const Campaign &campaign, const Target &target)
{
    if (!target.valid || target.campaignId != campaign.metadata.id) return nullptr;
    const auto *session = SessionPlan::findSession(campaign, target.sessionId);
    if (!session || !available(*session) || session->title != target.title || completed(*session) != target.completed) return nullptr;
    if (!target.completed && (SessionControl::activeSession(campaign) != session || !session->endedAtUtc.empty())) return nullptr;
    return session;
}
struct Owner
{
    SessionPlan::StartReview start;
    Target report, output;
    EntityId completedId;
    std::string reportError;
};
}
