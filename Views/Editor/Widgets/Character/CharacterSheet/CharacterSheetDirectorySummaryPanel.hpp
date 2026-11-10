    Campaign &_campaign = _controller._currentCampaign;
    if (_controller.hasUnsavedDraft() &&
        !gm::ux::resolveNavigationEntity(_campaign, _controller._notesDraftCharacterId).available)
    {
        if(_campaign.presentationModeActive) {
            ImGui::TextWrapped("%s",uiTextForLocale("Private notes are retained. Return to GM mode to review them.",
                "Private Notizen bleiben erhalten. Prüfe sie im SL-Modus."));
            return;
        }
        ImGui::TextWrapped("%s", uiTextForLocale("This character is unavailable. Your notes draft is retained.",
            "Diese Figur ist nicht mehr verfügbar. Dein Notizentwurf bleibt erhalten."));
        if (ImGui::Button(uiTextForLocale("Copy retained notes", "Erhaltene Notizen kopieren")))
        {
            const std::string text = _controller._backstoryDraft + "\n\n" + _controller._playerNotesDraft + "\n\n" + _controller._gmNotesDraft;
            ImGui::SetClipboardText(text.c_str());
        }
        if (ImGui::Button(uiTextForLocale("Discard unavailable notes draft", "Nicht mehr zugeordneten Notizentwurf verwerfen")))
            _controller.revertSelectedCharacterNotesDraft();
        return;
    }
    if (_campaign.characters.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No players exist in this campaign yet.").c_str());
        return;
    }

    if (_controller._selectedCharacterIndex < 0 ||
        _controller._selectedCharacterIndex >= static_cast<int>(_campaign.characters.size()))
        _controller._selectedCharacterIndex = 0;
    _controller.ensureNotesDraftForSelectedCharacter();

    const bool directoryWide = TaskUi::hasRoomForColumns(66.0f);
    if (!ImGui::BeginTable("CharacterSheetLayout", directoryWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Character Directory").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.32f);
    if (directoryWide) ImGui::TableSetupColumn(localizedUiLabel("Character Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.68f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    const auto characterLabel = [&](const CharacterModel &listedCharacter) {
        std::string label = listedCharacter.nameBuffer[0] == '\0' ? localizedUiLabel("<Unnamed Player>")
                                                                    : std::string(listedCharacter.nameBuffer);
        if (_controller.isCharacterArchived(listedCharacter))
            label += " [" + localizedUiLabel("Archived") + "]";
        return label;
    };
    const auto selectCharacter = [&](int index) {
        if (_controller._selectedCharacterIndex == index)
            return;
        if (_controller.selectedCharacterNotesDraftDirty())
        {
            _controller._statusMessage = localizedUiLabel("Save or revert draft notes before switching characters.");
            return;
        }
        _controller._selectedCharacterIndex = index;
        _controller._pendingCharacterRemovalIndex = -1;
        _controller.ensureNotesDraftForSelectedCharacter();
    };
    // Each open and begin call shares a stable hidden ID. The localized title
    // remains visible while the ID prevents collisions with other editor modals.
    const std::string exportCharacterPopupId = localizedUiLabel("Export Character Review") + "###CharacterSheetExport";
    const std::string importCharacterPopupId = localizedUiLabel("Import Character Review") + "###CharacterSheetImport";
    const std::string removalCharacterPopupId = localizedUiLabel("Confirm Character Removal") + "###CharacterSheetRemoval";

    if (directoryWide)
    {
        drawHelpText("Player Directory", "Click a player character to inspect their story links, equipment, inventory, and backstory.");
        if (ImGui::BeginTable("PlayerDirectoryTable", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                              ImVec2(-1.0f, TaskUi::listHeight(0.75f))))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn(localizedUiLabel("Class").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.32f);
            ImGui::TableSetupColumn(localizedUiLabel("Level").c_str(), ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableHeadersRow();
            for (int i = 0; i < static_cast<int>(_campaign.characters.size()); ++i)
            {
                const CharacterModel &listedCharacter = _campaign.characters[i];
                const bool selected = _controller._selectedCharacterIndex == i;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const std::string displayName = characterLabel(listedCharacter);
                if (ImGui::Selectable(displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                    selectCharacter(i);
                showHelpTooltip(displayName.c_str());
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(localizedUiLabel(listedCharacter.selectedClass.c_str()));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text(localizedUiLabel("%d").c_str(), listedCharacter.level);
            }
            ImGui::EndTable();
        }
    }
    else
    {
        // The Cast workspace already provides a directory. In a narrow or
        // embedded sheet, a picker keeps the selected character's summary in
        // view instead of placing another full directory above it.
        const CharacterModel &selectedCharacter = _campaign.characters[_controller._selectedCharacterIndex];
        const std::string selectedLabel = characterLabel(selectedCharacter) + " · " +
                                          (selectedCharacter.selectedClass.empty() ? localizedUiLabel("Unclassed")
                                                                                   : localizedUiLabel(selectedCharacter.selectedClass.c_str())) +
                                          " · " + localizedUiLabel("Level") + " " + std::to_string(selectedCharacter.level);
        ImGui::TextUnformatted(localizedUiLabel("Character").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo("##CharacterSheetPicker", selectedLabel.c_str()))
        {
            for (int i = 0; i < static_cast<int>(_campaign.characters.size()); ++i)
            {
                const CharacterModel &listedCharacter = _campaign.characters[i];
                const bool selected = _controller._selectedCharacterIndex == i;
                const std::string optionLabel = characterLabel(listedCharacter) + " · " +
                                                (listedCharacter.selectedClass.empty() ? localizedUiLabel("Unclassed")
                                                                                       : localizedUiLabel(listedCharacter.selectedClass.c_str())) +
                                                " · " + localizedUiLabel("Level") + " " + std::to_string(listedCharacter.level);
                if (ImGui::Selectable(optionLabel.c_str(), selected))
                    selectCharacter(i);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        showHelpTooltip(uiTextForLocale("Choose the player character whose sheet you want to inspect or edit.",
                                        "Wähle die Spielfigur, deren Bogen du ansehen oder bearbeiten möchtest."));
    }

    if (ImGui::CollapsingHeader(uiTextForLocale("Character tools", "Figurenwerkzeuge")))
    {
        if (ImGui::Button(localizedUiLabel("Duplicate Selected").c_str()))
            _controller.duplicateSelectedCharacter();
        showHelpTooltip("Create a new character from the selected sheet, including abilities, inventory, portrait, notes, and spellcasting.");
        if (ImGui::Button(localizedUiLabel("Export Selected").c_str()))
            ImGui::OpenPopup(exportCharacterPopupId.c_str());
        showHelpTooltip("Write this character to a .ttrpg-character exchange file.");
        if (ImGui::Button(localizedUiLabel("Import Character").c_str()))
            ImGui::OpenPopup(importCharacterPopupId.c_str());
        showHelpTooltip("Import a .ttrpg-character file as a new campaign character.");
        if (ImGui::Button(localizedUiLabel("Archive Selected").c_str()))
        {
            _controller.requestArchiveSelectedCharacter();
            ImGui::OpenPopup(removalCharacterPopupId.c_str());
        }
        showHelpTooltip("Mark the selected character as archived without removing references.");
        if (ImGui::Button(localizedUiLabel("Delete Selected").c_str()))
        {
            _controller.requestDeleteSelectedCharacter();
            ImGui::OpenPopup(removalCharacterPopupId.c_str());
        }
        showHelpTooltip("Remove the selected character and clean direct story/combat references after confirmation.");
    }

    setResponsiveAutoSizeWindow(560.0f, 360.0f);
    if (ImGui::BeginPopupModal(exportCharacterPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const bool hasSelection =
            _controller._selectedCharacterIndex >= 0 &&
            _controller._selectedCharacterIndex < static_cast<int>(_campaign.characters.size());
        ImGui::TextWrapped(localizedUiLabel("Export the selected character to a .ttrpg-character exchange file.").c_str());
        drawInlineLabelValue("Character", hasSelection ? _campaign.characters[static_cast<std::size_t>(_controller._selectedCharacterIndex)].nameBuffer : localizedUiText("No character selected"));
        if (drawModalActionButtons("Export", UiButtonRole::Primary))
            _controller.exportSelectedCharacter();
        ImGui::EndPopup();
    }

    setResponsiveAutoSizeWindow(560.0f, 360.0f);
    if (ImGui::BeginPopupModal(importCharacterPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("Import a .ttrpg-character file as a new campaign character.").c_str());
        drawInlineLabelValue("Campaign", _campaign.name.empty() ? localizedUiText("Current campaign") : _campaign.name.c_str());
        drawInlineLabelValue("Result", localizedUiText("Adds a new character sheet"));
        if (drawModalActionButtons("Import", UiButtonRole::Advanced))
            _controller.importCharacterFromFile();
        ImGui::EndPopup();
    }

    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(removalCharacterPopupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const int pendingIndex = _controller._pendingCharacterRemovalIndex;
        if (pendingIndex < 0 || pendingIndex >= static_cast<int>(_campaign.characters.size()))
        {
            ImGui::TextUnformatted(localizedUiLabel("No character is selected.").c_str());
        }
        else
        {
            const CharacterModel &pendingCharacter = _campaign.characters[pendingIndex];
            ImGui::Text(localizedUiLabel("%s %s?").c_str(), localizedUiText(_controller._pendingCharacterRemovalIsArchive ? "Archive" : "Delete"), pendingCharacter.nameBuffer);
            const std::vector<std::string> warnings = _controller.getCharacterReferenceWarnings(pendingIndex);
            if (warnings.empty())
            {
                ImGui::TextWrapped(localizedUiLabel("No direct story or combat references were found.").c_str());
            }
            else
            {
                ImGui::TextUnformatted(localizedUiLabel("Reference warnings:").c_str());
                for (const std::string &warning : warnings)
                {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::TextWrapped("%s", warning.c_str());
                }
            }
            if (!_controller._pendingCharacterRemovalIsArchive)
                ImGui::TextWrapped(localizedUiLabel("Deleting removes direct story links and encounter participants for this character.").c_str());
        }
        const UiModalAction action = drawModalActionButtonsEx(
            _controller._pendingCharacterRemovalIsArchive ? "Archive" : "Delete",
            _controller._pendingCharacterRemovalIsArchive ? UiButtonRole::Advanced : UiButtonRole::Destructive,
            "Cancel",
            110.0f);
        if (action == UiModalAction::Confirm)
        {
            if (_controller._pendingCharacterRemovalIsArchive)
                _controller.archivePendingCharacter();
            else
                _controller.deletePendingCharacter();
        }
        else if (action == UiModalAction::Cancel)
        {
            _controller._pendingCharacterRemovalIndex = -1;
        }
        ImGui::EndPopup();
    }

    if (_campaign.characters.empty())
    {
        ImGui::TableNextColumn();
        ImGui::TextWrapped(localizedUiLabel("No players exist in this campaign yet.").c_str());
        ImGui::EndTable();
        return;
    }

    CharacterModel &character = _campaign.characters[_controller._selectedCharacterIndex];
    normalizeCharacterHitPoints(character);
    std::vector<ItemModel> items;
    if (_campaign.ruleSet)
        items = _campaign.ruleSet->getItems();
    items.insert(items.end(), _campaign.customItems.begin(), _campaign.customItems.end());

    ImGui::TableNextColumn();
    const RuleArmorSummary armor = _campaign.ruleSet ? CharacterRules::evaluateArmor(*_campaign.ruleSet, character, items) : RuleArmorSummary{};
    const int passivePerception = _campaign.ruleSet ? _campaign.ruleSet->getPassiveScore(character.attributes, "Perception", character.level) : 10;
    const std::string selectedName = character.nameBuffer[0] == '\0' ? localizedUiLabel("<Unnamed Player>") : std::string(character.nameBuffer);
    const std::string summarySelection = selectedName + " – " + localizedUiLabel("Level") + " " + std::to_string(character.level) + " " +
                                         (character.selectedClass.empty() ? localizedUiLabel("Unclassed") : localizedUiLabel(character.selectedClass.c_str()));
    if (directoryWide)
    {
        UiStatusActionStrip detailStrip;
        detailStrip.mode = "Character Detail";
        detailStrip.selection = summarySelection.c_str();
        detailStrip.nextAction = "Start with Summary; use focused tabs for editing, play, magic, inventory, story, and notes.";
        detailStrip.help = "Small fields save immediately. Backstory and notes use an explicit draft so large edits are deliberate.";
        drawStatusActionStrip("##CharacterSheetDetailStatus", detailStrip);
    }

    if (ImGui::BeginTabBar("SelectedCharacterDetailSections"))
    {
        if (ImGui::BeginTabItem(localizedUiLabel("Summary").c_str()))
        {
            ImGui::SeparatorText(localizedUiLabel("Live Table Summary").c_str());
            if (ImGui::BeginTable("CharacterLiveSummary", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableNextColumn();
                ImGui::TextDisabled(localizedUiLabel("Health").c_str());
                ImGui::Text(localizedUiLabel("%d / %d").c_str(), character.currentHitPoints, character.maxHitPoints);
                if (character.temporaryHitPoints > 0)
                    ImGui::Text(uiTextForLocale("+%d temporary health", "+%d temporäre Trefferpunkte"), character.temporaryHitPoints);
                ImGui::TextDisabled(localizedUiLabel("AC").c_str());
                ImGui::Text(localizedUiLabel("%d").c_str(), armor.armorClass);
                ImGui::TableNextColumn();
                ImGui::TextDisabled(localizedUiLabel("Passives / Saves").c_str());
                tableTextWrapped(characterPassivesAndSavesLabel(character, passivePerception));
                ImGui::TextDisabled(localizedUiLabel("Speed").c_str());
                ImGui::Text(localizedUiLabel("%d ft").c_str(), character.speedFeet);
                ImGui::TableNextColumn();
                ImGui::TextDisabled(localizedUiLabel("State / Magic").c_str());
                tableTextWrapped(characterConditionsLabel(character));
                tableTextWrapped(characterMagicLoadoutSummary(_campaign, character));
                ImGui::EndTable();
            }

            ImGui::SeparatorText(localizedUiLabel("Readiness").c_str());
            if (ImGui::BeginTable("CharacterReadinessSummary", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Resources").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
                ImGui::TableSetupColumn(localizedUiLabel("Equipped / Combat Gear").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.46f);
                ImGui::TableSetupColumn(localizedUiLabel("Key Notes").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.3f);
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                tableTextWrapped(characterResourceSummary(character));
                ImGui::TableNextColumn();
                tableTextWrapped(characterEquippedGearSummary(character, items));
                ImGui::TableNextColumn();
                tableTextWrapped(characterKeyNotesSummary(character));
                ImGui::EndTable();
            }

            if (!armor.sources.empty() || !armor.warnings.empty())
            {
                ImGui::SeparatorText(localizedUiLabel("Defense Notes").c_str());
                if (!armor.sources.empty())
                    tableTextWrapped(localizedUiLabel("AC sources") + ": " + joinLimitedList(armor.sources, "-", 5));
                for (const std::string &warning : armor.warnings)
                    ImGui::BulletText("%s", warning.c_str());
            }

            const std::vector<std::string> validationWarnings = _controller.getSelectedCharacterValidationWarnings();
            if (!validationWarnings.empty())
            {
                ImGui::SeparatorText(localizedUiLabel("Validation Warnings").c_str());
                for (const std::string &warning : validationWarnings)
                    ImGui::BulletText("%s", warning.c_str());
            }
            ImGui::EndTabItem();
        }
