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

SessionModel *findSession(Campaign &campaign, const EntityId &id)
{
    for (auto &session : campaign.sessions) if (session.metadata.id == id) return &session;
    return nullptr;
}
const SessionModel *findSession(const Campaign &campaign, const EntityId &id)
{
    for (const auto &session : campaign.sessions) if (session.metadata.id == id) return &session;
    return nullptr;
}
const SessionModel *nextSession(const Campaign &campaign)
{
    const SessionModel *scheduled = nullptr, *draft = nullptr;
    for (const auto &session : campaign.sessions)
    {
        if (session.liveModeActive) return &session;
        if (!session.startedAtUtc.empty() || !session.endedAtUtc.empty() || archived(session.metadata)) continue;
        if (!session.scheduledAtUtc.empty())
        {
            if (!scheduled || session.scheduledAtUtc < scheduled->scheduledAtUtc) scheduled = &session;
        }
        else if (!draft || session.metadata.updatedAtUtc > draft->metadata.updatedAtUtc) draft = &session;
    }
    return scheduled ? scheduled : draft;
}
SessionModel *nextSession(Campaign &campaign)
{
    const auto *session = nextSession(static_cast<const Campaign &>(campaign));
    return session ? findSession(campaign, session->metadata.id) : nullptr;
}
StoryBeatModel *findScene(Campaign &campaign, const EntityId &id)
{
    for (auto &thread : campaign.storyThreads) for (auto &scene : thread.beats)
        if (scene.metadata.id == id) return &scene;
    return nullptr;
}
const StoryBeatModel *findScene(const Campaign &campaign, const EntityId &id)
{
    for (const auto &thread : campaign.storyThreads) for (const auto &scene : thread.beats)
        if (scene.metadata.id == id) return &scene;
    return nullptr;
}
SessionPlanEntry *findEntry(SessionModel &session, const EntityId &id)
{
    for (auto &entry : session.planEntries) if (entry.metadata.id == id) return &entry;
    return nullptr;
}
const SessionPlanEntry *findEntry(const SessionModel &session, const EntityId &id)
{
    for (const auto &entry : session.planEntries) if (entry.metadata.id == id) return &entry;
    return nullptr;
}

