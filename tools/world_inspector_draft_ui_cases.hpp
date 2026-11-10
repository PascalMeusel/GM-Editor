// Production CPU ImGui input + real isolated SQLite. No Application/native
// window, texture assets, clipboard operations or audio playback.
template<class T> const T &worldRow(const std::vector<T> &rows,const EntityId &id)
{
    const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto &row){return row.metadata.id==id;});
    require(found!=rows.end(),"Persisted World fixture record missing: "+id);return *found;
}
Campaign worldUiFixture()
{
    Campaign c;c.name="Synthetic World inspector fixture "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    WorldPlaceModel p;p.name="Place canary";p.type=WorldPlaceType::City;p.description="Original place description";
    WorldPlaceModel other;other.name="Other place";other.type=WorldPlaceType::City;c.worldPlaces={p,other};
    NationModel n;n.name="Nation canary";NationTerritoryModel t;t.name="Territory canary";t.polygon={{.2,.3},{.4,.5}};
    n.territories.push_back(t);c.nations.push_back(n);
    ItemModel item;item.name="Generated stock item";c.customItems.push_back(item);
    ShopModel shop;shop.name="Shop canary";shop.stock.emplace_back(item.metadata.id,5);c.shops.push_back(shop);
    DungeonModel dungeon;dungeon.name="Dungeon canary";c.dungeons.push_back(dungeon);
    RandomTableModel table;table.name="Table canary";RandomTableEntryModel entry;entry.result="Generated result";table.entries.push_back(entry);c.randomTables.push_back(table);
    return c;
}
void worldOpenPlace(EditorWindowController &editor,Interaction &ui,const EntityId &id)
{
    require(editor.openWorkspaceEntity("world_place",id),"Synthetic World place did not open");ui.frame(3);
}
void worldPlaceType(Interaction &ui,UiInteractionTrace &trace,bool de)
{
    // Combo submits native geometry without ITEM_INFO. Derive its ID from the
    // real directory table scope and verify that scope against observed Name.
    const auto name=ui.locate("Name");auto *window=ImGui::FindWindowByName(name.window.c_str());
    require(window!=nullptr,"World field window missing");
    const auto table=ImHashStr("IntegratedWorkspaceDirectory",0,window->ID);
    require(ImHashStr("Name",0,table)==name.id,"World field ID scope differs from production table");
    const auto id=ImHashStr(de?"Ortstyp":"Place type",0,table);
    for(int attempt=0;attempt<3;++attempt) {
        const auto found=trace.items().find(id);require(found!=trace.items().end()&&found->second.hasRectangle,"Place type combo geometry missing");
        const auto &item=found->second;const ImVec2 point((item.minimum.x+item.maximum.x)*.5f,(item.minimum.y+item.maximum.y)*.5f);
        require(window->InnerClipRect.Contains(point)&&item.minimum.x>=window->InnerClipRect.Min.x-1&&item.maximum.x<=window->InnerClipRect.Max.x+1,
            "Place type combo is clipped");
        ImGui::GetIO().AddMousePosEvent(point.x,point.y);ui.frame();
        if(ImGui::GetCurrentContext()->HoveredId==id){ImGui::GetIO().AddMouseButtonEvent(0,true);ui.frame();ImGui::GetIO().AddMouseButtonEvent(0,false);ui.frame(2);ui.click(de?"Schauplatz":"Site");return;}
    }
    throw std::runtime_error("Could not hover exact production Place type combo");
}
void worldPlaceRetention(UiInteractionTrace &trace,bool de)
{
    EditorWindowController editor;Interaction ui(editor,trace);auto c=worldUiFixture();
    require(ProjectSaveService::getInstance().saveCampaign(c,"World UI fixture"),"World fixture save failed");
    editor.setCurrentCampaign(c);worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    ui.replaceText("Name","Authored place", "Workspace");worldPlaceType(ui,trace,de);
    ui.replaceText(de?"Beschreibung":"Description","PRIVATE_WORLD_DESCRIPTION", "Workspace");
    require(editor.hasUnsavedChanges(),"Place edits absent from global pending state");
    const auto dirtyText=ui.renderedText();
    require(dirtyText.find(de?"Entwurf noch nicht gespeichert":"Draft not yet saved")!=std::string::npos&&
        dirtyText.find(de?"Ungespeicherte Änderungen":"Unsaved changes")!=std::string::npos,"World header/footer falsely reported saved");
    ui.clickDirectoryRow(std::string("Other place\n")+(de?"Stadt":"City"));
    ui.clickDirectoryRow(std::string("Place canary\n")+(de?"Stadt":"City"));
    require(WorkspaceInteractionProbe::placeDraft(editor).name=="Authored place"&&WorkspaceInteractionProbe::placeDraft(editor).description=="PRIVATE_WORLD_DESCRIPTION"&&
        WorkspaceInteractionProbe::placeDraft(editor).type==WorldPlaceType::Site,"Switching rows lost authored place fields");
    ui.click(de?"Sitzung":"Session");ui.frame(2);worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    require(editor.hasUnsavedChanges()&&WorkspaceInteractionProbe::placeDraft(editor).description=="PRIVATE_WORLD_DESCRIPTION",
        "Leaving World discarded authored fields");
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_world_ui BEFORE INSERT ON world_places BEGIN SELECT RAISE(ABORT, 'injected World UI failure'); END;");
    require(!editor.saveCurrentCampaign("World Save and Continue")&&editor.hasUnsavedChanges()&&!editor.worldDraftSaveError().empty()&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,c.worldPlaces[0].metadata.id).name=="Place canary","Failed global World save lost draft or changed SQL");
    sql.exec("DROP TRIGGER reject_world_ui;");
    ui.click(de?"Ort speichern":"Save place","Workspace");
    const auto saved=readCampaign(c.metadata.id);
    const auto &place=worldRow(saved.worldPlaces,c.worldPlaces[0].metadata.id);
    require(place.name=="Authored place"&&place.type==WorldPlaceType::Site&&place.description=="PRIVATE_WORLD_DESCRIPTION"&&
        worldRow(saved.worldPlaces,c.worldPlaces[1].metadata.id).name=="Other place"&&!editor.hasUnsavedChanges()&&editor.worldDraftSaveError().empty(),"Retry failed or changed the wrong place");
    ui.replaceText("Name","Discard this place draft","Workspace");
    // Application's Discard is chosen from a leave modal after the field loses
    // focus. Use real nonmutating navigation for the same blur boundary here;
    // resetting a user buffer while InputText is active is not that workflow.
    ui.click(de?"Sitzung":"Session");
    editor.discardUnsavedChanges("explicit World discard");
    require(!editor.hasUnsavedChanges(),"Explicit discard did not clear pending World state before reopening");
    worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    require(!editor.hasUnsavedChanges(),"World reopening after explicit discard became dirty");
    require(WorkspaceInteractionProbe::placeDraft(editor).name=="Authored place",
        "World reopening after discard loaded name='"+WorkspaceInteractionProbe::placeDraft(editor).name+
        "' selected="+WorkspaceInteractionProbe::worldSelection(editor));
    ui.replaceText("Name","Do not resurrect on reload","Workspace");ui.click(de?"Sitzung":"Session");editor.setCurrentCampaign(saved);
    worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    require(!editor.hasUnsavedChanges()&&WorkspaceInteractionProbe::worldRecovery(editor).pending&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,c.worldPlaces[0].metadata.id).name=="Authored place","Same-ID reload auto-restored or lost its uncommitted checkpoint");
    ui.click(de?"Weltentwürfe prüfen":"Review World drafts","Workspace");
    ui.click(de?"Gesicherte Entwürfe verwerfen":"Discard saved drafts");ui.frame(3);
    worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    require(!editor.hasUnsavedChanges()&&WorkspaceInteractionProbe::placeDraft(editor).name=="Authored place","Checked recovery discard did not preserve canonical World text");
    ui.replaceText("Name","Do not leak across campaigns","Workspace");ui.click(de?"Sitzung":"Session");auto replacement=worldUiFixture();
    require(ProjectSaveService::getInstance().saveCampaign(replacement,"replacement World campaign"),"Replacement fixture failed");
    editor.setCurrentCampaign(replacement);worldOpenPlace(editor,ui,replacement.worldPlaces[0].metadata.id);
    require(!editor.hasUnsavedChanges()&&WorkspaceInteractionProbe::placeDraft(editor).name=="Place canary",
        "Different campaign retained previous campaign World draft");
}
#include "world_draft_review_ui_cases.hpp"

