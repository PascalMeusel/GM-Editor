#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <functional>
#include <string>

class NPCOverviewContext final
{
public:
    NPCOverviewContext(Campaign &campaign, std::function<void(const EntityId &)> &openStoryThreadHandler)
        : campaign(campaign), openStoryThreadHandler(openStoryThreadHandler)
    {
    }

    Campaign &campaign;
    std::function<void(const EntityId &)> &openStoryThreadHandler;
};

struct NPCOverviewPersistenceState
{
    bool saveFailed = false;
    EntityId failureCampaignId;
    std::string errorMessage;
    bool resourceDraftPendingSave = false;
    bool actionDraftPendingSave = false;
};

struct NPCOverviewSelectionState
{
    int npcIndex = 0;
    int monsterIndex = 0;
    int monsterActionIndex = -1;
    int factionIndex = 0;
    int deityIndex = 0;
    int catalogMonsterVariantIndex = 0;
    int npcSpellToAddIndex = 0;
};

struct NPCOverviewDirectoryState
{
    std::string npcSpellAssignmentSearch;
    std::string npcSearch;
    int npcCategoryFilter = -1;
    int npcThreatFilter = -1;
    bool npcShowArchived = true;
    bool npcPlayerVisibleOnly = false;
    bool npcLiveRelevantOnly = false;
    bool npcStoryLinkedOnly = false;
    bool npcCombatLinkedOnly = false;
    std::string monsterSearch;
    bool monsterShowArchived = true;
};

struct NPCOverviewDraftState
{
    MonsterActionModel monsterAction;
    ResourceTrackerModel npcResource;
};

struct NPCOverviewRemovalState
{
    int npcIndex = -1;
    bool npcIsArchive = false;
    int monsterIndex = -1;
    bool monsterIsArchive = false;
};

struct NPCOverviewState
{
    NPCOverviewPersistenceState persistence;
    NPCOverviewSelectionState selection;
    NPCOverviewDirectoryState directory;
    NPCOverviewDraftState drafts;
    NPCOverviewRemovalState pendingRemoval;
};
