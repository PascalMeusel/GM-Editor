#pragma once
#include "world_draft_recovery_ui_helpers.hpp"
#include "world_recovery_attention_ui_helpers.hpp"

void worldRecoveryRestart(UiInteractionTrace &trace,bool de)
{
    auto c=worldUiFixture();const auto id=c.worldPlaces[0].metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"World recovery restart fixture"),"Cannot save recovery fixture");
    const auto canonical=worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes;
    {
        EditorWindowController writer;Interaction ui(writer,trace);require(writer.setCurrentCampaign(c),"Writer campaign refused");
        worldOpenPlace(writer,ui,id);ui.replaceText(de?"SL-Notizen":"GM notes","PRIVATE_RESTART_WORLD_TEXT","Workspace");
        require(writer.checkpointWorldRecoveryForLeave(),"Latest authored World text was not checkpointed");
        require(worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes==canonical,"Checkpoint implicitly saved canonical World content");
        ui.click(de?"Sitzung":"Session");
    }
    const auto checkpoint=worldCheckpointBytes(c);require(checkpoint.find("PRIVATE_RESTART_WORLD_TEXT")!=std::string::npos,"Restart file lacks authored canary");
    EditorWindowController reader;Interaction ui(reader,trace);require(reader.setCurrentCampaign(readCampaign(c.metadata.id)),"Fresh reader refused campaign");
    worldOpenPlace(reader,ui,id);
    const auto attention=worldAttention(ui,de,WorldAttentionCheck::Pending);
    require(WorkspaceInteractionProbe::worldRecovery(reader).pending&&trace.find("Name").empty()&&
        worldCheckpointBytes(c)==checkpoint,"Fresh reader auto-restored or overwrote unresolved checkpoint");
    require(!reader.hasUnsavedChanges()&&reader.checkpointWorldRecoveryForLeave()&&worldCheckpointBytes(c)==checkpoint,
        "An untouched disk checkpoint blocked ordinary leave or was removed");
    WorkspaceInteractionProbe::liveCampaign(reader).partyReputation="UNRELATED_PARTY_STATE_WITH_PENDING_WORLD";
    ProjectSaveService::getInstance().markDirty("unrelated party edit");
    require(reader.saveCurrentCampaign("save unrelated work preserving recovery")&&
        readCampaign(c.metadata.id).partyReputation=="UNRELATED_PARTY_STATE_WITH_PENDING_WORLD"&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes==canonical&&worldCheckpointBytes(c)==checkpoint&&
        WorkspaceInteractionProbe::worldRecovery(reader).pending,"Unrelated Save changed pending recovery or implicitly applied World text");
    ProjectSaveService::getInstance().markDirty("unrelated discarded work");
    require(reader.discardUnsavedChanges("discard unrelated work preserving recovery")&&worldCheckpointBytes(c)==checkpoint&&
        WorkspaceInteractionProbe::worldRecovery(reader).pending,"Global Discard implicitly removed unreviewed World history");
    worldRecoveryOpen(ui,de);WorldClipboardMock clipboard;
    ui.click(de?"Sicherung kopieren":"Copy checkpoint");
    const auto expected=WorldDraftRecovery::copyCheckpoint(WorldDraftRecovery::read(c));
    require(expected.success&&clipboard.text==expected.text,"Actual Copy omitted or changed raw checkpoint bytes");
    WorkspaceInteractionProbe::liveCampaign(reader).presentationModeActive=true;ui.frame(3);
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty()&&ui.renderedText().find("PRIVATE_RESTART_WORLD_TEXT")==std::string::npos&&
        trace.find(de?"Sicherung kopieren":"Copy checkpoint").empty(),"Presentation exposed recovery contents or actions");
    WorkspaceInteractionProbe::liveCampaign(reader).presentationModeActive=false;ui.frame(2);
    worldRecoveryRestore(ui,de);
    require(!WorkspaceInteractionProbe::worldRecovery(reader).pending&&reader.hasUnsavedChanges()&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes==canonical,"Restore changed SQL or omitted unsaved status");
    ui.click(de?"Eintrag öffnen":"Open entry");ui.frame(3);
    worldAttention(ui,de,WorldAttentionCheck::None,&attention);
    require(WorkspaceInteractionProbe::placeDraft(reader).notes=="PRIVATE_RESTART_WORLD_TEXT","Restored editor lost exact latest bytes");
    SQLite::Database sql(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    sql.exec("CREATE TRIGGER reject_world_recovery BEFORE INSERT ON world_places BEGIN SELECT RAISE(ABORT, 'recovery save refusal'); END;");
    ui.click(de?"Ort speichern":"Save place","Workspace");
    require(reader.hasUnsavedChanges()&&worldCheckpointBytes(c)==checkpoint&&
        worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes==canonical,"Failed recovered save changed SQL/checkpoint or lost RAM");
    sql.exec("DROP TRIGGER reject_world_recovery;");ui.click(de?"Ort speichern":"Save place","Workspace");
    require(worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="PRIVATE_RESTART_WORLD_TEXT"&&!reader.hasUnsavedChanges()&&
        !std::filesystem::exists(worldCheckpointPath(c)),"Explicit recovered save did not persist exact text and acknowledge checkpoint");
}

