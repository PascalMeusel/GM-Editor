#pragma once

#include "Models/Session/SessionPlanService.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace SessionPlan::detail
{
inline Result error(Status status, const char *message)
{
    return {status, message, {}, {}, {}};
}

inline Result success(const EntityId &session = {}, const EntityId &entry = {}, const EntityId &scene = {})
{
    return {Status::Applied, {}, session, entry, scene};
}

inline bool archived(const EntityMetadata &metadata)
{
    return std::find(metadata.tags.begin(), metadata.tags.end(), "archived") != metadata.tags.end();
}

inline bool blank(const std::string &text)
{
    return text.find_first_not_of(" \t\r\n") == std::string::npos;
}

template<class Mutation>
Result commit(Campaign &campaign, const Persist &persist, Mutation mutation)
{
    if (!persist)
        return error(Status::PersistenceFailed, "A save operation is required.");
    Campaign staged = campaign;
    Result result = mutation(staged);
    if (!result.applied())
        return result;
    staged.metadata.touch();
    try
    {
        if (!persist(staged))
            return error(Status::PersistenceFailed,
                         "The changes could not be saved. Your previous session state is unchanged.");
    }
    catch (const std::exception &)
    {
        return error(Status::PersistenceFailed,
                     "The changes could not be saved. Your previous session state is unchanged.");
    }
    catch (...)
    {
        return error(Status::PersistenceFailed,
                     "The changes could not be saved. Your previous session state is unchanged.");
    }
    campaign = std::move(staged);
    return result;
}

Result editable(const SessionModel *session);
Result active(const SessionModel *session);
StoryThreadModel *threadForScene(Campaign &campaign, const EntityId &sceneId);
} // namespace SessionPlan::detail

