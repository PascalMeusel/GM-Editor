#include "BattleTrackerViewActionGuideSupport.hpp"

void BattleTrackerView::drawSelectedCombatantPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (_controller._selectedParticipantIndex >= 0 && _controller._selectedParticipantIndex < static_cast<int>(encounter.participants.size()))
    {
        EncounterParticipantModel &selectedParticipant = encounter.participants[_controller._selectedParticipantIndex];
        ImGui::SeparatorText(localizedUiLabel("Selected Combatant").c_str());
        const bool selectedIsActive = _controller._combatStep == 1 && _controller._selectedParticipantIndex == encounter.activeParticipantIndex;
        const int selectedMovementLeft = selectedIsActive ? std::max(0, selectedParticipant.speedFeet - selectedParticipant.movementUsedFeet) : selectedParticipant.speedFeet;
        CharacterSpellcastingModel *summarySpellcasting = _controller.getSelectedSourceSpellcasting();
        int totalSlotsLeft = 0;
        if (summarySpellcasting && summarySpellcasting->enabled)
        {
            normalizeSpellcasting(*summarySpellcasting);
            for (int spellLevel = 1; spellLevel <= 9; ++spellLevel)
                totalSlotsLeft += std::max(0, summarySpellcasting->slotMaximums[static_cast<std::size_t>(spellLevel)] -
                                                  summarySpellcasting->slotUsed[static_cast<std::size_t>(spellLevel)]);
        }
        std::vector<ResourceTrackerModel> *summaryResources = _controller.getSelectedSourceResources();
        int totalResourcesLeft = 0;
        if (summaryResources)
        {
            for (ResourceTrackerModel tracker : *summaryResources)
            {
                normalizeResourceTracker(tracker);
                totalResourcesLeft += std::max(0, tracker.maximum - tracker.used);
            }
        }

        UiStatusActionStrip selectedStrip;
        const std::string selectedMode = battleText(selectedIsActive ? "Active turn" : "Inspecting");
        const std::string temporaryHitPoints = selectedParticipant.temporaryHitPoints > 0
                                                   ? battleFormat(" +{0} temp", {std::to_string(selectedParticipant.temporaryHitPoints)})
                                                   : std::string{};
        const std::string selectedSummary = battleFormat("{0} | {1}/{2} HP{3} | AC {4}",
                                                         {selectedParticipant.displayName,
                                                          std::to_string(selectedParticipant.currentHitPoints),
                                                          std::to_string(selectedParticipant.maxHitPoints),
                                                          temporaryHitPoints,
                                                          std::to_string(selectedParticipant.armorClass)});
        const std::string selectedNext = battleFormat("{0} conditions, {1} effects, {2} slots left, {3} resources left, {4} ft movement.",
                                                      {std::to_string(selectedParticipant.conditions.size()),
                                                       std::to_string(selectedParticipant.activeEffects.size()),
                                                       std::to_string(totalSlotsLeft),
                                                       std::to_string(totalResourcesLeft),
                                                       std::to_string(selectedMovementLeft)});
        selectedStrip.mode = selectedMode.c_str();
        selectedStrip.selection = selectedSummary.c_str();
        selectedStrip.nextAction = selectedNext.c_str();
        selectedStrip.help = "Open the drawer for temporary HP, conditions, active effects, spell slots, resources, and row-level combat state.";
        drawStatusActionStrip("BattleTrackerSelectedCombatantStrip", selectedStrip);

        // Manual table adjudication uses the existing rules, health syncing,
        // combat log and retryable encounter save, without changing turn phase.
        if (ImGui::TreeNodeEx(localizedUiLabel("Damage / Heal##SelectedCombatantHealth").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SetNextItemWidth(100.0f);
            FormUi::InputInt(localizedUiLabel("Amount").c_str(), &_controller._pendingHitPointDelta, 0, 0);
            _controller._pendingHitPointDelta = std::clamp(_controller._pendingHitPointDelta, 0, 1000000);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputTextWithHint("##SelectedDamageType", localizedUiText("Damage Type"), &_controller._damageTypeDraft);
            const bool hasAmount = _controller._pendingHitPointDelta > 0;
            int healthDelta = 0;
            ImGui::BeginDisabled(!hasAmount);
            if (ImGui::BeginTable("SelectedHealthActions", battleActionColumns(2, {"Apply Damage", "Heal"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableNextColumn();
                if (drawUiButton("Apply Damage", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                    healthDelta = -_controller._pendingHitPointDelta;
                ImGui::TableNextColumn();
                if (drawUiButton("Heal", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                    healthDelta = _controller._pendingHitPointDelta;
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
            ImGui::TreePop();
            if (healthDelta != 0)
            {
                _controller.applyHitPointDeltaToSelected(healthDelta);
                _controller.saveEncounter();
                return; // Saving may normalize/reallocate participant storage.
            }
        }

        if (ImGui::CollapsingHeader(localizedUiLabel("Selected Combatant Details##SelectedCombatantDrawer").c_str()))
        {
        ImGui::Text(localizedUiLabel("%s").c_str(), selectedParticipant.displayName.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(combatantKindLabel(selectedParticipant.kind)));
        if (!selectedParticipant.groupName.empty() || selectedParticipant.waveNumber > 1 || selectedParticipant.startsHidden || selectedParticipant.reinforcementRound > 0)
        {
            std::string prepLine = battleFormat("Wave {0}", {std::to_string(selectedParticipant.waveNumber)});
            if (!selectedParticipant.groupName.empty())
                prepLine = selectedParticipant.groupName + "  " + prepLine;
            if (selectedParticipant.startsHidden)
                prepLine += "  " + battleText("Hidden");
            if (selectedParticipant.reinforcementRound > 0)
                prepLine += "  " + battleFormat("Reinforces round {0}", {std::to_string(selectedParticipant.reinforcementRound)});
            ImGui::TextDisabled(localizedUiLabel("Prep: %s").c_str(), prepLine.c_str());
        }
        ImGui::SetNextItemWidth(120.0f);
        int temporaryHp = selectedParticipant.temporaryHitPoints;
        if (FormUi::InputInt(localizedUiLabel("Temporary HP").c_str(), &temporaryHp, 0, 0))
            _controller.setSelectedTemporaryHitPoints(temporaryHp);
        if (!selectedParticipant.damageResistances.empty() || !selectedParticipant.damageVulnerabilities.empty() || !selectedParticipant.damageImmunities.empty())
        {
            const auto join = [](const std::vector<std::string> &values) {
                std::string text;
                for (const std::string &value : values)
                {
                    if (!text.empty())
                        text += ", ";
                    text += battleText(value.c_str());
                }
                return text;
            };
            if (!selectedParticipant.damageResistances.empty())
                ImGui::TextDisabled(localizedUiLabel("Resists: %s").c_str(), join(selectedParticipant.damageResistances).c_str());
            if (!selectedParticipant.damageVulnerabilities.empty())
                ImGui::TextDisabled(localizedUiLabel("Vulnerable: %s").c_str(), join(selectedParticipant.damageVulnerabilities).c_str());
            if (!selectedParticipant.damageImmunities.empty())
                ImGui::TextDisabled(localizedUiLabel("Immune: %s").c_str(), join(selectedParticipant.damageImmunities).c_str());
        }

        if (ImGui::TreeNodeEx(localizedUiLabel("Conditions").c_str()))
        {
            if (selectedParticipant.conditions.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("No active conditions.").c_str());
            }
            else
            {
                int pendingRemovedCondition = -1;
                for (int i = 0; i < static_cast<int>(selectedParticipant.conditions.size()); ++i)
                {
                    ImGui::PushID(i);
                    const std::string localizedCondition = localizedConditionText(selectedParticipant.conditions[static_cast<std::size_t>(i)]);
                    ImGui::BulletText("%s", localizedCondition.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(localizedUiText("Remove")))
                        pendingRemovedCondition = i;
                    ImGui::PopID();
                }
                if (pendingRemovedCondition >= 0)
                    _controller.removeConditionFromSelectedParticipant(pendingRemovedCondition);
            }

            ImGui::SetNextItemWidth(220.0f);
            FormUi::InputText(localizedUiLabel("Add Condition").c_str(), &_controller._conditionDraft);
            ImGui::SameLine();
            if (ImGui::Button(localizedUiLabel("Add").c_str()))
                _controller.addConditionToSelectedParticipant();
            showHelpTooltip("Adds a named condition to this combatant. Player and NPC conditions sync back to their source sheet.");
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx(localizedUiLabel("Active Effects").c_str()))
        {
            if (selectedParticipant.activeEffects.empty())
            {
                ImGui::TextDisabled(localizedUiLabel("No active effects.").c_str());
            }
            else if (ImGui::BeginTable("SelectedCombatantActiveEffects", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Effect").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.26f);
                ImGui::TableSetupColumn(localizedUiLabel("Kind").c_str(), ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Source").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.18f);
                ImGui::TableSetupColumn(localizedUiLabel("Rounds").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();
                int pendingRemovedEffect = -1;
                for (int effectIndex = 0; effectIndex < static_cast<int>(selectedParticipant.activeEffects.size()); ++effectIndex)
                {
                    const ActiveEffectModel &effect = selectedParticipant.activeEffects[static_cast<std::size_t>(effectIndex)];
                    ImGui::PushID(effectIndex);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    const std::string localizedEffectName = effect.kind == ActiveEffectKind::Condition
                                                                ? localizedConditionText(effect.name)
                                                                : effect.name;
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedEffectName.c_str());
                    if (!effect.notes.empty())
                        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(effect.notes.c_str()));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text(localizedUiLabel("%s").c_str(), localizedUiText(activeEffectKindLabel(effect.kind)));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), effect.sourceName.empty() ? "-" : effect.sourceName.c_str());
                    ImGui::TableSetColumnIndex(3);
                    if (effect.remainingRounds < 0)
                        ImGui::TextDisabled(localizedUiLabel("-").c_str());
                    else
                        ImGui::Text(localizedUiLabel("%d").c_str(), effect.remainingRounds);
                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::SmallButton(localizedUiText("Remove")))
                        pendingRemovedEffect = effectIndex;
                    ImGui::PopID();
                }
                ImGui::EndTable();
                if (pendingRemovedEffect >= 0)
                    _controller.removeActiveEffectFromSelectedParticipant(pendingRemovedEffect);
            }
            showHelpTooltip("Structured buffs, debuffs, conditions, auras, summoned zones, and timed spell effects tracked on this combatant.");
            ImGui::TreePop();
        }

        CharacterSpellcastingModel *spellcasting = _controller.getSelectedSourceSpellcasting();
        if (spellcasting && spellcasting->enabled && ImGui::TreeNodeEx(localizedUiLabel("Spell Slots").c_str()))
        {
            normalizeSpellcasting(*spellcasting);
            if (ImGui::BeginTable("SelectedCombatantSpellSlots", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Level").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Max").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Used").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Left").c_str());
                ImGui::TableHeadersRow();
                for (int spellLevel = 1; spellLevel <= 9; ++spellLevel)
                {
                    ImGui::PushID(spellLevel);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text(localizedUiLabel("%d").c_str(), spellLevel);
                    ImGui::TableSetColumnIndex(1);
                    int maximum = spellcasting->slotMaximums[static_cast<std::size_t>(spellLevel)];
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Max").c_str(), &maximum))
                        _controller.setSelectedSpellSlotMaximum(spellLevel, maximum);
                    ImGui::TableSetColumnIndex(2);
                    int used = spellcasting->slotUsed[static_cast<std::size_t>(spellLevel)];
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                        _controller.setSelectedSpellSlotUsed(spellLevel, used);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text(localizedUiLabel("%d").c_str(), std::max(0, maximum - used));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            showHelpTooltip("Direct combat slot controls sync back to the source character or NPC.");
            ImGui::TreePop();
        }

        std::vector<ResourceTrackerModel> *resources = _controller.getSelectedSourceResources();
        if (resources && !resources->empty() && ImGui::TreeNodeEx(localizedUiLabel("Resources").c_str()))
        {
            if (ImGui::BeginTable("SelectedCombatantResources", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Resource").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
                ImGui::TableSetupColumn(localizedUiLabel("Used").c_str(), ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthFixed, 58.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Recharge").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.2f);
                ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 64.0f);
                ImGui::TableHeadersRow();
                for (int trackerIndex = 0; trackerIndex < static_cast<int>(resources->size()); ++trackerIndex)
                {
                    ResourceTrackerModel tracker = (*resources)[static_cast<std::size_t>(trackerIndex)];
                    normalizeResourceTracker(tracker);
                    ImGui::PushID(trackerIndex);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), tracker.name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    int used = tracker.used;
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                        _controller.setSelectedSourceResourceUsed(trackerIndex, used);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text(localizedUiLabel("%d").c_str(), std::max(0, tracker.maximum - tracker.used));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(tracker.rechargeRule.c_str()));
                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::SmallButton(localizedUiText("Reset")))
                        _controller.resetSelectedSourceResource(trackerIndex);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            showHelpTooltip("Combat resource controls sync back to the source character or NPC. Per-turn and per-round resources reset when their turn starts.");
            ImGui::TreePop();
        }
        }
    }

}
