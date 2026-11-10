// Reads the actual NPC view with no portrait metadata or image attachments.
// Read/reveal are UI state only; real SQL and publication flags remain unchanged.
void npcReadWorkflow(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("NPC Read and deliberate secret reveal 27px ")+(de?"DE":"EN"));
    SelectedPreviewFixture f;require(ProjectSaveService::getInstance().saveCampaign(f.campaign,"NPC Read fixture"),"Cannot save NPC Read fixture");
    EditorWindowController editor;require(editor.setCurrentCampaign(f.campaign),"Cannot bind NPC Read campaign");
    auto &c=PlayerReviewUiProbe::campaign(editor);auto &npc=PlayerReviewUiProbe::npc(editor);SessionEventCounter events(c.metadata.id);
    require(npc.selectEntityById(f.npc,"NPC"),"Cannot select exact NPC");
    ui.draw=[&]{ContextPreviewUiProbe::draw(npc);};ui.frame(3);
    const char *read=de?"NSC lesen":"Read NPC",*reveal=de?"Geheimnisse anzeigen":"Reveal secrets";
    const char *hide=de?"Geheimnisse verbergen":"Hide secrets";
    ui.compactClick(read);
    for(const char *text:{"CANONICAL_SELECTED_ROLE","PRIVATE_NPC_DESCRIPTION","PRIVATE_NPC_DIALOGUE","PRIVATE_NPC_GOAL"})
        require(contains(ui.rendered,text),"NPC Read omitted populated authored field");
    require(!contains(ui.rendered,"PRIVATE_NPC_SECRET")&&!ui.hasControl("##Name"),"Read exposed secrets or mutable profile input by default");
    ui.compactClick(reveal);require(contains(ui.rendered,"PRIVATE_NPC_SECRET"),"Explicit secret reveal did not render selected secret");
    ui.compactClick(hide);require(!contains(ui.rendered,"PRIVATE_NPC_SECRET"),"Hide secrets left secret submitted");
    ui.compactClick(reveal);
    // Select another same-name source through the actual compact NPC picker.
    const auto tab=ui.controlId(gm::localization::trUi("NPCs"));
    ui.compactClick("##NpcQuickSwitch",ImHashStr("##NpcQuickSwitch",0,tab));
    ui.compactClick((c.npcs[1].name+"##"+f.otherNpc).c_str(),0,true);
    require(ContextPreviewUiProbe::npc(npc)==f.otherNpc&&!contains(ui.rendered,"PRIVATE_NPC_SECRET")&&!contains(ui.rendered,"OTHER_NPC_SECRET"),
        "Switching same-name NPC inherited another source's secret reveal");
    require(npc.selectEntityById(f.npc,"NPC"),"Cannot restore original NPC");ui.frame(3);
    if(ui.hasControl(read))ui.compactClick(read);ui.compactClick(reveal);
    std::reverse(c.npcs.begin(),c.npcs.end());ui.frame(3);
    require(ContextPreviewUiProbe::npc(npc)==f.otherNpc&&!contains(ui.rendered,"OTHER_NPC_SECRET"),
        "NPC reorder transferred revealed secrets by vector position");
    require(npc.selectEntityById(f.npc,"NPC"),"Cannot select reordered NPC");ui.frame(3);
    if(ui.hasControl(read))ui.compactClick(read);ui.compactClick(reveal);
    require(editor.setCurrentCampaign(f.campaign),"Same-ID NPC context replacement failed");ui.frame(3);
    require(!contains(ui.rendered,"PRIVATE_NPC_SECRET")&&!ui.hasControl(hide),"Same-ID campaign reset retained secret reveal");
    if(ui.hasControl(read))ui.compactClick(read);ui.compactClick(reveal);
    c.presentationModeActive=true;ui.frame(3);
    for(const char *text:{"PRIVATE_NPC_SECRET","PRIVATE_NPC_DESCRIPTION","PRIVATE_NPC_GOAL","PRIVATE_NPC_DIALOGUE"})
        require(!contains(ui.rendered,text),"Presentation gate submitted private NPC read text");
    ui.absent(read);ui.absent(reveal);c.presentationModeActive=false;ui.frame(3);
    require(!contains(ui.rendered,"PRIVATE_NPC_SECRET")&&!ui.hasControl(hide),"Returning from presentation restored previous reveal");
    const auto disk=menuReload(c.metadata.id);
    const auto &saved=contextSaved(disk.npcs,f.npc);
    require(events.saves==0&&events.started==0&&saved.secrets=="PRIVATE_NPC_SECRET"&&
        saved.description=="PRIVATE_NPC_DESCRIPTION"&&!c.localPlayerDisplay.enabled&&!c.localPlayerDisplay.published,
        "Read/reveal/navigation implicitly saved, edited or published the NPC");
}