void worldRecoveryFailures(UiInteractionTrace &trace,bool de)
{
    auto c=worldUiFixture();const auto id=c.worldPlaces[0].metadata.id;
    require(ProjectSaveService::getInstance().saveCampaign(c,"World recovery failure fixture"),"Cannot save recovery failure fixture");
    EditorWindowController editor;Interaction ui(editor,trace);editor.setCurrentCampaign(c);worldOpenPlace(editor,ui,id);
    // Opening a place selects the Places category. The later real dungeon-row
    // departure must be exercised from the actual all-World directory.
    const auto count=c.worldPlaces.size()+c.nations.size()+c.shops.size()+c.dungeons.size()+c.maps.size();
    ui.click(std::string(de?"Alle (":"All (")+std::to_string(count)+")","Workspace");
    const auto attention=worldAttention(ui,de,WorldAttentionCheck::None);
    ui.replaceText(de?"SL-Notizen":"GM notes","FIRST_GOOD_CHECKPOINT","Workspace");
    require(editor.checkpointWorldRecoveryForLeave(),"Cannot create recovery baseline");const auto bytes=worldCheckpointBytes(c);
    ui.replaceText(de?"SL-Notizen":"GM notes","LATEST_UNFLUSHED_EDIT","Workspace");
    require(WorkspaceInteractionProbe::worldTick(editor,1)&&worldCheckpointBytes(c)==bytes,"World typing bypassed two-second cadence");
    WorldCheckpointBlock blocked(c);
    require(!WorkspaceInteractionProbe::worldTick(editor,2)&&editor.hasUnsavedChanges(),"Due checkpoint failure did not retain draft");
    const auto attempted=WorkspaceInteractionProbe::worldRecovery(editor).attempted;
    for(int i=0;i<8;++i)require(WorkspaceInteractionProbe::worldTick(editor,1),"Throttled retry unexpectedly attempted write");
    require(WorkspaceInteractionProbe::worldRecovery(editor).attempted==attempted,"Failed checkpoint retried every frame");
    require(!editor.checkpointWorldRecoveryForLeave()&&!editor.worldRecoveryError().empty(),"Leave allowed losing latest World edit");
    const auto privateError=editor.worldRecoveryError();
    worldAttention(ui,de,WorldAttentionCheck::Failed,&attention);
    worldRecoveryOpen(ui,de);
    require(ui.renderedText().find(privateError)!=std::string::npos,
        "Actual World review did not visibly expose the checkpoint error in GM mode");
    WorkspaceInteractionProbe::liveCampaign(editor).presentationModeActive=true;ui.frame(3);
    require(!WorkspaceInteractionProbe::worldTick(editor,2),"Presentation retry unexpectedly wrote blocked checkpoint");ui.frame();
    const std::string safeError=de?"Die private Weltsicherung benötigt Aufmerksamkeit. Prüfe sie im SL-Modus.":
        "Private World recovery needs attention. Return to GM mode to review it.";
    require(editor.shortcutStatusMessage()==safeError&&editor.worldRecoveryError()==safeError,
        "Presentation error accessors did not return the exact safe guidance");
    require(ImGui::GetCurrentContext()->OpenPopupStack.empty(),"Presentation left private World recovery review open");
    require(ui.renderedText().find(privateError)==std::string::npos,
        "Actual presentation frame rendered the private checkpoint error");
    worldAttention(ui,de,WorldAttentionCheck::Failed,&attention);
    require(ui.renderedText().find(privateError)==std::string::npos,
        "Hovering the attention cue exposed the private checkpoint error in presentation mode");
    WorkspaceInteractionProbe::liveCampaign(editor).presentationModeActive=false;ui.frame(2);
    auto replacement=worldUiFixture();require(!editor.setCurrentCampaign(replacement)&&editor.currentCampaign().metadata.id==c.metadata.id,
        "Refused checkpoint replaced campaign/owner");
    ProjectSaveService::getInstance().markDirty("discard refusal sentinel");
    require(!editor.discardUnsavedChanges("refused World discard")&&ProjectSaveService::getInstance().state().dirty&&
        WorkspaceInteractionProbe::placeDraft(editor).notes=="LATEST_UNFLUSHED_EDIT","Failed discard cleared RAM/service state");
    // A row departure is refused once; repeated rendering must not force disk
    // retries or use the old place buffer as a dungeon inspector.
    ui.clickDirectoryRow(std::string("Dungeon canary\n")+(de?"Dungeonstruktur":"Dungeon structure"));ui.frame(3);
    require(WorkspaceInteractionProbe::worldSelection(editor)==id&&trace.find(de?"Struktur speichern":"Save structure").empty(),
        "Failed typed-draft departure exposed a stale dungeon inspector");
    const auto rowAttempt=WorkspaceInteractionProbe::worldRecovery(editor).attempted;ui.frame(8);
    require(WorkspaceInteractionProbe::worldRecovery(editor).attempted==rowAttempt,"Rejected directory transition retried every frame");
    blocked.release();require(editor.checkpointWorldRecoveryForLeave()&&worldCheckpointBytes(c).find("LATEST_UNFLUSHED_EDIT")!=std::string::npos,
        "Recovery retry did not preserve latest text");
    worldAttention(ui,de,WorldAttentionCheck::None,&attention);
    // A committed canonical save is successful even if removing its old
    // checkpoint fails. Retrying cleanup must never reapply the mutation.
    WorldCheckpointBlock cleanup(c);ui.click(de?"Ort speichern":"Save place","Workspace");
    require(worldRow(readCampaign(c.metadata.id).worldPlaces,id).notes=="LATEST_UNFLUSHED_EDIT"&&!editor.hasUnsavedChanges()&&
        !editor.worldRecoveryError().empty()&&editor.worldDraftSaveError().empty(),"Post-commit cleanup failure was reported as an uncommitted edit");
    worldAttention(ui,de,WorldAttentionCheck::Cleanup,&attention);
    cleanup.release();require(WorkspaceInteractionProbe::worldTick(editor,2)&&!std::filesystem::exists(worldCheckpointPath(c))&&
        editor.worldRecoveryError().empty()&&editor.shortcutStatusMessage().find(privateError)==std::string::npos,
        "Committed cleanup retry did not retire old checkpoint/private status");
    worldAttention(ui,de,WorldAttentionCheck::None,&attention);
    ui.replaceText(de?"SL-Notizen":"GM notes","CHECKED_DISCARD_LATEST","Workspace");ui.click(de?"Sitzung":"Session");
    require(editor.discardUnsavedChanges("checked World discard")&&!std::filesystem::exists(worldCheckpointPath(c)),"Successful checked discard left resurrection bytes");
    require(editor.setCurrentCampaign(readCampaign(c.metadata.id))&&!WorkspaceInteractionProbe::worldRecovery(editor).pending,
        "Checked discard resurrected on same-ID reload");
}

#include "world_draft_recovery_scope_ui_cases.hpp"
void worldDraftRecoveryCases(UiInteractionTrace &trace,bool de)
{
    std::cout<<"World recovery UI group: restart/review/privacy\n";worldRecoveryRestart(trace,de);
    std::cout<<"World recovery UI group: cadence/failure/leave/cleanup\n";worldRecoveryFailures(trace,de);
    std::cout<<"World recovery UI group: scoped edits and purchase\n";worldRecoveryScopes(trace,de);
    std::cout<<"World recovery UI group: malformed/root/mixed work\n";worldRecoveryInvalidAndRoots(trace,de);
    std::cout<<"World recovery UI group: missing/archived targets\n";
    worldRecoveryUnavailableTarget(trace,de);
    std::cout<<"World recovery UI: explicit restart/Copy/Restore, privacy, cadence, refused leave/discard, committed cleanup, scoped edits, invalid files and root binding passed\n";
}
