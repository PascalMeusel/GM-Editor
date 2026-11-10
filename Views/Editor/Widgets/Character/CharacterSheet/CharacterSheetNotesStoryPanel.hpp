        if (ImGui::BeginTabItem(localizedUiLabel("Notes").c_str()))
        {
    if(_controller._currentCampaign.presentationModeActive||!_controller.draftRecoveryReady())ImGui::TextWrapped("%s",uiTextForLocale(
        "Review Item & note drafts in the sidebar before editing. Existing recovery is retained.",
        "Prüfe zuerst Gegenstands- und Notizentwürfe in der Seitenleiste. Die Sicherung bleibt erhalten."));
    else {
    ImGui::SeparatorText(localizedUiLabel("Backstory").c_str());
    _controller.ensureNotesDraftForSelectedCharacter();
    const bool notesDirty = _controller.selectedCharacterNotesDraftDirty();
    UiStatusActionStrip notesStrip;
    notesStrip.mode = "Draft Notes";
    notesStrip.selection = notesDirty ? "Unsaved edits" : "Saved";
    notesStrip.nextAction = notesDirty ? "Review, then Save Notes or Revert Draft." : "Edit backstory or notes, then save this draft.";
    notesStrip.help = "Large character text fields use an explicit draft. Other small character fields still save immediately.";
    drawStatusActionStrip("##CharacterNotesDraftStatus", notesStrip);
    if (!_controller._statusMessage.empty())
        ImGui::TextColored(ImVec4(0.48f, 0.78f, 0.54f, 1.0f), localizedUiLabel("%s").c_str(), localizedUiText(_controller._statusMessage.c_str()));

    ImGui::SetNextItemWidth(-1.0f);
    if(FormUi::InputTextMultiline(localizedUiLabel("##CharacterBackstory").c_str(), &_controller._backstoryDraft, ImVec2(-1.0f, 120.0f)))_controller.notesDraftEdited();
    drawMutedHelpText("Draft: backstory edits are not saved until you use Save Notes.", "Switching characters is blocked while this draft has unsaved edits.");

    ImGui::SeparatorText(localizedUiLabel("Notes").c_str());
    if (ImGui::BeginTable("CharacterNotes", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Player Notes").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("GM Notes").c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(localizedUiLabel("Player-visible Notes").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if(FormUi::InputTextMultiline(localizedUiLabel("##PlayerVisibleNotes").c_str(), &_controller._playerNotesDraft, ImVec2(-1.0f, 110.0f)))_controller.notesDraftEdited();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(localizedUiLabel("Private GM Notes").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if(FormUi::InputTextMultiline(localizedUiLabel("##PrivateGmNotes").c_str(), &_controller._gmNotesDraft, ImVec2(-1.0f, 110.0f)))_controller.notesDraftEdited();
        ImGui::EndTable();
    }
    drawMutedHelpText("Draft: player-visible and GM-only notes stay separate and save together.", "Use player-visible notes for material safe to show at the table; private GM notes remain GM-facing.");
    if (drawUiButton("Save Notes",
                     notesDirty,
                     "There are no draft note changes to save.",
                     UiButtonRole::Primary,
                     ImVec2(128.0f, 0.0f),
                     "Save backstory, player notes, and private GM notes for this character."))
    {
        _controller.saveSelectedCharacterNotesDraft();
    }
    ImGui::SameLine();
    if (drawUiButton("Revert Draft",
                     notesDirty,
                     "There are no draft note changes to revert.",
                     UiButtonRole::Secondary,
                     ImVec2(128.0f, 0.0f),
                     "Discard unsaved note edits and reload the saved character text."))
    {
        _controller.revertSelectedCharacterNotesDraft();
    }

    }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(localizedUiLabel("Story/Links").c_str()))
        {
    ImGui::SeparatorText(localizedUiLabel("Relationships").c_str());
    showHelpTooltip("Character-owned links to other campaign entities. Use Story Workspace for scene and arc link management.");
    if (character.relationships.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No relationships recorded.").c_str());
    }
    else if (ImGui::BeginTable("CharacterRelationships", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Target").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Label").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn(localizedUiLabel("Notes").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        int pendingRelationshipRemoval = -1;
        for (int i = 0; i < static_cast<int>(character.relationships.size()); ++i)
        {
            const CharacterRelationshipModel &relationship = character.relationships[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(localizedUiLabel(relationship.targetEntityType.c_str()));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(_controller.describeRelationshipTarget(relationship));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(relationship.label.empty() ? localizedUiLabel("Related") : relationship.label);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(relationship.notes);
            ImGui::TableSetColumnIndex(4);
            ImGui::PushID(i);
            if (ImGui::SmallButton(localizedUiLabel("Remove").c_str()))
                pendingRelationshipRemoval = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (pendingRelationshipRemoval >= 0)
            _controller.removeSelectedCharacterRelationship(pendingRelationshipRemoval);
    }

    const std::vector<std::string> relationshipTypes = _controller.getRelationshipTargetTypes();
    if (_controller._relationshipTargetTypeIndex >= static_cast<int>(relationshipTypes.size()))
        _controller._relationshipTargetTypeIndex = 0;
    const std::string selectedRelationshipType = relationshipTypes.empty() ? "" : relationshipTypes[static_cast<std::size_t>(_controller._relationshipTargetTypeIndex)];
    ImGui::SetNextItemWidth(140.0f);
    const std::string selectedRelationshipTypeLabel = localizedUiLabel(selectedRelationshipType.c_str());
    if (FormUi::BeginCombo(localizedUiLabel("Target Type").c_str(), selectedRelationshipTypeLabel.c_str()))
    {
        for (int i = 0; i < static_cast<int>(relationshipTypes.size()); ++i)
        {
            const bool selected = _controller._relationshipTargetTypeIndex == i;
            if (ImGui::Selectable(localizedUiLabel(relationshipTypes[static_cast<std::size_t>(i)].c_str()).c_str(), selected))
            {
                _controller._relationshipTargetTypeIndex = i;
                _controller._relationshipTargetIndex = 0;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    const std::vector<CharacterSheetController::RelationshipTargetOption> relationshipOptions = _controller.getRelationshipTargetOptions(selectedRelationshipType);
    if (_controller._relationshipTargetIndex >= static_cast<int>(relationshipOptions.size()))
        _controller._relationshipTargetIndex = 0;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if (relationshipOptions.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No targets available").c_str());
    }
    else if (FormUi::BeginCombo(localizedUiLabel("Target").c_str(), relationshipOptions[static_cast<std::size_t>(_controller._relationshipTargetIndex)].label.c_str()))
    {
        for (int i = 0; i < static_cast<int>(relationshipOptions.size()); ++i)
        {
            const bool selected = _controller._relationshipTargetIndex == i;
            if (ImGui::Selectable(relationshipOptions[static_cast<std::size_t>(i)].label.c_str(), selected))
                _controller._relationshipTargetIndex = i;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(220.0f);
    FormUi::InputText(localizedUiLabel("Relationship Label").c_str(), &_controller._relationshipLabel);
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::InputText(localizedUiLabel("Relationship Notes").c_str(), &_controller._relationshipNotes);
    if (!relationshipOptions.empty() && ImGui::Button(localizedUiLabel("Add Relationship").c_str()))
    {
        const auto &target = relationshipOptions[static_cast<std::size_t>(_controller._relationshipTargetIndex)];
        _controller.addSelectedCharacterRelationship(selectedRelationshipType, target.id, _controller._relationshipLabel, _controller._relationshipNotes);
        _controller._relationshipLabel.clear();
        _controller._relationshipNotes.clear();
    }

    const std::vector<CharacterStoryReference> storyReferences = findCharacterStoryReferences(_campaign, character);
    ImGui::SeparatorText(localizedUiLabel("Linked Stories").c_str());
    ImGui::Text(localizedUiLabel("Linked Stories: %d").c_str(), static_cast<int>(storyReferences.size()));
    showHelpTooltip("Story arcs or scenes that connect directly to this character.");
    if (storyReferences.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No story arcs are linked to this character yet.").c_str());
    }
    else if (ImGui::BeginTable("CharacterStories", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Story Arc").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Arc Kind").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Scene or Note").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("").c_str());
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(storyReferences.size()); ++i)
        {
            const CharacterStoryReference &reference = storyReferences[i];
            if (reference.threadIndex < 0 || reference.threadIndex >= static_cast<int>(_campaign.storyThreads.size()))
                continue;

            const StoryThreadModel &thread = _campaign.storyThreads[reference.threadIndex];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(thread.title.empty() ? localizedUiLabel("<Untitled Story>") : thread.title);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(localizedUiText(storyThreadTypeLabel(thread.type)));
            ImGui::TableSetColumnIndex(2);
            if (!reference.beatTitle.empty())
                tableTextWrapped(reference.beatTitle);
            else if (!reference.linkLabel.empty())
                tableTextWrapped(reference.linkLabel);
            else
                ImGui::TextDisabled(localizedUiLabel("Arc-level connection").c_str());
            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button(localizedUiLabel("Open Story").c_str()))
                _controller.openStoryThread(thread.metadata.id);
            showHelpTooltip("Open the Story & Plot Board focused on this story arc.");
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

            ImGui::EndTabItem();
        }
