// Real Editor update consumes genuine carry intents after drawing. The test
// sink captures an actual click only for the between-queue/consume fault cases.
void sessionPreparationNavigation(EditorWindowController &editor,Interaction &ui,bool de)
{
    std::cout<<"UI group: Deferred exact carry destination "<<(de?"DE":"EN")<<'\n';
    Campaign c;c.name="Deferred carry "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    SessionModel source;source.title="SOURCE_PRIVATE_SESSION";source.liveModeActive=true;source.startedAtUtc=currentUtcTimestamp();
    source.liveState.initialized=true;source.liveState.phase=SessionPhase::Scene;
    SessionOutcomes::Outcome outcome;outcome.privateText="EXACT_PRIVATE_FOLLOW_UP";
    SessionOutcomes::Ledger ledger;ledger.outcomes.push_back(outcome);SessionOutcomes::write(source,ledger);
    SessionModel decoy;decoy.title="DEFERRED_NAMED_DESTINATION";
    c.sessions={source,decoy};c.sessionIds={source.metadata.id,decoy.metadata.id};
    require(ProjectSaveService::getInstance().saveCampaign(c,"deferred carry fixture"),"Cannot save deferred carry fixture");
    require(editor.setCurrentCampaign(c),"Cannot bind deferred fixture");editor.activateWorkspaceByIndex(6);ui.frame(3);
    auto &live=WorkspaceInteractionProbe::liveSessions(editor);SessionEventCounter events(c.metadata.id);
    std::optional<PreparationTargetRequest> emitted;int emissions=0;bool duringDraw=false;
    live.setPreparationTargetHandler([&](PreparationTargetRequest request){++emissions;emitted=std::move(request);
        duringDraw=ImGui::GetCurrentContext()->WithinFrameScope&&ImGui::GetCurrentContext()->CurrentWindow!=nullptr;});
    const char *outcomes=de?"Ergebnisse und Abschluss":"Outcomes and wrap-up";
    const char *next=de?"Nächste Sitzung":"Next session",*open=de?"Diese Sitzung vorbereiten":"Prepare this session";
    ui.click(outcomes);ui.click(next);
    const auto pane=ui.observedScrollWindow("##carry-select");ui.click("##carry-select",pane.c_str());
    ui.replaceText("##next-title","DEFERRED_NAMED_DESTINATION",pane.c_str());
    ui.click(de?"Ausgewähltes Material übernehmen":"Carry selected material forward",pane.c_str());
    require(events.saves==1&&events.started==0&&editor.currentCampaign().sessions.size()==3&&
        WorkspaceInteractionProbe::workspace(editor)==gm::ux::WorkspaceTarget::Live&&!emitted,
        "Carry implicitly navigated/started or created wrong session count");
    ui.click(open,pane.c_str());require(emitted&&emissions==1&&duringDraw&&!WorkspaceInteractionProbe::preparationQueued(editor),
        "Actual Prepare click did not produce one owned deferred intent");
    const auto request=*emitted;const auto baseline=editor.currentCampaign();const auto target=request.targetSessionId;
    require(target!=source.metadata.id&&target!=decoy.metadata.id&&live.matchesPreparationTarget(request)&&
        SessionPlan::findSession(baseline,target)->title==decoy.title,"Prepare intent did not identify newly carried same-title destination");
    const auto selection=WorkspaceInteractionProbe::preparation(editor).sessionId();
    const auto unchanged=[&]{require(events.saves==1&&events.started==0&&events.ended==0&&
        WorkspaceInteractionProbe::workspace(editor)==gm::ux::WorkspaceTarget::Live&&
        WorkspaceInteractionProbe::preparation(editor).sessionId()==selection&&!WorkspaceInteractionProbe::preparationQueued(editor),
        "Refused deferred intent changed source navigation, selection, SQL or session lifecycle");};
    // Existing genuine receipt, exact producer scope, then external model drift.
    for(int mutation=0;mutation<7;++mutation){
        auto &current=WorkspaceInteractionProbe::liveCampaign(editor);current=baseline;
        editor.queuePreparationTarget(request);require(WorkspaceInteractionProbe::preparationQueued(editor),"Intent did not queue");
        auto *value=SessionPlan::findSession(current,target);
        if(mutation==0)current.sessions.erase(std::remove_if(current.sessions.begin(),current.sessions.end(),[&](const auto &s){return s.metadata.id==target;}),current.sessions.end());
        if(mutation==1)value->metadata.tags.push_back("archived");
        if(mutation==2)value->startedAtUtc="2026-01-01";
        if(mutation==3)current.sessions.push_back(*value);
        if(mutation==4)SessionPlan::findSession(current,source.metadata.id)->metadata.tags.push_back("archived");
        if(mutation==5)current.sessions.push_back(*SessionPlan::findSession(current,source.metadata.id));
        if(mutation==6)current.presentationModeActive=true;
        WorkspaceInteractionProbe::processPreparation(editor);unchanged();WorkspaceInteractionProbe::processPreparation(editor);unchanged();
    }
    WorkspaceInteractionProbe::liveCampaign(editor)=baseline;
    // Root binding is compared at consumption, with no writes under the changed root.
    const auto previousRoot=ApplicationPaths::writableRoot().string();
    const auto otherRoot=(ApplicationPaths::databasePath().parent_path()/"unselected-preparation-root").string();
    editor.queuePreparationTarget(request);
#ifdef _WIN32
    require(_putenv_s("TTRPG_DATA_ROOT",otherRoot.c_str())==0,"Cannot change isolated root fixture");
#else
    require(setenv("TTRPG_DATA_ROOT",otherRoot.c_str(),1)==0,"Cannot change isolated root fixture");
#endif
    WorkspaceInteractionProbe::processPreparation(editor);
#ifdef _WIN32
    require(_putenv_s("TTRPG_DATA_ROOT",previousRoot.c_str())==0,"Cannot restore isolated root fixture");
#else
    require(setenv("TTRPG_DATA_ROOT",previousRoot.c_str(),1)==0,"Cannot restore isolated root fixture");
#endif
    unchanged();require(!std::filesystem::exists(otherRoot),"Revoked root intent created files outside bound recovery root");
    // Pending disk recovery must be noticed although Prepare has never drawn.
    const auto &current=editor.currentCampaign();
    require(TextDraftRecovery::save(current,"session-workspace",{{"retained","UNREVIEWED_PRIVATE_RECOVERY"}}).empty(),"Cannot write isolated pending recovery");
    // Earlier unavailable targets stop before recovery read; use a fresh exact
    // session selector to validate this contract independently of cached reads.
    SessionWorkspace neverDrawn;
    require(neverDrawn.openPreparationTarget(WorkspaceInteractionProbe::liveCampaign(editor),target).status==PreparationTargetStatus::NeedsReview&&
        neverDrawn.sessionId().empty(),"Never-drawn preparation skipped pending recovery");
    require(TextDraftRecovery::remove(current,"session-workspace").empty(),"Cannot remove own pending recovery fixture");
    // Restore normal root wiring and let the real update (not the probe) consume.
    live.setPreparationTargetHandler([&editor](PreparationTargetRequest value){editor.queuePreparationTarget(std::move(value));});
    ui.frame(3);ui.click(open,pane.c_str());ui.frame(3);
    require(WorkspaceInteractionProbe::workspace(editor)==gm::ux::WorkspaceTarget::Home&&
        WorkspaceInteractionProbe::preparation(editor).sessionId()==target&&events.saves==1&&events.started==0&&
        !WorkspaceInteractionProbe::preparationQueued(editor)&&ImGui::GetCurrentContext()->BeginPopupStack.empty(),
        "Actual Editor post-draw handoff did not open exact Prepare target without committing/starting");
    const auto disk=readCampaign(c.metadata.id);const auto *destination=SessionPlan::findSession(disk,target);
    require(destination&&destination->startedAtUtc.empty()&&!destination->liveModeActive&&
        SessionOutcomes::read(*destination).ledger.followUps.size()==1&&
        SessionOutcomes::read(*destination).ledger.followUps[0].privateText==outcome.privateText,
        "Deferred navigation duplicated or altered the durable carry result");
    // A real false->true->false privacy lifecycle revokes even if no draw occurs
    // between toggles. Its deliberate settings saves are counted separately.
    editor.activateWorkspaceByIndex(6);ui.frame(3);editor.queuePreparationTarget(request);
    live.setPresentationModeActive(true);live.setPresentationModeActive(false);
    const auto privacySaves=events.saves;const auto privacySelection=WorkspaceInteractionProbe::preparation(editor).sessionId();
    WorkspaceInteractionProbe::processPreparation(editor);
    require(!live.matchesPreparationTarget(request)&&!WorkspaceInteractionProbe::preparationQueued(editor)&&
        WorkspaceInteractionProbe::workspace(editor)==gm::ux::WorkspaceTarget::Live&&
        WorkspaceInteractionProbe::preparation(editor).sessionId()==privacySelection&&events.saves==privacySaves&&events.started==0,
        "Live privacy round trip resurrected queued private preparation navigation");
    // Same-ID replacement invalidates the old producer receipt and queued epoch.
    editor.activateWorkspaceByIndex(6);ui.frame(3);editor.queuePreparationTarget(request);
    require(editor.setCurrentCampaign(disk),"Same-ID replacement failed");
    const auto resetSelection=WorkspaceInteractionProbe::preparation(editor).sessionId();
    WorkspaceInteractionProbe::processPreparation(editor);
    require(!live.matchesPreparationTarget(request)&&!WorkspaceInteractionProbe::preparationQueued(editor)&&
        WorkspaceInteractionProbe::preparation(editor).sessionId()==resetSelection&&events.saves==privacySaves&&events.started==0,
        "Same-ID reset retained old carry authorization or replayed navigation");
}
