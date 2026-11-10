    Campaign &campaign = _controller._currentCampaign;
    if (campaign.npcs.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No NPCs exist in this campaign yet.").c_str());
        return;
    }

    _controller._selectedNpcIndex = std::clamp(_controller._selectedNpcIndex, 0, static_cast<int>(campaign.npcs.size()) - 1);

    const bool compact = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 70.0f;
    if (compact)
    {
        ImGui::SetNextItemWidth(-1.0f);
        const auto &current = campaign.npcs[std::max(0, _controller._selectedNpcIndex)];
        if (FormUi::BeginCombo("##NpcQuickSwitch", current.name.c_str()))
        {
            FormUi::InputTextWithHint("##NpcQuickSearch", localizedUiText("Search"), &_quickSearch);
            for (int i = 0; i < static_cast<int>(campaign.npcs.size()); ++i)
            {
                const auto &npc = campaign.npcs[i];
                if (!_quickSearch.empty() && trimLowerCopy(npc.name).find(trimLowerCopy(_quickSearch)) == std::string::npos) continue;
                if (ImGui::Selectable((npc.name + "##" + npc.metadata.id).c_str(), _controller._selectedNpcIndex == i))
                    _controller._selectedNpcIndex = i;
            }
            ImGui::EndCombo();
        }
    }
    if (!compact)
    {
        if (!ImGui::BeginTable("NpcOverviewLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) return;
        ImGui::TableSetupColumn(localizedUiLabel("Directory").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn(localizedUiLabel("Selected Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.60f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
    drawHelpText("NPC Directory", "Search and filter people, creatures, faction agents, and powers before opening the detail panel.");
    if (ImGui::BeginTable("NpcDirectoryFilters", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Search").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.64f);
        ImGui::TableSetupColumn(localizedUiLabel("Kind").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputTextWithHint(localizedUiLabel("##NpcDirectorySearch").c_str(), (gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? "NSC suchen" : "Search NPCs"), &_controller._npcDirectorySearch);
        showHelpTooltip("Filters the NPC directory without changing campaign data.");

        ImGui::TableSetColumnIndex(1);
        std::string categoryFilterLabel = localizedUiLabel("All kinds");
        if (_controller._npcDirectoryCategoryFilter >= 0 && _controller._npcDirectoryCategoryFilter <= 3)
            categoryFilterLabel = localizedUiLabel(npcCategoryLabel(static_cast<NPCCategory>(_controller._npcDirectoryCategoryFilter)));
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo(localizedUiLabel("Kind##NpcDirectoryCategory").c_str(), categoryFilterLabel.c_str()))
        {
            if (ImGui::Selectable(localizedUiLabel("All kinds").c_str(), _controller._npcDirectoryCategoryFilter < 0))
                _controller._npcDirectoryCategoryFilter = -1;
            for (int categoryIndex = 0; categoryIndex <= 3; ++categoryIndex)
            {
                const bool selected = _controller._npcDirectoryCategoryFilter == categoryIndex;
                const std::string categoryLabel = localizedUiLabel(npcCategoryLabel(static_cast<NPCCategory>(categoryIndex)));
                if (ImGui::Selectable(categoryLabel.c_str(), selected))
                    _controller._npcDirectoryCategoryFilter = categoryIndex;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }

    const bool extraFiltersActive = _controller._npcDirectoryThreatFilter >= 0 ||
        _controller._npcDirectoryStoryLinkedOnly || _controller._npcDirectoryCombatLinkedOnly ||
        !_controller._npcDirectoryShowArchived || _controller._npcDirectoryPlayerVisibleOnly || _controller._npcDirectoryLiveRelevantOnly;
    const bool germanFilters = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    if (ImGui::CollapsingHeader(extraFiltersActive ? (germanFilters ? "Filter (aktiv)" : "Filters (active)") : (germanFilters ? "Filter" : "Filters")))
    {
    if (ImGui::BeginTable("NpcDirectoryReferenceFilters", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Threat").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableSetupColumn(localizedUiLabel("Refs").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.64f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1.0f);
        const std::string threatPreview = localizedUiLabel(npcThreatFilterLabel(_controller._npcDirectoryThreatFilter));
        if (FormUi::BeginCombo(localizedUiLabel("Threat##NpcDirectoryThreat").c_str(), threatPreview.c_str()))
        {
            if (ImGui::Selectable(localizedUiLabel("Any threat").c_str(), _controller._npcDirectoryThreatFilter < 0))
                _controller._npcDirectoryThreatFilter = -1;
            for (int threatFilter = 0; threatFilter <= 2; ++threatFilter)
            {
                const bool selected = _controller._npcDirectoryThreatFilter == threatFilter;
                if (ImGui::Selectable(localizedUiLabel(npcThreatFilterLabel(threatFilter)).c_str(), selected))
                    _controller._npcDirectoryThreatFilter = threatFilter;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        showHelpTooltip("Filter by the selected NPC threat range.");

        ImGui::TableSetColumnIndex(1);
        FormUi::Checkbox(localizedUiLabel("Story-linked").c_str(), &_controller._npcDirectoryStoryLinkedOnly);
        showHelpTooltip("Only show NPCs linked from story arcs or scenes.");
        FormUi::Checkbox(localizedUiLabel("Combat-linked").c_str(), &_controller._npcDirectoryCombatLinkedOnly);
        showHelpTooltip("Only show NPCs already present in a prepared encounter.");
        ImGui::EndTable();
    }

    FormUi::Checkbox(localizedUiLabel("Show archived").c_str(), &_controller._npcDirectoryShowArchived);
    showHelpTooltip("Archived NPCs stay available for reference unless this is off.");
    FormUi::Checkbox(localizedUiLabel("Player-visible only").c_str(), &_controller._npcDirectoryPlayerVisibleOnly);
    showHelpTooltip("Only show NPCs selected for the local player display.");
    FormUi::Checkbox(localizedUiLabel("Live-relevant only").c_str(), &_controller._npcDirectoryLiveRelevantOnly);
    showHelpTooltip("Only show NPCs with public visibility, combat appearances, conditions, resources, or magic.");

    }

    if (!_controller._npcDirectorySearch.empty())
    {
        ImGui::SameLine();
        if (drawSmallUiButton("Clear##NpcDirectorySearch", UiButtonRole::Secondary, "Clear the NPC directory search."))
            _controller._npcDirectorySearch.clear();
    }

    std::vector<int> filteredNpcIndexes;
    filteredNpcIndexes.reserve(campaign.npcs.size());
    for (int i = 0; i < static_cast<int>(campaign.npcs.size()); ++i)
    {
        const NPCModel &npc = campaign.npcs[static_cast<std::size_t>(i)];
        if (npcMatchesDirectoryFilters(campaign,
                                       npc,
                                       _controller._npcDirectorySearch,
                                       _controller._npcDirectoryCategoryFilter,
                                       _controller._npcDirectoryThreatFilter,
                                       _controller._npcDirectoryShowArchived,
                                       _controller._npcDirectoryPlayerVisibleOnly,
                                       _controller._npcDirectoryLiveRelevantOnly,
                                       _controller._npcDirectoryStoryLinkedOnly,
                                       _controller._npcDirectoryCombatLinkedOnly,
                                       _controller.isNpcArchived(npc)))
        {
            filteredNpcIndexes.push_back(i);
        }
    }

    const bool selectedNpcIsVisible = std::find(filteredNpcIndexes.begin(), filteredNpcIndexes.end(), _controller._selectedNpcIndex) != filteredNpcIndexes.end();
    if (!filteredNpcIndexes.empty() && !selectedNpcIsVisible)
        _controller._selectedNpcIndex = filteredNpcIndexes.front();

    ImGui::TextDisabled(localizedUiLabel("%d of %d NPCs shown").c_str(), static_cast<int>(filteredNpcIndexes.size()), static_cast<int>(campaign.npcs.size()));
    if (filteredNpcIndexes.empty())
        ImGui::TextDisabled(localizedUiLabel("No NPCs match the current search and filters. Clear a filter or search a broader term.").c_str());

    if (ImGui::BeginTable("NpcOverviewList", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, ImVec2(-1.0f, 500.0f)))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.48f);
        ImGui::TableSetupColumn(localizedUiLabel("Role").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.52f);
        ImGui::TableHeadersRow();
        for (int i : filteredNpcIndexes)
        {
            const NPCModel &npc = campaign.npcs[i];
            std::string displayName = npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name;
            if (_controller.isNpcArchived(npc)) displayName += " [" + localizedUiLabel("Archived") + "]";
            ImGui::PushID(npc.metadata.id.c_str());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(displayName.c_str(), _controller._selectedNpcIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                _controller._selectedNpcIndex = i;
            showHelpTooltip(displayName.c_str());
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(npc.role);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (beginAdvancedHeader("NPC Maintenance / Advanced", "Duplicate, archive, or delete selected NPC profiles. Destructive actions still require review."))
    {
        if (ImGui::Button(localizedUiLabel("Duplicate Selected NPC").c_str()))
            _controller.duplicateSelectedNpc();
        showHelpTooltip("Create a new NPC from the selected profile, including notes, abilities, inventory, and spellcasting.");
        if (ImGui::Button(localizedUiLabel("Archive Selected NPC").c_str()))
        {
            _controller.requestArchiveSelectedNpc();
            ImGui::OpenPopup(localizedUiLabel("Confirm NPC Removal").c_str());
        }
        showHelpTooltip("Mark the selected NPC as archived without removing references.");
        if (ImGui::Button(localizedUiLabel("Delete Selected NPC").c_str()))
        {
            _controller.requestDeleteSelectedNpc();
            ImGui::OpenPopup(localizedUiLabel("Confirm NPC Removal").c_str());
        }
        showHelpTooltip("Remove the selected NPC and clean direct story/combat references after confirmation.");
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("Confirm NPC Removal").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const int pendingIndex = _controller._pendingNpcRemovalIndex;
        if (pendingIndex < 0 || pendingIndex >= static_cast<int>(campaign.npcs.size()))
        {
            ImGui::TextUnformatted(localizedUiLabel("No NPC is selected.").c_str());
        }
        else
        {
            const NPCModel &pendingNpc = campaign.npcs[pendingIndex];
            const std::string pendingAction = localizedUiLabel(_controller._pendingNpcRemovalIsArchive ? "Archive" : "Delete");
            const std::string pendingName = pendingNpc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : pendingNpc.name;
            ImGui::Text(localizedUiLabel("%s %s?").c_str(), pendingAction.c_str(), pendingName.c_str());
            const std::vector<std::string> warnings = _controller.getNpcReferenceWarnings(pendingIndex);
            if (warnings.empty())
            {
                ImGui::TextWrapped(localizedUiLabel("No direct story or combat references were found.").c_str());
            }
            else
            {
                ImGui::TextUnformatted(localizedUiLabel("Reference warnings:").c_str());
                for (const std::string &warning : warnings)
                    ImGui::BulletText("%s", localizedUiLabel(warning.c_str()).c_str());
            }
            if (!_controller._pendingNpcRemovalIsArchive)
                ImGui::TextWrapped(localizedUiLabel("Deleting removes direct story links and encounter participants for this NPC.").c_str());
        }
        const UiModalAction action = drawModalActionButtonsEx(
            _controller._pendingNpcRemovalIsArchive ? "Archive" : "Delete",
            _controller._pendingNpcRemovalIsArchive ? UiButtonRole::Advanced : UiButtonRole::Destructive,
            "Cancel",
            110.0f);
        if (action == UiModalAction::Confirm)
        {
            if (_controller._pendingNpcRemovalIsArchive)
                _controller.archivePendingNpc();
            else
                _controller.deletePendingNpc();
        }
        else if (action == UiModalAction::Cancel)
        {
            _controller._pendingNpcRemovalIndex = -1;
        }
        ImGui::EndPopup();
    }

    if (campaign.npcs.empty())
    {
        ImGui::TableNextColumn();
        ImGui::TextWrapped(localizedUiLabel("No NPCs exist in this campaign yet.").c_str());
        ImGui::EndTable();
        return;
    }

    ImGui::TableNextColumn();
    }
