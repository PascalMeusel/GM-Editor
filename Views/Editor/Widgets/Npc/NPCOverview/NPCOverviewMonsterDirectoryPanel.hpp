    Campaign &campaign = _controller._currentCampaign;
    if (ImGui::Button(localizedUiLabel("Import Ruleset Monster Catalog").c_str()))
        _controller.importRulesetMonsterCatalog();
    showHelpTooltip("Import monsters exposed by the active ruleset into this campaign, deduped by their catalog IDs.");
    if (campaign.ruleSet)
    {
        const std::vector<MonsterDefinitionModel> catalog = campaign.ruleSet->getMonsters();
        if (!catalog.empty())
        {
            if (_controller._selectedCatalogMonsterVariantIndex >= static_cast<int>(catalog.size()))
                _controller._selectedCatalogMonsterVariantIndex = 0;
            const MonsterDefinitionModel &preview = catalog[static_cast<std::size_t>(_controller._selectedCatalogMonsterVariantIndex)];
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220.0f);
            const std::string variantPreview = preview.name.empty() ? localizedUiLabel("<Unnamed Monster>") : preview.name;
            if (FormUi::BeginCombo(localizedUiLabel("Variant Base").c_str(), variantPreview.c_str()))
            {
                for (int i = 0; i < static_cast<int>(catalog.size()); ++i)
                {
                    const MonsterDefinitionModel &monster = catalog[static_cast<std::size_t>(i)];
                    const bool selected = _controller._selectedCatalogMonsterVariantIndex == i;
                    const std::string monsterName = monster.name.empty() ? localizedUiLabel("<Unnamed Monster>") : monster.name;
                    if (ImGui::Selectable(monsterName.c_str(), selected))
                        _controller._selectedCatalogMonsterVariantIndex = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button(localizedUiLabel("Create Variant").c_str()))
                _controller.createVariantFromSelectedCatalogMonster();
            showHelpTooltip("Create an editable campaign monster that remembers the catalog monster it started from.");
        }
    }

    if (campaign.customMonsters.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No custom monster stat blocks exist yet.").c_str());
        return;
    }

    if (_controller._selectedMonsterIndex >= static_cast<int>(campaign.customMonsters.size()))
        _controller._selectedMonsterIndex = 0;

    const bool monsterWide = TaskUi::hasRoomForColumns(66.0f);
    if (!ImGui::BeginTable("MonsterOverviewLayout", monsterWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Directory").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.46f);
    if (monsterWide) ImGui::TableSetupColumn(localizedUiLabel("Selected Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.54f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    drawHelpText("Monster Stat Blocks", "Reusable monster definitions that can be added to prepared battlefields.");
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::InputTextWithHint(localizedUiLabel("##MonsterDirectorySearch").c_str(), localizedUiText("Search name, type, role tags, actions, story/combat links"), &_controller._monsterDirectorySearch);
    showHelpTooltip("Filters reusable monster stat blocks without changing campaign data.");
    FormUi::Checkbox(localizedUiLabel("Show archived##MonsterDirectory").c_str(), &_controller._monsterDirectoryShowArchived);
    showHelpTooltip("Archived monsters stay available for reference unless this is off.");
    if (!_controller._monsterDirectorySearch.empty())
    {
        ImGui::SameLine();
        if (drawSmallUiButton("Clear##MonsterDirectorySearch", UiButtonRole::Secondary, "Clear the monster directory search."))
            _controller._monsterDirectorySearch.clear();
    }

    std::vector<int> filteredMonsterIndexes;
    filteredMonsterIndexes.reserve(campaign.customMonsters.size());
    for (int i = 0; i < static_cast<int>(campaign.customMonsters.size()); ++i)
    {
        const MonsterDefinitionModel &monster = campaign.customMonsters[static_cast<std::size_t>(i)];
        if (monsterMatchesDirectoryFilters(campaign, monster, _controller._monsterDirectorySearch, _controller._monsterDirectoryShowArchived, _controller.isMonsterArchived(monster)))
            filteredMonsterIndexes.push_back(i);
    }

    const bool selectedMonsterIsVisible = std::find(filteredMonsterIndexes.begin(), filteredMonsterIndexes.end(), _controller._selectedMonsterIndex) != filteredMonsterIndexes.end();
    if (!filteredMonsterIndexes.empty() && !selectedMonsterIsVisible)
        _controller._selectedMonsterIndex = filteredMonsterIndexes.front();

    ImGui::TextDisabled(localizedUiLabel("%d of %d monsters shown").c_str(), static_cast<int>(filteredMonsterIndexes.size()), static_cast<int>(campaign.customMonsters.size()));
    if (filteredMonsterIndexes.empty())
        ImGui::TextDisabled(localizedUiLabel("No monsters match the current search and filters. Clear a filter or search a broader term.").c_str());

    if (ImGui::BeginTable("MonsterOverviewTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(monsterWide ? 0.7f : 0.3f))))
    {
        ImGui::TableSetupColumn(localizedUiText("Name"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn(localizedUiText("Profile"));
        ImGui::TableHeadersRow();

        for (int i : filteredMonsterIndexes)
        {
            const MonsterDefinitionModel &monster = campaign.customMonsters[i];
            std::string displayName = monster.name.empty() ? localizedUiLabel("<Unnamed Monster>") : monster.name;
            if (_controller.isMonsterArchived(monster))
                displayName += " [" + localizedUiLabel("Archived") + "]";
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(displayName.c_str(), _controller._selectedMonsterIndex == i, ImGuiSelectableFlags_SpanAllColumns))
            {
                _controller._selectedMonsterIndex = i;
                _controller._selectedMonsterActionIndex = campaign.customMonsters[static_cast<std::size_t>(i)].actions.empty() ? -1 : 0;
            }
            showHelpTooltip(displayName.c_str());
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped((monster.type.empty() ? std::string("Creature") : monster.type) + " | CR " + (monster.challengeRating.empty() ? std::string("?") : monster.challengeRating));
            }
        ImGui::EndTable();
    }
    if (beginAdvancedHeader("Monster Maintenance / Advanced", "Duplicate, archive, or delete selected monster stat blocks. Destructive actions still require review."))
    {
        if (ImGui::Button(localizedUiLabel("Duplicate Selected Monster").c_str()))
            _controller.duplicateSelectedMonster();
        showHelpTooltip("Create a new custom monster from the selected stat block, including abilities and actions.");
        if (ImGui::Button(localizedUiLabel("Archive Selected Monster").c_str()))
        {
            _controller.requestArchiveSelectedMonster();
            ImGui::OpenPopup(localizedUiLabel("Confirm Monster Removal").c_str());
        }
        showHelpTooltip("Mark the selected monster as archived without removing references.");
        if (ImGui::Button(localizedUiLabel("Delete Selected Monster").c_str()))
        {
            _controller.requestDeleteSelectedMonster();
            ImGui::OpenPopup(localizedUiLabel("Confirm Monster Removal").c_str());
        }
        showHelpTooltip("Remove the selected monster and clean direct story/combat references after confirmation.");
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("Confirm Monster Removal").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const int pendingIndex = _controller._pendingMonsterRemovalIndex;
        if (pendingIndex < 0 || pendingIndex >= static_cast<int>(campaign.customMonsters.size()))
        {
            ImGui::TextUnformatted(localizedUiLabel("No monster is selected.").c_str());
        }
        else
        {
            const MonsterDefinitionModel &pendingMonster = campaign.customMonsters[pendingIndex];
            const std::string pendingAction = localizedUiLabel(_controller._pendingMonsterRemovalIsArchive ? "Archive" : "Delete");
            const std::string pendingName = pendingMonster.name.empty() ? localizedUiLabel("<Unnamed Monster>") : pendingMonster.name;
            ImGui::Text(localizedUiLabel("%s %s?").c_str(), pendingAction.c_str(), pendingName.c_str());
            const std::vector<std::string> warnings = _controller.getMonsterReferenceWarnings(pendingIndex);
            if (warnings.empty())
            {
                ImGui::TextWrapped(localizedUiLabel("No direct story or combat references were found.").c_str());
            }
            else
            {
                ImGui::TextUnformatted(localizedUiLabel("Reference warnings:").c_str());
                for (const std::string &warning : warnings)
                    ImGui::BulletText("%s", warning.c_str());
            }
            if (!_controller._pendingMonsterRemovalIsArchive)
                ImGui::TextWrapped(localizedUiLabel("Deleting removes direct story links and encounter participants for this monster.").c_str());
        }
        const UiModalAction action = drawModalActionButtonsEx(
            _controller._pendingMonsterRemovalIsArchive ? "Archive" : "Delete",
            _controller._pendingMonsterRemovalIsArchive ? UiButtonRole::Advanced : UiButtonRole::Destructive,
            "Cancel",
            110.0f);
        if (action == UiModalAction::Confirm)
        {
            if (_controller._pendingMonsterRemovalIsArchive)
                _controller.archivePendingMonster();
            else
                _controller.deletePendingMonster();
        }
        else if (action == UiModalAction::Cancel)
        {
            _controller._pendingMonsterRemovalIndex = -1;
        }
        ImGui::EndPopup();
    }

    if (campaign.customMonsters.empty())
    {
        ImGui::TableNextColumn();
        ImGui::TextWrapped(localizedUiLabel("No custom monster stat blocks exist yet.").c_str());
        ImGui::EndTable();
        return;
    }

    ImGui::TableNextColumn();
    _controller.clampSelectedMonsterActionIndex();
    MonsterDefinitionModel &selectedMonster = campaign.customMonsters[_controller._selectedMonsterIndex];
    PortraitUi::preview(selectedMonster.metadata);
    const std::string monsterName = selectedMonster.name.empty() ? std::string("<Unnamed Monster>") : selectedMonster.name;
    const std::string monsterSelection = monsterName + " - CR " + (selectedMonster.challengeRating.empty() ? std::string("?") : selectedMonster.challengeRating);
    UiStatusActionStrip monsterStrip;
    monsterStrip.mode = "Monster Detail";
    monsterStrip.selection = monsterSelection.c_str();
    monsterStrip.nextAction = "Scan the stat summary first; edit traits, defenses, and actions below.";
    monsterStrip.help = "Monster stat blocks save edits immediately. Use the summary for table reference before deeper stat editing.";
    drawStatusActionStrip("##SelectedMonsterStatus", monsterStrip);