Result createSession(Campaign &campaign, const std::string &title, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        if (blank(title)) return error(Status::Invalid, "Give this session a title.");
        SessionModel session; session.title = title;
        const auto id = session.metadata.id;
        value.sessionIds.push_back(id); value.sessions.push_back(std::move(session));
        return success(id);
    });
}
Result updateDraft(Campaign &campaign, EntityId id, const std::string &title,
                   const std::string &notes, const std::string &scheduled, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        if (blank(title)) return error(Status::Invalid, "Give this session a title.");
        session->title = title; session->roughNotes = notes; session->scheduledAtUtc = scheduled;
        session->metadata.touch(); return success(id);
    });
}
Result addScene(Campaign &campaign, EntityId id, EntityId sceneId, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        const auto *scene = findScene(value, sceneId);
        const auto *thread = threadForScene(value, sceneId);
        if (!scene || !thread) return error(Status::NotFound, "Scene not found.");
        if (archived(scene->metadata) || archived(thread->metadata)) return error(Status::Conflict, "Restore this archived scene before adding it.");
        SessionPlanEntry entry; entry.storyBeatId = sceneId;
        const auto entryId = entry.metadata.id;
        session->planEntries.push_back(std::move(entry)); session->metadata.touch();
        return success(id, entryId, sceneId);
    });
}
Result createScene(Campaign &campaign, EntityId id, const std::string &title,
                   const std::string &summary, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        if (blank(title)) return error(Status::Invalid, "Give this scene a title.");
        // The ownership link is an ordinary persisted story link, not a duplicated scene.
        StoryThreadModel *owner = nullptr;
        for (auto &thread : value.storyThreads)
            if (thread.type == StoryThreadType::SessionThread && !archived(thread.metadata))
                for (const auto &link : thread.links)
                    if (link.targetEntityType == "Session" && link.targetEntityId == id) { owner = &thread; break; }
        if (!owner)
        {
            StoryThreadModel thread; thread.type = StoryThreadType::SessionThread;
            thread.title = session->title; thread.status = StoryStatus::Planned;
            StoryLinkModel link; link.targetEntityType = "Session"; link.targetEntityId = id;
            thread.links.push_back(std::move(link));
            value.storyThreads.push_back(std::move(thread)); owner = &value.storyThreads.back();
        }
        StoryBeatModel scene; scene.title = title; scene.summary = summary; scene.status = StoryStatus::Planned;
        const auto sceneId = scene.metadata.id;
        owner->beats.push_back(std::move(scene)); owner->metadata.touch();
        SessionPlanEntry entry; entry.storyBeatId = sceneId;
        const auto entryId = entry.metadata.id;
        session->planEntries.push_back(std::move(entry)); session->metadata.touch();
        return success(id, entryId, sceneId);
    });
}
Result updateScene(Campaign &campaign, EntityId id, const StoryBeatModel &replacement, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *scene = findScene(value, id);
        if (!scene) return error(Status::NotFound, "Scene not found.");
        if (blank(replacement.title)) return error(Status::Invalid, "Give this scene a title.");
        const auto metadata = scene->metadata; *scene = replacement; scene->metadata = metadata;
        scene->metadata.touch(); blackoutLocalPlayerDisplay(value.localPlayerDisplay);
        return success({}, {}, id);
    });
}
Result updateSceneDraft(Campaign &campaign, EntityId id, const StoryBeatModel &base,
                        const StoryBeatModel &draft, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *scene = findScene(value, id);
        if (!scene) return error(Status::NotFound, "Scene not found.");
        bool conflict = false;
        const auto mergeText = [&](std::string &current, const std::string &previous, const std::string &edited) {
            if (edited == previous) return;
            if (current != previous && current != edited) conflict = true;
            else current = edited;
        };
        mergeText(scene->title, base.title, draft.title);
        mergeText(scene->summary, base.summary, draft.summary);
        mergeText(scene->clues, base.clues, draft.clues);
        mergeText(scene->consequences, base.consequences, draft.consequences);
        mergeText(scene->readAloudText, base.readAloudText, draft.readAloudText);
        mergeText(scene->playerSummary, base.playerSummary, draft.playerSummary);
        mergeText(scene->gmNotes, base.gmNotes, draft.gmNotes);
        const auto sameLinks = [](const auto &left, const auto &right) {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](const auto &a, const auto &b) {
                return a.metadata.id == b.metadata.id && a.metadata.tags == b.metadata.tags &&
                       a.metadata.customProperties == b.metadata.customProperties && a.label == b.label &&
                       a.type == b.type && a.targetEntityType == b.targetEntityType && a.targetEntityId == b.targetEntityId;
            });
        };
        if (!sameLinks(draft.links, base.links))
        {
            if (!sameLinks(scene->links, base.links) && !sameLinks(scene->links, draft.links)) conflict = true;
            else scene->links = draft.links;
        }
        if (conflict) return error(Status::DraftConflict, "This scene changed in another editor. Your draft is still here. Copy your draft before reloading the scene to resolve the conflicting fields.");
        if (blank(scene->title)) return error(Status::Invalid, "Give this scene a title.");
        // Status, audio cues, dates and metadata remain the current canonical values.
        scene->metadata.touch(); blackoutLocalPlayerDisplay(value.localPlayerDisplay);
        return success({}, {}, id);
    });
}
Result updateEntry(Campaign &campaign, EntityId id, EntityId entryId,
                   const std::string &note, SessionEntryState state, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        auto *entry = findEntry(*session, entryId);
        if (!entry) return error(Status::NotFound, "Plan entry not found.");
        if (static_cast<int>(state) < 0 || static_cast<int>(state) > 2) return error(Status::Invalid, "Invalid scene progress.");
        entry->prepNote = note; entry->state = state; entry->metadata.touch(); session->metadata.touch();
        return success(id, entryId, entry->storyBeatId);
    });
}
Result moveEntry(Campaign &campaign, EntityId id, EntityId entryId, int offset, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        auto &entries = session->planEntries;
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return entry.metadata.id == entryId; });
        if (found == entries.end()) return error(Status::NotFound, "Plan entry not found.");
        const auto from = static_cast<long long>(found - entries.begin());
        const auto to = from + offset;
        if (to < 0 || to >= static_cast<long long>(entries.size())) return error(Status::Invalid, "The scene is already at the edge of the plan.");
        auto entry = std::move(*found); entries.erase(found); entries.insert(entries.begin() + to, std::move(entry));
        session->metadata.touch(); return success(id, entryId);
    });
}
Result removeEntry(Campaign &campaign, EntityId id, EntityId entryId, const Persist &persist)
{
    return commit(campaign, persist, [&](Campaign &value) {
        auto *session = findSession(value, id); const auto check = editable(session);
        if (!check.applied()) return check;
        auto &entries = session->planEntries;
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return entry.metadata.id == entryId; });
        if (found == entries.end()) return error(Status::NotFound, "Plan entry not found.");
        if (session->liveModeActive && session->liveState.activePlanEntryId == entryId)
            return error(Status::Conflict, "Activate another scene before removing the current scene from the plan.");
        entries.erase(found); session->metadata.touch(); return success(id, entryId);
    });
}
}
