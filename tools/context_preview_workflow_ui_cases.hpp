// These cases render actual source views and the real Editor review overlay.
// They never render Player Display or attach map/portrait/item image assets.
void contextItemDraftReview(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Item failed-save draft to contextual review 27px ")+(de?"DE":"EN"));
    SelectedPreviewFixture f;require(ProjectSaveService::getInstance().saveCampaign(f.campaign,"item contextual fixture"),"Cannot save context fixture");
    EditorWindowController editor;require(editor.setCurrentCampaign(f.campaign),"Cannot bind context fixture");
    require(editor.openWorkspaceEntity("Item",f.item),"Cannot select exact item through owning route");
    auto &item=PlayerReviewUiProbe::item(editor);auto &c=PlayerReviewUiProbe::campaign(editor);SessionEventCounter events(c.metadata.id);
    ui.draw=[&]{ContextPreviewUiProbe::draw(item);PlayerReviewUiProbe::draw(editor);};ui.frame(4);
    const auto route=PlayerReviewUiProbe::route(editor);const auto pane=ui.compactItem("##Name").window;
    ui.compactType("##Name","UNSAVED_CONTEXT_ITEM");
    {CompactSqlFault fault("reject_context_item","custom_items");ui.compactClick(gm::localization::trUi("Save Item"));}
    require(item.hasUnsavedDraft()&&item.getModel().name=="UNSAVED_CONTEXT_ITEM"&&
        contextSaved(menuReload(c.metadata.id).customItems,f.item).name=="Same named relic",
        "Failed item Save did not preserve exact draft and canonical baseline");
    const auto click=[&](const char *label){guidanceReachInObservedPane(ui,label,pane);ui.compactClick(label);};
    click(de?"Ausrüstung und Regeln":"Equipment and rules");
    click(gm::localization::trUi("Power List & Builder"));click(gm::localization::trUi("Add Item Power"));
    guidanceReachInObservedPane(ui,"##Power Name",pane);ui.compactType("##Power Name","UNFINISHED_CONTEXT_POWER");
    require(ContextPreviewUiProbe::powerDirty(item)&&ContextPreviewUiProbe::power(item).name=="UNFINISHED_CONTEXT_POWER",
        "Actual power input did not create unfinished authored power");
    guidanceReachInObservedPane(ui,de?"Spielerfelder ansehen":"Preview player fields",pane);
    contextOpenReview(ui,editor,de);auto &owner=PlayerReviewUiProbe::owner(editor);
    require(owner.review().playerNpcs.size()==1&&owner.review().playerNpcs[0].role=="CANONICAL_SELECTED_ROLE"&&
        owner.review().playerInventory.size()==3&&
        std::none_of(owner.review().playerInventory.begin(),owner.review().playerInventory.end(),[](const auto &row){return row.itemName=="UNSAVED_CONTEXT_ITEM";}),
        "All-staged review used private item draft as canonical public data");
    contextCancelReview(ui,editor,de);
    require(PlayerReviewUiProbe::route(editor)==route&&ContextPreviewUiProbe::item(item)==f.item&&
        item.getModel().name=="UNSAVED_CONTEXT_ITEM"&&item.hasUnsavedDraft()&&ContextPreviewUiProbe::powerDirty(item)&&
        ContextPreviewUiProbe::power(item).name=="UNFINISHED_CONTEXT_POWER"&&item.getModel().powers.empty()&&events.saves==0&&
        contextSaved(menuReload(c.metadata.id).customItems,f.item).name=="Same named relic"&&!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published,
        "Context preview/open/cancel navigated, saved, lost draft/power or changed publication");
    c.presentationModeActive=true;ui.frame(3);ui.absent("##Name");ui.absent("##Power Name");
    for(const char *text:{"UNSAVED_CONTEXT_ITEM","UNFINISHED_CONTEXT_POWER","PRIVATE_ITEM_DESCRIPTION"})
        require(!contains(ui.rendered,text),"Presentation mode submitted private item draft");
    c.presentationModeActive=false;ui.frame(3);
    require(item.getModel().name=="UNSAVED_CONTEXT_ITEM"&&ContextPreviewUiProbe::power(item).name=="UNFINISHED_CONTEXT_POWER",
        "Presentation return lost authored item/power");
}

