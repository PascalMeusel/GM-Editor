#include "BattleTrackerViewActionGuideSupport.hpp"

void BattleTrackerView::drawTurnActions()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (_controller._combatStep == 1 && !encounter.participants.empty())
    {
        EncounterParticipantModel &active = encounter.participants[encounter.activeParticipantIndex];
        ImGui::Separator();
        ImGui::TextWrapped(uiTextForLocale("Current turn: %s", "Aktueller Zug: %s"), active.displayName.c_str());
        ImGui::TextDisabled(localizedUiLabel("Combat phase: %s  Round %d").c_str(), localizedUiText(combatPhaseDisplayLabel(encounter.combatPhase)), encounter.round);
        if (encounter.theaterOfMind)
        {
            ImGui::TextDisabled(localizedUiLabel("Narrative movement, speed %d ft. Grid distance is not enforced.").c_str(), active.speedFeet);
            showHelpTooltip("Theater of the Mind keeps turn order and action resolution active while leaving distance and positioning to GM adjudication.");
        }
        else
        {
            ImGui::TextDisabled(localizedUiLabel("Movement %d/%d ft from start square (%d, %d)").c_str(), active.movementUsedFeet, active.speedFeet, active.turnStartX, active.turnStartY);
            showHelpTooltip("Movement is measured from the square where this participant started the current turn.");
        }

        if (ImGui::BeginTable("CombatPhaseRail", battleActionColumns(5, {"Turn", "Move", "Action", "Effects", "End"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            struct PhaseButton
            {
                CombatPhase phase;
                const char *label;
                const char *tooltip;
            };
            const PhaseButton phaseButtons[] = {
                {CombatPhase::ActiveTurn, "Turn", "Return to the active-turn overview."},
                {CombatPhase::Movement, "Move", "Focus on active-combatant movement and token positioning."},
                {CombatPhase::ActionResolution, "Action", "Open target selection, attack, spell, damage, healing, and effect resolution."},
                {CombatPhase::ReactionsEffects, "Effects", "Track reactions, conditions, concentration, and expiring effects."},
                {CombatPhase::EndTurn, "End", "Review turn-end state before advancing to the next combatant."},
            };
            for (const PhaseButton &button : phaseButtons)
            {
                ImGui::TableNextColumn();
                const bool currentPhase = encounter.combatPhase == button.phase;
                if (drawUiButton(button.label,
                                 !currentPhase,
                                 "Already in this combat phase.",
                                 currentPhase ? UiButtonRole::Primary : UiButtonRole::Secondary,
                                 ImVec2(-1.0f, 0.0f),
                                 button.tooltip))
                    changeCombatPhaseAndFocus(button.phase);
            }
            ImGui::EndTable();
        }

        if (encounter.combatPhase == CombatPhase::Movement)
            ImGui::TextDisabled(localizedUiText(encounter.theaterOfMind ? "Movement phase: adjudicate narrative positioning, then switch to Action."
                                                                       : "Movement phase: drag or confirm the active combatant, then switch to Action."));
        else if (encounter.combatPhase == CombatPhase::ActionResolution)
            ImGui::TextDisabled(localizedUiLabel("Action phase: choose targets and resolve an attack, spell, healing, or effect.").c_str());
        else if (encounter.combatPhase == CombatPhase::ReactionsEffects)
            ImGui::TextDisabled(localizedUiLabel("Effects phase: use Selected Combatant Details for conditions, resources, reactions, and active effects.").c_str());
        else if (encounter.combatPhase == CombatPhase::EndTurn)
            ImGui::TextDisabled(localizedUiLabel("End phase: advance when turn-end effects and notes are complete.").c_str());

        if (ImGui::TreeNodeEx(localizedUiLabel("Advanced Movement Override").c_str(), _controller._freePlacementMode ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, _controller._freePlacementMode ? ImVec4(0.98f, 0.78f, 0.40f, 1.0f) : ImVec4(0.80f, 0.83f, 0.84f, 1.0f));
            FormUi::Checkbox(localizedUiLabel("Free Placement Override").c_str(), &_controller._freePlacementMode);
            ImGui::PopStyleColor();
            showHelpTooltip("Advanced correction mode. When enabled, any token can be dragged anywhere and normal turn movement limits are ignored.");
            if (_controller._freePlacementMode)
                ImGui::TextColored(ImVec4(0.98f, 0.70f, 0.32f, 1.0f), localizedUiLabel("Free Placement is ON: token dragging ignores active-turn movement rules.").c_str());
            else
                ImGui::TextDisabled(localizedUiLabel("Normal mode: only the active combatant can move during combat, and movement is limited by speed.").c_str());
            ImGui::TreePop();
        }

        bool pauseReview = false;
        bool wrapUpRequested = false;
        if (ImGui::BeginTable("ActiveTurnPrimaryActions", battleActionColumns(2, {"Skip Turn", "Pause Combat", "Wrap up combat"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton("Skip Turn", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Logs that this combatant skipped and advances turn order."))
                _controller.skipTurn();
            ImGui::TableNextColumn();
            if (drawUiButton("Pause Combat", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Marks combat as paused for a table break or interruption."))
                pauseReview = true;
            ImGui::TableNextColumn();
            if (drawUiButton("Wrap up combat", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                wrapUpRequested = true;
            ImGui::EndTable();
        }

        if (wrapUpRequested)
        {
            _controller.generateAfterCombatSummary();
            _focusWrapUp = true;
            return;
        }

        if (pauseReview) ImGui::OpenPopup(localizedUiLabel("Pause Combat Review").c_str());
        const bool hasActiveSession = _controller.hasActiveSession();
        const bool sessionBreakActive = _controller.isSessionBreakActive();
        const int playingAudioChannels = _controller.playingAudioChannelCount();
        const std::string sessionState = battleText(hasActiveSession ? (sessionBreakActive ? "Break active" : "Running") : "No live session");
        const std::string audioState = battleFormat("{0} playing / {1} paused",
                                                    {std::to_string(playingAudioChannels),
                                                     std::to_string(_controller.pausedAudioChannelCount())});
        const char *breakPopup = nullptr;
        if (ImGui::CollapsingHeader(localizedUiLabel("Table break coordination").c_str()))
        {
        drawInlineLabelValue("Session State", sessionState.c_str());
        drawInlineLabelValue("Audio", audioState.c_str());
        const std::string breakReason = !hasActiveSession ? battleText("Start a live session before starting a table break from combat.")
                                         : sessionBreakActive ? battleText("The live session is already in break state.")
                                                              : "";
        const std::string audioBreakReason = !hasActiveSession ? battleText("Start a live session before starting a table break from combat.")
                                              : sessionBreakActive ? battleText("The live session is already in break state.")
                                              : playingAudioChannels <= 0 ? battleText("No playing audio channels to pause.")
                                                                         : "";
        if (ImGui::BeginTable("CombatBreakActions", 1, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton("Start Table Break",
                             hasActiveSession && !sessionBreakActive,
                             breakReason.c_str(),
                             UiButtonRole::Advanced,
                             ImVec2(-1.0f, 0.0f),
                             "Pauses combat and records a break marker in the live session. Audio is left unchanged."))
                breakPopup = "Combat Table Break";
            ImGui::TableNextColumn();
            if (drawUiButton("Break + Pause Audio",
                             hasActiveSession && !sessionBreakActive && playingAudioChannels > 0,
                             audioBreakReason.c_str(),
                             UiButtonRole::Advanced,
                             ImVec2(-1.0f, 0.0f),
                             "Pauses combat, starts a session break, and pauses all playing audio channels."))
                breakPopup = "Combat Table Break With Audio";
            ImGui::EndTable();
        }
        }

        if (breakPopup) ImGui::OpenPopup(localizedUiLabel(breakPopup).c_str());

        if (beginBoundedConfirmationModal(localizedUiLabel("Pause Combat Review").c_str()))
        {
            ImGui::TextWrapped(localizedUiLabel("Pause this encounter while keeping the current initiative order.").c_str());
            drawInlineLabelValue("Encounter", encounter.name.empty() ? localizedUiText("Current encounter") : encounter.name.c_str());
            drawInlineLabelValue("Round", std::to_string(encounter.round).c_str());
            if (drawModalActionButtons("Pause Combat", UiButtonRole::Advanced))
                _controller.pauseCombat();
            ImGui::EndPopup();
        }

        if (beginBoundedConfirmationModal(localizedUiLabel("Combat Table Break").c_str()))
        {
            ImGui::TextWrapped(localizedUiLabel("Pause combat and mark the live session as on break.").c_str());
            drawInlineLabelValue("Session", sessionState.c_str());
            drawInlineLabelValue("Audio", localizedUiText("Leave audio unchanged"));
            if (drawModalActionButtons("Start Break", UiButtonRole::Advanced))
                _controller.startCombatTableBreak(false);
            ImGui::EndPopup();
        }

        if (beginBoundedConfirmationModal(localizedUiLabel("Combat Table Break With Audio").c_str()))
        {
            ImGui::TextWrapped(localizedUiLabel("Pause combat, mark the live session as on break, and pause every playing audio channel.").c_str());
            drawInlineLabelValue("Session", sessionState.c_str());
            drawInlineLabelValue("Audio", audioState.c_str());
            if (drawModalActionButtons("Break + Audio", UiButtonRole::Advanced))
                _controller.startCombatTableBreak(true);
            ImGui::EndPopup();
        }
    }
    else if (encounter.combatPaused && !encounter.participants.empty())
    {
        ImGui::Separator();
        ImGui::TextDisabled(localizedUiLabel("Combat phase: %s at round %d.").c_str(), localizedUiText(combatPhaseDisplayLabel(encounter.combatPhase)), encounter.round);
        const bool hasActiveSession = _controller.hasActiveSession();
        const bool sessionBreakActive = _controller.isSessionBreakActive();
        const int pausedAudioChannels = _controller.pausedAudioChannelCount();
        const std::string sessionState = battleText(hasActiveSession ? (sessionBreakActive ? "Break active" : "Running") : "No live session");
        const std::string audioState = battleFormat("{0} playing / {1} paused",
                                                    {std::to_string(_controller.playingAudioChannelCount()),
                                                     std::to_string(pausedAudioChannels)});
        drawInlineLabelValue("Session State", sessionState.c_str());
        drawInlineLabelValue("Audio", audioState.c_str());
        const char *pendingPopup = nullptr;
        if (ImGui::BeginTable("CombatResumeActions", 1, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton("Resume Combat", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Returns to the active turn and keeps the current initiative order."))
                pendingPopup = "Resume Combat Review";
            if (hasActiveSession && sessionBreakActive)
            {
                ImGui::TableNextColumn();
                if (drawUiButton("Resume Table + Combat", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                                 "Ends the live session break and resumes this encounter. Audio is left unchanged."))
                    pendingPopup = "Resume Table And Combat";
                if (pausedAudioChannels > 0)
                {
                    ImGui::TableNextColumn();
                    if (drawUiButton("Resume Table + Combat + Audio", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                                     "Ends the live session break, resumes this encounter, and resumes paused audio channels."))
                        pendingPopup = "Resume Table Combat And Audio";
                }
            }
            ImGui::EndTable();
        }
        if (pendingPopup) ImGui::OpenPopup(localizedUiLabel(pendingPopup).c_str());

        if (beginBoundedConfirmationModal(localizedUiLabel("Resume Combat Review").c_str()))
        {
            ImGui::TextWrapped(localizedUiLabel("Resume this encounter without changing live-session break or audio state.").c_str());
            drawInlineLabelValue("Encounter", encounter.name.empty() ? localizedUiText("Current encounter") : encounter.name.c_str());
            drawInlineLabelValue("Session", sessionState.c_str());
            if (drawModalActionButtons("Resume Combat", UiButtonRole::Primary))
                _controller.resumeCombat();
            ImGui::EndPopup();
        }

        if (beginBoundedConfirmationModal(localizedUiLabel("Resume Table And Combat").c_str()))
        {
            ImGui::TextWrapped(localizedUiLabel("End the live-session break and resume this encounter. Audio is left unchanged.").c_str());
            drawInlineLabelValue("Session", sessionState.c_str());
            drawInlineLabelValue("Audio", localizedUiText("Leave audio unchanged"));
            if (drawModalActionButtons("Resume", UiButtonRole::Primary))
                _controller.resumeCombatFromTableBreak(false);
            ImGui::EndPopup();
        }

        if (beginBoundedConfirmationModal(localizedUiLabel("Resume Table Combat And Audio").c_str()))
        {
            ImGui::TextWrapped(localizedUiLabel("End the live-session break, resume this encounter, and resume paused audio channels.").c_str());
            drawInlineLabelValue("Session", sessionState.c_str());
            drawInlineLabelValue("Audio", audioState.c_str());
            if (drawModalActionButtons("Resume + Audio", UiButtonRole::Primary))
                _controller.resumeCombatFromTableBreak(true);
            ImGui::EndPopup();
        }
    }
}
