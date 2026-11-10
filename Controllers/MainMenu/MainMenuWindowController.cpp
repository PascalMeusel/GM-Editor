#include "MainMenuWindowController.hpp"
#include "Models/Project/CampaignRepository.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Rulesets/RulesetLibrary.hpp"
#include "Models/Rulesets/RulesetStudioLibrary.hpp"
#include "Views/MainMenu/MainMenuWindowView.hpp"

#include <algorithm>

MainMenuWindowController::MainMenuWindowController()
{
    view = std::make_unique<MainMenuWindowView>(*this);
    refreshRulesetLibrary();
}

MainMenuWindowController::~MainMenuWindowController() = default;

void MainMenuWindowController::update(std::shared_ptr<GLFWwindow> window)
{
    if(!view->isVisible)
        return;
    view->update(window);
};

void MainMenuWindowController::setWindowOpacity(bool showView)
{
    if (showView && view && !view->isVisible)
    {
        invalidateCampaignCache();
        refreshRulesetLibrary();
    }
    view->isVisible = showView;
}

WindowTypes MainMenuWindowController::getWindowType()
{
    return WindowTypes::MainMenuWindow;
}

bool MainMenuWindowController::getWindowActive()
{
    return view->isVisible;
}

const std::vector<Campaign> &MainMenuWindowController::loadCampaigns() const
{
    if (!campaignCacheLoaded)
    {
        campaignCache = CampaignRepository::loadCampaignSummaries();
        campaignCacheLoaded = true;
    }
    return campaignCache;
}

void MainMenuWindowController::invalidateCampaignCache() const
{
    campaignCache.clear();
    campaignCacheLoaded = false;
}

void MainMenuWindowController::refreshRulesetLibrary()
{
    if (!view)
        return;

    const std::vector<RulesetLibraryEntry> entries =
        buildRulesetLibrary(CampaignRepository::loadInstalledRulesetDefinitions());
    view->ruleSets.clear();
    view->ruleSets.reserve(entries.size());
    for (const RulesetLibraryEntry &entry : entries)
        view->ruleSets.push_back(entry.ruleSet);
    view->_selectedRuleIndex = std::clamp(view->_selectedRuleIndex,
                                         0,
                                         std::max(0, static_cast<int>(view->ruleSets.size()) - 1));
}

std::vector<RulesetStudioFamily> MainMenuWindowController::loadRulesetStudioLibrary() const
{
    return buildRulesetStudioLibrary(CampaignRepository::loadInstalledRulesetDefinitions(),
                                     loadCampaigns());
}

RulesetDefinitionModel MainMenuWindowController::createRulesetVersionDraft(
    const RulesetDefinitionModel &publishedSource)
{
    RulesetDefinitionModel draft = ::createNextRulesetVersionDraft(
        publishedSource, CampaignRepository::loadInstalledRulesetDefinitions());
    CampaignRepository::saveInstalledRulesetDefinition(draft);
    refreshRulesetLibrary();
    return draft;
}

bool MainMenuWindowController::campaignExists(const std::string &campaignName) const
{
    const std::vector<Campaign> &campaigns = loadCampaigns();
    return std::any_of(campaigns.begin(), campaigns.end(), [&](const Campaign &campaign) {
        return campaign.name == campaignName;
    });
}

const ProjectSaveConfig &MainMenuWindowController::projectSaveConfig() const
{
    return ProjectSaveService::getInstance().config();
}

