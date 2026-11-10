    ImGui::SeparatorText(localizedUiLabel("Stat Summary").c_str());
    if (ImGui::BeginTable("SelectedMonsterLiveSummary", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Identity").c_str());
        tableTextWrapped((selectedMonster.size.empty() ? "Unknown" : selectedMonster.size) + " " + (selectedMonster.type.empty() ? "Creature" : selectedMonster.type));
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Defense").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), selectedMonster.armorClass);
        ImGui::TextDisabled(localizedUiLabel("Speed").c_str());
        ImGui::Text(localizedUiLabel("%d ft").c_str(), selectedMonster.speedFeet);
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Health / Actions").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), selectedMonster.hitPoints);
        ImGui::TextDisabled(localizedUiLabel("Roles").c_str());
        tableTextWrapped(roleTagsLabel(selectedMonster));
        ImGui::TextDisabled(localizedUiLabel("Actions").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), static_cast<int>(selectedMonster.actions.size()));
        ImGui::EndTable();
    }

    ImGui::SeparatorText(localizedUiLabel("Stat Block").c_str());
    showHelpTooltip("Editing saves immediately. These combat values are copied into the Battle Tracker when this monster is added.");

    if (ImGui::BeginTable("SelectedMonsterBasics", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        std::string name = selectedMonster.name;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Name").c_str(), &name))
            _controller.setSelectedMonsterName(name);
        std::string size = selectedMonster.size;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Size").c_str(), &size))
            _controller.setSelectedMonsterSize(size);
        std::string type = selectedMonster.type;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Type").c_str(), &type))
            _controller.setSelectedMonsterType(type);
        std::string challengeRating = selectedMonster.challengeRating;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Challenge").c_str(), &challengeRating))
            _controller.setSelectedMonsterChallengeRating(challengeRating);

        ImGui::TableSetColumnIndex(1);
        int armorClass = selectedMonster.armorClass;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Armor Class").c_str(), &armorClass))
            _controller.setSelectedMonsterArmorClass(armorClass);
        int armorDamageReduction = selectedMonster.armorDamageReduction;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Damage Reduction").c_str(), &armorDamageReduction))
            _controller.setSelectedMonsterArmorDamageReduction(armorDamageReduction);
        int hitPoints = selectedMonster.hitPoints;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Hit Points").c_str(), &hitPoints))
            _controller.setSelectedMonsterHitPoints(hitPoints);
        int speedFeet = selectedMonster.speedFeet;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("Speed ft").c_str(), &speedFeet))
            _controller.setSelectedMonsterSpeedFeet(speedFeet);
        int experience = selectedMonster.experience;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputInt(localizedUiLabel("XP").c_str(), &experience))
            _controller.setSelectedMonsterExperience(experience);
        if (!selectedMonster.catalogSourceId.empty())
            ImGui::TextWrapped(localizedUiLabel("Catalog ID: %s").c_str(), selectedMonster.catalogSourceId.c_str());
        if (!selectedMonster.baseCatalogMonsterId.empty())
            ImGui::TextWrapped(localizedUiLabel("Variant Base: %s").c_str(), selectedMonster.baseCatalogMonsterId.c_str());
        ImGui::EndTable();
    }

    ImGui::SeparatorText(localizedUiLabel("Encounter Roles").c_str());
    const std::vector<std::string> encounterRoles{"minion", "boss", "controller", "artillery", "support", "hazard", "social"};
    for (int roleIndex = 0; roleIndex < static_cast<int>(encounterRoles.size()); ++roleIndex)
    {
        const std::string &role = encounterRoles[static_cast<std::size_t>(roleIndex)];
        bool enabled = std::find(selectedMonster.encounterRoleTags.begin(), selectedMonster.encounterRoleTags.end(), role) != selectedMonster.encounterRoleTags.end();
        ImGui::PushID(role.c_str());
        if (FormUi::Checkbox(role.c_str(), &enabled))
        {
            if (enabled)
                selectedMonster.encounterRoleTags.push_back(role);
            else
                selectedMonster.encounterRoleTags.erase(std::remove(selectedMonster.encounterRoleTags.begin(), selectedMonster.encounterRoleTags.end(), role), selectedMonster.encounterRoleTags.end());
            selectedMonster.metadata.touch();
            _controller.saveCampaign();
        }
        ImGui::PopID();
        if (roleIndex < static_cast<int>(encounterRoles.size()) - 1)
            ImGui::SameLine();
    }

    auto saveMonsterText = [&](std::string &field, const std::string &value) {
        field = value;
        selectedMonster.metadata.touch();
        _controller.saveCampaign();
    };

    ImGui::SeparatorText(localizedUiLabel("Traits and Special Actions").c_str());
    if (ImGui::BeginTable("SelectedMonsterSpecials", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        std::string traits = selectedMonster.traits;
        ImGui::TextUnformatted(localizedUiLabel("Traits").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputTextMultiline(localizedUiLabel("##MonsterTraits").c_str(), &traits, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
            saveMonsterText(selectedMonster.traits, traits);
        std::string reactions = selectedMonster.reactions;
        ImGui::TextUnformatted(localizedUiLabel("Reactions").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputTextMultiline(localizedUiLabel("##MonsterReactions").c_str(), &reactions, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
            saveMonsterText(selectedMonster.reactions, reactions);
        std::string rechargeActions = selectedMonster.rechargeActions;
        ImGui::TextUnformatted(localizedUiLabel("Recharge Actions").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputTextMultiline(localizedUiLabel("##MonsterRechargeActions").c_str(), &rechargeActions, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
            saveMonsterText(selectedMonster.rechargeActions, rechargeActions);
        ImGui::TableSetColumnIndex(1);
        std::string legendaryActions = selectedMonster.legendaryActions;
        ImGui::TextUnformatted(localizedUiLabel("Legendary Actions").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputTextMultiline(localizedUiLabel("##MonsterLegendaryActions").c_str(), &legendaryActions, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
            saveMonsterText(selectedMonster.legendaryActions, legendaryActions);
        std::string lairActions = selectedMonster.lairActions;
        ImGui::TextUnformatted(localizedUiLabel("Lair Actions").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputTextMultiline(localizedUiLabel("##MonsterLairActions").c_str(), &lairActions, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
            saveMonsterText(selectedMonster.lairActions, lairActions);
        ImGui::EndTable();
    }
    drawMutedHelpText("Autosave: monster traits and special action notes save immediately.", "These long stat block fields write through the monster overview controller as you edit.");

    ImGui::SeparatorText(localizedUiLabel("Senses, Languages, and Defenses").c_str());
    if (ImGui::BeginTable("SelectedMonsterDefenses", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        std::string senses = selectedMonster.senses;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Senses").c_str(), &senses))
            saveMonsterText(selectedMonster.senses, senses);
        std::string languages = selectedMonster.languages;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Languages").c_str(), &languages))
            saveMonsterText(selectedMonster.languages, languages);
        std::string savingThrows = selectedMonster.savingThrows;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Saving Throws").c_str(), &savingThrows))
            saveMonsterText(selectedMonster.savingThrows, savingThrows);
        std::string skills = selectedMonster.skills;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Skills").c_str(), &skills))
            saveMonsterText(selectedMonster.skills, skills);
        ImGui::TableSetColumnIndex(1);
        std::string resistances = selectedMonster.damageResistances;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Resistances").c_str(), &resistances))
            saveMonsterText(selectedMonster.damageResistances, resistances);
        std::string immunities = selectedMonster.damageImmunities;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Immunities").c_str(), &immunities))
            saveMonsterText(selectedMonster.damageImmunities, immunities);
        std::string vulnerabilities = selectedMonster.damageVulnerabilities;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Vulnerabilities").c_str(), &vulnerabilities))
            saveMonsterText(selectedMonster.damageVulnerabilities, vulnerabilities);
        std::string conditionImmunities = selectedMonster.conditionImmunities;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Condition Immunities").c_str(), &conditionImmunities))
            saveMonsterText(selectedMonster.conditionImmunities, conditionImmunities);
        ImGui::EndTable();
    }

    if (!selectedMonster.attributes.empty())
    {