void worldNationAndMissing(UiInteractionTrace &trace,bool de)
{
    EditorWindowController editor;Interaction ui(editor,trace);auto c=worldUiFixture();
    require(ProjectSaveService::getInstance().saveCampaign(c,"World nation fixture"),"Nation fixture save failed");
    editor.setCurrentCampaign(c);worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    ui.replaceText(de?"SL-Notizen":"GM notes","Retained place notes","Workspace");
    ui.clickDirectoryRow("Nation canary\nNation");
    ui.replaceText(de?"Regierung":"Government","Authored government","Workspace");
    ui.replaceText(de?"Gebietsname":"Territory name","Authored territory","Workspace");
    ui.click(de?"Gebiet hinzufügen":"Add territory","Workspace");ui.frame(3);
    require(editor.hasUnsavedChanges()&&WorkspaceInteractionProbe::nationDraft(editor).government=="Authored government"&&
        WorkspaceInteractionProbe::nationDraft(editor).territories[0].name=="Authored territory"&&
        worldRow(readCampaign(c.metadata.id).nations,c.nations[0].metadata.id).government.empty(),"Territory action discarded or implicitly saved independent authored text");
    require(editor.saveCurrentCampaign("all World drafts"),"World multi-record Save failed");
    const auto saved=readCampaign(c.metadata.id);
    const auto &nation=worldRow(saved.nations,c.nations[0].metadata.id);
    require(worldRow(saved.worldPlaces,c.worldPlaces[0].metadata.id).notes=="Retained place notes"&&nation.government=="Authored government"&&nation.territories.size()==2&&
        worldRow(nation.territories,c.nations[0].territories[0].metadata.id).name=="Authored territory","Global World batch omitted a retained record or new territory");
    worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    ui.replaceText(de?"SL-Notizen":"GM notes","MISSING_TARGET_RECOVERY_TEXT","Workspace");
    auto &live=WorkspaceInteractionProbe::liveCampaign(editor);
    live.worldPlaces.erase(std::find_if(live.worldPlaces.begin(),live.worldPlaces.end(),[&](const auto &p){return p.metadata.id==c.worldPlaces[0].metadata.id;}));
    require(ProjectSaveService::getInstance().saveCampaign(live,"simulate external target deletion"),"External deletion fixture failed");
    ui.frame(3);
    require(!editor.saveCurrentCampaign("reject deleted World target"),"Save accepted a deleted World draft target");
    require(editor.hasUnsavedChanges(),"Rejected deleted World draft became falsely saved");
    require(!editor.worldDraftSaveError().empty(),"Rejected deleted World draft lost its dedicated save error");
    const auto popup=worldReviewPopup(ui,de);
    ui.click(de?"Erhaltene Werte":"Retained values",popup.c_str());
    require(ui.renderedText().find("MISSING_TARGET_RECOVERY_TEXT")!=std::string::npos,"Deleted World text is not visibly recoverable");
    ui.click(de?"Diesen Entwurf verwerfen":"Discard this draft",popup.c_str());
    require(!editor.hasUnsavedChanges()&&editor.worldDraftSaveError().empty()&&readCampaign(c.metadata.id).worldPlaces.size()==1&&
        ImGui::GetCurrentContext()->OpenPopupStack.empty(),
        "Discard missing draft did not unblock save or resurrected data");
}
void worldShopReceipt(UiInteractionTrace &trace,bool de)
{
    EditorWindowController editor;Interaction ui(editor,trace);auto c=worldUiFixture();
    require(ProjectSaveService::getInstance().saveCampaign(c,"World shop fixture"),"Shop fixture save failed");
    editor.setCurrentCampaign(c);require(editor.openWorkspaceEntity("Shop",c.shops[0].metadata.id),"Shop unavailable");ui.frame(3);
    ui.replaceText(de?"Beschreibung":"Description","Retained merchant prose","Workspace");
    ui.replaceText(de?"Menge":"Quantity","7","Workspace");
    ui.replaceText(de?"Transfermenge":"Transfer quantity","2","Workspace");
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_world_stock BEFORE INSERT ON world_places BEGIN SELECT RAISE(ABORT, 'injected stock pre-review failure'); END;");
    ui.click(de?"Gruppe kauft":"Party buys","Workspace");
    require(!ImGui::GetTopMostPopupModal()&&worldRow(readCampaign(c.metadata.id).shops,c.shops[0].metadata.id).stock[0].quantity==5&&
        WorkspaceInteractionProbe::shopDraft(editor).stock[0].quantity==7&&editor.hasUnsavedChanges(),"Failed stock save opened review or lost the edited quantity");
    sql.exec("DROP TRIGGER reject_world_stock;");
    ui.click(de?"Gruppe kauft":"Party buys","Workspace");
    require(ImGui::GetTopMostPopupModal()!=nullptr&&worldRow(readCampaign(c.metadata.id).shops,c.shops[0].metadata.id).stock[0].quantity==7&&editor.hasUnsavedChanges(),
        "Purchase review did not save its quantity or consumed unrelated merchant prose");
    const auto confirmation=ui.renderedText();
    require(confirmation.find("7")!=std::string::npos&&confirmation.find("5")!=std::string::npos,"Purchase review omitted reviewed before/after quantity");
    auto &live=WorkspaceInteractionProbe::liveCampaign(editor);live.shops[0].stock[0].quantity=8;
    require(ProjectSaveService::getInstance().saveCampaign(live,"simulate changed stock"),"Live stock fixture failed");
    ui.click(de?"Kauf erfassen":"Record purchase");
    require(ImGui::GetTopMostPopupModal()!=nullptr&&WorldInspector::partyQuantity(editor.currentCampaign(),c.customItems[0].metadata.id)==0&&
        worldRow(readCampaign(c.metadata.id).shops,c.shops[0].metadata.id).stock[0].quantity==8,"Stale reviewed transaction transferred items");
    ui.click(de?"Abbrechen":"Cancel");
    ui.click(de?"Gruppe kauft":"Party buys","Workspace");
    sql.exec("CREATE TRIGGER reject_world_purchase BEFORE INSERT ON world_places BEGIN SELECT RAISE(ABORT, 'injected World purchase failure'); END;");
    ui.click(de?"Kauf erfassen":"Record purchase");
    require(ImGui::GetTopMostPopupModal()!=nullptr&&worldRow(readCampaign(c.metadata.id).shops,c.shops[0].metadata.id).stock[0].quantity==8,"Failed transfer changed SQL or closed review");
    sql.exec("DROP TRIGGER reject_world_purchase;");ui.click(de?"Kauf erfassen":"Record purchase");
    require(!ImGui::GetTopMostPopupModal()&&worldRow(readCampaign(c.metadata.id).shops,c.shops[0].metadata.id).stock[0].quantity==6&&
        WorldInspector::partyQuantity(readCampaign(c.metadata.id),c.customItems[0].metadata.id)==2&&editor.hasUnsavedChanges(),"Transfer retry failed, doubled or discarded other draft");
    ui.click(de?"Details speichern":"Save details","Workspace");
    require(worldRow(readCampaign(c.metadata.id).shops,c.shops[0].metadata.id).description=="Retained merchant prose"&&!editor.hasUnsavedChanges(),"Details save overwrote transfer or lost text");
    ui.replaceText(de?"Transfermenge":"Transfer quantity","1","Workspace");
    require(!editor.hasUnsavedChanges(),"Transfer input alone marked canonical edits pending");
}

#include "world_directory_creation_ui_cases.hpp"

void worldInspectorDraftCases(UiInteractionTrace &trace,bool de)
{
    worldPlaceRetention(trace,de);worldDraftReviewLayout(trace,de);worldNationAndMissing(trace,de);worldShopReceipt(trace,de);worldDirectoryCreation(trace,de);
    std::cout<<"World production UI: place/nation/shop retention, status, SQLite failure/retry, missing-target recovery, scoped stock review and five creation routes passed\n";
}
