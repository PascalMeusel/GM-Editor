#include "Views/Editor/EditorViewDetails.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"

void EditorWindowView::setWorldRecoveryError(std::string message)
{
    const auto previous=_controller.worldRecoveryError();
    const auto previousPrivate=_worldRecovery.error;
    _worldRecovery.error=std::move(message);
    if(!_worldRecovery.error.empty())_controller._shortcuts.statusMessage=_worldRecovery.error;
    else if(_controller._shortcuts.statusMessage==previous||_controller._shortcuts.statusMessage==previousPrivate)
        _controller._shortcuts.statusMessage.clear();
}
void EditorWindowView::resetWorldRecovery()
{
    setWorldRecoveryError({});
    _worldRecovery={};_worldDraftPopupCampaignId.clear();
    const auto &campaign=_controller.currentCampaign();
    if(campaign.metadata.id.empty())return;
    _worldRecovery.campaignId=campaign.metadata.id;
    try {_worldRecovery.root=std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal();
        _worldRecovery.recovered=WorldDraftRecovery::read(campaign);}
    catch(const std::exception &e){_worldRecovery.recovered.error=e.what();}
    _worldRecovery.initialized=true;
    _worldRecovery.pending=_worldRecovery.recovered.status!=WorldDraftRecovery::ReadStatus::Absent;
}
bool EditorWindowView::worldRecoveryBound() const
{
    try {return _worldRecovery.initialized&&_worldRecovery.campaignId==_controller.currentCampaign().metadata.id&&
        _worldRecovery.root==std::filesystem::absolute(ProjectPackageLayout::findProjectRoot()).lexically_normal();}
    catch(...){return false;}
}
bool EditorWindowView::worldRecoveryReady()
{
    if(!_worldRecovery.initialized)resetWorldRecovery();
    if(!worldRecoveryBound()) {
        setWorldRecoveryError(uiTextForLocale("The World recovery folder changed. Restore the original data location before saving or leaving.",
            "Der Wiederherstellungsordner wurde geändert. Stelle vor dem Speichern oder Verlassen den ursprünglichen Datenpfad wieder her."));return false;}
    if(_worldRecovery.pending) {
        setWorldRecoveryError(uiTextForLocale("Resolve the saved World checkpoint in World > Review World drafts before editing or saving World entries.",
            "Prüfe zuerst die gesicherten Weltentwürfe unter Welt > Weltentwürfe prüfen, bevor du Welteinträge bearbeitest oder speicherst."));return false;}
    return true;
}
bool EditorWindowView::checkpointWorldRecovery(bool force,EditorWorldRecoveryState::Clock::time_point now)
{
    if(!_worldRecovery.initialized)return true;
    retainWorldDraft();
    if(!worldRecoveryBound()){worldRecoveryReady();return false;}
    // An unresolved disk checkpoint is already owned by this campaign. Never
    // replace it with the currently empty editable store.
    if(_worldRecovery.pending)return true;
    if(!_worldRecovery.dirty)return true;
    if(!force&&now-_worldRecovery.attempted<std::chrono::seconds(2))return true;
    _worldRecovery.attempted=now;
    const auto encoded=WorldDraftRecovery::encode(_worldRecovery.campaignId,_worldDrafts);
    WorldDraftRecovery::WriteResult result;
    if(!encoded.success)result.error=encoded.error;
    else if(encoded.payload==_worldRecovery.lastPayload){_worldRecovery.dirty=false;_worldRecovery.cleanup=false;setWorldRecoveryError({});return true;}
    else result=WorldDraftRecovery::save(_controller.currentCampaign(),_worldDrafts);
    if(!result.success) {
        setWorldRecoveryError((_worldRecovery.cleanup?uiTextForLocale(
            "Campaign changes were saved, but the old World recovery checkpoint could not be updated. Cleanup will retry. ",
            "Die Kampagnenänderungen wurden gespeichert, aber die alte Weltsicherung konnte nicht aktualisiert werden. Die Bereinigung wird erneut versucht. "):
            uiTextForLocale("Latest World edits could not be checkpointed. They remain open; check folder access and retry. ",
                "Die neuesten Weltänderungen konnten nicht gesichert werden. Sie bleiben geöffnet; prüfe den Ordnerzugriff und versuche es erneut. "))+result.error);
        return _worldRecovery.cleanup&&_worldDrafts.empty();
    }
    _worldRecovery.lastPayload=encoded.payload;_worldRecovery.dirty=false;_worldRecovery.cleanup=false;setWorldRecoveryError({});return true;
}
void EditorWindowView::cleanupWorldRecoveryAfterCommit()
{
    _worldRecovery.dirty=true;_worldRecovery.cleanup=true;
    checkpointWorldRecovery(true); // A committed mutation cannot become a failed save.
}
bool EditorWindowView::discardWorldDraftsChecked(const WorldInspectorDrafts::Scope &scope)
{
    retainWorldDraft();
    if(!worldRecoveryBound()){worldRecoveryReady();return false;}
    if(_worldRecovery.pending&&scope.part!=WorldInspectorDrafts::Part::All)return false;
    auto remaining=_worldDrafts;remaining.discard(scope);
    const auto result=WorldDraftRecovery::save(_controller.currentCampaign(),remaining);
    if(!result.success) {setWorldRecoveryError(uiTextForLocale(
        "Nothing was discarded: the World checkpoint could not be updated. Retry or keep working. ",
        "Nichts wurde verworfen: Die Weltsicherung konnte nicht aktualisiert werden. Versuche es erneut oder arbeite weiter. ")+result.error);return false;}
    discardWorldDrafts(scope);_worldRecovery.recovered={};_worldRecovery.pending=false;
    _worldRecovery.lastPayload=WorldDraftRecovery::encode(_worldRecovery.campaignId,remaining).payload;
    _worldRecovery.dirty=false;_worldRecovery.cleanup=false;setWorldRecoveryError({});return true;
}
