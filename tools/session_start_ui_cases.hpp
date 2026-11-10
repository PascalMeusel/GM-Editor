struct SessionSqlFailure
{
    SQLite::Database sql{ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE};
    bool armed=false;
    void arm(){sql.exec("CREATE TRIGGER reject_reviewed_session BEFORE INSERT ON sessions BEGIN SELECT RAISE(ABORT, 'injected session review failure'); END;");armed=true;}
    void clear(){if(armed){sql.exec("DROP TRIGGER reject_reviewed_session;");armed=false;}}
    ~SessionSqlFailure(){clear();}
};
Campaign reviewedUiCampaign(bool prepared)
{
    Campaign c;c.name="Reviewed UI "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    CharacterModel player;c.characters.push_back(player);
    if(prepared){SessionModel s;s.title="EXACT_PREPARED_PRIVATE_TITLE";c.sessions.push_back(s);c.sessionIds.push_back(s.metadata.id);}
    require(ProjectSaveService::getInstance().saveCampaign(c,"session review fixture"),"Cannot save session review fixture");return c;
}
void reviewedStartUi(bool de,bool compact,bool legacy,bool prepared)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);
    ui.context(std::string("Reviewed start")+(legacy?" legacy GM":" Live")+(prepared?" prepared":" new")+(de?" DE":" EN")+(compact?" compact":" normal"));
    auto c=reviewedUiCampaign(prepared);LiveSessionDashboardController live(c,nullptr);GMScreenController gm(c,nullptr);
    SessionEventCounter events(c.metadata.id);SessionSqlFailure failure;
    ui.draw=[&]{if(legacy)SessionTargetProbe::legacy(gm);else SessionTargetProbe::lifecycle(live);};ui.frame(3);
    const auto open=legacy?gm::localization::trUi("Start Session"):gm::localization::tr(gm::localization::TextId::LiveStartSession);
    const auto confirm=legacy?gm::localization::trUi("Start"):gm::localization::tr(gm::localization::TextId::LiveStart);
    const auto cancel=gm::localization::tr(gm::localization::TextId::CommonCancel);
    ui.clickVisible(open);ui.boundedPopup(confirm,false);
    require(contains(ui.rendered,de?"Vorbereitung":"Setup"),"Start review does not describe Setup");
    if(prepared){require(contains(ui.rendered,"EXACT_PREPARED_PRIVATE_TITLE"),"Prepared review title differs from target");ui.absent(de?"Sitzungstitel":"Session title");}
    ui.clickVisible(cancel);require(!ui.popupOpen()&&events.saves==0&&events.started==0,"Start Cancel changed state");
    ui.clickVisible(open);
    if(!prepared)ui.typeVisible(de?"Sitzungstitel":"Session title","EXACT_CUSTOM_NEW_TITLE");
    const auto review=legacy?SessionTargetProbe::start(gm):SessionTargetProbe::start(live);
    const std::string title=prepared?"EXACT_PREPARED_PRIVATE_TITLE":"EXACT_CUSTOM_NEW_TITLE";
    require(review.title==title,"Typed title did not bind reviewed target");
    ui.step("SQL failure then same-target retry");failure.arm();ui.clickVisible(confirm);ui.frame(3);
    const auto failed=menuReload(c.metadata.id);
    require(ui.popupOpen()&&events.saves==0&&events.started==0&&!SessionControl::activeSession(c)&&failed.sessions.size()==(prepared?1u:0u)&&
        (legacy?SessionTargetProbe::start(gm):SessionTargetProbe::start(live)).sessionId==review.sessionId&&contains(ui.rendered,title.c_str()),
        "Failed start closed/retargeted review or durably created a partial session");
    require(contains(ui.rendered,de?(legacy?"Die Aktion wurde nicht gespeichert":"Nicht gespeichert"):"The changes could not be saved"),"Retained start review hides failure");
    ui.boundedPopup(confirm,false);failure.clear();ui.clickVisible(confirm);
    const auto saved=menuReload(c.metadata.id);const auto *session=SessionPlan::findSession(saved,review.sessionId);
    require(!ui.popupOpen()&&events.saves==1&&events.started==1&&saved.sessions.size()==1&&saved.sessionIds==std::vector<EntityId>{review.sessionId}&&
        session&&session->title==title&&session->liveModeActive&&session->liveState.phase==SessionPhase::Setup&&
        session->participantCharacterIds==std::vector<EntityId>{c.characters.front().metadata.id},"Successful reviewed start lost identity/party or saved/emitted twice");
    require(!(legacy?SessionTargetProbe::confirm(gm):SessionTargetProbe::confirm(live))&&events.saves==1&&events.started==1,"Revoked success review started twice");
}

void staleStartUi(bool de)
{
    HeadlessPanel ui(de);auto c=reviewedUiCampaign(true);LiveSessionDashboardController live(c,nullptr);SessionEventCounter events(c.metadata.id);
    ui.context(de?"Stale Live start DE":"Stale Live start EN");ui.draw=[&]{SessionTargetProbe::lifecycle(live);};ui.frame(3);
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveStartSession));
    c.sessions.front().title="NEW_CANONICAL_TITLE";
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::LiveStart));
    require(ui.popupOpen()&&events.saves==0&&events.started==0&&contains(ui.rendered,"EXACT_PREPARED_PRIVATE_TITLE")&&
        !SessionControl::activeSession(c),"Changed prepared review silently retargeted or started");
    require(contains(ui.rendered,de?"Bitte abbrechen und den Start erneut prüfen":"Cancel and review the start again"),
        "Stale start review lacks actionable localized guidance");
    ui.clickVisible(gm::localization::tr(gm::localization::TextId::CommonCancel));
}
