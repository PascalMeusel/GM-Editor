#include "Views/Editor/ExchangePanelSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"
#include "Views/Editor/Widgets/UiPopupPlacement.hpp"

using namespace ExchangeUi;
namespace
{
const char *action(EditorMarkdownExchange::Kind kind,bool editable)
{
    if(kind==EditorMarkdownExchange::Kind::Import)return tr("Import as private story-thread note","Als private Handlungsstrang-Notiz importieren");
    if(kind==EditorMarkdownExchange::Kind::Updates)return tr("Apply selected updates","Gewählte Änderungen übernehmen");
    return editable?tr("Export editable notes","Bearbeitbare Notizen exportieren"):tr("Export reviewed GM notes","Geprüfte SL-Notizen exportieren");
}
std::string scope(EditorMarkdownExchange::Kind kind,std::size_t selected)
{
    if(kind==EditorMarkdownExchange::Kind::Updates)return std::to_string(selected)+tr(" selected notes will change."," gewählte Notizen werden geändert.");
    if(kind==EditorMarkdownExchange::Kind::Import)return tr("Creates one new private story-thread note.","Erstellt eine neue private Handlungsstrang-Notiz.");
    return tr("Exports a private copy; the campaign is unchanged.","Exportiert eine private Kopie; die Kampagne bleibt unverändert.");
}
}
void ExchangePanel::reset()
{
    const bool opened=_wasOpen||_closeRequested;
    *this={};_closeRequested=opened;
}
void ExchangePanel::revokeConsent(){_reviewed=false;}
void ExchangePanel::keyboardReturn()
{if(ImGui::IsItemFocused()&&ImGui::IsAnyItemFocused())_focusReturn=true;}
void ExchangePanel::drawStatus(const EditorMarkdownExchange &exchange,const Snapshot *snapshot)
{
    if(exchange.busy()){
        const auto progress=exchange.progress();
        const char *phase=tr("Preparing…","Wird vorbereitet …");
        using P=CampaignMarkdownExchange::TaskPhase;
        if(progress.phase==P::Reading)phase=tr("Reading files…","Dateien werden gelesen …");
        if(progress.phase==P::Inventory)phase=tr("Checking file inventory…","Dateiübersicht wird geprüft …");
        if(progress.phase==P::Copying)phase=tr("Copying reviewed files…","Geprüfte Dateien werden kopiert …");
        if(exchange.status()==EditorMarkdownExchange::Status::CheckingApply)phase=tr("Checking reviewed files before saving…","Geprüfte Dateien werden vor dem Speichern geprüft …");
        ImGui::TextWrapped("%s",phase);
        if(progress.files||progress.bytes)ImGui::Text(tr("%llu files; %llu bytes processed","%llu Dateien; %llu Bytes verarbeitet"),
            static_cast<unsigned long long>(progress.files),static_cast<unsigned long long>(progress.bytes));
    }
    if(!exchange.message().empty()){
        if(exchange.status()==EditorMarkdownExchange::Status::Failed||exchange.status()==EditorMarkdownExchange::Status::Ready){
            ImGui::TextWrapped("%s",tr("The operation could not finish. Check the details, then retry or prepare a new preview. Nothing is retried automatically.",
                "Der Vorgang konnte nicht abgeschlossen werden. Prüfe die Details und versuche es erneut oder bereite eine neue Vorschau vor. Es wird nichts automatisch wiederholt."));
            diagnostic("OperationError",exchange.message());
        }else text(exchange.message());
    }
    if(snapshot&&!exchange.ready())ImGui::TextWrapped("%s",tr("Previous preview — read only. Prepare or refresh a preview before continuing.",
        "Vorherige Vorschau — nur lesbar. Bereite eine Vorschau vor oder aktualisiere sie, bevor du fortfährst."));
}
void ExchangePanel::drawLauncher(EditorMarkdownExchange &exchange)
{
    // Gate all cached paths, note text and errors before accessing the snapshot.
    if(!exchange.privateAvailable()){
        if(_wasOpen)exchange.cancel();
        _wasOpen=false;revokeConsent();
        ImGui::TextWrapped("%s",tr("Private note exchange is unavailable in this context. Reopen it in GM mode.",
            "Der Austausch privater Notizen ist in diesem Kontext nicht verfügbar. Öffne ihn erneut in der SL-Ansicht."));
        return;
    }
    if(_closeRequested&&!ImGui::IsPopupOpen("CampaignMarkdownExchange"))_closeRequested=false;
    if(button("Exchange campaign notes","Kampagnennotizen austauschen")){
        _closeRequested=false;ImGui::OpenPopup("CampaignMarkdownExchange");
    }
    PopupUi::prepareFixed("CampaignMarkdownExchange",ImVec2(860,760));
    if(!ImGui::BeginPopup("CampaignMarkdownExchange")){
        if(_wasOpen)exchange.cancel();
        _wasOpen=false;revokeConsent();return;
    }
    _wasOpen=true;
    exchange.uiActive();
    if(_closeRequested){_closeRequested=false;_wasOpen=false;ImGui::CloseCurrentPopup();ImGui::EndPopup();exchange.cancel();return;}
    auto snapshot=exchange.snapshot();
    const auto token=snapshot?snapshot->token:0;
    if(token!=_seenToken){_seenToken=token;revokeConsent();_attachments.clear();_selectedNotes.clear();_detailNote.clear();}
    if(!exchange.ready())revokeConsent();
    ImGui::TextWrapped("%s",tr("Private Markdown notes for other note tools. This does not replace a campaign backup.",
        "Privates Markdown für andere Notizprogramme. Dies ersetzt keine Kampagnensicherung."));
    UiGuidance::drawDetails(UiGuidance::Topic::DataSafety,tr("Choose an exchange format","Austauschformat wählen"));
    const float width=std::max(1.f,ImGui::GetContentRegionAvail().x),spacing=ImGui::GetStyle().ItemSpacing.y;
    const float half=std::max(1.f,(width-ImGui::GetStyle().ItemSpacing.x)*.5f);
    const char *cancel=tr("Cancel operation","Vorgang abbrechen"),*close=tr("Return to campaign","Zurück zur Kampagne");
    float primaryHeight=0,scopeHeight=0;
    for(auto kind:{Kind::Export,Kind::Import,Kind::Updates}){
        primaryHeight=std::max(primaryHeight,gm::presentation_review_ui::buttonHeight(action(kind,_editable),width));
        scopeHeight=std::max(scopeHeight,ImGui::CalcTextSize(scope(kind,_selectedNotes.size()).c_str(),nullptr,false,width).y);
    }
    const float secondaryHeight=std::max(gm::presentation_review_ui::buttonHeight(cancel,half),
        gm::presentation_review_ui::buttonHeight(close,half));
    const float footer=scopeHeight+primaryHeight+secondaryHeight+spacing*3;
    const auto previousTab=_tab;
    if(ImGui::BeginTabBar("MarkdownExchangeTabs")){
        const auto tab=[&](Kind kind,const char *label){
            if(!ImGui::BeginTabItem(label))return;
            _tab=kind;
            if(previousTab!=kind){exchange.cancel();revokeConsent();}
            ImGui::PushID(static_cast<int>(kind));
            const float height=std::max(1.f,ImGui::GetContentRegionAvail().y-footer);
            if(ImGui::BeginChild("ExchangeBody",ImVec2(0,height),ImGuiChildFlags_NavFlattened)){
                const auto *matching=snapshot&&snapshot->kind==kind?snapshot.get():nullptr;
                drawStatus(exchange,matching);
                if(kind==Kind::Export)drawExport(exchange,matching);
                else if(kind==Kind::Import)drawImport(exchange,matching);
                else drawUpdates(exchange,matching);
            }
            ImGui::EndChild();ImGui::PopID();ImGui::EndTabItem();
        };
        tab(Kind::Export,tr("Export GM notes","SL-Notizen exportieren"));
        tab(Kind::Import,tr("Import Markdown","Markdown importieren"));
        tab(Kind::Updates,tr("Update exported notes","Exportierte Notizen aktualisieren"));
        ImGui::EndTabBar();
    }
    const bool ready=exchange.ready()&&snapshot&&snapshot->kind==_tab;
    const bool selected=_tab!=Kind::Updates||!_selectedNotes.empty();
    const bool destination=_tab!=Kind::Export||!_destination.empty();
    const bool format=_tab!=Kind::Export||!_editable||(snapshot&&snapshot->inventory.editableError.empty());
    ImGui::TextWrapped("%s",scope(_tab,_selectedNotes.size()).c_str());
    const bool submit=drawUiButton(action(_tab,_editable),ready&&_reviewed&&selected&&destination&&format,
        tr("Prepare a current preview, select changes if needed, and review it first.","Bereite eine aktuelle Vorschau vor, wähle gegebenenfalls Änderungen und prüfe sie zuerst."),
        UiButtonRole::Primary,ImVec2(width,primaryHeight));
    if(submit)keyboardReturn();
    const bool stop=drawUiButton(cancel,exchange.busy(),tr("No operation is running.","Es läuft kein Vorgang."),UiButtonRole::Secondary,ImVec2(half,secondaryHeight));
    if(stop)keyboardReturn();
    ImGui::SameLine();
    if(_focusReturn){_focusReturn=false;ImGui::SetKeyboardFocusHere();}
    const bool leave=drawUiButton(close,UiButtonRole::Secondary,ImVec2(half,secondaryHeight));
    if(leave){ImGui::CloseCurrentPopup();_wasOpen=false;}
    ImGui::EndPopup();
    // Requests own their input; no callback retains a popup-body reference.
    if(stop||leave){revokeConsent();exchange.cancel();}
    else if(submit){
        revokeConsent();
        if(_tab==Kind::Export)exchange.requestExport(token,{_attachments.begin(),_attachments.end()},std::filesystem::u8path(_destination),_editable);
        else if(_tab==Kind::Import)exchange.requestImport(token);
        else exchange.requestApplyUpdates(token,{_selectedNotes.begin(),_selectedNotes.end()});
    }
}
