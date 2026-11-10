#pragma once

#include "../IWindowController.hpp"
#include "Models/Campaign/Campaign.hpp"
#include "Models/Project/ProjectSaveService.hpp"
#include "Models/Project/ProjectPackageImportService.hpp"
#include "Models/Project/ProjectPackageExportService.hpp"
#include "Models/Project/ProjectBackupRestoreService.hpp"
#include "Models/Rulesets/RulesetDefinitionModel.hpp"
#include "Models/Rulesets/RulesetStudioLibrary.hpp"

#include <string>
#include <vector>

class MainMenuWindowView;

class MainMenuWindowController : public IWindowController
{
public:
    MainMenuWindowController();
    ~MainMenuWindowController();

    virtual void update(std::shared_ptr<GLFWwindow> window) override;

    virtual void setWindowOpacity(bool showView) override;

    virtual WindowTypes getWindowType() override;
    virtual bool getWindowActive() override;
private:
    friend class MainMenuWindowView;
    friend struct BackupBrowserUiProbe;
    std::unique_ptr<MainMenuWindowView> view;
    mutable bool campaignCacheLoaded = false;
    mutable std::vector<Campaign> campaignCache;

    const std::vector<Campaign> &loadCampaigns() const;
    void invalidateCampaignCache() const;
    void refreshRulesetLibrary();
    std::vector<RulesetStudioFamily> loadRulesetStudioLibrary() const;
    RulesetDefinitionModel createRulesetVersionDraft(const RulesetDefinitionModel &publishedSource);
    bool campaignExists(const std::string &campaignName) const;
    const ProjectSaveConfig &projectSaveConfig() const;
    bool applyProjectSaveSettings(bool autosaveEnabled,
                                  int autosaveIntervalSeconds,
                                  bool backupSnapshotsEnabled,
                                  int maxBackupSnapshots,
                                  bool recoverySnapshotsEnabled,
                                  int maxRecoverySnapshots,
                                  std::string &error);
    void saveInstalledRulesetDefinition(const RulesetDefinitionModel &definition);
    void removeInstalledRulesetDefinition(const std::string &stableId);
    bool handleNewGame(Campaign campaign, std::string &errorMessage);
    void handleLoadGame(Campaign campaign);
    void handleResumeLastSession(Campaign campaign);
    ProjectPackageImport::ImportPreview previewCampaignPackage(const std::string &path) const;
    ProjectPackageExport::ExportResult exportCampaignPackage(const std::string &campaignName,
                                                             const std::string &destinationDirectory,
                                                             bool overwriteExisting);
    ProjectPackageImport::ImportResult importCampaignPackage(const std::string &path,
                                                              ProjectPackageImport::ConflictResolution resolution,
                                                              const std::string &cloneCampaignName = "");
    std::vector<ProjectBackupRestore::BackupEntry> listCampaignBackups(std::string &errorMessage) const;
    ProjectBackupRestore::RestoreResult restoreCampaignBackup(const ProjectBackupRestore::BackupEntry &backup);
    void openImportedCampaign(const std::string &campaignName);
};
