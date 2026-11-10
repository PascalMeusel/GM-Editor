#include "MainMenuCampaignSupport.hpp"
#include <algorithm>

namespace
{
using ProjectBackupRestore::BackupEntry;
std::string backupKey(const std::filesystem::path &path, const std::string &campaignId)
{ return path.lexically_normal().generic_u8string() + "\n" + campaignId; }
std::string backupKey(const BackupEntry &entry) { return backupKey(entry.snapshotPath, entry.campaignId); }
const BackupEntry *selectedBackup(const std::vector<BackupEntry> &entries, const std::string &key)
{
    const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) { return backupKey(entry) == key; });
    return found == entries.end() ? nullptr : &*found;
}
const char *backupKindLabel(const BackupEntry &entry)
{
    switch (entry.kind)
    {
    case ProjectBackupRestore::BackupKind::ImportReplacement: return launcherText("Before import", "Vor dem Import");
    case ProjectBackupRestore::BackupKind::BeforeRestore: return launcherText("Before restore", "Vor der Wiederherstellung");
    default: return launcherText("Before saving", "Vor dem Speichern");
    }
}
std::string backupSizeLabel(std::uintmax_t bytes) { return std::to_string((bytes + 1023U) / 1024U) + " KiB"; }
std::string backupTimeLabel(const BackupEntry &entry)
{
    if (entry.timestampUtc.empty()) return launcherText("Time unavailable", "Zeit nicht verfügbar");
    std::string value = entry.timestampUtc;
    if (value.size() >= 19 && value[10] == 'T') value[10] = ' ';
    if (!value.empty() && value.back() == 'Z') value.pop_back();
    return value + " UTC";
}
void backupDetail(const char *label, const std::string &value)
{ ImGui::TextDisabled("%s", label); ImGui::TextWrapped("%s", value.c_str()); }
void backupScope(const BackupEntry &entry)
{
    ImGui::TextWrapped("%s", entry.campaignPresent
        ? launcherText("Restore the saved data of this campaign. Other campaigns remain unchanged.",
                       "Die gesicherten Daten dieser Kampagne wiederherstellen. Andere Kampagnen bleiben unverändert.")
        : launcherText("This campaign did not exist before the earlier restore. Reversing that restore removes only this campaign from the library. Other campaigns remain unchanged.",
                       "Diese Kampagne existierte vor der früheren Wiederherstellung nicht. Deren Rücknahme entfernt nur diese Kampagne aus der Bibliothek. Andere Kampagnen bleiben unverändert."));
    switch (entry.packageAction)
    {
    case ProjectBackupRestore::PackageRestoreAction::Restore:
        ImGui::TextWrapped("%s", launcherText("Replace this campaign's managed package files with the saved package, including its assets.",
            "Die verwalteten Paketdateien dieser Kampagne einschließlich ihrer Assets durch das gesicherte Paket ersetzen.")); break;
    case ProjectBackupRestore::PackageRestoreAction::Remove:
        ImGui::TextWrapped("%s", launcherText("No managed package existed at that time. Remove this campaign's current managed package and its files.",
            "Damals existierte kein verwaltetes Paket. Das aktuelle verwaltete Paket dieser Kampagne und seine Dateien entfernen.")); break;
    default:
        ImGui::TextWrapped("%s", launcherText("Existing managed package files and assets remain unchanged.",
            "Vorhandene verwaltete Paketdateien und Assets bleiben unverändert.")); break;
    }
    if (entry.legacySafety)
        ImGui::TextWrapped("%s", launcherText("Older safety backup: prior package presence was not recorded. Only the listed campaign data can be restored.",
            "Ältere Sicherheitssicherung: Der frühere Paketbestand wurde nicht erfasst. Nur die aufgeführten Kampagnendaten können wiederhergestellt werden."));
}
}

