void sessionTargetCampaignReset(EditorWindowController &editor, Interaction &ui, bool german)
{
    Campaign c;c.name="Session reset "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    SessionModel prepared;prepared.title="RESET_REVIEW_PREPARED";c.sessions.push_back(prepared);c.sessionIds.push_back(prepared.metadata.id);
    SessionModel ended;ended.title="RESET_COMPLETED";ended.startedAtUtc="2026-01-01";ended.endedAtUtc="2026-01-02";ended.recap="RESET_PRIVATE_REPORT";
    c.sessions.push_back(ended);c.sessionIds.push_back(ended.metadata.id);
    require(ProjectSaveService::getInstance().saveCampaign(c,"reset reviewed session fixture"),"Could not save reviewed reset fixture");
    editor.setCurrentCampaign(c);editor.activateWorkspaceByIndex(6);ui.frame(3);
    // The existing Live tab bar preserves the last selected tool (the previous
    // recovery fixture used Outcomes). Navigate through its real Run tab.
    ui.click(german?"Sitzung leiten":"Run session");ui.frame(2);
    auto &live=WorkspaceInteractionProbe::liveSessions(editor);auto &gm=WorkspaceInteractionProbe::gmSessions(editor);
    SessionEventCounter events(c.metadata.id);
    for(int mode=0;mode<2;++mode) {
        ui.click(gm::localization::tr(gm::localization::TextId::LiveStartSession));
        require(SessionTargetProbe::start(live).valid,"Actual Live start opener failed before campaign reset");
        SessionTargetProbe::capture(gm);SessionTargetProbe::seedGm(gm);
        SessionTargetProbe::owner(live).report=SessionReport::review(WorkspaceInteractionProbe::liveCampaign(editor),ended.metadata.id);
        require(SessionTargetProbe::generate(live),"Could not generate reset report positive control");
        SessionTargetProbe::owner(live).report=SessionReport::review(WorkspaceInteractionProbe::liveCampaign(editor),ended.metadata.id);
        SessionTargetProbe::completed(live,ended.metadata.id);
        Campaign replacement=c; // Same logical ID and same stored session identities.
        if(mode){replacement.metadata.id=createEntityId("campaign");replacement.name="Different reset campaign "+replacement.metadata.id;}
        editor.setCurrentCampaign(replacement);
        SessionEventCounter replacementEvents(replacement.metadata.id);
        require(!SessionTargetProbe::start(live).valid&&SessionTargetProbe::gmReset(gm)&&
            !SessionTargetProbe::owner(live).report.valid&&!SessionTargetProbe::owner(live).output.valid&&
            SessionTargetProbe::owner(live).completedId.empty()&&SessionTargetProbe::output(live).empty()&&live.selectedPhase()==LiveSessionDashboardPhase::Prep,
            "Campaign replacement retained reviewed authorization, report, completed selection or phase");
        require(!SessionTargetProbe::confirm(live)&&!SessionTargetProbe::confirm(gm)&&!SessionTargetProbe::generate(live)&&
            !SessionControl::activeSession(WorkspaceInteractionProbe::liveCampaign(editor))&&events.saves==0&&events.started==0&&
            replacementEvents.saves==0&&replacementEvents.started==0,
            "Old same-ID/different-ID review remained authorized after real Editor replacement");
        editor.activateWorkspaceByIndex(6);ui.frame(3);
        require(ImGui::GetTopMostPopupModal()==nullptr,"Revoked start popup survived real Editor replacement");
        if(!mode){editor.setCurrentCampaign(c);editor.activateWorkspaceByIndex(6);ui.frame(3);}
    }
    const auto saved=readCampaign(c.metadata.id);
    require(!SessionControl::activeSession(saved)&&SessionPlan::findSession(saved,ended.metadata.id)->recap=="RESET_PRIVATE_REPORT",
        "Reset authorization check mutated durable session data");
}
