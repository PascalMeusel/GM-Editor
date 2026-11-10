// Actual embedded Live and preparation widgets; CPU ImGui, null playback and
// no map/handout assets. Tab assertions inspect the submitted production body.
void sessionRunTabNavigation(bool de)
{
    HeadlessPanel ui(de);ui.context(std::string("One-shot Live Run navigation ")+(de?"DE":"EN"));
    OutcomeSourceFixture f;auto &c=f.campaign;
    require(ProjectSaveService::getInstance().saveCampaign(c,"Run tab fixture"),"Cannot save Run tab fixture");
    LiveSessionDashboardController live(c,nullptr);SessionWorkspace prep;SessionEventCounter events(c.metadata.id);
    SessionWorkspace::Actions actions;bool preparing=false;
    actions.persist=[](Campaign &value){return ProjectSaveService::getInstance().saveCampaign(value,"Run tab preparation");};
    actions.run=[&]{live.requestRunSessionTab();preparing=false;};
    actions.started=[&](const EntityId &id,const EntityId &scene){live.afterSessionStarted(id,scene);};
    ui.draw=[&]{if(preparing)prep.draw(c,actions);else{SessionTargetProbe::phases(live);live.renderEmbedded();}};
    ui.frame(4);const char *outcomes=de?"Ergebnisse und Abschluss":"Outcomes and wrap-up";
    const auto requireOutcomes=[&]{require(ui.hasControl("##outcome-private"),"Live visit lost chosen Outcomes tab");};
    const auto requireRun=[&]{require(!ui.hasControl("##outcome-private")&&ui.hasControl(de?"Sitzungsübersicht":"Session overview"),
        "Explicit successful transition did not select Run session");};
    ui.compactClick(outcomes);requireOutcomes();
    preparing=true;ui.frame(3);preparing=false;ui.frame(3);requireOutcomes();
    require(events.saves==0&&events.started==0,"Ordinary Live visit persisted or started a session");
    preparing=true;ui.frame(3);ui.compactClick(de?"Live fortsetzen":"Resume live");requireRun();
    require(events.saves==0&&events.started==0,"Resume live duplicated a start or save");
    ui.compactClick(outcomes);ui.frame(4);requireOutcomes();
    SessionSqlFailure failure;failure.arm();
    ui.compactClick(gm::localization::tr(gm::localization::TextId::LiveExploration));requireOutcomes();
    require(events.saves==0&&c.sessions[0].liveState.phase==SessionPhase::Scene&&
        menuReload(c.metadata.id).sessions[0].liveState.phase==SessionPhase::Scene,"Failed phase persisted or changed phase");
    failure.clear();ui.compactClick(gm::localization::tr(gm::localization::TextId::LiveExploration));requireRun();
    require(events.saves==1&&c.sessions[0].liveState.phase==SessionPhase::Exploration,"Phase retry did not commit exactly once");
    ui.compactClick(outcomes);ui.frame(3);requireOutcomes();
    live.startSessionBreak(false);ui.frame(3);requireRun();
    require(c.sessions[0].liveState.phase==SessionPhase::Break&&events.saves==2,"Break did not commit exactly once");
    ui.compactClick(outcomes);live.resumeSessionFromBreak(false);ui.frame(3);requireRun();
    require(c.sessions[0].liveState.phase==SessionPhase::Exploration&&events.saves==3,"Resume did not preserve phase or commit once");
    ui.compactClick(outcomes);ui.frame(4);requireOutcomes();
    require(events.started==0&&events.ended==0,"Phase/break navigation started or ended the session");
}

void sessionPreparedRun(bool de)
{
    HeadlessPanel ui(de);ui.context(std::string("Prepared Run SQL failure and exact successful start ")+(de?"DE":"EN"));
    auto c=reviewedUiCampaign(true);const auto id=c.sessions[0].metadata.id;
    LiveSessionDashboardController live(c,nullptr);SessionWorkspace prep;SessionEventCounter events(c.metadata.id);
    bool inPrepare=false;SessionWorkspace::Actions actions;
    actions.persist=[](Campaign &value){return ProjectSaveService::getInstance().saveCampaign(value,"explicit prepared Run");};
    actions.started=[&](const EntityId &sid,const EntityId &scene){live.afterSessionStarted(sid,scene);};
    actions.run=[&]{live.requestRunSessionTab();inPrepare=false;};
    ui.draw=[&]{if(inPrepare)prep.draw(c,actions);else live.renderEmbedded();};ui.frame(3);
    ui.compactClick(de?"Ergebnisse und Abschluss":"Outcomes and wrap-up");
    inPrepare=true;ui.frame(3);SessionSqlFailure failure;failure.arm();
    ui.compactClick(de?"Sitzung starten":"Run session");
    require(inPrepare&&events.saves==0&&events.started==0&&!SessionControl::activeSession(c),"Failed Run changed route or started session");
    failure.clear();ui.compactClick(de?"Sitzung starten":"Run session");ui.frame(3);
    const auto disk=menuReload(c.metadata.id);const auto *saved=SessionPlan::findSession(disk,id);
    require(!inPrepare&&!ui.hasControl("##outcome-private")&&ui.hasControl(de?"Sitzungsübersicht":"Session overview")&&
        events.saves==1&&events.started==1&&saved&&saved->liveModeActive&&saved->title=="EXACT_PREPARED_PRIVATE_TITLE"&&disk.sessions.size()==1,
        "Successful prepared Run failed to select Run or started a different/duplicate session");
    ui.compactClick(de?"Ergebnisse und Abschluss":"Outcomes and wrap-up");ui.frame(3);
    require(ui.hasControl("##outcome-private"),"Successful Run request remained latched after deliberate tab choice");
}

