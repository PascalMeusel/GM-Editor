#include "editor_draft_recovery_fixture.hpp"
#include "Controllers/Editor/EditorDraftRecoveryController.hpp"
#include "Controllers/Editor/EditorWindowController.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Views/Editor/EditorWindowView.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include <SQLiteCpp/SQLiteCpp.h>

struct EditorDraftRecoveryProbe {
    static ItemCreatorController &items(EditorWindowController &e){return *e._children.itemCreator;}
    static CharacterSheetController &notes(EditorWindowController &e){return *e._children.characterSheet;}
    static Campaign &campaign(EditorWindowController &e){return e._currentCampaign;}
    static void newItem(ItemCreatorController &i){i.createNewDraft();}
    static void pending(ItemCreatorController &i){i._itemPowerDraft.name="Pending power";i._itemPowerDraft.notes="PRIVATE_POWER";i._powerDraftEdited=true;i.markDraftEdited();}
    static void unnamed(ItemCreatorController &i){pending(i);i._itemPowerDraft.name.clear();}
    static void edit(CharacterSheetController &n,const std::string &value){n._backstoryDraft=value+" back";n._playerNotesDraft=value+" player";n._gmNotesDraft=value+" gm";n.notesDraftEdited();}
    static void revert(CharacterSheetController &n){n.revertSelectedCharacterNotesDraft();}
    static void archive(ItemCreatorController &i){i.archiveSelectedItem(true);}
    static void world(EditorWindowController &e){WorldPlaceModel place;place.name="Saved place";e._currentCampaign.worldPlaces.push_back(place);
        auto edited=place;edited.notes="UNSAVED_WORLD";e.view->_worldDrafts.retain(WorldInspectorDrafts::Draft<WorldPlaceModel>{place,edited});}
    static bool worldDirty(const EditorWindowController &e){return e.view->hasPendingWorldDrafts();}
};
using namespace editor_recovery_test;
namespace {
Campaign live(){Campaign c;c.name="Generated editor recovery "+c.metadata.id;c.ruleSet=std::make_shared<DND5e>();
    ItemModel i;i.name="Original item";c.customItems.push_back(i);CharacterModel ch;ch.gmNotes="original gm";c.characters.push_back(ch);
    CampaignRepository::saveCampaign(c);return c;}
Campaign disk(const EntityId &id){for(auto c:CampaignRepository::loadAllCampaigns())if(c.metadata.id==id)return c;throw std::runtime_error("Missing campaign");}
struct Editors {
    Campaign &c;ItemCreatorController items;CharacterSheetController notes;EditorDraftRecoveryController recovery;
    explicit Editors(Campaign &campaign):c(campaign),items(c),notes(c),recovery(c,items,notes){recovery.initialize();}
    void edit(){require(items.selectItemById(c.customItems[0].metadata.id),"Cannot select item");items.getModel().description="PRIVATE_ITEM";items.markDraftEdited();
        require(notes.selectCharacterById(c.characters[0].metadata.id),"Cannot select notes");EditorDraftRecoveryProbe::edit(notes,"PRIVATE_NOTES");}
};
// A temporary non-directory ancestor deterministically refuses storage on both
// Windows and Linux. The original checkpoint stays intact under held/ throughout.
struct BlockRecovery {
    fs::path directory,held;bool active=false;
    explicit BlockRecovery(const Campaign &c):directory(path(c).parent_path()),held(directory.string()+"-held"){
        require(fs::exists(directory)&&!fs::exists(held),"Missing/occupied storage fault fixture");
        fs::rename(directory,held);AtomicProjectFile::write(directory,"BLOCKED_RECOVERY_ANCESTOR");active=true;
    }
    void release(){if(active){fs::remove(directory);fs::rename(held,directory);active=false;}}
    ~BlockRecovery(){if(active){std::error_code e;fs::remove(directory,e);fs::rename(held,directory,e);}}
};
void restoreAndScopedCommit(){
    auto c=live();Store expected;std::string checkpoint;
    {Editors e(c);e.edit();EditorDraftRecoveryProbe::pending(e.items);EditorDraftRecoveryProbe::newItem(e.items);
        e.items.getModel().name="New draft";e.items.markDraftEdited();EditorDraftRecoveryProbe::pending(e.items);
        expected=e.recovery.drafts();require(expected.items.size()==2&&expected.notes,"Retained/new/pending drafts missing");
        require(e.recovery.checkpoint(false,EditorDraftRecoveryController::Clock::now()+std::chrono::seconds(3)),e.recovery.error());checkpoint=bytes(path(c));}
    Editors e(c);require(e.recovery.pending()&&!e.items.hasUnsavedDraft()&&!e.notes.hasUnsavedDraft(),"Restart auto-restored drafts");
    require(!e.items.selectItemById(c.customItems[0].metadata.id),"Pending checkpoint allowed conflicting item edit");
    require(e.recovery.restore(),"Explicit restore failed");compare(expected,e.recovery.drafts());
    require(c.customItems.size()==1&&c.customItems[0].description.empty()&&disk(c.metadata.id).characters[0].gmNotes=="original gm","Restore committed canonical data");
    require(e.notes.flushDraft(),"Restored notes failed to save");auto pending=read(c);require(pending.status==ReadStatus::Ready&&!pending.drafts.notes&&pending.drafts.items.size()==2,"Notes cleanup erased independent item drafts");
    require(e.items.flushDrafts(),"Restored items failed to save");const auto saved=disk(c.metadata.id);
    require(saved.customItems.size()==2&&read(c).status==ReadStatus::Absent,"Successful commit did not clean checkpoint or duplicated new item");
    for(const auto &i:saved.customItems)require(i.powers.size()==1&&i.powers[0].notes=="PRIVATE_POWER","Pending power lost/duplicated");
    require(e.items.flushDrafts()&&disk(c.metadata.id).customItems.size()==2,"Clean retry duplicated items");
}
void failuresAndBaseline(){
    auto c=live();Editors e(c);e.edit();require(e.recovery.checkpoint(),e.recovery.error());const auto original=bytes(path(c));
    {BlockRecovery block(c);e.items.getModel().description="LATEST_RAM";e.items.markDraftEdited();
        require(!e.recovery.checkpoint(),"Checkpoint falsely succeeded under a regular-file ancestor: "+e.recovery.error());
        require(e.items.getModel().description=="LATEST_RAM","Failed checkpoint lost newest RAM");
        require(bytes(block.held/path(c).filename())==original,"Failed checkpoint altered held prior bytes");
        require(!e.recovery.discard(true),"Discard falsely succeeded under a regular-file ancestor: "+e.recovery.error());
        require(e.items.getModel().description=="LATEST_RAM","Refused discard lost newest RAM");
        require(bytes(block.held/path(c).filename())==original,"Refused discard altered held prior bytes");}
    require(e.recovery.checkpoint()&&read(c).drafts.items.begin()->second.item.description=="LATEST_RAM","Checkpoint retry lost latest text");
    c.customItems[0].description="External item";CampaignRepository::saveCampaign(c);
    require(!e.items.saveItem()&&e.items.getModel().description=="LATEST_RAM"&&disk(c.metadata.id).customItems[0].description=="External item","Stale draft overwrote canonical item");
    c.characters[0].gmNotes="External notes";CampaignRepository::saveCampaign(c);
    require(!e.notes.flushDraft()&&e.recovery.drafts().notes->gmNotes=="PRIVATE_NOTES gm","Stale notes overwritten/lost");
    // Restore original base, change only externally managed context. Draft save
    // must retain those current links and still persist all authored fields.
    c.customItems[0].description.clear();c.customItems[0].metadata.customProperties["gm.context.place"]="current-place";
    CampaignRepository::saveCampaign(c);require(e.items.saveItem(),"Independent context link wrongly blocked save");
    require(disk(c.metadata.id).customItems[0].metadata.customProperties.at("gm.context.place")=="current-place","Draft overwrote external context link");
}
void equalityStillNeedsDurability(){
    auto c=live();Editors e(c);e.edit();EditorDraftRecoveryProbe::pending(e.items);
    require(e.recovery.checkpoint(),e.recovery.error());
    // Normalize the pending power into the draft via one failed checked save.
    SQLite::Database db(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    db.exec("CREATE TRIGGER reject_editor_draft BEFORE UPDATE ON campaigns BEGIN SELECT RAISE(ABORT,'injected recovery save failure'); END;");
    require(!e.items.saveItem()&&e.items.hasUnsavedDraft(),"Injected save did not fail");
    c.customItems[0]=e.items.getModel();
    require(!e.items.saveItem()&&e.items.hasUnsavedDraft()&&disk(c.metadata.id).customItems[0].description.empty(),"RAM equality falsely proved item durability");
    const auto notes=e.recovery.drafts().notes;c.characters[0].backstory=notes->backstory;c.characters[0].playerNotes=notes->playerNotes;c.characters[0].gmNotes=notes->gmNotes;
    require(!e.notes.flushDraft()&&e.notes.hasUnsavedDraft()&&disk(c.metadata.id).characters[0].gmNotes=="original gm","RAM equality falsely proved note durability");
    db.exec("DROP TRIGGER reject_editor_draft;");require(e.items.saveItem()&&e.notes.flushDraft(),"Durable retry failed");
    require(disk(c.metadata.id).customItems[0].powers.size()==1&&read(c).status==ReadStatus::Absent,"Retry duplicated power/left checkpoint");
}
void cleanupAndUnavailable(){
    auto c=live();Editors e(c);e.edit();require(e.recovery.checkpoint(),e.recovery.error());const auto original=bytes(path(c));
    {BlockRecovery block(c);require(e.notes.flushDraft(),"Cleanup failure misreported committed notes as failed");
        require(e.recovery.cleanupPending()&&!e.notes.hasUnsavedDraft()&&!e.recovery.error().empty()&&bytes(block.held/path(c).filename())==original,"Cleanup lost retained checkpoint/warning");}
    require(e.recovery.checkpoint()&&!read(c).drafts.notes&&read(c).drafts.items.size()==1,"Cleanup retry lost other drafts");
    require(e.notes.selectCharacterById(c.characters[0].metadata.id),"Cannot select notes again");EditorDraftRecoveryProbe::edit(e.notes,"deleted target");
    require(e.recovery.checkpoint(),e.recovery.error());c.characters.clear();CampaignRepository::saveCampaign(c);
    // Resolve missing selection by explicit restart/restore, as the real UI does.
    e.items.configure();e.notes.resetDrafts();e.recovery.initialize();require(e.recovery.restore(),"Missing target recovery unavailable");
    const auto before=bytes(path(c));{BlockRecovery block(c);EditorDraftRecoveryProbe::revert(e.notes);
        require(e.notes.hasUnsavedDraft()&&bytes(block.held/path(c).filename())==before,"Failed unavailable-note discard lost data");}
    EditorDraftRecoveryProbe::revert(e.notes);require(!e.notes.hasUnsavedDraft()&&!read(c).drafts.notes&&read(c).drafts.items.size()==1,"Notes-only discard affected item recovery");
    c.customItems.clear();CampaignRepository::saveCampaign(c);require(!e.items.saveItem(),"Deleted item was resurrected");
    EditorDraftRecoveryProbe::pending(e.items);e.items.copyItemAsDraft(e.items.getModel());
    require(e.items.saveItem()&&c.customItems.size()==1&&c.customItems[0].powers.size()==1,"Copy unavailable item lost unfinished power");
}
void contextAndGlobal(){
    auto c=live();EditorWindowController editor;require(editor.setCurrentCampaign(c),"Initial load failed");
    auto &items=EditorDraftRecoveryProbe::items(editor);require(items.selectItemById(c.customItems[0].metadata.id),"Cannot select root item");
    items.getModel().description="ROOT_DRAFT";items.markDraftEdited();require(editor.checkpointEditorDraftRecoveryForLeave(),"Root checkpoint failed");
    const auto original=bytes(path(c));{BlockRecovery block(c);items.getModel().description="LATEST_ROOT";items.markDraftEdited();
        require(!editor.setCurrentCampaign(live())&&editor.currentCampaign().metadata.id==c.metadata.id&&items.hasUnsavedDraft(),"Old-context checkpoint failure allowed campaign replacement");}
    require(!editor.shortcutStatusMessage().empty(),"Failed leave did not expose owned recovery status");
    require(editor.editorDraftRecovery().checkpoint(),"Recovery retry failed");EditorDraftRecoveryProbe::campaign(editor).presentationModeActive=true;
    require(editor.shortcutStatusMessage().empty(),"Successful retry left raw stale recovery failure in presentation status");
    EditorDraftRecoveryProbe::campaign(editor).presentationModeActive=false;
    require(editor.setCurrentCampaign(c)&&editor.editorDraftRecovery().pending()&&!items.hasUnsavedDraft(),"Same-ID reload retained stale RAM or auto-restored");
    const auto pending=bytes(path(c));require(editor.saveCurrentCampaign("unrelated root save")&&bytes(path(c))==pending,"Unrelated Save changed pending checkpoint");
    require(editor.discardUnsavedChanges("unrelated discard")&&bytes(path(c))==pending&&editor.editorDraftRecovery().pending(),"Global Discard erased unreviewed recovery");
    EditorDraftRecoveryProbe::campaign(editor).presentationModeActive=true;
    require(!editor.editorDraftRecovery().restore()&&editor.editorDraftRecovery().copy().empty(),"Private mode exposed recovery actions");
    EditorDraftRecoveryProbe::campaign(editor).presentationModeActive=false;
    const auto oldRoot=ProjectPackageLayout::findProjectRoot();env("TTRPG_DATA_ROOT",fs::path(oldRoot)/"different");
    require(!editor.editorDraftRecovery().restore()&&editor.editorDraftRecovery().copy().empty()&&!editor.saveCurrentCampaign()&&!editor.discardUnsavedChanges("wrong root"),"Changed root allowed checkpoint action");
    env("TTRPG_DATA_ROOT",oldRoot);require(editor.editorDraftRecovery().restore(),"Restoring original root failed");
    require(editor.saveCurrentCampaign("actual root save")&&disk(c.metadata.id).customItems[0].description=="LATEST_ROOT"&&read(c).status==ReadStatus::Absent,"Root Save did not commit recovered item");
}
void partialDiscardAndArchiveSignal(){
    auto c=live();EditorWindowController editor;require(editor.setCurrentCampaign(c),"Partial discard load failed");
    auto &items=EditorDraftRecoveryProbe::items(editor);require(items.selectItemById(c.customItems[0].metadata.id),"Partial discard item unavailable");
    items.getModel().description="ITEM_RETAINED";items.markDraftEdited();EditorDraftRecoveryProbe::world(editor);
    require(editor.checkpointEditorDraftRecoveryForLeave()&&editor.checkpointWorldRecoveryForLeave(),"Partial discard fixture checkpoint failed");
    const auto checkpoint=path(c),held=fs::path(checkpoint.string()+".held");const auto before=bytes(checkpoint);
    fs::rename(checkpoint,held);fs::create_directory(checkpoint);AtomicProjectFile::write(checkpoint/"block","owned fixture");
    require(!editor.discardUnsavedChanges("partial discard")&&!EditorDraftRecoveryProbe::worldDirty(editor)&&items.hasUnsavedDraft()&&
        !editor.draftDiscardError().empty()&&bytes(held)==before,"Partial discard hid completed World action/lost failed W1 draft");
    fs::remove(checkpoint/"block");fs::remove(checkpoint);fs::rename(held,checkpoint);
    require(editor.discardUnsavedChanges("retry discard")&&editor.draftDiscardError().empty()&&!items.hasUnsavedDraft(),"Discard retry did not clear only acknowledged state");
    require(items.selectItemById(c.customItems[0].metadata.id)&&editor.editorDraftRecovery().checkpoint(),"Cannot prepare clean archive fixture");
    SQLite::Database db(ApplicationPaths::databasePath().string(),SQLite::OPEN_READWRITE);
    db.exec("CREATE TRIGGER reject_archive_recovery BEFORE UPDATE ON campaigns BEGIN SELECT RAISE(ABORT,'archive failure'); END;");
    EditorDraftRecoveryProbe::archive(items);require(items.hasUnsavedDraft(),"Archive failure did not retain draft");
    require(editor.editorDraftRecovery().checkpoint(false,EditorDraftRecoveryController::Clock::now()+std::chrono::seconds(3)),"Due archive checkpoint failed");
    const auto archived=read(c);const auto &tags=archived.drafts.items.begin()->second.item.metadata.tags;
    require(std::find(tags.begin(),tags.end(),"archived")!=tags.end(),"Failed-save archive mutation missed scheduled checkpoint");
    db.exec("DROP TRIGGER reject_archive_recovery;");
}
void unfinishedPowerDiscard(){
    auto c=live();c.customItems[0].powers.push_back(action("saved"));CampaignRepository::saveCampaign(c);Editors e(c);
    require(e.items.selectItemById(c.customItems[0].metadata.id),"Cannot select power fixture item");
    e.items.getModel().description="KEEP_OTHER_EDITS";e.items.markDraftEdited();EditorDraftRecoveryProbe::unnamed(e.items);
    require(e.recovery.checkpoint()&&!e.items.saveItem(),"Unnamed pending power did not remain available");
    {BlockRecovery block(c);require(!e.items.clearUnfinishedPower()&&e.recovery.drafts().items.begin()->second.powerDirty&&
        e.items.getModel().description=="KEEP_OTHER_EDITS","Failed checked power discard lost draft fields");}
    require(e.items.clearUnfinishedPower()&&!read(c).drafts.items.begin()->second.powerDirty&&
        e.items.getModel().description=="KEEP_OTHER_EDITS"&&e.items.getModel().powers.size()==1,"Scoped power discard lost existing powers/other edits");
    require(e.items.saveItem()&&disk(c.metadata.id).customItems[0].description=="KEEP_OTHER_EDITS"&&
        disk(c.metadata.id).customItems[0].powers.size()==1,"Cleared unfinished power still blocked save or removed saved power");
}
}
int main(){try{
    isolate(fs::temp_directory_path()/EntityMetadata{"editor-recovery-controller"}.id);
    auto &saveService=ProjectSaveService::getInstance();saveService.setBackupSnapshotsEnabled(false);saveService.setRecoverySnapshotsEnabled(false);
    restoreAndScopedCommit();failuresAndBaseline();equalityStillNeedsDurability();cleanupAndUnavailable();contextAndGlobal();partialDiscardAndArchiveSignal();unfinishedPowerDiscard();
    std::cout<<"Item/notes recovery: explicit restore, baseline conflicts, stable IDs/powers, failure/retry, scoped cleanup/discard, actual root Save/reset and privacy passed.\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
