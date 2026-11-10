#include "MainMenuCampaignSupport.hpp"

bool MainMenuWindowView::drawRulesetStudioActions(const RulesetStudioReviewState &review)
{
    const RulesetDefinitionModel &reviewDefinition = review.definition;
    const bool rulesetDraftDirty = review.draftDirty;
    const bool canSaveDraft = review.canSaveDraft;
    const bool canPublish = review.canPublish;
    const bool nameMissing = review.nameMissing;
    const bool versionMissing = review.versionMissing;
        const bool firstStep = _rulesetLabStep == RulesetLabStep::Basics;
        if (drawUiButton("Back",
                         !firstStep,
                         "This is the first step.",
                         UiButtonRole::Secondary,
                         ImVec2(110, 0),
                         "Return to the previous lab step."))
        {
            _addRulesetError.clear();
            _rulesetLabStep = rulesetLabStepFromIndex(rulesetLabStepIndex(_rulesetLabStep) - 1);
        }

        continueRulesetActionRow(rulesetStudioText("Library", "Bibliothek"), 110.0f);
        if (drawUiButton(rulesetStudioText("Library", "Bibliothek"), UiButtonRole::Secondary, ImVec2(110, 0)))
        {
            if (rulesetDraftDirty)
            {
                _pendingRulesetExitIntent = RulesetStudioExitIntent::ShowLibrary;
                ImGui::OpenPopup(rulesetStudioText("Unsaved ruleset changes", "Ungespeicherte Regelwerksänderungen"));
            }
            else
            {
                _rulesetStudioEditing = false;
            }
        }

        bool closeRulesetStudio = false;
        continueRulesetActionRow(rulesetStudioText("Close", "Schließen"), 110.0f);
        if (drawUiButton(rulesetStudioText("Close", "Schließen"), UiButtonRole::Secondary, ImVec2(110, 0)))
        {
            if (rulesetDraftDirty)
            {
                _pendingRulesetExitIntent = RulesetStudioExitIntent::CloseStudio;
                ImGui::OpenPopup(rulesetStudioText("Unsaved ruleset changes", "Ungespeicherte Regelwerksänderungen"));
            }
            else
            {
                closeRulesetStudio = true;
            }
        }

        if (_rulesetLabStep == RulesetLabStep::Review)
        {
            continueRulesetActionRow(rulesetStudioText("Save Draft", "Entwurf speichern"), 190.0f);
            if (drawUiButton("Save Draft",
                             canSaveDraft,
                             "Ruleset name is required.",
                             UiButtonRole::Advanced,
                             ImVec2(190, 0),
                             rulesetStudioText(
                                 "Save this editable draft even when publishing validation is still incomplete.",
                                 "Speichert diesen bearbeitbaren Entwurf auch bei noch unvollständiger Veröffentlichungsvalidierung.")))
            {
                _addRulesetError.clear();
                try
                {
                    _controller.saveInstalledRulesetDefinition(reviewDefinition);
                    _statusMessage = std::string(rulesetStudioText("Saved ruleset draft: ", "Regelwerksentwurf gespeichert: ")) + reviewDefinition.name;
                    _rulesetSourceDefinition = reviewDefinition;
                    _editingRulesetId = reviewDefinition.rulesetId;
                    _rulesetDraftBaselineFingerprint = rulesetDraftFingerprint(reviewDefinition, _selectedRulesetTemplateIndex);
                    _rulesetStudioEditing = false;
                }
                catch (const std::exception &error)
                {
                    _addRulesetError = std::string(rulesetStudioText("Could not save the ruleset draft: ",
                                                                    "Der Regelwerksentwurf konnte nicht gespeichert werden: ")) +
                                           error.what();
                }
            }

            continueRulesetActionRow(rulesetStudioText("Publish Version", "Version veröffentlichen"), 190.0f);
            if (drawUiButton("Publish Version",
                             canPublish,
                             nameMissing ? "Ruleset name is required."
                                         : (versionMissing ? "A version is required for publishing."
                                                           : "Resolve validation issues before publishing."),
                             UiButtonRole::Primary,
                             ImVec2(190, 0),
                             "Publishes an immutable executable version that campaigns can pin."))
            {
                _addRulesetError.clear();
                ImGui::OpenPopup(rulesetStudioText("Publish ruleset version?", "Regelwerk-Version veröffentlichen?"));
            }
        }
        else
        {
            continueRulesetActionRow(rulesetStudioText("Next", "Weiter"), 110.0f);
            if (drawUiButton("Next", UiButtonRole::Primary, ImVec2(110, 0)))
            {
                _addRulesetError.clear();
                _rulesetLabStep = rulesetLabStepFromIndex(rulesetLabStepIndex(_rulesetLabStep) + 1);
            }
        }

        if (!_addRulesetError.empty())
        {
            ImGui::Separator();
            const std::string localizedRulesetError = gm::rulesets::formatBackendError(
                _addRulesetError, gm::localization::LocalizationService::instance().locale());
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), localizedUiLabel("%s").c_str(),
                               localizedRulesetError.c_str());
        }

        const char *templatePopupId = rulesetStudioText("Replace draft with template?", "Entwurf durch Vorlage ersetzen?");
        if (ImGui::IsPopupOpen(templatePopupId))
        {
            setResponsiveAutoSizeWindow(540.0f, 340.0f);
            if (ImGui::BeginPopupModal(templatePopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetStudioText(
                    "Applying another template replaces every current ruleset list and the base ruleset ID. This cannot be undone.",
                    "Eine andere Vorlage ersetzt alle aktuellen Regelwerkslisten und die Basis-Regelwerk-ID. Dies kann nicht rückgängig gemacht werden."));
                if (_pendingRulesetTemplateIndex >= 0)
                    drawInlineLabelValue(rulesetStudioText("Replacement", "Neue Vorlage"),
                                         rulesetTemplateIntentName(_pendingRulesetTemplateIndex));

                const UiModalAction action = drawModalActionButtonsEx(
                    rulesetStudioText("Replace with Template", "Durch Vorlage ersetzen"),
                    UiButtonRole::Destructive,
                    rulesetStudioText("Keep Current Draft", "Aktuellen Entwurf behalten"),
                    170.0f);
                if (action == UiModalAction::Confirm && _pendingRulesetTemplateIndex >= 0)
                {
                    _selectedRulesetTemplateIndex = _pendingRulesetTemplateIndex;
                    applyRulesetTemplateIntent(_selectedRulesetTemplateIndex,
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
                                               _rulesetMonsterTemplates);
                    _addRulesetError.clear();
                    _pendingRulesetTemplateIndex = -1;
                }
                else if (action == UiModalAction::Cancel)
                {
                    _pendingRulesetTemplateIndex = -1;
                }
                ImGui::EndPopup();
            }
        }

        const char *publishPopupId = rulesetStudioText("Publish ruleset version?", "Regelwerk-Version veröffentlichen?");
        if (ImGui::IsPopupOpen(publishPopupId))
        {
            setResponsiveAutoSizeWindow(560.0f, 340.0f);
            if (ImGui::BeginPopupModal(publishPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetStudioText(
                    "Publishing creates an immutable version for campaign pinning and removes this editable draft. Later changes require a new version draft.",
                    "Das Veröffentlichen erzeugt eine unveränderliche Version für Kampagnen und entfernt diesen bearbeitbaren Entwurf. Spätere Änderungen benötigen einen neuen Versionsentwurf."));
                drawInlineLabelValue(rulesetStudioText("Ruleset", "Regelwerk"), reviewDefinition.name.c_str());
                drawInlineLabelValue(rulesetStudioText("Version", "Version"), reviewDefinition.version.c_str());

                const UiModalAction action = drawModalActionButtonsEx(
                    rulesetStudioText("Publish Version", "Version veröffentlichen"),
                    UiButtonRole::Destructive,
                    rulesetStudioText("Keep Editing", "Weiter bearbeiten"),
                    170.0f);
                if (action == UiModalAction::Confirm)
                {
                    const RulesetPublishResult published = publishRulesetVersion(reviewDefinition, currentUtcTimestamp());
                    if (!published.success)
                    {
                        _addRulesetError = published.errors.empty()
                                               ? rulesetStudioText("Publishing failed.", "Veröffentlichen fehlgeschlagen.")
                                               : published.errors.front();
                    }
                    else
                    {
                        const std::vector<RulesetStudioFamily> installedFamilies = _controller.loadRulesetStudioLibrary();
                        const bool versionAlreadyPublished = std::any_of(
                            installedFamilies.begin(),
                            installedFamilies.end(),
                            [&](const RulesetStudioFamily &family) {
                                if (family.rulesetId != published.version.rulesetId)
                                    return false;
                                return std::any_of(
                                    family.publishedVersions.begin(),
                                    family.publishedVersions.end(),
                                    [&](const RulesetStudioVersionEntry &entry) {
                                        return entry.definition.version == published.version.version;
                                    });
                            });
                        if (versionAlreadyPublished)
                        {
                            _addRulesetError = rulesetStudioText(
                                "This ruleset version is already published. Choose a new version number; the draft was kept.",
                                "Diese Regelwerk-Version ist bereits veröffentlicht. Wähle eine neue Versionsnummer; der Entwurf wurde beibehalten.");
                        }
                        else
                        {
                            try
                            {
                                _controller.saveInstalledRulesetDefinition(published.version);
                                _controller.removeInstalledRulesetDefinition(reviewDefinition.metadata.id);
                                _statusMessage = std::string(rulesetStudioText("Published ", "Veröffentlicht: ")) +
                                                 published.version.name + " " + published.version.version;
                                _rulesetStudioEditing = false;
                            }
                            catch (const std::exception &error)
                            {
                                _addRulesetError = std::string(rulesetStudioText(
                                    "Publishing failed; the editable draft was kept: ",
                                    "Veröffentlichen fehlgeschlagen; der bearbeitbare Entwurf wurde beibehalten: ")) +
                                                       error.what();
                            }
                        }
                    }
                }
                ImGui::EndPopup();
            }
        }

        const char *unsavedPopupId = rulesetStudioText("Unsaved ruleset changes", "Ungespeicherte Regelwerksänderungen");
        if (ImGui::IsPopupOpen(unsavedPopupId))
        {
            setResponsiveAutoSizeWindow(540.0f, 340.0f);
            if (ImGui::BeginPopupModal(unsavedPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetStudioText(
                    "This ruleset draft has changes that have not been saved. Discard them and continue?",
                    "Dieser Regelwerksentwurf enthält ungespeicherte Änderungen. Möchtest du sie verwerfen und fortfahren?"));
                const UiModalAction action = drawModalActionButtonsEx(
                    rulesetStudioText("Discard Changes", "Änderungen verwerfen"),
                    UiButtonRole::Destructive,
                    rulesetStudioText("Keep Editing", "Weiter bearbeiten"),
                    160.0f);
                if (action == UiModalAction::Confirm)
                {
                    if (_pendingRulesetExitIntent == RulesetStudioExitIntent::ShowLibrary)
                        _rulesetStudioEditing = false;
                    else if (_pendingRulesetExitIntent == RulesetStudioExitIntent::CloseStudio)
                        closeRulesetStudio = true;
                    _pendingRulesetExitIntent = RulesetStudioExitIntent::None;
                }
                else if (action == UiModalAction::Cancel)
                {
                    _pendingRulesetExitIntent = RulesetStudioExitIntent::None;
                }
                ImGui::EndPopup();
            }
        }
    return closeRulesetStudio;
}