void sessionStrictPreparationTarget(bool de)
{
    HeadlessPanel ui(de);ui.context(std::string("Exact preparation target and retained drafts ")+(de?"DE":"EN"));
    auto c=fixture();c.ruleSet=std::make_shared<DND5e>();SessionModel target;target.title="SAME_TITLE_TARGET";
    c.sessions[0].title=target.title;c.sessions[0].scheduledAtUtc="2030-01-01";
    c.sessions.push_back(target);c.sessionIds.push_back(target.metadata.id);
    const auto source=c.sessions[0].metadata.id,destination=target.metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"strict target fixture"),"Cannot save strict target fixture");
    SessionWorkspace prep;SessionWorkspace::Actions actions;SessionEventCounter events(c.metadata.id);
    actions.persist=[](Campaign &value){return ProjectSaveService::getInstance().saveCampaign(value,"explicit preparation save");};
    ui.draw=[&]{prep.draw(c,actions);};ui.frame(3);require(prep.sessionId()==source,"Fixture did not open its source");
    ui.compactType("##rough-notes","UNSAVED_EXACT_PREPARATION");
    require(prep.openPreparationTarget(c,source).opened()&&prep.dirty(),"Same-target focus replaced dirty preparation");
    ui.frame(2);require(contains(ui.rendered,"UNSAVED_EXACT_PREPARATION"),"Same-target focus lost actual typed text");
    require(prep.openPreparationTarget(c,destination).status==PreparationTargetStatus::NeedsReview&&prep.sessionId()==source&&events.saves==0,
        "Different target implicitly committed or discarded preparation");
    SessionSqlFailure failure;failure.arm();ui.compactClick(de?"Speichern":"Save");
    require(prep.dirty()&&prep.openPreparationTarget(c,destination).status==PreparationTargetStatus::NeedsReview&&events.saves==0,
        "Failed explicit preparation save enabled navigation");
    failure.clear();ui.compactClick(de?"Erneut speichern":"Retry save");
    require(!prep.dirty()&&events.saves==1&&SessionPlan::findSession(menuReload(c.metadata.id),source)->roughNotes=="UNSAVED_EXACT_PREPARATION",
        "Explicit preparation retry did not save retained text exactly once");
    std::reverse(c.sessions.begin(),c.sessions.end());require(prep.openPreparationTarget(c,destination).opened()&&prep.sessionId()==destination,
        "Named destination used vector position or duplicate title instead of stable ID");ui.frame(3);
    ui.compactClick(de?"Neue Sitzung":"New session");ui.compactType("##NewSessionTitle","UNFINISHED_NEW_SESSION");ui.compactClick(de?"Abbrechen":"Cancel");
    require(prep.openPreparationTarget(c,source).status==PreparationTargetStatus::NeedsReview&&prep.sessionId()==destination&&events.saves==1,
        "Unsubmitted new-session title was dropped or implicitly created");
    // Fresh strict selectors reject unavailable/ambiguous targets without fallback.
    for(int mutation=0;mutation<5;++mutation){auto changed=c;SessionWorkspace fresh;
        auto *value=SessionPlan::findSession(changed,source);
        if(mutation==0)changed.sessions.erase(std::remove_if(changed.sessions.begin(),changed.sessions.end(),[&](const auto &s){return s.metadata.id==source;}),changed.sessions.end());
        if(mutation==1)value->metadata.tags.push_back("archived");
        if(mutation==2)value->startedAtUtc="2026-01-01";
        if(mutation==3)changed.sessions.push_back(*value);
        if(mutation==4)value->liveModeActive=true;
        require(fresh.openPreparationTarget(changed,source).status==PreparationTargetStatus::Unavailable&&fresh.sessionId().empty(),
            "Unavailable/duplicate target selected another session");}
    // Real preparation checkpoint, read before the new selector has drawn.
    require(prep.flush(c,actions.persist),"Cannot checkpoint unfinished creation form");
    const auto recovery=TextDraftRecovery::read(c,"session-workspace");require(recovery.success&&recovery.found,"Creation recovery positive control missing");
    SessionWorkspace reopened;require(reopened.openPreparationTarget(c,source).status==PreparationTargetStatus::NeedsReview&&reopened.sessionId().empty(),
        "Never-drawn preparation ignored unresolved recovery");
    require(TextDraftRecovery::read(c,"session-workspace").fields==recovery.fields&&events.saves==1,"Target inspection altered recovery or SQL");
    require(TextDraftRecovery::remove(c,"session-workspace").empty(),"Cannot remove own fixture checkpoint");
}
