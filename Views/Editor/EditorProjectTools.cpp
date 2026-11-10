#include "Views/Editor/EditorWorkspacesSupport.hpp"
#include "Models/Project/DiagnosticReport.hpp"
#include "Models/Project/AtomicProjectFile.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"
#include "Controllers/Editor/EditorMarkdownExchange.hpp"

namespace
{
const char *uiText(const char *english, const char *german)
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
               ? german : english;
}
}

void EditorWindowView::renderProjectTools()
{
    ImGui::SeparatorText(uiText("Data exchange and support", "Datenaustausch und Hilfe"));
    UiGuidance::drawDetails(UiGuidance::Topic::DataSafety, uiText("Save, backup or export?", "Speichern, sichern oder exportieren?"));
    UiGuidance::drawDetails(UiGuidance::Topic::Terms, uiText("Terms and actions", "Begriffe und Aktionen"));
    _exchange.drawLauncher(*_controller._workspace.markdownExchange);
    if (drawUiButton(uiText("Preview diagnostic export", "Diagnoseexport prüfen"), UiButtonRole::Secondary))
    {
        const auto &save = ProjectSaveService::getInstance();
        _diagnosticPreview = DiagnosticReport::preview(_controller.currentCampaign(), save.state(), save.config());
        _diagnosticStatus.clear(); ImGui::OpenPopup("DiagnosticExport");
    }
    const auto size = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize(ImVec2(std::min(size.x - 40, 650.0f), std::min(size.y - 40, 590.0f)), ImGuiCond_Appearing);
    if (ImGui::BeginPopup("DiagnosticExport"))
    {
        ImGui::TextWrapped("%s", uiText("This is the complete exported content. Nothing is sent automatically.",
            "Dies ist der vollständige Exportinhalt. Es wird nichts automatisch versendet."));
        FormUi::InputTextMultiline("##diagnostic-preview", &_diagnosticPreview, ImVec2(-1, ImGui::GetContentRegionAvail().y - 95), ImGuiInputTextFlags_ReadOnly);
        if (drawUiButton(uiText("Export this summary", "Diese Zusammenfassung exportieren"), UiButtonRole::Primary))
        {
            try
            {
                const auto directory = ProjectPackageLayout::layoutForCampaign(_controller.currentCampaign()).exportsDirectory / "Diagnostics";
                std::filesystem::create_directories(directory);
                const auto output = directory / ("diagnostic_" + AtomicProjectFile::uniqueSuffix() + ".txt");
                AtomicProjectFile::write(output, _diagnosticPreview);
                _diagnosticStatus = output.u8string();
            }
            catch (const std::exception &error) { _diagnosticStatus = error.what(); }
        }
        if (!_diagnosticStatus.empty()) ImGui::TextWrapped("%s", _diagnosticStatus.c_str());
        if (drawUiButton(uiText("Close", "Schließen"), UiButtonRole::Secondary)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
