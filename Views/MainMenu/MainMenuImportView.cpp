#include "MainMenuCampaignSupport.hpp"

void MainMenuWindowView::openImportPackagePopup()
{
    _showImportPackagePopup = true;
    _requestOpenImportPackagePopup = true;
    _hasImportPreview = false;
    _confirmImportReplacement = false;
    _importConflictChoice = 0;
    _importCloneName.clear();
    _importPackageCompleted = false;
    _importedCampaignName.clear();
    _importPackageError.clear();
}

void MainMenuWindowView::handleImportPackagePopup()
{
    if (_requestOpenImportPackagePopup)
    {
        ImGui::OpenPopup(localizedUiLabel("Import Campaign Package").c_str());
        _requestOpenImportPackagePopup = false;
    }

    setResponsiveWindowSize(ImVec2(720.0f, 520.0f), ImVec2(480.0f, 360.0f));
    if (!ImGui::BeginPopupModal(localizedUiLabel("Import Campaign Package").c_str(), &_showImportPackagePopup, ImGuiWindowFlags_NoCollapse))
        return;

    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                                  "Select the project.json inside an exported .ttrpgpkg folder. Nothing is changed until you review the package and confirm the import.",
                                  "Wähle die project.json in einem exportierten .ttrpgpkg-Ordner. Bis zur Prüfung und Bestätigung wird nichts verändert."));
    ImGui::Spacing();

    if (_importPackageCompleted)
    {
        ImGui::SeparatorText(launcherText("Import Complete", "Import abgeschlossen"));
        ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("%s%s").c_str(),
                           launcherText("Campaign restored: ", "Kampagne wiederhergestellt: "),
                           _importedCampaignName.c_str());
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                                      "The campaign is now in your library. Open it now or return to the refreshed campaign list.",
                                      "Die Kampagne befindet sich jetzt in deiner Bibliothek. Öffne sie jetzt oder kehre zur aktualisierten Kampagnenliste zurück."));
        ImGui::Spacing();
        if (drawUiButton(launcherText("Open Imported Campaign", "Importierte Kampagne öffnen"),
                         UiButtonRole::Primary,
                         ImVec2(220.0f, 0.0f),
                         launcherText("Open the imported campaign at Campaign Home.",
                                      "Die importierte Kampagne auf der Kampagnen-Startseite öffnen.")))
        {
            _controller.openImportedCampaign(_importedCampaignName);
            _showImportPackagePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (drawUiButton(launcherText("Back to Campaigns", "Zur Kampagnenliste"), UiButtonRole::Secondary, ImVec2(180.0f, 0.0f)))
        {
            _showImportPackagePopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return;
    }

    const char *browsePackageLabel =
#if defined(_WIN32)
        launcherText("Browse...", "Durchsuchen...");
#else
        launcherText("Paste Path", "Pfad einfügen");
#endif
    const float browsePackageWidth = ImGui::CalcTextSize(browsePackageLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f;
    const float packagePathRowWidth = ImGui::GetContentRegionAvail().x;
    const bool packagePathInline = packagePathRowWidth >= browsePackageWidth + ImGui::GetStyle().ItemSpacing.x + 220.0f;
    ImGui::SetNextItemWidth(packagePathInline
                                ? packagePathRowWidth - browsePackageWidth - ImGui::GetStyle().ItemSpacing.x
                                : -1.0f);
    if (FormUi::InputTextWithHint(localizedUiLabel("##ImportPackagePath").c_str(), localizedUiText("/path/to/campaign.ttrpgpkg or project.json"), &_importPackagePath))
    {
        _hasImportPreview = false;
        _confirmImportReplacement = false;
        _importConflictChoice = 0;
        _importPackageError.clear();
    }
    if (packagePathInline)
        ImGui::SameLine();
    if (drawUiButton(browsePackageLabel,
                     UiButtonRole::Secondary,
                     ImVec2(packagePathInline ? browsePackageWidth : -1.0f, 0.0f)))
    {
#if defined(_WIN32)
        char fileName[MAX_PATH] = {};
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFilter = "GM Editor package manifest\0project.json\0JSON files\0*.json\0All files\0*.*\0";
        dialog.lpstrFile = fileName;
        dialog.nMaxFile = MAX_PATH;
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&dialog))
        {
            _importPackagePath = fileName;
            _hasImportPreview = false;
            _confirmImportReplacement = false;
            _importPackageError.clear();
        }
#else
        const char *clipboardPath = ImGui::GetClipboardText();
        if (clipboardPath && *clipboardPath)
        {
            _importPackagePath = trimCopy(clipboardPath);
            if (_importPackagePath.size() >= 2 &&
                ((_importPackagePath.front() == '"' && _importPackagePath.back() == '"') ||
                 (_importPackagePath.front() == '\'' && _importPackagePath.back() == '\'')))
                _importPackagePath = _importPackagePath.substr(1, _importPackagePath.size() - 2);
            _hasImportPreview = false;
            _confirmImportReplacement = false;
            _importConflictChoice = 0;
            _importPackageError.clear();
        }
        else
            _importPackageError = launcherText(
                "Copy a package folder or project.json path, then use Paste Path.",
                "Kopiere einen Paketordner oder project.json-Pfad und verwende dann Pfad einfügen.");
#endif
    }

    if (drawUiButton(launcherText("Review Package", "Paket prüfen"),
                     !_importPackagePath.empty(),
                     launcherText("Choose a package first.", "Wähle zuerst ein Paket."),
                     UiButtonRole::Secondary,
                     ImVec2(160.0f, 0.0f),
                     launcherText("Validate the manifest and database without changing your library.",
                                  "Manifest und Datenbank prüfen, ohne die Bibliothek zu verändern.")))
    {
        _importPreview = _controller.previewCampaignPackage(_importPackagePath);
        _hasImportPreview = true;
        _confirmImportReplacement = false;
        _importConflictChoice = 0;
        _importPackageError = _importPreview.errorMessage;
        if (_importPreview.valid && (_importPreview.packageConflict || _importPreview.repositoryConflict))
            _importCloneName = _importPreview.manifest.campaignName + launcherText(" Copy", " Kopie");
    }

    ImGui::Separator();
    if (_hasImportPreview && _importPreview.valid)
    {
        ImGui::SeparatorText(launcherText("Import Preview", "Importvorschau"));
        drawInlineLabelValue(launcherText("Campaign", "Kampagne"), _importPreview.manifest.campaignName.c_str());
        const std::string sizeSummary = std::to_string(_importPreview.fileCount) + launcherText(" files, ", " Dateien, ") +
                                        std::to_string(_importPreview.byteCount / 1024) + " KiB";
        drawInlineLabelValue(launcherText("Contents", "Inhalt"), sizeSummary.c_str());

        const bool conflict = _importPreview.packageConflict || _importPreview.repositoryConflict;
        if (conflict)
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.96f, 0.66f, 0.30f, 1.0f), localizedUiLabel("%s").c_str(), launcherText(
                                                                                     "A campaign with this ID or name already exists.",
                                                                                     "Eine Kampagne mit dieser ID oder diesem Namen existiert bereits."));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                                          "Replacing it creates a package and repository backup before any existing data is changed.",
                                          "Beim Ersetzen wird vor jeder Änderung eine Paket- und Repository-Sicherung erstellt."));
            if (FormUi::RadioButton(launcherText("Replace existing campaign (creates backup)", "Vorhandene Kampagne ersetzen (mit Sicherung)"),
                                   _importConflictChoice == 1))
                _importConflictChoice = 1;
            if (FormUi::RadioButton(launcherText("Import as an independent copy", "Als unabhängige Kopie importieren"),
                                   _importConflictChoice == 2))
                _importConflictChoice = 2;
            _confirmImportReplacement = _importConflictChoice == 1;
            if (_importConflictChoice == 2)
            {
                ImGui::SetNextItemWidth(-1.0f);
                FormUi::InputTextWithHint(localizedUiLabel("##ImportCloneName").c_str(),
                                         launcherText("Name for imported copy", "Name der importierten Kopie"),
                                         &_importCloneName);
                ImGui::TextDisabled(localizedUiLabel("%s").c_str(), launcherText(
                    "The copy receives new campaign and entity IDs; internal links are retained.",
                    "Die Kopie erhält neue Kampagnen- und Objekt-IDs; interne Verknüpfungen bleiben erhalten."));
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("%s").c_str(), launcherText(
                                                                                     "Ready to add as a new campaign.",
                                                                                     "Bereit zum Hinzufügen als neue Kampagne."));
        }
        for (const std::string &warning : _importPreview.warnings)
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), warning.c_str());

        ImGui::Separator();
        const bool conflictChoiceReady = !conflict || _importConflictChoice == 1 ||
                                         (_importConflictChoice == 2 && !trimCopy(_importCloneName).empty());
        if (drawUiButton(launcherText("Import Campaign", "Kampagne importieren"),
                         conflictChoiceReady,
                         launcherText("Choose Replace or enter a name for an independent copy.",
                                      "Wähle Ersetzen oder gib einen Namen für eine unabhängige Kopie ein."),
                         UiButtonRole::Primary,
                         ImVec2(180.0f, 0.0f),
                         launcherText("Copy the package and restore its campaign into the library.",
                                      "Paket kopieren und seine Kampagne in der Bibliothek wiederherstellen.")))
        {
            ProjectPackageImport::ConflictResolution resolution = ProjectPackageImport::ConflictResolution::Cancel;
            if (conflict && _importConflictChoice == 1)
                resolution = ProjectPackageImport::ConflictResolution::ReplaceExisting;
            else if (conflict && _importConflictChoice == 2)
                resolution = ProjectPackageImport::ConflictResolution::CloneAsCopy;
            const ProjectPackageImport::ImportResult result =
                _controller.importCampaignPackage(_importPackagePath, resolution, trimCopy(_importCloneName));
            if (result.success)
            {
                _statusMessage = launcherText("Imported campaign: ", "Kampagne importiert: ") + result.importedCampaignName;
                _importedCampaignName = result.importedCampaignName;
                _importPackageCompleted = true;
                _hasImportPreview = false;
            }
            else
            {
                _importPackageError = result.errorMessage;
            }
        }
    }

    if (!_importPackageError.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("%s").c_str(), _importPackageError.c_str());
    }

    ImGui::SameLine();
    if (drawUiButton(launcherText("Cancel", "Abbrechen"), UiButtonRole::Secondary, ImVec2(120.0f, 0.0f)))
    {
        _showImportPackagePopup = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
