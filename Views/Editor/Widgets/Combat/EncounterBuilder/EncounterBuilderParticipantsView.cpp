#include "EncounterBuilderViewSupport.hpp"

void EncounterBuilderView::drawParticipantTable()
{
    EncounterModel &encounter = _controller._draftEncounter;

    if (encounter.participants.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("Add players, NPCs, or monsters to prepare the encounter roster.").c_str());
        return;
    }

    if (_controller._selectedParticipantIndex < 0 || _controller._selectedParticipantIndex >= static_cast<int>(encounter.participants.size()))
        _controller._selectedParticipantIndex = 0;

    bool hasSelectedParticipant = _controller._selectedParticipantIndex >= 0 && _controller._selectedParticipantIndex < static_cast<int>(encounter.participants.size());
    if (drawUiButton("Remove Selected",
                     hasSelectedParticipant,
                     "Select a participant first.",
                     UiButtonRole::Destructive,
                     ImVec2(160.0f, 0.0f),
                     "Remove the selected participant after confirmation."))
        ImGui::OpenPopup(localizedUiLabel("Remove Encounter Participant").c_str());
    if (drawConfirmationModal("Remove Encounter Participant", "Remove the selected participant from this prepared encounter?", "Remove"))
        _controller.removeSelectedParticipant();
    if (encounter.participants.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("Add players, NPCs, or monsters to prepare the encounter roster.").c_str());
        return;
    }
    if (_controller._selectedParticipantIndex < 0 || _controller._selectedParticipantIndex >= static_cast<int>(encounter.participants.size()))
        _controller._selectedParticipantIndex = 0;
    hasSelectedParticipant = _controller._selectedParticipantIndex >= 0 && _controller._selectedParticipantIndex < static_cast<int>(encounter.participants.size());

    if (ImGui::BeginTable("EncounterBuilderParticipants", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, 260.0f)))
    {
        ImGui::TableSetupColumn(localizedUiLabel("#").c_str(), ImGuiTableColumnFlags_WidthFixed, 32.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthFixed, 78.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Stats").c_str(), ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Setup").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.46f);
        ImGui::TableHeadersRow();

        for (int index = 0; index < static_cast<int>(encounter.participants.size()); ++index)
        {
            EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(index)];
            ImGui::PushID(participant.metadata.id.c_str());
            ImGui::TableNextRow();
            if (_controller._selectedParticipantIndex == index)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(58, 76, 90, 120));

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(std::to_string(index + 1).c_str(), _controller._selectedParticipantIndex == index, ImGuiSelectableFlags_SpanAllColumns))
                _controller._selectedParticipantIndex = index;
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(participant.displayName);
            if (ImGui::IsItemClicked())
                _controller._selectedParticipantIndex = index;
            ImGui::TableSetColumnIndex(2);
            const std::string participantKind = localizedUiLabel(combatantKindLabel(participant.kind));
            ImGui::Text(localizedUiLabel("%s").c_str(), participantKind.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text(localizedUiLabel("%d/%d HP, AC %d").c_str(), participant.currentHitPoints, participant.maxHitPoints, participant.armorClass);
            ImGui::TableSetColumnIndex(4);
            const std::string groupLabel = participant.groupName.empty() ? localizedUiLabel("No group") : participant.groupName;
            std::string setupSummary = formatLocalizedUi("%s | wave %d", groupLabel.c_str(), participant.waveNumber);
            setupSummary = formatLocalizedUi("%s | init %d", setupSummary.c_str(), participant.initiative);
            setupSummary = formatLocalizedUi("%s | start %d,%d", setupSummary.c_str(), participant.startingX, participant.startingY);
            if (participant.startsHidden)
                setupSummary = formatLocalizedUi("%s | hidden", setupSummary.c_str());
            if (participant.friendly)
                setupSummary = formatLocalizedUi("%s | friendly", setupSummary.c_str());
            if (participant.reinforcementRound > 0)
                setupSummary = formatLocalizedUi("%s | round %d", setupSummary.c_str(), participant.reinforcementRound);
            tableTextWrapped(setupSummary);
            participant.waveNumber = std::max(1, participant.waveNumber);
            participant.reinforcementRound = std::max(0, participant.reinforcementRound);
            participant.startingX = std::clamp(participant.startingX, 0, encounter.battlefieldWidth - 1);
            participant.startingY = std::clamp(participant.startingY, 0, encounter.battlefieldHeight - 1);
            participant.x = participant.startingX;
            participant.y = participant.startingY;
            participant.turnStartX = std::clamp(participant.turnStartX, 0, encounter.battlefieldWidth - 1);
            participant.turnStartY = std::clamp(participant.turnStartY, 0, encounter.battlefieldHeight - 1);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    showHelpTooltip("Starting positions are saved on the encounter and used by Combat when the battle starts.");

    if (hasSelectedParticipant)
    {
        EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(_controller._selectedParticipantIndex)];
        ImGui::SeparatorText(localizedUiLabel("Selected Participant Details").c_str());
        drawInlineLabelValue("Participant", participant.displayName.empty() ? localizedUiText("<Unnamed>") : participant.displayName.c_str());
        TaskUi::sameLineIfFits();
        const std::string participantKind = localizedUiLabel(combatantKindLabel(participant.kind));
        drawInlineLabelValue("Type", participantKind.c_str());
        const std::string hitPoints = std::to_string(participant.currentHitPoints) + "/" + std::to_string(participant.maxHitPoints);
        const std::string statsSummary = formatLocalizedUi("%s HP, AC %d", hitPoints.c_str(), participant.armorClass);
        drawInlineLabelValue("Stats", statsSummary.c_str());

        drawFieldLabel("Group");
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputText("##EncounterParticipantGroup", &participant.groupName);
        if (ImGui::BeginTable("EncounterParticipantTiming", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            drawFieldLabel("Wave");
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt("##EncounterParticipantWave", &participant.waveNumber, 0, 0);
            ImGui::TableNextColumn();
            drawFieldLabel("Reinforcement Round");
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt("##EncounterParticipantReinforcementRound", &participant.reinforcementRound, 0, 0);
            ImGui::TableNextColumn();
            drawFieldLabel("Initiative");
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt("##EncounterParticipantInitiative", &participant.initiative, 0, 0);
            ImGui::EndTable();
        }
        if (ImGui::BeginTable("EncounterBuilderSelectedParticipantFlags", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            FormUi::Checkbox(localizedUiLabel("Starts Hidden").c_str(), &participant.startsHidden);
            ImGui::TableNextColumn();
            FormUi::Checkbox(localizedUiLabel("Friendly").c_str(), &participant.friendly);
            ImGui::EndTable();
        }
        if (ImGui::BeginTable("EncounterBuilderSelectedParticipantStart", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawFieldLabel("Start X");
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt("##EncounterParticipantStartX", &participant.startingX, 0, 0))
            {
                participant.x = participant.startingX;
                participant.turnStartX = participant.startingX;
            }
            ImGui::TableNextColumn();
            drawFieldLabel("Start Y");
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt("##EncounterParticipantStartY", &participant.startingY, 0, 0))
            {
                participant.y = participant.startingY;
                participant.turnStartY = participant.startingY;
            }
            ImGui::EndTable();
        }
        participant.waveNumber = std::max(1, participant.waveNumber);
        participant.reinforcementRound = std::max(0, participant.reinforcementRound);
        participant.startingX = std::clamp(participant.startingX, 0, encounter.battlefieldWidth - 1);
        participant.startingY = std::clamp(participant.startingY, 0, encounter.battlefieldHeight - 1);
        participant.x = participant.startingX;
        participant.y = participant.startingY;
        participant.turnStartX = std::clamp(participant.turnStartX, 0, encounter.battlefieldWidth - 1);
        participant.turnStartY = std::clamp(participant.turnStartY, 0, encounter.battlefieldHeight - 1);
        drawMutedHelpText("The roster stays compact for scanning. Edit wave, reinforcement, hidden/friendly state, initiative, and start position in this selected-detail drawer.", "Select a different roster row to edit that participant.");
    }
}
