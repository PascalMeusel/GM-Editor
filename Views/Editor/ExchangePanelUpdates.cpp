#include "Views/Editor/ExchangePanelSupport.hpp"

using namespace ExchangeUi;
void ExchangePanel::drawUpdates(EditorMarkdownExchange &exchange,const Snapshot *snapshot)
{
    using S=CampaignMarkdownExchange::UpdateState;
    ImGui::TextWrapped("%s",tr("Update existing story-thread GM notes in this campaign from an editable export. Select only changes you want to save. Conflicts and missing notes remain unchanged; no other fields or files are imported.",
        "Aktualisiere bestehende SL-Notizen von Handlungssträngen dieser Kampagne aus einem bearbeitbaren Export. Wähle nur Änderungen, die du speichern möchtest. Konflikte und fehlende Notizen bleiben unverändert; andere Felder und Dateien werden nicht importiert."));
    ImGui::TextWrapped("%s",tr("Limits: 4 MiB per note; 64 MiB of original and edited text combined. Keep the export's manifest and original-note files unchanged.",
        "Grenzen: 4 MiB je Notiz; zusammen 64 MiB Original- und bearbeiteter Text. Lass das Manifest und die Originalnotizen des Exports unverändert."));
    label("Editable export folder","Ordner des bearbeitbaren Exports");
    if(ImGui::InputText("##exchangeUpdateSource",&_updateSource)){revokeConsent();_selectedNotes.clear();exchange.cancel();}
    if(button(snapshot?"Refresh changes":"Preview changes",snapshot?"Änderungen aktualisieren":"Änderungen prüfen",!exchange.busy())){
        keyboardReturn();revokeConsent();_selectedNotes.clear();exchange.requestUpdatePreview(std::filesystem::u8path(_updateSource));
    }
    if(!snapshot)return;
    ImGui::Text(tr("%zu notes; %zu selected updates","%zu Notizen; %zu gewählte Änderungen"),snapshot->rows.size(),_selectedNotes.size());
    if(ImGui::BeginChild("UpdateRows",ImVec2(0,ImGui::GetFrameHeightWithSpacing()*5),ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened)){
        const float rowHeight=ImGui::GetFrameHeightWithSpacing()*2;
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(snapshot->rows.size()),rowHeight);
        while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
            const auto &row=snapshot->rows[static_cast<std::size_t>(i)];ImGui::PushID(row.id.c_str());
            const std::string label=std::string(state(row.state))+" — "+singleLine(row.title)+"###UpdateNote";
            if(ImGui::Selectable(label.c_str(),_detailNote==row.id,0,ImVec2(0,ImGui::GetFrameHeight())))_detailNote=row.id;
            bool selected=_selectedNotes.count(row.id)!=0;
            ImGui::BeginDisabled(!exchange.ready()||row.state!=S::Eligible);
            if(ImGui::Checkbox(tr("Select for update","Zum Aktualisieren wählen"),&selected)){
                if(selected)_selectedNotes.insert(row.id);else _selectedNotes.erase(row.id);
                _detailNote=row.id;revokeConsent();
            }
            ImGui::EndDisabled();ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(!exchange.ready()||_selectedNotes.empty());
    FormUi::Checkbox(tr("I reviewed the selected note changes","Ich habe die gewählten Notizänderungen geprüft"),&_reviewed);
    ImGui::EndDisabled();
    const auto found=std::find_if(snapshot->rows.begin(),snapshot->rows.end(),[&](const auto &row){return row.id==_detailNote;});
    if(found==snapshot->rows.end()){
        ImGui::TextWrapped("%s",tr("Choose a note to compare its original, current and edited text.","Wähle eine Notiz, um Original, aktuellen und bearbeiteten Text zu vergleichen."));return;
    }
    ImGui::Separator();text(found->title);ImGui::TextWrapped("%s: %s",tr("Stable note ID","Stabile Notiz-ID"),found->id.c_str());
    ImGui::TextWrapped("%s",state(found->state));
    if(found->state==S::Conflict)ImGui::TextWrapped("%s",tr(
        "The campaign note changed after export. This row cannot be applied. Compare the texts and reconcile the note in the campaign; create a fresh editable export if needed.",
        "Die Kampagnennotiz wurde nach dem Export geändert. Diese Zeile kann nicht übernommen werden. Vergleiche die Texte und gleiche die Notiz in der Kampagne ab; erstelle bei Bedarf einen neuen bearbeitbaren Export."));
    if(found->state==S::Unavailable)ImGui::TextWrapped("%s",tr(
        "This note cannot be updated. Check the campaign, note identity and text limits, then refresh. No missing note is created or restored automatically.",
        "Diese Notiz kann nicht aktualisiert werden. Prüfe Kampagne, Notizkennung und Textgrenzen und aktualisiere die Vorschau. Fehlende Notizen werden nicht automatisch angelegt oder wiederhergestellt."));
    if(!found->reason.empty())diagnostic("NoteReason",found->reason);
    const auto entry=std::find_if(snapshot->updates.entries.begin(),snapshot->updates.entries.end(),[&](const auto &value){return value.id==found->id;});
    if(entry==snapshot->updates.entries.end())return;
    // One text pane keeps all three exact versions readable on narrow screens.
    ImGui::RadioButton(tr("Original at export","Original beim Export"),&_detailVersion,0);
    ImGui::RadioButton(tr("Current campaign note","Aktuelle Kampagnennotiz"),&_detailVersion,1);
    ImGui::RadioButton(tr("Edited file","Bearbeitete Datei"),&_detailVersion,2);
    if(_detailVersion==1&&found->state==S::Unavailable){
        ImGui::TextWrapped("%s",tr("Current note text is unavailable for this entry. See the reason above.",
            "Der aktuelle Notiztext ist für diesen Eintrag nicht verfügbar. Beachte den Grund oben."));return;
    }
    const auto &body=_detailVersion==0?entry->baseText:_detailVersion==1?found->currentText:entry->editedText;
    ImGui::Text(tr("%zu UTF-8 bytes","%zu UTF-8-Bytes"),body.size());rawText("ComparedText",body);
}
