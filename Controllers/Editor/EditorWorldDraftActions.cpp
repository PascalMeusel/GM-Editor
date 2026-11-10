#include "Controllers/Editor/EditorWindowControllerSupport.hpp"

WorldInspector::MutationResult EditorWindowController::applyWorldInspectorMutation(
    const WorldInspector::Mutation &mutation,const std::string &reason,
    WorldInspectorDrafts::Scope drafts,WorldInspectorDrafts::Scope discarded)
{
    if(!_workspace.worldHistory->beforeHistoryAction())return {WorldInspector::MutationStatus::Conflict,
        _workspace.worldHistory->registrationError(),{}};
    if(!view->worldRecoveryReady())return {WorldInspector::MutationStatus::Conflict,worldRecoveryError(),{}};
    view->retainWorldDraft();
    const auto result=WorldInspector::applyWithRollback(_currentCampaign,[&](Campaign &campaign) {
        // Preserve the existing dungeon save contract independently.
        auto ready=view->applyDungeonDrafts(campaign);
        if(ready.applied())ready=view->_worldDrafts.apply(campaign,drafts);
        return ready.applied()?mutation(campaign):ready;
    },[&](Campaign &){return persistCurrentCampaign(reason);});
    if(result.applied()) {
        view->clearDungeonDrafts(); view->acknowledgeWorldDrafts(drafts); view->discardWorldDrafts(discarded);
        view->_worldDraftReloadRequested=true;
        if(_shortcuts.statusMessage==_worldDraftSaveError)_shortcuts.statusMessage.clear();
        _worldDraftSaveError.clear(); markSearchIndexDirty();
        view->cleanupWorldRecoveryAfterCommit();
    } else {
        _worldDraftSaveError=uiText("World edits were not saved. Your drafts are retained. Open World > Review World drafts to review or discard the affected draft. ",
            "Weltänderungen wurden nicht gespeichert. Deine Entwürfe bleiben erhalten. Öffne Welt > Weltentwürfe prüfen, um den betroffenen Entwurf zu prüfen oder zu verwerfen. ")+result.message;
        _shortcuts.statusMessage=_worldDraftSaveError;
    }
    return result;
}
WorldInspector::MutationResult EditorWindowController::saveWorldDraft(WorldInspectorDrafts::Scope scope,const std::string &reason)
{
    const auto history=_workspace.worldHistory;
    return history->save(std::move(scope),reason);
}
