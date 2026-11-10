// Internal renderWorkspaceLanding continuation: language, accessibility, and ruleset settings.
    if (_activeWorkspace == EditorWorkspace::Settings)
    {
        auto &localization = gm::localization::LocalizationService::instance();
        const Campaign &campaign = _controller.currentCampaign();
        if (_descriptionCampaignId != campaign.metadata.id)
        {
            _descriptionCampaignId = campaign.metadata.id;
            _campaignNameDraft = campaign.name;
            _descriptionDraft = campaign.description;
            _campaignDetailsStatus.clear();
        }

        ImGui::SeparatorText(uiText("Campaign details", "Kampagnendetails"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                           uiText("Change the campaign name or description here. Details are saved only with Save details.",
                                  "Ändere hier den Kampagnennamen oder die Beschreibung. Details werden nur mit Details speichern gesichert."));
        ImGui::TextUnformatted(uiText("Campaign name", "Kampagnenname"));
        FormUi::InputText("##CampaignDetailsName", &_campaignNameDraft);
        ImGui::TextUnformatted(uiText("Description", "Beschreibung"));
        FormUi::InputTextMultiline("##CampaignDetailsDescription", &_descriptionDraft,
                                   ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4.0f), ImGuiInputTextFlags_WordWrap);

        const bool detailsDirty = _campaignNameDraft != campaign.name || _descriptionDraft != campaign.description;
        if (detailsDirty)
            ImGui::TextColored(WorkspaceUi::Amber, "%s", uiText("Details draft — save or discard it before leaving.",
                                                                    "Details-Entwurf — vor dem Verlassen speichern oder verwerfen."));
        if (ImGui::BeginTable("CampaignDetailsActions", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (drawUiButton(uiText("Save details", "Details speichern"), UiButtonRole::Primary, ImVec2(0.0f, 0.0f),
                             uiText("Save this campaign name and description.",
                                    "Speichert diesen Kampagnennamen und diese Beschreibung.")))
            {
                std::string error;
                if (_controller.updateCampaignDetails(_campaignNameDraft, _descriptionDraft, &error))
                {
                    _campaignNameDraft = _controller.currentCampaign().name;
                    _descriptionDraft = _controller.currentCampaign().description;
                    _campaignDetailsStatus = uiText("Campaign details saved.", "Kampagnendetails gespeichert.");
                }
                else
                    _campaignDetailsStatus = error;
            }
            ImGui::TableNextColumn();
            if (drawUiButton(uiText("Discard details draft", "Details-Entwurf verwerfen"), UiButtonRole::Secondary,
                             ImVec2(0.0f, 0.0f),
                             uiText("Restore the last saved campaign details.",
                                    "Stellt die zuletzt gespeicherten Kampagnendetails wieder her.")))
            {
                _campaignNameDraft = campaign.name;
                _descriptionDraft = campaign.description;
                _campaignDetailsStatus.clear();
            }
            ImGui::EndTable();
        }
        if (!_campaignDetailsStatus.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _campaignDetailsStatus.c_str());

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::SettingsLanguage));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                           gm::localization::tr(gm::localization::TextId::SettingsLanguageHelp));
        int languageIndex = static_cast<int>(localization.preference());
        const char *languageOptions[] = {gm::localization::tr(gm::localization::TextId::SettingsLanguageSystem),
                                         gm::localization::tr(gm::localization::TextId::SettingsLanguageEnglish),
                                         gm::localization::tr(gm::localization::TextId::SettingsLanguageGerman)};
        ImGui::SetNextItemWidth(std::min(360.0f, ImGui::GetContentRegionAvail().x));
        if (FormUi::Combo(localizedUiLabel("##Language").c_str(), &languageIndex, languageOptions, 3))
        {
            std::string error;
            if (localization.setPreference(static_cast<gm::localization::LocalePreference>(languageIndex), true,
                                           &error))
                _localizationStatus = gm::localization::tr(gm::localization::TextId::SettingsApplied);
            else
                _localizationStatus = error;
        }
        if (!_localizationStatus.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _localizationStatus.c_str());

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::SeparatorText(uiText("Campaign content language", "Sprache der Kampagneninhalte"));
        ImGui::TextWrapped(
            localizedUiLabel("%s").c_str(),
            uiText(
                "Controls generated place names, map descriptions, and suggestions for this campaign. It does not change the interface language.",
                "Steuert erzeugte Ortsnamen, Kartenbeschreibungen und Vorschläge für diese Kampagne. Die Sprache der Oberfläche bleibt unverändert."));
        int contentLanguageIndex = _controller.currentCampaign().contentLanguage == "de" ? 1 : 0;
        const char *contentLanguageOptions[] = {uiText("English", "Englisch"), uiText("German", "Deutsch")};
        ImGui::SetNextItemWidth(std::min(360.0f, ImGui::GetContentRegionAvail().x));
        if (FormUi::Combo("##CampaignContentLanguage", &contentLanguageIndex, contentLanguageOptions,
                          static_cast<int>(std::size(contentLanguageOptions))))
        {
            if (_controller.setCampaignContentLanguage(contentLanguageIndex == 1 ? "de" : "en"))
                _localizationStatus =
                    uiText("Campaign content language saved.", "Sprache der Kampagneninhalte gespeichert.");
            else
                _localizationStatus = uiText("The campaign content language could not be saved.",
                                             "Die Sprache der Kampagneninhalte konnte nicht gespeichert werden.");
        }

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::SeparatorText(uiText("Accessibility", "Barrierefreiheit"));
        ImGui::TextWrapped(
            localizedUiLabel("%s").c_str(),
            uiText(
                "Adjust readability here or from View > Accessibility. Changes apply immediately and are saved for the next start.",
                "Passe die Lesbarkeit hier oder über Ansicht > Barrierefreiheit an. Änderungen gelten sofort und werden für den nächsten Start gespeichert."));

        gm::settings::AccessibilitySettings accessibility = gm::settings::activeAccessibilitySettings();
        int textScaleIndex = accessibility.textScale >= 1.40f ? 4
                             : accessibility.textScale >= 1.24f
                                 ? 3
                                 : (accessibility.textScale >= 1.09f ? 2 : (accessibility.textScale <= 0.95f ? 0 : 1));
        const char *textScaleOptions[] = {
            uiText("Compact (92%)", "Kompakt (92 %)"), uiText("Default (100%)", "Standard (100 %)"),
            uiText("Large (115%)", "Groß (115 %)"), uiText("Extra large (130%)", "Sehr groß (130 %)"),
            uiText("Maximum (150%)", "Maximal (150 %)")};
        bool accessibilityChanged = false;
        ImGui::SetNextItemWidth(std::min(360.0f, ImGui::GetContentRegionAvail().x));
        if (FormUi::Combo(uiText("Text size##AccessibilityTextSize", "Textgröße##AccessibilityTextSize"),
                          &textScaleIndex, textScaleOptions, static_cast<int>(std::size(textScaleOptions))))
        {
            static constexpr float kTextScales[] = {0.92f, 1.0f, 1.15f, 1.30f, 1.50f};
            accessibility.textScale = kTextScales[std::clamp(textScaleIndex, 0, 4)];
            accessibilityChanged = true;
        }

        const float accessibilityWidth = ImGui::GetContentRegionAvail().x;
        const bool accessibilityInline = accessibilityWidth >= 640.0f;
        if (FormUi::Checkbox(uiText("High contrast", "Hoher Kontrast"), &accessibility.highContrast))
            accessibilityChanged = true;
        showHelpTooltip(uiText("Strengthens text, focus, and separator contrast throughout the application.",
                               "Verstärkt Text-, Fokus- und Trennlinienkontrast in der gesamten Anwendung."));
        if (accessibilityInline)
            ImGui::SameLine(0.0f, 24.0f);
        if (FormUi::Checkbox(uiText("Compact tables", "Kompakte Tabellen"), &accessibility.compactTables))
            accessibilityChanged = true;
        showHelpTooltip(
            uiText("Reduces table padding while keeping normal buttons and form controls readable.",
                   "Verringert Tabellenabstände, ohne normale Schaltflächen und Formularfelder zu verkleinern."));
        if (accessibilityInline)
            ImGui::SameLine(0.0f, 24.0f);
        if (FormUi::Checkbox(uiText("Always show keyboard focus", "Tastaturfokus immer anzeigen"),
                             &accessibility.alwaysShowFocus))
            accessibilityChanged = true;
        showHelpTooltip(uiText("Keeps the keyboard focus marker visible even after using the mouse.",
                               "Hält die Tastatur-Fokusmarkierung auch nach der Mausbedienung sichtbar."));

        if (accessibilityChanged)
        {
            std::string error;
            if (gm::settings::applyAccessibilitySettings(accessibility, &error))
                _accessibilityStatus =
                    uiText("Accessibility settings applied and saved.", "Barrierefreiheit angewendet und gespeichert.");
            else
                _accessibilityStatus =
                    uiText("The setting is active for this session, but could not be saved.",
                           "Die Einstellung ist für diese Sitzung aktiv, konnte aber nicht gespeichert werden.");
        }
        if (drawUiButton(uiText("Show keyboard help", "Tastaturhilfe anzeigen"), UiButtonRole::Secondary,
                         ImVec2(210.0f, 0.0f),
                         uiText("Open the keyboard navigation and shortcut overview.",
                                "Öffnet die Übersicht für Tastaturnavigation und Tastenkürzel.")))
            gm::settings::requestKeyboardHelp();
        if (!_accessibilityStatus.empty())
        {
            ImGui::TextColored(WorkspaceUi::Teal, localizedUiLabel("%s").c_str(), _accessibilityStatus.c_str());
        }

        ImGui::Dummy(ImVec2(0.0f, 12.0f));
        ImGui::SeparatorText(uiText("Campaign ruleset", "Kampagnenregelwerk"));
        std::string currentRuleset =
            campaign.ruleSet
                ? std::string(campaign.ruleSet->getName())
                : std::string(campaign.rulesetReference.rulesetId.empty()
                                  ? uiText("No ruleset", "Kein Regelwerk")
                                  : uiText("Installed ruleset unavailable", "Installiertes Regelwerk nicht verfügbar"));
        if (!campaign.rulesetReference.version.empty())
        {
            std::string readableVersion = campaign.rulesetReference.version;
            const std::string internalPrefix = campaign.rulesetReference.rulesetId + "-";
            if (!campaign.rulesetReference.rulesetId.empty() && readableVersion.rfind(internalPrefix, 0) == 0)
                readableVersion.erase(0, internalPrefix.size());
            currentRuleset += std::string(" · ") + uiText("Version ", "Version ") + readableVersion;
        }
        ImGui::TextWrapped(localizedUiLabel("%s: %s").c_str(), uiText("Pinned version", "Fixierte Version"),
                           currentRuleset.c_str());
        if (campaign.rulesetReference.rulesetId == "dnd-2024" ||
            (campaign.ruleSet && std::string(campaign.ruleSet->getId()) == "dnd-2024"))
            ImGui::TextWrapped("%s", uiText(
                "Mixed SRD assistance: selected 5.2.1 rules and 5.1 catalog content. This is an incomplete implementation. Character advancement shows calculated changes and explicit overrides; the GM adjudicates unsupported rules. Existing version pins are preserved.",
                "Gemischte SRD-Regelhilfe: ausgewählte Regeln aus 5.2.1 und Kataloginhalte aus 5.1. Die Umsetzung ist unvollständig. Der Stufenaufstieg zeigt berechnete Änderungen und eigene Anpassungen; die SL entscheidet bei nicht unterstützten Regeln. Vorhandene Versionsbindungen bleiben erhalten."));
        std::vector<RulesetDefinitionModel> rulesetUpgrades = _controller.availableRulesetUpgrades();
        if (rulesetUpgrades.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(),
                                uiText("No other published version is installed for this ruleset.",
                                       "Für dieses Regelwerk ist keine weitere veröffentlichte Version installiert."));
        }
        else
        {
            _selectedRulesetUpgradeIndex =
                std::clamp(_selectedRulesetUpgradeIndex, 0, static_cast<int>(rulesetUpgrades.size()) - 1);
            std::vector<std::string> upgradeLabels;
            std::vector<const char *> upgradeItems;
            upgradeLabels.reserve(rulesetUpgrades.size());
            upgradeItems.reserve(rulesetUpgrades.size());
            for (const RulesetDefinitionModel &definition : rulesetUpgrades)
                upgradeLabels.push_back(definition.name + " · " + definition.version);
            for (const std::string &label : upgradeLabels)
                upgradeItems.push_back(label.c_str());
            ImGui::SetNextItemWidth(std::min(420.0f, ImGui::GetContentRegionAvail().x));
            FormUi::Combo(localizedUiLabel("##RulesetUpgrade").c_str(), &_selectedRulesetUpgradeIndex,
                          upgradeItems.data(), static_cast<int>(upgradeItems.size()));
            ImGui::SameLine();
            if (drawUiButton(uiText("Review upgrade", "Upgrade prüfen"), UiButtonRole::Advanced, ImVec2(160.0f, 0.0f)))
            {
                _rulesetUpgradeConfirmed = false;
                _requestRulesetUpgradePreview = true;
            }

            if (_requestRulesetUpgradePreview)
            {
                ImGui::OpenPopup(localizedUiLabel("Ruleset Upgrade Preview").c_str());
                _requestRulesetUpgradePreview = false;
                _rulesetUpgradePreviewOpen = true;
            }
            setResponsiveAutoSizeWindow(720.0f, 480.0f);
            if (ImGui::BeginPopupModal(localizedUiLabel("Ruleset Upgrade Preview").c_str(), &_rulesetUpgradePreviewOpen,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                const RulesetDefinitionModel &next =
                    rulesetUpgrades[static_cast<std::size_t>(_selectedRulesetUpgradeIndex)];
                const RulesetUpgradePreview preview = _controller.previewRulesetUpgrade(next);
                ImGui::Text(localizedUiLabel("%s").c_str(), uiText("Migration preview", "Migrationsvorschau"));
                ImGui::TextWrapped(localizedUiLabel("%s -> %s").c_str(), currentRuleset.c_str(), next.version.c_str());
                if (preview.overlayApplied)
                {
                    ImGui::TextColored(WorkspaceUi::Teal, localizedUiLabel("%s").c_str(),
                                       uiText("Campaign overlay preserved", "Kampagnen-Overlay bleibt erhalten"));
                    ImGui::TextWrapped(
                        uiText(
                            "The effective definitions include overlay %s (%s) before differences are calculated.",
                            "Die wirksamen Definitionen enthalten Overlay %s (%s), bevor Unterschiede berechnet werden."),
                        preview.overlayId.c_str(), preview.overlayVersion.c_str());
                }
                ImGui::Separator();
                const gm::localization::Locale rulesetLocale =
                    gm::localization::LocalizationService::instance().locale();
                if (!preview.structuredChanges.empty())
                {
                    for (const RulesetUpgradeChange &change : preview.structuredChanges)
                    {
                        const std::string localizedChange = gm::rulesets::formatUpgradeChange(change, rulesetLocale);
                        ImGui::Bullet();
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", localizedChange.c_str());
                    }
                }
                else
                {
                    for (const std::string &change : preview.changes)
                    {
                        const std::string localizedChange =
                            gm::rulesets::formatLegacyUpgradeChange(change, rulesetLocale);
                        ImGui::Bullet();
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", localizedChange.c_str());
                    }
                }
                if (preview.changes.empty())
                    ImGui::TextDisabled(localizedUiLabel("%s").c_str(),
                                        uiText("No stable-ID structure changes detected.",
                                               "Keine Änderungen an der Stable-ID-Struktur erkannt."));
                if (!preview.affectedStableIds.empty())
                {
                    ImGui::SeparatorText(uiText("Affected stable IDs", "Betroffene Stable-IDs"));
                    ImGui::TextWrapped(uiText("%d character(s), %d stored value(s) directly reference these IDs.",
                                              "%d Charakter(e), %d gespeicherte Werte verweisen direkt auf diese IDs."),
                                       preview.affectedCharacterCount, preview.affectedCharacterValueCount);
                    for (const std::string &id : preview.affectedStableIds)
                    {
                        ImGui::Bullet();
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", id.c_str());
                    }
                }
                if (!preview.errors.empty())
                {
                    ImGui::SeparatorText(uiText("Blocking errors", "Blockierende Fehler"));
                    if (!preview.validationDiagnostics.empty())
                    {
                        for (const RulesetValidationDiagnostic &diagnostic : preview.validationDiagnostics)
                        {
                            const std::string path = gm::rulesets::formatDiagnosticPath(diagnostic.path, rulesetLocale);
                            const std::string message =
                                gm::rulesets::formatValidationDiagnostic(diagnostic, rulesetLocale);
                            const std::string localizedError = path.empty() ? message : path + ": " + message;
                            ImGui::Bullet();
                            ImGui::SameLine();
                            ImGui::TextWrapped("%s", localizedError.c_str());
                        }
                    }
                    else
                    {
                        for (const std::string &error : preview.errors)
                        {
                            const std::string localizedError = gm::rulesets::formatBackendError(error, rulesetLocale);
                            ImGui::Bullet();
                            ImGui::SameLine();
                            ImGui::TextWrapped("%s", localizedError.c_str());
                        }
                    }
                }
                FormUi::Checkbox(uiText("I reviewed the migration preview", "Ich habe die Migrationsvorschau geprüft"),
                                 &_rulesetUpgradeConfirmed);
                const bool canApply = preview.valid && _rulesetUpgradeConfirmed;
                if (drawUiButton(uiText("Apply upgrade", "Upgrade anwenden"), canApply,
                                 uiText("Resolve validation errors and confirm the preview first.",
                                        "Behebe Validierungsfehler und bestätige zuerst die Vorschau."),
                                 UiButtonRole::Primary, ImVec2(150.0f, 0.0f)))
                {
                    std::vector<std::string> errors;
                    if (_controller.applyRulesetUpgrade(next, errors))
                    {
                        _rulesetUpgradeStatus = uiText("Ruleset upgrade applied and saved.",
                                                       "Regelwerk-Upgrade angewendet und gespeichert.");
                        _rulesetUpgradePreviewOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        _rulesetUpgradeStatus =
                            errors.empty() ? uiText("Ruleset upgrade failed.", "Regelwerk-Upgrade fehlgeschlagen.")
                                           : gm::rulesets::formatBackendError(errors.front(), rulesetLocale);
                    }
                }
                ImGui::SameLine();
                if (drawUiButton(gm::localization::tr(gm::localization::TextId::CommonCancel), UiButtonRole::Secondary,
                                 ImVec2(120.0f, 0.0f)))
                {
                    _rulesetUpgradePreviewOpen = false;
                    ImGui::CloseCurrentPopup();
                }
