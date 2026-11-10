#include "BattleTrackerViewActionGuideSupport.hpp"

void BattleTrackerView::drawDiceRollPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (!ImGui::CollapsingHeader(localizedUiLabel("Dice Rolls").c_str())) return;
    showHelpTooltip("Roll dice notation such as 1d20+5, 2d6+3, or saved encounter presets. Results are written to the combat log.");

    if (!encounter.diceRollPresets.empty())
    {
        std::vector<const char *> presetLabels;
        for (const DiceRollPresetModel &preset : encounter.diceRollPresets)
            presetLabels.push_back(preset.name.empty() ? "<Unnamed Roll>" : preset.name.c_str());
        _controller._selectedDiceRollPresetIndex = std::clamp(_controller._selectedDiceRollPresetIndex, 0, static_cast<int>(presetLabels.size()) - 1);
        if (FormUi::Combo(localizedUiLabel("Preset").c_str(), &_controller._selectedDiceRollPresetIndex, presetLabels.data(), static_cast<int>(presetLabels.size())))
            _controller.loadSelectedDiceRollPreset();
    }

    FormUi::InputText(localizedUiLabel("Roll Name").c_str(), &_controller._diceRollPresetName);
    FormUi::InputText(localizedUiLabel("Expression").c_str(), &_controller._diceRollExpression);
    if (_controller._currentCampaign.ruleSet)
    {
        const RuleDiceSummary summary = _controller._currentCampaign.ruleSet->summarizeDiceExpression(_controller._diceRollExpression);
        if (summary.valid)
            ImGui::TextDisabled(localizedUiLabel("Range %d-%d, avg %.1f, modifier %+d").c_str(), summary.minimum, summary.maximum, summary.average, summary.flatModifier);
        else
            ImGui::TextDisabled(localizedUiLabel("Enter dice notation like 1d20+5 or 3d8.").c_str());
    }

    bool removePreset = false;
    if (ImGui::BeginTable("DicePresetActions", battleActionColumns(4, {"Roll", "Add Preset", "Update", "Remove"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        if (drawUiButton("Roll", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
            _controller.rollDiceExpression();
        ImGui::TableNextColumn();
        if (drawUiButton("Add Preset", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.addDiceRollPreset();
        ImGui::BeginDisabled(encounter.diceRollPresets.empty());
        ImGui::TableNextColumn();
        if (drawUiButton("Update", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.updateSelectedDiceRollPreset();
        ImGui::TableNextColumn();
        removePreset = drawUiButton("Remove", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f));
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    if (removePreset) ImGui::OpenPopup(localizedUiLabel("Remove Dice Preset").c_str());
    if (drawConfirmationModal("Remove Dice Preset", "Remove this dice roll preset?", "Remove"))
        _controller.removeSelectedDiceRollPreset();

    FormUi::InputTextMultiline(localizedUiLabel("Preset Notes").c_str(), &_controller._diceRollPresetNotes, ImVec2(-1.0f, 48.0f));
    if (!_controller._lastDiceRollMessage.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._lastDiceRollMessage.c_str());
}

void BattleTrackerView::drawCombatLogPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (!ImGui::CollapsingHeader(localizedUiLabel("Combat Log").c_str())) return;
    showHelpTooltip("Persistent encounter history for rounds, turns, rolls, damage, healing, conditions, movement, deaths, spells, and GM notes.");

    if (encounter.combatLog.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No combat log entries yet.").c_str());
    }
    else
    {
        const float logHeight = _controller._combatStep == 1 && !encounter.combatPaused ? 142.0f : 164.0f;
        if (ImGui::BeginTable("CombatLogEntries", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, logHeight)))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Rnd").c_str(), ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthFixed, 72.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Actor").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
            ImGui::TableSetupColumn(localizedUiLabel("Entry").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.76f);
            ImGui::TableHeadersRow();

            for (auto it = encounter.combatLog.rbegin(); it != encounter.combatLog.rend(); ++it)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(localizedUiLabel("%d").c_str(), it->round);
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(battleText(it->eventType.c_str()));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped(it->actorName.empty() ? "-" : it->actorName);
                ImGui::TableSetColumnIndex(3);
                tableTextWrapped(it->summary);
            }
            ImGui::EndTable();
        }
    }

    const float addNoteWidth = ImGui::CalcTextSize(localizedUiText("Add Note")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - addNoteWidth - ImGui::GetStyle().ItemSpacing.x));
    FormUi::InputText(localizedUiLabel("##CombatLogQuickNote").c_str(), &_controller._combatLogNote);
    ImGui::SameLine();
    if (ImGui::Button(localizedUiLabel("Add Note").c_str(), ImVec2(addNoteWidth, 0.0f)))
        _controller.addCombatLogNote();
    showHelpTooltip("Adds a quick GM note to the combat log without opening wrap-up notes.");
}

void BattleTrackerView::drawWrapUpPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (_wrapUpNotesEncounterId != encounter.metadata.id)
    {
        _wrapUpNotesEncounterId = encounter.metadata.id;
        _wrapUpNotesDirty = false;
    }
    const bool activeTurn = _controller._combatStep == 1 && !encounter.combatPaused;
    const bool wrapUpReady = encounter.combatPhase == CombatPhase::WrapUp || !encounter.afterCombatSummary.empty();
    const ImGuiTreeNodeFlags wrapUpFlags = wrapUpReady ? ImGuiTreeNodeFlags_DefaultOpen : 0;

    ImGui::SeparatorText(localizedUiLabel("Wrap-up").c_str());
    if (_focusWrapUp)
        ImGui::SetNextItemOpen(true);
    if (!ImGui::CollapsingHeader(localizedUiLabel("After Combat Summary").c_str(), wrapUpFlags))
        return;
    if (_focusWrapUp)
    {
        ImGui::SetScrollHereY(0.0f);
        _focusWrapUp = false;
    }

    if (activeTurn && !wrapUpReady)
    {
        ImGui::TextWrapped("%s", localizedUiText("Detailed loot, consequence, and session-note fields stay here until combat is paused or ending."));
        if (drawUiButton("Generate Summary", UiButtonRole::Advanced, ImVec2(180.0f, 0.0f), "Summarizes defeated enemies, XP, loot, story consequences, and table notes, then pauses the encounter for post-combat wrap-up."))
            _controller.generateAfterCombatSummary();
        if (!encounter.afterCombatSummary.empty())
            FormUi::InputTextMultiline(localizedUiLabel("Summary").c_str(), &encounter.afterCombatSummary, ImVec2(-1.0f, 96.0f), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
        return;
    }

    ImGui::TextUnformatted(localizedUiText("Loot"));
    _wrapUpNotesDirty |= FormUi::InputTextMultiline("##WrapUpLoot", &encounter.afterCombatLootNotes, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3.0f), ImGuiInputTextFlags_WordWrap);
    ImGui::TextUnformatted(localizedUiText("Story Consequences"));
    _wrapUpNotesDirty |= FormUi::InputTextMultiline("##WrapUpConsequences", &encounter.afterCombatStoryConsequences, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3.0f), ImGuiInputTextFlags_WordWrap);
    ImGui::TextUnformatted(localizedUiText("Session Notes"));
    _wrapUpNotesDirty |= FormUi::InputTextMultiline("##WrapUpSessionNotes", &encounter.afterCombatSessionNotes, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 3.0f), ImGuiInputTextFlags_WordWrap);
    if (_wrapUpNotesDirty)
        ImGui::TextWrapped("%s", localizedUiText("These notes have unsaved changes."));
    if (drawUiButton("Save notes and summary", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
    {
        _controller.generateAfterCombatSummary();
        if (!_controller._encounterSaveFailed) _wrapUpNotesDirty = false;
    }
    if (!encounter.afterCombatSummary.empty())
    {
        ImGui::TextUnformatted(localizedUiText("Summary"));
        FormUi::InputTextMultiline("##WrapUpSummary", &encounter.afterCombatSummary, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 5.0f), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
    }
}