void contextInventoryReview(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Inventory selected source review 27px ")+(de?"DE":"EN"));
    SelectedPreviewFixture f;f.campaign.customItems[0].name="INVENTORY_SOURCE_NAME";
    require(ProjectSaveService::getInstance().saveCampaign(f.campaign,"inventory context fixture"),"Cannot save inventory context");
    EditorWindowController editor;require(editor.setCurrentCampaign(f.campaign),"Cannot bind inventory context");
    auto &inventory=PlayerReviewUiProbe::inventory(editor);auto &c=PlayerReviewUiProbe::campaign(editor);
    require(inventory.focusItemForAssignment(f.item,f.owner),"Cannot select actual inventory item and owner");
    SessionEventCounter events(c.metadata.id);const auto selected=ContextPreviewUiProbe::inventory(inventory);
    const auto route=PlayerReviewUiProbe::route(editor);
    ui.draw=[&]{ContextPreviewUiProbe::draw(inventory);PlayerReviewUiProbe::draw(editor);};ui.frame(4);
    require(contains(ui.rendered,"INVENTORY_SOURCE_NAME")&&ui.hasControl(gm::localization::trUi("Quantity")),
        "Inventory source name/input positive control was not submitted");
    contextOpenReview(ui,editor,de);contextCancelReview(ui,editor,de);
    require(ContextPreviewUiProbe::inventory(inventory)==selected&&PlayerReviewUiProbe::route(editor)==route&&events.saves==0&&
        contextSaved(menuReload(c.metadata.id).characters,f.owner).inventory.entries.size()==2&&!c.localPlayerDisplay.enabled,
        "Inventory preview/review changed selection, quantities, route or persistence");
    c.presentationModeActive=true;ui.frame(3);ui.absent(de?"Spielerfelder ansehen":"Preview player fields");
    ui.absent(gm::localization::trUi("Quantity"));
    for(const char *text:{"INVENTORY_SOURCE_NAME","PRIVATE_ITEM_DESCRIPTION","PRIVATE_ENTRY_NOTES","PRIVATE_OWNER_GM_NOTES"})
        require(!contains(ui.rendered,text),"Presentation mode submitted private inventory source");
    c.presentationModeActive=false;ui.frame(3);
    require(ContextPreviewUiProbe::inventory(inventory)==selected,"Presentation return lost inventory owner/item selection");
}

void contextReviewRevocation(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Genuine contextual review envelope revocation 27px ")+(de?"DE":"EN"));
    SelectedPreviewFixture f;require(ProjectSaveService::getInstance().saveCampaign(f.campaign,"context revocation fixture"),"Cannot save envelope fixture");
    EditorWindowController editor;require(editor.setCurrentCampaign(f.campaign),"Cannot bind envelope fixture");
    auto &npc=PlayerReviewUiProbe::npc(editor);auto &c=PlayerReviewUiProbe::campaign(editor);auto &owner=PlayerReviewUiProbe::owner(editor);
    SessionEventCounter events(c.metadata.id);bool source=true,overlay=false;
    ui.draw=[&]{if(source)ContextPreviewUiProbe::draw(npc);if(overlay)PlayerReviewUiProbe::draw(editor);};
    const auto queue=[&]{overlay=false;source=true;require(npc.selectEntityById(f.npc,"NPC"),"Cannot select envelope source");ui.frame(3);
        ui.compactClick(de?"Spielerfelder ansehen":"Preview player fields");
        ui.compactClick(de?"Bereitgestellte Inhalte prüfen":"Review staged content");
        require(PlayerReviewUiProbe::contextualPending(editor)&&!owner.prepared()&&!ui.popupOpen(),"Actual source did not defer owned review until popup unwound");source=false;};
    const auto refused=[&]{overlay=true;ui.frame(3);
        require(!PlayerReviewUiProbe::contextualPending(editor)&&!owner.prepared()&&!ui.popupOpen()&&
            contains(ui.rendered,de?"Die Auswahl wurde geändert":"The selection changed"),"Stale contextual request lacked visible refusal or opened review");
        PlayerReviewUiProbe::close(editor);ui.frame(2);overlay=false;};
    queue();require(npc.selectEntityById(f.otherNpc,"NPC"),"Cannot change same-name source");refused();
    queue();{player_review_async_test::DataRoot root((ApplicationPaths::writableRoot()/"context-revocation-only").string());refused();}
    queue();gm::localization::LocalizationService::instance().setPreference(de?gm::localization::LocalePreference::English:gm::localization::LocalePreference::German,false);
    overlay=true;ui.frame(3);require(!PlayerReviewUiProbe::contextualPending(editor)&&!owner.prepared()&&!ui.popupOpen()&&
        contains(ui.rendered,de?"The selection changed":"Die Auswahl wurde geändert"),"Locale change adopted queued source review");
    PlayerReviewUiProbe::close(editor);ui.frame(2);overlay=false;
    gm::localization::LocalizationService::instance().setPreference(de?gm::localization::LocalePreference::German:gm::localization::LocalePreference::English,false);
    queue();c.presentationModeActive=true;overlay=true;ui.frame(3);c.presentationModeActive=false;ui.frame(3);
    require(!PlayerReviewUiProbe::contextualPending(editor)&&!owner.prepared()&&!ui.popupOpen(),"Privacy round trip resurrected queued review");
    PlayerReviewUiProbe::close(editor);ui.frame(2);
    queue();require(editor.setCurrentCampaign(f.campaign),"Same-ID replacement failed");overlay=true;ui.frame(3);
    require(!PlayerReviewUiProbe::contextualPending(editor)&&!owner.prepared()&&!ui.popupOpen(),"Same-ID replacement preserved queued source consent");
    // Fresh actual request succeeds once after the negative controls.
    PlayerReviewUiProbe::close(editor);ui.frame(2);queue();overlay=true;ui.frame(3);ui.waitReview(owner);
    require(owner.prepared()&&owner.review().playerNpcs.size()==1&&owner.review().playerNpcs[0].role=="CANONICAL_SELECTED_ROLE"&&
        !PlayerReviewUiProbe::contextualPending(editor),"Valid source request did not consume exactly once");
    const auto generation=owner.generation();ui.frame(4);require(owner.generation()==generation,"Consumed source request reopened review repeatedly");
    contextCancelReview(ui,editor,de);require(events.saves==0&&!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published,
        "Context request validation crossed a save or publication boundary");
}
