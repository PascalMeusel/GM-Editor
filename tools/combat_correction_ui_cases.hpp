// Real battle view/controller/save/history. Narrative-only fixture returns
// before map canvas rendering; no maps, images, audio service or playback.
template<class T> const T &correctionById(const std::vector<T> &rows,const EntityId &id)
{
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto &v){return v.metadata.id==id;});
    require(found!=rows.end(),"Combat correction canary disappeared");return *found;
}
void combatCorrectionPopup(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);
    ui.context(std::string("Combat corrections")+(de?" DE":" EN")+(compact?" compact":" normal"));
    auto &history=ProjectCommandManager::getInstance();history.clear();
    Campaign c;c.name="Synthetic popup corrections "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    EncounterModel encounter;encounter.name="Narrative correction fixture";encounter.theaterOfMind=true;
    // ActionResolution intentionally selects its attacker on every render.
    // ActiveTurn permits inspecting another combatant while the top shortcut
    // still targets the active actor; use a coherent other->active target.
    encounter.combatPhase=CombatPhase::ActiveTurn;encounter.round=3;
    for(int i=0;i<2;++i) {
        CharacterModel actor;std::snprintf(actor.nameBuffer,sizeof(actor.nameBuffer),"Correction actor %d",i);
        actor.maxHitPoints=20+i*10;actor.currentHitPoints=actor.maxHitPoints;
        ResourceTrackerModel resource;resource.name="Synthetic reserve "+std::to_string(i);resource.maximum=3+i;resource.used=i;
        actor.resourceTrackers.push_back(resource);c.characters.push_back(actor);
        EncounterParticipantModel p;p.kind=CombatantKind::Player;p.sourceEntityId=actor.metadata.id;
        p.displayName=actor.nameBuffer;p.currentHitPoints=actor.currentHitPoints;p.maxHitPoints=actor.maxHitPoints;encounter.participants.push_back(p);
    }
    const auto activeId=c.characters[0].metadata.id,otherId=c.characters[1].metadata.id;
    const auto activeParticipant=encounter.participants[0].metadata.id,otherParticipant=encounter.participants[1].metadata.id;
    const auto resourceId=c.characters[0].resourceTrackers[0].metadata.id,otherResource=c.characters[1].resourceTrackers[0].metadata.id;
    c.encounters.push_back(encounter);
    require(ProjectSaveService::getInstance().saveCampaign(c,"combat popup fixture"),"Could not save real combat popup fixture");
    BattleTrackerController tracker(c,nullptr);require(tracker.selectEncounterById(encounter.metadata.id),"Could not select active narrative encounter");
    BattleTrackerTurnsProbe::inspectOther(tracker);
    ui.draw=[&]{tracker.renderEmbedded();};ui.frame(3);
    const auto assertState=[&](const char *stage,int hp,int used) {
        const std::string context=std::string(stage)+(de?" DE":" EN")+(compact?" compact":" normal");
        const auto equal=[&](const std::string &field,int actual,int expected) {
            if(actual!=expected)throw std::runtime_error(context+": "+field+" actual="+std::to_string(actual)+" expected="+std::to_string(expected));
        };
        const auto saved=menuReload(c.metadata.id);
        for(const Campaign *campaign:{static_cast<const Campaign *>(&c),&saved}) {
            const std::string owner=campaign==&c?"live ":"SQL ";
            const auto &active=correctionById(campaign->characters,activeId),&other=correctionById(campaign->characters,otherId);
            const auto &fight=correctionById(campaign->encounters,encounter.metadata.id);
            equal(owner+"active canonical HP",active.currentHitPoints,hp);equal(owner+"active temporary HP",active.temporaryHitPoints,0);
            equal(owner+"other canonical HP",other.currentHitPoints,30);equal(owner+"other temporary HP",other.temporaryHitPoints,0);
            equal(owner+"active resource",correctionById(active.resourceTrackers,resourceId).used,used);
            equal(owner+"other resource",correctionById(other.resourceTrackers,otherResource).used,1);
            equal(owner+"active encounter HP",correctionById(fight.participants,activeParticipant).currentHitPoints,hp);
            equal(owner+"other encounter HP",correctionById(fight.participants,otherParticipant).currentHitPoints,30);
            equal(owner+"round",fight.round,3);equal(owner+"active index",fight.activeParticipantIndex,0);
            equal(owner+"phase",int(fight.combatPhase),int(CombatPhase::ActiveTurn));
        }
        const auto &draft=BattleTrackerTurnsProbe::draft(tracker);
        equal("draft active HP",correctionById(draft.participants,activeParticipant).currentHitPoints,hp);
        equal("draft other HP",correctionById(draft.participants,otherParticipant).currentHitPoints,30);
        equal("draft round",draft.round,3);equal("draft active index",draft.activeParticipantIndex,0);
        equal("draft phase",int(draft.combatPhase),int(CombatPhase::ActiveTurn));
        equal("inspected participant",BattleTrackerTurnsProbe::inspected(tracker),1);
        equal("selected action",BattleTrackerTurnsProbe::action(tracker),0);
        equal("primary target",BattleTrackerTurnsProbe::primaryTarget(tracker),0);
        const auto &targets=BattleTrackerTurnsProbe::targets(tracker);
        if(targets!=std::vector<int>{0}) {
            std::string actual;for(int target:targets)actual+=(actual.empty()?"":",")+std::to_string(target);
            throw std::runtime_error(context+": selected targets actual={"+actual+"} expected={0}");
        }
    };
    assertState("before opening shortcut",20,0);
    const char *open=de?"TP / Ressourcen / Verlauf":"HP / Resources / Undo";
    ui.step("Open initial shortcut");
    const auto shortcut=ui.visibleItem(open);ui.clickVisible(open);ui.boundedPopup(de?"Betrag":"Amount",false);
    const auto amount=ui.visibleItem(de?"Betrag":"Amount");auto *body=ImGui::FindWindowByName(amount.window.c_str());
    auto *popup=body;while(popup&&!(popup->Flags&ImGuiWindowFlags_Popup))popup=popup->ParentWindow;
    auto *host=ImGui::FindWindowByName(shortcut.window.c_str());
    require(body&&body->InnerClipRect.Contains(ImRect(amount.minimum,amount.maximum))&&popup&&host&&
        popup->PopupId==ImHashStr("CombatQuickCorrections",0,host->ID)&&
        popup->ParentWindow==host&&shortcut.window.find("CombatControls")==std::string::npos,
        "Combat shortcut popup is not in the persistent navigation's parent scope");
    assertState("after opening shortcut",20,0);require(!history.canUndo(),"Opening combat corrections recorded an edit");
    ui.escape();require(!ui.popupOpen(),"Escape failed to close combat corrections");assertState("after initial Escape",20,0);
    ui.step("Reopen/type damage");ui.clickVisible(open);ui.typeVisible(de?"Betrag":"Amount","4");
    ui.step("Apply damage");ui.clickVisible(de?"Schaden anwenden":"Apply damage");
    require(ui.popupOpen()&&history.canUndo()&&!history.canRedo(),"Damage popup/history unavailable after correction");assertState("after damage",16,0);
    ui.step("Damage Undo");ui.clickVisible(de?"Rückgängig":"Undo");assertState("after damage Undo",20,0);
    require(!history.canUndo()&&history.canRedo(),"One damage click registered more than one undo step");
    ui.step("Damage Redo");ui.clickVisible(de?"Wiederholen":"Redo");assertState("after damage Redo",16,0);
    ui.step("Resource input");ui.typeVisible(de?"Verbraucht":"Used","1");assertState("after resource edit",16,1);
    ui.step("Resource Undo");ui.clickVisible(de?"Rückgängig":"Undo");assertState("after resource Undo",16,0);
    ui.step("Resource Redo");ui.clickVisible(de?"Wiederholen":"Redo");assertState("after resource Redo",16,1);
    require(ui.popupOpen(),"Correction history closed the real popup unexpectedly");
    ui.escape();assertState("after final Escape",16,1);history.clear();
}
