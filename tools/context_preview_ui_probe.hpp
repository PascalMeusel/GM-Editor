#pragma once
#include "Controllers/Editor/Widgets/NPCOverview/NPCOverviewController.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Controllers/Editor/Widgets/InventoryManager/InventoryManagerController.hpp"
#include <tuple>

// Calls actual text-only views. Tests provide no portrait or item image assets.
// Observation helpers never choose a mode, reveal secrets, or authorize review.
struct ContextPreviewUiProbe
{
    static void draw(NPCOverviewController &c){c._view->draw();}
    static void draw(ItemCreatorController &c){c._view->draw();}
    static void draw(InventoryManagerController &c){c._view->draw();}
    static EntityId npc(const NPCOverviewController &c){return c._selectedNpcIndex>=0&&
        c._selectedNpcIndex<int(c._currentCampaign.npcs.size())?c._currentCampaign.npcs[c._selectedNpcIndex].metadata.id:EntityId{};}
    static EntityId item(const ItemCreatorController &c){return c._selectedItemId;}
    static const MonsterActionModel &power(const ItemCreatorController &c){return c._itemPowerDraft;}
    static bool powerDirty(const ItemCreatorController &c){return c._powerDraftEdited;}
    static auto inventory(const InventoryManagerController &c){return std::make_tuple(c._selectedOwnerId,
        c._selectedItemId,c._selectedInventoryEntryId,int(c._activeTask));}
};
