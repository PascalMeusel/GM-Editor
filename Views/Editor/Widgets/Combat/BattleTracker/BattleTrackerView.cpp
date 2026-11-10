#include "BattleTrackerViewActionGuideSupport.hpp"

BattleTrackerView::BattleTrackerView(BattleTrackerController &controller)
    : DockableWidget("Battle Tracker", 0, ImVec2(1180.0f, 760.0f)), _controller(controller)
{
}

void BattleTrackerView::drawEmbedded()
{
    draw();
}

void BattleTrackerView::changeCombatPhaseAndFocus(CombatPhase phase)
{
    _compactBattlefield = phase == CombatPhase::Movement;
    _controller.changeCombatPhase(phase);
    if (phase == CombatPhase::ActionResolution)
    {
        // Always begin with the chooser. A default weapon must not make the
        // Action control skip past the spell or special action the GM needs.
        _pendingCombatControlsFocus = CombatControlsFocus::ActionSelection;
    }
    else if (phase == CombatPhase::ReactionsEffects)
    {
        _pendingCombatControlsFocus = CombatControlsFocus::Effects;
    }
    else
    {
        _pendingCombatControlsFocus = CombatControlsFocus::None;
    }
}

bool BattleTrackerView::hasCombatControlsFocus(CombatControlsFocus focus) const
{
    return _pendingCombatControlsFocus == focus;
}

bool BattleTrackerView::consumeCombatControlsFocus(CombatControlsFocus focus)
{
    if (!hasCombatControlsFocus(focus))
        return false;
    _pendingCombatControlsFocus = CombatControlsFocus::None;
    return true;
}

