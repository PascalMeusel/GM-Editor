#include "Models/Session/SessionPlanServiceInternal.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Session/SessionStartReview.hpp"
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
Result startInMemory(Campaign &value, const EntityId &id)
{
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        if (SessionControl::activeSession(value)) return error(Status::Conflict, "A session is already running. Resume or finish it first.");
        if (!session->startedAtUtc.empty()) return error(Status::Conflict, "This session has already started.");
        session->startedAtUtc = currentUtcTimestamp(); session->liveModeActive = true;
        session->liveState.initialized = true; session->liveState.phase = SessionPhase::Setup;
        if (session->participantCharacterIds.empty()) for (const auto &character : value.characters)
            session->participantCharacterIds.push_back(character.metadata.id);
        session->metadata.touch(); blackoutLocalPlayerDisplay(value.localPlayerDisplay);
        return success(id);
}
Result activateEntryInMemory(Campaign &value, const EntityId &id, const EntityId &entryId)
{
        auto *session = findSession(value, id); const auto check = active(session);
        if (!check.applied()) return check;
        auto *entry = findEntry(*session, entryId);
        if (!entry) return error(Status::NotFound, "Plan entry not found.");
        auto *scene = findScene(value, entry->storyBeatId); auto *thread = threadForScene(value, entry->storyBeatId);
        if (!scene || !thread) return error(Status::NotFound, "The linked scene is missing. Repair or remove this plan entry.");
        if (archived(scene->metadata) || archived(thread->metadata)) return error(Status::Conflict, "Restore this archived scene before running it.");
        if (session->liveState.activePlanEntryId == entryId) return error(Status::Conflict, "This scene is already active.");
        // Do not commit a broken live cursor or silently keep another scene's map.
        for (const auto &link : SceneLaunch::collectSceneLinks(*thread, *scene))
        {
            EntityId mapId;
            if (link.targetEntityType == "Map") mapId = link.targetEntityId;
            else if (link.targetEntityType == "Encounter")
            {
                const auto encounter = std::find_if(value.encounters.begin(), value.encounters.end(), [&](const auto &item) { return item.metadata.id == link.targetEntityId; });
                if (encounter == value.encounters.end()) return error(Status::NotFound, "A linked encounter is missing. Repair its scene link before running it.");
                if (archived(encounter->metadata)) return error(Status::Conflict, "Restore the linked encounter before running this scene.");
                mapId = encounter->mapId;
            }
            if (link.targetEntityType == "Map" && mapId.empty()) return error(Status::NotFound, "A linked map is missing. Repair its scene link before running it.");
            if (!mapId.empty())
            {
                const auto map = std::find_if(value.maps.begin(), value.maps.end(), [&](const auto &item) { return item.metadata.id == mapId; });
                if (map == value.maps.end()) return error(Status::NotFound, "A linked map is missing. Repair its scene or encounter link before running it.");
                if (archived(map->metadata)) return error(Status::Conflict, "Restore the linked map before running this scene.");
            }
        }
        const auto previousId = session->liveState.activePlanEntryId;
        if (const auto *previous = findEntry(*session, previousId))
            if (auto *beat = findScene(value, previous->storyBeatId))
                if (beat != scene && beat->status == StoryStatus::Active) beat->status = StoryStatus::Planned;
        const auto update = SceneLaunch::applyLiveStateForScene(value, *thread, *scene);
        session->liveState.initialized = true; session->liveState.phase = SessionPhase::Scene;
        session->liveState.activePlanEntryId = entryId; session->liveState.activeEncounterId = update.firstEncounterId;
        session->metadata.touch();
        // A new scene is staged; it never inherits a previously published payload.
        blackoutLocalPlayerDisplay(value.localPlayerDisplay);
        SceneLaunch::appendLaunchReceipt(value, *scene, scene->title);
        return success(id, entryId, scene->metadata.id);
}
}
Result start(Campaign &campaign, EntityId id, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) { return startInMemory(value, id); });
}
Result startReviewed(Campaign &campaign, const StartReview &captured, const Persist &persist)
{
    const StartReview review = captured; // The owner may be reset by a post-save subscriber.
    return commit(campaign, persist, [&](Campaign &value) {
        if (!currentStartReview(value, review))
            return error(Status::Conflict, "The reviewed session changed. Cancel and review the start again.");
        if (blank(review.title)) return error(Status::Invalid, "Enter a session title before starting.");
        if (review.create) {
            SessionModel session; session.metadata.id = review.sessionId; session.title = review.title;
            value.sessionIds.push_back(session.metadata.id); value.sessions.push_back(std::move(session));
        }
        return startInMemory(value, review.sessionId);
    });
}
Result startWithEntry(Campaign &campaign, EntityId id, EntityId entryId, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        const Result started = startInMemory(value, id);
        return !started.applied() || entryId.empty() ? started : activateEntryInMemory(value, id, entryId);
    });
}
Result activateEntry(Campaign &campaign, EntityId id, EntityId entryId, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) { return activateEntryInMemory(value, id, entryId); });
}
Result activateEncounter(Campaign &campaign, EntityId id, EntityId encounterId, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = active(session);
        if (!check.applied()) return check;
        const auto encounter = std::find_if(value.encounters.begin(), value.encounters.end(), [&](const auto &item) { return item.metadata.id == encounterId; });
        if (encounter == value.encounters.end()) return error(Status::NotFound, "Encounter not found.");
        if (archived(encounter->metadata)) return error(Status::Conflict, "Restore this encounter before running it.");
        session->liveState.initialized = true; session->liveState.phase = SessionPhase::Combat;
        session->liveState.activeEncounterId = encounterId;
        value.localPlayerDisplay.publicEncounterId = encounterId;
        value.localPlayerDisplay.showInitiative = true;
        if (!encounter->mapId.empty())
        {
            const auto map = std::find_if(value.maps.begin(), value.maps.end(), [&](const auto &item) { return item.metadata.id == encounter->mapId; });
            if (map == value.maps.end()) return error(Status::NotFound, "The encounter map is missing. Repair its map link before running it.");
            if (archived(map->metadata)) return error(Status::Conflict, "Restore the encounter map before running it.");
            value.partyLocation.mapId = map->metadata.id;
            value.partyTravel.currentMapId = map->metadata.id;
            value.localPlayerDisplay.publicMapId = map->metadata.id;
            value.localPlayerDisplay.showMap = true;
            // Keep the map's visibility unchanged. Player review remains explicit.
        }
        else
        {
            value.localPlayerDisplay.publicMapId.clear();
            value.localPlayerDisplay.showMap = false;
        }
        blackoutLocalPlayerDisplay(value.localPlayerDisplay);
        if (std::find(session->encounterIds.begin(), session->encounterIds.end(), encounterId) == session->encounterIds.end()) session->encounterIds.push_back(encounterId);
        session->metadata.touch(); return success(id, session->liveState.activePlanEntryId);
    });
}
Result setPhase(Campaign &campaign, EntityId id, SessionPhase phase, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = active(session);
        if (!check.applied()) return check;
        if (static_cast<int>(phase) < 0 || static_cast<int>(phase) > 5) return error(Status::Invalid, "Invalid session phase.");
        const bool wasBreak = session->liveState.initialized ? session->liveState.phase == SessionPhase::Break : SessionControl::isBreakActive(*session);
        if (phase == SessionPhase::Break && !wasBreak)
        {
            session->liveState.phaseBeforeBreak = session->liveState.phase;
            SessionControl::startBreak(value);
        }
        else if (phase != SessionPhase::Break && wasBreak) SessionControl::resumeFromBreak(value);
        session->liveState.initialized = true; session->liveState.phase = phase;
        session->metadata.touch(); return success(id);
    });
}
}
