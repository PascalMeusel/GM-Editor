#include "CampaignEditHistory.hpp"
#include "Models/Project/ProjectCommand.hpp"
#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace
{
bool same(const EntityMetadata &a, const EntityMetadata &b)
{
    return std::tie(a.id, a.tags, a.customProperties) == std::tie(b.id, b.tags, b.customProperties);
}
bool same(const SessionPlanEntry &a, const SessionPlanEntry &b)
{
    return same(a.metadata, b.metadata) && std::tie(a.storyBeatId, a.prepNote, a.state) ==
        std::tie(b.storyBeatId, b.prepNote, b.state);
}
bool same(const StoryLinkModel &a, const StoryLinkModel &b)
{
    return same(a.metadata, b.metadata) && std::tie(a.label, a.type, a.targetEntityId, a.targetEntityType) ==
        std::tie(b.label, b.type, b.targetEntityId, b.targetEntityType);
}
bool same(const StoryObjectiveModel &a, const StoryObjectiveModel &b)
{
    return same(a.metadata, b.metadata) &&
        std::tie(a.title, a.playerSummary, a.gmNotes, a.status, a.playerVisible, a.linkedStoryThreadId,
                 a.linkedStoryBeatId, a.assigneeCharacterIds, a.prerequisiteObjectiveIds) ==
        std::tie(b.title, b.playerSummary, b.gmNotes, b.status, b.playerVisible, b.linkedStoryThreadId,
                 b.linkedStoryBeatId, b.assigneeCharacterIds, b.prerequisiteObjectiveIds);
}
bool same(const StoryClueModel &a, const StoryClueModel &b)
{
    return same(a.metadata, b.metadata) &&
        std::tie(a.title, a.playerText, a.gmTruth, a.status, a.playerVisible, a.sourceEntityId,
                 a.sourceEntityType, a.discoveredAtUtc) ==
        std::tie(b.title, b.playerText, b.gmTruth, b.status, b.playerVisible, b.sourceEntityId,
                 b.sourceEntityType, b.discoveredAtUtc);
}
template<class T> auto find(std::vector<T> &values, const EntityId &id)
{
    return std::find_if(values.begin(), values.end(), [&](const T &value) { return value.metadata.id == id; });
}
template<class T> std::vector<EntityId> identities(const std::vector<T> &values)
{
    std::vector<EntityId> result;
    for (const auto &value : values) result.push_back(value.metadata.id);
    return result;
}
template<class T> struct Delta
{
    EntityId id;
    std::optional<T> before, after;
    std::size_t beforePosition = 0, afterPosition = 0;
};
template<class T> std::vector<Delta<T>> differences(const std::vector<T> &before, const std::vector<T> &after,
                                                  const std::set<EntityId> *trackedIds = nullptr)
{
    std::vector<Delta<T>> result;
    std::set<EntityId> seen;
    for (std::size_t index = 0; index < before.size(); ++index)
    {
        const auto &old = before[index]; seen.insert(old.metadata.id);
        const auto found = std::find_if(after.begin(), after.end(), [&](const T &value) { return value.metadata.id == old.metadata.id; });
        if (found == after.end()) result.push_back({old.metadata.id, old, {}, index, 0});
        else if (!same(old, *found)) result.push_back({old.metadata.id, old, *found, index, static_cast<std::size_t>(found - after.begin())});
    }
    for (std::size_t index = 0; index < after.size(); ++index)
        if (!seen.count(after[index].metadata.id)) result.push_back({after[index].metadata.id, {}, after[index], 0, index});
    if (trackedIds)
        result.erase(std::remove_if(result.begin(), result.end(), [&](const auto &change) {
            return !trackedIds->count(change.id);
        }), result.end());
    return result;
}
template<class T> void apply(std::vector<T> &values, const std::vector<Delta<T>> &changes, bool forward)
{
    // Validate the whole patch before changing even the staged collection.
    for (const auto &change : changes)
    {
        const auto &expected = forward ? change.before : change.after;
        const auto found = find(values, change.id);
        if ((expected && (found == values.end() || !same(*found, *expected))) || (!expected && found != values.end()))
            throw std::runtime_error("This record changed after the recorded edit. Newer changes were preserved.");
    }
    for (const auto &change : changes)
    {
        const auto &desired = forward ? change.after : change.before;
        const auto found = find(values, change.id);
        if (!desired) values.erase(found);
        else if (found != values.end()) { *found = *desired; found->metadata.touch(); }
        else
        {
            const auto index = std::min(values.size(), forward ? change.afterPosition : change.beforePosition);
            values.insert(values.begin() + static_cast<std::ptrdiff_t>(index), *desired);
        }
    }
}
template<class T> void order(std::vector<T> &values, const std::vector<EntityId> &ids)
{
    std::vector<T> result;
    for (const auto &id : ids)
    {
        const auto found = find(values, id);
        if (found == values.end()) throw std::runtime_error("A recorded entry is no longer available.");
        result.push_back(*found);
    }
    values = std::move(result);
}
template<class Mutation> bool record(Campaign &campaign, CampaignEditHistory::Persist persist,
                                    std::string label, Mutation mutation)
{
    const auto campaignId = campaign.metadata.id;
    auto perform = [&campaign, campaignId, persist = std::move(persist), mutation](bool forward) {
        if (campaign.metadata.id != campaignId) throw std::runtime_error("This edit belongs to another campaign.");
        Campaign staged = campaign;
        mutation(staged, forward);
        blackoutLocalPlayerDisplay(staged.localPlayerDisplay);
        staged.metadata.touch();
        if (!persist || !persist(staged)) throw std::runtime_error("The history change could not be saved. Nothing was changed.");
        campaign = std::move(staged);
    };
    return ProjectCommandManager::getInstance().recordApplied(std::make_unique<LambdaProjectCommand>(
        std::move(label), [perform] { perform(true); }, [perform] { perform(false); }));
}
}

