#include "MainMenuCampaignSupport.hpp"

void MainMenuWindowView::handleNewGamePopup()
{
    if (_requestOpenNewGamePopup)
    {
        ImGui::OpenPopup(localizedUiLabel("New Campaign").c_str());
        _requestOpenNewGamePopup = false;
    }

    setResponsiveWindowSize(ImVec2(680.0f, 620.0f), ImVec2(400.0f, 360.0f));
    if (ImGui::BeginPopupModal(localizedUiLabel("New Campaign").c_str(), &_showNewGamePopup, ImGuiWindowFlags_None))
    {
        std::vector<std::string> rulesetLabels;
        std::vector<const char *> rulesetNames;
        rulesetLabels.reserve(ruleSets.size());
        rulesetNames.reserve(ruleSets.size());
        for (const auto &rulesetPtr : ruleSets)
            rulesetLabels.push_back(rulesetPtr->getName());
        for (const std::string &label : rulesetLabels)
            rulesetNames.push_back(label.c_str());
        if (!ruleSets.empty())
            _selectedRuleIndex = std::clamp(_selectedRuleIndex, 0, static_cast<int>(ruleSets.size()) - 1);
        else
            _selectedRuleIndex = 0;
        const std::string trimmedName = trimCopy(_campaignName);
        auto validateName = [&]() {
            if (!validateCampaignName(_campaignName, _newGameError))
                return false;
            if (_controller.campaignExists(trimCopy(_campaignName)))
            {
                _newGameError = launcherText("A campaign with that name already exists.", "Eine Kampagne mit diesem Namen existiert bereits.");
                return false;
            }
            return true;
        };

        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
            "Give the campaign a name, then choose whether to begin with a blank workspace or a ready-to-edit starter.",
            "Gib der Kampagne einen Namen und wähle dann zwischen einem leeren Arbeitsbereich und einem bearbeitbaren Starter."));
        // Campaign creation is intentionally one compact form. Ruleset authoring
        // remains a separate, multi-step draft and publication workflow.
        _newCampaignWizardStep=NewCampaignWizardStep::Name;
        ImGui::Separator();

        ImGui::BeginChild("CampaignSetupFields", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 3.0f));
        switch (_newCampaignWizardStep)
        {
        case NewCampaignWizardStep::Name:
            ImGui::SeparatorText(localizedUiLabel("Campaign Name").c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            FormUi::InputTextWithHint(localizedUiLabel("##CampaignName").c_str(), localizedUiText("Example: Lantern Fair"), &_campaignName);
            [[fallthrough]];
        case NewCampaignWizardStep::RulesetTemplate:
            ImGui::SeparatorText(localizedUiLabel("Ruleset").c_str());
            if (!rulesetNames.empty())
            {
                ImGui::SetNextItemWidth(-1.0f);
                FormUi::Combo(localizedUiLabel("##CampaignRuleset").c_str(), &_selectedRuleIndex, rulesetNames.data(), static_cast<int>(rulesetNames.size()));
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), localizedUiLabel("No rulesets are available.").c_str());
            }

            ImGui::SeparatorText(localizedUiLabel("Template").c_str());
            if (FormUi::RadioButton(localizedUiLabel("Blank campaign workspace").c_str(), _selectedCampaignTemplateIndex == 0))
                _selectedCampaignTemplateIndex = 0;
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", launcherText(
                "Start with an empty workspace. You can add or import content later.",
                "Beginne mit einem leeren Arbeitsbereich. Inhalte kannst du später hinzufügen oder importieren."));
            ImGui::PopStyleColor();
            if (FormUi::RadioButton(launcherText("Guided starter campaign", "Geführte Startkampagne"), _selectedCampaignTemplateIndex == 1))
                _selectedCampaignTemplateIndex = 1;
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), launcherText(
                "Recommended for a first campaign: adds three scenes, a map, an encounter, and example people and places. Everything stays editable.",
                "Für die erste Kampagne empfohlen: Fügt drei Szenen, eine Karte, eine Begegnung sowie Beispielpersonen und -orte hinzu. Alles bleibt bearbeitbar."));
            ImGui::PopStyleColor();
            [[fallthrough]];
        case NewCampaignWizardStep::ContentDescription:
            if (ImGui::CollapsingHeader(launcherText("Optional description", "Optionale Beschreibung")))
            {
                ImGui::SetNextItemWidth(-1.0f);
                FormUi::InputTextMultiline(localizedUiLabel("##CampaignDescription").c_str(), &_campaignDescription, ImVec2(-1.0f, 78.0f), ImGuiInputTextFlags_WordWrap);
            }
            [[fallthrough]];

        case NewCampaignWizardStep::Review:
        default:
        {
            ImGui::SeparatorText(launcherText("Ready to create", "Bereit zum Erstellen"));
            const std::string summary = launcherText("Creates ", "Erstellt ") +
                                        (trimmedName.empty() ? launcherText("your campaign", "deine Kampagne") : trimmedName) +
                                        launcherText(" with ", " mit ") +
                                        selectedRulesetName(ruleSets, _selectedRuleIndex) +
                                        launcherText(" and the ", " und der Vorlage ") +
                                        selectedTemplateName(_selectedCampaignTemplateIndex) + ".";
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), summary.c_str());
            break;
        }
        }

        ImGui::Separator();

        ImGui::EndChild();
        const float actionWidth = ImGui::GetContentRegionAvail().x;
        const float cancelWidth = std::min(110.0f, actionWidth * 0.32f);
        const float createWidth = std::max(150.0f, actionWidth - cancelWidth - ImGui::GetStyle().ItemSpacing.x);
        if (drawUiButton("Cancel", UiButtonRole::Secondary, ImVec2(cancelWidth, 36.0f)))
        {
            _showNewGamePopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        {
            if (drawUiButton("Create Campaign", UiButtonRole::Primary, ImVec2(createWidth, 36.0f)))
            {
                _newGameError.clear();
                if (validateName() &&
                    validateCampaignRuleset(ruleSets, _selectedRuleIndex, _newGameError))
                {
                    Campaign newCampaign;
                    newCampaign.name = trimCopy(_campaignName);
                    newCampaign.description = trimCopy(_campaignDescription);
                    newCampaign.contentLanguage =
                        gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                            ? "de"
                            : "en";
                    newCampaign.ruleSet = ruleSets[static_cast<std::size_t>(_selectedRuleIndex)];
                    if (_selectedCampaignTemplateIndex == 1)
                    {
                        StarterCampaignTemplate::apply(
                            newCampaign,
                            gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German
                                ? StarterCampaignTemplate::Language::German
                                : StarterCampaignTemplate::Language::English);
                    }
                    if (_controller.handleNewGame(newCampaign, _newGameError))
                    {
                        _statusMessage = std::string(launcherText("Campaign created: ", "Kampagne erstellt: ")) + newCampaign.name;
                        _showNewGamePopup = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }

        if (!_newGameError.empty())
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), localizedUiLabel("%s").c_str(), _newGameError.c_str());
        }

        ImGui::EndPopup();
    }
}
