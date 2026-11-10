#pragma once
void worldRecoveryUnavailableTarget(UiInteractionTrace &trace,bool de)
{
    for(const bool archived:{false,true}) {
        auto c=worldUiFixture();const auto base=c.worldPlaces[0];auto edited=base;
        edited.notes="RECOVERED_MISSING_PRIVATE_NOTES";edited.description="Retained missing place prose";
        WorldInspectorDrafts::Store drafts;drafts.retain(WorldInspectorDrafts::Draft<WorldPlaceModel>{base,edited});
        if(archived)c.worldPlaces[0].metadata.tags.push_back("archived");else c.worldPlaces.erase(c.worldPlaces.begin());
        require(ProjectSaveService::getInstance().saveCampaign(c,"unavailable recovered target")&&WorldDraftRecovery::save(c,drafts).success,
            "Cannot seed missing/archived recovery");
        const auto bytes=worldCheckpointBytes(c);
        EditorWindowController editor;Interaction ui(editor,trace);editor.setCurrentCampaign(c);
        worldOpenPlace(editor,ui,c.worldPlaces.back().metadata.id);worldRecoveryRestore(ui,de);
        require(editor.hasUnsavedChanges()&&ui.renderedText().find(de?"Ziel fehlt oder ist archiviert":"Target missing or archived")!=std::string::npos,
            "Unavailable recovered target was hidden or silently dropped");
        WorldClipboardMock clipboard;ui.click(de?"Entwurf kopieren":"Copy retained draft");
        require(clipboard.text.find(edited.notes)!=std::string::npos&&clipboard.text.find(base.metadata.id)!=std::string::npos,
            "Missing recovered draft Copy omitted its exact identity/text");
        WorldCheckpointBlock blocked(c);ui.click(de?"Diesen Entwurf verwerfen":"Discard this draft");
        require(!ImGui::GetCurrentContext()->OpenPopupStack.empty()&&editor.hasUnsavedChanges()&&
            WorkspaceInteractionProbe::worldRecovery(editor).lastPayload.find(edited.notes)!=std::string::npos,
            "Failed per-entry discard consumed recovered RAM or closed review");
        blocked.release();require(worldCheckpointBytes(c)==bytes,"Failed discard changed last-good checkpoint bytes");
        ui.click(de?"Diesen Entwurf verwerfen":"Discard this draft");ui.frame(3);
        require(!editor.hasUnsavedChanges()&&!std::filesystem::exists(worldCheckpointPath(c))&&
            ImGui::GetCurrentContext()->OpenPopupStack.empty(),"Explicit missing-draft discard did not cleanly acknowledge storage");
        const auto saved=readCampaign(c.metadata.id);const auto found=std::find_if(saved.worldPlaces.begin(),saved.worldPlaces.end(),
            [&](const auto &place){return place.metadata.id==base.metadata.id;});
        require(archived?(found!=saved.worldPlaces.end()&&
            std::find(found->metadata.tags.begin(),found->metadata.tags.end(),"archived")!=found->metadata.tags.end()&&found->notes==base.notes):found==saved.worldPlaces.end(),
            "Recovery resurrected or changed the unavailable canonical record");
    }
}

