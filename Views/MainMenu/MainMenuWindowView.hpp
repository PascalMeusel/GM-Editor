#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <memory>
#include <iostream>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <GLFW/glfw3.h>

#include "../IWindowView.hpp"
#include "../../Models/Campaign/Campaign.hpp"
#include "../../Models/Rulesets/DND/DND5e.hpp"
#include "../../Models/Rulesets/RulesetDefinitionModel.hpp"
#include "../../Models/Rulesets/RulesetDefinitionValidator.hpp"
#include "../../Models/Project/ProjectPackageImportService.hpp"
#include "../../Models/Project/ProjectBackupRestoreService.hpp"

class MainMenuWindowController;

enum class MainMenuOption
{
    None,
    NewGame,
    LoadGame,
    AddRuleSet,
    Settings,
    Exit
};

enum class NewCampaignWizardStep
{
    Name = 0,
    RulesetTemplate,
    ContentDescription,
    Review
};

enum class RulesetLabStep
{
    Basics = 0,
    Template,
    StarterTerms,
    DerivedStats,
    AdvancedIds,
    Review
};

enum class RulesetStudioExitIntent
{
    None,
    ShowLibrary,
    CloseStudio
};

class MainMenuWindowView : public IWindowView
{
public:
    MainMenuWindowView() = delete;
    MainMenuWindowView(MainMenuWindowController &controller) : _controller(controller)
    {
        ruleSets.push_back(std::make_unique<DND5e>()); // Add a default ruleset, e.g., DND5e
    }
    ~MainMenuWindowView() = default;

    // Function to update the view
    void update(std::shared_ptr<GLFWwindow>) override;

protected:
    struct RulesetStudioReviewState
    {
        RulesetDefinitionModel definition;
        std::vector<RulesetValidationDiagnostic> diagnostics;
        bool nameMissing = false;
        bool versionMissing = false;
        bool canSaveDraft = false;
        bool canPublish = false;
        bool draftDirty = false;
    };

    MainMenuWindowController &_controller;
    std::vector<std::shared_ptr<IRuleSet>> ruleSets;

    friend class MainMenuWindowController;
    friend struct BackupBrowserUiProbe;

    // New game dialogWindow
    bool _showNewGamePopup = false;
    bool _requestOpenNewGamePopup = false;
    std::string _campaignName = std::string();
    std::string _campaignDescription = std::string();
    std::string _newGameError = std::string();
    int _selectedRuleIndex = 0;
    int _selectedCampaignTemplateIndex = 1;
    NewCampaignWizardStep _newCampaignWizardStep = NewCampaignWizardStep::Name;
    void handleNewGamePopup();

    // Campaign list selection
    int _selectedLoadGameIndex = 0;
    std::string _campaignFilter;
    bool _liveCampaignsOnly = false;
    std::string _statusMessage;

    // Campaign package import
    bool _showImportPackagePopup = false;
    bool _requestOpenImportPackagePopup = false;
    bool _hasImportPreview = false;
    bool _confirmImportReplacement = false;
    int _importConflictChoice = 0;
    std::string _importCloneName;
    bool _importPackageCompleted = false;
    std::string _importPackagePath;
    std::string _importPackageError;
    std::string _importedCampaignName;
    ProjectPackageImport::ImportPreview _importPreview;
    void openImportPackagePopup();
    void handleImportPackagePopup();

    // Campaign package export
    bool _showExportPackagePopup = false;
    bool _requestOpenExportPackagePopup = false;
    bool _confirmExportOverwrite = false;
    std::string _exportCampaignName;
    std::string _exportPackageFolder;
    std::string _exportDirectory;
    std::string _exportPackageStatus;
    void openExportPackagePopup(const Campaign &campaign);
    void handleExportPackagePopup();

    // Managed backup browser
    bool _showBackupBrowserPopup = false;
    bool _requestOpenBackupBrowserPopup = false;
    bool _confirmBackupRestore = false;
    std::string _selectedBackupKey;
    std::filesystem::path _lastRestoreSafetyPath;
    std::string _lastRestoreCampaignId;
    std::string _backupBrowserStatus;
    std::vector<ProjectBackupRestore::BackupEntry> _backupEntries;
    void openBackupBrowserPopup();
    void handleBackupBrowserPopup();
    void refreshBackupBrowser(const std::filesystem::path &selectPath = {}, const std::string &campaignId = {});

