// Real production launcher, retained recovery, and SQLite transaction refusal.
// Memory-only viewports model a lower owner work area, not native monitor UX.
struct CompactSqlFault
{
    SQLite::Database sql{ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE};
    std::string name;
    CompactSqlFault(const char *id,const char *table,const char *when=""):name(id)
    {sql.exec("CREATE TRIGGER "+name+" BEFORE INSERT ON "+table+" "+when+
        " BEGIN SELECT RAISE(ABORT, 'compact popup injected save failure'); END;");}
    void clear(){if(!name.empty()){sql.exec("DROP TRIGGER "+name);name.clear();}}
    ~CompactSqlFault(){if(!name.empty())try{clear();}catch(...){}}
};
void compactOutcomePopup(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Owner-bounded outcome popup 27px ")+(de?"DE":"EN"));
    Campaign c;c.name="Compact outcome "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    SessionModel session;session.title="Session with retained authored notes";
    c.sessions.push_back(session);c.sessionIds.push_back(session.metadata.id);
    WorldPlaceModel place;place.name="Lantern archive";place.notes="Unchanged canonical place notes";c.worldPlaces.push_back(place);
    require(ProjectSaveService::getInstance().saveCampaign(c,"compact outcome fixture"),"Cannot save outcome fixture");
    SessionOutcomesPanel panel;int attempts=0;
    const auto sid=session.metadata.id;
    const char *open=de?"Ergebnisse und nächste Sitzung":"Outcomes and next session";
    const char *close=de?"Zurück zur Sitzung":"Return to session";
    const char *record=de?"Ergebnis festhalten":"Record outcome";
    const char *tab=de?"Festhalten":"Record";
    ui.draw=[&]{const auto *vp=ImGui::GetMainViewport();
        ImGui::SetCursorScreenPos(ImVec2(vp->WorkPos.x+25,vp->WorkPos.y+vp->WorkSize.y-85));
        panel.drawLauncher(c,sid,[&](Campaign &staged){++attempts;
            return ProjectSaveService::getInstance().saveCampaign(staged,"compact outcome record");});};
    ui.frame(4);require(ui.compactItem(open).minimum.y>ImGui::GetMainViewport()->WorkPos.y+720*.8f,
        "Outcome launcher no longer exercises lower-edge placement");
    ui.compactClick(open);ui.compactBounds(close,"SessionOutcomes");
    ui.compactType("##outcome-private","COMPACT_OUTCOME_PRIVATE");
    const auto target=ImHashStr("##outcome-target",0,ui.compactItem(tab).id);
    ui.compactClick("##outcome-target",target);ui.frame(3);
    ui.compactClick((place.name+"##"+place.metadata.id).c_str(),0,true);
    ui.compactClick(de?"Änderung zur Abschlussprüfung vorschlagen":"Propose a change for wrap-up review");
    for(int n=0;n<4;++n){ui.frame();ui.compactBounds(close,"SessionOutcomes");}
    ui.compactType("##outcome-change","COMPACT_PROPOSED_PLACE_NOTE");
    ui.compactClick(de?"Ursprung und Spielertext":"Origin and player text");
    for(int n=0;n<4;++n){ui.frame();ui.compactBounds(close,"SessionOutcomes");}
    ui.compactType("##outcome-public","COMPACT_SEPARATE_PUBLIC_TEXT");
    const auto key=SessionOutcomeDraftKeys::composer(SessionOutcomeDraftKeys::Surface::Preparation,sid);
    const auto checkpoint=[&] {
        const auto saved=TextDraftRecovery::read(c,key);
        require(saved.success&&saved.found&&saved.fields.count("ledger"),"Outcome failure lost recovery file");
        const auto decoded=SessionOutcomes::deserialize(saved.fields.at("ledger"));
        require(decoded.success&&decoded.ledger.outcomes.size()==1,"Outcome checkpoint is not one complete draft");
        const auto &draft=decoded.ledger.outcomes[0];
        require(draft.privateText=="COMPACT_OUTCOME_PRIVATE"&&draft.changeText=="COMPACT_PROPOSED_PLACE_NOTE"&&
            draft.publicText=="COMPACT_SEPARATE_PUBLIC_TEXT"&&draft.target.id==place.metadata.id&&
            draft.action==SessionOutcomes::Action::PlaceNote,"Outcome checkpoint changed authored fields or target");
    };
    ui.step("Real SQL refusal and retained authored fields");
    CompactSqlFault fault("reject_compact_outcome","campaign_entity_properties","WHEN NEW.property_key='gm.session.outcomes.v1'");
    ui.reviewTabTo(record);ui.reviewKey(ImGuiKey_Space);
    require(attempts==1&&ui.popupOpen(),"Failed Record closed popup or repeated persistence");
    for(const auto &campaign:{c,menuReload(c.metadata.id)}) {
        const auto ledger=SessionOutcomes::read(*SessionPlan::findSession(campaign,sid));
        require(ledger.success&&ledger.ledger.outcomes.empty()&&campaign.worldPlaces[0].notes==place.notes,
            "Failed Record changed committed ledger or place");
    }
    checkpoint();ui.compactBounds(close,"SessionOutcomes");
    ui.compactOutcomeStatus(tab,session.title.c_str(),"Changes could not be saved. Previous state and drafts are retained.");
    // Genuine keyboard traversal from grown body into the persistent footer,
    // then back, then activation: no focus/scroll assignments or fake callbacks.
    ui.step("Body to fixed footer keyboard navigation");
    ui.reviewTabTo(close);ui.reviewKey(ImGuiKey_Tab,true);ui.reviewTabTo(close);ui.reviewKey(ImGuiKey_Space);
    require(!ui.popupOpen()&&attempts==1,"Return footer lost draft or retried failed save");checkpoint();
    ui.compactClick(open);ui.compactBounds(close,"SessionOutcomes");
    ui.compactReach("##outcome-private");require(contains(ui.rendered,"COMPACT_OUTCOME_PRIVATE"),"Reopened outcome draft missing");
    fault.clear();ui.step("Explicit successful retry exactly once");
    ui.reviewTabTo(record);ui.reviewKey(ImGuiKey_Space);
    require(attempts==2&&ui.popupOpen(),"Record retry did not cross one persistence boundary");
    const auto saved=menuReload(c.metadata.id);const auto ledger=SessionOutcomes::read(*SessionPlan::findSession(saved,sid));
    require(ledger.success&&ledger.ledger.outcomes.size()==1&&ledger.ledger.outcomes[0].privateText=="COMPACT_OUTCOME_PRIVATE"&&
        ledger.ledger.outcomes[0].changeText=="COMPACT_PROPOSED_PLACE_NOTE"&&saved.worldPlaces[0].notes==place.notes&&
        !TextDraftRecovery::read(c,key).found,"Successful Record lost text, applied proposal early, or kept completed recovery");
    ui.compactBounds(close,"SessionOutcomes");ui.reviewKey(ImGuiKey_Escape);
    require(!ui.popupOpen()&&attempts==2,"Escape altered completed outcome or failed to close popup");
}