bool MainMenuWindowController::applyProjectSaveSettings(bool autosaveEnabled,
                                                        int autosaveIntervalSeconds,
                                                        bool backupSnapshotsEnabled,
                                                        int maxBackupSnapshots,
                                                        bool recoverySnapshotsEnabled,
                                                        int maxRecoverySnapshots,
                                                        std::string &error)
{
    ProjectSaveService &saveService = ProjectSaveService::getInstance();
    const ProjectSaveConfig previous = saveService.config();
    saveService.setAutosaveEnabled(autosaveEnabled);
    saveService.setAutosaveIntervalSeconds(autosaveIntervalSeconds);
    saveService.setBackupSnapshotsEnabled(backupSnapshotsEnabled);
    saveService.setMaxBackupSnapshots(maxBackupSnapshots);
    saveService.setRecoverySnapshotsEnabled(recoverySnapshotsEnabled);
    saveService.setMaxRecoverySnapshots(maxRecoverySnapshots);
    if (saveService.persistConfig(&error))
        return true;

    // Keep the live session consistent with the file shown to the user when
    // persistence fails (read-only profile, full disk, invalid path, ...).
    saveService.setAutosaveEnabled(previous.autosaveEnabled);
    saveService.setAutosaveIntervalSeconds(previous.autosaveIntervalSeconds);
    saveService.setBackupSnapshotsEnabled(previous.backupSnapshotsEnabled);
    saveService.setMaxBackupSnapshots(previous.maxBackupSnapshots);
    saveService.setRecoverySnapshotsEnabled(previous.recoverySnapshotsEnabled);
    saveService.setMaxRecoverySnapshots(previous.maxRecoverySnapshots);
    return false;
}

void MainMenuWindowController::saveInstalledRulesetDefinition(const RulesetDefinitionModel &definition)
{
    CampaignRepository::saveInstalledRulesetDefinition(definition);
    refreshRulesetLibrary();
}

void MainMenuWindowController::removeInstalledRulesetDefinition(const std::string &stableId)
{
    CampaignRepository::removeInstalledRulesetDefinition(stableId);
    refreshRulesetLibrary();
}

bool MainMenuWindowController::handleNewGame(Campaign campaign, std::string &errorMessage)
{
    pinCampaignRuleset(campaign);
    const ProjectSaveResult result = ProjectSaveService::getInstance().saveCampaignDetailed(
        campaign, "create campaign");
    if (!result.committed())
    {
        errorMessage = result.error.empty()
                           ? "The campaign could not be saved."
                           : result.error;
        return false;
    }
    invalidateCampaignCache();
    try
    {
        handleLoadGame(campaign);
    }
    catch (const std::exception &error)
    {
        errorMessage = error.what();
        return false;
    }
    errorMessage.clear();
    return true;
}

void MainMenuWindowController::handleLoadGame(Campaign campaign)
{
    auto chosenCampaign = CampaignRepository::loadCampaign(campaign.name);
    view->isVisible = false;
}

void MainMenuWindowController::handleResumeLastSession(Campaign campaign)
{
    handleLoadGame(campaign);
}

ProjectPackageImport::ImportPreview MainMenuWindowController::previewCampaignPackage(const std::string &path) const
{
    return ProjectPackageImport::previewCampaignPackage(path);
}

ProjectPackageExport::ExportResult MainMenuWindowController::exportCampaignPackage(
    const std::string &campaignName,
    const std::string &destinationDirectory,
    bool overwriteExisting)
{
    ProjectPackageExport::ExportRequest request;
    request.destinationDirectory = std::filesystem::u8path(destinationDirectory);
    request.overwriteExisting = overwriteExisting;
    return ProjectPackageExport::exportCampaignPackage(CampaignRepository::loadCampaign(campaignName), request);
}

ProjectPackageImport::ImportResult MainMenuWindowController::importCampaignPackage(
    const std::string &path,
    ProjectPackageImport::ConflictResolution resolution,
    const std::string &cloneCampaignName)
{
    ProjectPackageImport::ImportRequest request;
    request.packagePath = path;
    request.conflictResolution = resolution;
    request.cloneCampaignName = cloneCampaignName;
    ProjectPackageImport::ImportResult result = ProjectPackageImport::importCampaignPackage(request);
    if (result.success)
        invalidateCampaignCache();
    return result;
}

std::vector<ProjectBackupRestore::BackupEntry> MainMenuWindowController::listCampaignBackups(std::string &errorMessage) const
{
    return ProjectBackupRestore::listBackups(errorMessage);
}

ProjectBackupRestore::RestoreResult MainMenuWindowController::restoreCampaignBackup(
    const ProjectBackupRestore::BackupEntry &backup)
{
    ProjectBackupRestore::RestoreResult result = ProjectBackupRestore::restoreBackup(backup);
    if (result.success)
        invalidateCampaignCache();
    return result;
}

void MainMenuWindowController::openImportedCampaign(const std::string &campaignName)
{
    CampaignRepository::loadCampaign(campaignName);
    view->isVisible = false;
}