    // Settings dialogWindow
    bool _showSettingsPopup = false;
    bool _requestOpenSettingsPopup = false;
    bool _settingsAutosaveEnabled = true;
    bool _settingsBackupSnapshotsEnabled = true;
    bool _settingsRecoverySnapshotsEnabled = true;
    int _settingsAutosaveIntervalSeconds = 120;
    int _settingsMaxBackupSnapshots = 10;
    int _settingsMaxRecoverySnapshots = 5;
    std::string _settingsStatus = std::string();
    void openSettingsPopup();
    void handleSettingsPopup();

    // Ruleset editor dialogWindow
    bool _showAddRulesetPopup = false;
    bool _requestOpenAddRulesetPopup = false;
    bool _rulesetStudioEditing = false;
    int _selectedRulesetFamilyIndex = 0;
    int _selectedPublishedRulesetIndex = 0;
    std::string _editingRulesetId;
    RulesetDefinitionModel _rulesetSourceDefinition;
    std::string _rulesetName = std::string();
    std::string _rulesetVersion = "1.0.0";
    std::string _rulesetBaseId = "dnd-2024-5.5e";
    std::vector<RulesetAbilityScoreDefinition> _rulesetAbilities;
    std::vector<RulesetSkillDefinition> _rulesetSkills;
    std::vector<RulesetDerivedStatDefinition> _rulesetDerivedStats;
    std::vector<RulesetDefenseDefinition> _rulesetDefenses;
    std::vector<RulesetResourceDefinition> _rulesetResources;
    std::vector<RulesetCurrencyDefinition> _rulesetCurrencies;
    std::vector<RulesetSizeDefinition> _rulesetSizes;
    std::vector<RulesetSpeedDefinition> _rulesetSpeeds;
    std::vector<RulesetConditionDefinition> _rulesetConditions;
    std::vector<RulesetClassDefinition> _rulesetClasses;
    std::vector<RulesetAncestryDefinition> _rulesetAncestries;
    std::vector<RulesetBackgroundDefinition> _rulesetBackgrounds;
    std::vector<RulesetFeatureDefinition> _rulesetFeatures;
    std::vector<RulesetEquipmentCategoryDefinition> _rulesetEquipmentCategories;
    std::vector<RulesetSpellTraditionDefinition> _rulesetSpellTraditions;
    std::vector<RulesetMonsterTemplateDefinition> _rulesetMonsterTemplates;
    std::string _addRulesetError = std::string();
    int _selectedRulesetTemplateIndex = 0;
    int _selectedRulesetAbilityIndex = -1;
    int _selectedRulesetSkillIndex = -1;
    int _selectedRulesetDerivedStatIndex = -1;
    int _selectedRulesetDefenseIndex = -1;
    int _selectedRulesetResourceIndex = -1;
    int _selectedRulesetCurrencyIndex = -1;
    int _selectedRulesetSizeIndex = -1;
    int _selectedRulesetSpeedIndex = -1;
    int _selectedRulesetConditionIndex = -1;
    int _selectedRulesetClassIndex = -1;
    int _selectedRulesetAncestryIndex = -1;
    int _selectedRulesetBackgroundIndex = -1;
    int _selectedRulesetFeatureIndex = -1;
    int _selectedRulesetEquipmentCategoryIndex = -1;
    int _selectedRulesetSpellTraditionIndex = -1;
    int _selectedRulesetMonsterTemplateIndex = -1;
    RulesetLabStep _rulesetLabStep = RulesetLabStep::Basics;
    std::string _rulesetDraftBaselineFingerprint;
    int _pendingRulesetTemplateIndex = -1;
    RulesetStudioExitIntent _pendingRulesetExitIntent = RulesetStudioExitIntent::None;
    void beginEditingRulesetDefinition(const RulesetDefinitionModel &definition);
    void beginNewRulesetDefinition();
    RulesetDefinitionModel currentRulesetDraftDefinition() const;
    RulesetStudioReviewState currentRulesetStudioReviewState() const;
    void drawRulesetStudioLibrary();
    void drawRulesetStudioStep(const RulesetStudioReviewState &review);
    bool drawRulesetStudioActions(const RulesetStudioReviewState &review);
    void handleAddRulesetPopup();

    void openEditor(Campaign campaign);

    MainMenuOption _currentChoice = MainMenuOption::None;
};
