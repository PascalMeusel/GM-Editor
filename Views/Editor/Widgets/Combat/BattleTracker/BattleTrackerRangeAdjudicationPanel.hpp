    if (beginActionResolutionSection("Step 3 - Adjudicate###BattleActionStep3", 2, activeResolutionStep))
    {
        bool adjudicationChanged = false;
        adjudicationChanged |= FormUi::Checkbox(localizedUiLabel("Advantage").c_str(), &_controller._attackHasAdvantage);
        ImGui::SameLine();
        adjudicationChanged |= FormUi::Checkbox(localizedUiLabel("Disadvantage").c_str(), &_controller._attackHasDisadvantage);
        adjudicationChanged |= FormUi::Checkbox(localizedUiLabel("Cover (+2 AC)").c_str(), &_controller._targetHasCover);
        ImGui::SameLine();
        adjudicationChanged |= FormUi::Checkbox(localizedUiLabel("Prone").c_str(), &_controller._targetProne);
        adjudicationChanged |= FormUi::Checkbox(localizedUiLabel("Unseen").c_str(), &_controller._targetUnseen);
        ImGui::SameLine();
        adjudicationChanged |= FormUi::Checkbox(localizedUiLabel("Hidden").c_str(), &_controller._targetHidden);
        adjudicationChanged |= FormUi::InputText(localizedUiLabel("GM Note").c_str(), &_controller._gmAdjudicationNotes);
        showHelpTooltip("Use toggles for common table adjudication. Advantage/disadvantage changes the d20 result; cover increases target AC by 2; the other states are logged as context.");
        if (adjudicationChanged)
            resetActionProgress();
        refreshActionState();

        std::string adjudicationReason;
        if (selectedAction.name.empty())
            adjudicationReason = battleText("Choose an action first.");
        else if (selectedTargets.empty())
            adjudicationReason = battleText("Select at least one target.");
        else if (!allTargetsInRange)
            adjudicationReason = battleText("One or more selected targets are out of range.");
        const bool canContinueToRoll = adjudicationReason.empty();
        if (drawUiButton("Continue to Roll / Save",
                         canContinueToRoll,
                         adjudicationReason.c_str(),
                         UiButtonRole::Primary,
                         ImVec2(196.0f, 0.0f),
                         "Marks GM adjudication reviewed and focuses the roll/save step."))
        {
            _controller._actionAdjudicationReviewed = true;
            refreshActionState();
        }
    }

    refreshActionState();
    if (beginActionResolutionSection("Step 4 - Roll / Save###BattleActionStep4", 3, activeResolutionStep))
    {
        if (!_controller._actionAdjudicationReviewed)
        {
            ImGui::TextDisabled(localizedUiLabel("Review GM adjudication before entering rolls.").c_str());
        }
        else if (selectedActionIsSpell)
        {
            const std::string spellSlotStatus = _controller.selectedSpellSlotStatus();
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), spellSlotStatus.c_str());
            if (hasSpellSummary)
            {
                ImGui::TextDisabled(localizedUiLabel("Spell Resolution: %s").c_str(), localizedUiText(spellResolutionLabel(spellSummary.resolution)));
                if (!spellSummary.savingThrowAbility.empty())
                    ImGui::TextDisabled(localizedUiLabel("Save DC: %s %d").c_str(), localizedUiText(spellSummary.savingThrowAbility.c_str()), spellSummary.saveDc);
                if (!spellSummary.area.shape.empty())
                {
                    const std::string areaText = spellSummary.area.rawText.empty() ? "" : " (" + battleText(spellSummary.area.rawText.c_str()) + ")";
                    ImGui::TextDisabled(localizedUiLabel("Area Template: %s %d ft%s").c_str(), localizedUiText(spellSummary.area.shape.c_str()), spellSummary.area.sizeFeet, areaText.c_str());
                }
                if (spellSummary.concentration)
                    ImGui::TextDisabled(localizedUiLabel("Concentration spell").c_str());
            }
            if (_controller.selectedSpellCanBeCastAsRitual())
            {
                FormUi::Checkbox(localizedUiLabel("Cast as Ritual").c_str(), &_controller._castSelectedSpellAsRitual);
                showHelpTooltip("Ritual casting records the cast without spending a spell slot.");
            }
            else
            {
                _controller._castSelectedSpellAsRitual = false;
            }

            if (hasSpellSummary && spellSummary.resolution == SpellResolutionType::SpellAttack)
            {
                FormUi::InputInt(localizedUiLabel("Spell Attack D20").c_str(), &_controller._attackRollResult);
                if (_controller._attackHasAdvantage != _controller._attackHasDisadvantage)
                    FormUi::InputInt(localizedUiLabel("Second D20").c_str(), &_controller._secondAttackRollResult);
                showHelpTooltip("Enter the raw d20 result. The caster's spell attack bonus is added below.");
                drawAttackTotalSummary("Spell Attack Bonus");
            }
            else if (hasSpellSummary && spellSummary.resolution == SpellResolutionType::SavingThrow)
            {
                _controller.ensureSavingThrowInputs();
                ImGui::TextDisabled(localizedUiLabel("Target saves: %s DC %d. Ties meet the DC and count as a success.").c_str(),
                                    localizedUiText(spellSummary.savingThrowAbility.empty() ? "Save" : spellSummary.savingThrowAbility.c_str()),
                                    spellSummary.saveDc);
                showHelpTooltip("Enter each selected target's save roll and modifier before resolving. Only failed saves receive automatic damage or ongoing effects.");
                if (selectedTargets.empty())
                {
                    ImGui::TextDisabled(localizedUiLabel("Select targets to preview saving throw outcomes.").c_str());
                }
                else if (ImGui::BeginTable("SpellSaveTargetPreview", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Target").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
                    ImGui::TableSetupColumn(localizedUiLabel("Roll").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
                    ImGui::TableSetupColumn(localizedUiLabel("Result").c_str(), ImGuiTableColumnFlags_WidthFixed, 88.0f);
                    ImGui::TableSetupColumn(localizedUiLabel("Preview").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.46f);
                    ImGui::TableHeadersRow();
                    for (int targetIndex : selectedTargets)
                    {
                        if (targetIndex < 0 || targetIndex >= static_cast<int>(encounter.participants.size()))
                            continue;
                        EncounterParticipantModel &saveTarget = encounter.participants[targetIndex];
                        ImGui::PushID(targetIndex);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), saveTarget.displayName.c_str());
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(58.0f);
                        FormUi::InputInt(localizedUiLabel("##SaveD20").c_str(), &_controller._targetSavingThrowRollResults[static_cast<std::size_t>(targetIndex)], 0, 0);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(58.0f);
                        FormUi::InputInt(localizedUiLabel("##SaveMod").c_str(), &_controller._targetSavingThrowModifiers[static_cast<std::size_t>(targetIndex)], 0, 0);
                        const int saveTotal = _controller.getSavingThrowTotalForTarget(targetIndex);
                        const bool failedSave = saveTotal < spellSummary.saveDc;
                        ImGui::TableNextColumn();
                        ImGui::Text(localizedUiLabel("%d total").c_str(), saveTotal);
                        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(failedSave ? "Fails" : "Succeeds"));
                        ImGui::TableNextColumn();
                        const std::string preview = _controller.previewSpellOutcomeForTarget(targetIndex, spellSummary, failedSave);
                        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), preview.c_str());
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
            else if (hasSpellSummary && spellSummary.resolution == SpellResolutionType::Healing)
            {
                ImGui::TextDisabled(localizedUiLabel("Healing spells resolve in Apply / Log after you confirm targets.").c_str());
            }
        }
        else
        {
            FormUi::InputInt(localizedUiLabel("D20 Roll").c_str(), &_controller._attackRollResult);
            if (_controller._attackHasAdvantage != _controller._attackHasDisadvantage)
                FormUi::InputInt(localizedUiLabel("Second D20").c_str(), &_controller._secondAttackRollResult);
            showHelpTooltip("Enter the raw d20 result from the table. The attack bonus is added below.");
            drawAttackTotalSummary("Attack Bonus");
            std::string checkHitReason;
            if (selectedTargets.empty())
                checkHitReason = battleText("Select at least one target.");
            else if (!allTargetsInRange)
                checkHitReason = battleText("One or more selected targets are out of range.");
            else if (!_controller._actionAdjudicationReviewed)
                checkHitReason = battleText("Review GM adjudication first.");
            if (drawUiButton("Check if Hit",
                             !selectedTargets.empty() && allTargetsInRange && _controller._actionAdjudicationReviewed,
                             checkHitReason.c_str(),
                             UiButtonRole::Primary,
                             ImVec2(120.0f, 0.0f),
                             "Checks the attack roll against the selected target defense and logs the result."))
                _controller.resolveAttackRoll();
            if (_controller._lastAttackResolved)
            {
                ImGui::SameLine();
                ImGui::Text(localizedUiLabel("%s").c_str(), _controller._lastAttackMessage.c_str());
            }
        }
    }

    refreshActionState();
