#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <algorithm>
#include <utility>

// Follow Live unless the GM deliberately chooses another session. Selection
// changes never transfer or clear the per-session editor drafts.
struct LiveOutcomeSessionSelection
{
    EntityId campaignId, sessionId;
    bool historical = false;
    bool synchronize(const Campaign &campaign, const EntityId &activeId)
    {
        const auto previous = sessionId;
        if (campaignId != campaign.metadata.id)
        { campaignId = campaign.metadata.id; sessionId.clear(); historical = false; }
        const auto valid = [&](const EntityId &id) {
            return !id.empty() && std::any_of(campaign.sessions.begin(), campaign.sessions.end(), [&](const auto &session) {
                return session.metadata.id == id &&
                    std::find(session.metadata.tags.begin(), session.metadata.tags.end(), "archived") == session.metadata.tags.end();
            });
        };
        if (!valid(sessionId)) { sessionId.clear(); historical = false; }
        if (valid(activeId) && sessionId == activeId) historical = false;
        if (!historical && valid(activeId)) sessionId = activeId;
        if (sessionId.empty())
            for (auto it = campaign.sessions.rbegin(); it != campaign.sessions.rend(); ++it)
                if (valid(it->metadata.id)) { sessionId = it->metadata.id; break; }
        return previous != sessionId;
    }
    void choose(EntityId id, const EntityId &activeId)
    { sessionId = std::move(id); historical = sessionId != activeId; }
    void followCurrent() { historical = false; }
};
