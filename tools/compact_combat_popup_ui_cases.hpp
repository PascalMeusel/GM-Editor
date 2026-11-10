// No maps, textures, playback service or native window: real battle controller,
// source character data, transaction fault, retry and scoped command history.
void compactCombatPopup(bool de)
{
    HeadlessPanel ui(de,ImVec2(1280,720),1.5f,18.f);ui.compactOwner();
    ui.context(std::string("Owner-bounded combat popup 27px ")+(de?"DE":"EN"));
    auto &history=ProjectCommandManager::getInstance();history.clear();
    Campaign c;c.name="Compact battle "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    EncounterModel fight;fight.name="Synthetic narrative battle";fight.theaterOfMind=true;
    fight.round=3;fight.combatPhase=CombatPhase::ActiveTurn;
    for(int i=0;i<2;++i){
        CharacterModel actor;std::snprintf(actor.nameBuffer,sizeof(actor.nameBuffer),"Compact actor %d",i);
        actor.maxHitPoints=actor.currentHitPoints=20+i*10;
        for(int n=0;n<5;++n){ResourceTrackerModel r;
            r.name=n<3?"Reserve "+std::to_string(n):
                "Long authored resource name for a wrapped correction row and reliable lower-body scrolling "+std::to_string(n);
            r.maximum=5+n;r.used=i;actor.resourceTrackers.push_back(r);}
        c.characters.push_back(actor);EncounterParticipantModel p;p.kind=CombatantKind::Player;
        p.sourceEntityId=actor.metadata.id;p.displayName=actor.nameBuffer;
        p.currentHitPoints=actor.currentHitPoints;p.maxHitPoints=actor.maxHitPoints;fight.participants.push_back(p);
    }
    c.encounters.push_back(fight);
    const auto actorId=c.characters[0].metadata.id,otherId=c.characters[1].metadata.id;
    const auto participant=fight.participants[0].metadata.id,otherParticipant=fight.participants[1].metadata.id;
    const auto resource=c.characters[0].resourceTrackers.back().metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"compact combat fixture"),"Cannot save compact combat fixture");
    BattleTrackerController tracker(c,nullptr);require(tracker.selectEncounterById(fight.metadata.id),"Cannot select compact battle");
    BattleTrackerTurnsProbe::inspectOther(tracker);
    ui.draw=[&]{ImGui::Dummy(ImVec2(1,220));tracker.renderEmbedded();};ui.frame(4);
    const char *open=de?"TP / Ressourcen / Verlauf":"HP / Resources / Undo";
    const char *close=de?"Schließen":"Close",*amount=de?"Betrag":"Amount";
    const char *undo=de?"Rückgängig":"Undo",*redo=de?"Wiederholen":"Redo";
    const auto state=[&](int ramHp,int sqlHp,int used) {
        const auto saved=menuReload(c.metadata.id);
        for(const Campaign *campaign:{static_cast<const Campaign *>(&c),&saved}){
            const int hp=campaign==&c?ramHp:sqlHp;
            const auto &actor=correctionById(campaign->characters,actorId),&other=correctionById(campaign->characters,otherId);
            const auto &encounter=correctionById(campaign->encounters,fight.metadata.id);
            require(actor.currentHitPoints==hp&&other.currentHitPoints==30&&actor.temporaryHitPoints==0&&other.temporaryHitPoints==0,
                "Compact correction changed wrong source actor or HP domain");
            require(correctionById(encounter.participants,participant).currentHitPoints==hp&&
                correctionById(encounter.participants,otherParticipant).currentHitPoints==30&&encounter.round==3&&
                encounter.activeParticipantIndex==0&&encounter.combatPhase==CombatPhase::ActiveTurn,
                "Compact correction changed wrong participant, turn, round or phase");
            for(const auto &r:actor.resourceTrackers)require(r.used==(r.metadata.id==resource?used:0),"Unrelated active resource changed");
            for(const auto &r:other.resourceTrackers)require(r.used==1,"Other actor resource changed");
        }
        const auto &draft=BattleTrackerTurnsProbe::draft(tracker);
        require(correctionById(draft.participants,participant).currentHitPoints==ramHp&&draft.round==3&&
            draft.activeParticipantIndex==0&&draft.combatPhase==CombatPhase::ActiveTurn,
            "Combat RAM draft did not retain intended correction");
        require(BattleTrackerTurnsProbe::inspected(tracker)==1&&BattleTrackerTurnsProbe::action(tracker)==0&&
            BattleTrackerTurnsProbe::primaryTarget(tracker)==0&&BattleTrackerTurnsProbe::targets(tracker)==std::vector<int>{0},
            "Popup correction changed inspected actor or action targets");
    };
    state(20,20,0);
    require(ui.compactItem(open).minimum.y>ImGui::GetMainViewport()->WorkPos.y+360,
        "Combat launcher no longer exercises lower-half owner placement");
    ui.compactClick(open);auto *popup=ui.compactBounds(close,"CombatQuickCorrections");
    const auto input=ui.compactItem(amount);auto *body=ImGui::FindWindowByName(input.window.c_str());
    require(body&&body->ParentWindow==popup&&(body->ChildFlags&ImGuiChildFlags_NavFlattened),
        "Combat fields are not in the real flattened scrolling body");
    const auto footerRect=ImRect(ui.compactItem(close).minimum,ui.compactItem(close).maximum);
    ui.compactType(amount,"4");
    ui.step("Failed damage retains RAM, prior SQL and guarded history");
    CompactSqlFault fault("reject_compact_combat","encounters");
    ui.compactClick(de?"Schaden anwenden":"Apply damage");state(16,20,0);
    require(ui.popupOpen()&&history.canUndo()&&!history.canRedo(),"Failed damage lost correction history");
    const auto pendingLabel=history.undoLabel();
    ui.compactBounds(close,"CombatQuickCorrections");
    const char *error=gm::localization::trUi("Combat changes could not be saved. Your edits are retained. Retry saving.");
    ui.compactLastRow(amount,error);
    require(ImRect(ui.compactItem(close).minimum,ui.compactItem(close).maximum).Min.y==footerRect.Min.y,
        "Wrapped failure/resource growth displaced persistent combat Close footer");
    ui.compactReach(amount);ui.reviewState(amount,true);
    ui.compactClick(undo);state(16,20,0);
    require(history.undoLabel()==pendingLabel&&history.canUndo()&&!history.canRedo()&&!history.lastError().empty(),
        "Pending-save Undo crossed its guard or consumed the correction command");
    ui.step("Keyboard Close then actual Retry saving");
    ui.reviewTabTo(close);ui.reviewKey(ImGuiKey_Tab,true);ui.reviewTabTo(close);ui.reviewKey(ImGuiKey_Space);
    require(!ui.popupOpen(),"Persistent combat footer did not close by actual Space");fault.clear();
    ui.compactClick(gm::localization::trUi("Retry saving"));state(16,16,0);
    ui.compactClick(open);ui.compactBounds(close,"CombatQuickCorrections");
    ui.compactClick(undo);state(20,20,0);
    require(!history.canUndo()&&history.canRedo(),"Retry registered duplicate damage commands");
    ui.compactClick(redo);state(16,16,0);
    // The last resource is intentionally below several wrapped rows. Resolve
    // its actual scoped input from the observed body seed, then wheel to it.
    body=ImGui::FindWindowByName(ui.compactItem(amount).window.c_str());
    const auto resourceScope=ImHashStr(resource.c_str(),0,body->ID);
    const auto usedId=ImHashStr(de?"Verbraucht":"Used",0,resourceScope);
    ui.step("Reach wrapped resource, edit and undo without changing turn");
    ui.compactType(de?"Verbraucht":"Used","1",usedId);state(16,16,1);
    ui.compactClick(undo);state(16,16,0);ui.compactClick(redo);state(16,16,1);
    ui.compactBounds(close,"CombatQuickCorrections");ui.reviewKey(ImGuiKey_Escape);
    require(!ui.popupOpen(),"Escape failed after combat history replaced live storage");state(16,16,1);history.clear();
}
