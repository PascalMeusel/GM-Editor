#include "Views/Editor/EditorViewDetails.hpp"

const char *EditorWindowView::worldRecoveryAttentionTooltip() const
{
    const bool de=gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German;
    // Cached generic state only: no paths, checkpoint bodies or filesystem work.
    if(_worldRecovery.pending)return de?"Gesicherte Weltdaten müssen geprüft werden. Welt > Weltentwürfe prüfen.":
        "Saved World checkpoint needs review. World > Review World drafts.";
    if(_worldRecovery.error.empty())return "";
    if(_worldRecovery.cleanup)return de?"Kampagnenänderungen sind gespeichert; die Weltsicherung muss noch bereinigt werden.":
        "Campaign changes were saved; World recovery cleanup still needs retry.";
    return de?"Die Weltsicherung benötigt Aufmerksamkeit. Lass Entwürfe geöffnet und prüfe den Ordnerzugriff.":
        "World recovery needs attention. Keep drafts open and check folder access before retrying.";
}

bool EditorWindowView::renderWorldRecoveredDrafts()
{
    if(!_worldRecovery.pending)return false;
    const bool de=gm::localization::LocalizationService::instance().locale()==gm::localization::Locale::German;
    ImGui::SeparatorText(de?"Gesicherte Weltentwürfe":"Saved World checkpoint");
    ImGui::TextWrapped("%s",de?
        "Die Sicherung wurde noch nicht übernommen. Wiederherstellen öffnet Entwürfe zur Prüfung; erst Speichern ändert die Kampagne.":
        "The checkpoint has not been restored. Restore opens drafts for review; only Save changes the campaign.");
    const auto &read=_worldRecovery.recovered;
    if(read.status==WorldDraftRecovery::ReadStatus::Ready) {
        ImGui::Text(de?"Orte: %d · Nationen: %d · Händler: %d":"Places: %d · Nations: %d · Shops: %d",
            int(read.drafts.places.size()),int(read.drafts.nations.size()),int(read.drafts.shops.size()));
        if(drawUiButton(de?"Zur Prüfung wiederherstellen":"Restore for review",_worldDrafts.empty()&&!_worldFormEdited,
            de?"Vorhandene Entwürfe zuerst prüfen.":"Review existing drafts first.",UiButtonRole::Primary)) {
            if(!worldRecoveryBound()){worldRecoveryReady();return true;}
            _worldDrafts=read.drafts;_worldRecovery.pending=false;
            _worldRecovery.lastPayload=WorldDraftRecovery::encode(_worldRecovery.campaignId,_worldDrafts).payload;
            _worldRecovery.dirty=false;setWorldRecoveryError({});_worldRecovery.recovered={};
            _worldDraftKind=WorldDraftKind::None;_worldDraftEntityId.clear();_worldDraftReloadRequested=true;
            return true;
        }
    }
    else ImGui::TextWrapped("%s",de?"Diese Sicherung kann derzeit nicht wiederhergestellt werden. Sie bleibt unverändert erhalten.":
        "This checkpoint cannot currently be restored. Its bytes remain unchanged.");
    if(!read.error.empty())ImGui::TextWrapped("%s",read.error.c_str());
    if(!_worldRecovery.error.empty())ImGui::TextWrapped("%s",_worldRecovery.error.c_str());
    if(drawUiButton(de?"Sicherung kopieren":"Copy checkpoint",read.rawAvailable,
        de?"Die Sicherungsdatei konnte nicht vollständig gelesen werden.":"The complete checkpoint bytes could not be read.",UiButtonRole::Secondary)) {
        if(!worldRecoveryBound()){worldRecoveryReady();return true;}
        const auto copy=WorldDraftRecovery::copyCheckpoint(read);
        if(copy.success)ImGui::SetClipboardText(copy.text.c_str());else setWorldRecoveryError(copy.error);
    }
    if(drawUiButton(de?"Sicherung erneut lesen":"Retry checkpoint read",UiButtonRole::Secondary)) {
        if(worldRecoveryBound()){_worldRecovery.recovered=WorldDraftRecovery::read(_controller.currentCampaign());
            _worldRecovery.pending=_worldRecovery.recovered.status!=WorldDraftRecovery::ReadStatus::Absent;setWorldRecoveryError({});}
        else worldRecoveryReady();
        return true;
    }
    if(drawUiButton(de?"Gesicherte Entwürfe verwerfen":"Discard saved drafts",UiButtonRole::Destructive)) {
        if(discardWorldDraftsChecked(WorldInspectorDrafts::Scope::all()))ImGui::CloseCurrentPopup();
    }
    return true;
}
void EditorWindowView::renderWorldRecoveryNotice()
{
    if(_controller.currentCampaign().presentationModeActive) {
        ImGui::TextWrapped("%s",uiTextForLocale("Private World recovery is hidden during presentation.",
            "Private Weltsicherungen sind im Präsentationsmodus ausgeblendet."));return;}
    ImGui::TextWrapped("%s",uiTextForLocale("Open Review World drafts above to restore or discard the saved checkpoint before editing.",
        "Öffne oben Weltentwürfe prüfen, um die Sicherung vor dem Bearbeiten wiederherzustellen oder zu verwerfen."));
}
