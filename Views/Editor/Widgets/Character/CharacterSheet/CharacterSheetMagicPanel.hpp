        if (ImGui::BeginTabItem(localizedUiLabel("Magic").c_str()))
        {
    ImGui::SeparatorText(localizedUiLabel("Spellcasting").c_str());
    showHelpTooltip("D&D-style spell support: spellcasting ability, attack bonus, save DC, known/prepared spells, spell slots, and casting.");
    bool spellcastingEnabled = character.spellcasting.enabled;
    if (FormUi::Checkbox(localizedUiLabel("Uses Magic").c_str(), &spellcastingEnabled))
        _controller.setSpellcastingEnabled(spellcastingEnabled);
    if (!spellcastingEnabled)
        ImGui::TextWrapped(uiTextForLocale(
            "Enable spellcasting to choose an ability, track spell slots, and learn or prepare spells.",
            "Aktiviere Zauberwirken, um ein Attribut auszuwählen, Zauberplätze zu verwalten und Zauber zu lernen oder vorzubereiten."));

    if (character.spellcasting.enabled)
    {
        normalizeSpellcasting(character.spellcasting);
        const bool wideMagicBasics = ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 44.0f;
        if (ImGui::BeginTable("SpellcastingBasicsReadable", wideMagicBasics ? 3 : 1, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Ability").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            if (wideMagicBasics)
            {
                ImGui::TableSetupColumn(localizedUiLabel("Attack").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Save DC").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            }
            ImGui::TableNextColumn();
            const std::vector<std::string> abilities = _campaign.ruleSet ? _campaign.ruleSet->getAttributes() : std::vector<std::string>{"Intelligence", "Wisdom", "Charisma"};
            const std::string spellcastingAbilityPreview = character.spellcasting.ability.empty()
                ? uiTextForLocale("Choose an ability", "Attribut wählen")
                : localizedUiLabel(character.spellcasting.ability.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::BeginCombo(localizedUiLabel("Spellcasting Ability").c_str(), spellcastingAbilityPreview.c_str()))
            {
                for (const std::string &ability : abilities)
                {
                    const bool selected = character.spellcasting.ability == ability;
                    if (ImGui::Selectable(localizedUiLabel(ability.c_str()).c_str(), selected))
                        _controller.setSpellcastingAbility(ability);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::TableNextColumn();
            if (const auto edit = SpellcastingValueUi::drawField(_campaign, character.attributes, character.spellcasting, character.level, SpellcastingValues::Field::Attack)) {
                if (edit->valueEdited) _controller.setSpellAttackBonus(edit->value);
                else _controller.setSpellAttackMode(edit->mode);
            }
            ImGui::TableNextColumn();
            if (const auto edit = SpellcastingValueUi::drawField(_campaign, character.attributes, character.spellcasting, character.level, SpellcastingValues::Field::SaveDc)) {
                if (edit->valueEdited) _controller.setSpellSaveDc(edit->value);
                else _controller.setSpellSaveDcMode(edit->mode);
            }
            ImGui::EndTable();
            SpellcastingValueUi::drawSourceDetails(_campaign, character.attributes, character.spellcasting, character.level);
        }

        ImGui::SeparatorText(localizedUiLabel("Spell Slots").c_str());
        if (ImGui::Button(localizedUiLabel("Reset Used Slots").c_str()))
            _controller.resetSpellSlots();
        showHelpTooltip("Clears used spell slots, usually after a long rest.");
        ImGui::Checkbox(uiTextForLocale("Show unused spell levels", "Ungenutzte Zaubergrade anzeigen"), &_showUnusedSpellLevels);
        if (std::none_of(character.spellcasting.slotMaximums.begin() + 1, character.spellcasting.slotMaximums.end(),
                         [](int maximum) { return maximum > 0; }))
            ImGui::TextWrapped(uiTextForLocale(
                "Set the maximum slots granted by the character's class and level. Learn spells below; casting spends an available slot.",
                "Trage die maximalen Zauberplätze für Klasse und Stufe ein. Lerne unten Zauber; beim Wirken wird ein verfügbarer Platz verbraucht."));
        if (ImGui::BeginTable("SpellSlotsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Level").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Max").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Used").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Left").c_str());
            ImGui::TableHeadersRow();
            for (int spellLevel = 1; spellLevel <= 9; ++spellLevel)
            {
                if (!_showUnusedSpellLevels && spellLevel != 1 &&
                    character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)] == 0)
                    continue;
                ImGui::PushID(spellLevel);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(localizedUiLabel("%d").c_str(), spellLevel);
                ImGui::TableSetColumnIndex(1);
                int maximum = character.spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)];
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("##Max").c_str(), &maximum))
                    _controller.setSpellSlotMaximum(spellLevel, maximum);
                ImGui::TableSetColumnIndex(2);
                int used = character.spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)];
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                    _controller.setSpellSlotUsed(spellLevel, used);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text(localizedUiLabel("%d").c_str(), std::max(0, maximum - used));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::SeparatorText(localizedUiLabel("Known and Prepared Spells").c_str());
        const std::vector<SpellModel> availableSpells = getAvailableSpells(_campaign);
        if (availableSpells.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("Create spells in the Spell Library before assigning them here.").c_str());
            ImGui::BeginDisabled();
            ImGui::Button(localizedUiLabel("Learn Spell").c_str());
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled(localizedUiLabel("No spells are available to assign.").c_str());
        }
        else
        {
            if (_controller._selectedSpellToAddIndex < 0 || _controller._selectedSpellToAddIndex >= static_cast<int>(availableSpells.size()))
                _controller._selectedSpellToAddIndex = 0;

            std::vector<int> filteredSpellIndices;
            for (int i = 0; i < static_cast<int>(availableSpells.size()); ++i)
            {
                if (spellAssignmentSearchMatches(_campaign, availableSpells[static_cast<std::size_t>(i)], _controller._spellAssignmentSearch))
                    filteredSpellIndices.push_back(i);
            }
            if (!filteredSpellIndices.empty() &&
                std::find(filteredSpellIndices.begin(), filteredSpellIndices.end(), _controller._selectedSpellToAddIndex) == filteredSpellIndices.end())
            {
                _controller._selectedSpellToAddIndex = filteredSpellIndices.front();
            }

            ImGui::SetNextItemWidth(260.0f);
            FormUi::InputText(localizedUiLabel("Search Spells").c_str(), &_controller._spellAssignmentSearch);
            ImGui::SameLine();
            ImGui::TextDisabled(localizedUiLabel("%d/%d").c_str(), static_cast<int>(filteredSpellIndices.size()), static_cast<int>(availableSpells.size()));

            const bool hasFilteredSpells = !filteredSpellIndices.empty();
            const SpellModel *previewSpell = hasFilteredSpells ? &availableSpells[static_cast<std::size_t>(_controller._selectedSpellToAddIndex)] : nullptr;
            const CharacterSpellEntryModel *previewEntry = previewSpell ? findCharacterSpellEntry(character.spellcasting, previewSpell->metadata.id) : nullptr;
            const std::string comboPreview = previewSpell ? previewSpell->name : localizedUiLabel("No matching spells");
            ImGui::BeginDisabled(!hasFilteredSpells);
            if (FormUi::BeginCombo(localizedUiLabel("Add Spell").c_str(), comboPreview.c_str()))
            {
                for (const int spellIndex : filteredSpellIndices)
                {
                    const SpellModel &spell = availableSpells[static_cast<std::size_t>(spellIndex)];
                    const CharacterSpellEntryModel *entry = findCharacterSpellEntry(character.spellcasting, spell.metadata.id);
                    const bool selected = _controller._selectedSpellToAddIndex == spellIndex;
                    std::string label = spell.name + " – " + localizedSpellLevelLabel(spell.level) + " – " + localizedSpellResolutionLabel(spell.resolution) + " – " + spellSourceLabel(_campaign, spell);
                    if (entry)
                        label += entry->prepared ? localizedUiLabel(" (prepared)") : localizedUiLabel(" (learned)");
                    if (ImGui::Selectable(label.c_str(), selected))
                        _controller._selectedSpellToAddIndex = spellIndex;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();

            if (previewSpell && ImGui::BeginTable("SpellAssignmentPreview", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Level").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Resolution").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Source").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("State").c_str());
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(localizedSpellLevelLabel(previewSpell->level));
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(localizedSpellResolutionLabel(previewSpell->resolution));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped(spellSourceLabel(_campaign, *previewSpell));
                ImGui::TableSetColumnIndex(3);
                tableTextWrapped(spellAssignmentContext(character, *previewSpell));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(previewSpell->concentration ? localizedUiLabel("Concentration") : (previewSpell->ritual ? localizedUiLabel("Ritual") : "-"));
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(spellCastContext(_campaign, character, *previewSpell));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped(previewSpell->classes.empty() ? "-" : previewSpell->classes);
                ImGui::TableSetColumnIndex(3);
                tableTextWrapped(spellSummaryLine(*previewSpell));
                ImGui::EndTable();
            }

            std::string disabledReason;
            if (!hasFilteredSpells)
                disabledReason = localizedUiLabel("No spells match the search.");
            else if (!previewSpell)
                disabledReason = localizedUiLabel("Choose a spell to learn.");
            else if (previewEntry)
                disabledReason = localizedUiLabel("This character already knows the selected spell.");

            ImGui::BeginDisabled(!disabledReason.empty());
            if (ImGui::Button(localizedUiLabel("Learn Spell").c_str()) && previewSpell)
                _controller.addSpellToSelectedCharacter(previewSpell->metadata.id);
            ImGui::EndDisabled();
            if (!disabledReason.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled(localizedUiLabel("%s").c_str(), disabledReason.c_str());
            }
        }

        if (character.spellcasting.spells.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("No spells assigned to this character.").c_str());
        }
        else if (ImGui::BeginTable("CharacterSpellsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Prepared").c_str(), ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableSetupColumn(localizedUiLabel("Spell").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn(localizedUiLabel("Profile").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.18f);
            ImGui::TableSetupColumn(localizedUiLabel("Resolution").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
            ImGui::TableSetupColumn(localizedUiLabel("Actions").c_str(), ImGuiTableColumnFlags_WidthFixed, 156.0f);
            ImGui::TableHeadersRow();
            int pendingRemovedSpellEntry = -1;
            for (int spellEntryIndex = 0; spellEntryIndex < static_cast<int>(character.spellcasting.spells.size()); ++spellEntryIndex)
            {
                const CharacterSpellEntryModel &entry = character.spellcasting.spells[static_cast<std::size_t>(spellEntryIndex)];
                const SpellModel *spell = findSpellById(availableSpells, entry.spellId);
                ImGui::PushID(spellEntryIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool prepared = entry.prepared;
                if (FormUi::Checkbox(localizedUiLabel("##Prepared").c_str(), &prepared))
                    _controller.togglePreparedSpell(spellEntryIndex);
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(spell ? spell->name : localizedUiLabel("<Missing Spell>"));
                ImGui::TableSetColumnIndex(2);
                if (spell)
                {
                    tableTextWrapped(localizedSpellLevelLabel(spell->level) + " / " + localizedSpellResolutionLabel(spell->resolution));
                    if (spell->concentration || spell->ritual)
                        tableTextWrapped(std::string(spell->concentration ? localizedUiText("Concentration") : "") + (spell->concentration && spell->ritual ? ", " : "") + (spell->ritual ? localizedUiText("Ritual") : ""));
                }
                else
                {
                    ImGui::TextDisabled(localizedUiLabel("-").c_str());
                }
                const auto entryCasting = _campaign.ruleSet ? CharacterAdvancement::spellcastingForEntry(character, *_campaign.ruleSet, entry) : character.spellcasting;
                ImGui::TableSetColumnIndex(3);
                if (!spell)
                    ImGui::TextDisabled(localizedUiLabel("-").c_str());
                else if (spell->resolution == SpellResolutionType::SpellAttack || spell->resolution == SpellResolutionType::SavingThrow)
                    tableTextWrapped(SpellcastingValueUi::spellValueText(_campaign, character.attributes, entryCasting, character.level, *spell));
                else
                    ImGui::TextDisabled(localizedUiLabel("-").c_str());
                if (spell)
                    tableTextWrapped(!spell->damage.empty() ? spell->damage : (!spell->healing.empty() ? spell->healing : spell->effectSummary));
                ImGui::TableSetColumnIndex(4);
                SpellcastingValues::Reason calculationReason = SpellcastingValues::Reason::None;
                const bool canResolve = spell && SpellcastingValues::readyForSpell(_campaign, character.attributes, entryCasting, character.level, *spell, &calculationReason);
                ImGui::BeginDisabled(!canResolve);
                if (spell && (spell->level == 0 || entry.prepared) && ImGui::SmallButton(localizedUiLabel("Cast").c_str()))
                    _controller.castSpellFromSelectedCharacter(spellEntryIndex, spell->level);
                if (spell && spell->ritual && spell->level > 0)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton(localizedUiLabel("Ritual").c_str()))
                        _controller.castSpellFromSelectedCharacter(spellEntryIndex, spell->level, true);
                }
                if (spell && entry.prepared && spell->level > 0 && CharacterAdvancement::pactSlotLevel(character) >= spell->level && ImGui::SmallButton(localizedUiLabel("Pact Magic").c_str()))
                    _controller.castSpellFromSelectedCharacter(spellEntryIndex, CharacterAdvancement::pactSlotLevel(character), false, true);
                ImGui::EndDisabled();
                if (spell && !canResolve) tableTextWrapped(SpellcastingValueUi::reasonText(calculationReason));
                if (ImGui::SmallButton(localizedUiLabel("Remove").c_str()))
                    pendingRemovedSpellEntry = spellEntryIndex;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (pendingRemovedSpellEntry >= 0)
                _controller.removeSpellFromSelectedCharacter(pendingRemovedSpellEntry);
        }
    }

            ImGui::EndTabItem();
        }
