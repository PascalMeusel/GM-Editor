#include "backup_browser_ui_fixture.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "SQLiteCpp/SQLiteCpp.h"

using namespace backup_ui;
namespace
{
Campaign fixture(const std::string &prefix)
{
    Campaign value; value.name = prefix + " " + value.metadata.id;
    value.ruleSet = std::make_shared<DND5e>(); value.description = "BEFORE_SAVE_PRIVATE_CANARY";
    return value;
}
fs::path snapshot(const Campaign &campaign)
{
    const auto folder = ProjectPackageLayout::findProjectRoot() / "Resources/Backups" /
        ProjectAssets::sanitizePathSegment(campaign.metadata.id);
    fs::create_directories(folder); const auto path = folder / "2026-10-04T21_00_00Z_browser.db";
    CampaignRepository::writeCampaignSnapshot(campaign, path.u8string()); return path;
}
ProjectBackupRestore::BackupEntry entry(MainMenuWindowController &controller, const fs::path &path, const EntityId &id)
{
    for (const auto &value : BackupBrowserUiProbe::entries(controller))
        if (key(value.snapshotPath, value.campaignId) == key(path, id)) return value;
    throw std::runtime_error("Expected managed backup absent from actual browser: " + path.string());
}
void unchanged(const Campaign &campaign, const std::string &expected, const Campaign &other)
{
    require(read(campaign.metadata.id).description == expected, "Backup browser changed the wrong campaign state");
    const auto actual = read(other.metadata.id);
    require(actual.name == other.name && actual.description == other.description, "Backup browser changed unrelated campaign identity/content");
}
void confirm(Ui &ui, bool de) { ui.click(label(de, "I have reviewed the scope", "Umfang geprüft")); }
void restore(Ui &ui, bool de) { ui.click(label(de, "Restore selected backup", "Ausgewählte Sicherung wiederherstellen")); }
void refresh(Ui &ui, bool de) { ui.click(label(de, "Refresh", "Aktualisieren")); }
void safety(Ui &ui, bool de) { ui.click(label(de, "Review safety backup", "Sicherheitssicherung prüfen")); }

void restoreCase(bool de, bool compact)
{
    std::cout << "Backup browser restore: " << (de ? "DE" : "EN") << (compact ? " compact150" : " normal") << '\n';
    Ui ui(de, compact); auto campaign = fixture("Browser restore"), other = fixture("Unrelated browser canary");
    other.description = "UNRELATED_MUST_SURVIVE"; CampaignRepository::saveCampaign(other);
    const auto path = snapshot(campaign);
    const auto original = bytes(path);
    campaign.description = "CURRENT_BEFORE_RESTORE"; CampaignRepository::saveCampaign(campaign);
    MainMenuWindowController controller; BackupBrowserUiProbe::open(controller);
    ui.draw = [&] { BackupBrowserUiProbe::draw(controller); }; ui.frame(4); ui.footer(de);
    require(BackupBrowserUiProbe::selected(controller).empty(), "Opening browser silently selected a campaign");
    const auto initialEntry = entry(controller, path, campaign.metadata.id);
    ui.select(initialEntry); ui.footer(de);
    require(initialEntry.timestampUtc.size() == 20 && initialEntry.timestampUtc[10] == 'T' && initialEntry.timestampUtc.back() == 'Z',
        "Discovery omitted an explicit UTC snapshot timestamp");
    const auto displayedTime = initialEntry.timestampUtc.substr(0, 10) + " " + initialEntry.timestampUtc.substr(11, 8) + " UTC";
    require(ui.submitted.find(label(de, "Before saving", "Vor dem Speichern")) != std::string::npos &&
        ui.submitted.find(displayedTime) != std::string::npos &&
        ui.submitted.find(label(de, "Database snapshot size (package files excluded)", "Größe der Datenbanksicherung (ohne Paketdateien)")) != std::string::npos &&
        ui.submitted.find(label(de, "Existing managed package files and assets remain unchanged.", "Vorhandene verwaltete Paketdateien und Assets bleiben unverändert.")) != std::string::npos,
        "Pre-save browser omitted accurate kind/time/database-only-size/package scope");
    confirm(ui, de); require(BackupBrowserUiProbe::confirmed(controller), "Actual review checkbox did not confirm");
    const auto selected = BackupBrowserUiProbe::selected(controller);
    refresh(ui, de);
    require(BackupBrowserUiProbe::selected(controller) == selected && !BackupBrowserUiProbe::confirmed(controller),
        "Refresh rebound selection or retained authorization");
    confirm(ui, de);
    {
        SQLite::Database changed(path.u8string(), SQLite::OPEN_READWRITE);
        changed.exec("UPDATE campaigns SET description='CHANGED_AFTER_REVIEW'");
    }
    restore(ui, de); ui.footer(de);
    require(BackupBrowserUiProbe::openNow(controller) && !BackupBrowserUiProbe::confirmed(controller) &&
        BackupBrowserUiProbe::status(controller).find(label(de, "Restore failed:", "Wiederherstellung fehlgeschlagen:")) == 0 &&
        ui.submitted.find(BackupBrowserUiProbe::status(controller)) != std::string::npos,
        "Changed snapshot did not fail visibly inside the open browser or retained confirmation");
    unchanged(campaign, campaign.description, other);
    auto *feedback = ui.window("/BackupBrowserStatus_");
    require(feedback->ContentSize.y > 0 && feedback->InnerClipRect.GetHeight() > feedback->FontRefSize,
        "Failure had no visible scrollable feedback area");
    write(path, original); refresh(ui, de); confirm(ui, de); restore(ui, de);
    unchanged(campaign, "BEFORE_SAVE_PRIVATE_CANARY", other);
    const auto safetyPath = BackupBrowserUiProbe::safety(controller);
    require(!safetyPath.empty() && fs::is_regular_file(safetyPath) && !BackupBrowserUiProbe::confirmed(controller),
        "Successful restore did not expose its exact safety snapshot or clear consent");
    ui.footer(de); safety(ui, de);
    require(BackupBrowserUiProbe::selected(controller) == key(safetyPath, campaign.metadata.id) &&
        !BackupBrowserUiProbe::confirmed(controller), "Safety action selected another snapshot or authorized another restore");
    unchanged(campaign, "BEFORE_SAVE_PRIVATE_CANARY", other);
    require(entry(controller, safetyPath, campaign.metadata.id).kind == ProjectBackupRestore::BackupKind::BeforeRestore &&
        ui.submitted.find(label(de, "Before restore", "Vor der Wiederherstellung")) != std::string::npos,
        "Created safety entry is not accurately presented as Before restore");
    confirm(ui, de); restore(ui, de); unchanged(campaign, campaign.description, other);

    // The success affordance only selects; a deleted selection must not silently jump
    // to the many other valid entries now present in the managed browser.
    safety(ui, de); const auto selectedSafety = BackupBrowserUiProbe::safety(controller);
    auto held = selectedSafety; held += ".held"; fs::rename(selectedSafety, held);
    refresh(ui, de); ui.footer(de);
    require(BackupBrowserUiProbe::selected(controller).empty() && !BackupBrowserUiProbe::confirmed(controller) &&
        BackupBrowserUiProbe::status(controller).find(label(de, "The selected backup is no longer available.", "Die ausgewählte Sicherung ist nicht mehr verfügbar.")) != std::string::npos,
        "Missing selected backup silently rebound to another row or omitted review guidance");
    fs::rename(held, selectedSafety); refresh(ui, de);
    require(BackupBrowserUiProbe::selected(controller).empty(), "Returning snapshot silently restored old consent/selection");
    const auto importFolder = ProjectPackageLayout::campaignsDirectory() / "ImportBackups" /
        (ProjectAssets::sanitizePathSegment(campaign.metadata.id) + "-browser-import");
    fs::create_directories(importFolder);
    const auto importPath = importFolder / "repository-before-import.db";
    CampaignRepository::writeCampaignSnapshot(campaign, importPath.u8string()); refresh(ui, de);
    const auto imported = entry(controller, importPath, campaign.metadata.id); ui.select(imported);
    require(imported.kind == ProjectBackupRestore::BackupKind::ImportReplacement && imported.packageBackupPath.empty() &&
        ui.submitted.find(label(de, "Before import", "Vor dem Import")) != std::string::npos &&
        ui.submitted.find(label(de, "Existing managed package files and assets remain unchanged.",
            "Vorhandene verwaltete Paketdateien und Assets bleiben unverändert.")) != std::string::npos,
        "Data-only import backup claims package replacement or omits its correct kind");
    ui.footer(de); unchanged(campaign, campaign.description, other);
    ui.keyboard(label(de, "Close", "Schließen"));
    require(!BackupBrowserUiProbe::openNow(controller), "Keyboard Close did not dismiss real backup browser");
    unchanged(campaign, campaign.description, other);
}

void absenceCase(bool de, bool compact)
{
    std::cout << "Backup browser absent-target reversal: " << (de ? "DE" : "EN") << (compact ? " compact150" : " normal") << '\n';
    Ui ui(de, compact); auto introduced = fixture("Introduced by restore"), other = fixture("Absence unrelated");
    other.description = "ABSENCE_UNRELATED"; CampaignRepository::saveCampaign(other);
    const auto path = snapshot(introduced);
    require(!fs::exists(ProjectPackageLayout::campaignPackageDirectory(introduced)), "Absent target fixture unexpectedly has a package");
    MainMenuWindowController controller; BackupBrowserUiProbe::open(controller);
    ui.draw = [&] { BackupBrowserUiProbe::draw(controller); }; ui.frame(4);
    ui.select(entry(controller, path, introduced.metadata.id)); confirm(ui, de); restore(ui, de);
    unchanged(introduced, introduced.description, other);
    safety(ui, de); const auto receiptEntry = entry(controller, BackupBrowserUiProbe::safety(controller), introduced.metadata.id);
    require(!receiptEntry.campaignPresent && receiptEntry.packageAction == ProjectBackupRestore::PackageRestoreAction::Keep,
        "Data-only safety reversal did not offer campaign removal while retaining package scope");
    ui.footer(de, true);
    require(ui.submitted.find(label(de, "This campaign did not exist before the earlier restore.",
            "Diese Kampagne existierte vor der früheren Wiederherstellung nicht.")) != std::string::npos &&
        ui.submitted.find(label(de, "Existing managed package files and assets remain unchanged.",
            "Vorhandene verwaltete Paketdateien und Assets bleiben unverändert.")) != std::string::npos,
        "Removal confirmation omitted the exact campaign and managed-package scope");
    require(!BackupBrowserUiProbe::confirmed(controller), "Reviewing absent-state receipt implicitly consented to removal");
    unchanged(introduced, introduced.description, other);
    confirm(ui, de); ui.click(label(de, "Remove selected campaign", "Ausgewählte Kampagne entfernen"));
    const auto campaigns = CampaignRepository::loadAllCampaigns();
    require(std::none_of(campaigns.begin(), campaigns.end(), [&](const auto &value) { return value.metadata.id == introduced.metadata.id; }) &&
        !fs::exists(ProjectPackageLayout::campaignPackageDirectory(introduced)), "Explicit removal failed to restore recorded absence");
    require(read(other.metadata.id).description == other.description, "Absence reversal removed or modified unrelated campaign");
    const auto removedSafety = BackupBrowserUiProbe::safety(controller); safety(ui, de);
    require(BackupBrowserUiProbe::selected(controller) == key(removedSafety, introduced.metadata.id) &&
        entry(controller, removedSafety, introduced.metadata.id).campaignPresent && !BackupBrowserUiProbe::confirmed(controller),
        "Removal did not expose its own reversible safety snapshot without consent");
    ui.footer(de); confirm(ui, de); restore(ui, de); unchanged(introduced, introduced.description, other);
    ui.click(label(de, "Close", "Schließen"));
}
}
int main()
{
    try
    {
        const auto root = fs::temp_directory_path() / ("gm-backup-browser-ui-" + createEntityId("run")); isolate(root);
        auto &save = ProjectSaveService::getInstance(); save.setBackupSnapshotsEnabled(false); save.setRecoverySnapshotsEnabled(false);
        for (bool de : {false, true}) for (bool compact : {false, true}) { restoreCase(de, compact); absenceCase(de, compact); }
        std::cout << "8 backup browser UI groups passed: observed EN/DE normal/150% footer, confirmation, stale refusal/retry, stable refresh, exact safety review/reversal, receipt-backed removal and keyboard close.\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
