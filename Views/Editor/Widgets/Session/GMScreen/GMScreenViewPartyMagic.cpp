#include "Views/Editor/Widgets/Session/GMScreen/GMScreenViewSupport.hpp"

void GMScreenView::drawPartyPassives(const std::vector<ItemModel> &items)
{
    ImGui::SeparatorText(gm::localization::trUi("Party Passives"));
    if (_controller._currentCampaign.characters.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No players available."));
        return;
    }

    if (ImGui::BeginTable("GMScreenPartyPassives", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(gm::localization::trUi("Player"));
        ImGui::TableSetupColumn(gm::localization::trUi("Passives"), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(gm::localization::trUi("Survival"), ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn(gm::localization::trUi("Conditions"));
        ImGui::TableHeadersRow();

        for (CharacterModel character : _controller._currentCampaign.characters)
        {
            normalizeCharacterHitPoints(character);
            const RuleArmorSummary armor = _controller._currentCampaign.ruleSet ? CharacterRules::evaluateArmor(*_controller._currentCampaign.ruleSet, character, items) : RuleArmorSummary{};
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(character.nameBuffer);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(localizedUiLabel("Perception %d").c_str(), _controller._currentCampaign.ruleSet ? _controller._currentCampaign.ruleSet->getPassiveScore(character.attributes, "Perception", character.level) : 10);
            ImGui::TextDisabled(localizedUiLabel("Insight %d").c_str(), _controller._currentCampaign.ruleSet ? _controller._currentCampaign.ruleSet->getPassiveScore(character.attributes, "Insight", character.level) : 10);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text(localizedUiLabel("AC %d").c_str(), armor.armorClass);
            ImGui::TextDisabled(localizedUiLabel("%d/%d HP").c_str(), character.currentHitPoints, character.maxHitPoints);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(joinStrings(character.conditions));
        }
        ImGui::EndTable();
    }

}

void GMScreenView::drawCommonDcs()
{
    ImGui::SeparatorText(gm::localization::trUi("Common DCs"));
    if (ImGui::BeginTable("GMScreenDcTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        const std::vector<RuleDifficultyClass> dcs = _controller._currentCampaign.ruleSet ? _controller._currentCampaign.ruleSet->getCommonDifficultyClasses() : std::vector<RuleDifficultyClass>{};
        ImGui::TableSetupColumn(gm::localization::trUi("Difficulty"));
        ImGui::TableSetupColumn(localizedUiLabel("DC").c_str());
        ImGui::TableHeadersRow();
        for (const auto &dc : dcs)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(dc.label.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(localizedUiLabel("%d").c_str(), dc.value);
        }
        ImGui::EndTable();
    }
}

void GMScreenView::drawConditionsSnapshot()
{
    ImGui::SeparatorText(gm::localization::trUi("Condition Reminders"));
    const std::vector<RuleConditionDefinition> conditions = _controller._currentCampaign.ruleSet ? _controller._currentCampaign.ruleSet->getConditionDefinitions() : std::vector<RuleConditionDefinition>{};
    if (conditions.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No condition reference is available for this ruleset."));
        return;
    }

    if (ImGui::BeginTable("GMScreenConditionSnapshot", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, 150.0f)))
    {
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

void GMScreenView::drawLiveSpellSlots()
{
    ImGui::SeparatorText(gm::localization::trUi("Live Spell Slots"));
    bool anyCaster = false;
    for (int characterIndex = 0; characterIndex < static_cast<int>(_controller._currentCampaign.characters.size()); ++characterIndex)
    {
        const CharacterModel &character = _controller._currentCampaign.characters[static_cast<std::size_t>(characterIndex)];
        if (!character.spellcasting.enabled && !hasTrackedSlots(character.spellcasting))
            continue;

        anyCaster = true;
        CharacterSpellcastingModel spellcasting = character.spellcasting;
        normalizeSpellcasting(spellcasting);
        const std::string label = std::string(character.nameBuffer) + "##PlayerSpellSlots" + std::to_string(characterIndex);
        if (ImGui::TreeNodeEx(label.c_str()))
        {
            const std::string resetLabel = localizedUiLabel("Reset Used Slots") + "##PlayerSlots" + std::to_string(characterIndex);
            if (ImGui::SmallButton(resetLabel.c_str()))
                _controller.resetPlayerSpellSlots(characterIndex);
            if (ImGui::BeginTable(("PlayerLiveSpellSlots" + std::to_string(characterIndex)).c_str(), 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
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
                    int maximum = spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)];
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Max").c_str(), &maximum))
                        _controller.setPlayerSpellSlotMaximum(characterIndex, spellLevel, maximum);
                    ImGui::TableSetColumnIndex(2);
                    int used = spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)];
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                        _controller.setPlayerSpellSlotUsed(characterIndex, spellLevel, used);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text(localizedUiLabel("%d").c_str(), std::max(0, maximum - used));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    for (int npcIndex = 0; npcIndex < static_cast<int>(_controller._currentCampaign.npcs.size()); ++npcIndex)
    {
        const NPCModel &npc = _controller._currentCampaign.npcs[static_cast<std::size_t>(npcIndex)];
        if (!npc.spellcasting.enabled && !hasTrackedSlots(npc.spellcasting))
            continue;

        anyCaster = true;
        CharacterSpellcastingModel spellcasting = npc.spellcasting;
        normalizeSpellcasting(spellcasting);
        const std::string label = (npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name) + "##NpcSpellSlots" + std::to_string(npcIndex);
        if (ImGui::TreeNodeEx(label.c_str()))
        {
            const std::string resetLabel = localizedUiLabel("Reset Used Slots") + "##NpcSlots" + std::to_string(npcIndex);
            if (ImGui::SmallButton(resetLabel.c_str()))
                _controller.resetNpcSpellSlots(npcIndex);
            if (ImGui::BeginTable(("NpcLiveSpellSlots" + std::to_string(npcIndex)).c_str(), 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
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
                    int maximum = spellcasting.slotMaximums[static_cast<std::size_t>(spellLevel)];
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Max").c_str(), &maximum))
                        _controller.setNpcSpellSlotMaximum(npcIndex, spellLevel, maximum);
                    ImGui::TableSetColumnIndex(2);
                    int used = spellcasting.slotUsed[static_cast<std::size_t>(spellLevel)];
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##Used").c_str(), &used))
                        _controller.setNpcSpellSlotUsed(npcIndex, spellLevel, used);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text(localizedUiLabel("%d").c_str(), std::max(0, maximum - used));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    if (!anyCaster)
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No player or NPC spell slot trackers are active."));
    showHelpTooltip("Live controls update the same source spell slots used by character sheets and combat.");
}
