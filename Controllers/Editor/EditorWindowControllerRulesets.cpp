#include "Controllers/Editor/EditorWindowControllerSupport.hpp"
#include "Models/Rulesets/RulesetResolution.hpp"

std::vector<RulesetDefinitionModel> EditorWindowController::availableRulesetUpgrades() const
{
    std::vector<RulesetDefinitionModel> upgrades;
    const std::string &rulesetId = _currentCampaign.rulesetReference.rulesetId;
    const std::string &currentVersion = _currentCampaign.rulesetReference.version;
    if (rulesetId.empty())
        return upgrades;

    for (const RulesetDefinitionModel &definition : CampaignRepository::loadInstalledRulesetDefinitions())
    {
        const std::string definitionId = definition.rulesetId.empty() ? definition.metadata.id : definition.rulesetId;
        if (!definition.published || !definition.installedGlobally || definitionId != rulesetId || compareRulesetVersions(definition.version, currentVersion) <= 0)
            continue;
        upgrades.push_back(definition);
    }
    std::sort(upgrades.begin(), upgrades.end(), [](const RulesetDefinitionModel &left, const RulesetDefinitionModel &right) {
        return compareRulesetVersions(left.version, right.version) < 0;
    });
    return upgrades;
}

RulesetUpgradePreview EditorWindowController::previewRulesetUpgrade(const RulesetDefinitionModel &next) const
{
    std::vector<RulesetDefinitionModel> available = CampaignRepository::loadInstalledRulesetDefinitions();
    available.insert(available.end(), _currentCampaign.rulesetDefinitions.begin(), _currentCampaign.rulesetDefinitions.end());
    RulesetDefinitionModel current;
    bool foundCurrent = false;
    const RulesetDefinitionModel *overlay = nullptr;
    SpellcastingValues::Reason overlayReason = SpellcastingValues::Reason::None;
    if (!_currentCampaign.rulesetReference.overlayId.empty() || !_currentCampaign.rulesetReference.overlayVersion.empty())
    {
        overlay = findPinnedRulesetDefinition(available, _currentCampaign.rulesetReference.overlayId,
            _currentCampaign.rulesetReference.overlayVersion, true, overlayReason);
        if (!overlay)
        {
            RulesetUpgradePreview refused; refused.valid = false;
            refused.errors = {uiText("The exact pinned overlay is unavailable or ambiguous. Restore it before upgrading.",
                "Die genaue gebundene Erweiterung fehlt oder ist mehrdeutig. Stelle sie vor dem Aktualisieren wieder her.")};
            return refused;
        }
    }
    for (const RulesetDefinitionModel &definition : available)
    {
        const std::string definitionId = definition.rulesetId.empty() ? definition.metadata.id : definition.rulesetId;
        if (definitionId == _currentCampaign.rulesetReference.rulesetId && definition.version == _currentCampaign.rulesetReference.version)
        {
            current = definition;
            foundCurrent = true;
        }
    }
    if (!foundCurrent)
    {
        if (const auto *runtime = dynamic_cast<const DefinitionBackedRuleSet *>(_currentCampaign.ruleSet.get()))
            current = runtime->definition();
    }
    RulesetUpgradePreview preview = createRulesetUpgradePreview(current, next, overlay);
    for (const CharacterModel &character : _currentCampaign.characters)
    {
        int values = 0;
        for (const auto &[stableId, value] : character.ruleValues)
        {
            (void)value;
            if (std::find(preview.affectedStableIds.begin(), preview.affectedStableIds.end(), stableId) != preview.affectedStableIds.end())
                ++values;
        }
        if (values > 0)
        {
            ++preview.affectedCharacterCount;
            preview.affectedCharacterValueCount += values;
        }
    }
    return preview;
}

bool EditorWindowController::applyRulesetUpgrade(const RulesetDefinitionModel &next, std::vector<std::string> &errors)
{
    std::vector<RulesetDefinitionModel> available = CampaignRepository::loadInstalledRulesetDefinitions();
    available.insert(available.end(), _currentCampaign.rulesetDefinitions.begin(), _currentCampaign.rulesetDefinitions.end());
    const RulesetDefinitionModel *overlay = nullptr;
    if (!_currentCampaign.rulesetReference.overlayId.empty() || !_currentCampaign.rulesetReference.overlayVersion.empty())
    {
        SpellcastingValues::Reason reason;
        overlay = findPinnedRulesetDefinition(available, _currentCampaign.rulesetReference.overlayId,
            _currentCampaign.rulesetReference.overlayVersion, true, reason);
        if (!overlay)
        {
            errors = {uiText("The exact pinned overlay is unavailable or ambiguous. Restore it before upgrading.",
                "Die genaue gebundene Erweiterung fehlt oder ist mehrdeutig. Stelle sie vor dem Aktualisieren wieder her.")};
            return false;
        }
    }
    const Campaign rollbackSnapshot = _currentCampaign;
    if (!migrateCampaignToRuleset(_currentCampaign, next, overlay, errors))
        return false;
    if (!persistCurrentCampaign("ruleset upgrade"))
    {
        _currentCampaign = rollbackSnapshot;
        errors.push_back(uiText("The upgraded campaign could not be saved. The previous ruleset pin and character values were restored in memory.",
                                "Die aktualisierte Kampagne konnte nicht gespeichert werden. Die vorherige Regelwerkbindung und die Charakterwerte wurden im Arbeitsspeicher wiederhergestellt."));
        return false;
    }
    return true;
}

void EditorWindowController::showLocalPlayerDisplay()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    if (!_currentCampaign.localPlayerDisplay.enabled)
    {
        view->_presentationPanelOpen = true;
    }
}

void EditorWindowController::showPresentationPanel()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
    view->_presentationPanelOpen = true;
}

void EditorWindowController::showAudioManager()
{
openWorkspaceTool("show:audio_manager", EditorWorkspace::Compendium);
}

void EditorWindowController::showAudioQuickControls()
{
    if (!view->activateWorkspace(EditorWorkspace::Live)) return;
    view->_liveUtilityRailExplicitlyOpened = true;
}

void EditorWindowController::showShopManager()
{
    if (!view->activateWorkspace(EditorWorkspace::World)) return;
    hideAllWorkspaceTools();
    _workspace.navigation.current().editorRoute.clear();
    view->_worldCategoryFilter = 4;
    if (!_currentCampaign.shops.empty())
        view->_workspaceSelectionId = _currentCampaign.shops.front().metadata.id;
}

void EditorWindowController::showSpellLibrary()
{
openWorkspaceTool("show:spell_library", EditorWorkspace::Compendium);
}

void EditorWindowController::showStoryWorkspace()
{
openWorkspaceTool("show:story_workspace", EditorWorkspace::Story);
}

void EditorWindowController::showGlobalSearch()
{
    view->_universalSearchOpen = true;
    view->_focusUniversalSearchNextFrame = true;
}

void EditorWindowController::showKnowledgeGraph()
{
    view->_knowledgeGraphOpen = true;
}

void EditorWindowController::showCommandPalette()
{
    view->_universalSearchOpen = true;
    view->_focusUniversalSearchNextFrame = true;
}
