#include "Views/Editor/Widgets/Session/GMScreen/GMScreenViewSupport.hpp"

void GMScreenView::drawConditionsReference()
{
    ImGui::SeparatorText(gm::localization::trUi("Conditions Reference"));
    if (ImGui::BeginTable("GMScreenConditions", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        const std::vector<RuleConditionDefinition> conditions = _controller._currentCampaign.ruleSet ? _controller._currentCampaign.ruleSet->getConditionDefinitions() : std::vector<RuleConditionDefinition>{};
        ImGui::TableSetupColumn(gm::localization::trUi("Condition"), ImGuiTableColumnFlags_WidthFixed, 112.0f);
        ImGui::TableSetupColumn(gm::localization::trUi("Reminder"));
        ImGui::TableHeadersRow();
        for (const auto &condition : conditions)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(condition.name.c_str());
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(condition.summary);
        }
        ImGui::EndTable();
    }
}

void GMScreenView::drawQuickNpcs()
{
    ImGui::SeparatorText(gm::localization::trUi("Quick NPCs"));
    if (_controller._currentCampaign.npcs.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No NPCs available."));
        return;
    }

    if (ImGui::BeginTable("GMScreenQuickNpcs", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, 180.0f)))
    {
        ImGui::TableSetupColumn(localizedUiLabel("NPC").c_str());
        ImGui::TableSetupColumn(gm::localization::trUi("Role"));
        ImGui::TableSetupColumn(gm::localization::trUi("Faction"));
        ImGui::TableSetupColumn(gm::localization::trUi("Conditions"));
        ImGui::TableHeadersRow();
        for (const NPCModel &npc : _controller._currentCampaign.npcs)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name);
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(npc.role);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(npc.faction);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(joinStrings(npc.conditions));
        }
        ImGui::EndTable();
    }
}

void GMScreenView::drawRandomTools()
{
    ImGui::SeparatorText(gm::localization::trUi("Quick Rolls"));
    if (ImGui::Button(gm::localization::trUi("Roll d20")))
        _controller._lastD20Roll = _controller.rollDie(20);
    ImGui::SameLine();
    ImGui::Text(localizedUiLabel("d20: %s").c_str(), _controller._lastD20Roll > 0 ? std::to_string(_controller._lastD20Roll).c_str() : "-");
    if (ImGui::Button(gm::localization::trUi("Roll d100")))
        _controller._lastD100Roll = _controller.rollDie(100);
    ImGui::SameLine();
    ImGui::Text(localizedUiLabel("d100: %s").c_str(), _controller._lastD100Roll > 0 ? std::to_string(_controller._lastD100Roll).c_str() : "-");

    ImGui::SeparatorText(gm::localization::trUi("Improvisation Prompts"));
    ImGui::BulletText("%s", localizedUiLabel("Reaction: helpful, wary, hostile, distracted, desperate.").c_str());
    ImGui::BulletText("%s", localizedUiLabel("Complication: time pressure, witness, cost, clue, rival.").c_str());
    ImGui::BulletText("%s", localizedUiLabel("Reward: information, favor, safe path, supplies, leverage.").c_str());

    ImGui::SeparatorText(gm::localization::trUi("Campaign Tables"));
    if (_controller._currentCampaign.randomTables.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                           gm::localization::trUi("Create reusable random tables in Campaign > Compendium."));
        return;
    }
    std::vector<const char *> labels;
    labels.reserve(_controller._currentCampaign.randomTables.size());
    for (const RandomTableModel &table : _controller._currentCampaign.randomTables)
        labels.push_back(table.name.empty() ? gm::localization::trUi("Unnamed table") : table.name.c_str());
    _controller._selectedRandomTableIndex = std::clamp(
        _controller._selectedRandomTableIndex, 0, static_cast<int>(_controller._currentCampaign.randomTables.size()) - 1);
    FormUi::Combo(gm::localization::trUi("Table"), &_controller._selectedRandomTableIndex, labels.data(),
                  static_cast<int>(labels.size()));
    if (drawUiButton(gm::localization::trUi("Roll table"), UiButtonRole::Primary))
        _controller.rollSelectedRandomTable();
    if (!_controller._randomTableRollHistory.empty())
    {
        const RandomTableRollResult &latest = _controller._randomTableRollHistory.front();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), RandomTables::formatRoll(latest).c_str());
        if (!latest.gmNotes.empty())
            drawInlineLabelValue(gm::localization::trUi("GM notes"), latest.gmNotes.c_str());
        if (drawSmallUiButton(gm::localization::trUi("Add to session notes"), UiButtonRole::Secondary))
            _controller.saveLatestRandomTableRollToNotes();
        TaskUi::sameLineIfFits();
        ImGui::BeginDisabled(!latest.playerSafe);
        if (drawSmallUiButton(gm::localization::trUi("Create player handout"), UiButtonRole::Secondary))
            _controller.createLatestRandomTableHandout();
        ImGui::EndDisabled();
        if (_controller._randomTableRollHistory.size() > 1 &&
            ImGui::CollapsingHeader(gm::localization::trUi("Recent table rolls")))
        {
            for (std::size_t index = 1; index < _controller._randomTableRollHistory.size(); ++index)
                ImGui::BulletText("%s", RandomTables::formatRoll(_controller._randomTableRollHistory[index]).c_str());
        }
    }
    if (!_controller._randomTableStatus.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._randomTableStatus.c_str());
}

