    EncounterModel &encounter = _controller._draftEncounter;
    normalizeEncounter(encounter);

    if (encounter.theaterOfMind)
    {
        const char *const positionOptions[] = {"In the scene", "Front line", "Near allies", "Back line", "Elevated", "Hidden", "At objective", "Off screen"};
        const char *const rangeOptions[] = {"Engaged", "Near", "Far", "Distant", "Out of scene"};
        constexpr int positionOptionCount = static_cast<int>(sizeof(positionOptions) / sizeof(positionOptions[0]));
        constexpr int rangeOptionCount = static_cast<int>(sizeof(rangeOptions) / sizeof(rangeOptions[0]));
        std::vector<std::string> localizedPositionStorage;
        std::vector<const char *> localizedPositionOptions;
        localizedPositionStorage.reserve(positionOptionCount);
        localizedPositionOptions.reserve(positionOptionCount);
        for (const char *option : positionOptions)
            localizedPositionStorage.push_back(battleText(option));
        for (const std::string &option : localizedPositionStorage)
            localizedPositionOptions.push_back(option.c_str());
        std::vector<std::string> localizedRangeStorage;
        std::vector<const char *> localizedRangeOptions;
        localizedRangeStorage.reserve(rangeOptionCount);
        localizedRangeOptions.reserve(rangeOptionCount);
        for (const char *option : rangeOptions)
            localizedRangeStorage.push_back(battleText(option));
        for (const std::string &option : localizedRangeStorage)
            localizedRangeOptions.push_back(option.c_str());
        int spotlightIndex = -1;
        for (int index = 0; index < static_cast<int>(encounter.participants.size()); ++index)
        {
            if (encounter.participants[static_cast<std::size_t>(index)].narrativeSpotlight)
            {
                spotlightIndex = index;
                break;
            }
        }

        ImGui::BeginChild("TheaterOfMindCombat", ImGui::GetContentRegionAvail(), true);
        ImGui::Text(localizedUiLabel("Theater of the Mind").c_str());
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), encounter.description.empty() ? localizedUiText("No encounter narration has been entered.") : encounter.description.c_str());
        if (spotlightIndex >= 0 && spotlightIndex < static_cast<int>(encounter.participants.size()))
        {
            const EncounterParticipantModel &spotlight = encounter.participants[static_cast<std::size_t>(spotlightIndex)];
            const std::string spotlightPosition = localizedNarrativeValue(spotlight.narrativePosition);
            const std::string spotlightRange = battleText(spotlight.narrativeRangeBand.c_str());
            ImGui::TextDisabled(localizedUiLabel("Spotlight: %s | %s | %s").c_str(),
                                spotlight.displayName.c_str(),
                                spotlightPosition.c_str(),
                                spotlightRange.c_str());
        }
        else
        {
            ImGui::TextDisabled(localizedUiLabel("Spotlight: none selected").c_str());
        }
        ImGui::Separator();
        const int activeIndex = encounter.participants.empty() ? -1 : std::clamp(encounter.activeParticipantIndex, 0, static_cast<int>(encounter.participants.size()) - 1);
        const int anchorIndex = spotlightIndex >= 0 ? spotlightIndex : activeIndex;
        const int selectedIndex = (_controller._selectedParticipantIndex >= 0 && _controller._selectedParticipantIndex < static_cast<int>(encounter.participants.size()))
                                      ? _controller._selectedParticipantIndex
                                      : activeIndex;
        if (selectedIndex >= 0 && anchorIndex >= 0)
        {
            const EncounterParticipantModel &selectedParticipant = encounter.participants[static_cast<std::size_t>(selectedIndex)];
            const EncounterParticipantModel &anchorParticipant = encounter.participants[static_cast<std::size_t>(anchorIndex)];
            const std::string selectedName = selectedParticipant.displayName;
            const std::string anchorName = anchorParticipant.displayName;
            const bool selectedHasSpotlight = selectedParticipant.narrativeSpotlight;
            ImGui::SeparatorText(localizedUiLabel("Relative Positioning").c_str());
            ImGui::TextDisabled(localizedUiLabel("Selected: %s | Anchor: %s%s").c_str(),
                                selectedName.c_str(),
                                anchorName.c_str(),
                                localizedUiText(spotlightIndex >= 0 ? " (spotlight)" : " (active)"));
            if (drawSmallUiButton("Engaged", UiButtonRole::Primary, "Set the selected combatant as engaged with the spotlight or active actor."))
                _controller.setNarrativeRelativeTo(selectedIndex, anchorIndex, "Engaged", "Engaged with");
            ImGui::SameLine();
            if (drawSmallUiButton("Near", UiButtonRole::Secondary, "Set the selected combatant near the spotlight or active actor."))
                _controller.setNarrativeRelativeTo(selectedIndex, anchorIndex, "Near", "Near");
            ImGui::SameLine();
            if (drawSmallUiButton("Far", UiButtonRole::Secondary, "Set the selected combatant far from the spotlight or active actor."))
                _controller.setNarrativeRelativeTo(selectedIndex, anchorIndex, "Far", "Far from");
            ImGui::SameLine();
            if (drawSmallUiButton("Distant", UiButtonRole::Secondary, "Set the selected combatant distant from the spotlight or active actor."))
                _controller.setNarrativeRelativeTo(selectedIndex, anchorIndex, "Distant", "Distant from");
            ImGui::SameLine();
            if (drawSmallUiButton("Out", UiButtonRole::Advanced, "Move the selected combatant out of the current scene."))
                _controller.setNarrativeRelativeTo(selectedIndex, anchorIndex, "Out of scene", "Away from");
            ImGui::SameLine();
            if (drawSmallUiButton("Focus", selectedHasSpotlight ? UiButtonRole::Primary : UiButtonRole::Secondary, "Make the selected combatant the narrative spotlight."))
                _controller.setNarrativeSpotlight(selectedIndex);
            drawMutedHelpText("Click a roster row to choose the selected combatant. The helper anchors to the spotlight when one is set, otherwise to the active actor.", "These quick buttons update the same persisted position and range-band fields as the table combos.");
        }
        if (ImGui::BeginTable("TheaterOfMindParticipants", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Turn").c_str(), ImGuiTableColumnFlags_WidthFixed, 52.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn(localizedUiLabel("Position / Range").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.30f);
            ImGui::TableSetupColumn(localizedUiLabel("Health / State").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
            ImGui::TableSetupColumn(localizedUiLabel("Focus").c_str(), ImGuiTableColumnFlags_WidthFixed, 68.0f);
            ImGui::TableHeadersRow();
            for (int index = 0; index < static_cast<int>(encounter.participants.size()); ++index)
            {
                EncounterParticipantModel &participant = encounter.participants[index];
                const bool selected = _controller._selectedParticipantIndex == index || _controller._rangeTargetIndex == index;
                ImGui::PushID(index);
                ImGui::TableNextRow();
                if (participant.narrativeSpotlight)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(86, 72, 38, 145));
                else if (selected)
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(58, 76, 90, 110));
                ImGui::TableSetColumnIndex(0);
                drawActiveMarker(index == encounter.activeParticipantIndex && _controller._combatStep == 1, "Active combatant");
                ImGui::SameLine(0.0f, 2.0f);
                ImGui::Text(localizedUiLabel("%d").c_str(), index + 1);
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(participant.displayName);
                if (ImGui::IsItemClicked())
                {
                    if (_controller._combatStep == 1 && encounter.combatPhase == CombatPhase::ActionResolution && index != encounter.activeParticipantIndex)
                    {
                        _controller._rangeTargetIndex = index;
                        _controller._selectedTargetIndices.clear();
                        _controller._selectedTargetIndices.push_back(index);
                    }
                    else
                    {
                        _controller._selectedParticipantIndex = index;
                    }
                }
                ImGui::TableSetColumnIndex(2);
                int positionIndex = optionIndexForValue(participant.narrativePosition, positionOptions, positionOptionCount, 0);
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::Combo(localizedUiLabel("##NarrativePosition").c_str(), &positionIndex, localizedPositionOptions.data(), positionOptionCount))
                    _controller.setNarrativePosition(index, positionOptions[positionIndex]);
                int rangeIndex = optionIndexForValue(participant.narrativeRangeBand, rangeOptions, rangeOptionCount, 1);
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::Combo(localizedUiLabel("##NarrativeRange").c_str(), &rangeIndex, localizedRangeOptions.data(), rangeOptionCount))
                    _controller.setNarrativeRangeBand(index, rangeOptions[rangeIndex]);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text(localizedUiLabel("%d/%d HP").c_str(), participant.currentHitPoints, participant.maxHitPoints);
                if (participant.conditions.empty())
                    ImGui::TextDisabled(localizedUiLabel("-").c_str());
                else
                    tableTextWrapped(localizedConditionText(participant.conditions.front()));
                ImGui::TableSetColumnIndex(4);
                if (drawSmallUiButton(participant.narrativeSpotlight ? "On" : "Focus",
                                      participant.narrativeSpotlight ? UiButtonRole::Primary : UiButtonRole::Secondary,
                                      "Marks this combatant as the current narrative spotlight."))
                    _controller.setNarrativeSpotlight(index);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        drawMutedHelpText("Narrative range bands are GM-facing table state. Grid distance and range blocking stay disabled until Theater of the Mind is turned off.", "Use Engaged, Near, Far, Distant, and Out of Scene to keep action resolution quick without pretending there is a grid.");
        ImGui::EndChild();
        return;
    }
