#include "Models/Session/SessionPlanServiceInternal.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include "Models/Story/SceneLaunchService.hpp"
#include <algorithm>

namespace SessionPlan
{
using detail::active;
using detail::archived;
using detail::blank;
using detail::commit;
using detail::editable;
using detail::error;
using detail::success;
using detail::threadForScene;

namespace
{
void applyWrapUpDraft(SessionModel &session, const WrapUpDraft &draft)
{
    session.summary = draft.summary; session.recap = draft.recap;
    session.unresolvedThreads = draft.unresolvedThreads; session.lootAwarded = draft.lootAwarded;
    session.xpMilestones = draft.xpMilestones; session.npcsMet = draft.npcsMet;
    session.locationsVisited = draft.locationsVisited; session.openHooks = draft.openHooks;
    if (!draft.pendingNote.body.empty())
    {
        auto note = draft.pendingNote;
        if (note.timestampUtc.empty()) note.timestampUtc = currentUtcTimestamp();
        session.notes.push_back(std::move(note));
    }
    session.metadata.touch();
}
Result finishInTransaction(Campaign &campaign, const EntityId &id, const WrapUpDraft *draft, const Persist &persist)
{
    // Blackout is a safety action even when the following disk write fails.
    blackoutLocalPlayerDisplay(campaign.localPlayerDisplay);
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = active(session);
        if (!check.applied()) return check;
        if (draft) applyWrapUpDraft(*session, *draft);
        SessionControl::endLiveSession(value);
        session->liveState.initialized = true; session->liveState.phase = SessionPhase::WrapUp;
        return success(id);
    });
}
}
Result finish(Campaign &campaign, EntityId id, const Persist &persist)
{ return finishInTransaction(campaign, id, nullptr, persist); }
Result finishWithDraft(Campaign &campaign, EntityId id, const WrapUpDraft &draft, const Persist &persist)
{ return finishInTransaction(campaign, id, &draft, persist); }
Result saveWrapUpDraft(Campaign &campaign, EntityId id, const WrapUpDraft &draft, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = active(session);
        if (!check.applied()) return check;
        applyWrapUpDraft(*session, draft);
        return success(id);
    });
}
Result removeNote(Campaign &campaign, EntityId id, EntityId noteId, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = active(session);
        if (!check.applied()) return check;
        const auto note = std::find_if(session->notes.begin(), session->notes.end(), [&](const auto &item) { return item.metadata.id == noteId; });
        if (note == session->notes.end()) return error(Status::NotFound, "Session note not found.");
        session->notes.erase(note); session->metadata.touch();
        return success(id);
    });
}
Result advanceClock(Campaign &campaign, const std::string &amount, const std::string &reason, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        const auto *session = SessionControl::activeSession(value);
        const EntityId sessionId = session ? session->metadata.id : EntityId{};
        if (session && !reason.empty())
        {
            if (!SessionControl::advanceInWorldTime(value, amount, reason, InWorldTime::formatDateTime(value.clock)))
                return error(Status::Invalid, "Enter a valid time amount and reason.");
        }
        else
        {
            long long minutes = 0;
            if (!InWorldTime::parseDurationMinutes(amount, value.clock.calendar, minutes))
                return error(Status::Invalid, "Enter a valid time amount.");
            InWorldTime::advanceMinutes(value.clock, minutes);
        }
        return success(sessionId);
    });
}
Result setClockDateTime(Campaign &campaign, const InWorldDateTime &dateTime, const std::string &reason, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        const auto *session = SessionControl::activeSession(value);
        const EntityId sessionId = session ? session->metadata.id : EntityId{};
        if (session && !reason.empty())
        {
            if (!SessionControl::setInWorldDateTime(value, dateTime, reason))
                return error(Status::Invalid, "Enter a valid date and reason.");
        }
        else
            InWorldTime::setDateTime(value.clock, dateTime, value.clock.useSystemTime || value.clock.running);
        return success(sessionId);
    });
}
Result setClockCalendar(Campaign &campaign, const CampaignCalendarModel &calendar, const InWorldDateTime &dateTime,
                       double multiplier, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        value.clock.calendar = calendar;
        value.clock.multiplier = multiplier;
        InWorldTime::setDateTime(value.clock, dateTime, value.clock.useSystemTime || value.clock.running);
        return success();
    });
}
Result setClockRunning(Campaign &campaign, bool running, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        InWorldTime::setRunning(value.clock, running);
        return success();
    });
}
Result setClockMultiplier(Campaign &campaign, double multiplier, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        InWorldTime::setMultiplier(value.clock, multiplier);
        return success();
    });
}
Result resetClockToSystemTime(Campaign &campaign, const Persist &persist)
{
    return commit(campaign, persist, [](Campaign &value) {
        InWorldTime::setUseSystemTime(value.clock);
        return success();
    });
}
}

