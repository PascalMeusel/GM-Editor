#include "EditorWorldHistory.hpp"
#include "EditorWindowControllerSupport.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/WorldInspectorHistoryFields.hpp"
#include "EditorWorldHistoryInstall.hpp"
#include <stdexcept>

namespace
{
struct BusyScope { bool &flag;explicit BusyScope(bool &v):flag(v){flag=true;}~BusyScope(){flag=false;} };
}
EditorWorldHistory::Context EditorWorldHistory::context() const
{
    if(!owner_)throw std::runtime_error("The World editor is no longer available.");
    return {generation_,owner_->_currentCampaign.metadata.id,
        std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal()};
}
bool EditorWorldHistory::current(const Context &expected) const
{
    try {const auto now=context();return now.generation==expected.generation&&now.campaignId==expected.campaignId&&now.root==expected.root;}
    catch(...){return false;}
}
void EditorWorldHistory::reset()
{ ++generation_;pending_.reset();pendingContext_={};error_.clear();label_.clear(); }
void EditorWorldHistory::detach() noexcept
{ ++generation_;owner_=nullptr;pending_.reset(); }
bool EditorWorldHistory::canRetryRegistration() const
{
    return owner_&&pending_&&!busy_&&pendingContext_.generation==generation_&&
        pendingContext_.campaignId==owner_->_currentCampaign.metadata.id&&!owner_->_currentCampaign.presentationModeActive;
}
bool EditorWorldHistory::registerPending()
{
    if(!pending_)return true;
    const auto stamp=pendingContext_;const auto patch=pending_;
    if(!current(stamp)) {error_=uiText("Undo belongs to an earlier campaign or data folder.","Rückgängig gehört zu einer früheren Kampagne oder einem anderen Datenordner.");return false;}
    try {
        std::weak_ptr<EditorWorldHistory> weak=shared_from_this();
        const auto action=[weak,stamp,patch](bool forward) {
            const auto owner=weak.lock();
            if(!owner)throw std::runtime_error("This World history owner is no longer available.");
            owner->perform(*patch,forward,stamp);
        };
        const bool accepted=ProjectCommandManager::getInstance().recordApplied(std::make_unique<LambdaProjectCommand>(
            label_,[action]{action(true);},[action]{action(false);}));
        if(!current(stamp))return false; // A notification can replace this editor's campaign.
        if(accepted) {pending_.reset();error_.clear();label_.clear();return true;}
    } catch(const std::exception &e) {if(current(stamp))error_=e.what();}
    if(current(stamp)&&error_.empty())error_=uiText("Saved. Retry Undo registration when the current history operation finishes.",
        "Gespeichert. Versuche den Rückgängig-Eintrag erneut, sobald der aktuelle Verlaufsvorgang beendet ist.");
    return false;
}
bool EditorWorldHistory::retryRegistration()
{
    if(!canRetryRegistration())return false;
    BusyScope busy(busy_);return registerPending();
}
bool EditorWorldHistory::beforeHistoryAction()
{
    if(!owner_||busy_)return false;
    if(!pending_)return true;
    error_=uiText("Saved. Open World > Review World drafts and choose Retry Undo before another history action.",
        "Gespeichert. Öffne Welt > Weltentwürfe prüfen und wähle Rückgängig erneut versuchen, bevor du den Verlauf weiter verwendest.");
    owner_->_shortcuts.statusMessage=error_;return false;
}
bool EditorWorldHistory::acknowledge(const WorldInspectorDrafts::Store &submitted,const WorldInspectorDrafts::Scope &scope)
{
    auto &view=*owner_->view;
    auto remaining=view._worldDrafts;
    if(!WorldInspectorHistory::acknowledgeSubmitted(remaining,submitted,scope))return false;
    // Prepare baseline replacements before touching any live form. Edited vectors
    // remain stable until the usual next-frame reload.
    auto place=WorldInspectorDrafts::Draft<WorldPlaceModel>{view._worldPlaceBase,view._worldPlaceDraft};
    auto nation=WorldInspectorDrafts::Draft<NationModel>{view._worldNationBase,view._worldNationDraft};
    auto shop=WorldInspectorDrafts::Draft<ShopModel>{view._worldShopBase,view._worldShopDraft};
    const auto accept=[&](auto &draft,const auto &rows) {
        const auto found=rows.find(view._worldDraftEntityId);
        if(found!=rows.end()&&WorldInspectorDrafts::detail::selected(scope,found->first))
            WorldInspectorHistory::detail::acknowledge(draft,found->second,scope);
    };
    try {accept(place,submitted.places);accept(nation,submitted.nations);accept(shop,submitted.shops);}
    catch(...){return false;}
    view._worldDrafts=std::move(remaining);
    view._worldPlaceBase=std::move(place.base);view._worldNationBase=std::move(nation.base);view._worldShopBase=std::move(shop.base);
    view._worldDraftReloadRequested=true;return true;
}
bool EditorWorldHistory::committed(const Context &stamp)
{
    if(!current(stamp))return false;
    owner_->markSearchIndexDirty();
    try {EntityRegistry::getInstance().rebuild(owner_->_currentCampaign);}catch(...){}
    owner_->_children.liveSessionDashboard->onLiveCampaignCommitted();
    if(!current(stamp))return false;
    owner_->_children.gmScreen->onLiveCampaignCommitted();
    return current(stamp);
}
void EditorWorldHistory::revokeCommitted(const Context &stamp)
{
    if(!current(stamp))return;
    ++generation_;pending_.reset();label_.clear();error_.clear();
    ProjectCommandManager::getInstance().clear();
    owner_->_worldDraftSaveError=uiText(
        "The World change was saved, but newer state could not be combined safely. Newer state and drafts were retained; obsolete Undo history was cleared.",
        "Die Weltänderung wurde gespeichert, konnte aber nicht sicher mit neuerem Zustand verbunden werden. Neuerer Zustand und Entwürfe bleiben erhalten; der überholte Rückgängig-Verlauf wurde geleert.");
    owner_->_shortcuts.statusMessage=owner_->_worldDraftSaveError;
    owner_->view->_worldInspectorStatus=owner_->_worldDraftSaveError;
}
void EditorWorldHistory::perform(const WorldInspectorHistory::Patch &patch,bool forward,const Context &stamp)
{
    if(busy_||!current(stamp))throw std::runtime_error(uiText("This World edit belongs to an earlier editor context.","Diese Weltänderung gehört zu einem früheren Bearbeitungskontext."));
    BusyScope busy(busy_);
    auto &view=*owner_->view;
    if(owner_->_currentCampaign.presentationModeActive||!view.worldRecoveryReady())
        throw std::runtime_error(uiText("Review private World recovery before using World history.","Prüfe die privaten Weltentwürfe, bevor du den Weltverlauf verwendest."));
    view.retainWorldDraft();view.retainDungeonDraft();
    if(WorldInspectorHistory::overlaps(patch,view._worldDrafts))
        throw std::runtime_error(uiText("An unfinished World draft overlaps this history edit. Save or review that draft first.","Ein unfertiger Weltentwurf betrifft dieselben Felder. Speichere oder prüfe zuerst diesen Entwurf."));
    if(!view.checkpointWorldRecovery(true))throw std::runtime_error(uiText("World drafts could not be checkpointed. Nothing was changed.","Weltentwürfe konnten nicht gesichert werden. Nichts wurde geändert."));
    if(!current(stamp))throw std::runtime_error(uiText("The World history context changed.","Der Kontext des Weltverlaufs wurde geändert."));
    if(owner_->_children.mapViewer->transientFogEditActive())owner_->_children.mapViewer->cancelTransientFogEdit();
    Campaign staged=owner_->_currentCampaign;
    const auto times=EditorWorldHistoryDetail::rememberTimes(staged,patch);
    const auto applied=WorldInspectorHistory::apply(staged,patch,forward);
    if(!applied.applied())throw std::runtime_error(uiText("The recorded World fields or bindings changed. Newer values were retained.","Die gespeicherten Weltfelder oder Verknüpfungen wurden geändert. Neuere Werte bleiben erhalten."));
    EditorWorldHistoryDetail::touch(staged,patch);
    blackoutLocalPlayerDisplay(staged.localPlayerDisplay);
    const auto saved=ProjectSaveService::getInstance().saveCampaignDetailed(staged,forward?"redo World edit":"undo World edit");
    if(!saved.committed())throw std::runtime_error(uiText("The World history change could not be saved. Nothing was changed. ","Die Weltänderung konnte nicht gespeichert werden. Nichts wurde geändert. ")+saved.error);
    try {
        if(!current(stamp))return; // The reset already revoked this command.
        view.retainWorldDraft();view.retainDungeonDraft();
        Campaign merged=owner_->_currentCampaign;
        if(!WorldInspectorHistory::apply(merged,patch,forward).applied()) {revokeCommitted(stamp);return;}
        EditorWorldHistoryDetail::adoptTimestamps(merged,staged,times);
        blackoutLocalPlayerDisplay(merged.localPlayerDisplay);
        // Subscriber-created drafts stay untouched even when they concern the
        // just-committed field. A later opposite operation will refuse overlap.
        owner_->_currentCampaign=std::move(merged);
        view._worldDraftReloadRequested=true;
        committed(stamp);
    } catch(...) {revokeCommitted(stamp);}
}
