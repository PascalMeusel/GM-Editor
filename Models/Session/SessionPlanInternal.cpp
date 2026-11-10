#include "Models/Session/SessionPlanServiceInternal.hpp"

namespace SessionPlan::detail
{
Result editable(const SessionModel *session)
{
    if (!session)
        return error(Status::NotFound, "Session not found.");
    if (!session->endedAtUtc.empty())
        return error(Status::Conflict, "This session has ended. Create a new session plan.");
    return success(session->metadata.id);
}

Result active(const SessionModel *session)
{
    const Result result = editable(session);
    if (!result.applied())
        return result;
    return session->liveModeActive
               ? result
               : error(Status::Conflict, "Start this session before changing its live state.");
}

StoryThreadModel *threadForScene(Campaign &campaign, const EntityId &sceneId)
{
    for (auto &thread : campaign.storyThreads)
        for (const auto &scene : thread.beats)
            if (scene.metadata.id == sceneId)
                return &thread;
    return nullptr;
}
} // namespace SessionPlan::detail

