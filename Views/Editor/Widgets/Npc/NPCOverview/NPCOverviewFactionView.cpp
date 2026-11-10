#include "NPCOverviewViewMonsterSupport.hpp"

void NPCOverviewView::drawFactionTab()
{
    Campaign &campaign = _controller._currentCampaign;
    if (ImGui::Button(localizedUiLabel("Create Faction").c_str()))
        _controller.createFaction();
    showHelpTooltip("Create a modeled faction that NPCs can reference directly.");

    if (campaign.factions.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No modeled factions exist yet. Create one here, then assign NPCs to it from the NPC profile.").c_str());
        return;
    }

    if (_controller._selectedFactionIndex >= static_cast<int>(campaign.factions.size()))
        _controller._selectedFactionIndex = 0;

    const bool overviewWide = TaskUi::hasRoomForColumns(58.0f);
    if (!ImGui::BeginTable("FactionOverviewLayout", overviewWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Directory").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
    if (overviewWide) ImGui::TableSetupColumn(localizedUiLabel("Selected Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.64f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    drawHelpText("Faction Directory", "Modeled factions can collect goals, resources, leaders, attitude, and linked member NPCs.");
    if (ImGui::BeginTable("FactionOverviewTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(overviewWide ? 0.7f : 0.3f))))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Attitude").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Threat").c_str(), ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(campaign.factions.size()); ++i)
        {
            const FactionModel &faction = campaign.factions[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string displayName = faction.name.empty() ? "<Unnamed Faction>" : faction.name;
            if (ImGui::Selectable(displayName.c_str(), _controller._selectedFactionIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                _controller._selectedFactionIndex = i;
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(faction.type);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(faction.attitudeTowardParty);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text(localizedUiLabel("%d").c_str(), faction.threatLevel);
        }
        ImGui::EndTable();
    }

    ImGui::TableNextColumn();
    FactionModel &selectedFaction = campaign.factions[_controller._selectedFactionIndex];

    ImGui::SeparatorText(localizedUiLabel("Faction Profile").c_str());
    showHelpTooltip("Editing saves immediately to the active campaign.");
    if (ImGui::BeginTable("SelectedFactionBasics", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        std::string name = selectedFaction.name;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Name").c_str(), &name))
            _controller.setSelectedFactionName(name);
        std::string type = selectedFaction.type;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Type").c_str(), &type))
            _controller.setSelectedFactionType(type);
        std::string headquarters = selectedFaction.headquarters;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Headquarters").c_str(), &headquarters))
            _controller.setSelectedFactionHeadquarters(headquarters);

        ImGui::TableSetColumnIndex(1);
        int influence = selectedFaction.influence;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::SliderInt(localizedUiLabel("Influence").c_str(), &influence, 0, 10))
            _controller.setSelectedFactionInfluence(influence);
        int threatLevel = selectedFaction.threatLevel;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::SliderInt(localizedUiLabel("Threat").c_str(), &threatLevel, 0, 10))
            _controller.setSelectedFactionThreatLevel(threatLevel);

        std::string leaderLabel = "<No Leader>";
        for (const NPCModel &npc : campaign.npcs)
        {
            if (npc.metadata.id == selectedFaction.leaderNpcId)
            {
                leaderLabel = npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name;
                break;
            }
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo(localizedUiLabel("Leader").c_str(), leaderLabel.c_str()))
        {
            if (ImGui::Selectable(localizedUiLabel("<No Leader>").c_str(), selectedFaction.leaderNpcId.empty()))
                _controller.setSelectedFactionLeaderNpcId("");
            for (const NPCModel &npc : campaign.npcs)
            {
                const bool selected = selectedFaction.leaderNpcId == npc.metadata.id;
                const std::string label = npc.name.empty() ? localizedUiLabel("<Unnamed NPC>") : npc.name;
                if (ImGui::Selectable(label.c_str(), selected))
                    _controller.setSelectedFactionLeaderNpcId(npc.metadata.id);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }

    std::string attitude = selectedFaction.attitudeTowardParty;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputText(localizedUiLabel("Attitude Toward Party").c_str(), &attitude))
        _controller.setSelectedFactionAttitude(attitude);

    ImGui::SeparatorText(localizedUiLabel("Summary").c_str());
    std::string summary = selectedFaction.summary;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##FactionSummary").c_str(), &summary, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedFactionSummary(summary);

    ImGui::SeparatorText(localizedUiLabel("Goals").c_str());
    std::string goals = selectedFaction.goals;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##FactionGoals").c_str(), &goals, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedFactionGoals(goals);

    ImGui::SeparatorText(localizedUiLabel("Resources").c_str());
    std::string resources = selectedFaction.resources;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##FactionResources").c_str(), &resources, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedFactionResources(resources);

    ImGui::SeparatorText(localizedUiLabel("GM Notes").c_str());
    std::string notes = selectedFaction.gmNotes;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##FactionNotes").c_str(), &notes, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedFactionGmNotes(notes);

    ImGui::SeparatorText(localizedUiLabel("Member NPCs").c_str());
    bool foundMember = false;
    for (const NPCModel &npc : campaign.npcs)
    {
        if (npc.factionId == selectedFaction.metadata.id)
        {
            ImGui::BulletText("%s", npc.name.empty() ? localizedUiText("<Unnamed NPC>") : npc.name.c_str());
            foundMember = true;
        }
    }
    if (!foundMember)
        ImGui::TextDisabled(localizedUiLabel("No NPCs are assigned to this faction yet.").c_str());

    ImGui::EndTable();
}
