#pragma once

#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/ContextLinkMetadata.hpp"
#include <algorithm>
#include <utility>

struct CampaignSceneDraftContext
{
    EntityId campaignId, sessionId, sceneId;
    bool empty() const { return campaignId.empty() && sessionId.empty() && sceneId.empty(); }
};

// Called on the proposed campaign so the entity and its scene link are saved
// together. A stale creation context must never redirect a link to another scene.
inline bool linkCampaignEntityDraftToScene(Campaign &proposed, const CampaignSceneDraftContext &context,
                                         const EntityId &entityId, const std::string &type, const std::string &name)
{
    if (context.empty()) return true;
    if (context.campaignId != proposed.metadata.id || context.sessionId.empty() || context.sceneId.empty() || entityId.empty())
        return false;
    const auto archived = [](const EntityMetadata &metadata) {
        return std::find(metadata.tags.begin(), metadata.tags.end(), "archived") != metadata.tags.end();
    };
    const auto session = std::find_if(proposed.sessions.begin(), proposed.sessions.end(), [&](const auto &item) {
        return item.metadata.id == context.sessionId;
    });
    if (session == proposed.sessions.end() || archived(session->metadata) || !session->endedAtUtc.empty() ||
        std::none_of(session->planEntries.begin(), session->planEntries.end(), [&](const auto &entry) {
            return entry.storyBeatId == context.sceneId;
        })) return false;
    for (auto &thread : proposed.storyThreads)
        for (auto &scene : thread.beats)
            if (scene.metadata.id == context.sceneId && !archived(thread.metadata) && !archived(scene.metadata))
            {
                auto link = std::find_if(scene.links.begin(), scene.links.end(), [&](const auto &item) {
                    return item.targetEntityId == entityId && item.targetEntityType == type;
                });
                if (link == scene.links.end())
                {
                    StoryLinkModel created;
                    created.targetEntityId = entityId;
                    created.targetEntityType = type;
                    created.label = name;
                    scene.links.push_back(std::move(created));
                }
                else { link->label = name; link->metadata.touch(); }
                scene.metadata.touch();
                thread.metadata.touch();
                return true;
            }
    return false;
}

// A creator keeps its editable draft until persistence succeeds. Retrying the
// same stable entity replaces the proposal instead of inserting a duplicate.
template <typename Entity, typename Persist>
bool persistCampaignEntityDraft(Campaign &campaign, const Entity &draft,
                                std::vector<Entity> Campaign::*collection,
                                const Persist &persist)
{
    if (draft.metadata.id.empty())
        return false;
    Campaign proposed = campaign;
    auto &entries = proposed.*collection;
    const auto existing = std::find_if(entries.begin(), entries.end(), [&](const Entity &entry) {
        return entry.metadata.id == draft.metadata.id;
    });
    if (existing == entries.end())
        entries.push_back(draft);
    else
    {
        Entity merged = draft;
        preserveCurrentContextLinks(merged.metadata, existing->metadata);
        *existing = std::move(merged);
    }
    proposed.metadata.touch();
    if (!persist(proposed))
        return false;
    campaign = std::move(proposed);
    return true;
}
