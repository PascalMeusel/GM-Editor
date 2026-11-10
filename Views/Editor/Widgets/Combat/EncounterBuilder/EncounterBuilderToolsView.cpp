#include "EncounterBuilderViewSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

void EncounterBuilderView::drawDifficultyPreview()
{
    EncounterModel &encounter = _controller._draftEncounter;

    if (!_controller._currentCampaign.ruleSet)
    {
        ImGui::TextWrapped(localizedUiLabel("No ruleset is active for difficulty math.").c_str());
        return;
    }

    std::vector<int> partyLevels;
    partyLevels.reserve(_controller._currentCampaign.characters.size());
    for (const CharacterModel &character : _controller._currentCampaign.characters)
        partyLevels.push_back(std::max(1, character.level));

    std::vector<std::string> challengeRatings;
    const std::vector<MonsterDefinitionModel> monsters = _controller.getMonsterCatalog();
    for (const EncounterParticipantModel &participant : encounter.participants)
    {
        if (participant.kind != CombatantKind::Monster)
            continue;
        const auto monsterIt = std::find_if(monsters.begin(), monsters.end(), [&](const MonsterDefinitionModel &monster) {
            return monster.metadata.id == participant.monsterDefinitionId || monster.metadata.id == participant.sourceEntityId;
        });
        if (monsterIt != monsters.end())
            challengeRatings.push_back(monsterIt->challengeRating);
    }

    const RuleEncounterDifficultySummary summary = _controller._currentCampaign.ruleSet->summarizeEncounterDifficulty(partyLevels, challengeRatings);
    if (ImGui::BeginTable("EncounterDifficultyPreview", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Party").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Opposition").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Thresholds").c_str());
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text(localizedUiLabel("%d PCs, level total %d").c_str(), summary.partySize, summary.partyLevelTotal);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text(localizedUiLabel("%d adjusted XP").c_str(), summary.adjustedExperience);
        const std::string difficultyLabel = localizedUiLabel(summary.difficulty.c_str());
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), difficultyLabel.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::Text(localizedUiLabel("Easy %d | Medium %d").c_str(), summary.easyThreshold, summary.mediumThreshold);
        ImGui::TextDisabled(localizedUiLabel("Hard %d | Deadly %d").c_str(), summary.hardThreshold, summary.deadlyThreshold);
        ImGui::EndTable();
    }

    const std::string difficultyLabel = localizedUiLabel(summary.difficulty.c_str());
    ImGui::TextColored(ImVec4(0.92f, 0.74f, 0.42f, 1.0f), localizedUiLabel("Difficulty: %s").c_str(), difficultyLabel.c_str());
    if (partyLevels.empty())
        ImGui::TextDisabled(localizedUiLabel("Add player characters to the campaign to calculate party thresholds.").c_str());
    if (challengeRatings.empty())
        ImGui::TextDisabled(localizedUiLabel("Add monsters to the encounter to calculate monster XP.").c_str());
    showHelpTooltip("Preview uses party character levels and monster challenge ratings from the active ruleset.");
}

void EncounterBuilderView::drawParticipantTools()
{
    UiGuidance::drawPurpose(UiGuidance::Topic::CampaignParticipants);
    UiGuidance::drawDetails(UiGuidance::Topic::CampaignParticipants);
    if (!_controller._currentCampaign.ruleSet)
    {
        ImGui::TextWrapped(localizedUiLabel("Select a ruleset before adding combatants.").c_str());
        return;
    }

    if (!_controller._currentCampaign.characters.empty())
    {
        std::vector<const char *> playerLabels;
        for (const CharacterModel &character : _controller._currentCampaign.characters)
            playerLabels.push_back(character.nameBuffer);
        drawFieldLabel(uiTextForLocale("Campaign character", "Kampagnenfigur"));
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::Combo("##EncounterPlayerSource", &_controller._selectedCharacterIndex, playerLabels.data(), static_cast<int>(playerLabels.size()));
        ImGui::TextWrapped(uiTextForLocale("Encounter: %s · Add all adds %d participants. Repeated additions include characters already present.",
            "Kampf: %s · Alle hinzufügen ergänzt %d Teilnehmende. Erneutes Hinzufügen ergänzt auch bereits vorhandene Figuren."),
            _controller._draftEncounter.name.c_str(), static_cast<int>(playerLabels.size()));
        if (drawUiButton(uiTextForLocale("Add selected character", "Gewählte Figur hinzufügen"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.addSelectedPlayer();
        if (drawUiButton(uiTextForLocale("Add all campaign characters", "Alle Kampagnenfiguren hinzufügen"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.addAllPlayers();
    }
    else ImGui::TextWrapped("%s", uiTextForLocale("Open Cast → New cast member → Player character to create one first. NPCs and monsters can be added separately.",
        "Erstelle zuerst unter Figuren → Neue Figur → Spielercharakter eine Figur. NSC und Monster kannst du separat ergänzen."));

    if (!_controller._currentCampaign.npcs.empty())
    {
        std::vector<std::string> npcLabelStorage;
        std::vector<const char *> npcLabels;
        for (const NPCModel &npc : _controller._currentCampaign.npcs)
            npcLabelStorage.push_back(npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name);
        for (const std::string &label : npcLabelStorage)
            npcLabels.push_back(label.c_str());
        drawFieldLabel("NPC");
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::Combo("##EncounterNpcSource", &_controller._selectedNPCIndex, npcLabels.data(), static_cast<int>(npcLabels.size()));
        if (drawUiButton("Add NPC", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Add the selected NPC to the encounter roster."))
            _controller.addSelectedNPC();
    }

    const std::vector<MonsterDefinitionModel> monsters = _controller.getMonsterCatalog();
    if (!monsters.empty())
    {
        std::vector<const char *> monsterLabels;
        for (const MonsterDefinitionModel &monster : monsters)
            monsterLabels.push_back(monster.name.c_str());
        drawFieldLabel("Monster");
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::Combo("##EncounterMonsterSource", &_controller._selectedMonsterIndex, monsterLabels.data(), static_cast<int>(monsterLabels.size()));
        if (drawUiButton("Add Monster", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Add the selected monster definition to the encounter roster."))
            _controller.addSelectedMonster();

        const MonsterDefinitionModel &monster = monsters[std::clamp(_controller._selectedMonsterIndex, 0, static_cast<int>(monsters.size()) - 1)];
        ImGui::TextWrapped(localizedUiLabel("AC %d  HP %d  Speed %d ft  CR %s").c_str(), monster.armorClass, monster.hitPoints, monster.speedFeet, monster.challengeRating.c_str());
    }
}
