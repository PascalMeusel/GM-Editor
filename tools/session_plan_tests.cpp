#include "Models/UX/PreparationText.hpp"
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionReportTarget.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/InWorldTimeService.hpp"
#include <iostream>
#include <stdexcept>

namespace
{
int failures = 0;
void check(bool condition, const char *message)
{ if (!condition) { ++failures; std::cerr << message << '\n'; } }
bool sameClock(const CampaignClockModel &a, const CampaignClockModel &b)
{
    if (a.useSystemTime != b.useSystemTime || a.running != b.running || a.multiplier != b.multiplier ||
        a.anchorAbsoluteMinutes != b.anchorAbsoluteMinutes || a.anchorUtc != b.anchorUtc ||
        a.calendar.name != b.calendar.name || a.calendar.hoursPerDay != b.calendar.hoursPerDay ||
        a.calendar.daylightHours != b.calendar.daylightHours || a.calendar.nightHours != b.calendar.nightHours ||
        a.calendar.dayStartsAtHour != b.calendar.dayStartsAtHour || a.calendar.weekdays != b.calendar.weekdays ||
        a.calendar.months.size() != b.calendar.months.size()) return false;
    for (std::size_t i = 0; i < a.calendar.months.size(); ++i)
        if (a.calendar.months[i].name != b.calendar.months[i].name || a.calendar.months[i].days != b.calendar.months[i].days) return false;
    return true;
}
}
#include "session_review_service_cases.hpp"
int main()
{
    reviewedSessionServiceCases();
    check(PreparationText::sceneTitle("  Arrival at Havenfall. Meet the ferryman and inspect the ruined lantern tower.") == "Arrival at Havenfall", "Scene title must stop after the first sentence");
    check(PreparationText::sceneTitle("Eine ungewöhnlich lange Beschreibung mit vielen weiteren Wörtern ohne Satzende", 36) == "Eine ungewöhnlich lange Beschreibung", "Scene title must use UTF-8 character and word boundaries");
    check(PreparationText::sceneTitle("\n   ").empty(), "Whitespace is not a scene title");
    Campaign campaign; campaign.name = "Session plan test";
    int saves = 0;
    const SessionPlan::Persist save = [&](Campaign &) { ++saves; return true; };
    const SessionPlan::Persist fail = [](Campaign &) { return false; };
    check(!SessionPlan::createSession(campaign, "Not saved", fail).applied() && campaign.sessions.empty(), "Failed creation leaked a session");
    const auto created = SessionPlan::createSession(campaign, "The lanterns", save);
    check(created.applied(), "Session creation failed");
    const EntityId id = created.sessionId;
    check(SessionPlan::nextSession(campaign)->metadata.id == id, "Next session did not select the prepared draft");
    const std::string prose = "Ruß; \"Geheimnis\" \\ Weg\n第二場 🕯";
    check(SessionPlan::updateDraft(campaign, id, "The lanterns", prose, "2026-10-01T18:00:00Z", save).applied(), "Draft update failed");
    const auto first = SessionPlan::createScene(campaign, id, "At the gate", "Find Mara", save);
    const auto second = SessionPlan::createScene(campaign, id, "Below", "Follow the light", save);
    check(first.applied() && second.applied() && campaign.storyThreads.size() == 1 && campaign.storyThreads[0].beats.size() == 2, "Scene creation must create canonical linked beats");
    const auto again = SessionPlan::addScene(campaign, id, first.sceneId, save);
    check(again.applied() && again.entryId != first.entryId && campaign.storyThreads[0].beats.size() == 2, "Reusing a scene must add an entry without copying its scene");
    check(SessionPlan::moveEntry(campaign, id, second.entryId, -1, save).applied(), "Reorder failed");
    check(SessionPlan::findSession(campaign, id)->planEntries[0].metadata.id == second.entryId, "Order was not applied");
    check(SessionPlan::updateEntry(campaign, id, first.entryId, prose, SessionEntryState::Planned, save).applied(), "Entry note update failed");
    auto *session = SessionPlan::findSession(campaign, id);
    session->planEntries[1].metadata.tags = {"tag; punctuation"};
    session->planEntries[1].metadata.customProperties["key\n"] = prose;
    const auto serialized = SessionPlan::serializeEntries(session->planEntries);
    const auto restored = SessionPlan::deserializeEntries(serialized);
    check(restored.size() == 3 && SessionPlan::serializeEntries(restored) == serialized, "Plan serialization lost order, IDs, prose or metadata");
    check(SessionPlan::deserializeEntries("").empty() && !SessionPlan::deserializeLiveState("").initialized, "Legacy data must not invent a plan/live history");
    bool corruptRejected = false;
    try { SessionPlan::deserializeEntries("[\"session-plan-v1\",[\"bad\"]]"); } catch (const std::exception &) { corruptRejected = true; }
    check(corruptRejected, "Malformed saved plans must not silently erase user data");
    auto duplicate = restored; duplicate.push_back(duplicate.front());
    corruptRejected = false;
    try { SessionPlan::deserializeEntries(SessionPlan::serializeEntries(duplicate)); } catch (const std::exception &) { corruptRejected = true; }
    check(corruptRejected, "Duplicate persisted entry IDs were accepted");
    check(SessionPlan::removeEntry(campaign, id, again.entryId, save).applied() && SessionPlan::findScene(campaign, first.sceneId), "Removing an entry deleted canonical content");
    check(SessionPlan::start(campaign, id, save).applied() && campaign.sessions.size() == 1 && SessionPlan::findSession(campaign, id)->liveModeActive, "Starting preparation should reuse the session ID");
    const int beforeRepeat = saves;
    check(!SessionPlan::start(campaign, id, save).applied() && saves == beforeRepeat, "Starting an active session should not save or duplicate it");
    const auto other = SessionPlan::createSession(campaign, "Tomorrow", save);
    check(!SessionPlan::start(campaign, other.sessionId, save).applied(), "Two sessions became active");
    const auto notesBefore = SessionPlan::findSession(campaign, id)->notes.size();
    check(!SessionPlan::activateEntry(campaign, id, first.entryId, fail).applied(), "Injected activation failure was ignored");
    check(SessionPlan::findSession(campaign, id)->liveState.activePlanEntryId.empty() && SessionPlan::findSession(campaign, id)->notes.size() == notesBefore && SessionPlan::findScene(campaign, first.sceneId)->status == StoryStatus::Planned, "Failed activation leaked state or a launch receipt");
    const SessionPlan::Persist throwing = [](Campaign &) -> bool { throw std::runtime_error("locked"); };
    check(!SessionPlan::activateEntry(campaign, id, first.entryId, throwing).applied() && SessionPlan::findSession(campaign, id)->liveState.activePlanEntryId.empty(), "Thrown save failure leaked activation");
    check(SessionPlan::activateEntry(campaign, id, first.entryId, save).applied(), "Scene activation failed");
    check(SessionPlan::findSession(campaign, id)->liveState.activePlanEntryId == first.entryId && campaign.localPlayerDisplay.blackout && !campaign.localPlayerDisplay.published, "Scene activation did not preserve review safety");
    check(!SessionPlan::activateEntry(campaign, id, first.entryId, save).applied(), "Repeated activation would replay a cue");
    check(!SessionPlan::removeEntry(campaign, id, first.entryId, save).applied(), "Active cursor entry was removed");
    check(SessionPlan::setPhase(campaign, id, SessionPhase::Exploration, save).applied(), "Phase change failed");
    check(SessionPlan::setPhase(campaign, id, SessionPhase::Break, save).applied() && SessionControl::isLiveSessionBreakActive(campaign), "Break phase was not persisted");
    const auto live = SessionPlan::deserializeLiveState(SessionPlan::serializeLiveState(SessionPlan::findSession(campaign, id)->liveState));
    check(live.phase == SessionPhase::Break && live.phaseBeforeBreak == SessionPhase::Exploration && live.activePlanEntryId == first.entryId, "Restart state lost break return phase/cursor");
    check(SessionPlan::setPhase(campaign, id, live.phaseBeforeBreak, save).applied() && !SessionControl::isLiveSessionBreakActive(campaign), "Break resume did not restore phase");
    EncounterModel encounter; encounter.name = "A missing map"; encounter.mapId = "missing";
    const EntityId encounterId = encounter.metadata.id; campaign.encounters.push_back(encounter);
    check(!SessionPlan::activateEncounter(campaign, id, encounterId, save).applied() && SessionPlan::findSession(campaign, id)->liveState.activeEncounterId.empty(), "Missing encounter map mutated live state");
    campaign.encounters.back().mapId.clear();
    check(SessionPlan::activateEncounter(campaign, id, encounterId, save).applied() && campaign.localPlayerDisplay.publicEncounterId == encounterId, "Encounter activation did not stage coherent live context");
    SessionPlan::findScene(campaign, second.sceneId)->metadata.tags.push_back("archived");
    check(!SessionPlan::activateEntry(campaign, id, second.entryId, save).applied(), "Archived scene launched");
    check(SessionPlan::updateEntry(campaign, id, first.entryId, prose, SessionEntryState::Completed, save).applied(), "Completed state did not persist");
    check(SessionPlan::findEntry(*SessionPlan::findSession(campaign, id), first.entryId)->state == SessionEntryState::Completed &&
          SessionPlan::findSession(campaign, id)->liveState.activePlanEntryId == first.entryId,
          "Completing the current scene lost its active context");
    check(SessionPlan::updateEntry(campaign, id, first.entryId, prose, SessionEntryState::Planned, save).applied() &&
          SessionPlan::findEntry(*SessionPlan::findSession(campaign, id), first.entryId)->state == SessionEntryState::Planned &&
          SessionPlan::findSession(campaign, id)->liveState.activePlanEntryId == first.entryId,
          "A completed current scene could not be resumed as planned");
    check(SessionPlan::updateEntry(campaign, id, first.entryId, prose, SessionEntryState::Skipped, save).applied() &&
          SessionPlan::findSession(campaign, id)->liveState.activePlanEntryId == first.entryId,
          "Skipping the current scene lost its active context");
    check(SessionPlan::updateEntry(campaign, id, first.entryId, prose, SessionEntryState::Planned, save).applied(),
          "A skipped current scene could not be resumed as planned");
    campaign.localPlayerDisplay.published = true; campaign.localPlayerDisplay.blackout = false;
    check(!SessionPlan::finish(campaign, id, fail).applied() && SessionPlan::findSession(campaign, id)->liveModeActive && campaign.localPlayerDisplay.blackout, "Failed finish must preserve live state but immediately black out output");
    check(SessionPlan::finish(campaign, id, save).applied() && !SessionPlan::findSession(campaign, id)->endedAtUtc.empty(), "Session completion did not persist");
    check(!SessionPlan::start(campaign, id, save).applied(), "A finished session restarted");
    check(SessionPlan::nextSession(campaign)->metadata.id == other.sessionId, "Next draft was lost after wrap-up");

    Campaign launch; launch.name = "Atomic run";
    const auto runSession = SessionPlan::createSession(launch, "Selected session", save);
    const auto runScene = SessionPlan::createScene(launch, runSession.sessionId, "Opening", "Opening notes", save);
    const auto differentSession = SessionPlan::createSession(launch, "Other next session", save);
    MapModel privateMap; privateMap.name = "Private map"; privateMap.visibility = MapVisibility::GameMasterOnly;
    const EntityId privateMapId = privateMap.metadata.id;
    launch.maps.push_back(privateMap);
    StoryLinkModel mapLink; mapLink.targetEntityType = "Map"; mapLink.targetEntityId = privateMapId;
    // A thread-level map must stage the same context as the scene's live map.
    launch.storyThreads.front().links.push_back(mapLink);
    launch.localPlayerDisplay.publicEncounterId = "previous encounter";
    launch.localPlayerDisplay.showInitiative = true;
    check(!SessionPlan::startWithEntry(launch, runSession.sessionId, "missing entry", save).applied() &&
          SessionPlan::findSession(launch, runSession.sessionId)->startedAtUtc.empty(), "Invalid opening scene partially started its session");
    check(!SessionPlan::startWithEntry(launch, runSession.sessionId, runScene.entryId, fail).applied() &&
          SessionPlan::findSession(launch, runSession.sessionId)->startedAtUtc.empty() &&
          SessionPlan::findSession(launch, runSession.sessionId)->notes.empty() &&
          SessionPlan::findScene(launch, runScene.sceneId)->status == StoryStatus::Planned,
          "Failed atomic run leaked the session, scene or launch receipt");
    launch.maps.clear();
    const int beforeBrokenRun = saves;
    check(!SessionPlan::startWithEntry(launch, runSession.sessionId, runScene.entryId, save).applied() && saves == beforeBrokenRun &&
          SessionPlan::findSession(launch, runSession.sessionId)->startedAtUtc.empty(), "Missing linked map partially started its session");
    launch.maps.push_back(privateMap);
    const int beforeAtomicRun = saves;
    const auto started = SessionPlan::startWithEntry(launch, runSession.sessionId, runScene.entryId, save);
    check(started.applied() && saves == beforeAtomicRun + 1 && started.sceneId == runScene.sceneId &&
          SessionControl::activeSession(launch)->metadata.id == runSession.sessionId &&
          SessionPlan::findSession(launch, differentSession.sessionId)->startedAtUtc.empty(),
          "Run must save exactly once and start the explicitly selected session");
    check(launch.maps.front().visibility == MapVisibility::GameMasterOnly && launch.localPlayerDisplay.publicMapId == privateMapId &&
          launch.partyLocation.mapId == privateMapId && launch.localPlayerDisplay.publicEncounterId.empty() &&
          !launch.localPlayerDisplay.showInitiative && launch.localPlayerDisplay.blackout,
          "Scene staging leaked map privacy, stale initiative or inconsistent thread-level context");
    launch.storyThreads.front().links.pop_back();
    const auto nextScene = SessionPlan::createScene(launch, runSession.sessionId, "Quiet scene", "No map", save);
    check(SessionPlan::activateEntry(launch, runSession.sessionId, nextScene.entryId, save).applied() &&
          launch.localPlayerDisplay.publicMapId.empty() && !launch.localPlayerDisplay.showMap,
          "A scene without a map inherited the previous scene's player selection");
    const StoryBeatModel base = *SessionPlan::findScene(launch, nextScene.sceneId);
    StoryBeatModel draft = base; draft.summary = "Draft summary";
    auto *canonical = SessionPlan::findScene(launch, nextScene.sceneId);
    canonical->status = StoryStatus::Resolved; canonical->cueAudioAssetId = "new cue";
    canonical->gmNotes = "Concurrent private note"; canonical->links.push_back(mapLink);
    check(SessionPlan::updateSceneDraft(launch, nextScene.sceneId, base, draft, save).applied(), "Disjoint draft changes should merge");
    canonical = SessionPlan::findScene(launch, nextScene.sceneId);
    check(canonical->summary == "Draft summary" && canonical->gmNotes == "Concurrent private note" &&
          canonical->status == StoryStatus::Resolved && canonical->cueAudioAssetId == "new cue" && canonical->links.size() == 1,
          "Draft save overwrote unrelated canonical scene state");
    const StoryBeatModel conflictBase = *canonical;
    StoryBeatModel conflictDraft = conflictBase; conflictDraft.summary = "Unsaved local text";
    canonical->summary = "Other editor text";
    const int beforeConflict = saves;
    check(SessionPlan::updateSceneDraft(launch, nextScene.sceneId, conflictBase, conflictDraft, save).status == SessionPlan::Status::DraftConflict &&
          saves == beforeConflict && SessionPlan::findScene(launch, nextScene.sceneId)->summary == "Other editor text" &&
          conflictDraft.summary == "Unsaved local text", "Conflicting draft lost local or canonical text");
    StoryBeatModel linksDraft = conflictBase; linksDraft.links.clear();
    SessionPlan::findScene(launch, nextScene.sceneId)->links.front().label = "Other editor label";
    check(SessionPlan::updateSceneDraft(launch, nextScene.sceneId, conflictBase, linksDraft, save).status == SessionPlan::Status::DraftConflict,
          "Concurrent link change was silently overwritten");
    SessionPlan::WrapUpDraft wrap;
    wrap.summary = "Final summary"; wrap.recap = prose; wrap.unresolvedThreads = "A door remains locked";
    wrap.lootAwarded = "A lantern"; wrap.xpMilestones = "Met the spirit"; wrap.npcsMet = "Mara";
    wrap.locationsVisited = "Village square"; wrap.openHooks = "Return below";
    wrap.pendingNote.body = "Last unfinished note"; wrap.pendingNote.linkedEntityId = nextScene.sceneId;
    wrap.pendingNote.linkedEntityType = "story_beat"; wrap.pendingNote.linkedEntityLabel = "Quiet scene";
    const auto wrapNotesBefore = SessionPlan::findSession(launch, runSession.sessionId)->notes.size();
    Campaign savedDraftCampaign = launch;
    check(!SessionPlan::saveWrapUpDraft(savedDraftCampaign, runSession.sessionId, wrap, fail).applied() &&
          SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->summary.empty() &&
          SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.size() == wrapNotesBefore,
          "Failed Live draft save partially changed notes or recap");
    Campaign reopenedDraft;
    int draftSaves = 0;
    check(SessionPlan::saveWrapUpDraft(savedDraftCampaign, runSession.sessionId, wrap, [&](Campaign &saved) {
        ++draftSaves; reopenedDraft = saved; return true;
    }).applied() && draftSaves == 1, "Live draft flush must save all fields in one write");
    const auto *savedLive = SessionPlan::findSession(reopenedDraft, runSession.sessionId);
    check(savedLive && savedLive->liveModeActive && savedLive->endedAtUtc.empty() &&
          savedLive->liveState.activePlanEntryId == nextScene.entryId && savedLive->summary == wrap.summary && savedLive->recap == prose &&
          savedLive->notes.size() == wrapNotesBefore + 1 && savedLive->notes.back().body == wrap.pendingNote.body &&
          savedLive->notes.back().linkedEntityId == nextScene.sceneId,
          "Explicit Live draft save ended the session, moved its cursor or omitted a pending note");
    auto withoutPendingNote = wrap; withoutPendingNote.pendingNote.body.clear();
    check(SessionPlan::saveWrapUpDraft(savedDraftCampaign, runSession.sessionId, withoutPendingNote, save).applied() &&
          SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.size() == wrapNotesBefore + 1,
          "A later draft flush duplicated its previously saved note");
    SessionNoteModel insertedNote; insertedNote.body = "Inserted before the removal selection";
    SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.insert(
        SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.begin(), insertedNote);
    check(!SessionPlan::removeNote(savedDraftCampaign, runSession.sessionId, wrap.pendingNote.metadata.id, fail).applied() &&
          SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.size() == wrapNotesBefore + 2,
          "Failed note removal changed the saved note list");
    check(SessionPlan::removeNote(savedDraftCampaign, runSession.sessionId, wrap.pendingNote.metadata.id, save).applied() &&
          SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.size() == wrapNotesBefore + 1 &&
          SessionPlan::findSession(savedDraftCampaign, runSession.sessionId)->notes.front().metadata.id == insertedNote.metadata.id,
          "Removing a note after its row moved targeted another note");
    check(!SessionPlan::finishWithDraft(launch, runSession.sessionId, wrap, fail).applied() &&
          SessionPlan::findSession(launch, runSession.sessionId)->liveModeActive &&
          SessionPlan::findSession(launch, runSession.sessionId)->recap.empty() &&
          SessionPlan::findSession(launch, runSession.sessionId)->notes.size() == wrapNotesBefore && wrap.pendingNote.body == "Last unfinished note",
          "Failed wrap-up lost drafts or partially finished the session");
    Campaign persistedWrap;
    int wrapSaves = 0;
    check(SessionPlan::finishWithDraft(launch, runSession.sessionId, wrap, [&](Campaign &saved) {
        ++wrapSaves; persistedWrap = saved; return true;
    }).applied() && wrapSaves == 1, "Wrap-up and session completion must use one write");
    const auto *reopened = SessionPlan::findSession(persistedWrap, runSession.sessionId);
    check(reopened && !reopened->liveModeActive && !reopened->endedAtUtc.empty() &&
          reopened->summary == wrap.summary && reopened->recap == prose && reopened->openHooks == wrap.openHooks &&
          reopened->unresolvedThreads == wrap.unresolvedThreads && reopened->lootAwarded == wrap.lootAwarded &&
          reopened->xpMilestones == wrap.xpMilestones && reopened->npcsMet == wrap.npcsMet && reopened->locationsVisited == wrap.locationsVisited &&
          reopened->notes.size() == wrapNotesBefore + 1 && reopened->notes.back().body == wrap.pendingNote.body &&
          !reopened->notes.back().timestampUtc.empty() && reopened->notes.back().linkedEntityId == nextScene.sceneId,
          "The durable finished session omitted wrap-up text or its pending linked note");
    Campaign scopedLinks;
    const auto scopedSession = SessionPlan::createSession(scopedLinks, "Per-scene context", save);
    const auto scopedScene = SessionPlan::createScene(scopedLinks, scopedSession.sessionId, "Specific location", "", save);
    MapModel arcMap; arcMap.name = "Arc map";
    MapModel sceneMap; sceneMap.name = "Scene map";
    scopedLinks.maps = {arcMap, sceneMap};
    EncounterModel arcEncounter; arcEncounter.name = "Arc encounter"; arcEncounter.mapId = arcMap.metadata.id;
    EncounterModel sceneEncounter; sceneEncounter.name = "Scene encounter"; sceneEncounter.mapId = sceneMap.metadata.id;
    scopedLinks.encounters = {arcEncounter, sceneEncounter};
    StoryLinkModel arcMapLink; arcMapLink.targetEntityType = "Map"; arcMapLink.targetEntityId = arcMap.metadata.id;
    StoryLinkModel arcEncounterLink; arcEncounterLink.targetEntityType = "Encounter"; arcEncounterLink.targetEntityId = arcEncounter.metadata.id;
    scopedLinks.storyThreads.front().links.push_back(arcMapLink); scopedLinks.storyThreads.front().links.push_back(arcEncounterLink);
    StoryLinkModel sceneMapLink; sceneMapLink.targetEntityType = "Map"; sceneMapLink.targetEntityId = sceneMap.metadata.id;
    StoryLinkModel sceneEncounterLink; sceneEncounterLink.targetEntityType = "Encounter"; sceneEncounterLink.targetEntityId = sceneEncounter.metadata.id;
    SessionPlan::findScene(scopedLinks, scopedScene.sceneId)->links = {sceneMapLink, sceneEncounterLink};
    check(SessionPlan::startWithEntry(scopedLinks, scopedSession.sessionId, scopedScene.entryId, save).applied() &&
          scopedLinks.partyLocation.mapId == sceneMap.metadata.id && scopedLinks.localPlayerDisplay.publicMapId == sceneMap.metadata.id &&
          scopedLinks.localPlayerDisplay.publicEncounterId == sceneEncounter.metadata.id &&
          SessionPlan::findSession(scopedLinks, scopedSession.sessionId)->liveState.activeEncounterId == sceneEncounter.metadata.id,
          "Arc defaults overrode the scene's explicit map or encounter");
    scopedLinks.maps.front().metadata.tags.push_back("archived");
    const int beforeArchivedEncounter = saves;
    check(!SessionPlan::activateEncounter(scopedLinks, scopedSession.sessionId, arcEncounter.metadata.id, save).applied() &&
          saves == beforeArchivedEncounter && scopedLinks.localPlayerDisplay.publicMapId == sceneMap.metadata.id &&
          SessionPlan::findSession(scopedLinks, scopedSession.sessionId)->liveState.activeEncounterId == sceneEncounter.metadata.id,
          "An archived encounter map partially replaced active session context");
    Campaign aliased;
    const auto aliasedSession = SessionPlan::createSession(aliased, "Aliased IDs", save);
    const auto aliasedScene = SessionPlan::createScene(aliased, aliasedSession.sessionId, "First", "", save);
    const auto aliasedStarted = SessionPlan::startWithEntry(aliased, aliased.sessions.front().metadata.id,
        aliased.sessions.front().planEntries.front().metadata.id, save);
    check(aliasedStarted.applied() && aliasedStarted.sessionId == aliasedSession.sessionId &&
          aliasedStarted.entryId == aliasedScene.entryId && aliasedStarted.sceneId == aliasedScene.sceneId,
          "Campaign-owned ID arguments did not survive transaction replacement");
    const auto aliasedNext = SessionPlan::createScene(aliased, aliased.sessions.front().metadata.id, "Second", "", save);
    const auto aliasedActivated = SessionPlan::activateEntry(aliased, aliased.sessions.front().metadata.id,
        aliased.sessions.front().planEntries.back().metadata.id, save);
    check(aliasedActivated.applied() && aliasedActivated.sessionId == aliasedSession.sessionId &&
          aliasedActivated.entryId == aliasedNext.entryId && aliasedActivated.sceneId == aliasedNext.sceneId,
          "Returned activation IDs referenced invalidated campaign storage");

    Campaign timed;
    const auto timedSession = SessionPlan::createSession(timed, "Clock retry", save);
    check(SessionPlan::start(timed, timedSession.sessionId, save).applied() &&
          SessionPlan::setPhase(timed, timedSession.sessionId, SessionPhase::Combat, save).applied() &&
          SessionPlan::setPhase(timed, timedSession.sessionId, SessionPhase::Break, save).applied(),
          "Clock retry fixture could not enter a combat break");
    InWorldTime::setDateTime(timed.clock, {1432, 5, 6, 9, 30}, false);
    const auto clockBefore = timed.clock;
    const auto clockNotesBefore = timed.sessions.front().notes.size();
    const auto clockTimelineBefore = timed.timelineEvents.size();
    const auto clockLiveBefore = SessionPlan::serializeLiveState(timed.sessions.front().liveState);
    const std::string clockReason = "The party searches the harbour";
    const auto clockUnchanged = [&] {
        return timed.clock.anchorAbsoluteMinutes == clockBefore.anchorAbsoluteMinutes &&
            timed.clock.anchorUtc == clockBefore.anchorUtc && timed.clock.running == clockBefore.running &&
            timed.clock.useSystemTime == clockBefore.useSystemTime &&
            timed.sessions.front().notes.size() == clockNotesBefore && timed.timelineEvents.size() == clockTimelineBefore &&
            SessionPlan::serializeLiveState(timed.sessions.front().liveState) == clockLiveBefore;
    };
    check(SessionPlan::advanceClock(timed, "10 minutes", clockReason, fail).status == SessionPlan::Status::PersistenceFailed && clockUnchanged(),
          "Failed clock save advanced canonical time or appended an unsaved session/timeline note");
    check(SessionPlan::advanceClock(timed, "10 minutes", clockReason, throwing).status == SessionPlan::Status::PersistenceFailed && clockUnchanged(),
          "Throwing clock save changed the prior clock, notes or Break/Combat cursor");
    const int beforeInvalidClock = saves;
    check(SessionPlan::advanceClock(timed, "not a duration", clockReason, save).status == SessionPlan::Status::Invalid &&
          saves == beforeInvalidClock && clockUnchanged(), "Invalid clock amount reached persistence or changed session state");
    Campaign reopenedClock;
    int clockWrites = 0;
    const auto advanced = SessionPlan::advanceClock(timed, "10 minutes", clockReason, [&](Campaign &value) {
        ++clockWrites; reopenedClock = value; return true;
    });
    check(advanced.applied() && advanced.sessionId == timedSession.sessionId && clockWrites == 1 &&
          timed.clock.anchorAbsoluteMinutes == clockBefore.anchorAbsoluteMinutes + 10 &&
          reopenedClock.clock.anchorAbsoluteMinutes == timed.clock.anchorAbsoluteMinutes &&
          reopenedClock.sessions.front().notes.size() == clockNotesBefore + 1 && reopenedClock.timelineEvents.size() == clockTimelineBefore + 1 &&
          reopenedClock.sessions.front().notes.back().body.find(clockReason) != std::string::npos &&
          SessionPlan::serializeLiveState(reopenedClock.sessions.front().liveState) == clockLiveBefore,
          "A successful clock retry must persist exactly one advance and one linked session/timeline note without leaving Break");
    Campaign unstartedClock;
    InWorldTime::setDateTime(unstartedClock.clock, {1432, 5, 6, 9, 30}, false);
    const auto unstartedMinutes = unstartedClock.clock.anchorAbsoluteMinutes;
    check(SessionPlan::advanceClock(unstartedClock, "10 minutes", "", save).applied() &&
          unstartedClock.clock.anchorAbsoluteMinutes == unstartedMinutes + 10 &&
          unstartedClock.sessions.empty() && unstartedClock.timelineEvents.empty(),
          "Advancing a clock before a session must remain available without inventing session notes");

    // Every clock control shares the save-before-replace contract. Exercise
    // each command against false and throwing storage before a single retry.
    const auto verifyClockControl = [&](const auto &operation, const auto &expected, bool addsNote = false) {
        Campaign value = timed;
        const auto before = value.clock;
        const auto notes = value.sessions.front().notes.size();
        const auto events = value.timelineEvents.size();
        const auto live = SessionPlan::serializeLiveState(value.sessions.front().liveState);
        const auto priorState = [&] {
            return sameClock(value.clock, before) && value.sessions.front().notes.size() == notes &&
                value.timelineEvents.size() == events && SessionPlan::serializeLiveState(value.sessions.front().liveState) == live;
        };
        check(operation(value, fail).status == SessionPlan::Status::PersistenceFailed && priorState(),
              "A failed clock control changed the committed clock, calendar, notes or live cursor");
        check(operation(value, throwing).status == SessionPlan::Status::PersistenceFailed && priorState(),
              "A throwing clock control changed the committed clock, calendar, notes or live cursor");
        Campaign durable;
        int writes = 0;
        const SessionPlan::Persist write = [&](Campaign &staged) { ++writes; durable = staged; return true; };
        check(operation(value, write).applied() && writes == 1 && sameClock(value.clock, durable.clock) &&
              expected(durable.clock) && durable.sessions.front().notes.size() == notes + (addsNote ? 1 : 0) &&
              durable.timelineEvents.size() == events + (addsNote ? 1 : 0) &&
              SessionPlan::serializeLiveState(durable.sessions.front().liveState) == live,
              "Clock retry omitted its requested state, duplicated a note, or changed the saved live cursor");
    };
    const auto dateMatches = [](const CampaignClockModel &clock, const InWorldDateTime &expected) {
        const auto actual = InWorldTime::currentDateTime(clock);
        return actual.year == expected.year && actual.month == expected.month && actual.day == expected.day &&
               actual.hour == expected.hour && actual.minute == expected.minute;
    };
    const InWorldDateTime selectedDate{1433, 6, 7, 14, 25};
    verifyClockControl([&](Campaign &value, const SessionPlan::Persist &write) {
        return SessionPlan::setClockDateTime(value, selectedDate, clockReason, write);
    }, [&](const CampaignClockModel &clock) { return !clock.useSystemTime && !clock.running && dateMatches(clock, selectedDate); }, true);
    CampaignCalendarModel customCalendar;
    customCalendar.name = "Four seasons";
    customCalendar.months = {{"Thaw",21},{"Bloom",22},{"Harvest",23},{"Frost",24}};
    customCalendar.weekdays = {"First","Second","Third"};
    customCalendar.hoursPerDay = 9; customCalendar.daylightHours = 5; customCalendar.nightHours = 4;
    customCalendar.dayStartsAtHour = 2;
    const InWorldDateTime customDate{120, 3, 7, 2, 42};
    verifyClockControl([&](Campaign &value, const SessionPlan::Persist &write) {
        return SessionPlan::setClockCalendar(value, customCalendar, customDate, 2.5, write);
    }, [&](const CampaignClockModel &clock) {
        return !clock.useSystemTime && !clock.running && clock.multiplier == 2.5 &&
            clock.calendar.name == customCalendar.name && clock.calendar.months.size() == 4 &&
            clock.calendar.months[2].days == 23 && clock.calendar.weekdays == customCalendar.weekdays &&
            clock.calendar.hoursPerDay == 9 && dateMatches(clock, customDate);
    });
    verifyClockControl([](Campaign &value, const SessionPlan::Persist &write) {
        return SessionPlan::setClockRunning(value, true, write);
    }, [](const CampaignClockModel &clock) { return clock.running && !clock.useSystemTime; });
    verifyClockControl([](Campaign &value, const SessionPlan::Persist &write) {
        return SessionPlan::setClockMultiplier(value, 4.0, write);
    }, [](const CampaignClockModel &clock) { return clock.multiplier == 4.0 && !clock.running && !clock.useSystemTime; });
    timed.clock.running = true;
    verifyClockControl([](Campaign &value, const SessionPlan::Persist &write) {
        return SessionPlan::setClockRunning(value, false, write);
    }, [](const CampaignClockModel &clock) { return !clock.running && !clock.useSystemTime; });
    verifyClockControl([](Campaign &value, const SessionPlan::Persist &write) {
        return SessionPlan::resetClockToSystemTime(value, write);
    }, [](const CampaignClockModel &clock) { return clock.useSystemTime && clock.running && clock.multiplier == 1.0; });
    if (failures) return 1;
    std::cout << "Session plan transaction, ordering, restart and serialization tests passed.\n";
    return 0;
}
