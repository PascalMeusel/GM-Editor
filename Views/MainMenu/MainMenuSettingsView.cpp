#include "MainMenuCampaignSupport.hpp"

void MainMenuWindowView::openSettingsPopup()
{
    const ProjectSaveConfig &config = _controller.projectSaveConfig();
    _settingsAutosaveEnabled = config.autosaveEnabled;
    _settingsBackupSnapshotsEnabled = config.backupSnapshotsEnabled;
    _settingsRecoverySnapshotsEnabled = config.recoverySnapshotsEnabled;
    _settingsAutosaveIntervalSeconds = config.autosaveIntervalSeconds;
    _settingsMaxBackupSnapshots = config.maxBackupSnapshots;
    _settingsMaxRecoverySnapshots = config.maxRecoverySnapshots;
    _settingsStatus.clear();
    _showSettingsPopup = true;
    _requestOpenSettingsPopup = true;
}

void MainMenuWindowView::handleSettingsPopup()
{
    const std::string popupTitle = std::string(gm::localization::tr(gm::localization::TextId::AppSettings)) + "###Settings";
    if (_requestOpenSettingsPopup)
    {
        ImGui::OpenPopup(popupTitle.c_str());
        _requestOpenSettingsPopup = false;
    }

    setResponsiveAutoSizeWindow(620.0f, 480.0f);
    if (ImGui::BeginPopupModal(popupTitle.c_str(), &_showSettingsPopup, ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto &localization = gm::localization::LocalizationService::instance();
        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::SettingsLanguage));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::SettingsLanguageHelp));
        int languageIndex = static_cast<int>(localization.preference());
        const char *languageOptions[] = {
            gm::localization::tr(gm::localization::TextId::SettingsLanguageSystem),
            gm::localization::tr(gm::localization::TextId::SettingsLanguageEnglish),
            gm::localization::tr(gm::localization::TextId::SettingsLanguageGerman)};
        ImGui::SetNextItemWidth(320.0f);
        if (FormUi::Combo(localizedUiLabel("##Language").c_str(), &languageIndex, languageOptions, 3))
        {
            std::string error;
            if (localization.setPreference(static_cast<gm::localization::LocalePreference>(languageIndex), true, &error))
                _settingsStatus = gm::localization::tr(gm::localization::TextId::SettingsApplied);
            else
                _settingsStatus = error;
        }

        ImGui::SeparatorText(launcherText("Accessibility", "Barrierefreiheit"));
        ImGui::TextWrapped("%s", launcherText(
            "Changes apply immediately. The same controls remain available from View > Accessibility.",
            "Änderungen gelten sofort. Dieselben Optionen sind auch unter Ansicht > Barrierefreiheit verfügbar."));
        gm::settings::AccessibilitySettings accessibility = gm::settings::activeAccessibilitySettings();
        int textScaleIndex = accessibility.textScale >= 1.40f ? 4 : accessibility.textScale >= 1.24f ? 3 :
                             (accessibility.textScale >= 1.09f ? 2 :
                              (accessibility.textScale <= 0.95f ? 0 : 1));
        const char *textScaleOptions[] = {
            launcherText("Compact (92%)", "Kompakt (92 %)"),
            launcherText("Default (100%)", "Standard (100 %)"),
            launcherText("Large (115%)", "Groß (115 %)"),
            launcherText("Extra large (130%)", "Sehr groß (130 %)"),
            launcherText("Maximum (150%)", "Maximal (150 %)")};
        bool accessibilityChanged = false;
        ImGui::SetNextItemWidth(std::min(320.0f, ImGui::GetContentRegionAvail().x));
        if (FormUi::Combo(launcherText("Text size##LauncherAccessibilityTextSize", "Textgröße##LauncherAccessibilityTextSize"),
                         &textScaleIndex, textScaleOptions, 5))
        {
            static constexpr float kTextScales[] = {0.92f, 1.0f, 1.15f, 1.30f, 1.50f};
            accessibility.textScale = kTextScales[std::clamp(textScaleIndex, 0, 4)];
            accessibilityChanged = true;
        }
        if (FormUi::Checkbox(launcherText("High contrast", "Hoher Kontrast"), &accessibility.highContrast))
            accessibilityChanged = true;
        if (FormUi::Checkbox(launcherText("Compact tables", "Kompakte Tabellen"), &accessibility.compactTables))
            accessibilityChanged = true;
        if (FormUi::Checkbox(launcherText("Always show keyboard focus", "Tastaturfokus immer anzeigen"),
                            &accessibility.alwaysShowFocus))
            accessibilityChanged = true;
        if (accessibilityChanged)
        {
            std::string error;
            _settingsStatus = gm::settings::applyAccessibilitySettings(accessibility, &error)
                                  ? launcherText("Accessibility settings applied and saved.", "Barrierefreiheit angewendet und gespeichert.")
                                  : launcherText("The setting is active for this session, but could not be saved.",
                                                 "Die Einstellung ist für diese Sitzung aktiv, konnte aber nicht gespeichert werden.");
        }
        if (drawUiButton(launcherText("Show keyboard help", "Tastaturhilfe anzeigen"),
                         UiButtonRole::Secondary,
                         ImVec2(210.0f, 0.0f)))
            gm::settings::requestKeyboardHelp();

        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::SettingsProjectSaves));
        FormUi::Checkbox(gm::localization::tr(gm::localization::TextId::SettingsAutosave), &_settingsAutosaveEnabled);
        if (!_settingsAutosaveEnabled)
            ImGui::BeginDisabled();
        FormUi::InputInt(gm::localization::tr(gm::localization::TextId::SettingsAutosaveInterval), &_settingsAutosaveIntervalSeconds, 10, 60);
        if (!_settingsAutosaveEnabled)
            ImGui::EndDisabled();

        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::SettingsRecovery));
        FormUi::Checkbox(gm::localization::tr(gm::localization::TextId::SettingsRecoverySnapshots), &_settingsRecoverySnapshotsEnabled);
        if (!_settingsRecoverySnapshotsEnabled)
            ImGui::BeginDisabled();
        FormUi::InputInt(gm::localization::tr(gm::localization::TextId::SettingsMaxRecoverySnapshots), &_settingsMaxRecoverySnapshots, 1, 5);
        if (!_settingsRecoverySnapshotsEnabled)
            ImGui::EndDisabled();

        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::SettingsBackups));
        FormUi::Checkbox(gm::localization::tr(gm::localization::TextId::SettingsPreSaveBackups), &_settingsBackupSnapshotsEnabled);
        if (!_settingsBackupSnapshotsEnabled)
            ImGui::BeginDisabled();
        FormUi::InputInt(gm::localization::tr(gm::localization::TextId::SettingsMaxBackupSnapshots), &_settingsMaxBackupSnapshots, 1, 5);
        if (!_settingsBackupSnapshotsEnabled)
            ImGui::EndDisabled();
        if (drawUiButton(launcherText("Browse & Restore Backups...", "Sicherungen durchsuchen & wiederherstellen..."),
                         UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f)))
        {
            _showSettingsPopup = false;
            ImGui::CloseCurrentPopup();
            openBackupBrowserPopup();
        }

        ImGui::SeparatorText(launcherText("About GM Editor", "Über GM Editor"));
        ImGui::Text("%s %s · %s", ProductInfo::name, ProductInfo::version, ProductInfo::releaseChannel);
        const std::string storagePath = ApplicationPaths::storageDescription();
        ImGui::TextWrapped("%s: %s", launcherText("Local data", "Lokale Daten"), storagePath.c_str());
        if (drawUiButton(launcherText("Copy data folder", "Datenordner kopieren"),
                         UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f)))
        {
            ImGui::SetClipboardText(storagePath.c_str());
            _settingsStatus = launcherText("Data folder copied.", "Datenordner kopiert.");
        }

        ImGui::Separator();
        if (ImGui::Button(gm::localization::tr(gm::localization::TextId::CommonApply), ImVec2(120, 0)))
        {
            _settingsAutosaveIntervalSeconds = std::max(10, _settingsAutosaveIntervalSeconds);
            _settingsMaxRecoverySnapshots = std::clamp(_settingsMaxRecoverySnapshots, 1, 50);
            _settingsMaxBackupSnapshots = std::clamp(_settingsMaxBackupSnapshots, 1, 50);
            std::string saveError;
            const bool saved = _controller.applyProjectSaveSettings(_settingsAutosaveEnabled,
                                                                     _settingsAutosaveIntervalSeconds,
                                                                     _settingsBackupSnapshotsEnabled,
                                                                     _settingsMaxBackupSnapshots,
                                                                     _settingsRecoverySnapshotsEnabled,
                                                                     _settingsMaxRecoverySnapshots,
                                                                     saveError);
            _settingsStatus = saved
                                  ? gm::localization::tr(gm::localization::TextId::SettingsApplied)
                                  : std::string(launcherText("Could not save settings: ", "Einstellungen konnten nicht gespeichert werden: ")) + saveError;
            _statusMessage = _settingsStatus;
        }

        ImGui::SameLine();
        if (ImGui::Button(gm::localization::tr(gm::localization::TextId::CommonClose), ImVec2(120, 0)))
        {
            _showSettingsPopup = false;
            ImGui::CloseCurrentPopup();
        }

        if (!_settingsStatus.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("%s").c_str(), _settingsStatus.c_str());
        }

        ImGui::EndPopup();
    }
}
