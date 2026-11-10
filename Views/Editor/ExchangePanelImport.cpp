#include "Views/Editor/ExchangePanelSupport.hpp"

using namespace ExchangeUi;
void ExchangePanel::drawImport(EditorMarkdownExchange &exchange,const Snapshot *snapshot)
{
    ImGui::TextWrapped("%s",tr("Import one UTF-8 .md file (up to 4 MiB) as a new private story-thread note. Original text is preserved exactly. Links and frontmatter remain text; existing entries and attachments are not imported.",
        "Importiere eine UTF-8-.md-Datei (bis 4 MiB) als neue private Notiz in einem Handlungsstrang. Der Originaltext bleibt exakt erhalten. Links und Frontmatter bleiben Text; bestehende Einträge und Anhänge werden nicht importiert."));
    label("Markdown file path","Pfad zur Markdown-Datei");
    if(ImGui::InputText("##exchangeSource",&_source)){revokeConsent();exchange.cancel();}
    if(button(snapshot?"Refresh file preview":"Preview file",snapshot?"Dateivorschau aktualisieren":"Datei als Vorschau laden",!exchange.busy())){
        keyboardReturn();revokeConsent();exchange.requestSingleImportPreview(std::filesystem::u8path(_source));
    }
    if(!snapshot)return;
    ImGui::TextWrapped("%s: %s",tr("New story-thread title","Titel des neuen Handlungsstrangs"),snapshot->imported.title.c_str());
    ImGui::Text(tr("%zu original UTF-8 bytes","%zu originale UTF-8-Bytes"),snapshot->imported.text.size());
    ImGui::BeginDisabled(!exchange.ready());
    FormUi::Checkbox(tr("I reviewed the text to import","Ich habe den zu importierenden Text geprüft"),&_reviewed);
    ImGui::EndDisabled();
    ImGui::SeparatorText(tr("Original text","Originaltext"));rawText("ImportedText",snapshot->imported.text);
}