void worldRecoveryScopes(UiInteractionTrace &trace,bool de)
{
    auto original=worldUiFixture();auto live=original;live.worldPlaces[0].description="LATER_UNRELATED_CANONICAL_FIELD";
    WorldInspectorDrafts::Store drafts;
    auto place=original.worldPlaces[0];place.notes="RECOVERED_PRIVATE_PLACE_NOTES";
    auto nation=original.nations[0];nation.government="RECOVERED_GOVERNMENT";
    auto shop=original.shops[0];shop.description="RECOVERED_SHOP_PROSE";shop.stock[0].quantity=7;
    drafts.retain(WorldInspectorDrafts::Draft<WorldPlaceModel>{original.worldPlaces[0],place});
    drafts.retain(WorldInspectorDrafts::Draft<NationModel>{original.nations[0],nation});
    drafts.retain(WorldInspectorDrafts::Draft<ShopModel>{original.shops[0],shop});
    require(ProjectSaveService::getInstance().saveCampaign(live,"World scoped recovery fixture")&&WorldDraftRecovery::save(live,drafts).success,
        "Cannot seed typed scoped recovery");
    EditorWindowController editor;Interaction ui(editor,trace);editor.setCurrentCampaign(live);worldOpenPlace(editor,ui,place.metadata.id);
    worldRecoveryRestore(ui,de);worldRecoveryClose(ui);worldOpenPlace(editor,ui,place.metadata.id);
    ui.click(de?"Ort speichern":"Save place","Workspace");
    auto pending=WorldDraftRecovery::read(live);const auto saved=readCampaign(live.metadata.id);
    require(pending.status==WorldDraftRecovery::ReadStatus::Ready&&pending.drafts.places.empty()&&pending.drafts.nations.size()==1&&pending.drafts.shops.size()==1&&
        worldRow(saved.worldPlaces,place.metadata.id).notes==place.notes&&worldRow(saved.worldPlaces,place.metadata.id).description=="LATER_UNRELATED_CANONICAL_FIELD",
        "Scoped recovered save consumed other drafts or replaced an unrelated live field");
    require(editor.openWorkspaceEntity("Shop",shop.metadata.id),"Recovered shop did not open");ui.frame(3);
    WorldCheckpointBlock block(live);
    ui.click(de?"Gruppe kauft":"Party buys","Workspace");
    require(ImGui::GetTopMostPopupModal()&&worldRow(readCampaign(live.metadata.id).shops,shop.metadata.id).stock[0].quantity==7&&
        !editor.worldRecoveryError().empty(),"Stock commit cleanup failure blocked faithful transaction review");
    ui.click(de?"Kauf erfassen":"Record purchase");
    require(!ImGui::GetTopMostPopupModal()&&worldRow(readCampaign(live.metadata.id).shops,shop.metadata.id).stock[0].quantity==6&&
        WorldInspector::partyQuantity(readCampaign(live.metadata.id),shop.stock[0].itemId)==1,
        "Committed purchase was hidden or repeated after checkpoint failure");
    block.release();require(WorkspaceInteractionProbe::worldTick(editor,2),"Scoped cleanup retry failed");
    pending=WorldDraftRecovery::read(live);
    require(pending.status==WorldDraftRecovery::ReadStatus::Ready&&pending.drafts.shops.at(shop.metadata.id).edited.description==shop.description&&
        pending.drafts.shops.at(shop.metadata.id).base.stock[0].quantity==pending.drafts.shops.at(shop.metadata.id).edited.stock[0].quantity&&
        worldRow(readCampaign(live.metadata.id).shops,shop.metadata.id).stock[0].quantity==6&&
        WorldInspector::partyQuantity(readCampaign(live.metadata.id),shop.stock[0].itemId)==1,"Cleanup retry replayed transfer or lost independent shop prose");
    ui.click(de?"Details speichern":"Save details","Workspace");
    pending=WorldDraftRecovery::read(live);require(pending.drafts.shops.empty()&&pending.drafts.nations.size()==1,"Shop detail save consumed unrelated nation recovery");
    auto &current=WorkspaceInteractionProbe::liveCampaign(editor);current.nations[0].government="LATER_CONFLICTING_GOVERNMENT";
    require(ProjectSaveService::getInstance().saveCampaign(current,"later nation conflict"),"Cannot persist later conflict fixture");
    require(!editor.saveCurrentCampaign("refuse stale recovered baseline")&&editor.hasUnsavedChanges()&&
        worldRow(readCampaign(live.metadata.id).nations,nation.metadata.id).government=="LATER_CONFLICTING_GOVERNMENT",
        "Restore silently rebased an authored conflict");
    ui.click(de?"Sitzung":"Session");require(editor.discardUnsavedChanges("discard stale recovered nation"),"Cannot explicitly discard conflicted recovery");
}

