#include "BattleTrackerViewActionGuideSupport.hpp"
#include "Views/Editor/Widgets/UiGuidance.hpp"

void BattleTrackerView::drawSetupPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    const bool setupPhase = encounter.combatPhase == CombatPhase::Setup || encounter.combatPhase == CombatPhase::Initiative || encounter.participants.empty();
    // Store the initial state so adding the first participant does not close
    // an untouched header. Later user choices remain authoritative.
    ImGui::SetNextItemOpen(encounter.participants.empty(), ImGuiCond_Once);
    if (!ImGui::CollapsingHeader(localizedUiText(setupPhase ? "Encounter Setup" : "Encounter Setup / Advanced")))
        return;

    if (_controller._currentCampaign.encounters.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No prepared battlefields exist yet. Open Prepare Battlefield first.").c_str());
        return;
    }

    std::vector<const char *> encounterLabels;
    for (const auto &existingEncounter : _controller._currentCampaign.encounters)
        encounterLabels.push_back(existingEncounter.name.empty() ? "<Unnamed Battlefield>" : existingEncounter.name.c_str());

    if (_controller._selectedEncounterIndex < 0)
        _controller._selectedEncounterIndex = 0;
    ImGui::TextUnformatted(localizedUiText("Prepared Battlefield"));
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::Combo("##PreparedBattlefield", &_controller._selectedEncounterIndex, encounterLabels.data(), static_cast<int>(encounterLabels.size()));
    showHelpTooltip("Choose a saved combat setup. A battlefield stores the grid, terrain, participants, and combat state.");
    if (ImGui::BeginTable("BattlefieldSetupActions", battleActionColumns(2, {"Load Setup", "Save Setup"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        if (drawUiButton("Load Setup", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.loadSelectedEncounter();
        ImGui::TableNextColumn();
        if (drawUiButton("Save Setup", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.saveEncounter();
        ImGui::EndTable();
    }
    ImGui::TextUnformatted(localizedUiText("Zoom"));
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::SliderFloat("##BattlefieldZoom", &_controller._zoom, 0.5f, 2.6f, "%.1fx");

    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), encounter.name.c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled(localizedUiLabel("%s  %d x %d, %d ft squares").c_str(),
                        encounter.mapContextName.empty() ? localizedUiText("Standalone battlefield") : encounter.mapContextName.c_str(),
                        encounter.battlefieldWidth,
                        encounter.battlefieldHeight,
                        encounter.cellSizeFeet);
    ImGui::PopTextWrapPos();
    showHelpTooltip("Each square uses the listed distance for movement and range.");
    FormUi::Checkbox(localizedUiLabel("Theater of the Mind").c_str(), &encounter.theaterOfMind);
    showHelpTooltip("Disables grid range enforcement and replaces the battlefield canvas with a narrative roster.");

    const ImGuiTreeNodeFlags addParticipantsFlags = setupPhase ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    if (!ImGui::CollapsingHeader(localizedUiLabel("Add Participants").c_str(), addParticipantsFlags))
        return;
    UiGuidance::drawPurpose(UiGuidance::Topic::CampaignParticipants);
    UiGuidance::drawDetails(UiGuidance::Topic::CampaignParticipants);

    if (!_controller._currentCampaign.characters.empty())
    {
        std::vector<const char *> playerLabels;
        for (const auto &character : _controller._currentCampaign.characters)
            playerLabels.push_back(character.nameBuffer);
        FormUi::Combo(uiTextForLocale("Campaign character", "Kampagnenfigur"), &_controller._selectedCharacterIndex, playerLabels.data(), static_cast<int>(playerLabels.size()));
        ImGui::TextWrapped(uiTextForLocale("Encounter: %s · Add all adds %d participants. Repeated additions include characters already present.",
            "Kampf: %s · Alle hinzufügen ergänzt %d Teilnehmende. Erneutes Hinzufügen ergänzt auch bereits vorhandene Figuren."),
            encounter.name.c_str(), static_cast<int>(playerLabels.size()));
        const char *addOne = uiTextForLocale("Add selected character", "Gewählte Figur hinzufügen");
        const char *addAll = uiTextForLocale("Add all campaign characters", "Alle Kampagnenfiguren hinzufügen");
        const bool canAdd = static_cast<bool>(_controller._currentCampaign.ruleSet);
        const char *rulesReason = uiTextForLocale("Select a campaign ruleset before adding characters.",
            "Wähle ein Kampagnenregelwerk, bevor du Figuren hinzufügst.");
        if (!canAdd) ImGui::TextWrapped("%s", rulesReason);
        if (ImGui::BeginTable("AddPlayerActions", battleActionColumns(2, {addOne, addAll}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton(addOne, canAdd, rulesReason, UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                _controller.addSelectedPlayer();
            ImGui::TableNextColumn();
            if (drawUiButton(addAll, canAdd, rulesReason, UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                _controller.addAllPlayers();
            ImGui::EndTable();
        }
    }
    else ImGui::TextWrapped("%s", uiTextForLocale("Open Cast → New cast member → Player character to create one first. NPCs and monsters can be added separately.",
        "Erstelle zuerst unter Figuren → Neue Figur → Spielercharakter eine Figur. NSC und Monster kannst du separat ergänzen."));

    if (!_controller._currentCampaign.npcs.empty())
    {
        std::vector<const char *> npcLabels;
        for (const auto &npc : _controller._currentCampaign.npcs)
            npcLabels.push_back(npc.name.empty() ? "<Unnamed NPC>" : npc.name.c_str());
        FormUi::Combo(localizedUiLabel("NPC").c_str(), &_controller._selectedNPCIndex, npcLabels.data(), static_cast<int>(npcLabels.size()));
        if (ImGui::Button(localizedUiLabel("Add NPC").c_str(), ImVec2(-1.0f, 0.0f)))
            _controller.addSelectedNPC();
    }

    const std::vector<MonsterDefinitionModel> monsters = _controller.getMonsterCatalog();
    if (!monsters.empty())
    {
        std::vector<const char *> monsterLabels;
        for (const auto &monster : monsters)
            monsterLabels.push_back(monster.name.c_str());
        FormUi::Combo(localizedUiLabel("Monster").c_str(), &_controller._selectedMonsterIndex, monsterLabels.data(), static_cast<int>(monsterLabels.size()));
        if (ImGui::Button(localizedUiLabel("Add Monster").c_str(), ImVec2(-1.0f, 0.0f)))
            _controller.addSelectedMonster();

        const MonsterDefinitionModel &monster = monsters[std::clamp(_controller._selectedMonsterIndex, 0, static_cast<int>(monsters.size()) - 1)];
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(localizedUiLabel("Armor Class %d  Hit Points %d  Speed %d ft  Challenge %s").c_str(), monster.armorClass, monster.hitPoints, monster.speedFeet, monster.challengeRating.c_str());
        ImGui::PopTextWrapPos();
        showHelpTooltip("Challenge is the monster difficulty rating from the ruleset or custom monster entry.");
    }
    else
    {
        ImGui::TextWrapped(localizedUiLabel("This ruleset does not expose a monster catalog yet.").c_str());
    }
}

void BattleTrackerView::drawParticipantPanel()
{
    EncounterModel &encounter = _controller._draftEncounter;
    const bool initiativePhase = encounter.combatPhase == CombatPhase::Setup || encounter.combatPhase == CombatPhase::Initiative || encounter.participants.empty();

    if (!ImGui::CollapsingHeader(localizedUiText("Turn order and combatant details"), initiativePhase ? ImGuiTreeNodeFlags_DefaultOpen : 0)) return;
    showHelpTooltip("Initiative decides who acts first. Enter each participant's initiative roll, then sort or adjust the order.");
    const char *orderStatus = localizedUiText(_controller._initiativeLocked ? "Order decided" : "Enter rolls, then decide order");
    battleSameLineIfFits(ImGui::CalcTextSize(orderStatus).x);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled(localizedUiText(_controller._initiativeLocked ? "Order decided" : "Enter rolls, then decide order"));
    ImGui::PopTextWrapPos();

    if (encounter.participants.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("Add players, NPCs, or monsters to decide turn order.").c_str());
        return;
    }

    if (initiativePhase)
    {
        const char *pendingPopup = nullptr;
        if (ImGui::BeginTable("InitiativePrimaryActions", battleActionColumns(2, {"Roll Initiative", "Sort by Roll", "Begin Combat", "Remove Selected"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            if (drawUiButton("Roll Initiative", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Rolls d20 plus Dexterity modifier for every participant. You can still edit any roll manually."))
                _controller.rollInitiativeForAll();
            ImGui::TableNextColumn();
            if (drawUiButton("Sort by Roll", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Sort participants from highest initiative roll to lowest."))
                _controller.sortInitiative();
            ImGui::TableNextColumn();
            if (drawUiButton("Begin Combat", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Starts the active turn workflow with the first combatant in initiative order."))
                _controller.beginCombat();
            ImGui::TableNextColumn();
            if (drawUiButton("Remove Selected", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f), "Review before removing the selected combatant from this encounter."))
                pendingPopup = "Remove Combatant";
            ImGui::EndTable();
        }
        if (pendingPopup) ImGui::OpenPopup(localizedUiLabel(pendingPopup).c_str());
        if (drawConfirmationModal("Remove Combatant", "Remove the selected combatant from this encounter?", "Remove"))
            _controller.removeSelectedParticipant();
    }
    else
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(localizedUiLabel("Initiative setup is collapsed during live combat. The table below remains visible for turn order and active-combatant context.").c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::CollapsingHeader(localizedUiLabel("Initiative Setup / Reorder").c_str()))
        {
            const char *pendingPopup = nullptr;
            if (ImGui::BeginTable("InitiativeCorrectionActions", battleActionColumns(2, {"Roll Initiative", "Sort by Roll", "Remove Selected"}), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableNextColumn();
                if (drawUiButton("Roll Initiative", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Advanced live correction. Re-rolls initiative for every participant."))
                    _controller.rollInitiativeForAll();
                ImGui::TableNextColumn();
                if (drawUiButton("Sort by Roll", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Advanced live correction. Sorts participants from highest initiative roll to lowest."))
                    _controller.sortInitiative();
                ImGui::TableNextColumn();
                if (drawUiButton("Remove Selected", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f), "Review before removing the selected combatant from this encounter."))
                    pendingPopup = "Remove Combatant";
                ImGui::EndTable();
            }
            if (pendingPopup) ImGui::OpenPopup(localizedUiLabel(pendingPopup).c_str());
            if (drawConfirmationModal("Remove Combatant", "Remove the selected combatant from this encounter?", "Remove"))
                _controller.removeSelectedParticipant();
        }
    }

    const float textScale = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const float initiativeWidth = std::max(76.0f * textScale, ImGui::CalcTextSize(localizedUiText("Initiative")).x + ImGui::GetStyle().CellPadding.x * 2.0f);
    const float orderWidth = std::max(64.0f * textScale, ImGui::GetFrameHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.x + ImGui::GetStyle().CellPadding.x * 2.0f);
    if (ImGui::BeginTable("CombatantsReadable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings, ImVec2(-1.0f, (_controller._combatStep == 0 ? 260.0f : 190.0f) * textScale)))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Combatant").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableSetupColumn(localizedUiLabel("Initiative").c_str(), ImGuiTableColumnFlags_WidthFixed, initiativeWidth);
        ImGui::TableSetupColumn(localizedUiLabel("Health / State").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableSetupColumn(localizedUiLabel("Order").c_str(), ImGuiTableColumnFlags_WidthFixed, orderWidth);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(encounter.participants.size()); ++i)
        {
            EncounterParticipantModel &participant = encounter.participants[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawActiveMarker(i == encounter.activeParticipantIndex && _controller._combatStep == 1, "Active combatant");
            ImGui::SameLine(0.0f, 2.0f);
            const bool selected = _controller._selectedParticipantIndex == i;
            ImGui::TextWrapped(localizedUiLabel("%d. %s").c_str(), i + 1, participant.displayName.c_str());
            if (ImGui::IsItemClicked())
                _controller._selectedParticipantIndex = i;
            if (selected)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(58, 76, 90, 110));
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt(("##Init" + participant.metadata.id).c_str(), &participant.initiative, 0, 0))
                _controller.saveEncounter();
            ImGui::TableNextColumn();
            ImGui::Text(localizedUiLabel("%d/%d HP").c_str(), participant.currentHitPoints, participant.maxHitPoints);
            battleSameLineIfFits(ImGui::GetFontSize() * 4.0f);
            ImGui::TextDisabled(localizedUiLabel("AC %d").c_str(), participant.armorClass);
            const std::string conditionCount = participant.conditions.empty()
                                                   ? "-"
                                                   : battleFormat("{0} conditions", {std::to_string(static_cast<int>(participant.conditions.size()))});
            tableTextWrapped(conditionCount);
            ImGui::TableNextColumn();
            if (drawTriangleIconButton(("##OrderUp" + participant.metadata.id).c_str(), true, UiButtonRole::IconOnly, "Move earlier in initiative."))
                _controller.moveParticipantInOrder(i, -1);
            ImGui::SameLine();
            if (drawTriangleIconButton(("##OrderDown" + participant.metadata.id).c_str(), false, UiButtonRole::IconOnly, "Move later in initiative."))
                _controller.moveParticipantInOrder(i, 1);
        }
        ImGui::EndTable();
    }

}
