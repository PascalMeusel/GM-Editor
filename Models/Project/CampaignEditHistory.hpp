#pragma once

#include "Models/Session/SessionPlanService.hpp"
#include <functional>
#include <set>

namespace CampaignEditHistory
{
using Persist = SessionPlan::Persist;
// The owning editor clears history before loading another campaign. Commands
// retain only typed record deltas and stable IDs; each application stages the
// current campaign and persists before replacing its live state.
void recordPlanEntries(Campaign &, EntityId sessionId, const std::vector<SessionPlanEntry> &before,
                       const std::vector<SessionPlanEntry> &after, Persist, std::string label);
void recordSceneLinks(Campaign &, EntityId sceneId, const std::vector<StoryLinkModel> &before,
                      const std::vector<StoryLinkModel> &after, Persist, std::string label);
// Pass full ordered collections, even for a scoped edit. Filtering happens after
// positions are captured. True acknowledges registration (or no actual delta);
// false means the caller must retain its pending baseline for a later retry.
bool recordObjectives(Campaign &, const std::vector<StoryObjectiveModel> &before,
                      const std::vector<StoryObjectiveModel> &after, Persist, std::string label,
                      const std::set<EntityId> *trackedIds = nullptr);
bool recordClues(Campaign &, const std::vector<StoryClueModel> &before,
                 const std::vector<StoryClueModel> &after, Persist, std::string label,
                 const std::set<EntityId> *trackedIds = nullptr);
SessionPlan::Result editPlan(Campaign &, EntityId sessionId, const Persist &, std::string label,
                             const std::function<SessionPlan::Result()> &mutation);
}
