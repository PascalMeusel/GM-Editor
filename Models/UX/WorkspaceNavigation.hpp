#pragma once

#include "Models/Core/EntityMetadata.hpp"
#include "Models/UX/LegacySurfaceRegistry.hpp"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace gm::ux
{
// Runtime navigation owns identifiers and view state, never campaign pointers.
struct WorkspaceLocation
{
    WorkspaceTarget workspace = WorkspaceTarget::Home;
    EntityId sessionId, planEntryId, sceneId;
    EntityId selectedEntityId;
    std::string selectedEntityType;
    std::string filter;
    int category = 0;
    std::string editorRoute;
    bool targetUnavailable = false;
    bool sessionUnavailable = false;
};

class WorkspaceNavigation
{
public:
    using LeaveGuard = std::function<bool()>;
    using Available = std::function<bool(const std::string &, const EntityId &)>;

    WorkspaceNavigation();
    void reset(EntityId campaignId, WorkspaceTarget initial = WorkspaceTarget::Home);
    const EntityId &campaignId() const { return campaignId_; }
    WorkspaceLocation &current();
    const WorkspaceLocation &current() const;
    const WorkspaceLocation &location(WorkspaceTarget workspace) const;
    const WorkspaceLocation *returnDestination() const;

    // A failed save/leave guard leaves both the location and return history intact.
    bool navigate(WorkspaceTarget destination, bool rememberReturn, const LeaveGuard &guard);
    bool returnToPrevious(const LeaveGuard &guard);
    void reconcile(const Available &available);

private:
    static constexpr std::size_t workspaceCount = 8;
    static std::size_t index(WorkspaceTarget workspace);
    EntityId campaignId_;
    WorkspaceTarget active_ = WorkspaceTarget::Home;
    std::array<WorkspaceLocation, workspaceCount> locations_;
    std::vector<WorkspaceLocation> returns_;
};
}
