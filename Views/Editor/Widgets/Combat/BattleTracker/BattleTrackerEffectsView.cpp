#include "BattleTrackerViewActionGuideSupport.hpp"

void BattleTrackerView::drawEffectsReviewPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (_controller._combatStep != 1 || encounter.combatPaused || encounter.combatPhase != CombatPhase::ReactionsEffects || encounter.participants.empty())
        return;

    const std::vector<int> selectedTargets = _controller.getSelectedTargetIndices();
    std::vector<int> reviewParticipants;
    const auto addParticipant = [&](int participantIndex) {
        if (participantIndex < 0 || participantIndex >= static_cast<int>(encounter.participants.size()))
            return;
        if (std::find(reviewParticipants.begin(), reviewParticipants.end(), participantIndex) == reviewParticipants.end())
            reviewParticipants.push_back(participantIndex);
    };

    addParticipant(encounter.activeParticipantIndex);
    for (int targetIndex : selectedTargets)
        addParticipant(targetIndex);
    for (int participantIndex = 0; participantIndex < static_cast<int>(encounter.participants.size()); ++participantIndex)
    {
        const EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(participantIndex)];
        if (!participant.conditions.empty() || !participant.activeEffects.empty() || participantHasConcentrationMarker(participant))
            addParticipant(participantIndex);
    }

    ImGui::Separator();
    drawHelpText("4. Reactions / Effects", "Review reactions, concentration, conditions, and active effects before ending the turn.");
    if (consumeCombatControlsFocus(CombatControlsFocus::Effects))
        ImGui::SetScrollHereY(0.20f);

    if (reviewParticipants.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No reactions or effects are currently flagged.").c_str());
    }
    else if (ImGui::BeginTable("EffectsReviewTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Combatant").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Review").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.20f);
        ImGui::TableSetupColumn(localizedUiLabel("Conditions").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn(localizedUiLabel("Effects").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Conc.").c_str(), ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableHeadersRow();

        for (int participantIndex : reviewParticipants)
        {
            EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(participantIndex)];
            const bool selected = _controller._selectedParticipantIndex == participantIndex;
            ImGui::PushID(participantIndex);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(participant.displayName.empty() ? "<Unnamed>" : participant.displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns))
                _controller._selectedParticipantIndex = participantIndex;
            if (selected)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(58, 76, 90, 110));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(effectReviewReason(encounter, selectedTargets, participantIndex));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(summarizeConditions(participant));
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(summarizeActiveEffects(participant));
            ImGui::TableSetColumnIndex(4);
            ImGui::Text(localizedUiLabel("%s").c_str(), participantHasConcentrationMarker(participant) ? localizedUiText("Yes") : "-");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (_controller._selectedParticipantIndex >= 0 && _controller._selectedParticipantIndex < static_cast<int>(encounter.participants.size()))
    {
        const EncounterParticipantModel &selected = encounter.participants[static_cast<std::size_t>(_controller._selectedParticipantIndex)];
        ImGui::TextDisabled(localizedUiLabel("Selected review: %s").c_str(), selected.displayName.c_str());
        drawInlineHelpMarker("Use Selected Combatant Details above to add or remove conditions and active effects for the selected row.", "?", "EffectsReviewSelected");
    }

    bool anyReactionActions = false;
    if (ImGui::BeginTable("EffectsReviewReactionActions", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Combatant").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Reaction").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn(localizedUiLabel("Resource").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Resolve").c_str(), ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();
        for (int participantIndex = 0; participantIndex < static_cast<int>(encounter.participants.size()); ++participantIndex)
        {
            const EncounterParticipantModel &participant = encounter.participants[static_cast<std::size_t>(participantIndex)];
            for (int actionIndex = 0; actionIndex < static_cast<int>(participant.actions.size()); ++actionIndex)
            {
                const MonsterActionModel &action = participant.actions[static_cast<std::size_t>(actionIndex)];
                if (!isReactionAction(action))
                    continue;

                anyReactionActions = true;
                ImGui::PushID(participantIndex * 1000 + actionIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(participant.displayName);
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(action.name.empty() ? battleText("Reaction") : action.name);
                if (!action.notes.empty())
                    tableTextWrapped(action.notes);
                ImGui::TableSetColumnIndex(2);
                const std::string resourceStatus = _controller.reactionResourceStatus(participantIndex, actionIndex);
                tableTextWrapped(resourceStatus);
                ImGui::TableSetColumnIndex(3);
                std::string spendReason;
                const bool hasLinkedResource = !action.linkedResourceId.empty();
                std::string disabledReason;
                const bool canResolveReaction = _controller.reactionResourceAvailable(participantIndex, actionIndex, &disabledReason);
                if (drawUiButton(hasLinkedResource ? "Spend" : "Log",
                                 canResolveReaction,
                                 disabledReason.c_str(),
                                 UiButtonRole::Primary,
                                 ImVec2(-1.0f, 0.0f),
                                 hasLinkedResource ? "Spend the linked reaction resource and log the reaction." : "Log that this reaction was reviewed."))
                {
                    if (!_controller.spendReactionActionResource(participantIndex, actionIndex, &spendReason))
                        _controller._combatErrorMessage = spendReason;
                    else
                        _controller._combatStatusMessage = spendReason.empty() ? battleText("Reaction reviewed.") : spendReason;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    if (!anyReactionActions)
        ImGui::TextDisabled(localizedUiLabel("No explicit reaction actions are listed on the current combatants.").c_str());

    int shownLogEntries = 0;
    if (ImGui::BeginTable("EffectsReviewRecentLog", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Event").c_str(), ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Actor").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn(localizedUiLabel("Prompt").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.72f);
        ImGui::TableHeadersRow();
        for (auto it = encounter.combatLog.rbegin(); it != encounter.combatLog.rend() && shownLogEntries < 5; ++it)
        {
            if (it->round != std::max(1, encounter.round) || !isEffectsReviewLogType(it->eventType))
                continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(battleText(it->eventType.c_str()));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(it->actorName.empty() ? "-" : it->actorName);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(it->summary);
            ++shownLogEntries;
        }
        ImGui::EndTable();
    }
    if (shownLogEntries == 0)
        ImGui::TextDisabled(localizedUiLabel("No reaction, concentration, or active-effect log prompts for this round yet.").c_str());

    if (drawUiButton("Complete Effects", UiButtonRole::Primary, ImVec2(168.0f, 0.0f), "Logs the active effect review and moves to End Turn."))
        _controller.completeEffectsReview();
    ImGui::SameLine();
    if (drawUiButton("Back to Action", UiButtonRole::Secondary, ImVec2(140.0f, 0.0f), "Return to action resolution for another target, attack, spell, or effect."))
        changeCombatPhaseAndFocus(CombatPhase::ActionResolution);
}
