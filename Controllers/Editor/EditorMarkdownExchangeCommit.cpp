#include "EditorMarkdownExchange.hpp"
#include "EditorWindowControllerSupport.hpp"

void EditorMarkdownExchange::commit(const CampaignMarkdownExchange::TaskResult &checked)
{
    using namespace CampaignMarkdownExchange;
    const auto held=shared_from_this();const auto stamp=context_;const auto selected=selected_;
    const auto valid=[&]{return current(stamp);};
    if(!valid()){reset();return;}
    const DurableSave save=[&](Campaign &staged){
        if(!valid())return ProjectSaveResult{ProjectSaveOutcome::NotCommitted,"The exchange context changed.",{}};
        return ProjectSaveService::getInstance().saveCampaignDetailed(staged,"reviewed Markdown notes");
    };
    auto &campaign=owner_->_currentCampaign;
    const auto result=checked.kind==TaskKind::CheckImport?applyImportedNote(campaign,checked.imported,save,valid):
        applyUpdates(campaign,checked.updates,selected,save,valid);
    if(result.state==ApplyState::NotCommitted){status_=valid()?Status::Ready:Status::Failed;message_=result.error;return;}
    ++token_;selected_.clear();status_=Status::Idle;
    if(result.state==ApplyState::CommittedContextChanged){
        message_=uiText("Saved, but the campaign or selected note changed during save. Newer state was retained. Prepare a new preview before continuing.",
            "Gespeichert, aber die Kampagne oder eine gewählte Notiz wurde währenddessen geändert. Der neuere Stand blieb erhalten. Bereite vor dem Fortfahren eine neue Vorschau vor.");
        if(valid())ProjectSaveService::getInstance().markDirty("newer note state retained after Markdown save");
        review_.reset();export_.reset();return;
    }
    if(valid())owner_->markSearchIndexDirty();
    message_=result.state==ApplyState::NoChange?uiText("Already applied. No additional save was needed.","Bereits übernommen. Erneutes Speichern war nicht nötig."):
        checked.kind==TaskKind::CheckImport?uiText("Saved a new private story-thread note.","Neue private Handlungsstrang-Notiz gespeichert."):
        uiText("Saved the selected GM notes and their update provenance.","Gewählte SL-Notizen und ihre Aktualisierungsherkunft gespeichert.");
}
