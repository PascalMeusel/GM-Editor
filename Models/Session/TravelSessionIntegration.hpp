#pragma once
#include "Models/Session/SessionPlanService.hpp"
#include "Models/Session/SessionControlService.hpp"
#include "Models/Map/MapAtlasService.hpp"
#include "Models/Map/TravelSceneGenerator.hpp"
#include <set>

namespace TravelSession
{
inline EntityId geographicSource(const Campaign &campaign, EntityId preferred)
{
    const auto ancestor = [&](EntityId id) {
        std::set<EntityId> seen;
        while (!id.empty() && seen.insert(id).second)
        {
            const auto found = std::find_if(campaign.maps.begin(), campaign.maps.end(),
                [&](const MapModel &map) { return map.metadata.id == id; });
            if (found == campaign.maps.end()) break;
            if (found->atlasLevel != MapLevel::Tactical) return id;
            id = MapAtlasService::effectiveParentId(*found);
        }
        return EntityId{};
    };
    if (const auto result = ancestor(preferred); !result.empty()) return result;
    return ancestor(campaign.partyTravel.currentMapId);
}

inline EntityId activeSessionId(const Campaign &campaign)
{
    const auto *session = SessionControl::activeSession(campaign);
    return session ? session->metadata.id : EntityId{};
}

// Called only inside the existing map acceptance transaction. The inner
// service stages the ordinary session scene/entry; the outer transaction saves
// map, optional encounter, scene and entry together or rolls all of them back.
inline bool stageAcceptedScene(Campaign &campaign, const EntityId &expectedSession,
                               const TravelSceneCandidate &candidate, const EntityId &mapId,
                               const EntityId &encounterId, std::string &error)
{
    if (activeSessionId(campaign) != expectedSession)
    {
        error = "The active session changed. Generate and review travel scenes again.";
        return false;
    }
    if (expectedSession.empty()) return true;
    if (std::none_of(campaign.maps.begin(), campaign.maps.end(), [&](const auto &map) { return map.metadata.id == mapId; }) ||
        (!encounterId.empty() && std::none_of(campaign.encounters.begin(), campaign.encounters.end(),
            [&](const auto &encounter) { return encounter.metadata.id == encounterId; })))
    {
        error = "The accepted travel map or encounter is missing.";
        return false;
    }
    const auto result = SessionPlan::createScene(campaign, expectedSession, candidate.title,
        candidate.summary, [&](Campaign &staged) {
            auto *session = SessionPlan::findSession(staged, expectedSession);
            if (!session || session->planEntries.empty()) return false;
            auto *scene = SessionPlan::findScene(staged, session->planEntries.back().storyBeatId);
            if (!scene) return false;
            scene->clues = candidate.discovery;
            scene->consequences = candidate.complication;
            scene->gmNotes = candidate.gmSecret;
            StoryLinkModel mapLink; mapLink.targetEntityType = "Map"; mapLink.targetEntityId = mapId;
            scene->links.push_back(std::move(mapLink));
            if (!encounterId.empty())
            {
                StoryLinkModel encounterLink;
                encounterLink.targetEntityType = "Encounter"; encounterLink.targetEntityId = encounterId;
                scene->links.push_back(std::move(encounterLink));
            }
            return true;
        });
    if (!result.applied()) { error = result.message; return false; }
    return true;
}
}
