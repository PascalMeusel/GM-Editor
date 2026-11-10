    if (beginActionResolutionSection("Step 5 - Apply / Log###BattleActionStep5", 4, activeResolutionStep))
    {
        EncounterParticipantModel &target = encounter.participants[_controller._rangeTargetIndex];
        if (FormUi::InputText(localizedUiLabel("Damage Type").c_str(), &_controller._damageTypeDraft))
            _controller._lastAttackMessage.clear();
        showHelpTooltip("Optional override for resistance, vulnerability, immunity, temporary HP, and concentration checks. Leave blank to infer from the action damage text.");

        if (selectedActionIsSpell)
        {
            if (hasSpellSummary && (spellSummary.resolution == SpellResolutionType::SpellAttack || spellSummary.resolution == SpellResolutionType::SavingThrow || spellSummary.resolution == SpellResolutionType::Healing))
            {
                const bool healingSpell = spellSummary.resolution == SpellResolutionType::Healing;
                if (healingSpell && drawSmallUiButton("Roll healing", UiButtonRole::Secondary,
                                                       "Rolls the healing formula and keeps the dice result editable."))
                    _controller.rollSelectedSpellHealing();
                if (healingSpell)
                    ImGui::SameLine();
                if (FormUi::InputInt(localizedUiText(healingSpell ? "Healing Dice Result" : "Damage Dice Result"), &_controller._damageRollResult) && healingSpell)
                    _controller._healingDiceEntered = true;
                showHelpTooltip(localizedUiText(healingSpell ? "Enter only the rolled healing dice total. Flat healing modifiers are added automatically." : "Enter only the rolled damage dice total. Flat damage modifiers are added automatically."));
                const int modifier = _controller.getSelectedDamageModifier();
                if (healingSpell)
                    ImGui::TextDisabled(localizedUiLabel("Healing Modifier: %+d  Healing Applied: %d").c_str(), modifier, _controller.getSpellHealingTotal());
                else
                    ImGui::TextDisabled(localizedUiLabel("Damage Modifier: %+d  Damage Reduction: -%d  Damage Applied: %d").c_str(), modifier, target.armorDamageReduction, _controller.getArmorAdjustedDamageTotal());
            }

            if (hasSpellSummary && spellSummary.resolution == SpellResolutionType::Healing && !selectedTargets.empty())
            {
                if (ImGui::BeginTable("SpellHealingTargetPreview", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Target").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
                    ImGui::TableSetupColumn(localizedUiLabel("Preview").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.66f);
                    ImGui::TableHeadersRow();
                    for (int targetIndex : selectedTargets)
                    {
                        if (targetIndex < 0 || targetIndex >= static_cast<int>(encounter.participants.size()))
                            continue;
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), encounter.participants[targetIndex].displayName.c_str());
                        ImGui::TableNextColumn();
                        const std::string preview = _controller.previewSpellOutcomeForTarget(targetIndex, spellSummary, true);
                        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), preview.c_str());
                    }
                    ImGui::EndTable();
                }
            }

            std::string spellSlotReason;
            const bool spellSlotReady = _controller.canCastSelectedSpell(&spellSlotReason);
            std::string disabledReason;
            if (!_controller._actionAdjudicationReviewed)
                disabledReason = battleText("Review GM adjudication first.");
            else if (!hasSpellSummary)
                disabledReason = battleText("Selected spell has no combat resolution details.");
            else if (selectedTargets.empty())
                disabledReason = battleText("Select at least one target.");
            else if (!allTargetsInRange)
                disabledReason = battleText("One or more selected targets are out of range.");
            else if (hasSpellSummary && spellSummary.resolution == SpellResolutionType::Healing && !_controller._healingDiceEntered)
                disabledReason = battleText("Roll healing or enter a healing dice result first.");
            else if (!spellSlotReady)
                disabledReason = spellSlotReason;
            const bool canCastSpell = hasSpellSummary && !selectedTargets.empty() && allTargetsInRange && spellSlotReady &&
                                      _controller._actionAdjudicationReviewed &&
                                      (spellSummary.resolution != SpellResolutionType::Healing || _controller._healingDiceEntered);
            if (hasSpellSummary && !selectedTargets.empty())
            {
                if (spellSummary.resolution == SpellResolutionType::SavingThrow)
                    ImGui::TextDisabled(localizedUiLabel("Resolve Spell Cast will spend or record the spell, log each target save, and apply damage/effects only to failed saves.").c_str());
                else if (spellSummary.resolution == SpellResolutionType::Healing)
                    ImGui::TextDisabled(localizedUiLabel("Resolve Spell Cast will spend or record the spell, log the cast, and apply the previewed healing to selected targets.").c_str());
                else
                    ImGui::TextDisabled(localizedUiLabel("Resolve Spell Cast will spend or record the spell, log the cast, then apply the selected target resolution.").c_str());
            }
            if (!canCastSpell && !disabledReason.empty())
                ImGui::TextDisabled(localizedUiLabel("Cannot resolve spell: %s").c_str(), disabledReason.c_str());
            if (drawUiButton("Resolve Spell Cast",
                             canCastSpell,
                             disabledReason.c_str(),
                             UiButtonRole::Primary,
                             ImVec2(-1.0f, 0.0f),
                             "Spends the spell slot, resolves the selected target interaction, applies HP or ongoing effect changes, and writes combat log entries."))
                _controller.resolveSelectedSpellCast();
            if (_controller._lastAttackResolved)
            {
                ImGui::SameLine();
                ImGui::Text(localizedUiLabel("%s").c_str(), _controller._lastAttackMessage.c_str());
            }
        }
        else
        {
            FormUi::InputInt(localizedUiLabel("Damage Dice Result").c_str(), &_controller._damageRollResult);
            showHelpTooltip("Enter only the rolled dice total. Flat weapon modifiers are added automatically.");
            const int damageModifier = _controller.getSelectedDamageModifier();
            ImGui::TextDisabled(localizedUiLabel("Weapon Modifier: %+d  Damage Reduction: -%d").c_str(), damageModifier, target.armorDamageReduction);
            ImGui::TextDisabled(localizedUiLabel("Before Armor: %d  Damage Applied: %d").c_str(), _controller.getDamageTotal(), _controller.getArmorAdjustedDamageTotal());
            std::string applyDamageReason;
            if (!_controller._lastAttackResolved)
                applyDamageReason = battleText("Check whether the attack hits first.");
            else if (!_controller._lastAttackHit)
                applyDamageReason = battleText("The last checked attack did not hit.");
            if (drawUiButton("Apply Damage",
                             _controller._lastAttackResolved && _controller._lastAttackHit,
                             applyDamageReason.c_str(),
                             UiButtonRole::Primary,
                             ImVec2(-1.0f, 0.0f),
                             "Applies the damage result to the selected target and writes a combat log entry."))
                _controller.applyDamageRoll();
        }
    }
}
