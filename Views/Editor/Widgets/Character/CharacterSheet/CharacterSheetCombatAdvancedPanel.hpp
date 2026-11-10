        if (ImGui::BeginTabItem(localizedUiLabel("Combat").c_str()))
        {
    ImGui::SeparatorText(localizedUiLabel("Health").c_str());
    showHelpTooltip("Persistent player health. Combat can use this value when adding the character to an encounter.");
    const float healthFraction = character.maxHitPoints > 0 ? static_cast<float>(character.currentHitPoints) / static_cast<float>(character.maxHitPoints) : 0.0f;
    const std::string healthLabel = std::to_string(character.currentHitPoints) + " / " + std::to_string(character.maxHitPoints);
    ImGui::ProgressBar(std::clamp(healthFraction, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), healthLabel.c_str());

    int currentHitPoints = character.currentHitPoints;
    int maxHitPoints = character.maxHitPoints;
    ImGui::SetNextItemWidth(120.0f);
    if (FormUi::InputInt(localizedUiLabel("Current Health").c_str(), &currentHitPoints))
        _controller.setSelectedCharacterCurrentHitPoints(currentHitPoints);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (FormUi::InputInt(localizedUiLabel("Max Health").c_str(), &maxHitPoints))
        _controller.setSelectedCharacterMaxHitPoints(maxHitPoints);
    int temporaryHitPoints = character.temporaryHitPoints;
    ImGui::SetNextItemWidth(120.0f);
    if (FormUi::InputInt(uiTextForLocale("Temporary Health", "Temporäre Trefferpunkte"), &temporaryHitPoints))
        _controller.setSelectedCharacterTemporaryHitPoints(temporaryHitPoints);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", uiTextForLocale(
        "Temporary health absorbs damage first. Set the total; it does not stack automatically.",
        "Temporäre Trefferpunkte fangen Schaden zuerst ab. Setze den Gesamtwert; er wird nicht automatisch addiert."));
    if (ImGui::SmallButton(uiTextForLocale("Damage 1", "1 Schaden")))
        _controller.adjustSelectedCharacterHitPoints(-1);
    ImGui::SameLine();
    if (ImGui::SmallButton(uiTextForLocale("Damage 5", "5 Schaden")))
        _controller.adjustSelectedCharacterHitPoints(-5);
    ImGui::SameLine();
    if (ImGui::SmallButton(uiTextForLocale("Heal 1", "1 heilen")))
        _controller.adjustSelectedCharacterHitPoints(1);
    ImGui::SameLine();
    if (ImGui::SmallButton(uiTextForLocale("Heal 5", "5 heilen")))
        _controller.adjustSelectedCharacterHitPoints(5);
    showHelpTooltip("Quickly apply damage or healing without typing the full value.");

            ImGui::SeparatorText(localizedUiLabel("Defenses and Conditions").c_str());
            if (ImGui::BeginTable("CharacterCombatReadiness", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Armor Class").c_str(), ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Damage Reduction").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn(localizedUiLabel("Conditions").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
                ImGui::TableSetupColumn(localizedUiLabel("Equipped Gear").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.46f);
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text(localizedUiLabel("%d").c_str(), armor.armorClass);
                ImGui::TableNextColumn();
                ImGui::Text(localizedUiLabel("%d").c_str(), armor.damageReduction);
                ImGui::TableNextColumn();
                tableTextWrapped(characterConditionsLabel(character));
                ImGui::TableNextColumn();
                tableTextWrapped(characterEquippedGearSummary(character, items));
                ImGui::EndTable();
            }
            if (!armor.warnings.empty())
            {
                ImGui::SeparatorText(localizedUiLabel("Armor Warnings").c_str());
                for (const std::string &warning : armor.warnings)
                    ImGui::BulletText("%s", warning.c_str());
            }

            ImGui::SeparatorText(localizedUiLabel("Combat Resources").c_str());
            tableTextWrapped(characterResourceSummary(character));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(localizedUiLabel("Advanced").c_str()))
        {
    const auto *definitionRules = dynamic_cast<const DefinitionBackedRuleSet *>(_campaign.ruleSet.get());
    if (definitionRules)
    {
        const RulesetDefinitionModel &definition = definitionRules->definition();
        RulesEvaluationContext evaluationContext;
        evaluationContext.level = character.level;
        evaluationContext.values = character.ruleValues;
        for (const RulesetAbilityScoreDefinition &ability : definition.abilityScores)
        {
            if (evaluationContext.values.find(ability.id) != evaluationContext.values.end())
                continue;
            const auto legacy = character.attributes.find(ability.name);
            if (legacy != character.attributes.end())
                evaluationContext.values[ability.id] = legacy->second;
        }
        const RulesEvaluationSnapshot snapshot = definitionRules->evaluate(evaluationContext);

        ImGui::SeparatorText(localizedUiLabel("Ruleset Values").c_str());
        ImGui::TextDisabled(localizedUiLabel("%s · %s").c_str(), definitionRules->getName(), definition.version.c_str());
        if (ImGui::BeginTable("DefinitionBackedCharacterValues", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Ability").c_str());
            ImGui::TableSetupColumn(uiTextForLocale("Manual score", "Manueller Wert"), ImGuiTableColumnFlags_WidthFixed, 112.0f);
            ImGui::TableSetupColumn(uiTextForLocale("Calculated modifier", "Berechneter Modifikator"), ImGuiTableColumnFlags_WidthFixed, 112.0f);
            ImGui::TableHeadersRow();
            for (const RulesetAbilityScoreDefinition &ability : definition.abilityScores)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                tableTextWrapped(ability.name.empty() ? ability.id : ability.name);
                ImGui::TableNextColumn();
                int score = evaluationContext.values[ability.id];
                ImGui::PushID(ability.id.c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("##StableRuleValue").c_str(), &score))
                    _controller.setSelectedCharacterRuleValue(ability.id, ability.name, score);
                ImGui::PopID();
                ImGui::TableNextColumn();
                const auto modifier = snapshot.abilityModifiers.find(ability.id);
                ImGui::Text(localizedUiLabel("%+.0f").c_str(), modifier == snapshot.abilityModifiers.end() ? 0.0 : modifier->second);
            }
            ImGui::EndTable();
        }

        if (!snapshot.derivedStats.empty() || !snapshot.defenses.empty() || !snapshot.resourceMaximums.empty())
        {
            ImGui::SeparatorText(localizedUiLabel("Calculated Snapshot").c_str());
            if (ImGui::BeginTable("DefinitionBackedCalculatedValues", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Value").c_str());
                ImGui::TableSetupColumn(localizedUiLabel("Result").c_str(), ImGuiTableColumnFlags_WidthFixed, 132.0f);
                ImGui::TableHeadersRow();
                const auto drawCalculated = [](const std::string &prefix, const std::map<std::string, double> &values) {
                    for (const auto &value : values)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted((prefix + value.first).c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text(localizedUiLabel("%.2f").c_str(), value.second);
                    }
                };
                drawCalculated("", snapshot.derivedStats);
                drawCalculated(localizedUiLabel("Defense") + " · ", snapshot.defenses);
                drawCalculated(localizedUiLabel("Resource") + " · ", snapshot.resourceMaximums);
                ImGui::EndTable();
            }
        }
        if (!snapshot.valid && ImGui::CollapsingHeader(localizedUiLabel("Ruleset Evaluation Errors").c_str()))
        {
            for (const std::string &error : snapshot.errors)
            {
                const std::string localizedError = gm::rulesets::formatBackendError(
                    error, gm::localization::LocalizationService::instance().locale());
                ImGui::BulletText("%s", localizedError.c_str());
            }
        }
    }
    else if (!character.attributes.empty())
    {
        ImGui::Separator();
        ImGui::SeparatorText(localizedUiLabel("Ability Scores").c_str());
        showHelpTooltip("Ability scores are editable here. Combat uses Dexterity for defense, Constitution for suggested health, and weapon abilities for attacks.");
        if (ImGui::BeginTable("CharacterAttributes", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Ability").c_str());
            ImGui::TableSetupColumn(uiTextForLocale("Manual score", "Manueller Wert"), ImGuiTableColumnFlags_WidthFixed, 132.0f);
            ImGui::TableHeadersRow();
            for (const auto &attribute : character.attributes)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(localizedUiLabel(attribute.first.c_str()));
                ImGui::TableSetColumnIndex(1);
                int score = attribute.second;
                ImGui::PushID(attribute.first.c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputInt(localizedUiLabel("##Score").c_str(), &score))
                    _controller.setSelectedCharacterAttribute(attribute.first, score);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText(localizedUiLabel("Advanced Character Details").c_str());
    {
    drawMutedHelpText("Proficiencies, resources, and feature actions are tucked here to keep the live sheet readable first.", "Open this section when you are doing deeper character maintenance instead of live-table scanning.");

    ImGui::SeparatorText(localizedUiLabel("Background and Proficiencies").c_str());
    showHelpTooltip("Track rules-facing character details that are useful during play: backgrounds, feats, traits, proficiencies, languages, senses, and speed.");
    auto renderDetailList = [&](const char *label, std::vector<std::string> &entries, std::string &draft, CharacterSheetController::CharacterDetailList list) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(localizedUiText(label));
        if (entries.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled(localizedUiLabel("None").c_str());
        }
        else if (ImGui::BeginTable("Entries", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Value").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
            int pendingRemoval = -1;
            for (int i = 0; i < static_cast<int>(entries.size()); ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(entries[static_cast<std::size_t>(i)]);
                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(i);
                if (drawSmallUiButton("Remove##CharacterDetail", UiButtonRole::Destructive, "Remove this character detail entry."))
                    pendingRemoval = i;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (pendingRemoval >= 0)
                _controller.removeCharacterDetailListEntry(list, pendingRemoval);
        }

        ImGui::SetNextItemWidth(260.0f);
        FormUi::InputText(localizedUiLabel("##NewEntry").c_str(), &draft);
        ImGui::SameLine();
        if (ImGui::Button(localizedUiLabel("Add").c_str()))
        {
            _controller.addCharacterDetailListEntry(list, draft);
            draft.clear();
        }
        ImGui::PopID();
    };

    if (ImGui::BeginTable("CharacterDetailLists", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Left").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Right").c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        renderDetailList("Feats", character.feats, _controller._newFeatEntry, CharacterSheetController::CharacterDetailList::Feats);
        renderDetailList("Traits / Features", character.traitsAndFeatures, _controller._newFeatureEntry, CharacterSheetController::CharacterDetailList::TraitsAndFeatures);
        renderDetailList("Languages", character.languages, _controller._newLanguageEntry, CharacterSheetController::CharacterDetailList::Languages);
        ImGui::TableSetColumnIndex(1);
        renderDetailList("Senses", character.senses, _controller._newSenseEntry, CharacterSheetController::CharacterDetailList::Senses);
        renderDetailList("Saving Throws", character.savingThrowProficiencies, _controller._newSavingThrowEntry, CharacterSheetController::CharacterDetailList::SavingThrows);
        renderDetailList("Skills", character.skillProficiencies, _controller._newSkillEntry, CharacterSheetController::CharacterDetailList::Skills);
        renderDetailList("Tools", character.toolProficiencies, _controller._newToolEntry, CharacterSheetController::CharacterDetailList::Tools);
        ImGui::EndTable();
    }

    ImGui::SeparatorText(localizedUiLabel("Resource Trackers").c_str());
    showHelpTooltip("Track limited-use features with per-turn, rest, day, or custom recharge rules.");
    if (beginContextTreeNode("Resource Tracker List", !character.resourceTrackers.empty(), "Show limited-use feature resources only when this character has resources to manage or you are adding one."))
    {
        if (character.resourceTrackers.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("No resources defined.").c_str());
        }
        else if (ImGui::BeginTable("CharacterResourceTrackers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Resource").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.32f);
            ImGui::TableSetupColumn(localizedUiLabel("Uses").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
            ImGui::TableSetupColumn(localizedUiLabel("Recharge").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 116.0f);
            ImGui::TableHeadersRow();
            int pendingRemovedTracker = -1;
            for (int trackerIndex = 0; trackerIndex < static_cast<int>(character.resourceTrackers.size()); ++trackerIndex)
            {
                ResourceTrackerModel tracker = character.resourceTrackers[static_cast<std::size_t>(trackerIndex)];
                normalizeResourceTracker(tracker);
                ImGui::PushID(trackerIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(tracker.name);
                ImGui::TableSetColumnIndex(1);
                int used = tracker.used;
                ImGui::SetNextItemWidth(76.0f);
                if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                    _controller.setSelectedCharacterResourceUsed(trackerIndex, used);
                ImGui::SameLine();
                ImGui::TextDisabled(localizedUiLabel("/ %d max, %d left").c_str(), tracker.maximum, std::max(0, tracker.maximum - tracker.used));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped(tracker.rechargeRule);
                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton(localizedUiLabel("Reset").c_str()))
                    _controller.resetSelectedCharacterResource(trackerIndex);
                ImGui::SameLine();
                if (ImGui::SmallButton(localizedUiLabel("Remove").c_str()))
                    pendingRemovedTracker = trackerIndex;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (pendingRemovedTracker >= 0)
                _controller.removeResourceTrackerFromSelectedCharacter(pendingRemovedTracker);
        }
        ResourceTrackerModel &resourceDraft = _controller._resourceTrackerDraft;
        if (beginAdvancedTreeNode("Add Resource Tracker", "Open only when creating a new limited-use feature resource."))
        {
            FormUi::InputText(localizedUiLabel("Resource Name").c_str(), &resourceDraft.name);
            FormUi::InputInt(localizedUiLabel("Maximum Uses").c_str(), &resourceDraft.maximum);
            FormUi::InputInt(localizedUiLabel("Used Uses").c_str(), &resourceDraft.used);
            FormUi::InputText(localizedUiLabel("Recharge Rule").c_str(), &resourceDraft.rechargeRule);
            FormUi::InputTextMultiline(localizedUiLabel("Resource Notes").c_str(), &resourceDraft.notes, ImVec2(-1.0f, 54.0f));
            normalizeResourceTracker(resourceDraft);
            if (ImGui::Button(localizedUiLabel("Add Resource").c_str()))
                _controller.addResourceTrackerToSelectedCharacter();
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    ImGui::SeparatorText(localizedUiLabel("Feature Actions").c_str());
    showHelpTooltip("Reusable class features, feats, and homebrew abilities that can appear as combat actions.");
    if (beginContextTreeNode("Feature Action List", !character.featureActions.empty(), "Show reusable class, feat, and homebrew actions only when this character has actions or you are adding one."))
    {
        if (character.featureActions.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("No feature actions defined.").c_str());
        }
        else if (ImGui::BeginTable("CharacterFeatureActions", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Action").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
            ImGui::TableSetupColumn(localizedUiLabel("Use").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
            ImGui::TableSetupColumn(localizedUiLabel("Roll / Damage").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 74.0f);
            ImGui::TableHeadersRow();
            int pendingRemovedAction = -1;
            for (int actionIndex = 0; actionIndex < static_cast<int>(character.featureActions.size()); ++actionIndex)
            {
                const MonsterActionModel &action = character.featureActions[static_cast<std::size_t>(actionIndex)];
                ImGui::PushID(actionIndex);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(action.name);
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped((action.actionKind.empty() ? localizedUiLabel("Feature") : action.actionKind) + " | " + std::to_string(std::max(action.reachFeet, action.rangeFeet)) + " " + localizedUiLabel("ft"));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped((action.attackBonus.empty() ? std::string("-") : action.attackBonus) + " | " + (action.damage.empty() ? std::string("-") : action.damage));
                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton(localizedUiLabel("Remove").c_str()))
                    pendingRemovedAction = actionIndex;
                ImGui::PopID();
            }
            ImGui::EndTable();
            if (pendingRemovedAction >= 0)
                _controller.removeFeatureActionFromSelectedCharacter(pendingRemovedAction);
        }
        MonsterActionModel &featureDraft = _controller._featureActionDraft;
        if (featureDraft.actionKind.empty())
            featureDraft.actionKind = "Feature";
        if (beginAdvancedTreeNode("Add Feature Action", "Open only when creating a new reusable feature action."))
        {
            FormUi::InputText(localizedUiLabel("Action Name").c_str(), &featureDraft.name);
            FormUi::InputText(localizedUiLabel("Kind").c_str(), &featureDraft.actionKind);
            FormUi::InputInt(localizedUiLabel("Reach").c_str(), &featureDraft.reachFeet);
            FormUi::InputInt(localizedUiLabel("Normal Range").c_str(), &featureDraft.normalRangeFeet);
            FormUi::InputInt(localizedUiLabel("Max Range").c_str(), &featureDraft.rangeFeet);
            FormUi::InputText(localizedUiLabel("Attack Bonus").c_str(), &featureDraft.attackBonus);
            FormUi::InputText(localizedUiLabel("Damage").c_str(), &featureDraft.damage);
            if (FormUi::BeginCombo(localizedUiLabel("Linked Resource").c_str(), featureDraft.linkedResourceId.empty() ? localizedUiText("None") : featureDraft.linkedResourceId.c_str()))
            {
                if (ImGui::Selectable(localizedUiLabel("None").c_str(), featureDraft.linkedResourceId.empty()))
                    featureDraft.linkedResourceId.clear();
                for (const ResourceTrackerModel &tracker : character.resourceTrackers)
                {
                    const bool selected = featureDraft.linkedResourceId == tracker.metadata.id;
                    const std::string label = tracker.name.empty() ? tracker.metadata.id : tracker.name;
                    if (ImGui::Selectable(label.c_str(), selected))
                        featureDraft.linkedResourceId = tracker.metadata.id;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            FormUi::InputTextMultiline(localizedUiLabel("Notes").c_str(), &featureDraft.notes, ImVec2(-1.0f, 64.0f));
            featureDraft.reachFeet = std::max(0, featureDraft.reachFeet);
            featureDraft.normalRangeFeet = std::max(featureDraft.reachFeet, featureDraft.normalRangeFeet);
            featureDraft.rangeFeet = std::max(featureDraft.normalRangeFeet, featureDraft.rangeFeet);
            if (ImGui::Button(localizedUiLabel("Add Feature Action").c_str()))
                _controller.addFeatureActionToSelectedCharacter();
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    }

            ImGui::EndTabItem();
        }
