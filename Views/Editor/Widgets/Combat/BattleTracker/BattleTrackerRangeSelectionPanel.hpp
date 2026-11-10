    if (hasCombatControlsFocus(CombatControlsFocus::ActionTargets))
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    else if (hasCombatControlsFocus(CombatControlsFocus::ActionSelection))
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    if (beginActionResolutionSection("Step 1 - Choose Action###BattleActionStep1", 0, activeResolutionStep))
    {
        const bool focusActionSelection = consumeCombatControlsFocus(CombatControlsFocus::ActionSelection);
        if (focusActionSelection)
            ImGui::SetScrollHereY(0.0f);
        if (!attacker.actions.empty())
        {
            std::vector<const char *> actionLabels;
            for (const auto &action : attacker.actions)
                actionLabels.push_back(action.name.c_str());
            if (FormUi::Combo(localizedUiLabel("Action").c_str(), &_controller._selectedActionIndex, actionLabels.data(), static_cast<int>(actionLabels.size())))
                resetActionProgress();
            showHelpTooltip("Equipped character weapons and monster actions appear here.");
        }
        else
        {
            bool manualChanged = false;
            manualChanged |= FormUi::InputText(localizedUiLabel("Attack Name").c_str(), &_controller._manualActionName);
            manualChanged |= FormUi::InputInt(localizedUiLabel("Range in Feet").c_str(), &_controller._manualAttackRangeFeet);
            manualChanged |= FormUi::InputText(localizedUiLabel("Damage Formula").c_str(), &_controller._manualDamageFormula);
            showHelpTooltip("Example: 1d8 +3 piercing. The flat +3 is added automatically to the damage dice roll below.");
            _controller._manualAttackRangeFeet = std::max(0, _controller._manualAttackRangeFeet);
            if (manualChanged)
                resetActionProgress();
        }
        refreshActionState();

        const bool healingSpell = selectedActionIsSpell && hasSpellSummary &&
                                  spellSummary.resolution == SpellResolutionType::Healing;
        ImGui::TextDisabled(localizedUiLabel("Selected Action: %s").c_str(), selectedAction.name.empty() ? localizedUiText("<None>") : selectedAction.name.c_str());
        ImGui::TextDisabled(localizedUiLabel(healingSpell ? "Healing Formula: %s" : "Damage Formula: %s").c_str(),
                            selectedAction.damage.empty() ? localizedUiText("manual result") : selectedAction.damage.c_str());
        if (_controller._currentCampaign.ruleSet)
        {
            const RuleDiceSummary damageSummary = _controller._currentCampaign.ruleSet->summarizeDiceExpression(selectedAction.damage);
            if (damageSummary.valid)
                ImGui::TextDisabled(localizedUiLabel(healingSpell ? "Healing Range: %d-%d, avg %.1f" : "Damage Range: %d-%d, avg %.1f").c_str(), damageSummary.minimum, damageSummary.maximum, damageSummary.average);
        }
        if (!healingSpell && !selectedAction.attackBonus.empty())
            ImGui::TextDisabled(localizedUiLabel("Attack Bonus: %s").c_str(), selectedAction.attackBonus.c_str());
        if (healingSpell)
            ImGui::TextDisabled(localizedUiText("Resolution: Healing"));
        else if (!selectedAction.actionKind.empty() || !selectedAction.sourceLabel.empty())
            ImGui::TextDisabled(localizedUiLabel("Action: %s%s%s").c_str(),
                                localizedUiText(selectedAction.actionKind.empty() ? "Action" : selectedAction.actionKind.c_str()),
                                selectedAction.sourceLabel.empty() ? "" : localizedUiText(" from "),
                                selectedAction.sourceLabel.empty() ? "" : selectedAction.sourceLabel.c_str());
        if (!selectedAction.linkedResourceId.empty())
        {
            const std::vector<ResourceTrackerModel> *resources = _controller.getSelectedSourceResources();
            if (resources)
            {
                const auto trackerIt = std::find_if(resources->begin(), resources->end(), [&](const ResourceTrackerModel &tracker) {
                    return tracker.metadata.id == selectedAction.linkedResourceId;
                });
                if (trackerIt != resources->end())
                {
                    ResourceTrackerModel tracker = *trackerIt;
                    normalizeResourceTracker(tracker);
                    ImGui::TextDisabled(localizedUiLabel("Resource: %s %d/%d left").c_str(), tracker.name.c_str(), std::max(0, tracker.maximum - tracker.used), tracker.maximum);
                }
            }
        }
        if (!selectedAction.notes.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(selectedAction.notes.c_str()));

    }

    refreshActionState();
    if (hasCombatControlsFocus(CombatControlsFocus::ActionSelection))
        ImGui::SetNextItemOpen(false, ImGuiCond_Always);
    else if (hasCombatControlsFocus(CombatControlsFocus::ActionTargets))
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    if (beginActionResolutionSection("Step 2 - Choose Targets###BattleActionStep2", 1, activeResolutionStep))
    {
        const int previousPrimaryTarget = _controller._rangeTargetIndex;
        const bool primaryTargetChanged = FormUi::Combo(localizedUiLabel("Primary Target").c_str(), &_controller._rangeTargetIndex, participantLabels.data(), static_cast<int>(participantLabels.size()));
        if (primaryTargetChanged || _controller._selectedTargetIndices.empty() || previousPrimaryTarget != _controller._rangeTargetIndex)
        {
            _controller._selectedTargetIndices.clear();
            if (_controller._rangeTargetIndex != _controller._selectedParticipantIndex)
                _controller._selectedTargetIndices.push_back(_controller._rangeTargetIndex);
            resetActionProgress();
        }
        showHelpTooltip("The primary target drives the range line and the quick damage preview. Additional selected targets resolve at the same time.");
        refreshActionState();

        EncounterParticipantModel &target = encounter.participants[_controller._rangeTargetIndex];
        if (encounter.theaterOfMind)
            ImGui::Text(localizedUiLabel("Target Defense: Armor Class %d  Damage Reduction %d  Distance narrative").c_str(), target.armorClass, target.armorDamageReduction);
        else
            ImGui::Text(localizedUiLabel("Target Defense: Armor Class %d  Damage Reduction %d  Distance %d ft").c_str(), target.armorClass, target.armorDamageReduction, distance);
        showHelpTooltip("Armor Class is the number the attack total must meet or beat. Damage Reduction subtracts from damage after a hit.");
        ImGui::Text(localizedUiLabel("%s").c_str(), localizedUiText(enforceGridRange ? (targetInRange ? "Target is in range." : "Target is out of range.")
                                                                                      : "Range is GM-adjudicated."));
        if (targetBeyondNormalRange)
            ImGui::TextDisabled(localizedUiLabel("Target is beyond normal range; D&D ranged attacks usually need disadvantage adjudication.").c_str());

        for (int targetIndex = 0; targetIndex < static_cast<int>(encounter.participants.size()); ++targetIndex)
        {
            if (targetIndex == _controller._selectedParticipantIndex)
                continue;
            EncounterParticipantModel &candidate = encounter.participants[targetIndex];
            bool selected = std::find(_controller._selectedTargetIndices.begin(), _controller._selectedTargetIndices.end(), targetIndex) != _controller._selectedTargetIndices.end();
            if (FormUi::Checkbox((candidate.displayName + "##Target" + std::to_string(targetIndex)).c_str(), &selected))
            {
                if (selected)
                {
                    if (std::find(_controller._selectedTargetIndices.begin(), _controller._selectedTargetIndices.end(), targetIndex) == _controller._selectedTargetIndices.end())
                        _controller._selectedTargetIndices.push_back(targetIndex);
                    _controller._rangeTargetIndex = targetIndex;
                }
                else
                {
                    _controller._selectedTargetIndices.erase(std::remove(_controller._selectedTargetIndices.begin(), _controller._selectedTargetIndices.end(), targetIndex), _controller._selectedTargetIndices.end());
                }
                resetActionProgress();
            }
        }
        refreshActionState();
        ImGui::TextDisabled(localizedUiLabel("Selected Targets: %d").c_str(), static_cast<int>(selectedTargets.size()));

        if (consumeCombatControlsFocus(CombatControlsFocus::ActionTargets))
            ImGui::SetScrollHereY(0.30f);
    }

    refreshActionState();
