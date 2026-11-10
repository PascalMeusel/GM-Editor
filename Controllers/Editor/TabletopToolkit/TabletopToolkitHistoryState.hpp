#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <set>

// Shared only by toolkit views editing the same live Campaign object. Pending
// history is one transaction; each view continues to own its new-form drafts.
struct TabletopToolkitHistoryState
{
    const Campaign *owner = nullptr;
    std::vector<StoryObjectiveModel> objectives;
    std::vector<StoryClueModel> clues;
    std::set<EntityId> objectiveIds, clueIds;
    bool pending = false;
};
