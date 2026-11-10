#include "ProjectEventBus.hpp"
#include "ProjectLogger.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <utility>

ProjectEventBus &ProjectEventBus::getInstance()
{
    static ProjectEventBus bus;
    return bus;
}

int ProjectEventBus::subscribe(Handler handler)
{
    const int subscriptionId = _nextSubscriptionId++;
    _handlers[subscriptionId] = std::move(handler);
    return subscriptionId;
}

void ProjectEventBus::unsubscribe(int subscriptionId)
{
    _handlers.erase(subscriptionId);
}

void ProjectEventBus::publish(ProjectEvent event)
{
    for (const auto &warning : publishDetailed(std::move(event)))
    {
        try { ProjectLogger::getInstance().warning("ProjectEventBus", warning); }
        catch (...) {} // Logging must not change an already dispatched event.
    }
}

std::vector<std::string> ProjectEventBus::publishDetailed(ProjectEvent event)
{
    if (event.occurredAtUtc.empty())
        event.occurredAtUtc = currentUtcTimestamp();

    event.sequence = _nextSequence;
    _history.push_back(event);
    ++_nextSequence;
    constexpr std::size_t maxHistory = 200;
    if (_history.size() > maxHistory)
        _history.erase(_history.begin(), _history.begin() + static_cast<std::ptrdiff_t>(_history.size() - maxHistory));

    // Copy before invoking user code: subscriptions may change during dispatch.
    // New subscriptions take effect on the next event; every current subscriber
    // gets this event even if another unsubscribes it or throws.
    std::vector<std::pair<int, Handler>> handlers(_handlers.begin(), _handlers.end());
    std::sort(handlers.begin(), handlers.end(), [](const auto &left, const auto &right) {
        return left.first < right.first;
    });
    std::vector<std::string> warnings;
    for (const auto &handler : handlers)
    {
        try { handler.second(event); }
        catch (const std::exception &error) {
            warnings.push_back("Event subscriber " + std::to_string(handler.first) + ": " + error.what());
        }
        catch (...) {
            warnings.push_back("Event subscriber " + std::to_string(handler.first) + ": unknown failure.");
        }
    }
    return warnings;
}

const char *projectEventTypeLabel(ProjectEventType type)
{
    switch (type)
    {
    case ProjectEventType::EntityCreated:
        return "Entity Created";
    case ProjectEventType::EntityUpdated:
        return "Entity Updated";
    case ProjectEventType::EntityDeleted:
        return "Entity Deleted";
    case ProjectEventType::CampaignSaved:
        return "Campaign Saved";
    case ProjectEventType::CampaignLoaded:
        return "Campaign Loaded";
    case ProjectEventType::SessionStarted:
        return "Session Started";
    case ProjectEventType::SessionEnded:
        return "Session Ended";
    case ProjectEventType::CombatChanged:
        return "Combat Changed";
    case ProjectEventType::MapChanged:
        return "Map Changed";
    case ProjectEventType::InventoryChanged:
        return "Inventory Changed";
    case ProjectEventType::AudioChanged:
        return "Audio Changed";
    }
    return "Project Event";
}
