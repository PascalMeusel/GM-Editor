#pragma once

#include "Models/Campaign/Campaign.hpp"
#include <functional>

namespace SessionPlan
{
enum class Status { Applied, Invalid, NotFound, Conflict, PersistenceFailed, DraftConflict };
struct Result
{
    Status status = Status::Invalid;
    std::string message;
    EntityId sessionId;
    EntityId entryId;
    EntityId sceneId;
    bool applied() const { return status == Status::Applied; }
};
using Persist = std::function<bool(Campaign &)>;
// Successful mutations replace campaign storage. Mutating APIs copy their ID
// arguments and Results own their IDs; callers must reacquire entity pointers.
struct WrapUpDraft
{
    std::string summary, recap, unresolvedThreads, lootAwarded, xpMilestones, npcsMet, locationsVisited, openHooks;
    SessionNoteModel pendingNote;
};
SessionModel *findSession(Campaign &, const EntityId &);
const SessionModel *findSession(const Campaign &, const EntityId &);
SessionModel *nextSession(Campaign &);
const SessionModel *nextSession(const Campaign &);
StoryBeatModel *findScene(Campaign &, const EntityId &);
const StoryBeatModel *findScene(const Campaign &, const EntityId &);
SessionPlanEntry *findEntry(SessionModel &, const EntityId &);
const SessionPlanEntry *findEntry(const SessionModel &, const EntityId &);
Result createSession(Campaign &, const std::string &title, const Persist &);
Result updateDraft(Campaign &, EntityId sessionId, const std::string &title,
                   const std::string &roughNotes, const std::string &scheduledAtUtc, const Persist &);
Result addScene(Campaign &, EntityId sessionId, EntityId storyBeatId, const Persist &);
Result createScene(Campaign &, EntityId sessionId, const std::string &title,
                   const std::string &summary, const Persist &);
Result updateScene(Campaign &, EntityId sceneId, const StoryBeatModel &replacement, const Persist &);
Result updateSceneDraft(Campaign &, EntityId sceneId, const StoryBeatModel &base,
                        const StoryBeatModel &draft, const Persist &);
Result updateEntry(Campaign &, EntityId sessionId, EntityId entryId,
                   const std::string &prepNote, SessionEntryState, const Persist &);
Result moveEntry(Campaign &, EntityId sessionId, EntityId entryId, int offset, const Persist &);
Result removeEntry(Campaign &, EntityId sessionId, EntityId entryId, const Persist &);
Result start(Campaign &, EntityId sessionId, const Persist &);
struct StartReview;
Result startReviewed(Campaign &, const StartReview &, const Persist &);
// Start the selected session and optional first scene in one durable write.
Result startWithEntry(Campaign &, EntityId sessionId, EntityId entryId, const Persist &);
Result activateEntry(Campaign &, EntityId sessionId, EntityId entryId, const Persist &);
Result activateEncounter(Campaign &, EntityId sessionId, EntityId encounterId, const Persist &);
Result setPhase(Campaign &, EntityId sessionId, SessionPhase, const Persist &);
Result finish(Campaign &, EntityId sessionId, const Persist &);
Result finishWithDraft(Campaign &, EntityId sessionId, const WrapUpDraft &, const Persist &);
Result saveWrapUpDraft(Campaign &, EntityId sessionId, const WrapUpDraft &, const Persist &);
Result removeNote(Campaign &, EntityId sessionId, EntityId noteId, const Persist &);
// Advance the campaign clock and its optional active-session/timeline note in
// one durable write. A failed save leaves the clock and notes unchanged.
Result advanceClock(Campaign &, const std::string &amount, const std::string &reason, const Persist &);
Result setClockDateTime(Campaign &, const InWorldDateTime &, const std::string &reason, const Persist &);
Result setClockCalendar(Campaign &, const CampaignCalendarModel &, const InWorldDateTime &, double multiplier, const Persist &);
Result setClockRunning(Campaign &, bool running, const Persist &);
Result setClockMultiplier(Campaign &, double multiplier, const Persist &);
Result resetClockToSystemTime(Campaign &, const Persist &);

// Storage helpers are versioned and preserve ordering; absent data means legacy.
std::string serializeEntries(const std::vector<SessionPlanEntry> &);
std::vector<SessionPlanEntry> deserializeEntries(const std::string &);
std::string serializeLiveState(const SessionLiveState &);
SessionLiveState deserializeLiveState(const std::string &);
}
