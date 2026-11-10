#include "MainMenuCampaignSupport.hpp"

void MainMenuWindowView::beginNewRulesetDefinition()
{
    resetRulesetDraft(_rulesetName,
                      _rulesetVersion,
                      _rulesetBaseId,
                      _rulesetAbilities,
                      _selectedRulesetAbilityIndex,
                      _rulesetSkills,
                      _selectedRulesetSkillIndex,
                      _rulesetDerivedStats,
                      _selectedRulesetDerivedStatIndex,
                      _rulesetDefenses,
                      _selectedRulesetDefenseIndex,
                      _rulesetResources,
                      _selectedRulesetResourceIndex,
                      _rulesetCurrencies,
                      _selectedRulesetCurrencyIndex,
                      _rulesetSizes,
                      _selectedRulesetSizeIndex,
                      _rulesetSpeeds,
                      _selectedRulesetSpeedIndex,
                      _rulesetConditions,
                      _selectedRulesetConditionIndex,
                      _rulesetClasses,
                      _selectedRulesetClassIndex,
                      _rulesetAncestries,
                      _selectedRulesetAncestryIndex,
                      _rulesetBackgrounds,
                      _selectedRulesetBackgroundIndex,
                      _rulesetFeatures,
                      _selectedRulesetFeatureIndex,
                      _rulesetEquipmentCategories,
                      _selectedRulesetEquipmentCategoryIndex,
                      _rulesetSpellTraditions,
                      _selectedRulesetSpellTraditionIndex,
                      _rulesetMonsterTemplates,
                      _selectedRulesetMonsterTemplateIndex,
                      _addRulesetError,
                      _selectedRulesetTemplateIndex,
                      _rulesetLabStep);
    _editingRulesetId.clear();
    _rulesetSourceDefinition = RulesetDefinitionModel{};
    _pendingRulesetTemplateIndex = -1;
    _pendingRulesetExitIntent = RulesetStudioExitIntent::None;
    _rulesetStudioEditing = true;
    _rulesetDraftBaselineFingerprint = rulesetDraftFingerprint(currentRulesetDraftDefinition(), _selectedRulesetTemplateIndex);
}

void MainMenuWindowView::beginEditingRulesetDefinition(const RulesetDefinitionModel &definition)
{
    _rulesetSourceDefinition = definition;
    _editingRulesetId = definition.rulesetId.empty() ? definition.metadata.id : definition.rulesetId;
    _rulesetName = definition.name;
    _rulesetVersion = definition.version;
    _rulesetBaseId = definition.baseRulesetId;
    _rulesetAbilities = definition.abilityScores;
    _rulesetSkills = definition.skills;
    _rulesetDerivedStats = definition.derivedStats;
    _rulesetDefenses = definition.defenses;
    _rulesetResources = definition.resources;
    _rulesetCurrencies = definition.currencies;
    _rulesetSizes = definition.sizes;
    _rulesetSpeeds = definition.speeds;
    _rulesetConditions = definition.conditions;
    _rulesetClasses = definition.classes;
    _rulesetAncestries = definition.ancestries;
    _rulesetBackgrounds = definition.backgrounds;
    _rulesetFeatures = definition.features;
    _rulesetEquipmentCategories = definition.equipmentCategories;
    _rulesetSpellTraditions = definition.spellTraditions;
    _rulesetMonsterTemplates = definition.monsterTemplates;
    _selectedRulesetAbilityIndex = _rulesetAbilities.empty() ? -1 : 0;
    _selectedRulesetSkillIndex = _rulesetSkills.empty() ? -1 : 0;
    _selectedRulesetDerivedStatIndex = _rulesetDerivedStats.empty() ? -1 : 0;
    _selectedRulesetDefenseIndex = _rulesetDefenses.empty() ? -1 : 0;
    _selectedRulesetResourceIndex = _rulesetResources.empty() ? -1 : 0;
    _selectedRulesetCurrencyIndex = _rulesetCurrencies.empty() ? -1 : 0;
    _selectedRulesetSizeIndex = _rulesetSizes.empty() ? -1 : 0;
    _selectedRulesetSpeedIndex = _rulesetSpeeds.empty() ? -1 : 0;
    _selectedRulesetConditionIndex = _rulesetConditions.empty() ? -1 : 0;
    _selectedRulesetClassIndex = _rulesetClasses.empty() ? -1 : 0;
    _selectedRulesetAncestryIndex = _rulesetAncestries.empty() ? -1 : 0;
    _selectedRulesetBackgroundIndex = _rulesetBackgrounds.empty() ? -1 : 0;
    _selectedRulesetFeatureIndex = _rulesetFeatures.empty() ? -1 : 0;
    _selectedRulesetEquipmentCategoryIndex = _rulesetEquipmentCategories.empty() ? -1 : 0;
    _selectedRulesetSpellTraditionIndex = _rulesetSpellTraditions.empty() ? -1 : 0;
    _selectedRulesetMonsterTemplateIndex = _rulesetMonsterTemplates.empty() ? -1 : 0;
    _selectedRulesetTemplateIndex = definition.baseRulesetId.empty() ? 1 : 0;
    _rulesetLabStep = RulesetLabStep::Basics;
    _addRulesetError.clear();
    _pendingRulesetTemplateIndex = -1;
    _pendingRulesetExitIntent = RulesetStudioExitIntent::None;
    _rulesetStudioEditing = true;
    _rulesetDraftBaselineFingerprint = rulesetDraftFingerprint(currentRulesetDraftDefinition(), _selectedRulesetTemplateIndex);
}