void MainMenuWindowView::refreshBackupBrowser(const std::filesystem::path &selectPath, const std::string &campaignId)
{
    const std::string desired = selectPath.empty() ? _selectedBackupKey : backupKey(selectPath, campaignId);
    _backupEntries = _controller.listCampaignBackups(_backupBrowserStatus);
    _confirmBackupRestore = false;
    _selectedBackupKey = selectedBackup(_backupEntries, desired) ? desired : std::string{};
    if (!desired.empty() && _selectedBackupKey.empty())
    {
        if (!_backupBrowserStatus.empty()) _backupBrowserStatus += "\n";
        _backupBrowserStatus += launcherText("The selected backup is no longer available. Choose a backup to review.",
                                             "Die ausgewählte Sicherung ist nicht mehr verfügbar. Wähle eine Sicherung zur Prüfung.");
    }
    else if (_backupEntries.empty() && _backupBrowserStatus.empty())
        _backupBrowserStatus = launcherText("No managed campaign backups were found.", "Keine verwalteten Kampagnensicherungen gefunden.");
}

void MainMenuWindowView::openBackupBrowserPopup()
{
    refreshBackupBrowser();
    _showBackupBrowserPopup = true;
    _requestOpenBackupBrowserPopup = true;
}

void MainMenuWindowView::handleBackupBrowserPopup()
{
    const std::string title = std::string(launcherText("Campaign backups", "Kampagnensicherungen")) + "###CampaignBackupBrowser";
    if (_requestOpenBackupBrowserPopup)
    { ImGui::OpenPopup(title.c_str()); _requestOpenBackupBrowserPopup = false; }
    setResponsiveWindowSize(ImVec2(920, 680), ImVec2(520, 520));
    if (!ImGui::BeginPopupModal(title.c_str(), &_showBackupBrowserPopup, ImGuiWindowFlags_NoCollapse)) return;

    const auto *selected = selectedBackup(_backupEntries, _selectedBackupKey);
    const char *restoreLabel = selected && !selected->campaignPresent
        ? launcherText("Remove selected campaign", "Ausgewählte Kampagne entfernen")
        : launcherText("Restore selected backup", "Ausgewählte Sicherung wiederherstellen");
    const char *safetyLabel = launcherText("Review safety backup", "Sicherheitssicherung prüfen");
    const char *refreshLabel = launcherText("Refresh", "Aktualisieren");
    const char *closeLabel = launcherText("Close", "Schließen");
    const auto &style = ImGui::GetStyle();
    const float width = ImGui::GetContentRegionAvail().x;
    const auto buttonHeight = [&](const char *label) {
        return std::max(ImGui::GetFrameHeight(), ImGui::CalcTextSize(label, nullptr, false,
            std::max(1.f, width - style.FramePadding.x * 2)).y + style.FramePadding.y * 2); };
    const float statusHeight = ImGui::GetTextLineHeightWithSpacing() * 2.5f;
    const float footer = statusHeight + ImGui::GetFrameHeight() + buttonHeight(restoreLabel) + buttonHeight(safetyLabel)
        + ImGui::GetFrameHeight() + style.ItemSpacing.y * 6 + 8;
    ImGui::BeginChild("BackupBrowserContent", ImVec2(0, std::max(40.f, ImGui::GetContentRegionAvail().y - footer)), ImGuiChildFlags_NavFlattened);
    ImGui::TextWrapped("%s", launcherText(
        "Choose a managed backup, review its scope, then confirm. Every restore creates a safety backup of the current state first.",
        "Wähle eine verwaltete Sicherung, prüfe ihren Umfang und bestätige. Jede Wiederherstellung sichert zuerst den aktuellen Zustand."));
    ImGui::BeginChild("BackupList", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    for (const auto &entry : _backupEntries)
    {
        const auto key = backupKey(entry); ImGui::PushID(key.c_str());
        if (ImGui::Selectable(entry.campaignName.c_str(), _selectedBackupKey == key))
        { _selectedBackupKey = key; _confirmBackupRestore = false; _backupBrowserStatus.clear(); }
        ImGui::Indent();
        ImGui::TextWrapped("%s", (std::string(backupKindLabel(entry)) + " | " + backupTimeLabel(entry)).c_str());
        ImGui::Unindent(); ImGui::PopID();
    }
    ImGui::EndChild();
    selected = selectedBackup(_backupEntries, _selectedBackupKey);
    if (selected)
    {
        backupDetail(launcherText("Campaign", "Kampagne"), selected->campaignName);
        backupDetail(launcherText("Backup", "Sicherung"), backupKindLabel(*selected));
        backupDetail(launcherText("Snapshot time", "Sicherungszeit"), backupTimeLabel(*selected));
        backupDetail(launcherText("Database snapshot size (package files excluded)", "Größe der Datenbanksicherung (ohne Paketdateien)"), backupSizeLabel(selected->byteCount));
        backupScope(*selected);
        if (ImGui::TreeNode(launcherText("Technical details", "Technische Details")))
        {
            backupDetail(launcherText("Campaign ID", "Kampagnen-ID"), selected->campaignId);
            backupDetail(launcherText("Snapshot file", "Sicherungsdatei"), selected->snapshotPath.u8string());
            if (!selected->packageBackupPath.empty())
                backupDetail(launcherText("Package files", "Paketdateien"), selected->packageBackupPath.u8string());
            ImGui::TreePop();
        }
    }
    else ImGui::TextWrapped("%s", launcherText("Select a backup to see exactly what will change.", "Wähle eine Sicherung, um die genauen Änderungen zu sehen."));
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::BeginChild("BackupBrowserStatus", ImVec2(0, statusHeight), true);
    if (!_backupBrowserStatus.empty()) ImGui::TextWrapped("%s", _backupBrowserStatus.c_str());
    else ImGui::TextWrapped("%s", launcherText("Review and confirm the selected scope before restoring.",
                                               "Prüfe und bestätige vor der Wiederherstellung den ausgewählten Umfang."));
    ImGui::EndChild();
    ImGui::BeginDisabled(!selected);
    FormUi::Checkbox(launcherText("I have reviewed the scope", "Umfang geprüft"), &_confirmBackupRestore);
    ImGui::EndDisabled();
    // Own the reviewed entry across callbacks; the safety shortcut never executes a restore.
    if (drawUiButton(restoreLabel, selected && _confirmBackupRestore,
        launcherText("Select a backup and confirm its scope first.", "Wähle eine Sicherung und bestätige zuerst ihren Umfang."),
        UiButtonRole::Destructive, ImVec2(-1, 0)))
    {
        const BackupEntry reviewed = *selected;
        const auto result = _controller.restoreCampaignBackup(reviewed);
        _confirmBackupRestore = false;
        if (result.success)
        {
            _lastRestoreSafetyPath = result.safetyBackupPath / "repository-before-restore.db";
            _lastRestoreCampaignId = reviewed.campaignId;
            _backupBrowserStatus = std::string(result.campaignRemoved
                ? launcherText("Campaign removed: ", "Kampagne entfernt: ")
                : launcherText("Campaign restored: ", "Kampagne wiederhergestellt: ")) + result.restoredCampaignName + ". " +
                launcherText("Review the safety backup below to reverse this change.", "Prüfe unten die Sicherheitssicherung, um diese Änderung rückgängig zu machen.");
        }
        else _backupBrowserStatus = std::string(launcherText("Restore failed: ", "Wiederherstellung fehlgeschlagen: ")) + result.errorMessage;
        for (const auto &warning : result.warnings) _backupBrowserStatus += "\n" + warning;
        if (!result.safetyBackupPath.empty())
            _backupBrowserStatus += std::string("\n") + launcherText("Safety location: ", "Sicherungsort: ") + result.safetyBackupPath.u8string();
        for (const auto &path : result.retainedRecoveryPaths)
            _backupBrowserStatus += std::string("\n") + launcherText("Retained recovery location: ", "Erhaltener Wiederherstellungsort: ") + path.u8string();
        if (result.success) _statusMessage = _backupBrowserStatus;
    }
    if (drawUiButton(safetyLabel, !_lastRestoreSafetyPath.empty(),
        launcherText("Available after a successful restore.", "Nach einer erfolgreichen Wiederherstellung verfügbar."),
        UiButtonRole::Secondary, ImVec2(-1, 0)))
        refreshBackupBrowser(_lastRestoreSafetyPath, _lastRestoreCampaignId);
    const float half = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * .5f;
    if (drawUiButton(refreshLabel, UiButtonRole::Secondary, ImVec2(half, 0))) refreshBackupBrowser();
    ImGui::SameLine();
    if (drawUiButton(closeLabel, UiButtonRole::Secondary, ImVec2(-1, 0)))
    { _showBackupBrowserPopup = false; _confirmBackupRestore = false; ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}