namespace CampaignEditHistory
{
void recordPlanEntries(Campaign &campaign, EntityId sessionId, const std::vector<SessionPlanEntry> &before,
                       const std::vector<SessionPlanEntry> &after, Persist persist, std::string label)
{
    const auto changes = differences(before, after);
    const auto oldOrder = identities(before), newOrder = identities(after);
    if (changes.empty() && oldOrder == newOrder) return;
    record(campaign, std::move(persist), std::move(label), [=](Campaign &staged, bool forward) {
        auto *session = SessionPlan::findSession(staged, sessionId);
        if (!session || !session->endedAtUtc.empty()) throw std::runtime_error("This session is missing or has ended.");
        if (oldOrder != newOrder && identities(session->planEntries) != (forward ? oldOrder : newOrder))
            throw std::runtime_error("The scene sequence changed after this edit. Newer changes were preserved.");
        apply(session->planEntries, changes, forward);
        if (oldOrder != newOrder) order(session->planEntries, forward ? newOrder : oldOrder);
        if (session->liveModeActive && !session->liveState.activePlanEntryId.empty() &&
            !SessionPlan::findEntry(*session, session->liveState.activePlanEntryId))
            throw std::runtime_error("Activate another scene before removing the current scene from the plan.");
        session->metadata.touch();
    });
}
void recordSceneLinks(Campaign &campaign, EntityId sceneId, const std::vector<StoryLinkModel> &before,
                      const std::vector<StoryLinkModel> &after, Persist persist, std::string label)
{
    const auto changes = differences(before, after);
    const auto oldOrder = identities(before), newOrder = identities(after);
    if (changes.empty() && oldOrder == newOrder) return;
    record(campaign, std::move(persist), std::move(label), [=](Campaign &staged, bool forward) {
        auto *scene = SessionPlan::findScene(staged, sceneId);
        if (!scene) throw std::runtime_error("The scene is no longer available.");
        if (oldOrder != newOrder && identities(scene->links) != (forward ? oldOrder : newOrder))
            throw std::runtime_error("The scene links changed after this edit. Newer changes were preserved.");
        apply(scene->links, changes, forward);
        if (oldOrder != newOrder) order(scene->links, forward ? newOrder : oldOrder);
        scene->metadata.touch();
    });
}
bool recordObjectives(Campaign &campaign, const std::vector<StoryObjectiveModel> &before,
                      const std::vector<StoryObjectiveModel> &after, Persist persist, std::string label,
                      const std::set<EntityId> *trackedIds)
{
    const auto changes = differences(before, after, trackedIds);
    return changes.empty() || record(campaign, std::move(persist), std::move(label), [changes](Campaign &staged, bool forward) {
        apply(staged.storyObjectives, changes, forward);
    });
}
bool recordClues(Campaign &campaign, const std::vector<StoryClueModel> &before,
                 const std::vector<StoryClueModel> &after, Persist persist, std::string label,
                 const std::set<EntityId> *trackedIds)
{
    const auto changes = differences(before, after, trackedIds);
    return changes.empty() || record(campaign, std::move(persist), std::move(label), [changes](Campaign &staged, bool forward) {
        apply(staged.storyClues, changes, forward);
    });
}
SessionPlan::Result editPlan(Campaign &campaign, EntityId sessionId, const Persist &persist, std::string label,
                             const std::function<SessionPlan::Result()> &mutation)
{
    const auto *session = SessionPlan::findSession(campaign, sessionId);
    const auto before = session ? session->planEntries : std::vector<SessionPlanEntry>{};
    auto result = mutation();
    session = SessionPlan::findSession(campaign, sessionId);
    if (result.applied() && session) recordPlanEntries(campaign, std::move(sessionId), before, session->planEntries, persist, std::move(label));
    return result;
}
}
