#include "NPCOverviewViewMonsterSupport.hpp"

NPCOverviewView::NPCOverviewView(NPCOverviewController &controller)
    : DockableWidget("NPC & Monster Overview", 0, ImVec2(1080.0f, 700.0f)), _controller(controller)
{
}

void NPCOverviewView::draw()
{
    if (_controller._currentCampaign.presentationModeActive)
    {
        resetReadingContext();
        ImGui::TextWrapped("%s", uiTextForLocale("NPC reference is private. Leave presentation mode to read or edit it.",
            "Die NSC-Referenz ist privat. Beende den Präsentationsmodus, um sie zu lesen oder zu bearbeiten."));
        return;
    }
    if (_controller._saveFailed)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", _controller._saveErrorMessage.c_str());
        ImGui::PopStyleColor();
        if (drawUiButton("Retry Save", true, nullptr, UiButtonRole::Primary))
            _controller.retrySave();
    }

    Campaign &campaign = _controller._currentCampaign;
    const std::string actorSelection = std::to_string(campaign.npcs.size()) + " " + localizedUiLabel("NPCs") + ", " +
                                       std::to_string(campaign.customMonsters.size()) + " " + localizedUiLabel("monsters") + ", " +
                                       std::to_string(campaign.factions.size()) + " " + localizedUiLabel("factions") + ", " +
                                       std::to_string(campaign.deities.size()) + " " + localizedUiLabel("powers");
    UiStatusActionStrip strip;
    strip.mode = "World Actors";
    strip.selection = actorSelection.c_str();
    strip.nextAction = "Choose a tab, then select an actor for a summary-first detail panel.";
    strip.help = "Changes are saved as you edit. A failed save retains your changes and offers Retry Save.";
    drawStatusActionStrip("##NpcOverviewStatus", strip);

    if (!ImGui::BeginTabBar("NpcOverviewTabs"))
        return;

    if (ImGui::BeginTabItem(localizedUiLabel("NPCs").c_str()))
    {
        drawNpcTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(localizedUiLabel("Monsters").c_str()))
    {
        drawMonsterTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(localizedUiLabel("Factions").c_str()))
    {
        drawFactionTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(localizedUiLabel("Deities & Patrons").c_str()))
    {
        drawDeityTab();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}
