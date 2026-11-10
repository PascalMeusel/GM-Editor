        ImGui::SeparatorText(localizedUiLabel("Ability Scores").c_str());
        showHelpTooltip("Core stats used by initiative and ruleset helpers.");
        if (ImGui::BeginTable("SelectedMonsterAttributes", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Ability").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Score").c_str(), ImGuiTableColumnFlags_WidthFixed, 132.0f);
            for (const auto &attribute : selectedMonster.attributes)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(attribute.first);
                ImGui::TableSetColumnIndex(1);
                int score = attribute.second;
                ImGui::PushID(attribute.first.c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("##Score").c_str(), &score))
                    _controller.setSelectedMonsterAttribute(attribute.first, score);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText(localizedUiLabel("Combat Actions").c_str());
    MonsterActionModel &draft = _controller._monsterActionDraft;
    int pendingRemovedAction = -1;
    if (ImGui::BeginTable("SelectedMonsterActionEditor", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Action List").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableSetupColumn(localizedUiLabel("Action Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImGui::TextDisabled(localizedUiLabel("Actions").c_str());
        if (selectedMonster.actions.empty())
        {
            ImGui::TextWrapped(localizedUiLabel("No combat actions defined yet. Fill out the draft below, then add the first action.").c_str());
        }
        else if (ImGui::BeginTable("SelectedMonsterActionList", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Action").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.52f);
            ImGui::TableSetupColumn(localizedUiLabel("Range").c_str(), ImGuiTableColumnFlags_WidthFixed, 74.0f);
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 68.0f);
            ImGui::TableHeadersRow();
            for (int actionIndex = 0; actionIndex < static_cast<int>(selectedMonster.actions.size()); ++actionIndex)
            {
                const MonsterActionModel &action = selectedMonster.actions[static_cast<std::size_t>(actionIndex)];
                ImGui::PushID(actionIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const bool selected = _controller._selectedMonsterActionIndex == actionIndex;
                if (ImGui::Selectable(monsterActionDisplayName(action).c_str(), selected))
                    _controller._selectedMonsterActionIndex = actionIndex;
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(monsterActionRangeLabel(action).c_str());
                ImGui::TableSetColumnIndex(2);
                if (ImGui::SmallButton(localizedUiText("Remove")))
                    pendingRemovedAction = actionIndex;
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::TextDisabled(localizedUiLabel("Add Action").c_str());
        ImGui::PushID("ActionDraft");
        ImGui::BeginDisabled(_controller._actionDraftPendingSave);
        if (ImGui::BeginTable("SelectedMonsterActionDraft", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputText(localizedUiLabel("Action Name").c_str(), &draft.name);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputText(localizedUiLabel("Attack Bonus").c_str(), &draft.attackBonus);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputText(localizedUiLabel("Damage").c_str(), &draft.damage);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("Reach ft").c_str(), &draft.reachFeet);
            draft.reachFeet = std::max(0, draft.reachFeet);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("Range ft").c_str(), &draft.rangeFeet);
            draft.rangeFeet = std::max(draft.reachFeet, draft.rangeFeet);
            ImGui::EndTable();
        }
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputText(localizedUiLabel("Action Notes").c_str(), &draft.notes);
        if (ImGui::Button(localizedUiLabel("Add Action").c_str()))
            _controller.addActionToSelectedMonster();
        ImGui::EndDisabled();
        ImGui::PopID();

        ImGui::TableSetColumnIndex(1);
        ImGui::PushID("ActionDetail");

        const int selectedActionIndex = _controller._selectedMonsterActionIndex;
        const bool hasSelectedAction = selectedActionIndex >= 0 && selectedActionIndex < static_cast<int>(selectedMonster.actions.size());
        ImGui::TextDisabled(localizedUiLabel("Selected Action Detail").c_str());
        if (!hasSelectedAction)
        {
            ImGui::BeginDisabled();
            std::string emptyText = selectedMonster.actions.empty() ? "Add an action to enable editing." : "Select an action from the list to edit it.";
            FormUi::InputText(localizedUiLabel("Action Name").c_str(), &emptyText, ImGuiInputTextFlags_ReadOnly);
            int disabledNumber = 0;
            FormUi::InputInt(localizedUiLabel("Reach ft").c_str(), &disabledNumber);
            FormUi::InputInt(localizedUiLabel("Range ft").c_str(), &disabledNumber);
            FormUi::InputText(localizedUiLabel("Attack Bonus").c_str(), &emptyText, ImGuiInputTextFlags_ReadOnly);
            FormUi::InputText(localizedUiLabel("Damage").c_str(), &emptyText, ImGuiInputTextFlags_ReadOnly);
            FormUi::InputTextMultiline(localizedUiLabel("Action Notes").c_str(), &emptyText, ImVec2(-1.0f, 104.0f), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndDisabled();
            drawMutedHelpText("Action editor unavailable.", selectedMonster.actions.empty() ? "This monster has no actions yet." : "Choose an action in the list to edit its fields.");
        }
        else
        {
            const MonsterActionModel &action = selectedMonster.actions[static_cast<std::size_t>(selectedActionIndex)];
            std::string actionName = action.name;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputText(localizedUiLabel("Action Name").c_str(), &actionName))
                _controller.setSelectedMonsterActionName(selectedActionIndex, actionName);

            if (ImGui::BeginTable("SelectedMonsterActionNumbers", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                int reachFeet = action.reachFeet;
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("Reach ft").c_str(), &reachFeet))
                    _controller.setSelectedMonsterActionReachFeet(selectedActionIndex, reachFeet);
                std::string attackBonus = action.attackBonus;
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputText(localizedUiLabel("Attack Bonus").c_str(), &attackBonus))
                    _controller.setSelectedMonsterActionAttackBonus(selectedActionIndex, attackBonus);
                ImGui::TableSetColumnIndex(1);
                int rangeFeet = action.rangeFeet;
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("Range ft").c_str(), &rangeFeet))
                    _controller.setSelectedMonsterActionRangeFeet(selectedActionIndex, rangeFeet);
                std::string damage = action.damage;
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputText(localizedUiLabel("Damage").c_str(), &damage))
                    _controller.setSelectedMonsterActionDamage(selectedActionIndex, damage);
                ImGui::EndTable();
            }

            std::string notes = action.notes;
            ImGui::TextUnformatted(localizedUiLabel("Action Notes").c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedMonsterActionNotes").c_str(), &notes, ImVec2(-1.0f, 118.0f), ImGuiInputTextFlags_WordWrap))
                _controller.setSelectedMonsterActionNotes(selectedActionIndex, notes);

            if (ImGui::Button(localizedUiLabel("Remove Selected Action").c_str()))
                pendingRemovedAction = selectedActionIndex;
        }
        ImGui::PopID();
        ImGui::EndTable();
    }
    if (pendingRemovedAction >= 0)
        _controller.removeSelectedMonsterAction(pendingRemovedAction);

    const std::vector<StoryReference> storyReferences = findStoryReferences(campaign, selectedMonster.metadata.id, "Monster");
    ImGui::SeparatorText(localizedUiLabel("Connected Story Arcs").c_str());
    if (storyReferences.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No story arcs are linked to this monster yet.").c_str());
    }
    else if (ImGui::BeginTable("SelectedMonsterStories", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Story Arc").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Arc Kind").c_str(), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Scene or Note").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(storyReferences.size()); ++i)
        {
            const StoryReference &reference = storyReferences[i];
            if (reference.threadIndex < 0 || reference.threadIndex >= static_cast<int>(campaign.storyThreads.size()))
                continue;

            const StoryThreadModel &thread = campaign.storyThreads[reference.threadIndex];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(thread.title.empty() ? localizedUiLabel("<Untitled Story>") : thread.title);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(localizedUiText(storyThreadTypeLabel(thread.type)));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(reference.sceneTitle.empty() ? reference.note : reference.sceneTitle);
            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button(localizedUiLabel("Open Story").c_str()))
                _controller.openStoryThread(thread.metadata.id);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    const std::vector<EncounterReference> encounterReferences = findEncounterReferences(campaign, selectedMonster.metadata.id);
    ImGui::SeparatorText(localizedUiLabel("Combat Appearances").c_str());
    if (encounterReferences.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("This monster is not currently present in a prepared combat.").c_str());
    }
    else
    {
        for (const EncounterReference &reference : encounterReferences)
        {
            const std::string defeated = reference.defeated ? localizedUiLabel(" – defeated") : std::string{};
            ImGui::BulletText(localizedUiLabel("%s – round %d%s").c_str(), reference.name.c_str(), reference.round, defeated.c_str());
        }
    }

    ImGui::EndTable();