void BattleTrackerView::draw()
{
    normalizeEncounter(_controller._draftEncounter);

    EncounterModel &encounter = _controller._draftEncounter;
    std::string selection = battleText("No combatants");
    if (!encounter.participants.empty())
    {
        const EncounterParticipantModel &active = encounter.participants[encounter.activeParticipantIndex];
        selection = battleFormat("Active: {0} ({1}/{2} HP)",
                                 {active.displayName,
                                  std::to_string(active.currentHitPoints),
                                  std::to_string(active.maxHitPoints)});
        if (_controller._dragParticipantIndex >= 0 && _controller._dragParticipantIndex < static_cast<int>(encounter.participants.size()))
        {
            const EncounterParticipantModel &moving = encounter.participants[_controller._dragParticipantIndex];
            selection = battleFormat("moving: {0} ({1}, {2})",
                                     {moving.displayName, std::to_string(moving.x), std::to_string(moving.y)});
        }
    }
    ImGui::TextWrapped("%s", localizedUiText(combatPhaseNextAction(encounter)));
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(selection.c_str());
        ImGui::TextWrapped("%s", localizedUiText(combatPhaseNextAction(encounter)));
        if (!_controller._combatStatusMessage.empty())
            ImGui::TextWrapped("%s", _controller._combatStatusMessage.c_str());
        ImGui::EndTooltip();
    }
    if (_lastVisibleCombatStatus != _controller._combatStatusMessage)
    {
        _lastVisibleCombatStatus = _controller._combatStatusMessage;
        _combatStatusChangedAt = ImGui::GetTime();
    }
    drawStableTransientStatus("##CombatTransientStatus", _controller._combatStatusMessage.c_str(),
                              ImGui::GetTime() - _combatStatusChangedAt < 6.0);
    if (!_controller._combatErrorMessage.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", _controller._combatErrorMessage.c_str());
        ImGui::PopStyleColor();
    }
    if (_controller._encounterSaveFailed && drawUiButton("Retry saving", UiButtonRole::Primary))
    {
        _controller.saveEncounter();
        if (!_controller._encounterSaveFailed) _controller._combatErrorMessage.clear();
    }

    const auto drawCombatControls = [&]() {
        const bool setupFirst = encounter.combatPhase == CombatPhase::Setup || encounter.participants.empty();
        drawTurnActions();
        drawEffectsReviewPanel();
        drawRangePanel();
        // Keep setup in place while adding participants. Snapshot the choice
        // before controls mutate the roster so setup is submitted only once.
        // Later phases retain their initiative/active-participant priority.
        if (setupFirst) drawSetupPanel();
        drawParticipantPanel();
        drawSelectedCombatantPanel();
        if (!setupFirst) drawSetupPanel();
        drawDiceRollPanel();
        drawCombatLogPanel();
        drawWrapUpPanel();
    };

    // Keep the active turn and corrections accessible while either pane scrolls.
    drawPersistentCombatNavigation();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float textScale = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const bool compact = availableWidth < 980.0f * textScale;
    if (!compact &&
        ImGui::BeginTable("BattleTrackerLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        const float controlsWidth = std::clamp(availableWidth * 0.48f, 470.0f * textScale, 640.0f * textScale);
        ImGui::TableSetupColumn(localizedUiLabel("Combat Controls").c_str(), ImGuiTableColumnFlags_WidthFixed, controlsWidth);
        ImGui::TableSetupColumn(localizedUiLabel("Battlefield").c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::BeginChild("CombatControlsScroll", ImVec2(0, 0));
        drawCombatControls();
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("BattlefieldScroll", ImVec2(0, 0));
        drawBattlefield();
        ImGui::EndChild();
        ImGui::EndTable();
    }
    else
    {
        // A single pane selector avoids another tab layer inside the workspace.
        if (ImGui::BeginTable("BattleTrackerCompactPanes", 2,
                             ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton(uiTextForLocale("Controls", "Steuerung"),
                             _compactBattlefield ? UiButtonRole::Secondary : UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
                _compactBattlefield = false;
            ImGui::TableNextColumn();
            if (drawUiButton(uiTextForLocale("Map", "Karte"),
                             _compactBattlefield ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                _compactBattlefield = true;
            ImGui::EndTable();
        }
        ImGui::BeginChild(_compactBattlefield ? "BattleTrackerCompactMap" : "BattleTrackerCompactControls", ImVec2(0.0f, 0.0f));
        if (_compactBattlefield)
            drawBattlefield();
        else
            drawCombatControls();
        ImGui::EndChild();
    }
}

int BattleTrackerView::adjudicationModifierCount() const
{
    int count = 0;
    if (_controller._attackHasAdvantage || _controller._attackHasDisadvantage)
        ++count;
    if (_controller._targetHasCover)
        ++count;
    if (_controller._targetProne)
        ++count;
    if (_controller._targetUnseen)
        ++count;
    if (_controller._targetHidden)
        ++count;
    if (!_controller._gmAdjudicationNotes.empty())
        ++count;
    return count;
}

std::string BattleTrackerView::adjudicationSummaryText() const
{
    std::vector<std::string> modifiers;
    if (_controller._attackHasAdvantage && !_controller._attackHasDisadvantage)
        modifiers.push_back(battleText("Adv"));
    else if (_controller._attackHasDisadvantage && !_controller._attackHasAdvantage)
        modifiers.push_back(battleText("Dis"));
    else if (_controller._attackHasAdvantage && _controller._attackHasDisadvantage)
        modifiers.push_back(battleText("Adv/Dis cancel"));
    if (_controller._targetHasCover)
        modifiers.push_back(battleText("Cover +2 AC"));
    if (_controller._targetProne)
        modifiers.push_back(battleText("Prone"));
    if (_controller._targetUnseen)
        modifiers.push_back(battleText("Unseen"));
    if (_controller._targetHidden)
        modifiers.push_back(battleText("Hidden"));
    if (!_controller._gmAdjudicationNotes.empty())
        modifiers.push_back(battleText("GM note"));

    if (modifiers.empty())
        return battleText("Mods: none");

    std::string summary = battleText("Mods: ");
    for (std::size_t index = 0; index < modifiers.size(); ++index)
    {
        if (index > 0)
            summary += ", ";
        summary += modifiers[index];
    }
    return summary;
}

std::string BattleTrackerView::adjudicationSummaryTooltip() const
{
    if (adjudicationModifierCount() == 0)
        return battleText("No GM adjudication modifiers are active.");

    std::string tooltip = adjudicationSummaryText();
    tooltip += "\n" + battleText("Advantage/disadvantage changes the d20 result. Cover adds +2 to target AC. Prone, unseen, hidden, and notes are logged as GM context.");
    if (!_controller._gmAdjudicationNotes.empty())
        tooltip += "\n" + battleFormat("GM note: {0}", {_controller._gmAdjudicationNotes});
    return tooltip;
}

void BattleTrackerView::drawAttackTotalSummary(const char *bonusLabel) const
{
    const std::string totalText = battleFormat("{0}: {1}  Attack Total: {2}",
                                               {battleText(bonusLabel),
                                                signedNumber(_controller.getSelectedAttackModifier()),
                                                std::to_string(_controller.getAttackTotal())});
    std::string modifierText = adjudicationSummaryText();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float totalWidth = ImGui::CalcTextSize(totalText.c_str()).x;
    float modifierWidth = ImGui::CalcTextSize(modifierText.c_str()).x;
    if (adjudicationModifierCount() > 0 && totalWidth + ImGui::GetStyle().ItemSpacing.x + modifierWidth > availableWidth)
    {
        modifierText = battleFormat("Mods: {0} active", {std::to_string(adjudicationModifierCount())});
        modifierWidth = ImGui::CalcTextSize(modifierText.c_str()).x;
    }
    const bool sameLine = totalWidth + ImGui::GetStyle().ItemSpacing.x + modifierWidth <= availableWidth;

    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), totalText.c_str());
    if (sameLine)
        ImGui::SameLine();

    if (adjudicationModifierCount() > 0)
        ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.42f, 1.0f), localizedUiLabel("%s").c_str(), modifierText.c_str());
    else
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), modifierText.c_str());
    const std::string tooltip = adjudicationSummaryTooltip();
    showHelpTooltip(tooltip.c_str());
}
