        if (ImGui::BeginTabItem(localizedUiLabel("Profile").c_str()))
        {
    bool openLevelUp = false;
    if (ImGui::BeginTable("CharacterHeader", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Portrait").c_str(), ImGuiTableColumnFlags_WidthFixed, 184.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Identity").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(localizedUiLabel("Character Picture").c_str());
        showHelpTooltip("Upload a portrait image for this character. The image is copied into this campaign's resources.");
        const std::filesystem::path portraitPath = _controller.resolvePortraitPath(character);
        const bool portraitPathExists = !portraitPath.empty() && std::filesystem::exists(portraitPath);
        if (!portraitPath.empty())
        {
            const CachedImageTexture &portrait = getCachedImageTexture(portraitPath);
            if (portrait.loaded && portrait.textureId)
            {
                const float maxSide = 168.0f;
                const float widthScale = maxSide / static_cast<float>(portrait.width);
                const float heightScale = maxSide / static_cast<float>(portrait.height);
                const float scale = std::min(widthScale, heightScale);
                ImGui::Image(portrait.textureId, ImVec2(static_cast<float>(portrait.width) * scale, static_cast<float>(portrait.height) * scale));
            }
            else
            {
                ImGui::TextWrapped(localizedUiLabel("Portrait file could not be loaded.").c_str());
            }
        }
        else if(gm::portraits::exists(character.metadata)) PortraitUi::preview(character.metadata);
        else
        {
            ImGui::BeginChild("PortraitPlaceholder", ImVec2(168.0f, 168.0f), true);
            ImGui::TextWrapped(localizedUiLabel("No portrait uploaded.").c_str());
            ImGui::EndChild();
        }
        const char *portraitModal = uiTextForLocale("Choose portrait###CharacterPortraitChoice", "Porträt wählen###CharacterPortraitChoice");
        if (ImGui::Button(uiTextForLocale("Choose portrait...", "Porträt wählen..."), ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup(portraitModal);
        const ImGuiViewport *portraitViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(ImVec2(std::min(720.0f, portraitViewport->WorkSize.x - 40.0f),
                                      std::min(520.0f, portraitViewport->WorkSize.y - 40.0f)), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(portraitViewport->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(portraitModal, nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::BeginChild("PortraitChoicesBody", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() - 8.0f));
            if (!character.portraitAssetId.empty())
                ImGui::TextWrapped("%s", uiTextForLocale(
                    "Your uploaded picture is currently in use. Remove it to show a collection portrait.",
                    "Dein hochgeladenes Bild wird verwendet. Entferne es, um ein Porträt aus der Sammlung anzuzeigen."));
            if (PortraitUi::editor(character.metadata)) _controller.saveCampaign();
            ImGui::TextWrapped("%s", uiTextForLocale("Portrait choices save immediately.", "Die Porträtwahl wird sofort gespeichert."));
            ImGui::EndChild();
            if (ImGui::Button(uiTextForLocale("Done", "Fertig"), ImVec2(-1.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::Button(localizedUiLabel("Upload Picture").c_str(), ImVec2(-1.0f, 0.0f)))
            _controller.browseForSelectedCharacterPortrait();
        showHelpTooltip("Choose a png, jpg, jpeg, bmp, or gif from your computer.");
        if (ImGui::Button(localizedUiLabel("Open Portrait Folder").c_str(), ImVec2(-1.0f, 0.0f)))
            _controller.openPortraitDirectory();
        showHelpTooltip("Open this campaign's portrait asset folder.");
        if (!character.portraitAssetId.empty())
        {
            ImGui::TextWrapped(localizedUiLabel("Asset: %s").c_str(), character.portraitAssetId.c_str());
            if (!portraitPathExists)
                ImGui::TextColored(ImVec4(0.95f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Portrait file is missing.").c_str());
            if (ImGui::Button(localizedUiLabel("Remove Picture").c_str(), ImVec2(-1.0f, 0.0f)))
                _controller.clearSelectedCharacterPortrait();
        }

        ImGui::TableSetColumnIndex(1);
        std::string characterName = character.nameBuffer;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Name").c_str(), &characterName))
            _controller.setSelectedCharacterName(characterName);
        showHelpTooltip("Editing saves immediately to the active campaign.");
        if (_controller.isCharacterArchived(character))
            ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.38f, 1.0f), localizedUiLabel("Archived").c_str());

        int level = character.level;
        ImGui::SetNextItemWidth(110.0f);
        if (FormUi::InputInt(localizedUiLabel("Level").c_str(), &level))
            _controller.setSelectedCharacterLevel(level);
        showHelpTooltip("The character's current level. Combat uses this for proficiency and player weapon attacks.");
        if (_campaign.ruleSet && _campaign.ruleSet->levelsExist())
        {
            ImGui::SameLine();
            if (ImGui::Button(localizedUiLabel("Level Up").c_str()))
            {
                _controller.requestLevelUpSelectedCharacter();
                openLevelUp = _controller._levelUpPending;
            }
            showHelpTooltip("Preview ruleset-derived proficiency and spell math before applying the next level.");
        }

        std::string selectedRace = character.selectedRace;
        ImGui::SetNextItemWidth(220.0f);
        renderCombo("Species", _campaign.ruleSet ? _campaign.ruleSet->getRaces() : std::vector<std::string>{}, selectedRace);
        if (selectedRace != character.selectedRace)
            _controller.setSelectedCharacterRace(selectedRace);

        if (character.classLevels.size() > 1) {
            for (const auto &[name, level] : character.classLevels)
                ImGui::Text("%s: %d", localizedUiLabel(name.c_str()).c_str(), level);
        }
        std::string selectedClass = character.selectedClass;
        ImGui::SetNextItemWidth(220.0f);
        renderCombo("Class", _campaign.ruleSet ? _campaign.ruleSet->getClasses() : std::vector<std::string>{}, selectedClass);
        if (selectedClass != character.selectedClass)
            _controller.setSelectedCharacterClass(selectedClass);

        std::string background = character.background;
        ImGui::SetNextItemWidth(260.0f);
        if (FormUi::InputText(localizedUiLabel("Background").c_str(), &background))
            _controller.setSelectedCharacterBackground(background);
        int speedFeet = character.speedFeet;
        ImGui::SetNextItemWidth(110.0f);
        if (FormUi::InputInt(localizedUiLabel("Speed (ft)").c_str(), &speedFeet))
            _controller.setSelectedCharacterSpeedFeet(speedFeet);

        if (!_controller._statusMessage.empty())
            ImGui::TextColored(ImVec4(0.48f, 0.78f, 0.54f, 1.0f), localizedUiLabel("%s").c_str(), localizedUiText(_controller._statusMessage.c_str()));
        if (!_controller._errorMessage.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.42f, 0.38f, 1.0f));
            ImGui::TextWrapped("%s", localizedUiText(_controller._errorMessage.c_str()));
            ImGui::PopStyleColor();
            if (drawUiButton(localizedUiText("Save"), UiButtonRole::Primary))
                _controller.saveCampaign();
        }
        ImGui::EndTable();
    }

    drawAdvancement(openLevelUp);

            ImGui::EndTabItem();
        }
