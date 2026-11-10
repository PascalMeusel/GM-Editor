Campaign completedUiCampaign()
{
    auto c=reviewedUiCampaign(false);
    for(int i=0;i<3;++i) {
        SessionModel s;s.title=i==0?"OLDER_PRIVATE_RECAP":i==1?"LATEST_PRIVATE_RECAP":"FUTURE_PREPARED_CANARY";
        if(i<2){s.startedAtUtc="2026-01-01T10:00:00Z";s.endedAtUtc=i==0?"2026-01-01T14:00:00Z":"2026-02-01T14:00:00Z";
            s.recap=i==0?"OLDER_RECAP_TEXT":"LATEST_RECAP_TEXT";s.unresolvedThreads="SAVED_PRIVATE_THREADS";s.lootAwarded="SAVED_PRIVATE_REWARDS";}
        c.sessionIds.push_back(s.metadata.id);c.sessions.push_back(s);
    }
    require(ProjectSaveService::getInstance().saveCampaign(c,"completed recap fixture"),"Cannot save completed recap fixture");return c;
}
void completedReportUi(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);auto c=completedUiCampaign();
    ui.context(std::string("Completed report")+(de?" DE":" EN")+(compact?" compact":" normal"));
    const auto older=c.sessions[0].metadata.id,latest=c.sessions[1].metadata.id;
    const auto before=c.metadata.updatedAtUtc;const auto savedAt=ProjectSaveService::getInstance().state().lastSavedAtUtc;
    LiveSessionDashboardController live(c,nullptr);SessionEventCounter events(c.metadata.id);
    ui.draw=[&]{live.renderEmbedded();};ui.frame(3);
    ui.clickVisible(de?"Abgeschlossenen Rückblick ansehen":"View completed recap");ui.frame(4);
    require(live.selectedPhase()==LiveSessionDashboardPhase::WrapUp&&SessionTargetProbe::owner(live).completedId==latest&&
        contains(ui.rendered,"LATEST_RECAP_TEXT"),"Visible Prep action did not retain the latest completed recap in WrapUp");
    ui.absent("###LiveRecap");ui.absent(gm::localization::tr(gm::localization::TextId::LiveSaveRecap));
    ui.clickVisible(de?"Abgeschlossene Sitzung wählen":"Choose completed session");ui.clickVisible("OLDER_PRIVATE_RECAP");ui.frame(3);
    require(SessionTargetProbe::owner(live).completedId==older&&contains(ui.rendered,"OLDER_RECAP_TEXT")&&contains(ui.rendered,"SAVED_PRIVATE_REWARDS"),
        "Explicit completed recap lost saved private readback");
    ui.clickVisible(de?"Ausführlicher Bericht":"Detailed report");
    const auto generate=gm::localization::tr(gm::localization::TextId::LiveGenerateReport);
    const auto confirm=gm::localization::tr(gm::localization::TextId::LiveGenerate);
    const auto cancel=gm::localization::tr(gm::localization::TextId::CommonCancel);
    ui.clickVisible(generate);ui.boundedPopup(confirm,false);
    require(SessionTargetProbe::owner(live).report.sessionId==older&&contains(ui.rendered,"OLDER_PRIVATE_RECAP"),"Report modal target differs from completed selection");
    ui.clickVisible(confirm);
    require(SessionTargetProbe::owner(live).output.sessionId==older&&contains(SessionTargetProbe::output(live),"# OLDER_PRIVATE_RECAP")&&
        contains(SessionTargetProbe::output(live),"OLDER_RECAP_TEXT")&&!contains(SessionTargetProbe::output(live),"LATEST_RECAP_TEXT")&&
        contains(SessionTargetProbe::output(live),de?"Kampagnenzeitverlauf":"Campaign timeline"),"Report output uses wrong session/timeline scope");
    const auto output=SessionTargetProbe::output(live);
    ui.clickVisible(de?"Abgeschlossene Sitzung wählen":"Choose completed session");ui.clickVisible("LATEST_PRIVATE_RECAP");
    require(SessionTargetProbe::owner(live).output.sessionId==older&&SessionTargetProbe::output(live)==output,"Selection relabelled or replaced previous report");
    ui.clickVisible(generate);ui.clickVisible(cancel);require(SessionTargetProbe::output(live)==output,"Cancelled report overwrote prior output");
    ui.step("Stale report review");ui.clickVisible(generate);SessionPlan::findSession(c,latest)->title="RENAMED_LATEST";ui.clickVisible(confirm);
    require(ui.popupOpen()&&SessionTargetProbe::output(live)==output&&SessionTargetProbe::owner(live).report.sessionId==latest&&
        contains(ui.rendered,de?"hat sich geändert":"reviewed session changed"),"Stale report silently switched target or hid failure");
    ui.clickVisible(cancel);SessionPlan::findSession(c,latest)->title="LATEST_PRIVATE_RECAP";
    c.presentationModeActive=true;ui.frame(3);
    for(const auto *text:{"OLDER_PRIVATE_RECAP","LATEST_PRIVATE_RECAP","OLDER_RECAP_TEXT","LATEST_RECAP_TEXT","SAVED_PRIVATE_THREADS","SAVED_PRIVATE_REWARDS"})
        require(!contains(ui.rendered,text),"Completed/report private canary crossed parent presentation gate");
    c.presentationModeActive=false;ui.frame(3);
    require(SessionTargetProbe::owner(live).completedId==latest&&SessionTargetProbe::output(live)==output,"Presentation toggle lost completed selection/output");
    const auto removed=*SessionPlan::findSession(c,latest);
    c.sessions.erase(std::remove_if(c.sessions.begin(),c.sessions.end(),[&](const auto &s){return s.metadata.id==latest;}),c.sessions.end());ui.frame(3);
    require(SessionTargetProbe::owner(live).completedId==latest&&!SessionReport::resolve(c,latest)&&
        contains(ui.rendered,de?"Keine verfügbare Sitzung ausgewählt":"No available session selected")&&SessionTargetProbe::output(live)==output,
        "Missing explicit completed selection silently fell through or destroyed owned output");
    c.sessions.push_back(removed);ui.frame(3);
    require(events.saves==0&&events.started==0&&events.ended==0&&c.metadata.updatedAtUtc==before&&
        ProjectSaveService::getInstance().state().lastSavedAtUtc==savedAt&&!TextDraftRecovery::read(c,"live-dashboard").found,
        "Read-only completed viewing/report wrote campaign or recovery");
    const auto disk=menuReload(c.metadata.id);
    require(disk.sessions.size()==3&&SessionPlan::findSession(disk,latest)->recap=="LATEST_RECAP_TEXT"&&
        SessionPlan::findSession(disk,older)->recap=="OLDER_RECAP_TEXT"&&disk.metadata.updatedAtUtc==before,"Read-only report changed durable session data");
}

