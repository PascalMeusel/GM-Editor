#include "Views/Editor/EditorViewDetails.hpp"
#include "Models/Project/WorldInspectorDraftFields.hpp"
#include "Models/UX/CampaignNavigationResolver.hpp"
#include "Views/Editor/EditorWorldDraftText.hpp"
#include "Controllers/Editor/EditorWorldHistory.hpp"

using namespace gm::editor_view_detail;
using WorldInspectorDrafts::Scope;
using WorldInspectorDrafts::Draft;

void EditorWindowView::retainWorldDraft()
{
    if (_worldDraftEntityId.empty() || !_worldFormEdited) return;
    _worldRecovery.dirty=true;_worldRecovery.cleanup=false;
    if (_worldDraftKind == WorldDraftKind::Place) _worldDrafts.retain(Draft<WorldPlaceModel>{_worldPlaceBase,_worldPlaceDraft});
    if (_worldDraftKind == WorldDraftKind::Nation) _worldDrafts.retain(Draft<NationModel>{_worldNationBase,_worldNationDraft});
    if (_worldDraftKind == WorldDraftKind::Shop) _worldDrafts.retain(Draft<ShopModel>{_worldShopBase,_worldShopDraft});
    _worldFormEdited=false;
}
bool EditorWindowView::hasPendingWorldDrafts() const
{
    auto retained=_worldDrafts.places.size()+_worldDrafts.nations.size()+_worldDrafts.shops.size();
    if (!_worldFormEdited || _worldDraftEntityId.empty()) return retained!=0;
    if (_worldDraftKind == WorldDraftKind::Place) return retained>_worldDrafts.places.count(_worldDraftEntityId)||WorldInspectorDrafts::changed(_worldPlaceBase,_worldPlaceDraft);
    if (_worldDraftKind == WorldDraftKind::Nation) return retained>_worldDrafts.nations.count(_worldDraftEntityId)||WorldInspectorDrafts::changed(_worldNationBase,_worldNationDraft);
    if (_worldDraftKind == WorldDraftKind::Shop) return retained>_worldDrafts.shops.count(_worldDraftEntityId)||WorldInspectorDrafts::changed(_worldShopBase,_worldShopDraft);
    return retained!=0;
}
void EditorWindowView::acknowledgeWorldDrafts(const Scope &scope)
{
    _worldDrafts.acknowledge(scope);
    if (!WorldInspectorDrafts::detail::selected(scope,_worldDraftEntityId)) return;
    // Only the separate baseline changes during this frame. Controls can still
    // hold references into the authored vectors until the next safe reload.
    if (_worldDraftKind == WorldDraftKind::Place) {
        Draft<WorldPlaceModel> d{_worldPlaceBase,_worldPlaceDraft}; WorldInspectorDrafts::acknowledge(d,scope); _worldPlaceBase=std::move(d.base);
    }
    if (_worldDraftKind == WorldDraftKind::Nation) {
        Draft<NationModel> d{_worldNationBase,_worldNationDraft}; WorldInspectorDrafts::acknowledge(d,scope); _worldNationBase=std::move(d.base);
    }
    if (_worldDraftKind == WorldDraftKind::Shop) {
        Draft<ShopModel> d{_worldShopBase,_worldShopDraft}; WorldInspectorDrafts::acknowledge(d,scope); _worldShopBase=std::move(d.base);
    }
    _worldDraftReloadRequested=true;
}
void EditorWindowView::discardWorldDrafts(const Scope &scope)
{
    if (scope.part==WorldInspectorDrafts::Part::None) return;
    acknowledgeWorldDrafts(scope);
    _worldDrafts.discard(scope);
    if(_controller._shortcuts.statusMessage==_controller._worldDraftSaveError)_controller._shortcuts.statusMessage.clear();
    _controller._worldDraftSaveError.clear();
    if (WorldInspectorDrafts::detail::selected(scope,_worldDraftEntityId) &&
        (scope.part==WorldInspectorDrafts::Part::All || scope.part==WorldInspectorDrafts::Part::Entity)) {
        _worldFormEdited=false; _worldDraftKind=WorldDraftKind::None;
        _worldDraftEntityId.clear(); _worldDraftReloadRequested=true;
    }
}
void EditorWindowView::clearWorldDrafts()
{
    if(_controller._shortcuts.statusMessage==_controller._worldDraftSaveError)_controller._shortcuts.statusMessage.clear();
    _controller._worldDraftSaveError.clear();
    _worldDrafts.clear(); _worldFormEdited=false; _worldDraftKind=WorldDraftKind::None;
    _worldDraftReloadRequested=false; _worldDraftEntityId.clear(); _worldInspectorStatus.clear();
    _worldStockTransferReviewed=false;_worldStockTransfer={};
    _worldNewShopName.clear();_worldNewMerchantName.clear();
    _worldDraftPopupCampaignId.clear();
}
void EditorWindowView::renderWorldDraftStatus()
{
    retainWorldDraft();
    const bool de=gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German;
    const auto &campaign=_controller.currentCampaign();
    const auto history=_controller._workspace.worldHistory;
    const bool pendingUndo=history&&history->hasPendingRegistration();
    const bool allowed=(!_worldDrafts.empty()||_worldRecovery.pending||!_worldRecovery.error.empty()||pendingUndo)&&!campaign.presentationModeActive;
    // Keep the same entry geometry while editing. Recovery detail is opened
    // deliberately instead of inserting a remembered expansion above the form.
    if(drawUiButton(de?"Weltentwürfe prüfen":"Review World drafts",allowed,
        campaign.presentationModeActive?(de?"Private Entwürfe sind im Präsentationsmodus ausgeblendet.":"Private drafts are hidden in presentation mode."):
            (de?"Keine ausstehenden Weltentwürfe.":"No pending World drafts."),UiButtonRole::Secondary,ImVec2(-1,0))) {
        _worldDraftPopupCampaignId=campaign.metadata.id;
        ImGui::OpenPopup("WorldDraftReview");
    }
    TaskUi::preparePickerWindow();
    if(!ImGui::BeginPopup("WorldDraftReview"))return;
    if(!allowed || _worldDraftPopupCampaignId!=campaign.metadata.id) {
        ImGui::CloseCurrentPopup();ImGui::EndPopup();return;
    }
    bool retryUndo=false;
    const auto finishReview=[&] {
        ImGui::EndPopup();
        // Registration can call external history hooks. Keep its owner alive,
        // finish drawing first, and never replay the saved World mutation.
        if(retryUndo)history->retryRegistration();
    };
    if(pendingUndo) {
        ImGui::TextWrapped("%s",de?"Gespeichert. Rückgängig ist noch nicht bereit.":"Saved. Undo is not ready yet.");
        ImGui::TextWrapped("%s",de?"Erneut versuchen wiederholt die gespeicherte Änderung nicht.":
            "Retry does not repeat the saved change.");
        if(!history->registrationError().empty())ImGui::TextWrapped("%s",history->registrationError().c_str());
        retryUndo=drawUiButton(de?"Rückgängig erneut versuchen":"Retry Undo",history->canRetryRegistration(),
            de?"Prüfe zuerst die aktuelle Kampagne und ihre Änderungen.":"Review the current campaign and its changes first.",UiButtonRole::Secondary);
        ImGui::Separator();
    }
    ImGui::TextWrapped("%s",de?"Rückgängig für Weltformulare umfasst gespeicherte Felder von Orten, Nationen, Händlern, Beständen und Gebieten. Käufe, Verkäufe, Erstellen, Löschen und Kartenerzeugung sind davon nicht umfasst.":
        "World form Undo covers saved place, nation, shop, stock and territory fields. It does not cover purchases, sales, creation, deletion or map generation.");
    ImGui::TextWrapped("%s",de?"Beende zuerst die Texteingabe. Nutze dann Bearbeiten > Rückgängig (Strg+Z) oder Bearbeiten > Wiederholen (Strg+Y).":
        "Finish text input first. Then use Edit > Undo (Ctrl+Z) or Edit > Redo (Ctrl+Y).");
    if(renderWorldRecoveredDrafts()){finishReview();return;}
    if(!_worldDrafts.empty()) {
        ImGui::SeparatorText(de?"Nicht gespeicherte Weltentwürfe":"Unsaved World drafts");
        ImGui::TextWrapped("%s",de?"Entwürfe bleiben beim Wechsel erhalten. Speichern übernimmt sie; fehlende oder geänderte Ziele werden nicht überschrieben.":
            "Drafts are retained while navigating. Save applies them; missing or changed targets are not overwritten.");
    }
    if (!_controller.worldDraftSaveError().empty()) ImGui::TextWrapped("%s",_controller.worldDraftSaveError().c_str());
    if(!_worldRecovery.error.empty()) {
        ImGui::TextWrapped("%s",_worldRecovery.error.c_str());
        if(drawUiButton(de?"Sicherung erneut versuchen":"Retry checkpoint",UiButtonRole::Secondary))checkpointWorldRecovery(true);
    }
    EntityId discard,open;
    const auto count=_worldDrafts.places.size()+_worldDrafts.nations.size()+_worldDrafts.shops.size();
    const auto rows=[&](const auto &values,const auto &prose,const char *kind){for(const auto &[id,draft]:values) {
        ImGui::PushID(id.c_str());
        ImGui::TextDisabled("%s",kind);
        ImGui::TextWrapped("%s",draft.edited.name.empty()?(de?"Unbenannter Eintrag":"Untitled entry"):draft.edited.name.c_str());
        if(ImGui::IsItemHovered())ImGui::SetTooltip("ID: %s",id.c_str());
        const bool available=gm::ux::resolveNavigationEntity(_controller.currentCampaign(),id).available;
        if(!available)ImGui::TextWrapped("%s",de?"Ziel fehlt oder ist archiviert; der Entwurf bleibt unten verfügbar.":"Target missing or archived; the retained draft is available below.");
        ImGui::BeginDisabled(!available);
        if (drawSmallUiButton(de?"Eintrag öffnen":"Open entry",UiButtonRole::Secondary)) {open=id;ImGui::CloseCurrentPopup();}
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (drawSmallUiButton(de?"Entwurf kopieren":"Copy retained draft",UiButtonRole::Secondary)) {
            const auto text=prose(draft.edited); ImGui::SetClipboardText(text.c_str());
        }
        if (drawSmallUiButton(de?"Diesen Entwurf verwerfen":"Discard this draft",UiButtonRole::Destructive)) {
            discard=id;
        }
        if (ImGui::TreeNode(de?"Erhaltene Werte":"Retained values")) {
            const auto text=prose(draft.edited); ImGui::TextWrapped("%s",text.c_str()); ImGui::TreePop();
        }
        ImGui::PopID();
    }};
    rows(_worldDrafts.places,[](const auto &v){return WorldDraftText::show(v);},de?"Ort":"Place");
    rows(_worldDrafts.nations,[](const auto &v){return WorldDraftText::show(v);},"Nation");
    rows(_worldDrafts.shops,[](const auto &v){return WorldDraftText::show(v);},de?"Händler":"Shop");
    if (!discard.empty()&&discardWorldDraftsChecked(Scope::entity(discard))&&count==1)ImGui::CloseCurrentPopup();
    finishReview();
    if(retryUndo)return;
    if (!open.empty()) {
        _workspaceFilter.clear(); _worldCategoryFilter=0; _workspaceSelectionId=open;
        reloadWorldInspectorDraft(open);
    }
}
