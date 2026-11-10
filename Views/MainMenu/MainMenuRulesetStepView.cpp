#include "MainMenuCampaignSupport.hpp"

void MainMenuWindowView::drawRulesetStudioStep(const RulesetStudioReviewState &review)
{
    const RulesetDefinitionModel &reviewDefinition = review.definition;
    const std::vector<RulesetValidationDiagnostic> &reviewDiagnostics = review.diagnostics;
    const bool nameMissing = review.nameMissing;
    const bool versionMissing = review.versionMissing;
    const bool canSaveDraft = review.canSaveDraft;
    const bool canPublish = review.canPublish;
        switch (_rulesetLabStep)
        {
        case RulesetLabStep::Basics:
            ImGui::SeparatorText(localizedUiLabel("Basics").c_str());
            drawRulesetTextField("##RulesetName", "Ruleset name", _rulesetName, "Example: Lantern Fair Homebrew", 520.0f);
            drawRulesetTextField("##RulesetVersion", "Version", _rulesetVersion, "", 180.0f);
            drawMutedHelpText("Drafts stay editable; published versions are immutable.",
                              "Campaigns pin an exact published version. Optional overlays replace supported definitions without changing the base package.");
            break;

        case RulesetLabStep::Template:
        {
            const auto requestTemplateSwitch = [&](int templateIndex) {
                if (_selectedRulesetTemplateIndex == templateIndex)
                    return;
                _pendingRulesetTemplateIndex = templateIndex;
                ImGui::OpenPopup(rulesetStudioText("Replace draft with template?", "Entwurf durch Vorlage ersetzen?"));
            };
            ImGui::SeparatorText(localizedUiLabel("Template Intent").c_str());
            if (FormUi::RadioButton(localizedUiLabel("D&D-like baseline").c_str(), _selectedRulesetTemplateIndex == 0))
                requestTemplateSwitch(0);
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetTemplateIntentSummary(0));

            if (FormUi::RadioButton(localizedUiLabel("Blank ruleset draft").c_str(), _selectedRulesetTemplateIndex == 1))
                requestTemplateSwitch(1);
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetTemplateIntentSummary(1));

            break;
        }

        case RulesetLabStep::StarterTerms:
            drawRulesetCoreListsEditor(_rulesetAbilities,
                                       _selectedRulesetAbilityIndex,
                                       _rulesetSkills,
                                       _selectedRulesetSkillIndex,
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
                                       _selectedRulesetSpellTraditionIndex);
            break;

        case RulesetLabStep::DerivedStats:
            drawRulesetFormulaListsEditor(_rulesetDerivedStats,
                                          _selectedRulesetDerivedStatIndex,
                                          _rulesetDefenses,
                                          _selectedRulesetDefenseIndex,
                                          _rulesetResources,
                                          _selectedRulesetResourceIndex,
                                          _rulesetMonsterTemplates,
                                          _selectedRulesetMonsterTemplateIndex,
                                          reviewDefinition);
            break;

        case RulesetLabStep::AdvancedIds:
            ImGui::SeparatorText(localizedUiLabel("Stable Identifiers").c_str());
            ImGui::TextWrapped(localizedUiLabel("IDs are for import/export, validation, and future mapping. Most GMs can keep defaults or leave blank drafts blank.").c_str());
            drawRulesetTextField("##BaseRulesetId", "Base ruleset ID", _rulesetBaseId, "Example: dnd-2024-5.5e", 360.0f);
            drawMutedHelpText("Definition IDs live with each list row.",
                              "Use Core Lists and Formula Lists to edit stable IDs for abilities, skills, defenses, resources, and other definitions.");
            break;

        case RulesetLabStep::Review:
        default:
            ImGui::SeparatorText(localizedUiLabel("Review").c_str());
            drawInlineLabelValue("Ruleset", reviewDefinition.name.empty()
                                                ? rulesetStudioText("Missing", "Fehlt")
                                                : reviewDefinition.name.c_str());
            drawInlineLabelValue("Version", reviewDefinition.version.empty()
                                                ? rulesetStudioText("Unspecified", "Nicht festgelegt")
                                                : reviewDefinition.version.c_str());
            drawInlineLabelValue("Template intent", rulesetTemplateIntentName(_selectedRulesetTemplateIndex));
            drawInlineLabelValue("Base ruleset ID", reviewDefinition.baseRulesetId.empty()
                                                       ? rulesetStudioText("None", "Keine")
                                                       : reviewDefinition.baseRulesetId.c_str());
            drawInlineLabelValue("Core lists", (countSummary(reviewDefinition.abilityScores.size(), rulesetStudioText("abilities", "Attribute")) + ", " +
                                                countSummary(reviewDefinition.skills.size(), rulesetStudioText("skills", "Fertigkeiten")) + ", " +
                                                countSummary(reviewDefinition.currencies.size(), rulesetStudioText("currencies", "Währungen")) + ", " +
                                                countSummary(reviewDefinition.conditions.size(), rulesetStudioText("conditions", "Zustände"))).c_str());
            drawInlineLabelValue("Actor content", (countSummary(reviewDefinition.classes.size(), rulesetStudioText("classes", "Klassen")) + ", " +
                                                   countSummary(reviewDefinition.ancestries.size(), rulesetStudioText("ancestries", "Abstammungen")) + ", " +
                                                   countSummary(reviewDefinition.backgrounds.size(), rulesetStudioText("backgrounds", "Hintergründe")) + ", " +
                                                   countSummary(reviewDefinition.features.size(), rulesetStudioText("features", "Merkmale"))).c_str());
            drawInlineLabelValue("Formula lists", (countSummary(reviewDefinition.derivedStats.size(), rulesetStudioText("derived stats", "abgeleitete Werte")) + ", " +
                                                   countSummary(reviewDefinition.defenses.size(), rulesetStudioText("defenses", "Verteidigungen")) + ", " +
                                                   countSummary(reviewDefinition.resources.size(), rulesetStudioText("resources", "Ressourcen")) + ", " +
                                                   countSummary(reviewDefinition.monsterTemplates.size(), rulesetStudioText("monster templates", "Monstervorlagen"))).c_str());

            ImGui::SeparatorText(localizedUiLabel("Validation").c_str());
            ImGui::TextWrapped(localizedUiLabel("Validation checks stable IDs, references, and formulas. Publishing makes this version available to definition-backed sheets and rules evaluation.").c_str());
            if (canPublish)
            {
                ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("%s").c_str(),
                                   rulesetStudioText("Draft passes validation and is ready to publish.",
                                                     "Der Entwurf ist validiert und kann veröffentlicht werden."));
            }
            else
            {
                if (canSaveDraft)
                {
                    ImGui::TextColored(ImVec4(0.96f, 0.66f, 0.30f, 1.0f), localizedUiLabel("%s").c_str(),
                                       rulesetStudioText(
                                           "You can save this incomplete draft. Resolve the items below before publishing.",
                                           "Du kannst diesen unvollständigen Entwurf speichern. Behebe die folgenden Punkte vor dem Veröffentlichen."));
                }
                if (nameMissing)
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("Ruleset name is required.").c_str());
                    if (drawSmallUiButton("Go to Basics", UiButtonRole::Primary, "Jump back to the ruleset name field."))
                        _rulesetLabStep = RulesetLabStep::Basics;
                }
                if (versionMissing)
                {
                    ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("%s").c_str(),
                                       rulesetStudioText("A version is required for publishing.",
                                                         "Zum Veröffentlichen ist eine Version erforderlich."));
                }
                if (!reviewDiagnostics.empty())
                {
                    drawRulesetValidationDiagnosticsPanel(reviewDiagnostics,
                                                          _rulesetLabStep,
                                                          _selectedRulesetAbilityIndex,
                                                          _rulesetAbilities,
                                                          _selectedRulesetSkillIndex,
                                                          _rulesetSkills,
                                                          _selectedRulesetDerivedStatIndex,
                                                          _rulesetDerivedStats,
                                                          _selectedRulesetDefenseIndex,
                                                          _rulesetDefenses,
                                                          _selectedRulesetResourceIndex,
                                                          _rulesetResources,
                                                          _selectedRulesetMonsterTemplateIndex,
                                                          _rulesetMonsterTemplates);
                }
            }
            break;
        }
}
