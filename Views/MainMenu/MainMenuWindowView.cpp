#include "MainMenuCampaignSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

void MainMenuWindowView::update(std::shared_ptr<GLFWwindow> window)
{
    _currentChoice = MainMenuOption::None;
    // Get the main viewport to determine the available drawing area
    ImGuiViewport *mainViewport = ImGui::GetMainViewport();

    // Set our next window to be the size of the main viewport, with no decorations
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(1.0f);

    // Define window flags to make it act as a drawing canvas rather than a window
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoDocking;

    // Begin a "window" that covers the entire viewport.
    // This allows us to position elements within the main application window.
    if (ImGui::Begin(localizedUiLabel("MainMenuContainer").c_str(), nullptr, windowFlags))
    {
        const CompactUiDensityScope uiDensity;

        const std::vector<Campaign> &campaigns = _controller.loadCampaigns();
        const bool hasCampaigns = !campaigns.empty();
        if (hasCampaigns)
            _selectedLoadGameIndex = std::clamp(_selectedLoadGameIndex, 0, static_cast<int>(campaigns.size()) - 1);
        else
            _selectedLoadGameIndex = 0;
        const Campaign *selectedCampaign = hasCampaigns ? &campaigns[static_cast<std::size_t>(_selectedLoadGameIndex)] : nullptr;

        const float contentWidth = std::max(1.0f, std::min(1440.0f, mainViewport->WorkSize.x - 48.0f));
        const float contentHeight = std::max(1.0f, std::min(860.0f, mainViewport->WorkSize.y - 40.0f));
        const float startX = (mainViewport->WorkSize.x - contentWidth) * 0.5f;
        const float startY = (mainViewport->WorkSize.y - contentHeight) * 0.5f;

        ImGui::SetCursorPos(ImVec2(startX, startY));
        ImGui::BeginGroup();

        ImGui::SetWindowFontScale(1.35f);
        drawCenteredText(gm::localization::tr(gm::localization::TextId::ProductName), contentWidth, true);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, gm::approved_ui::Muted);
        drawCenteredText(gm::localization::tr(gm::localization::TextId::ProductTagline), contentWidth);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 18.0f));

        constexpr float panelGap = 20.0f;
        const float leftWidth = (contentWidth - panelGap) * 0.60f;
        const float rightWidth = contentWidth - leftWidth - panelGap;
        const float panelHeight = std::max(300.0f, startY + contentHeight - ImGui::GetCursorPosY());

        ImGui::PushStyleColor(ImGuiCol_ChildBg, gm::approved_ui::Surface);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::BeginChild("RecentCampaignsPanel", ImVec2(leftWidth, panelHeight), false);
        drawPanelHeading(gm::localization::tr(gm::localization::TextId::LauncherRecentCampaigns));
        ImGui::Separator();

        if (campaigns.empty())
        {
            if (getBoldFont())
                ImGui::PushFont(getBoldFont());
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LauncherNoRecentTitle));
            if (getBoldFont())
                ImGui::PopFont();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                "Start with a campaign. The guided starter gives you editable scenes, a map, an encounter, and example people and places.",
                "Beginne mit einer Kampagne. Der geführte Starter enthält bearbeitbare Szenen, eine Karte, eine Begegnung sowie Beispielpersonen und -orte."));
            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            ImGui::SeparatorText(launcherText("Your next step", "Dein nächster Schritt"));
            drawLauncherCapability(
                launcherText("Create a new campaign", "Neue Kampagne erstellen"),
                launcherText("Choose Guided starter in the form for a ready-to-edit first session, or choose Blank campaign workspace to begin from scratch.",
                             "Wähle im Formular die Geführte Startkampagne für eine bearbeitbare erste Sitzung oder den Leeren Kampagnenarbeitsbereich für einen eigenen Anfang."));
            drawLauncherCapability(
                launcherText("Already have a campaign?", "Bereits eine Kampagne?"),
                launcherText("Use Import Campaign Package to review and restore a saved .ttrpgpkg package.",
                             "Nutze Kampagnenpaket importieren, um ein gespeichertes .ttrpgpkg-Paket zu prüfen und wiederherzustellen."));
        }
        else
        {
            const auto campaignMatchesFilter = [&](const Campaign &campaign) {
                const std::string filter = trimCopy(_campaignFilter);
                if (filter.empty())
                    return true;
                const auto containsCaseInsensitive = [&](const std::string &value) {
                    return std::search(value.begin(), value.end(), filter.begin(), filter.end(),
                        [](unsigned char valueCharacter, unsigned char filterCharacter) {
                            return std::tolower(valueCharacter) == std::tolower(filterCharacter);
                        }) != value.end();
                };
                return containsCaseInsensitive(campaign.name) || containsCaseInsensitive(campaign.description);
            };
            std::vector<int> visibleCampaignIndices;
            for (int index = 0; index < static_cast<int>(campaigns.size()); ++index)
            {
                const Campaign &campaign = campaigns[static_cast<std::size_t>(index)];
                if (campaignMatchesFilter(campaign) &&
                    (!_liveCampaignsOnly || resumeStateForCampaign(campaign).available))
                    visibleCampaignIndices.push_back(index);
            }

            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputTextWithHint("##CampaignFilter",
                                      launcherText("Search campaigns", "Kampagnen suchen"),
                                      &_campaignFilter);
            FormUi::Checkbox(launcherText("Live sessions only", "Nur Live-Sitzungen"), &_liveCampaignsOnly);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", launcherText("Filters change the list, not your current selection.",
                                                    "Filter ändern die Liste, nicht die aktuelle Auswahl."));
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            const float campaignListHeight = std::clamp(panelHeight * 0.39f, 184.0f, 236.0f);
            ImGui::BeginChild("CampaignList", ImVec2(0.0f, campaignListHeight), false);
            if (visibleCampaignIndices.empty())
                ImGui::TextWrapped("%s", launcherText("No campaigns match this filter.",
                                                        "Keine Kampagnen entsprechen diesem Filter."));
            for (const int i : visibleCampaignIndices)
            {
                const Campaign &campaign = campaigns[i];
                const bool selected = _selectedLoadGameIndex == i;
                bool doubleClicked = false;
                if (drawCampaignListEntry(campaign, i, selected, doubleClicked))
                {
                    _selectedLoadGameIndex = i;
                    selectedCampaign = &campaign;
                }
                if (doubleClicked)
                    _controller.handleLoadGame(campaign);
                ImGui::Dummy(ImVec2(0.0f, 3.0f));
            }
            ImGui::EndChild();

            ImGui::BeginChild("SelectedCampaignDetail", ImVec2(0.0f, 0.0f), false);
            if (selectedCampaign && std::find(visibleCampaignIndices.begin(), visibleCampaignIndices.end(),
                                               _selectedLoadGameIndex) == visibleCampaignIndices.end())
                ImGui::TextDisabled("%s", launcherText("The selected campaign is hidden by the current filter.",
                                                         "Die ausgewählte Kampagne wird durch den aktuellen Filter ausgeblendet."));
            drawSelectedCampaignDetail(selectedCampaign);
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, panelGap);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, gm::approved_ui::Surface);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::BeginChild("ActionsPanel", ImVec2(rightWidth, panelHeight), false);
        drawPanelHeading(selectedCampaign
                             ? launcherText("Continue or create", "Fortsetzen oder erstellen")
                             : launcherText("Get started", "Loslegen"));
        ImGui::Separator();

        const ResumeStateSummary resumeState = selectedCampaign ? resumeStateForCampaign(*selectedCampaign) : ResumeStateSummary{};
        if (selectedCampaign)
        {
            if (getBoldFont())
                ImGui::PushFont(getBoldFont());
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), selectedCampaign->name.c_str());
            if (getBoldFont())
                ImGui::PopFont();
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), rulesetName(*selectedCampaign).c_str());
            if (resumeState.available)
                ImGui::TextColored(ImVec4(0.36f, 0.76f, 0.69f, 1.0f), localizedUiLabel("%s").c_str(),
                                   gm::localization::LocalizationService::instance().format(
                                       gm::localization::TextId::LauncherReadyToResume,
                                       {resumeState.mode}).c_str());
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), suggestedNextAction(*selectedCampaign).c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
        }
        else
        {
            if (getBoldFont())
                ImGui::PushFont(getBoldFont());
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText("Create your first campaign", "Erstelle deine erste Kampagne"));
            if (getBoldFont())
                ImGui::PopFont();
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                "Create a campaign or import an existing campaign package to get started.",
                "Erstelle eine Kampagne oder importiere ein bestehendes Kampagnenpaket, um loszulegen."));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                "Choose Guided starter for editable scenes, a map, an encounter, and example people; choose Blank workspace to start from scratch.",
                "Wähle Geführte Startkampagne für bearbeitbare Szenen, eine Karte, eine Begegnung und Beispielpersonen; wähle Leerer Arbeitsbereich für einen eigenen Anfang."));
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
        }

        if (selectedCampaign && resumeState.available)
        {
            const LauncherButtonPairResult continueActions = drawLauncherButtonPair(
                gm::localization::tr(gm::localization::TextId::LauncherResume),
                UiButtonRole::Primary,
                "Load the selected campaign to Campaign Home with its saved live or paused state.",
                gm::localization::tr(gm::localization::TextId::LauncherOpenCampaign),
                UiButtonRole::Secondary,
                "Open the selected campaign at Home.");
            if (continueActions.first)
                _controller.handleResumeLastSession(*selectedCampaign);
            if (continueActions.second)
                _controller.handleLoadGame(*selectedCampaign);
        }
        else if (selectedCampaign)
        {
            const float primaryWidth = std::min(320.0f, ImGui::GetContentRegionAvail().x);
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LauncherOpenCampaign),
                             UiButtonRole::Primary,
                             ImVec2(primaryWidth, 40.0f),
                             "Open the selected campaign at Home."))
            {
                _controller.handleLoadGame(*selectedCampaign);
            }
        }
        else
        {
            const float primaryWidth = std::min(320.0f, ImGui::GetContentRegionAvail().x);
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LauncherNewCampaign),
                             UiButtonRole::Primary,
                             ImVec2(primaryWidth, 40.0f),
                             "Create a new campaign workspace."))
            {
                _showNewGamePopup = true;
                _requestOpenNewGamePopup = true;
                resetNewCampaignDraft(_campaignName,
                                      _campaignDescription,
                                      _newGameError,
                                      _selectedRuleIndex,
                                      _selectedCampaignTemplateIndex,
                                      _newCampaignWizardStep);
            }
        }

        if (selectedCampaign)
        {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
            if (drawUiButton(launcherText("Export selected campaign", "Ausgewählte Kampagne exportieren"),
                             UiButtonRole::Secondary,
                             ImVec2(ImGui::GetContentRegionAvail().x, 36.0f),
                             launcherText("Create a portable package containing campaign data and assets.",
                                          "Ein übertragbares Paket mit Kampagnendaten und Medien erstellen.")))
                openExportPackagePopup(*selectedCampaign);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::SeparatorText(launcherText("Campaign library", "Kampagnenbibliothek"));
        UiGuidance::drawDetails(UiGuidance::Topic::DataSafety, launcherText("Save, backup or export?", "Speichern, sichern oder exportieren?"));
        if (selectedCampaign)
        {
            const LauncherButtonPairResult libraryActions = drawLauncherButtonPair(
                gm::localization::tr(gm::localization::TextId::LauncherNewCampaign),
                UiButtonRole::Secondary,
                "Create a new campaign workspace.",
                launcherText("Import package", "Paket importieren"),
                UiButtonRole::Secondary,
                launcherText("Review and restore a .ttrpgpkg campaign package.",
                             "Ein .ttrpgpkg-Kampagnenpaket prüfen und wiederherstellen."));
            if (libraryActions.first)
            {
                _showNewGamePopup = true;
                _requestOpenNewGamePopup = true;
                resetNewCampaignDraft(_campaignName,
                                      _campaignDescription,
                                      _newGameError,
                                      _selectedRuleIndex,
                                      _selectedCampaignTemplateIndex,
                                      _newCampaignWizardStep);
            }
            if (libraryActions.second)
                openImportPackagePopup();
        }
        else
        {
            const float secondaryWidth = std::min(320.0f, ImGui::GetContentRegionAvail().x);
            if (drawUiButton(launcherText("Import Campaign Package", "Kampagnenpaket importieren"),
                             UiButtonRole::Secondary,
                             ImVec2(secondaryWidth, 36.0f),
                             launcherText("Review and restore a .ttrpgpkg campaign package.",
                                          "Ein .ttrpgpkg-Kampagnenpaket prüfen und wiederherstellen.")))
            {
                openImportPackagePopup();
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LauncherApplication));
        const LauncherButtonPairResult applicationActions = drawLauncherButtonPair(
            gm::localization::tr(gm::localization::TextId::RulesetStudio),
            UiButtonRole::Secondary,
            "Design, validate, test, version, and publish a reusable no-code ruleset.",
            gm::localization::tr(gm::localization::TextId::AppSettings),
            UiButtonRole::Secondary,
            "Autosave, backup, recovery, and language settings.");
        if (applicationActions.first)
        {
            _showAddRulesetPopup = true;
            _requestOpenAddRulesetPopup = true;
            _rulesetStudioEditing = false;
        }
        if (applicationActions.second)
            openSettingsPopup();

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::CommonExit),
                         UiButtonRole::Secondary,
                         ImVec2(112.0f, 32.0f),
                         "Close the application."))
        {
            glfwSetWindowShouldClose(window.get(), true);
        }

        if (!_statusMessage.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, 10.0f));
            ImGui::Separator();
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _statusMessage.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();

        ImGui::EndGroup();

        handleNewGamePopup();
        handleAddRulesetPopup();
        handleImportPackagePopup();
        handleExportPackagePopup();
        handleSettingsPopup();
        handleBackupBrowserPopup();
    }
    // Always call End, even if Begin returned false
    ImGui::End();
}

