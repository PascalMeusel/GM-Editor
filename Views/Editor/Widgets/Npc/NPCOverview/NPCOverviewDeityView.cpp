#include "NPCOverviewViewMonsterSupport.hpp"

void NPCOverviewView::drawDeityTab()
{
    Campaign &campaign = _controller._currentCampaign;
    if (ImGui::Button(localizedUiLabel("Create Deity/Patron").c_str()))
        _controller.createDeity();
    showHelpTooltip("Create a modeled deity, patron, power, pantheon figure, or homebrew religion entry.");

    if (campaign.deities.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No deity, patron, or power models exist yet. Create one here, then link NPCs to it from the NPC profile.").c_str());
        return;
    }

    if (_controller._selectedDeityIndex >= static_cast<int>(campaign.deities.size()))
        _controller._selectedDeityIndex = 0;

    const bool overviewWide = TaskUi::hasRoomForColumns(58.0f);
    if (!ImGui::BeginTable("DeityOverviewLayout", overviewWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn(localizedUiLabel("Directory").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
    if (overviewWide) ImGui::TableSetupColumn(localizedUiLabel("Selected Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.64f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    drawHelpText("Powers Directory", "Track deities, patrons, cult powers, pantheons, and homebrew religions as first-class campaign entities.");
    if (ImGui::BeginTable("DeityOverviewTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(overviewWide ? 0.7f : 0.3f))))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn(localizedUiLabel("Kind").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn(localizedUiLabel("Pantheon").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn(localizedUiLabel("Domains").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn(localizedUiLabel("Threat").c_str(), ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < static_cast<int>(campaign.deities.size()); ++i)
        {
            const DeityModel &deity = campaign.deities[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string displayName = deity.name.empty() ? localizedUiLabel("<Unnamed Power>") : deity.name;
            if (ImGui::Selectable(displayName.c_str(), _controller._selectedDeityIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                _controller._selectedDeityIndex = i;
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(deity.powerType);
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(deity.pantheon);
            ImGui::TableSetColumnIndex(3);
            tableTextWrapped(deity.domains);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text(localizedUiLabel("%d").c_str(), deity.threatLevel);
        }
        ImGui::EndTable();
    }

    ImGui::TableNextColumn();
    DeityModel &selectedDeity = campaign.deities[_controller._selectedDeityIndex];
    ImGui::SeparatorText(localizedUiLabel("Power Profile").c_str());
    showHelpTooltip("Editing saves immediately to the active campaign.");

    if (ImGui::BeginTable("SelectedDeityBasics", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Left").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn(localizedUiLabel("Right").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        std::string name = selectedDeity.name;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Name").c_str(), &name))
            _controller.setSelectedDeityName(name);
        std::string powerType = selectedDeity.powerType;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Kind").c_str(), &powerType))
            _controller.setSelectedDeityPowerType(powerType);
        std::string pantheon = selectedDeity.pantheon;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Pantheon").c_str(), &pantheon))
            _controller.setSelectedDeityPantheon(pantheon);
        std::string domains = selectedDeity.domains;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Domains").c_str(), &domains))
            _controller.setSelectedDeityDomains(domains);

        ImGui::TableSetColumnIndex(1);
        std::string symbol = selectedDeity.symbol;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Symbol").c_str(), &symbol))
            _controller.setSelectedDeitySymbol(symbol);
        std::string alignment = selectedDeity.alignment;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::InputText(localizedUiLabel("Alignment").c_str(), &alignment))
            _controller.setSelectedDeityAlignment(alignment);
        int influence = selectedDeity.influence;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::SliderInt(localizedUiLabel("Influence").c_str(), &influence, 0, 10))
            _controller.setSelectedDeityInfluence(influence);
        int threatLevel = selectedDeity.threatLevel;
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::SliderInt(localizedUiLabel("Threat").c_str(), &threatLevel, 0, 10))
            _controller.setSelectedDeityThreatLevel(threatLevel);
        ImGui::EndTable();
    }

    ImGui::SeparatorText(localizedUiLabel("Tenets").c_str());
    std::string tenets = selectedDeity.tenets;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##DeityTenets").c_str(), &tenets, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedDeityTenets(tenets);

    ImGui::SeparatorText(localizedUiLabel("Worshipers and Cults").c_str());
    std::string worshipers = selectedDeity.worshipers;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##DeityWorshipers").c_str(), &worshipers, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedDeityWorshipers(worshipers);

    ImGui::SeparatorText(localizedUiLabel("Relationships").c_str());
    std::string relationships = selectedDeity.relationships;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##DeityRelationships").c_str(), &relationships, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedDeityRelationships(relationships);

    ImGui::SeparatorText(localizedUiLabel("GM Notes").c_str());
    std::string notes = selectedDeity.gmNotes;
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::InputTextMultiline(localizedUiLabel("##DeityNotes").c_str(), &notes, ImVec2(-1.0f, 82.0f), ImGuiInputTextFlags_WordWrap))
        _controller.setSelectedDeityGmNotes(notes);

    ImGui::SeparatorText(localizedUiLabel("Linked NPCs").c_str());
    bool foundLinkedNpc = false;
    for (const NPCModel &npc : campaign.npcs)
    {
        if (npc.deityId == selectedDeity.metadata.id)
        {
            ImGui::BulletText("%s", npc.name.empty() ? localizedUiText("<Unnamed NPC>") : npc.name.c_str());
            foundLinkedNpc = true;
        }
    }
    if (!foundLinkedNpc)
        ImGui::TextDisabled(localizedUiLabel("No NPCs are linked to this power yet.").c_str());

    ImGui::EndTable();
}
