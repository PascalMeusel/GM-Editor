#pragma once

#include "Models/Core/EntityMetadata.hpp"

#include <functional>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>

enum class ProjectEventType
{
    EntityCreated,
    EntityUpdated,
    EntityDeleted,
    CampaignSaved,
    CampaignLoaded,
    SessionStarted,
    SessionEnded,
    CombatChanged,
    MapChanged,
    InventoryChanged,
    AudioChanged
};

struct ProjectEvent
{
    ProjectEventType type = ProjectEventType::EntityUpdated;
    EntityId campaignId;
    EntityId entityId;
    std::string entityType;
    std::string message;
    std::string occurredAtUtc;
    std::uint64_t sequence = 0;
};

class ProjectEventBus
{
public:
    using Handler = std::function<void(const ProjectEvent &)>;

    static ProjectEventBus &getInstance();

    int subscribe(Handler handler);
    void unsubscribe(int subscriptionId);
    void publish(ProjectEvent event);
    std::vector<std::string> publishDetailed(ProjectEvent event);
    const std::vector<ProjectEvent> &history() const { return _history; }

private:
    int _nextSubscriptionId = 1;
    std::unordered_map<int, Handler> _handlers;
    std::vector<ProjectEvent> _history;
    std::uint64_t _nextSequence = 1;
};

const char *projectEventTypeLabel(ProjectEventType type);