void worldRecoveryInvalidAndRoots(UiInteractionTrace &trace,bool de)
{
    auto c=worldUiFixture();require(ProjectSaveService::getInstance().saveCampaign(c,"invalid recovery fixture"),"Cannot save invalid checkpoint fixture");
    const auto path=worldCheckpointPath(c);std::filesystem::create_directories(path.parent_path());
    const std::string invalid("BAD\0PRIVATE_RAW_BYTES",21);{std::ofstream file(path,std::ios::binary);file.write(invalid.data(),std::streamsize(invalid.size()));}
    EditorWindowController editor;Interaction ui(editor,trace);editor.setCurrentCampaign(c);worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);
    require(WorkspaceInteractionProbe::worldRecovery(editor).pending&&WorldDraftRecovery::read(c).status==WorldDraftRecovery::ReadStatus::Invalid&&
        trace.find("Name").empty(),"Malformed checkpoint did not block authored input");
    require(!editor.hasUnsavedChanges()&&editor.checkpointWorldRecoveryForLeave()&&worldCheckpointBytes(c)==invalid,
        "Untouched invalid checkpoint trapped ordinary leave or changed its bytes");
    WorkspaceInteractionProbe::liveCampaign(editor).partyReputation="UNRELATED_SAVE_WITH_INVALID_WORLD";
    ProjectSaveService::getInstance().markDirty("unrelated save with invalid checkpoint");
    require(editor.saveCurrentCampaign("preserve invalid checkpoint during unrelated Save")&&
        readCampaign(c.metadata.id).partyReputation=="UNRELATED_SAVE_WITH_INVALID_WORLD"&&worldCheckpointBytes(c)==invalid,
        "Unrelated Save changed malformed checkpoint bytes");
    ProjectSaveService::getInstance().markDirty("unrelated discard with invalid checkpoint");
    require(editor.discardUnsavedChanges("preserve invalid checkpoint during unrelated Discard")&&
        WorkspaceInteractionProbe::worldRecovery(editor).pending&&worldCheckpointBytes(c)==invalid,
        "Global Discard silently removed malformed checkpoint history");
    worldRecoveryOpen(ui,de);WorldClipboardMock clipboard;ui.click(de?"Sicherung kopieren":"Copy checkpoint");
    require(clipboard.text==WorldDraftRecovery::copyCheckpoint(WorldDraftRecovery::read(c)).text&&
        trace.find(de?"Zur Prüfung wiederherstellen":"Restore for review").empty(),"Invalid raw Copy lost bytes or enabled Restore");
    WorldCheckpointBlock block(c);ui.click(de?"Gesicherte Entwürfe verwerfen":"Discard saved drafts");
    require(!ImGui::GetCurrentContext()->OpenPopupStack.empty()&&WorkspaceInteractionProbe::worldRecovery(editor).pending&&
        !editor.worldRecoveryError().empty(),"Failed pending discard closed popup or consumed original bytes");
    block.release();ui.click(de?"Gesicherte Entwürfe verwerfen":"Discard saved drafts");ui.frame(3);
    require(!std::filesystem::exists(path)&&!WorkspaceInteractionProbe::worldRecovery(editor).pending&&
        ImGui::GetCurrentContext()->OpenPopupStack.empty(),"Explicit invalid-checkpoint discard failed");
    worldOpenPlace(editor,ui,c.worldPlaces[0].metadata.id);ui.replaceText(de?"SL-Notizen":"GM notes","OLD_ROOT_LATEST_CANARY","Workspace");
    const auto oldRoot=ApplicationPaths::writableRoot();const auto alternate=oldRoot/("alternate-root-"+c.metadata.id);
    std::filesystem::create_directory(alternate);
    {
        WorldDataRootOverride otherRoot(alternate);auto replacement=worldUiFixture();
        require(!editor.checkpointWorldRecoveryForLeave()&&!editor.setCurrentCampaign(replacement)&&!editor.discardUnsavedChanges("wrong root discard")&&
            editor.currentCampaign().metadata.id==c.metadata.id&&!std::filesystem::exists(worldCheckpointPath(c))&&
            !std::filesystem::exists(worldCheckpointPath(replacement)),"Old-owner text was written or discarded through a different data root");
    }
    require(editor.checkpointWorldRecoveryForLeave()&&worldCheckpointBytes(c).find("OLD_ROOT_LATEST_CANARY")!=std::string::npos,
        "Restoring original root did not recover latest retained bytes");
    ui.click(de?"Sitzung":"Session");auto replacement=worldUiFixture();
    require(editor.setCurrentCampaign(replacement)&&editor.currentCampaign().metadata.id==replacement.metadata.id&&
        !WorkspaceInteractionProbe::worldRecovery(editor).pending&&WorldDraftRecovery::read(c).status==WorldDraftRecovery::ReadStatus::Ready,
        "Campaign replacement lost old checkpoint or attached it to a different campaign");
}