void GMScreenView::drawRulesBookmarks()
{
    ImGui::SeparatorText(gm::localization::trUi("Rules Bookmarks"));
    FormUi::InputText(gm::localization::trUi("Bookmark"), &_controller._rulesBookmarkDraft);
    ImGui::SameLine();
    if (drawUiButton("Add Bookmark", !_controller._rulesBookmarkDraft.empty(), "Type a rule page, link, or table note first.", UiButtonRole::Primary))
        _controller.addRulesBookmark();

    if (_controller._rulesBookmarks.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No bookmarks yet."));
        return;
    }

    bool openBookmarkRemoval = false;
    for (int i = 0; i < static_cast<int>(_controller._rulesBookmarks.size()); ++i)
    {
        ImGui::PushID(i);
        ImGui::BulletText("%s", _controller._rulesBookmarks[static_cast<std::size_t>(i)].c_str());
        ImGui::SameLine();
        if (drawSmallUiButton("Remove", UiButtonRole::Destructive, "Remove this rules bookmark."))
        {
            _pendingRulesBookmarkRemovalIndex = i;
            openBookmarkRemoval = true;
        }
        ImGui::PopID();
    }

    if (openBookmarkRemoval)
        ImGui::OpenPopup(localizedUiLabel("Remove Rules Bookmark").c_str());

    if (ImGui::BeginPopupModal(localizedUiLabel("Remove Rules Bookmark").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (_pendingRulesBookmarkRemovalIndex < 0 ||
            _pendingRulesBookmarkRemovalIndex >= static_cast<int>(_controller._rulesBookmarks.size()))
        {
            ImGui::TextUnformatted(localizedUiLabel("No rules bookmark is selected.").c_str());
        }
        else
        {
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Remove this rules bookmark?"));
            drawInlineLabelValue("Bookmark", _controller._rulesBookmarks[static_cast<std::size_t>(_pendingRulesBookmarkRemovalIndex)].c_str());
        }

        const UiModalAction action = drawModalActionButtonsEx("Remove", UiButtonRole::Destructive, "Cancel", 110.0f);
        if (action == UiModalAction::Confirm)
        {
            _controller.removeRulesBookmark(_pendingRulesBookmarkRemovalIndex);
            _pendingRulesBookmarkRemovalIndex = -1;
        }
        else if (action == UiModalAction::Cancel)
        {
            _pendingRulesBookmarkRemovalIndex = -1;
        }
        ImGui::EndPopup();
    }
}

void GMScreenView::drawActiveEffects()
{
    ImGui::SeparatorText(gm::localization::trUi("Active Effects"));
    bool anyEffect = false;
    for (const CharacterModel &character : _controller._currentCampaign.characters)
    {
        if (character.conditions.empty())
            continue;
        anyEffect = true;
        ImGui::BulletText("%s: %s", character.nameBuffer, joinStrings(character.conditions).c_str());
    }
    for (const NPCModel &npc : _controller._currentCampaign.npcs)
    {
        if (npc.conditions.empty())
            continue;
        anyEffect = true;
        const std::string npcName = npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name;
        ImGui::BulletText("%s: %s", npcName.c_str(), joinStrings(npc.conditions).c_str());
    }
    if (!anyEffect)
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No active conditions or effects on players/NPCs."));
}
