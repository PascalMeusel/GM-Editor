        if (_detailSection == 3)
        {
            ImGui::SeparatorText(localizedUiLabel("Magic").c_str());
            showHelpTooltip("D&D-style spell support for NPCs. Prepared spells become combat actions when this NPC joins a battle.");
            bool spellcastingEnabled = selectedNpc.spellcasting.enabled;
            if (FormUi::Checkbox(localizedUiLabel("Uses Magic").c_str(), &spellcastingEnabled))
                _controller.setSelectedNpcSpellcastingEnabled(spellcastingEnabled);

            if (selectedNpc.spellcasting.enabled)
            {
                normalizeSpellcasting(selectedNpc.spellcasting);
                const bool wideBasics = ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 44.0f;
                if (ImGui::BeginTable("SelectedNpcSpellcastingBasics", wideBasics ? 3 : 1, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Ability").c_str());
                    if (wideBasics) {
                        ImGui::TableSetupColumn(localizedUiLabel("Attack").c_str());
                        ImGui::TableSetupColumn(localizedUiLabel("Save DC").c_str());
                    }
                    ImGui::TableNextColumn();
                    const std::vector<std::string> abilities = campaign.ruleSet ? campaign.ruleSet->getAttributes() : std::vector<std::string>{"Intelligence", "Wisdom", "Charisma"};
                    const std::string spellcastingAbilityPreview = selectedNpc.spellcasting.ability.empty()
                                                                       ? localizedUiLabel("<Unset>")
                                                                       : localizedUiLabel(selectedNpc.spellcasting.ability.c_str());
                    if (FormUi::BeginCombo(localizedUiLabel("Spellcasting Ability").c_str(), spellcastingAbilityPreview.c_str()))
                    {
                        for (const std::string &ability : abilities)
                        {
                            const bool selected = selectedNpc.spellcasting.ability == ability;
                            if (ImGui::Selectable(localizedUiLabel(ability.c_str()).c_str(), selected))
                                _controller.setSelectedNpcSpellcastingAbility(ability);
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::TableNextColumn();
                    if (const auto edit = SpellcastingValueUi::drawField(campaign, selectedNpc.attributes, selectedNpc.spellcasting, 1, SpellcastingValues::Field::Attack)) {
                        if (edit->valueEdited) _controller.setSelectedNpcSpellAttackBonus(edit->value);
                        else _controller.setSelectedNpcSpellAttackMode(edit->mode);
                    }
                    ImGui::TableNextColumn();
                    if (const auto edit = SpellcastingValueUi::drawField(campaign, selectedNpc.attributes, selectedNpc.spellcasting, 1, SpellcastingValues::Field::SaveDc)) {
                        if (edit->valueEdited) _controller.setSelectedNpcSpellSaveDc(edit->value);
                        else _controller.setSelectedNpcSpellSaveDcMode(edit->mode);
                    }
                    ImGui::EndTable();
                }
                SpellcastingValueUi::drawSourceDetails(campaign, selectedNpc.attributes, selectedNpc.spellcasting, 1);

                if (ImGui::BeginTable("SelectedNpcSpellSlots", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
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
                        int maximum = selectedNpc.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)];
                        ImGui::SetNextItemWidth(-1.0f);
                        if (FormUi::InputInt(localizedUiLabel("##Max").c_str(), &maximum))
                            _controller.setSelectedNpcSpellSlotMaximum(spellLevel, maximum);
                        ImGui::TableSetColumnIndex(2);
                        int used = selectedNpc.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)];
                        ImGui::SetNextItemWidth(-1.0f);
                        if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                            _controller.setSelectedNpcSpellSlotUsed(spellLevel, used);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text(localizedUiLabel("%d").c_str(), std::max(0, maximum - used));
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                const std::vector<SpellModel> availableSpells = getAvailableSpells(campaign);
                if (availableSpells.empty())
                {
                    ImGui::TextDisabled(localizedUiLabel("Create spells in the Spell Library before assigning them here.").c_str());
                    drawDisabledUiButton("Learn Spell", "No ruleset or campaign spells are available to assign.", UiButtonRole::Primary, ImVec2(118.0f, 0.0f));
                }
                else
                {
                    if (_controller._selectedNpcSpellToAddIndex >= static_cast<int>(availableSpells.size()))
                        _controller._selectedNpcSpellToAddIndex = 0;
                    if (_controller._selectedNpcSpellToAddIndex < 0)
                        _controller._selectedNpcSpellToAddIndex = 0;

                    ImGui::SeparatorText(localizedUiLabel("Assign Spell").c_str());
                    ImGui::SetNextItemWidth(-1.0f);
                    FormUi::InputText(localizedUiLabel("Search Spells").c_str(), &_controller._npcSpellAssignmentSearch);

                    const std::vector<int> filteredSpellIndexes = filteredSpellAssignmentIndexes(campaign, availableSpells, _controller._npcSpellAssignmentSearch);
                    if (!filteredSpellIndexes.empty() && !containsSpellAssignmentIndex(filteredSpellIndexes, _controller._selectedNpcSpellToAddIndex))
                        _controller._selectedNpcSpellToAddIndex = filteredSpellIndexes.front();

                    const SpellModel *previewSpell = filteredSpellIndexes.empty() ? nullptr : &availableSpells[static_cast<std::size_t>(_controller._selectedNpcSpellToAddIndex)];
                    const std::string previewSource = previewSpell ? spellSourceLabel(campaign, *previewSpell) : std::string();
                    const std::string comboPreview = previewSpell ? spellAssignmentOptionLabel(*previewSpell, previewSource) : localizedUiLabel("<No matching spell>");
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::BeginCombo(localizedUiLabel("Filtered Spell").c_str(), comboPreview.c_str()))
                    {
                        for (int spellIndex : filteredSpellIndexes)
                        {
                            const SpellModel &spell = availableSpells[static_cast<std::size_t>(spellIndex)];
                            const std::string source = spellSourceLabel(campaign, spell);
                            const bool learned = findNpcSpellEntry(selectedNpc, spell.metadata.id) != nullptr;
                            const bool selected = _controller._selectedNpcSpellToAddIndex == spellIndex;
                            const std::string label = spellAssignmentOptionLabel(spell, source) + (learned ? " – " + localizedUiLabel("Known") : "");
                            if (ImGui::Selectable(label.c_str(), selected))
                                _controller._selectedNpcSpellToAddIndex = spellIndex;
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::TextDisabled(localizedUiLabel("%d of %d spells shown").c_str(), static_cast<int>(filteredSpellIndexes.size()), static_cast<int>(availableSpells.size()));
                    if (!previewSpell)
                    {
                        ImGui::TextDisabled(localizedUiLabel("No spells match the current search.").c_str());
                        drawDisabledUiButton("Learn Spell", "No spell is selected. Clear or broaden the search filter.", UiButtonRole::Primary, ImVec2(118.0f, 0.0f));
                    }
                    else
                    {
                        const CharacterSpellEntryModel *knownEntry = findNpcSpellEntry(selectedNpc, previewSpell->metadata.id);
                        const bool canLearnSpell = knownEntry == nullptr;
                        const std::string learnDisabledReason = canLearnSpell ? std::string() : localizedUiLabel("This NPC already knows the selected spell.");
                        const std::string selectedSpellSummary = localizedSpellLevelLabel(previewSpell->level) + " | " + localizedSpellResolutionLabel(previewSpell->resolution) + " | " + previewSource;
                        const std::string knownContext = knownEntry ? (localizedUiLabel("Known from") + " " + (knownEntry->source.empty() ? previewSource : knownEntry->source)) : localizedUiLabel("Not learned");
                        const std::string preparedContext = npcSpellPreparedContext(selectedNpc, *previewSpell);
                        const std::string castContext = npcSpellCastContext(campaign, selectedNpc, *previewSpell);
                        UiStatusActionStrip spellStrip;
                        spellStrip.mode = "Spell Assignment";
                        spellStrip.selection = selectedSpellSummary.c_str();
                        spellStrip.nextAction = canLearnSpell ? localizedUiText("Learn adds it prepared so it can become a combat action.") : learnDisabledReason.c_str();
                        spellStrip.help = "Search filters by name, level, school, resolution, source, class/source text, and spell effect text.";
                        drawStatusActionStrip("##SelectedNpcSpellAssignmentStatus", spellStrip);

                        if (ImGui::BeginTable("SelectedNpcSpellAssignmentPreview", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableSetupColumn(localizedUiLabel("Known").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
                            ImGui::TableSetupColumn(localizedUiLabel("Prepared").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
                            ImGui::TableSetupColumn(localizedUiLabel("Cast").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
                            ImGui::TableSetupColumn(localizedUiLabel("Resolution").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            tableTextWrapped(knownContext);
                            ImGui::TableNextColumn();
                            tableTextWrapped(preparedContext);
                            ImGui::TableNextColumn();
                            tableTextWrapped(castContext);
                            ImGui::TableNextColumn();
                            tableTextWrapped(npcSpellResolutionPreview(campaign, selectedNpc, *previewSpell));
                            ImGui::EndTable();
                        }

                        if (!previewSpell->effectSummary.empty())
                            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), previewSpell->effectSummary.c_str());
                        else if (!previewSpell->description.empty())
                            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), previewSpell->description.c_str());

                        if (drawUiButton("Learn Spell",
                                         canLearnSpell,
                                         learnDisabledReason.c_str(),
                                         UiButtonRole::Primary,
                                         ImVec2(118.0f, 0.0f),
                                         "Assign the selected spell to this NPC and mark it prepared."))
                            _controller.addSpellToSelectedNpc(previewSpell->metadata.id);
                        ImGui::SameLine();
                        ImGui::TextDisabled(localizedUiLabel("Source: %s").c_str(), previewSource.c_str());
                    }
                }

                if (selectedNpc.spellcasting.spells.empty())
                {
                    ImGui::TextDisabled(localizedUiLabel("No spells assigned to this NPC.").c_str());
                }
                else if (ImGui::BeginTable("SelectedNpcSpells", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn(localizedUiLabel("Prepared").c_str(), ImGuiTableColumnFlags_WidthFixed, 78.0f);
                    ImGui::TableSetupColumn(localizedUiLabel("Spell").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
                    ImGui::TableSetupColumn(localizedUiLabel("Level").c_str(), ImGuiTableColumnFlags_WidthFixed, 78.0f);
                    ImGui::TableSetupColumn(localizedUiLabel("Effect").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.48f);
                    ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 68.0f);
                    ImGui::TableHeadersRow();
                    int pendingRemovedSpellEntry = -1;
                    for (int spellEntryIndex = 0; spellEntryIndex < static_cast<int>(selectedNpc.spellcasting.spells.size()); ++spellEntryIndex)
                    {
                        const CharacterSpellEntryModel &entry = selectedNpc.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)];
                        const SpellModel *spell = findSpellById(availableSpells, entry.spellId);
                        ImGui::PushID(spellEntryIndex);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        bool prepared = entry.prepared;
                        if (FormUi::Checkbox(localizedUiLabel("##Prepared").c_str(), &prepared))
                            _controller.toggleSelectedNpcPreparedSpell(spellEntryIndex);
                        ImGui::TableSetColumnIndex(1);
                        tableTextWrapped(spell ? spell->name : localizedUiLabel("<Missing Spell>"));
                        ImGui::TableSetColumnIndex(2);
                        tableTextWrapped(spell ? localizedSpellLevelLabel(spell->level) : "-");
                        ImGui::TableSetColumnIndex(3);
                        if (spell)
                        {
                            tableTextWrapped(!spell->damage.empty() ? spell->damage : (!spell->healing.empty() ? spell->healing : spell->effectSummary));
                            if (spell->concentration || spell->ritual)
                                tableTextWrapped(std::string(spell->concentration ? localizedUiText("Concentration") : "") +
                                                 (spell->concentration && spell->ritual ? ", " : "") +
                                                 (spell->ritual ? localizedUiText("Ritual") : ""));
                        }
                        else
                            ImGui::TextDisabled(localizedUiLabel("-").c_str());
                        ImGui::TableSetColumnIndex(4);
                        if (ImGui::SmallButton(localizedUiText("Remove")))
                            pendingRemovedSpellEntry = spellEntryIndex;
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                    if (pendingRemovedSpellEntry >= 0)
                        _controller.removeSpellFromSelectedNpc(pendingRemovedSpellEntry);
                }
            }

        }
