    NPCModel &selectedNpc = campaign.npcs[_controller._selectedNpcIndex];
    std::vector<ItemModel> items;
    if (campaign.ruleSet)
        items = campaign.ruleSet->getItems();
    items.insert(items.end(), campaign.customItems.begin(), campaign.customItems.end());

    const char *sections[] = {"Summary", "Profile", "Secrets/Goals", "Magic", "Inventory", "Story", "Combat", "Advanced"};
    const float unit = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const bool useSummaryColumns = ImGui::GetContentRegionAvail().x >= 520.0f * unit;
    const auto drawPortrait = [&](float size) {
        PortraitUi::preview(selectedNpc.metadata, size);
    };
    const auto drawSummary = [&] {
            if (drawUiButton(uiTextForLocale("Read NPC", "NSC lesen"), UiButtonRole::Primary))
                _reading = true;
            SelectedPlayerPreview::npc(campaign, _controller.currentSelectedNpcId(), _controller._playerReviewHandler);
            if (drawUiButton(uiTextForLocale("Edit profile", "Profil bearbeiten"), UiButtonRole::Secondary))
                _detailSection = 1;
            TaskUi::sameLineIfFits();
            if (drawUiButton(uiTextForLocale("Secrets and goals", "Geheimnisse und Ziele"), UiButtonRole::Secondary))
                _detailSection = 2;
            ImGui::Spacing();
            bool selectedVisibleToPlayers = npcIsPlayerVisible(campaign, selectedNpc);
            if (FormUi::Checkbox(localizedUiLabel("Visible to players").c_str(), &selectedVisibleToPlayers))
                _controller.setNpcPublicVisibility(selectedNpc.metadata.id, selectedVisibleToPlayers);
            showHelpTooltip(npcPublicStateReason(campaign, selectedNpc).c_str());
            const auto detail = [&](const char *heading, const std::string &value) {
                if(value.empty()) return;
                ImGui::TextDisabled("%s", heading);
                tableTextWrapped(value);
                ImGui::Spacing();
            };
            detail(uiTextForLocale("Role", "Rolle"), selectedNpc.role);
            detail(uiTextForLocale("Description", "Beschreibung"), selectedNpc.description);
            detail(uiTextForLocale("Attitude to the party", "Haltung zur Gruppe"), selectedNpc.attitudeTowardParty);
            detail(uiTextForLocale("Dialogue notes", "Gesprächsnotizen"), selectedNpc.dialogueNotes);
            detail(uiTextForLocale("Location", "Ort"), selectedNpc.threatLocation);
            if(!selectedNpc.factionId.empty() || !selectedNpc.faction.empty())
                detail(localizedUiText("Faction"), factionDisplayName(campaign, selectedNpc));
            if(!selectedNpc.conditions.empty())
                detail(localizedUiText("Conditions"), npcConditionsLabel(selectedNpc));
            if(selectedNpc.threatLevel>0)
                ImGui::Text("%s: %d / 10", localizedUiText("Threat"), selectedNpc.threatLevel);
            if(selectedVisibleToPlayers)
                detail(uiTextForLocale("Player text", "Spielertext"), npcPlayerSafeLine(selectedNpc));
    };
    const auto drawDetailHeader = [&] {
        const std::string npcName = selectedNpc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : selectedNpc.name;
        ImGui::TextUnformatted(npcName.c_str());
        ImGui::TextDisabled("%s", localizedUiLabel(npcCategoryLabel(selectedNpc.category)).c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo("##NpcDetailSection", localizedUiText(sections[_detailSection])))
        {
            for (int i = 0; i < 8; ++i)
                if (ImGui::Selectable(localizedUiText(sections[i]), _detailSection == i)) _detailSection = i;
            ImGui::EndCombo();
        }
    };

    if (useSummaryColumns && ImGui::BeginTable("SelectedNpcSummaryHeader", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Portrait", ImGuiTableColumnFlags_WidthFixed, 136.0f * unit);
        ImGui::TableSetupColumn("Summary", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        drawPortrait(128.0f * unit);
        ImGui::TableSetColumnIndex(1);
        drawDetailHeader();
        if (_detailSection == 0)
            drawSummary();
        ImGui::EndTable();
    }
    else
    {
        drawPortrait(std::min(88.0f * unit, ImGui::GetContentRegionAvail().x));
        drawDetailHeader();
        if (_detailSection == 0)
            drawSummary();
    }

    // The portrait picker needs the full editor width, not the narrow preview
    // column beside the summary.
    if (ImGui::TreeNode(uiTextForLocale("Edit portrait", "Porträt bearbeiten")))
    {
        if (PortraitUi::editor(selectedNpc.metadata)) _controller.saveCampaign();
        ImGui::TreePop();
    }

    {

        if (_detailSection == 1)
        {
            ImGui::SeparatorText(localizedUiLabel("Profile").c_str());
            showHelpTooltip("Editing saves immediately to the active campaign.");

            if (ImGui::BeginTable("SelectedNpcProfileEditor", 1, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                std::string name = selectedNpc.name;
                ImGui::TextUnformatted(localizedUiText("Name"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputText(localizedUiLabel("##Name").c_str(), &name))
                    _controller.setSelectedNpcName(name);

                int categoryIndex = static_cast<int>(selectedNpc.category);
                const char *categoryItems = "Person\0Creature\0Faction Entity\0Deity\0";
                const std::string localizedCategoryItems = localizedComboItems(categoryItems);
                ImGui::TextUnformatted(localizedUiText("Category"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::Combo(localizedUiLabel("##Category").c_str(), &categoryIndex, localizedCategoryItems.c_str()))
                    _controller.setSelectedNpcCategory(categoryIndex);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const FactionModel *selectedFaction = findFactionById(campaign, selectedNpc.factionId);
                const std::string selectedFactionLabel = selectedFaction ? (selectedFaction->name.empty() ? localizedUiLabel("<Unnamed Faction>") : selectedFaction->name) : localizedUiLabel("<No Modeled Faction>");
                ImGui::TextUnformatted(localizedUiText("Faction Model"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::BeginCombo(localizedUiLabel("##Faction Model").c_str(), selectedFactionLabel.c_str()))
                {
                    if (ImGui::Selectable(localizedUiLabel("<No Modeled Faction>").c_str(), selectedNpc.factionId.empty()))
                        _controller.setSelectedNpcFactionId("");
                    for (const FactionModel &faction : campaign.factions)
                    {
                        const bool selected = selectedNpc.factionId == faction.metadata.id;
                        const std::string label = faction.name.empty() ? "<Unnamed Faction>" : faction.name;
                        if (ImGui::Selectable(label.c_str(), selected))
                            _controller.setSelectedNpcFactionId(faction.metadata.id);
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                std::string faction = selectedNpc.faction;
                ImGui::TextUnformatted(localizedUiText("Legacy Faction Text"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputText(localizedUiLabel("##Legacy Faction Text").c_str(), &faction))
                    _controller.setSelectedNpcFaction(faction);
                const bool canCreateFactionFromText = hasVisibleText(selectedNpc.faction);
                if (drawUiButton("Create/Link Faction From Text",
                                 canCreateFactionFromText,
                                 "Enter legacy faction text before creating a modeled faction.",
                                 UiButtonRole::Secondary,
                                 ImVec2(-1.0f, 0.0f),
                                 "Reuse a matching modeled faction by name, or create one from this legacy text and link this NPC to it."))
                {
                    _controller.createOrLinkFactionFromSelectedNpcText();
                }
                drawMutedHelpText("Legacy bridge: free-text faction names can become modeled factions.", "The original text remains readable while the NPC gains a durable faction link for graph, search, and story references.");
                const DeityModel *selectedDeity = findDeityById(campaign, selectedNpc.deityId);
                const std::string selectedDeityLabel = selectedDeity ? (selectedDeity->name.empty() ? localizedUiLabel("<Unnamed Power>") : selectedDeity->name) : localizedUiLabel("<No Deity/Patron>");
                ImGui::TextUnformatted(localizedUiText("Deity or Patron"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::BeginCombo(localizedUiLabel("##Deity or Patron").c_str(), selectedDeityLabel.c_str()))
                {
                    if (ImGui::Selectable(localizedUiLabel("<No Deity/Patron>").c_str(), selectedNpc.deityId.empty()))
                        _controller.setSelectedNpcDeityId("");
                    for (const DeityModel &deity : campaign.deities)
                    {
                        const bool selected = selectedNpc.deityId == deity.metadata.id;
                        const std::string label = deity.name.empty() ? localizedUiLabel("<Unnamed Power>") : deity.name;
                        if (ImGui::Selectable(label.c_str(), selected))
                            _controller.setSelectedNpcDeityId(deity.metadata.id);
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                const bool canCreateDeityFromName = selectedNpc.category == NPCCategory::Deity && hasVisibleText(selectedNpc.name);
                const std::string deityDisabledReason = selectedNpc.category == NPCCategory::Deity
                                                            ? "Name the Deity/Patron NPC before creating a modeled power."
                                                            : "Set category to Deity before creating a modeled deity or patron from this NPC.";
                if (drawUiButton("Create/Link Deity From Name",
                                 canCreateDeityFromName,
                                 deityDisabledReason.c_str(),
                                 UiButtonRole::Secondary,
                                 ImVec2(-1.0f, 0.0f),
                                 "Reuse a matching modeled deity/patron by name, or create one from this Deity/Patron NPC and link it here."))
                {
                    _controller.createOrLinkDeityFromSelectedNpcName();
                }
                std::string role = selectedNpc.role;
                ImGui::TextUnformatted(localizedUiText("Role"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputText(localizedUiLabel("##Role").c_str(), &role))
                    _controller.setSelectedNpcRole(role);
                std::string location = selectedNpc.threatLocation;
                ImGui::TextUnformatted(localizedUiText("Location"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputText(localizedUiLabel("##Location").c_str(), &location))
                    _controller.setSelectedNpcLocation(location);
                int threatLevel = selectedNpc.threatLevel;
                ImGui::TextUnformatted(localizedUiText("Threat"));
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::SliderInt(localizedUiLabel("##Threat").c_str(), &threatLevel, 0, 10))
                    _controller.setSelectedNpcThreatLevel(threatLevel);
                ImGui::EndTable();
            }

            std::string attitude = selectedNpc.attitudeTowardParty;
            ImGui::TextUnformatted(localizedUiText("Attitude Toward Party"));
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputText(localizedUiLabel("##Attitude Toward Party").c_str(), &attitude))
                _controller.setSelectedNpcAttitude(attitude);

            ImGui::SeparatorText(localizedUiLabel("Description").c_str());
            std::string description = selectedNpc.description;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedNpcDescription").c_str(), &description, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap))
                _controller.setSelectedNpcDescription(description);
            drawMutedHelpText("Autosave: description edits save immediately.", "This large text field writes through the NPC overview controller as you edit.");

        }

        if (_detailSection == 2)
        {
            ImGui::SeparatorText(localizedUiLabel("Goals, Secrets, and Resources").c_str());
            if (ImGui::BeginTable("SelectedNpcDossier", 1, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                std::string goals = selectedNpc.goals;
                ImGui::TextUnformatted(localizedUiLabel("Goals").c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedNpcGoals").c_str(), &goals, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
                    _controller.setSelectedNpcGoals(goals);
                std::string resources = selectedNpc.resources;
                ImGui::TextUnformatted(localizedUiLabel("Resources").c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedNpcResources").c_str(), &resources, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
                    _controller.setSelectedNpcResources(resources);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                std::string secrets = selectedNpc.secrets;
                ImGui::TextUnformatted(localizedUiLabel("Secrets").c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedNpcSecrets").c_str(), &secrets, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
                    _controller.setSelectedNpcSecrets(secrets);
                std::string relationships = selectedNpc.relationships;
                ImGui::TextUnformatted(localizedUiLabel("Relationships").c_str());
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedNpcRelationships").c_str(), &relationships, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
                    _controller.setSelectedNpcRelationships(relationships);
                ImGui::EndTable();
            }
            drawMutedHelpText("Autosave: goals, secrets, resources, and relationship notes save immediately.", "These are GM-facing dossier fields; keep player-safe summaries separate when presentation tooling is added.");

            ImGui::SeparatorText(localizedUiLabel("Dialogue Notes").c_str());
            showHelpTooltip("GM-facing speech patterns, known lines, secrets, or conversation reminders.");
            std::string dialogueNotes = selectedNpc.dialogueNotes;
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputTextMultiline(localizedUiLabel("##SelectedNpcDialogueNotes").c_str(), &dialogueNotes, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap))
                _controller.setSelectedNpcDialogueNotes(dialogueNotes);
            drawMutedHelpText("Autosave: dialogue notes save immediately.", "Use this field for table prompts and voice reminders during roleplay.");

        }