RulesetDefinitionModel MainMenuWindowView::currentRulesetDraftDefinition() const
{
    RulesetDefinitionModel definition = buildRulesetDraftDefinition(_rulesetName,
                                                                     _rulesetVersion,
                                                                     _rulesetBaseId,
                                                                     _rulesetAbilities,
                                                                     _rulesetSkills,
                                                                     _rulesetDerivedStats,
                                                                     _rulesetDefenses,
                                                                     _rulesetResources,
                                                                     _rulesetCurrencies,
                                                                     _rulesetSizes,
                                                                     _rulesetSpeeds,
                                                                     _rulesetConditions,
                                                                     _rulesetClasses,
                                                                     _rulesetAncestries,
                                                                     _rulesetBackgrounds,
                                                                     _rulesetFeatures,
                                                                     _rulesetEquipmentCategories,
                                                                     _rulesetSpellTraditions,
                                                                     _rulesetMonsterTemplates,
                                                                     _editingRulesetId);
    if (!_editingRulesetId.empty())
    {
        // The current wizard does not expose every executable v1 field yet.
        // Preserve those source values while editing the supported sections.
        definition.schemaVersion = _rulesetSourceDefinition.schemaVersion;
        definition.abilityModifierFormula = _rulesetSourceDefinition.abilityModifierFormula;
        definition.proficiencyFormula = _rulesetSourceDefinition.proficiencyFormula;
        definition.checkDiceFormula = _rulesetSourceDefinition.checkDiceFormula;
        definition.passiveScoreBase = _rulesetSourceDefinition.passiveScoreBase;
        definition.diceFormulas = _rulesetSourceDefinition.diceFormulas;
        definition.subclasses = _rulesetSourceDefinition.subclasses;
    }
    return definition;
}


MainMenuWindowView::RulesetStudioReviewState MainMenuWindowView::currentRulesetStudioReviewState() const
{
    RulesetStudioReviewState review;
    review.definition = currentRulesetDraftDefinition();
    review.diagnostics = validateRulesetDefinition(review.definition);
    review.nameMissing = review.definition.name.empty();
    review.versionMissing = review.definition.version.empty();
    review.canSaveDraft = !review.nameMissing;
    review.canPublish = review.canSaveDraft && !review.versionMissing && review.diagnostics.empty();
    review.draftDirty = rulesetDraftFingerprint(review.definition, _selectedRulesetTemplateIndex) !=
                        _rulesetDraftBaselineFingerprint;
    return review;
}
