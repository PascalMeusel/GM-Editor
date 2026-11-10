#include "EditorWorldHistory.hpp"
#include "EditorWorldHistoryInstall.hpp"
#include "EditorWindowControllerSupport.hpp"

WorldInspector::MutationResult EditorWorldHistory::save(WorldInspectorDrafts::Scope scope,const std::string &reason)
{
    using Status=WorldInspector::MutationStatus;
    Context stamp;
    bool contextCaptured=false;
    const auto failed=[&](WorldInspector::MutationResult result) {
        if(owner_&&(!contextCaptured||current(stamp))) {
            owner_->_worldDraftSaveError=uiText(
                "World edits were not saved. Your drafts are retained. Open World > Review World drafts to review or discard the affected draft. ",
                "Weltänderungen wurden nicht gespeichert. Deine Entwürfe bleiben erhalten. Öffne Welt > Weltentwürfe prüfen, um den betroffenen Entwurf zu prüfen oder zu verwerfen. ")+result.message;
            owner_->_shortcuts.statusMessage=owner_->_worldDraftSaveError;
        }
        return result;
    };
    const auto rejected=[&](std::string error){return failed(WorldInspector::MutationResult{Status::Conflict,std::move(error),{}});};
    if(!owner_||busy_)return rejected("A World history operation is already in progress.");
    if(pending_) {beforeHistoryAction();return {Status::Conflict,error_,{}};}
    const auto held=shared_from_this();
    struct Busy {bool &flag;explicit Busy(bool &v):flag(v){flag=true;}~Busy(){flag=false;}} busy(busy_);
    try {stamp=context();contextCaptured=true;}catch(const std::exception &e){return rejected(e.what());}
    auto &view=*owner_->view;
    const auto changed=[&] {revokeCommitted(stamp);return WorldInspector::MutationResult{Status::CommittedContextChanged,
        "The submitted World changes were saved, but the campaign or draft changed during save. Newer state was retained.",{}};};
    if(owner_->_currentCampaign.presentationModeActive||!view.worldRecoveryReady())return rejected("Review private World recovery before saving.");
    bool durable=false;
    try {
        view.retainWorldDraft();view.retainDungeonDraft();
        if(owner_->_children.mapViewer->transientFogEditActive())owner_->_children.mapViewer->cancelTransientFogEdit();
        const auto submitted=view._worldDrafts;
        const auto dungeonDrafts=view._retainedDungeonDrafts;
        const Campaign original=owner_->_currentCampaign;
        Campaign staged=original;
        auto result=view.applyDungeonDrafts(staged);
        if(!result.applied())return failed(std::move(result));
        const Campaign worldBefore=staged;
        result=submitted.apply(staged,scope);
        if(!result.applied())return failed(std::move(result));
        auto captured=WorldInspectorHistory::capture(worldBefore,staged,scope);
        if(!captured.success)return rejected(captured.error);
        // All optional history allocation happens before the durable boundary.
        const auto patch=std::make_shared<const WorldInspectorHistory::Patch>(std::move(captured.patch));
        const auto times=EditorWorldHistoryDetail::rememberTimes(original,*patch);
        const auto label=uiText("Edit World fields","Weltfelder bearbeiten");
        if(!current(stamp))return rejected("The World save context changed.");
        const auto saved=ProjectSaveService::getInstance().saveCampaignDetailed(staged,reason);
        durable=saved.committed();
        if(!durable)return failed({Status::PersistenceFailed,saved.error,scope.entityId});
        if(!current(stamp))return changed();
        view.retainWorldDraft();view.retainDungeonDraft();
        if(!WorldInspectorHistory::sameDrafts(submitted,view._worldDrafts,scope)||
           !EditorWorldHistoryDetail::sameDungeonDrafts(dungeonDrafts,view._retainedDungeonDrafts))return changed();
        Campaign merged=owner_->_currentCampaign;
        result=WorldInspectorHistory::apply(merged,*patch,true);
        if(!result.applied())return changed();
        EditorWorldHistoryDetail::installDungeons(merged,original,staged,dungeonDrafts);
        EditorWorldHistoryDetail::adoptTimestamps(merged,staged,times);
        // Copy only committed fields into the post-callback live state. Unrelated
        // subscriber changes and independent drafts never come from staged.
        if(!acknowledge(submitted,scope))return changed();
        owner_->_currentCampaign=std::move(merged);
        for(const auto &[id,draft]:dungeonDrafts)view._retainedDungeonDrafts.erase(id);
        if(dungeonDrafts.count(view._worldDraftEntityId))view._worldDungeonDirty=false;
        if(owner_->_shortcuts.statusMessage==owner_->_worldDraftSaveError)owner_->_shortcuts.statusMessage.clear();
        owner_->_worldDraftSaveError.clear();
        view.cleanupWorldRecoveryAfterCommit();
        if(!patch->empty()) {pending_=patch;pendingContext_=stamp;label_=label;error_.clear();registerPending();}
        if(!current(stamp))return changed();
        if(!committed(stamp))return changed();
        return {Status::Applied,{},scope.entityId};
    } catch(const std::exception &e) {
        if(durable)return changed();
        return rejected(e.what());
    }
}
