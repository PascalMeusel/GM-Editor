bool testSelectedContextPreview()
{
    using namespace PlayerSafeSelection;bool ok=true;SelectedPreviewFixture f;auto &c=f.campaign;const auto original=c;
    const auto selected=npc(c,f.npc),other=npc(c,f.otherNpc);
    ok&=require(selected.state==State::Staged&&selected.candidate&&selected.candidate->name=="Same named guide"&&
        selected.candidate->role=="CANONICAL_SELECTED_ROLE","Selected NPC projection did not resolve exact stable ID before DTO conversion");
    ok&=require(other.state==State::NotSelected&&other.candidate&&other.candidate->role=="OTHER_NPC_ROLE",
        "Same-name NPC inherited another NPC's staging consent");
    const auto rows=inventory(c,f.item);
    ok&=require(rows.state==State::Staged&&rows.rows.size()==2,"Selected inventory included wrong item/owner/NPC inventory");
    if(rows.rows.size()==2)ok&=require(rows.rows[0].ownerName=="Same named owner"&&rows.rows[0].itemName=="Same named relic"&&
        rows.rows[0].quantity==2&&!rows.rows[0].partyOwned&&rows.rows[1].partyOwned&&rows.rows[1].ownerName.empty()&&rows.rows[1].quantity==3,
        "Selected inventory violated owner/name/quantity allowlist or matched duplicate display names");
    const auto otherRows=inventory(c,f.otherItem);
    ok&=require(otherRows.state==State::Staged&&otherRows.rows.size()==1&&otherRows.rows[0].quantity==13,
        "Same-name item preview borrowed selected item's rows");
    c.localPlayerDisplay.mapOnly=true;
    ok&=require(npc(c,f.npc).state==State::MapOnly&&inventory(c,f.item).state==State::MapOnly&&inventory(c,f.item).rows.empty(),
        "Map-only display advertised text as staged output");
    c.localPlayerDisplay.mapOnly=false;c.localPlayerDisplay.showVisibleNpcs=false;
    ok&=require(npc(c,f.npc).state==State::ChannelHidden&&npc(c,f.npc).candidate.has_value(),
        "NPC candidate confused channel-disabled content with staged consent");
    for(int gate=0;gate<2;++gate){c.localPlayerDisplay.showParty=gate!=0;c.localPlayerDisplay.showPartyInventory=gate==0;
        ok&=require(inventory(c,f.item).state==State::ChannelHidden&&inventory(c,f.item).rows.empty(),"Inventory ignored independent channel opt-in");}
    c.localPlayerDisplay.showParty=c.localPlayerDisplay.showPartyInventory=true;
    c.localPlayerDisplay.visibleCharacterIds.clear();c.partyInventory.entries.clear();
    ok&=require(inventory(c,f.item).state==State::NotSelected&&inventory(c,f.item).rows.empty(),
        "Unselected character or visible NPC inventory entered public preview");
    c.localPlayerDisplay.visibleCharacterIds={f.owner};c.characters[0].inventory.entries[0].quantity=0;
    ok&=require(inventory(c,f.item).rows.empty(),"Zero quantity item entered staged inventory preview");
    c=original;
    c.characters[1].inventory.entries.clear();c.localPlayerDisplay.visibleCharacterIds.push_back(f.otherOwner);
    c.characters.push_back(c.characters[1]);
    ok&=require(inventory(c,f.item).state==State::Staged&&inventory(c,f.item).rows.size()==2,
        "Duplicate unrelated owner without this item blocked independently valid staged uses");
    c=original;
    c.characters.push_back(c.characters[0]);
    ok&=require(inventory(c,f.item).state==State::Ambiguous&&inventory(c,f.item).rows.empty(),"Duplicate eligible owner ID was silently resolved");
    c.characters.pop_back();c.customItems.push_back(c.customItems[0]);
    ok&=require(inventory(c,f.item).state==State::Ambiguous&&inventory(c,f.item).rows.empty(),"Duplicate canonical item ID was silently resolved");
    c.customItems.pop_back();c.npcs.push_back(c.npcs[0]);
    ok&=require(npc(c,f.npc).state==State::Ambiguous&&!npc(c,f.npc).candidate,"Duplicate NPC ID returned a private source by position");
    ok&=require(npc(c,{}).state==State::Unavailable&&inventory(c,"missing-item").state==State::Unavailable,
        "Missing selection silently used first available entity");
    return ok;
}
