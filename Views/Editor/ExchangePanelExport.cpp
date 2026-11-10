#include "Views/Editor/ExchangePanelSupport.hpp"

using namespace ExchangeUi;
void ExchangePanel::drawExport(EditorMarkdownExchange &exchange,const Snapshot *snapshot)
{
    const bool editableUnavailable=snapshot&&!snapshot->inventory.editableError.empty();
    if(editableUnavailable&&_editable)revokeConsent();
    ImGui::TextWrapped("%s",tr("Includes secrets and private GM notes. Use the session's Player recap for players.",
        "Enthält Geheimnisse und private SL-Notizen. Für Spieler verwende den Spieler-Rückblick der Sitzung."));
    label("New destination folder (parent must exist)","Neuer Zielordner (übergeordneter Ordner muss existieren)");
    ImGui::BeginDisabled(exchange.busy());
    if(ImGui::InputText("##exchangeDestination",&_destination))revokeConsent();
    ImGui::BeginDisabled(editableUnavailable&&!_editable);
    if(FormUi::Checkbox(tr("Include editable story-thread notes","Bearbeitbare Handlungsstrang-Notizen einschließen"),&_editable))revokeConsent();
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if(editableUnavailable){
        ImGui::TextWrapped("%s",tr("Editable notes are unavailable in this preview. Check the note sizes, text format and stable identities.",
            "Bearbeitbare Notizen sind in dieser Vorschau nicht verfügbar. Prüfe Notizgrößen, Textformat und stabile Kennungen."));
        ImGui::TextWrapped("%s",_editable?tr("Clear the editable option to export reading copies.","Deaktiviere die bearbeitbare Ausgabe, um Lesekopien zu exportieren."):
            tr("Reading-copy export is still available.","Der Export von Lesekopien ist weiterhin möglich."));
        diagnostic("EditableError",snapshot->inventory.editableError);
    }
    if(_editable)ImGui::TextWrapped("%s",tr("Only story-thread GM notes can be updated from editable files. Other exported pages are reading copies. Titles, scenes, links and attachments are not updated. Limits: 4 MiB per note; 64 MiB for original and edited text combined.",
        "Nur SL-Notizen von Handlungssträngen können aus bearbeitbaren Dateien aktualisiert werden. Andere exportierte Seiten sind Lesekopien. Titel, Szenen, Verknüpfungen und Anhänge werden nicht aktualisiert. Grenzen: 4 MiB je Notiz; zusammen 64 MiB für Original- und bearbeiteten Text."));
    if(button(snapshot?"Refresh export inventory":"Prepare export inventory",snapshot?"Exportübersicht aktualisieren":"Exportübersicht vorbereiten",!exchange.busy())){
        keyboardReturn();revokeConsent();exchange.requestExportInventory();
    }
    if(!snapshot)return;
    const auto &preview=snapshot->inventory;
    ImGui::Text(tr("%zu note files; %zu available attachments","%zu Notizdateien; %zu verfügbare Anhänge"),preview.documents.size(),preview.attachments.size());
    if(ImGui::TreeNode(tr("Exported pages","Exportierte Seiten"))){
        if(ImGui::BeginChild("ExportedPages",ImVec2(0,ImGui::GetTextLineHeightWithSpacing()*5),ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar)){
            ImGuiListClipper clipper;clipper.Begin(static_cast<int>(preview.documents.size()),ImGui::GetTextLineHeightWithSpacing());
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
                const auto &d=preview.documents[static_cast<std::size_t>(i)];
                ImGui::Text("%s: %s [%s]",d.type.c_str(),singleLine(d.title).c_str(),d.id.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::TreePop();
    }
    ImGui::TextWrapped("%s",tr("Choose attachments deliberately. None is included automatically.","Wähle Anhänge bewusst aus. Kein Anhang wird automatisch aufgenommen."));
    ImGui::BeginDisabled(!exchange.ready());
    if(ImGui::BeginChild("AttachmentInventory",ImVec2(0,ImGui::GetFrameHeightWithSpacing()*4),ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened)){
        ImGuiListClipper clipper;clipper.Begin(static_cast<int>(preview.attachments.size()),ImGui::GetFrameHeightWithSpacing());
        while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){
            const auto &a=preview.attachments[static_cast<std::size_t>(i)];ImGui::PushID(a.relativePath.c_str());
            bool chosen=_attachments.count(a.relativePath)!=0;
            const auto caption=singleLine(a.relativePath)+"###AttachmentChoice";
            if(ImGui::Checkbox(caption.c_str(),&chosen)){
                if(chosen)_attachments.insert(a.relativePath);else _attachments.erase(a.relativePath);revokeConsent();
            }
            showHelpTooltip(a.relativePath.c_str());ImGui::PopID();
        }
    }
    ImGui::EndChild();ImGui::EndDisabled();
    if(!preview.warnings.empty()&&ImGui::TreeNode(tr("Omitted attachments","Ausgelassene Anhänge"))){
        for(const auto &warning:preview.warnings)text(warning);ImGui::TreePop();
    }
    ImGui::BeginDisabled(!exchange.ready()||(_editable&&editableUnavailable));
    FormUi::Checkbox(tr("I reviewed this private GM export and the selected attachments","Ich habe den privaten SL-Export und die gewählten Anhänge geprüft"),&_reviewed);
    ImGui::EndDisabled();
}
