void guidanceNoAction(HeadlessPanel &ui,int depth=1)
{
    ui.compactClick("GuidanceDetails");guidanceBounds(ui,depth);
    ui.reviewTabTo("GuidanceClose");ui.reviewKey(ImGuiKey_Space);
}
void guidanceReachInObservedPane(HeadlessPanel &ui,const char *label,const std::string &pane)
{
    // A newly populated roster can push a table outside the scroll clip. This
    // pinned ImGui then skips the table's widgets, so there is no target rect
    // yet. Search only the exact pane observed from the original real control.
    for(float direction:{-1.f,1.f})for(int n=0;n<48;++n){
        const auto matches=ui.activeControls(label);
        if(matches.size()>1)ui.controlFailure("Ambiguous guidance workflow control",label,0,pane.c_str());
        auto *scroll=ImGui::FindWindowByName(pane.c_str());
        require(scroll&&scroll->Active&&scroll->InnerClipRect.GetHeight()>0,
            "Previously observed guidance workflow pane disappeared");
        if(matches.size()==1){auto *owner=ImGui::FindWindowByName(matches[0].window.c_str());
            while(owner&&owner!=scroll)owner=owner->ParentWindow;
            require(owner==scroll,"Reacquired guidance control belongs to another pane");
            ui.compactReach(label);return;}
        if(scroll->ScrollMax.y<=0)ui.controlFailure("Missing guidance control has no scrolling range",label,0,pane.c_str());
        auto &io=ImGui::GetIO();const auto p=scroll->InnerClipRect.GetCenter();
        io.AddMouseViewportEvent(scroll->ViewportId);io.AddMousePosEvent(p.x,p.y);ui.frame();
        const float before=scroll->Scroll.y;io.AddMouseWheelEvent(0,direction);ui.frame(3);
        if(scroll->Scroll.y==before)break;
    }
    ui.controlFailure("Queued wheel did not recover guidance workflow control",label,0,pane.c_str());
}
void guidanceOutcomeDraft(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Actual outcome guidance preserves draft 27px ")+(de?"DE":"EN"));
    auto c=fixture();SessionOutcomesPanel panel;int saves=0;const auto sid=c.sessions[0].metadata.id;
    const auto before=SessionOutcomes::read(c.sessions[0]);
    ui.draw=[&]{panel.drawLauncher(c,sid,[&](Campaign &){++saves;return false;});};ui.frame(3);
    ui.compactClick(de?"Ergebnisse und nächste Sitzung":"Outcomes and next session");
    ui.compactType("##outcome-private","GUIDANCE_RETAINED_PRIVATE_DRAFT");
    const auto input=ui.compactItem("##outcome-private").id;
    const auto key=SessionOutcomeDraftKeys::composer(SessionOutcomeDraftKeys::Surface::Preparation,sid);
    require(panel.checkpointDrafts(c),"Cannot checkpoint guidance outcome fixture");
    const auto saved=TextDraftRecovery::read(c,key);require(saved.success&&saved.found,"Guidance fixture lacks actual checkpoint");
    guidanceNoAction(ui,2);ui.compactReach("##outcome-private");
    const auto after=SessionOutcomes::read(c.sessions[0]);const auto retained=TextDraftRecovery::read(c,key);
    require(saves==0&&ui.compactItem("##outcome-private").id==input&&contains(ui.rendered,"GUIDANCE_RETAINED_PRIVATE_DRAFT")&&
        before.success&&after.success&&after.ledger.outcomes.size()==before.ledger.outcomes.size()&&
        retained.success&&retained.fields==saved.fields,"Outcome help altered draft, checkpoint, selection or committed outcomes");
    ui.compactClick(de?"Zurück zur Sitzung":"Return to session");require(!ui.popupOpen(),"Outcome parent no longer closes");
}
void guidanceParticipants(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Actual encounter participant guidance 27px ")+(de?"DE":"EN"));
    Campaign c;c.name="Guidance participants "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    for(int i=0;i<2;++i){CharacterModel a;std::snprintf(a.nameBuffer,sizeof(a.nameBuffer),"Guidance actor %d",i);
        a.maxHitPoints=a.currentHitPoints=12+i;c.characters.push_back(a);}
    EncounterModel e;e.name="Guidance narrative encounter";e.theaterOfMind=true;c.encounters.push_back(e);
    require(ProjectSaveService::getInstance().saveCampaign(c,"guidance participant fixture"),"Cannot save participant fixture");
    SessionEventCounter events(c.metadata.id);BattleTrackerController tracker(c,nullptr);
    require(tracker.selectEncounterById(e.metadata.id),"Cannot select guidance encounter");
    ui.draw=[&]{tracker.renderEmbedded();};ui.frame(4);
    const char *one=de?"Gewählte Figur hinzufügen":"Add selected character";
    const char *all=de?"Alle Kampagnenfiguren hinzufügen":"Add all campaign characters";
    const auto unchanged=[&]{require(BattleTrackerTurnsProbe::draft(tracker).participants.empty()&&
        menuReload(c.metadata.id).encounters[0].participants.empty()&&events.saves==0,
        "Opening participant guidance mutated encounter or saved campaign");};
    unchanged();const int selected=BattleTrackerTurnsProbe::inspected(tracker);guidanceNoAction(ui);unchanged();
    require(BattleTrackerTurnsProbe::inspected(tracker)==selected,"Participant help changed inspected combatant");
    ui.compactReach(one);const auto oneBefore=ui.compactItem(one),allBefore=ui.compactItem(all);
    ui.reviewVisible(oneBefore);ui.reviewVisible(allBefore);
    const auto pane=oneBefore.window;ui.compactClick(one);
    require(BattleTrackerTurnsProbe::draft(tracker).participants.size()==1&&
        BattleTrackerTurnsProbe::draft(tracker).participants[0].sourceEntityId==c.characters[0].metadata.id,
        "Add selected character does not match the visible first campaign character");
    const auto sameControl=[&](const char *label,const UiInteractionTrace::Item &before){
        const auto after=ui.compactItem(label);ui.reviewVisible(after);
        require(after.id==before.id&&after.window==before.window&&
            after.minimum.x==before.minimum.x&&after.minimum.y==before.minimum.y&&
            after.maximum.x==before.maximum.x&&after.maximum.y==before.maximum.y,
            "First participant addition moved/replaced the next setup action");};
    sameControl(one,oneBefore);sameControl(all,allBefore);
    ui.step("Reach Add all after actual roster growth");
    guidanceReachInObservedPane(ui,all,pane);ui.compactClick(all);
    guidanceReachInObservedPane(ui,all,pane);ui.compactClick(all);
    const auto &draft=BattleTrackerTurnsProbe::draft(tracker);
    require(draft.participants.size()==5,"Repeated Add all did not add both campaign characters on each press");
    int counts[2]={0,0};for(const auto &p:draft.participants)for(int i=0;i<2;++i)
        if(p.sourceEntityId==c.characters[i].metadata.id)++counts[i];
    require(counts[0]==3&&counts[1]==2&&events.saves==0&&menuReload(c.metadata.id).encounters[0].participants.empty(),
        "Participant addition changed source roster or crossed Save Setup without consent");
    guidanceReachInObservedPane(ui,gm::localization::trUi("Save Setup"),pane);
    ui.compactClick(gm::localization::trUi("Save Setup"));
    require(events.saves==1&&menuReload(c.metadata.id).encounters[0].participants.size()==5&&c.characters.size()==2,
        "Explicit Save Setup did not persist exactly the demonstrated five participants");
    c.ruleSet.reset();ui.frame(3);guidanceReachInObservedPane(ui,one,pane);ui.reviewState(one,true);
    guidanceReachInObservedPane(ui,all,pane);ui.reviewState(all,true);
    require(events.saves==1&&BattleTrackerTurnsProbe::draft(tracker).participants.size()==5,
        "Missing-ruleset prerequisite changed participants or saved again");
}
void guidanceEmbeddedLive(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Actual embedded Live phase guidance 27px ")+(de?"DE":"EN"));
    OutcomeSourceFixture fixture;auto &c=fixture.campaign;
    c.clock.useSystemTime=false;c.clock.running=false;c.clock.anchorAbsoluteMinutes=321;
    require(ProjectSaveService::getInstance().saveCampaign(c,"embedded guidance fixture"),"Cannot save embedded Live fixture");
    LiveSessionDashboardController live(c,nullptr);SessionEventCounter events(c.metadata.id);
    const auto before=c.sessions.front();const char *run=de?"Sitzung leiten":"Run session";
    ui.draw=[&]{live.renderEmbedded();};ui.frame(4);
    const auto tab=ui.compactItem(run).id;guidanceNoAction(ui);
    require(ui.compactItem(run).id==tab&&events.saves==0&&events.started==0&&events.ended==0,
        "Embedded Live help changed source tab or crossed a session persistence boundary");
    for(const auto &value:{c,menuReload(c.metadata.id)}){
        const auto *session=SessionPlan::findSession(value,fixture.sessionId);
        require(session&&session->liveState.phase==before.liveState.phase&&
            session->liveState.activePlanEntryId==before.liveState.activePlanEntryId&&
            session->startedAtUtc==before.startedAtUtc&&session->endedAtUtc==before.endedAtUtc&&
            session->liveModeActive==before.liveModeActive&&value.clock.anchorAbsoluteMinutes==321&&!value.clock.running,
            "Embedded Live guidance changed phase, active scene, timing or canonical session");
    }
}
