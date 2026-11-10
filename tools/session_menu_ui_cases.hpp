// Production preparation menus, real isolated SQLite, no native window/assets.
Campaign menuReload(const EntityId &id)
{
    for(auto c:CampaignRepository::loadAllCampaigns())if(c.metadata.id==id)return c;
    throw std::runtime_error("Menu/combat fixture missing from SQLite");
}
struct SessionMenuFixture
{
    Campaign campaign;SessionWorkspace workspace;SessionWorkspace::Actions actions;
    EntityId session,scene,entry,otherScene;std::vector<EntityId> originalOrder;
    int attempts=0,successful=0,failAt=-1;
    SQLite::Database sql{ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE};
    SessionMenuFixture()
    {
        ProjectCommandManager::getInstance().clear();
        campaign.name="Synthetic menu fixture "+campaign.metadata.id;campaign.ruleSet=std::make_shared<DND5e>();
        StoryThreadModel thread;thread.title="Menu thread";SessionModel plan;plan.title="Selected menu session";
        for(int i=0;i<3;++i) {
            StoryBeatModel beat;beat.title="Menu scene "+std::to_string(i);beat.summary="Original scene "+std::to_string(i);
            SessionPlanEntry link;link.storyBeatId=beat.metadata.id;originalOrder.push_back(link.metadata.id);
            if(i==1){scene=beat.metadata.id;entry=link.metadata.id;}
            if(i==0)otherScene=beat.metadata.id;
            thread.beats.push_back(beat);plan.planEntries.push_back(link);
        }
        session=plan.metadata.id;campaign.sessions.push_back(plan);campaign.sessionIds.push_back(session);campaign.storyThreads.push_back(thread);
        require(ProjectSaveService::getInstance().saveCampaign(campaign,"menu fixture"),"Could not save menu fixture");
        actions.persist=[this](Campaign &staged) {
            const bool fail=++attempts==failAt;
            if(fail)sql.exec("CREATE TRIGGER reject_menu_save BEFORE INSERT ON sessions BEGIN SELECT RAISE(ABORT, 'injected menu persistence failure'); END;");
            const bool saved=ProjectSaveService::getInstance().saveCampaign(staged,"menu UI command");
            if(fail)sql.exec("DROP TRIGGER reject_menu_save;");
            if(saved)++successful;return saved;
        };
        require(workspace.restoreSelection(campaign,session,entry,false,actions.persist),"Could not select real prepared scene");
    }
    ~SessionMenuFixture(){ProjectCommandManager::getInstance().clear();}
    std::vector<EntityId> order()const
    {
        const auto saved=menuReload(campaign.metadata.id);const auto *s=SessionPlan::findSession(saved,session);
        require(s!=nullptr,"Selected menu session disappeared");std::vector<EntityId> ids;
        for(const auto &e:s->planEntries)ids.push_back(e.metadata.id);return ids;
    }
    std::string text()const
    {
        const auto saved=menuReload(campaign.metadata.id);const auto *s=SessionPlan::findScene(saved,scene);
        require(s!=nullptr,"Underlying scene disappeared");return s->summary;
    }
};
void menuScenePane(HeadlessPanel &ui,bool de,bool compact)
{if(compact)ui.clickVisible(de?"Szene":"Scene");}
void menuLinksPane(HeadlessPanel &ui,bool de,bool compact)
{if(compact)ui.clickVisible(de?"Verknüpfungen":"Linked material");}
void menuVisibleFailure(HeadlessPanel &ui,const char *lastAction,const char *message)
{
    ui.frame(2);if(!ui.popupOpen()||!contains(ui.rendered,message))ui.controlFailure("Menu failure feedback missing",lastAction);
    const auto button=ui.visibleItem(lastAction);auto *popup=ImGui::FindWindowByName(button.window.c_str());
    if(!popup||!(popup->Flags&ImGuiWindowFlags_Popup))ui.controlFailure("Failure action is not popup-owned",lastAction,button.id);
    // statusText is deliberately the final submitted row in each popup. Its
    // saved line geometry proves feedback is inside the popup, not solely in
    // an occluded background LogToBuffer entry.
    const ImRect row(ImVec2(popup->WorkRect.Min.x,popup->DC.CursorPosPrevLine.y),
        ImVec2(popup->DC.CursorPosPrevLine.x,popup->DC.CursorPosPrevLine.y+popup->DC.PrevLineSize.y));
    if(!(row.GetHeight()>0&&row.Min.y>=button.maximum.y&&popup->InnerClipRect.Contains(row.Min)&&
        popup->InnerClipRect.Contains(ImVec2(row.Max.x-1,row.Max.y-1)))) {
        std::ostringstream reason;reason<<"Failure feedback is outside the visible popup body\nexpectedText='"<<message
            <<"' row=("<<row.Min.x<<","<<row.Min.y<<")-("<<row.Max.x<<","<<row.Max.y<<")"
            <<" lastAction=("<<button.minimum.x<<","<<button.minimum.y<<")-("<<button.maximum.x<<","<<button.maximum.y<<")"
            <<" prevLineHeight="<<popup->DC.PrevLineSize.y<<" cursor="<<popup->DC.CursorPos.x<<","<<popup->DC.CursorPos.y;
        ui.controlFailure(reason.str().c_str(),lastAction,button.id);
    }
    ui.boundedPopup(lastAction,true,0); // Inspect this same error frame, no extra settling.
}
void sessionCreateMenu(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);SessionMenuFixture f;
    ui.context(std::string("Create menu")+(de?" DE":" EN")+(compact?" compact":" normal"));
    std::vector<int> callbacks(4);std::string expected;
    const auto called=[&](int route){
        ++callbacks[route];require(f.text()==expected&&!f.workspace.dirty()&&f.workspace.sessionId()==f.session&&
            f.workspace.sceneId()==f.scene&&f.workspace.planEntryId()==f.entry,"Create callback preceded save or changed context");
    };
    f.actions.newMap=[&]{called(0);};f.actions.newNpc=[&]{called(1);};f.actions.newCharacter=[&]{called(2);};f.actions.newEncounter=[&]{called(3);};
    ui.draw=[&]{f.workspace.draw(f.campaign,f.actions);};ui.frame(3);
    const char *commands[]={de?"Karte erstellen":"Create a map",de?"NSC erstellen":"Create an NPC",
        de?"Spielerfigur erstellen":"Create a player character",de?"Begegnung vorbereiten":"Prepare an encounter"};
    for(int route=0;route<4;++route) {
        ui.step("Create route "+std::to_string(route));
        menuScenePane(ui,de,compact);expected="EXACT_CREATE_DRAFT_"+std::to_string(route);ui.typeVisible("##scene-summary",expected.c_str());
        menuLinksPane(ui,de,compact);ui.clickVisible(de?"Neuen Inhalt erstellen…":"Create new material…");
        ui.boundedPopup(commands[0],true);for(const char *command:commands)ui.visibleItem(command);
        const int before=f.attempts;
        if(route==0) {
            f.failAt=before+1;ui.clickVisible(commands[route]);
            require(f.attempts==before+1&&f.successful==0&&callbacks[route]==0&&f.workspace.dirty()&&f.text()=="Original scene 1",
                "Failed Create flush navigated, duplicated a save or lost the draft");
            menuVisibleFailure(ui,commands[3],de?"Nicht gespeichert. Dein Entwurf bleibt erhalten.":"Not saved. Your draft is still here.");
        }
        const int retry=f.attempts;ui.clickVisible(commands[route]);
        require(!ui.popupOpen()&&f.attempts==retry+1&&f.successful==route+1&&callbacks[route]==1&&f.text()==expected&&f.order()==f.originalOrder,
            "Create command did not flush exactly once and close successfully");
        const auto saved=menuReload(f.campaign.metadata.id);
        const auto *other=SessionPlan::findScene(saved,f.otherScene);
        require(other&&other->summary=="Original scene 0","Create flush overwrote unrelated scene content");
    }
    ui.step("Create Escape/Attach");const int before=f.attempts;ui.clickVisible(de?"Neuen Inhalt erstellen…":"Create new material…");ui.escape();
    require(f.attempts==before&&callbacks==std::vector<int>({1,1,1,1})&&f.text()==expected&&f.order()==f.originalOrder,
        "Escape from Create changed persistence, callbacks or canonical scene data");
    require(!ui.popupOpen(),"Escape did not close the Create popup");
    ui.clickVisible(de?"+ Inhalt verknüpfen":"+ Attach material");require(ui.popupOpen(),"Existing Attach material popup failed");ui.escape();
    require(f.attempts==before,"Opening/cancelling Attach persisted content");
    require(!ui.popupOpen(),"Escape did not close the Attach popup");
}
void sessionArrangeMenu(bool de,bool compact)
{
    HeadlessPanel ui(de,compact?ImVec2(960,720):ImVec2(1282,752),compact?1.5f:1.f);SessionMenuFixture f;
    ui.context(std::string("Arrange menu")+(de?" DE":" EN")+(compact?" compact":" normal"));
    ui.step("Move up/history/down");
    ui.draw=[&]{f.workspace.draw(f.campaign,f.actions);};ui.frame(3);menuScenePane(ui,de,compact);
    const char *arrange=de?"Szene anordnen":"Arrange scene",*remove=de?"Aus Sitzung entfernen":"Remove from session";
    if(compact)require(ui.visibleItem(arrange).minimum.y>ImGui::GetMainViewport()->WorkPos.y+ImGui::GetMainViewport()->WorkSize.y*.6f,
        "Compact Arrange fixture no longer opens near the lower viewport edge");
    ui.clickVisible(arrange,compact?"SessionScenePane":"ScenePane");ui.boundedPopup(remove,true);
    ui.clickVisible(de?"Nach oben":"Move up");auto raised=f.originalOrder;std::swap(raised[0],raised[1]);
    require(ui.popupOpen()&&f.order()==raised&&f.attempts==1,"Move up did not preserve menu or exact order");
    auto &history=ProjectCommandManager::getInstance();require(history.undo()&&f.order()==f.originalOrder,"UI ordering edit could not undo");
    require(history.redo()&&f.order()==raised,"UI ordering edit could not redo");ui.frame(2);
    ui.clickVisible(de?"Nach unten":"Move down");require(f.order()==f.originalOrder,"Move down changed the wrong entry");ui.escape();
    ui.step("Remove flush failure");ui.typeVisible("##scene-summary","RETAIN_REMOVE_DRAFT");ui.clickVisible(arrange,compact?"SessionScenePane":"ScenePane");
    const int before=f.attempts;f.failAt=before+1;const auto previousUndo=history.undoLabel();ui.clickVisible(remove);
    require(ui.popupOpen()&&f.attempts==before+1&&f.workspace.dirty()&&f.text()=="Original scene 1"&&
        f.workspace.planEntryId()==f.entry&&f.order()==f.originalOrder,"Failed pre-remove flush lost scene/context or closed menu");
    menuVisibleFailure(ui,remove,de?"Nicht gespeichert. Dein Entwurf bleibt erhalten.":"Not saved. Your draft is still here.");
    ui.step("Remove transaction failure");const int flushThenRemove=f.attempts;f.failAt=flushThenRemove+2;ui.clickVisible(remove);
    require(ui.popupOpen()&&f.attempts==flushThenRemove+2&&!f.workspace.dirty()&&f.text()=="RETAIN_REMOVE_DRAFT"&&
        f.workspace.planEntryId()==f.entry&&f.order()==f.originalOrder&&history.undoLabel()==previousUndo,
        "Failed removal after successful flush closed review, changed membership or registered history");
    menuVisibleFailure(ui,remove,de?"Die Aktion wurde nicht gespeichert.":"The action was not saved.");
    ui.step("Remove retry");const int retry=f.attempts;ui.clickVisible(remove);
    auto removed=f.originalOrder;removed.erase(removed.begin()+1);
    require(!ui.popupOpen()&&f.attempts==retry+1&&f.order()==removed&&f.text()=="RETAIN_REMOVE_DRAFT"&&
        f.workspace.sessionId()==f.session,"Successful removal failed to close or deleted underlying scene");
}
