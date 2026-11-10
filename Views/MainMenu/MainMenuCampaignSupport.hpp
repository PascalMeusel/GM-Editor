#pragma once
#include "MainMenuRulesetFormulaSupport.hpp"

namespace
{
bool validateCampaignName(const std::string &campaignName, std::string &error)
{
    const std::string trimmedName = trimCopy(campaignName);
    if (trimmedName.empty())
    {
        error = launcherText("Campaign name is required.", "Ein Kampagnenname ist erforderlich.");
        return false;
    }
    return true;
}

bool validateCampaignRuleset(const std::vector<std::shared_ptr<IRuleSet>> &ruleSets, int selectedRuleIndex, std::string &error)
{
    if (ruleSets.empty())
    {
        error = launcherText("No rulesets are available.", "Es sind keine Regelwerke verfügbar.");
        return false;
    }
    if (selectedRuleIndex < 0 || selectedRuleIndex >= static_cast<int>(ruleSets.size()))
    {
        error = launcherText("Choose a ruleset before creating the campaign.", "Wähle ein Regelwerk, bevor du die Kampagne erstellst.");
        return false;
    }
    return true;
}

void drawCampaignCountRow(const char *label, int value)
{
    ImGui::TableNextColumn();
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), localizedUiText(label));
    ImGui::SameLine();
    ImGui::TextUnformatted(std::to_string(value).c_str());
}

void drawCampaignSummaryTable(const Campaign &campaign)
{
    if (ImGui::BeginTable("SelectedCampaignSummary", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextRow();
        drawCampaignCountRow("Players", static_cast<int>(campaign.characters.size()));
        drawCampaignCountRow("NPCs", static_cast<int>(campaign.npcs.size()));
        drawCampaignCountRow("Maps", static_cast<int>(campaign.maps.size()));

        ImGui::TableNextRow();
        drawCampaignCountRow("Stories", static_cast<int>(campaign.storyThreads.size()));
        drawCampaignCountRow("Encounters", static_cast<int>(campaign.encounters.size()));
        drawCampaignCountRow("Sessions", static_cast<int>(campaign.sessions.size()));
        ImGui::EndTable();
    }
}

void drawSelectedCampaignDetail(const Campaign *selectedCampaign)
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LauncherSelectedCampaign));
    if (!selectedCampaign)
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LauncherNoSelectionBody));
        return;
    }

    if (getBoldFont())
        ImGui::PushFont(getBoldFont());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), selectedCampaign->name.c_str());
    if (getBoldFont())
        ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, gm::approved_ui::Muted);
    ImGui::TextWrapped(localizedUiLabel("Ruleset: %s").c_str(), rulesetName(*selectedCampaign).c_str());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), campaignUpdatedLabel(*selectedCampaign).c_str());
    ImGui::PopStyleColor();

    const ResumeStateSummary resumeState = resumeStateForCampaign(*selectedCampaign);
    if (resumeState.available)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.82f, 0.78f, 1.0f));
        ImGui::TextWrapped(localizedUiLabel("%s: %s").c_str(), resumeState.mode.c_str(), resumeState.selection.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    drawCampaignSummaryTable(*selectedCampaign);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    if (!selectedCampaign->description.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), selectedCampaign->description.c_str());
    else
        ImGui::TextColored(gm::approved_ui::Muted, localizedUiLabel("No description yet.").c_str());
}

void resetNewCampaignDraft(std::string &name,
                           std::string &description,
                           std::string &error,
                           int &selectedRuleIndex,
                           int &selectedTemplateIndex,
                           NewCampaignWizardStep &wizardStep)
{
    name.clear();
    description.clear();
    error.clear();
    selectedRuleIndex = 0;
    selectedTemplateIndex = 1;
    wizardStep = NewCampaignWizardStep::Name;
}

void resetRulesetDraft(std::string &name,
                       std::string &version,
                       std::string &baseId,
                       std::vector<RulesetAbilityScoreDefinition> &abilities,
                       int &selectedAbilityIndex,
                       std::vector<RulesetSkillDefinition> &skills,
                       int &selectedSkillIndex,
                       std::vector<RulesetDerivedStatDefinition> &derivedStats,
                       int &selectedDerivedStatIndex,
                       std::vector<RulesetDefenseDefinition> &defenses,
                       int &selectedDefenseIndex,
                       std::vector<RulesetResourceDefinition> &resources,
                       int &selectedResourceIndex,
                       std::vector<RulesetCurrencyDefinition> &currencies,
                       int &selectedCurrencyIndex,
                       std::vector<RulesetSizeDefinition> &sizes,
                       int &selectedSizeIndex,
                       std::vector<RulesetSpeedDefinition> &speeds,
                       int &selectedSpeedIndex,
                       std::vector<RulesetConditionDefinition> &conditions,
                       int &selectedConditionIndex,
                       std::vector<RulesetClassDefinition> &classes,
                       int &selectedClassIndex,
                       std::vector<RulesetAncestryDefinition> &ancestries,
                       int &selectedAncestryIndex,
                       std::vector<RulesetBackgroundDefinition> &backgrounds,
                       int &selectedBackgroundIndex,
                       std::vector<RulesetFeatureDefinition> &features,
                       int &selectedFeatureIndex,
                       std::vector<RulesetEquipmentCategoryDefinition> &equipmentCategories,
                       int &selectedEquipmentCategoryIndex,
                       std::vector<RulesetSpellTraditionDefinition> &spellTraditions,
                       int &selectedSpellTraditionIndex,
                       std::vector<RulesetMonsterTemplateDefinition> &monsterTemplates,
                       int &selectedMonsterTemplateIndex,
                       std::string &error,
                       int &selectedTemplateIndex,
                       RulesetLabStep &labStep)
{
    name.clear();
    version = "1.0.0";
    selectedTemplateIndex = 0;
    applyRulesetTemplateIntent(selectedTemplateIndex,
                               baseId,
                               abilities,
                               skills,
                               derivedStats,
                               defenses,
                               resources,
                               currencies,
                               sizes,
                               speeds,
                               conditions,
                               classes,
                               ancestries,
                               backgrounds,
                               features,
                               equipmentCategories,
                               spellTraditions,
                               monsterTemplates);
    selectedAbilityIndex = abilities.empty() ? -1 : 0;
    selectedSkillIndex = skills.empty() ? -1 : 0;
    selectedDerivedStatIndex = derivedStats.empty() ? -1 : 0;
    selectedDefenseIndex = defenses.empty() ? -1 : 0;
    selectedResourceIndex = resources.empty() ? -1 : 0;
    selectedCurrencyIndex = currencies.empty() ? -1 : 0;
    selectedSizeIndex = sizes.empty() ? -1 : 0;
    selectedSpeedIndex = speeds.empty() ? -1 : 0;
    selectedConditionIndex = conditions.empty() ? -1 : 0;
    selectedClassIndex = classes.empty() ? -1 : 0;
    selectedAncestryIndex = ancestries.empty() ? -1 : 0;
    selectedBackgroundIndex = backgrounds.empty() ? -1 : 0;
    selectedFeatureIndex = features.empty() ? -1 : 0;
    selectedEquipmentCategoryIndex = equipmentCategories.empty() ? -1 : 0;
    selectedSpellTraditionIndex = spellTraditions.empty() ? -1 : 0;
    selectedMonsterTemplateIndex = monsterTemplates.empty() ? -1 : 0;
    labStep = RulesetLabStep::Basics;
    error.clear();
}
}