void MainMenuWindowView::handleAddRulesetPopup()
{
    if (_requestOpenAddRulesetPopup)
    {
        ImGui::OpenPopup(localizedUiLabel("Ruleset Studio").c_str());
        _requestOpenAddRulesetPopup = false;
    }

    setResponsiveWindowSize(ImVec2(1080.0f, 720.0f), ImVec2(640.0f, 480.0f));
    if (ImGui::BeginPopupModal(localizedUiLabel("Ruleset Studio").c_str(), nullptr, ImGuiWindowFlags_NoCollapse))
    {
        if (!_rulesetStudioEditing)
        {
            drawRulesetStudioLibrary();
            ImGui::EndPopup();
            return;
        }

        const int stepIndex = rulesetLabStepIndex(_rulesetLabStep);
        const std::string stepMode = std::string(rulesetStudioText("Step ", "Schritt ")) +
                                     std::to_string(stepIndex + 1) +
                                     rulesetStudioText(" of ", " von ") +
                                     std::to_string(kRulesetLabStepCount);
        const std::string trimmedName = trimCopy(_rulesetName);
        const std::string selection = trimmedName.empty()
                                          ? rulesetStudioText("Ruleset draft", "Regelwerksentwurf")
                                          : trimmedName;
        const RulesetStudioReviewState review = currentRulesetStudioReviewState();

        drawStatusActionStrip(stepMode.c_str(),
                              selection.c_str(),
                              rulesetLabNextAction(_rulesetLabStep),
                              rulesetStudioText(
                                  "Ruleset Studio produces executable definitions for sheets, checks, derived values, resources, and conditions.",
                                  "Das Regelwerk-Studio erstellt ausführbare Definitionen für Bögen, Proben, abgeleitete Werte, Ressourcen und Zustände."));
        drawRulesetLabProgress(_rulesetLabStep);
        ImGui::Separator();

        drawRulesetStudioStep(review);
        ImGui::Separator();
        const bool closeRulesetStudio = drawRulesetStudioActions(review);
        if (closeRulesetStudio)
        {
            _showAddRulesetPopup = false;
            _rulesetStudioEditing = false;
            _pendingRulesetExitIntent = RulesetStudioExitIntent::None;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