void finishAndHistoryUi(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);auto c=completedUiCampaign();
    ui.context(std::string("Finish/history phase")+(de?" DE":" EN")+(compact?" compact":" normal"));
    const auto activeId=c.sessions.back().metadata.id,oldId=c.sessions.front().metadata.id;
    c.sessions.back().startedAtUtc="2026-10-04T10:00:00Z";c.sessions.back().liveModeActive=true;c.sessions.back().liveState.initialized=true;
    c.sessions.back().liveState.phase=SessionPhase::Setup;
    require(ProjectSaveService::getInstance().saveCampaign(c,"active recap fixture"),"Cannot save active recap fixture");
    LiveSessionDashboardController live(c,nullptr);SessionEventCounter events(c.metadata.id);SessionSqlFailure failure;
    ui.draw=[&]{live.renderEmbedded();};ui.frame(3);ui.clickVisible(de?"Abgeschlossenen Rückblick ansehen":"View completed recap");ui.frame(3);
    require(live.selectedPhase()==LiveSessionDashboardPhase::WrapUp&&SessionPlan::findSession(c,activeId)->liveState.phase==SessionPhase::Setup&&events.saves==0,
        "Viewing completed history persisted a phase change to active Setup");
    ui.draw=[&]{SessionTargetProbe::phases(live);};ui.frame(3);
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveUseCurrent));
    require(SessionTargetProbe::owner(live).completedId.empty()&&live.selectedPhase()==LiveSessionDashboardPhase::Prep&&events.saves==0&&
        SessionPlan::findSession(c,activeId)->liveState.phase==SessionPhase::Setup,"Top Use current persisted a phase or retained historical target");
    ui.draw=[&]{live.renderEmbedded();};ui.frame(3);ui.clickVisible(de?"Abgeschlossenen Rückblick ansehen":"View completed recap");
    ui.clickVisible(de?"Aktuelle verwenden":"Use current");
    require(SessionTargetProbe::owner(live).completedId.empty()&&live.selectedPhase()==LiveSessionDashboardPhase::Prep&&events.saves==0,
        "Completed recap Use current changed canonical phase or retained selection");
    ui.draw=[&]{SessionTargetProbe::recap(live);};ui.frame(3);
    ui.typeVisible("###LiveRecap","EXACT_TYPED_ACTIVE_RECAP");
    ui.clickVisible(de?"Ausführlicher Bericht":"Detailed report");
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveGenerateReport));
    require(SessionTargetProbe::owner(live).report.sessionId==activeId&&!SessionTargetProbe::owner(live).report.completed,
        "Automatic report did not capture current active session");
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveGenerate));
    require(SessionTargetProbe::owner(live).output.sessionId==activeId&&!contains(SessionTargetProbe::output(live),"EXACT_TYPED_ACTIVE_RECAP")&&
        SessionTargetProbe::recapDraft(live)=="EXACT_TYPED_ACTIVE_RECAP"&&events.saves==0,
        "Saved-state report flushed or included unsaved active recap");
    ui.step("Type pending timestamped note");
    ui.draw=[&]{SessionTargetProbe::notes(live);};ui.frame(3);
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveTimestampedNotes));
    ui.typeVisible("##NewSessionNote","EXACT_PENDING_END_NOTE");
    SessionTargetProbe::completed(live,oldId);
    ui.draw=[&]{SessionTargetProbe::lifecycle(live);};ui.frame(3);
    const auto end=gm::localization::tr(gm::localization::TextId::LiveEndSession);
    ui.clickVisible(end);failure.arm();ui.clickVisible(end);
    require(ui.popupOpen()&&SessionControl::activeSession(c)&&events.saves==0&&events.ended==0&&
        SessionTargetProbe::owner(live).completedId==oldId&&live.selectedPhase()==LiveSessionDashboardPhase::WrapUp&&
        SessionTargetProbe::recapDraft(live)=="EXACT_TYPED_ACTIVE_RECAP"&&SessionTargetProbe::pendingNote(live)=="EXACT_PENDING_END_NOTE",
        "Failed finish lost active text, changed completed target/phase or closed review");
    failure.clear();ui.clickVisible(end);ui.draw=[&]{live.renderEmbedded();};ui.frame(5);
    const auto disk=menuReload(c.metadata.id);const auto *ended=SessionPlan::findSession(disk,activeId);
    require(!SessionControl::activeSession(c)&&events.saves==1&&events.ended==1&&ended&&!ended->endedAtUtc.empty()&&
        ended->recap=="EXACT_TYPED_ACTIVE_RECAP"&&std::count_if(ended->notes.begin(),ended->notes.end(),[](const auto &n){return n.body=="EXACT_PENDING_END_NOTE";})==1&&
        SessionTargetProbe::owner(live).completedId==activeId&&live.selectedPhase()==LiveSessionDashboardPhase::WrapUp&&contains(ui.rendered,"EXACT_TYPED_ACTIVE_RECAP"),
        "Finish retry failed to retain exact saved recap in WrapUp or duplicated pending note");
    require(!TextDraftRecovery::read(c,"live-dashboard").found,"Successful finish left consumed text recovery");
    ui.draw=[&]{SessionTargetProbe::lifecycle(live);};ui.frame(3);ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveStartSession));
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveStart));ui.draw=[&]{live.renderEmbedded();};ui.frame(4);
    require(events.started==1&&events.saves==2&&live.selectedPhase()==LiveSessionDashboardPhase::Prep&&SessionTargetProbe::owner(live).completedId.empty()&&
        SessionControl::activeSession(c)->liveState.phase==SessionPhase::Setup&&SessionTargetProbe::recapDraft(live).empty(),
        "Next session inherited completed-view phase/recap or failed to return to Setup");
}
