#include "Controllers/Editor/EditorDraftRecoveryController.hpp"
#include "Controllers/Editor/Widgets/ItemCreator/ItemCreatorController.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include <algorithm>
#include <stdexcept>

namespace {
const char *text(const char *en,const char *de) {
    return gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German?de:en;
}
}
EditorDraftRecoveryController::EditorDraftRecoveryController(Campaign &c,ItemCreatorController &i,CharacterSheetController &n)
    :_campaign(c),_items(i),_characters(n)
{
    _items._recoveryReady=_characters._recoveryReady=[this]{return ready();};
    _items._recoveryChanged=_characters._recoveryChanged=[this]{_dirty=true;};
    _items._recoveryCommitted=_characters._recoveryCommitted=[this]{committed();};
    _characters._recoveryDiscard=[this]{return discardNotes();};
    _items._recoveryCheckpoint=[this]{_cleanup=false;return checkpoint(true);};
}
EditorDraftRecoveryController::~EditorDraftRecoveryController()
{
    _items._recoveryReady={};_items._recoveryChanged={};_items._recoveryCommitted={};_items._recoveryCheckpoint={};
    _characters._recoveryReady={};_characters._recoveryChanged={};_characters._recoveryCommitted={};_characters._recoveryDiscard={};
}
void EditorDraftRecoveryController::initialize()
{
    open=false;_initialized=true;_campaignId=_campaign.metadata.id;_pending=false;_dirty=false;_cleanup=false;
    _lastPayload.clear();_error.clear();_read={};_root.clear();_attempted=Clock::now();
    try {_root=std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal();
        _read=EditorDraftRecovery::read(_campaign);}
    catch(const std::exception &e){_read.status=EditorDraftRecovery::ReadStatus::Unavailable;_read.error=e.what();}
    _pending=_read.status!=EditorDraftRecovery::ReadStatus::Absent;
}
bool EditorDraftRecoveryController::bound() const
{
    try {return _initialized&&_campaignId==_campaign.metadata.id&&
        _root==std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal();}
    catch(...){return false;}
}
bool EditorDraftRecoveryController::ready()
{
    if(!_initialized)return true; // construction precedes the first campaign load
    if(!bound()) {_error=text("The item/notes recovery folder changed. Restore the original data location before saving or leaving.",
        "Der Sicherungsordner für Gegenstände/Notizen wurde geändert. Stelle vor Speichern oder Verlassen den ursprünglichen Datenpfad wieder her.");return false;}
    if(_pending) {_error=text("Review the saved item/character-note checkpoint before editing these drafts.",
        "Prüfe die gesicherten Gegenstands-/Figurnotizentwürfe, bevor du diese Entwürfe bearbeitest.");return false;}
    return true;
}
EditorDraftRecovery::Store EditorDraftRecoveryController::drafts() const
{
    EditorDraftRecovery::Store result;
    for(const auto &[id,d]:_items._retainedDrafts)
        if(d.dirty||d.powerDirty)result.items[id]={d.item,d.base,d.power,d.powerDirty};
    if(_items._draftEdited||_items._powerDraftEdited)
        result.items[_items._model.metadata.id]={_items._model,_items._draftBase,_items._itemPowerDraft,_items._powerDraftEdited};
    else result.items.erase(_items._model.metadata.id); // current state supersedes an old retained copy
    const auto &n=_characters._state.notes;
    if(n.dirty())result.notes=EditorDraftRecovery::Notes{n.characterId,n.backstory,n.playerNotes,n.gmNotes,
        n.savedBackstory,n.savedPlayerNotes,n.savedGmNotes};
    return result;
}
bool EditorDraftRecoveryController::checkpoint(bool force,Clock::time_point now)
{
    if(!_initialized)return true;
    if(!bound()){ready();return false;}
    if(_pending)return true; // untouched durable recovery is not an unsaved RAM draft
    if(!force&&(!_dirty||now-_attempted<std::chrono::seconds(2)))return true;
    _attempted=now;
    const auto current=drafts();const auto encoded=EditorDraftRecovery::encode(_campaignId,current);
    std::string failure;
    if(!encoded.success)failure=encoded.error;
    else if(encoded.payload==_lastPayload){_dirty=false;_cleanup=false;_error.clear();return true;}
    else failure=EditorDraftRecovery::save(_campaign,current);
    if(!failure.empty()) {
        _dirty=true;_error=std::string(_cleanup?text(
            "Campaign changes were saved; item/notes recovery cleanup failed and will retry. ",
            "Kampagnenänderungen wurden gespeichert; die Gegenstands-/Notizensicherung konnte nicht bereinigt werden. Erneuter Versuch folgt. "):text(
            "Latest item/notes drafts could not be checkpointed. Keep them open and retry. ",
            "Die neuesten Gegenstands-/Notizentwürfe konnten nicht gesichert werden. Lass sie geöffnet und versuche es erneut. "))+failure;
        return _cleanup&&current.empty();
    }
    _lastPayload=encoded.payload;_dirty=false;_cleanup=false;_error.clear();return true;
}
void EditorDraftRecoveryController::committed() {_dirty=true;_cleanup=true;checkpoint(true);}
bool EditorDraftRecoveryController::attention() const
{return _pending||!_error.empty()||_items.hasUnsavedDraft()||_characters.selectedCharacterNotesDraftDirty();}
const std::string &EditorDraftRecoveryController::error() const
{
    if(privateMode()&&!_error.empty()) {
        static const std::string en="Private item/notes recovery needs attention. Return to GM mode to review it.";
        static const std::string de="Private Gegenstands-/Notizensicherung benötigt Aufmerksamkeit. Prüfe sie im SL-Modus.";
        return gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German?de:en;
    }
    return _error;
}
bool EditorDraftRecoveryController::restore()
{
    if(privateMode()||!bound()){if(!bound())ready();return false;}
    if(!_pending||_read.status!=EditorDraftRecovery::ReadStatus::Ready||!drafts().empty())return false;
    _items.configure();
    for(const auto &[id,d]:_read.drafts.items)
        _items._retainedDrafts[id]={d.item,d.power,d.base?d.item.metadata.id:EntityId{},true,d.powerDirty,d.base};
    if(!_read.drafts.items.empty())_items.restoreDraft(_read.drafts.items.begin()->first);
    if(_read.drafts.notes) {
        const auto &n=*_read.drafts.notes;
        _characters._state.notes={n.characterId,n.backstory,n.playerNotes,n.gmNotes,n.savedBackstory,n.savedPlayerNotes,n.savedGmNotes};
        const auto found=std::find_if(_campaign.characters.begin(),_campaign.characters.end(),[&](const auto &c){return c.metadata.id==n.characterId;});
        _characters._selectedCharacterIndex=found==_campaign.characters.end()?-1:static_cast<int>(found-_campaign.characters.begin());
    }
    _pending=false;_lastPayload=EditorDraftRecovery::encode(_campaignId,drafts()).payload;_dirty=false;_cleanup=false;_error.clear();
    return true;
}
bool EditorDraftRecoveryController::discard(bool savedCheckpoint)
{
    if(!_initialized)return true;
    if(!bound()){ready();return false;}
    if(_pending&&!savedCheckpoint&&drafts().empty())return true;
    const auto failure=EditorDraftRecovery::save(_campaign,{});
    if(!failure.empty()){_error=std::string(text("Item/notes drafts were not discarded. ","Gegenstands-/Notizentwürfe wurden nicht verworfen. "))+failure;return false;}
    _items.configure();_characters._state.notes.clear();_read={};_pending=false;_dirty=false;_cleanup=false;_error.clear();
    _lastPayload=EditorDraftRecovery::encode(_campaignId,{}).payload;return true;
}
bool EditorDraftRecoveryController::discardNotes()
{
    if(!ready())return false;
    auto remaining=drafts();remaining.notes.reset();
    const auto failure=EditorDraftRecovery::save(_campaign,remaining);
    if(!failure.empty()){_error=std::string(text("Notes were not reverted. ","Notizen wurden nicht verworfen. "))+failure;return false;}
    _lastPayload=EditorDraftRecovery::encode(_campaignId,remaining).payload;_error.clear();_dirty=false;_cleanup=false;return true;
}
bool EditorDraftRecoveryController::retryRead()
{
    if(privateMode()||!bound()||!drafts().empty()){if(!bound())ready();return false;}
    _read=EditorDraftRecovery::read(_campaign);_pending=_read.status!=EditorDraftRecovery::ReadStatus::Absent;_error.clear();return true;
}
std::string EditorDraftRecoveryController::copy()
{
    if(privateMode()||!bound()){if(!bound())ready();return {};}
    try {
        if(_pending)return EditorDraftRecovery::copyCheckpoint(_read);
        // Copy current RAM, including edits newer than the last disk checkpoint.
        auto currentDrafts=drafts();const auto encoded=EditorDraftRecovery::encode(_campaignId,currentDrafts);
        if(!encoded.success)throw std::runtime_error(encoded.error);
        EditorDraftRecovery::ReadResult current;current.status=EditorDraftRecovery::ReadStatus::Ready;
        current.drafts=std::move(currentDrafts);current.rawAvailable=true;current.rawBytes=encoded.payload;
        return EditorDraftRecovery::copyCheckpoint(current);
    } catch(const std::exception &e){_error=e.what();return {};}
}
