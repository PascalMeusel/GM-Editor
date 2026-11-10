#include "MainMenuCampaignSupport.hpp"

#include <filesystem>

void MainMenuWindowView::openExportPackagePopup(const Campaign &campaign)
{
    _exportCampaignName = campaign.name;
    const std::filesystem::path directory = ProjectPackageExport::defaultExportDirectory();
    _exportDirectory = directory.u8string();
    _exportPackageFolder = ProjectPackageExport::packageExportPath(campaign, directory).filename().u8string();
    _exportPackageStatus.clear();
    _confirmExportOverwrite = false;
    _showExportPackagePopup = true;
    _requestOpenExportPackagePopup = true;
}

void MainMenuWindowView::handleExportPackagePopup()
{
    const char *title = launcherText("Export Campaign Package", "Kampagnenpaket exportieren");
    if (_requestOpenExportPackagePopup)
    {
        ImGui::OpenPopup(localizedUiLabel(title).c_str());
        _requestOpenExportPackagePopup = false;
    }

    setResponsiveWindowSize(ImVec2(680.0f, 420.0f), ImVec2(460.0f, 340.0f));
    if (!ImGui::BeginPopupModal(localizedUiLabel(title).c_str(), &_showExportPackagePopup, ImGuiWindowFlags_NoCollapse))
        return;

    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _exportCampaignName.c_str());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
        "The package includes campaign data, GM notes, and managed assets. Share it only with someone you trust.",
        "Das Paket enthält Kampagnendaten, SL-Notizen und verwaltete Medien. Gib es nur an vertrauenswürdige Personen weiter."));
    ImGui::Spacing();
    ImGui::TextUnformatted(launcherText("Destination folder", "Zielordner"));
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextWithHint(localizedUiLabel("##CampaignExportDirectory").c_str(),
                                  launcherText("Folder for the .ttrpgpkg package", "Ordner für das .ttrpgpkg-Paket"),
                                  &_exportDirectory))
    {
        _confirmExportOverwrite = false;
        _exportPackageStatus.clear();
    }

    std::filesystem::path packagePath;
    bool packageExists = false;
    if (!_exportDirectory.empty())
    {
        packagePath = std::filesystem::u8path(_exportDirectory) / std::filesystem::u8path(_exportPackageFolder);
        std::error_code pathError;
        packageExists = std::filesystem::exists(packagePath, pathError);
        if (pathError)
            _exportPackageStatus = pathError.message();
        ImGui::TextWrapped(localizedUiLabel("%s%s").c_str(),
                           launcherText("Package: ", "Paket: "), packagePath.u8string().c_str());
    }
    if (packageExists)
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
            "A package already exists at this destination.",
            "Am Zielort ist bereits ein Paket vorhanden."));
        FormUi::Checkbox(launcherText("Replace existing package", "Vorhandenes Paket ersetzen"),
                         &_confirmExportOverwrite);
    }

    const bool canExport = !_exportDirectory.empty() && (!packageExists || _confirmExportOverwrite);
    if (drawUiButton(launcherText("Export package", "Paket exportieren"), canExport,
                     launcherText("Choose a folder and confirm replacement when needed.",
                                  "Wähle einen Ordner und bestätige bei Bedarf das Ersetzen."),
                     UiButtonRole::Primary, ImVec2(180.0f, 0.0f)))
    {
        const ProjectPackageExport::ExportResult result = _controller.exportCampaignPackage(
            _exportCampaignName, _exportDirectory, _confirmExportOverwrite);
        _exportPackageStatus = result.success
                                   ? std::string(launcherText("Package exported: ", "Paket exportiert: ")) + result.exportPath.u8string()
                                   : result.errorMessage;
        _confirmExportOverwrite = false;
    }
    ImGui::SameLine();
    if (drawUiButton(launcherText("Close", "Schließen"), UiButtonRole::Secondary, ImVec2(110.0f, 0.0f)))
    {
        _showExportPackagePopup = false;
        ImGui::CloseCurrentPopup();
    }
    if (!_exportPackageStatus.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _exportPackageStatus.c_str());
    }
    ImGui::EndPopup();
}
