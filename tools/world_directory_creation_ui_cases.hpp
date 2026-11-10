// Included by the production World UI cases. All writes use the invocation's
// isolated workflow database; creation/edit/save goes through real controls.
std::size_t worldCreatedCount(const Campaign &c,int kind)
{
    switch(kind){case 0:return c.worldPlaces.size();case 1:return c.nations.size();case 2:return c.shops.size();
        case 3:return c.dungeons.size();default:return c.randomTables.size();}
}
std::vector<EntityId> worldCreatedIds(const Campaign &c,int kind)
{
    const auto ids=[](const auto &rows){std::vector<EntityId> result;for(const auto &row:rows)result.push_back(row.metadata.id);return result;};
    switch(kind){case 0:return ids(c.worldPlaces);case 1:return ids(c.nations);case 2:return ids(c.shops);
        case 3:return ids(c.dungeons);default:return ids(c.randomTables);}
}
std::string worldCreatedName(const Campaign &c,int kind,const EntityId &id)
{
    const auto name=[&](const auto &rows){for(const auto &v:rows)if(v.metadata.id==id)return v.name;return std::string{};};
    switch(kind){case 0:return name(c.worldPlaces);case 1:return name(c.nations);case 2:return name(c.shops);
        case 3:return name(c.dungeons);default:return name(c.randomTables);}
}
void worldNewMenu(Interaction &ui,bool de,int kind)
{
    ui.click(kind==4?(de?"Neuer Kompendiumseintrag":"New compendium entry"):(de?"Neuer Welteintrag":"New world entry"),"Workspace");
    const char *en[]={"City or place","Nation","Shop","Dungeon structure","Random table"};
    const char *ge[]={"Stadt oder Ort","Nation","Händler","Dungeonstruktur","Zufallstabelle"};
    ui.click(de?ge[kind]:en[kind]);
}
void worldDirectoryCreation(UiInteractionTrace &trace,bool de)
{
    for(int kind=0;kind<5;++kind) {
        EditorWindowController editor;Interaction ui(editor,trace);auto c=worldUiFixture();
        require(ProjectSaveService::getInstance().saveCampaign(c,"World creation fixture"),"Creation fixture save failed");
        editor.setCurrentCampaign(c);
        const auto oldId=kind==4?c.randomTables[0].metadata.id:c.worldPlaces[0].metadata.id;
        require(editor.openWorkspaceEntity(kind==4?"random_table":"world_place",oldId),"Creation fixture inspector did not open");ui.frame(3);
        if(kind==4)ui.replaceText("Name","Retained old random table","Workspace");
        const char *filter=kind==4?"Table canary":"Place canary";
        ui.replaceText("##workspace-filter",filter,"Workspace");
        const auto count=worldCreatedCount(c,kind);
        SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
        sql.exec("CREATE TRIGGER reject_world_create BEFORE INSERT ON world_places BEGIN SELECT RAISE(ABORT, 'injected creation failure'); END;");
        worldNewMenu(ui,de,kind);
        if(kind==2) {
            ui.replaceText(de?"Händlername":"Shop name","Named created shop");
            ui.replaceText(de?"Händlerperson":"Merchant","Named merchant");
            ui.click(de?"Händler erstellen":"Create shop");
        }
        require(worldCreatedCount(readCampaign(c.metadata.id),kind)==count&&WorkspaceInteractionProbe::worldSelection(editor)==oldId&&
            WorkspaceInteractionProbe::worldFilter(editor)==filter,"Failed creation changed selection, filter or persisted collection");
        if(kind==4)require(editor.hasUnsavedChanges()&&WorkspaceInteractionProbe::tableDraft(editor).name=="Retained old random table"&&
            worldRow(readCampaign(c.metadata.id).randomTables,c.randomTables[0].metadata.id).name=="Table canary","Failed creation discarded or committed previous random-table draft");
        if(kind==2)require(ImGui::GetTopMostPopupModal()&&WorkspaceInteractionProbe::newShopName(editor)=="Named created shop",
            "Failed named-shop creation lost its form");
        sql.exec("DROP TRIGGER reject_world_create;");
        if(kind==2)ui.click(de?"Händler erstellen":"Create shop");else worldNewMenu(ui,de,kind);
        ui.frame(3);
        const auto created=readCampaign(c.metadata.id);const auto beforeIds=worldCreatedIds(c,kind);std::vector<EntityId> added;
        for(const auto &candidate:worldCreatedIds(created,kind))if(std::find(beforeIds.begin(),beforeIds.end(),candidate)==beforeIds.end())added.push_back(candidate);
        require(added.size()==1,"Creation did not produce exactly one new stable ID");const auto id=added.front();
        require(worldCreatedCount(created,kind)==count+1&&id!=oldId&&WorkspaceInteractionProbe::worldSelection(editor)==id&&
            WorkspaceInteractionProbe::worldFilter(editor).empty(),"Created entry was overwritten by same-frame old directory selection");
        if(kind==2)require(worldRow(created.shops,id).merchantName=="Named merchant","Named merchant not persisted");
        if(kind==4)require(worldRow(created.randomTables,c.randomTables[0].metadata.id).name=="Retained old random table","Creation lost previous random-table draft on retry");
        const auto name="Edited created entry "+std::to_string(kind);ui.replaceText("Name",name.c_str(),"Workspace");
        const char *saveEn[]={"Save place","Save nation","Save details","Save structure","Save table"};
        const char *saveDe[]={"Ort speichern","Nation speichern","Details speichern","Struktur speichern","Tabelle speichern"};
        ui.click(de?saveDe[kind]:saveEn[kind],"Workspace");
        const auto saved=readCampaign(c.metadata.id);
        require(worldCreatedName(saved,kind,id)==name&&worldRow(saved.worldPlaces,c.worldPlaces[0].metadata.id).name=="Place canary"&&
            worldRow(saved.nations,c.nations[0].metadata.id).name=="Nation canary"&&worldRow(saved.shops,c.shops[0].metadata.id).name=="Shop canary"&&
            worldRow(saved.dungeons,c.dungeons[0].metadata.id).name=="Dungeon canary"&&!editor.hasUnsavedChanges(),
            "New entry edit/save targeted a canary or retained unexpected draft");
        std::cout<<"World creation route "<<kind<<" selected and saved the new record after failed-save retry\n";
    }
    // An explicit discard ends unfinished named creation too; it is separate
    // from retained authored entity drafts and is never implicitly persisted.
    EditorWindowController editor;Interaction ui(editor,trace);auto c=worldUiFixture();
    require(ProjectSaveService::getInstance().saveCampaign(c,"named creation discard"),"Named discard fixture failed");
    editor.setCurrentCampaign(c);worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);worldNewMenu(ui,de,2);
    ui.replaceText(de?"Händlername":"Shop name","Unfinished shop creation");ui.click(de?"Abbrechen":"Cancel");
    editor.discardUnsavedChanges("discard unfinished named shop");worldNewMenu(ui,de,2);
    require(WorkspaceInteractionProbe::newShopName(editor).empty()&&readCampaign(c.metadata.id).shops.size()==1,"Explicit discard revived unfinished named creation");
    ui.click(de?"Abbrechen":"Cancel");
}
