// Actual World widgets and real SQLite, with public controller undo/redo.
// This does not claim application shortcut or native-window coverage.
struct WorldHistoryEvents
{
    int saves=0,id;
    explicit WorldHistoryEvents(EntityId campaign):id(ProjectEventBus::getInstance().subscribe([this,campaign](const ProjectEvent &event){
        if(event.type==ProjectEventType::CampaignSaved&&event.campaignId==campaign)++saves;})){}
    ~WorldHistoryEvents(){ProjectEventBus::getInstance().unsubscribe(id);}
};
struct WorldHistorySqlFault
{
    SQLite::Database sql{ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE};
    WorldHistorySqlFault(){sql.exec("CREATE TRIGGER reject_world_history BEFORE INSERT ON world_places BEGIN SELECT RAISE(ABORT,'World history SQL failure'); END;");}
    ~WorldHistorySqlFault(){try{sql.exec("DROP TRIGGER reject_world_history;");}catch(...){}}
};
void worldHistoryFailureAndDrafts(UiInteractionTrace &trace,bool de)
{
    std::cout<<"World history UI: save/undo/redo failures and retained drafts\n";
    auto c=worldUiFixture();const auto id=c.worldPlaces[0].metadata.id,dungeon=c.dungeons[0].metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"World history fixture"),"Cannot save World history fixture");
    EditorWindowController editor;Interaction ui(editor,trace);require(editor.setCurrentCampaign(c),"Cannot bind World history fixture");
    auto &history=ProjectCommandManager::getInstance();WorldHistoryEvents events(c.metadata.id);worldOpenPlace(editor,ui,id);
    ui.replaceText(de?"SL-Notizen":"GM notes","COMMITTED_WORLD_NOTES","Workspace");
    require(editor.checkpointWorldRecoveryForLeave(),"Cannot checkpoint initial World edit");const auto originalBytes=worldCheckpointBytes(c);
    {WorldHistorySqlFault failure;ui.click(de?"Ort speichern":"Save place","Workspace");}
    require(!history.canUndo()&&events.saves==0&&editor.hasUnsavedChanges()&&worldCheckpointBytes(c)==originalBytes&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes.empty(),"Failed initial Save registered history or consumed RAM/recovery");
    ui.click(de?"Ort speichern":"Save place","Workspace");ui.frame(3);
    require(history.canUndo()&&!history.canRedo()&&events.saves==1&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="COMMITTED_WORLD_NOTES",
        "Successful Save retry did not register one committed World edit");
    require(editor.openWorkspaceEntity("Dungeon",dungeon),"Cannot open independent dungeon draft");ui.frame(3);
    ui.replaceText(de?"Beschreibung":"Description","UNFINISHED_DUNGEON_DESCRIPTION","Workspace");worldOpenPlace(editor,ui,id);
    ui.replaceText(de?"Beschreibung":"Description","UNFINISHED_WORLD_DESCRIPTION","Workspace");
    require(editor.checkpointWorldRecoveryForLeave(),"Cannot checkpoint unrelated same-entity World draft");
    ui.click(de?"Sitzung":"Session");const auto independentBytes=worldCheckpointBytes(c);
    require(independentBytes.find("UNFINISHED_WORLD_DESCRIPTION")!=std::string::npos,"Unrelated World draft checkpoint missing");
    {WorldHistorySqlFault failure;require(!editor.undoCommand(),"Injected undo persistence failure succeeded");}
    require(events.saves==1&&history.canUndo()&&!history.canRedo()&&worldCheckpointBytes(c)==independentBytes&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="COMMITTED_WORLD_NOTES","Failed undo changed SQL, stacks or unrelated recovery");
    require(editor.undoCommand()&&history.canRedo()&&!history.canUndo(),"Undo retry failed or duplicate initial registration remained");
    auto saved=readCampaign(c.metadata.id);
    require(events.saves==2&&worldRow(saved.worldPlaces,id).notes.empty()&&worldRow(saved.worldPlaces,id).description=="Original place description"&&
        worldRow(saved.dungeons,dungeon).description.empty()&&worldCheckpointBytes(c)==independentBytes&&editor.hasUnsavedChanges(),
        "Undo persisted/discarded unrelated World or dungeon drafts");
    {WorldHistorySqlFault failure;require(!editor.redoCommand(),"Injected redo persistence failure succeeded");}
    require(events.saves==2&&history.canRedo()&&!history.canUndo()&&worldCheckpointBytes(c)==independentBytes&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes.empty(),"Failed redo moved history or changed recovery/SQL");
    require(editor.redoCommand()&&events.saves==3&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="COMMITTED_WORLD_NOTES"&&
        worldCheckpointBytes(c)==independentBytes,"Redo retry lost committed notes or rewrote unrelated recovery");
    require(editor.openWorkspaceEntity("Dungeon",dungeon),"Cannot revisit retained dungeon draft");ui.frame(3);
    require(WorkspaceInteractionProbe::dungeonDraft(editor).description=="UNFINISHED_DUNGEON_DESCRIPTION","Undo/redo lost independent dungeon RAM");
    worldOpenPlace(editor,ui,id);require(WorkspaceInteractionProbe::placeDraft(editor).description=="UNFINISHED_WORLD_DESCRIPTION",
        "Undo/redo lost independently authored same-place description");
    ui.replaceText(de?"SL-Notizen":"GM notes","OVERLAPPING_UNFINISHED_NOTE","Workspace");
    require(editor.checkpointWorldRecoveryForLeave(),"Cannot checkpoint overlapping draft");ui.click(de?"Sitzung":"Session");
    const auto overlapping=worldCheckpointBytes(c);require(!editor.undoCommand()&&events.saves==3&&history.canUndo()&&
        worldCheckpointBytes(c)==overlapping&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="COMMITTED_WORLD_NOTES",
        "Undo overwrote an overlapping unfinished field or its checkpoint");
}
void worldHistoryRegistration(UiInteractionTrace &trace,bool de)
{
    std::cout<<"World history UI: deferred registration without replaying Save\n";
    auto c=worldUiFixture();const auto id=c.worldPlaces[0].metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"World busy history fixture"),"Cannot save busy history fixture");
    EditorWindowController editor;Interaction ui(editor,trace);require(editor.setCurrentCampaign(c),"Cannot bind busy history fixture");
    auto &history=ProjectCommandManager::getInstance();WorldHistoryEvents events(c.metadata.id);worldOpenPlace(editor,ui,id);
    ui.replaceText(de?"SL-Notizen":"GM notes","BUSY_COMMITTED_NOTES","Workspace");
    history.execute(std::make_unique<LambdaProjectCommand>("Outer busy command",[&]{
        ui.click(de?"Ort speichern":"Save place","Workspace");
        require(WorkspaceInteractionProbe::worldHistory(editor)->hasPendingRegistration(),"Busy registration dropped saved World patch");
    },[]{}));
    require(events.saves==1&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="BUSY_COMMITTED_NOTES"&&
        history.undoLabel()=="Outer busy command"&&WorkspaceInteractionProbe::worldHistory(editor)->hasPendingRegistration(),
        "Busy manager incorrectly acknowledged or repeated committed World save");
    require(!editor.undoCommand()&&history.undoLabel()=="Outer busy command"&&events.saves==1,
        "Undo silently skipped pending World registration to execute another command");
    worldRecoveryOpen(ui,de);
    require(ui.renderedText().find(de?"Gespeichert. Rückgängig ist noch nicht bereit.":"Saved. Undo is not ready yet.")!=std::string::npos,
        "Saved-but-unregistered World edit lacks truthful visible feedback");
    ui.click(de?"Rückgängig erneut versuchen":"Retry Undo");ui.frame(3);
    require(!WorkspaceInteractionProbe::worldHistory(editor)->hasPendingRegistration()&&history.canUndo()&&
        history.undoLabel()!="Outer busy command"&&events.saves==1&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="BUSY_COMMITTED_NOTES",
        "Explicit registration retry repeated Save or failed to register committed patch");
    require(editor.undoCommand()&&events.saves==2&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes.empty()&&
        history.undoLabel()=="Outer busy command","Registered World patch duplicated history or lost original value");
}

