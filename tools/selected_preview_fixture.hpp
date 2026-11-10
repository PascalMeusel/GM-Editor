#pragma once
#include "Models/Campaign/Campaign.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <cstdio>

// Text-only synthetic identities shared by pure projection and actual UI tests.
// Duplicate display names are deliberate; no portrait/image/file asset exists.
struct SelectedPreviewFixture
{
    Campaign campaign;
    EntityId npc,otherNpc,item,otherItem,owner,otherOwner,entry;
    SelectedPreviewFixture()
    {
        auto &c=campaign;c.name="Selected preview "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
        NPCModel a,b;a.name=b.name="Same named guide";a.role="CANONICAL_SELECTED_ROLE";b.role="OTHER_NPC_ROLE";
        a.description="PRIVATE_NPC_DESCRIPTION";a.secrets="PRIVATE_NPC_SECRET";a.goals="PRIVATE_NPC_GOAL";
        a.dialogueNotes="PRIVATE_NPC_DIALOGUE";a.resources="PRIVATE_NPC_RESOURCES";a.relationships="PRIVATE_NPC_RELATIONSHIP";
        a.threatLocation="PRIVATE_NPC_LOCATION";a.attitudeTowardParty="PRIVATE_NPC_ATTITUDE";
        b.secrets="OTHER_NPC_SECRET";npc=a.metadata.id;otherNpc=b.metadata.id;
        ItemModel first,second;first.name=second.name="Same named relic";first.description="PRIVATE_ITEM_DESCRIPTION";
        first.commandWords="PRIVATE_ITEM_COMMAND";first.effects={"PRIVATE_ITEM_EFFECT"};first.baseValue=731;
        first.cursed=true;first.stackable=true;first.itemType="Gear";first.category="Gear";
        second.description="OTHER_ITEM_DESCRIPTION";item=first.metadata.id;otherItem=second.metadata.id;
        c.customItems={first,second};
        CharacterModel p,q;std::snprintf(p.nameBuffer,sizeof(p.nameBuffer),"%s","Same named owner");
        std::snprintf(q.nameBuffer,sizeof(q.nameBuffer),"%s","Same named owner");
        owner=p.metadata.id;otherOwner=q.metadata.id;
        p.gmNotes="PRIVATE_OWNER_GM_NOTES";q.gmNotes="OTHER_OWNER_GM_NOTES";
        p.inventory.entries={{item,2},{otherItem,13}};q.inventory.entries={{item,9}};
        p.inventory.entries[0].stateNotes="PRIVATE_ENTRY_NOTES";entry=p.inventory.entries[0].metadata.id;
        p.inventory.entries[0].charges=4;p.inventory.entries[0].attuned=true;
        a.inventory.entries={{item,7}};c.npcs={a,b};c.characters={p,q};c.partyInventory.entries={{item,3}};
        auto &display=c.localPlayerDisplay;display.mapOnly=false;display.showMap=false;display.showSharedArt=false;
        display.showVisibleNpcs=true;display.visibleNpcIds={npc};display.showParty=true;display.showPartyInventory=true;
        display.visibleCharacterIds={owner};display.showInitiative=false;display.showPublicHandouts=false;
        display.enabled=false;display.published=false;display.blackout=true;
    }
};
