// The production helper is isolated from its private source editor here: a
// whole-frame log of that editor is not a player-field privacy boundary.
void selectedPreviewUi(bool de,bool item)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Selected canonical field preview 27px ")+(item?"item ":"NPC ")+(de?"DE":"EN"));
    SelectedPreviewFixture f;auto &c=f.campaign;auto candidate=c.customItems[0];candidate.name="UNSAVED_ITEM_CANDIDATE";
    int reviews=0;EntityId requested;const auto id=item?f.item:f.npc;
    const auto callback=[&](EntityId selected){++reviews;requested=selected;};
    ui.draw=[&]{if(item)SelectedPlayerPreview::item(c,id,callback,&candidate);else SelectedPlayerPreview::npc(c,id,callback);};
    ui.frame(3);const char *open=de?"Spielerfelder ansehen":"Preview player fields";
    const char *close=de?"Schließen":"Close",*review=de?"Bereitgestellte Inhalte prüfen":"Review staged content";
    ui.compactClick(open);ui.compactBounds(close,"##SelectedPlayerFields");
    for(const char *privateText:{"PRIVATE_NPC_DESCRIPTION","PRIVATE_NPC_SECRET","PRIVATE_NPC_GOAL","PRIVATE_NPC_DIALOGUE",
        "PRIVATE_NPC_RESOURCES","PRIVATE_NPC_RELATIONSHIP","PRIVATE_ITEM_DESCRIPTION","PRIVATE_ITEM_COMMAND",
        "PRIVATE_ITEM_EFFECT","PRIVATE_ENTRY_NOTES","PRIVATE_OWNER_GM_NOTES","OTHER_OWNER_GM_NOTES"})
        require(!contains(ui.rendered,privateText),"Selected preview submitted a field outside the canonical public allowlist");
    if(item)require(contains(ui.rendered,"UNSAVED_ITEM_CANDIDATE")&&contains(ui.rendered,"Same named relic × 2")&&
        contains(ui.rendered,"Same named relic × 3")&&!contains(ui.rendered,"× 7")&&!contains(ui.rendered,"× 9")&&!contains(ui.rendered,"× 13"),
        "Item preview mixed editor candidate with canonical selected rows or included another owner/item");
    else require(contains(ui.rendered,"CANONICAL_SELECTED_ROLE")&&!contains(ui.rendered,"OTHER_NPC_ROLE"),
        "Same-name NPC preview resolved a different source");
    ui.reviewTabTo(close);ui.reviewKey(ImGuiKey_Space);require(!ui.popupOpen()&&reviews==0,"Closing selected preview issued a review request");
    c.localPlayerDisplay.mapOnly=true;ui.compactClick(open);
    require(contains(ui.rendered,de?"nur die Karte":"map only")&&(!item||!contains(ui.rendered,"Same named relic ×")),
        "Map-only preview still represented inventory text as staged output");
    ui.reviewKey(ImGuiKey_Escape);require(!ui.popupOpen(),"Selected preview Escape failed");
    c.localPlayerDisplay.mapOnly=false;ui.compactClick(open);ui.compactClick(review);
    require(!ui.popupOpen()&&reviews==1&&requested==id&&!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published&&
        c.customItems[0].name=="Same named relic"&&c.characters[0].inventory.entries[0].quantity==2,
        "Explicit preview review callback changed source, staging or publication");
}

template<class Values> const typename Values::value_type &contextSaved(const Values &values,const EntityId &id)
{
    const auto found=std::find_if(values.begin(),values.end(),[&](const auto &v){return v.metadata.id==id;});
    require(found!=values.end(),"Context preview persisted source is missing");return *found;
}
void contextOpenReview(HeadlessPanel &ui,EditorWindowController &editor,bool de)
{
    ui.compactClick(de?"Spielerfelder ansehen":"Preview player fields");
    ui.compactBounds(de?"Schließen":"Close","##SelectedPlayerFields");
    ui.compactClick(de?"Bereitgestellte Inhalte prüfen":"Review staged content");
    ui.waitReview(PlayerReviewUiProbe::owner(editor));
    require(ui.popupOpen()&&PlayerReviewUiProbe::owner(editor).prepared(),"Explicit source review did not open existing prepared review");
}
void contextCancelReview(HeadlessPanel &ui,EditorWindowController &editor,bool de)
{
    ui.compactClick(gm::localization::trUi("Cancel"));
    require(!ui.popupOpen()&&!PlayerReviewUiProbe::owner(editor).prepared(),"Cancelling all-staged review retained consent");
    PlayerReviewUiProbe::close(editor);ui.frame(3);
}
