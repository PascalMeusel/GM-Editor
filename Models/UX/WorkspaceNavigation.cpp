#include "Models/UX/WorkspaceNavigation.hpp"
#include <stdexcept>
#include <utility>

namespace gm::ux
{
std::size_t WorkspaceNavigation::index(WorkspaceTarget workspace)
{
    const auto value = static_cast<std::size_t>(workspace);
    if (value >= workspaceCount) throw std::out_of_range("Unknown workspace");
    return value;
}

WorkspaceNavigation::WorkspaceNavigation() { reset({}); }

void WorkspaceNavigation::reset(EntityId campaignId, WorkspaceTarget initial)
{
    index(initial);
    campaignId_ = std::move(campaignId);
    active_ = initial;
    returns_.clear();
    for (std::size_t i = 0; i < locations_.size(); ++i)
    {
        locations_[i] = {};
        locations_[i].workspace = static_cast<WorkspaceTarget>(i);
    }
}

WorkspaceLocation &WorkspaceNavigation::current() { return locations_[index(active_)]; }
const WorkspaceLocation &WorkspaceNavigation::current() const { return locations_[index(active_)]; }
const WorkspaceLocation &WorkspaceNavigation::location(WorkspaceTarget workspace) const
{
    return locations_[index(workspace)];
}
const WorkspaceLocation *WorkspaceNavigation::returnDestination() const
{
    return returns_.empty() ? nullptr : &returns_.back();
}

bool WorkspaceNavigation::navigate(WorkspaceTarget destination, bool rememberReturn, const LeaveGuard &guard)
{
    index(destination);
    if (destination == active_ && !rememberReturn) return true;
    if (guard && !guard()) return false;
    if (rememberReturn)
    {
        // Bound a session's navigation history without retaining campaign objects.
        if (returns_.size() == 32) returns_.erase(returns_.begin());
        returns_.push_back(current());
    }
    active_ = destination;
    return true;
}

bool WorkspaceNavigation::returnToPrevious(const LeaveGuard &guard)
{
    if (returns_.empty()) return false;
    if (guard && !guard()) return false;
    auto destination = std::move(returns_.back());
    returns_.pop_back();
    active_ = destination.workspace;
    current() = std::move(destination);
    return true;
}

void WorkspaceNavigation::reconcile(const Available &available)
{
    if (!available) return;
    const auto validate = [&](WorkspaceLocation &location) {
        if (!location.sessionId.empty() && !available("Session", location.sessionId))
        {
            location.sessionUnavailable = true;
            location.sessionId.clear();
            location.planEntryId.clear();
            location.sceneId.clear();
        }
        if (!location.planEntryId.empty() && !available("PlanEntry", location.planEntryId))
        {
            location.sessionUnavailable = true;
            location.planEntryId.clear();
        }
        if (!location.sceneId.empty() && !available("scene", location.sceneId))
        {
            location.sessionUnavailable = true;
            location.sceneId.clear();
        }
        if (!location.selectedEntityId.empty() && !available(location.selectedEntityType, location.selectedEntityId))
        {
            location.selectedEntityId.clear();
            location.selectedEntityType.clear();
            location.editorRoute.clear();
            location.targetUnavailable = true;
        }
    };
    for (auto &location : locations_) validate(location);
    for (auto &location : returns_) validate(location);
}
}