struct WorldHistorySubscription
{
    int id;
    explicit WorldHistorySubscription(ProjectEventBus::Handler callback):id(ProjectEventBus::getInstance().subscribe(std::move(callback))){}
    ~WorldHistorySubscription(){ProjectEventBus::getInstance().unsubscribe(id);}
};
void worldHistoryCallbacks(UiInteractionTrace &trace,bool de)
{
    std::cout<<"World history UI: post-save edits and same-ID callback revocation\n";
    for(int mode=0;mode<5;++mode) {
        auto c=worldUiFixture();const auto id=c.worldPlaces[0].metadata.id;
        require(ProjectSaveService::getInstance().saveCampaign(c,"World history callback fixture"),"Cannot save history callback fixture");
        EditorWindowController editor;Interaction ui(editor,trace);require(editor.setCurrentCampaign(c),"Cannot bind callback fixture");
        auto &history=ProjectCommandManager::getInstance();WorldHistoryEvents events(c.metadata.id);worldOpenPlace(editor,ui,id);
        ui.replaceText(de?"SL-Notizen":"GM notes","CALLBACK_COMMITTED_NOTE","Workspace");
        // A real nonmutating control ends InputText's active buffer before
        // testing synchronous save subscribers against the owned draft.
        worldRecoveryOpen(ui,de);worldRecoveryClose(ui);
        if(mode==2){ui.click(de?"Ort speichern":"Save place","Workspace");require(history.canUndo(),"Cannot seed undo reset fixture");}
        bool called=false,resetAccepted=false;
        WorldHistorySubscription subscription([&](const ProjectEvent &event){
            const bool signal=mode==3?event.type==ProjectEventType::EntityUpdated&&event.entityType=="command":
                event.type==ProjectEventType::CampaignSaved&&event.campaignId==c.metadata.id;
            if(called||!signal)return;called=true;
            if(mode==0){auto &live=WorkspaceInteractionProbe::liveCampaign(editor);live.partyReputation="LATE_LIVE_REPUTATION";
                WorkspaceInteractionProbe::lateWorldDescription(editor,"LATE_UNFINISHED_DESCRIPTION");}
            else if(mode==1)WorkspaceInteractionProbe::lateWorldNotes(editor,"LATE_OVERLAPPING_NOTE");
            else {auto replacement=readCampaign(c.metadata.id);replacement.name="REPLACEMENT_CONTEXT";
                resetAccepted=editor.setCurrentCampaign(std::move(replacement));}
        });
        if(mode==2){editor.undoCommand();require(called&&resetAccepted&&editor.currentCampaign().name=="REPLACEMENT_CONTEXT"&&
            worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes.empty()&&!history.canUndo()&&!history.canRedo()&&
            !WorkspaceInteractionProbe::worldHistory(editor)->hasPendingRegistration(),"Undo tail reinstalled retired same-ID context or history");continue;}
        const auto result=editor.saveWorldDraft(WorldInspectorDrafts::Scope::entity(id),"World callback save");
        require(called&&events.saves==1&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="CALLBACK_COMMITTED_NOTE",
            "History callback positive control did not cross exactly one durable Save");
        if(mode==0){require(result.applied()&&editor.currentCampaign().partyReputation=="LATE_LIVE_REPUTATION"&&
                WorkspaceInteractionProbe::placeDraft(editor).description=="LATE_UNFINISHED_DESCRIPTION"&&editor.hasUnsavedChanges()&&history.canUndo(),
                "World save overwrote unrelated subscriber live/draft changes");
            require(editor.checkpointWorldRecoveryForLeave()&&worldCheckpointBytes(c).find("LATE_UNFINISHED_DESCRIPTION")!=std::string::npos,
                "Subscriber-authored unrelated draft could not be recovered");
            require(editor.undoCommand()&&worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes.empty()&&
                editor.currentCampaign().partyReputation=="LATE_LIVE_REPUTATION"&&WorkspaceInteractionProbe::placeDraft(editor).description=="LATE_UNFINISHED_DESCRIPTION",
                "Undo replaced newer subscriber edits");}
        else if(mode==1)require(result.status==WorldInspector::MutationStatus::CommittedContextChanged&&
            WorkspaceInteractionProbe::placeDraft(editor).notes=="LATE_OVERLAPPING_NOTE"&&editor.hasUnsavedChanges(),
            "Touched-field callback was misreported as unsaved or consumed its newer draft");
        else require(resetAccepted&&result.status==WorldInspector::MutationStatus::CommittedContextChanged&&
            editor.currentCampaign().name=="REPLACEMENT_CONTEXT"&&!history.canUndo()&&!history.canRedo()&&
            !WorkspaceInteractionProbe::worldHistory(editor)->hasPendingRegistration(),
            mode==3?"Post-registration notification resurrected retired history":"Initial Save callback resurrected retired same-ID context or registered obsolete history");
    }
}

void worldHistoryUiCases(UiInteractionTrace &trace,bool de)
{worldHistoryFailureAndDrafts(trace,de);worldHistoryRegistration(trace,de);worldHistoryCallbacks(trace,de);}
