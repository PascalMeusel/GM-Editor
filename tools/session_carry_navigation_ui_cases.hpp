void sessionCarryNavigation(bool de)
{
    HeadlessPanel ui(de);ui.context(std::string("Explicit carry destination and checkpoint refusal ")+(de?"DE":"EN"));
    auto c=fixture();c.ruleSet=std::make_shared<DND5e>();const auto source=c.sessions[0].metadata.id;
    SessionModel decoy;decoy.title="EXACT_NAMED_DESTINATION";c.sessions.push_back(decoy);c.sessionIds.push_back(decoy.metadata.id);
    require(ProjectSaveService::getInstance().saveCampaign(c,"carry navigation fixture"),"Cannot save carry fixture");
    SessionOutcomesPanel panel;SessionEventCounter events(c.metadata.id);std::optional<PreparationTargetRequest> request;
    int emissions=0;bool emittedInsidePopup=false;
    const auto sink=[&](PreparationTargetRequest value){request=std::move(value);++emissions;
        emittedInsidePopup=!ImGui::GetCurrentContext()->BeginPopupStack.empty();};
    const auto persist=[](Campaign &value){return ProjectSaveService::getInstance().saveCampaign(value,"explicit carry");};
    ui.draw=[&]{panel.drawLauncher(c,source,persist,{},sink);};ui.frame(3);
    const char *launcher=de?"Ergebnisse und nächste Sitzung":"Outcomes and next session";
    const char *next=de?"Nächste Sitzung":"Next session",*carry=de?"Ausgewähltes Material übernehmen":"Carry selected material forward";
    const char *open=de?"Diese Sitzung vorbereiten":"Prepare this session";
    ui.compactClick(launcher);ui.compactClick(next);ui.compactClick("##carry-select");
    ui.compactType("##next-title","EXACT_NAMED_DESTINATION");
    SessionSqlFailure failure;failure.arm();ui.compactClick(carry);
    require(!ui.hasControl(open)&&!request&&c.sessions.size()==2&&events.saves==0&&events.started==0,
        "Failed carry offered a destination or partly created/started a session");
    failure.clear();ui.compactClick(carry);require(ui.hasControl(open)&&contains(ui.rendered,"EXACT_NAMED_DESTINATION")&&
        c.sessions.size()==3&&events.saves==1&&events.started==0&&!request,"Carry silently navigated or duplicated creation");
    EntityId target;for(const auto &s:c.sessions)if(s.metadata.id!=source&&s.metadata.id!=decoy.metadata.id)target=s.metadata.id;
    require(!target.empty()&&SessionOutcomes::read(*SessionPlan::findSession(c,target)).ledger.followUps.size()==1,"Carry positive control has no exact follow-up");
    ui.compactClick(carry);require(c.sessions.size()==3&&events.saves==1,"Same carry receipt duplicated destination or saved twice");
    // A retained composer checkpoint must succeed before navigation is emitted.
    ui.compactClick(de?"Festhalten":"Record");ui.compactType("##outcome-private","LATEST_UNRECORDED_CARRY_DRAFT");
    const auto key=SessionOutcomeDraftKeys::composer(SessionOutcomeDraftKeys::Surface::Preparation,source);
    require(panel.checkpointDrafts(c),"Cannot save baseline carry composer checkpoint");
    const auto baseline=TextDraftRecovery::read(c,key);
    ui.compactType("##outcome-private","NEWEST_UNRECORDED_CARRY_DRAFT");
    const auto path=ProjectPackageLayout::layoutForCampaign(c).recoveryDirectory/"Drafts"/(key+".draft");
    const auto held=path.parent_path()/(key+".held");std::filesystem::rename(path,held);std::filesystem::create_directory(path);
    ui.compactClick(next);ui.compactClick(open);
    require(ui.popupOpen()&&!request&&emissions==0&&events.saves==1&&
        SessionOutcomes::read(*SessionPlan::findSession(c,source)).ledger.outcomes.size()==1,
        "Failed checkpoint closed the launcher, emitted navigation or implicitly recorded draft");
    std::filesystem::remove(path);std::filesystem::rename(held,path);
    require(TextDraftRecovery::read(c,key).fields==baseline.fields,"Failed checkpoint replaced prior bytes");
    ui.compactClick(open);
    require(request&&request->campaignId==c.metadata.id&&request->sourceSessionId==source&&request->targetSessionId==target&&
        request->sourceSurface==SessionOutcomeDraftKeys::Surface::Preparation&&!request->carryRequestId.empty()&&
        panel.matchesPreparationTarget(*request)&&emissions==1&&emittedInsidePopup&&!ui.popupOpen(),
        "Explicit Prepare did not emit the exact carried target and unwind its popup once");
    const auto checkpoint=TextDraftRecovery::read(c,key);
    require(checkpoint.success&&checkpoint.found&&checkpoint.fields.at("ledger").find("NEWEST_UNRECORDED_CARRY_DRAFT")!=std::string::npos&&
        events.saves==1&&events.started==0,"Prepare retry lost newest private draft or crossed canonical save/start");
    c.presentationModeActive=true;ui.frame(2);require(!panel.matchesPreparationTarget(*request),"Presentation retained a private carry authorization");
}

void sessionCarryExistingDestination(bool de)
{
    HeadlessPanel ui(de);ui.context(std::string("Carry into existing session exact result ")+(de?"DE":"EN"));
    auto c=fixture();c.ruleSet=std::make_shared<DND5e>();const auto source=c.sessions[0].metadata.id;
    SessionModel target;target.title="EXISTING_NAMED_DESTINATION";c.sessions.push_back(target);c.sessionIds.push_back(target.metadata.id);
    require(ProjectSaveService::getInstance().saveCampaign(c,"existing carry destination fixture"),"Cannot save existing carry fixture");
    SessionOutcomesPanel panel;SessionEventCounter events(c.metadata.id);std::optional<PreparationTargetRequest> request;
    const auto sink=[&](PreparationTargetRequest value){request=std::move(value);};
    const auto persist=[](Campaign &value){return ProjectSaveService::getInstance().saveCampaign(value,"carry existing target");};
    ui.draw=[&]{panel.draw(c,source,persist,{},sink);};ui.frame(3);
    const char *next=de?"Nächste Sitzung":"Next session";ui.compactClick(next);ui.compactClick("##carry-select");
    // Inline BeginCombo has geometry but no label hook. BeginTabItem supplies
    // the observed tab's exact native seed, as in existing source-scene cases.
    ui.compactClick("##carry-target",ImHashStr("##carry-target",0,ui.controlId(next)));
    ui.compactClick((target.title+"##"+target.metadata.id).c_str(),0,true);
    ui.compactClick(de?"Ausgewähltes Material übernehmen":"Carry selected material forward");
    const auto saved=SessionOutcomes::read(*SessionPlan::findSession(c,target.metadata.id));
    require(saved.success&&saved.ledger.carryRequestId.empty()&&saved.ledger.followUps.size()==1&&c.sessions.size()==2&&events.saves==1,
        "Existing destination fixture did not exercise carry without new-session receipt field");
    ui.compactClick(de?"Diese Sitzung vorbereiten":"Prepare this session");
    require(request&&request->targetSessionId==target.metadata.id&&panel.matchesPreparationTarget(*request)&&events.saves==1&&events.started==0,
        "Existing destination action depended on new-session-only receipt or replayed carry");
}
