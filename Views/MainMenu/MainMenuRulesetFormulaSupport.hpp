#pragma once
#include "MainMenuRulesetListSupport.hpp"

namespace
{
void drawRulesetDerivedStatsEditor(std::vector<RulesetDerivedStatDefinition> &derivedStats,
                                   int &selectedIndex,
                                   const RulesetDefinitionModel &previewDefinition)
{
    ImGui::SeparatorText(localizedUiLabel("Derived Stats").c_str());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rulesetStudioText(
        "Add and preview formula-backed stats. Published definitions drive definition-backed runtime values; compiled D&D remains the fallback.",
        "Ergänze und prüfe formelbasierte Werte. Veröffentlichte Definitionen steuern definitionsbasierte Laufzeitwerte; das integrierte D&D-Regelwerk bleibt der Rückfall."));

    if (drawUiButton("Add Derived Stat", UiButtonRole::Primary, ImVec2(150.0f, 0.0f), "Create a new derived stat draft."))
    {
        RulesetDerivedStatDefinition stat;
        const int nextNumber = static_cast<int>(derivedStats.size()) + 1;
        stat.id = derivedStats.empty() ? "armor_class" : "derived_stat_" + std::to_string(nextNumber);
        stat.name = derivedStats.empty() ? "Armor Class" : "Derived Stat " + std::to_string(nextNumber);
        stat.formula = derivedStats.empty() ? "10 + strength.mod" : "level + proficiency";
        derivedStats.push_back(stat);
        selectedIndex = static_cast<int>(derivedStats.size()) - 1;
    }

    continueRulesetActionRow("Remove", 100.0f);
    if (drawUiButton("Remove",
                     selectedIndex >= 0 && selectedIndex < static_cast<int>(derivedStats.size()),
                     "Select a derived stat before removing it.",
                     UiButtonRole::Destructive,
                     ImVec2(100.0f, 0.0f),
                     "Remove the selected derived stat draft."))
    {
        derivedStats.erase(derivedStats.begin() + selectedIndex);
        selectedIndex = derivedStats.empty() ? -1 : std::clamp(selectedIndex, 0, static_cast<int>(derivedStats.size()) - 1);
    }

    if (derivedStats.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No derived stats yet.").c_str());
        return;
    }

    selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(derivedStats.size()) - 1);
    if (ImGui::BeginTable("RulesetDerivedStatsEditor", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(localizedUiLabel("List").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableSetupColumn(localizedUiLabel("Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.64f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginListBox("##DerivedStats", ImVec2(-1.0f, 190.0f)))
        {
            for (int index = 0; index < static_cast<int>(derivedStats.size()); ++index)
            {
                const RulesetDerivedStatDefinition &stat = derivedStats[static_cast<std::size_t>(index)];
                const std::string label = fallbackLabel(stat.name, "Untitled stat") + "##DerivedStat" + std::to_string(index);
                if (ImGui::Selectable(label.c_str(), selectedIndex == index))
                    selectedIndex = index;
            }
            ImGui::EndListBox();
        }

        ImGui::TableSetColumnIndex(1);
        RulesetDerivedStatDefinition &selected = derivedStats[static_cast<std::size_t>(selectedIndex)];
        ImGui::PushID("RulesetDerivedStatsEditorFields");
        drawRulesetTextField("##StableId", "Stable ID", selected.id, "armor_class");
        drawRulesetTextField("##DisplayLabel", "Label", selected.name, "Armor Class");
        drawRulesetTextField("##Formula", "Formula", selected.formula, "10 + dexterity.mod + shield");
        drawRulesetMultilineField("##Description", "Description", selected.description);
        drawRulesetFormulaPreview(previewDefinition, selected.formula);
        ImGui::PopID();
        ImGui::EndTable();
    }
}

void drawRulesetFormulaListsEditor(std::vector<RulesetDerivedStatDefinition> &derivedStats,
                                   int &selectedDerivedStatIndex,
                                   std::vector<RulesetDefenseDefinition> &defenses,
                                   int &selectedDefenseIndex,
                                   std::vector<RulesetResourceDefinition> &resources,
                                   int &selectedResourceIndex,
                                   std::vector<RulesetMonsterTemplateDefinition> &monsterTemplates,
                                   int &selectedMonsterTemplateIndex,
                                   const RulesetDefinitionModel &previewDefinition)
{
    ImGui::SeparatorText(localizedUiLabel("Formula-Backed Lists").c_str());
    ImGui::TextWrapped(localizedUiLabel("Preview formulas with example variables before saving. Diagnostics can jump back to these rows by ID.").c_str());

    if (ImGui::BeginTabBar("RulesetFormulaListTabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        if (ImGui::BeginTabItem(localizedUiLabel("Derived Stats").c_str()))
        {
            drawRulesetDerivedStatsEditor(derivedStats, selectedDerivedStatIndex, previewDefinition);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Defenses").c_str()))
        {
            drawRulesetListDetailEditor("Defenses", "Defense formulas can reference abilities, derived stats, resources, level, proficiency, party_level, shield, and hit_die_average.", "RulesetDefensesEditor", "Add Defense", defenses, selectedDefenseIndex, [](int next) {
                return RulesetDefenseDefinition{"defense_" + std::to_string(next), "Defense " + std::to_string(next), "10 + dexterity.mod", ""};
            }, [&](RulesetDefenseDefinition &defense) {
                drawRulesetTextField("##Formula", "Formula", defense.formula, "10 + dexterity.mod + shield");
                drawRulesetFormulaPreview(previewDefinition, defense.formula);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Resources").c_str()))
        {
            drawRulesetListDetailEditor("Resources", rulesetStudioText(
                "Resources expose maximum and recovery formulas to definition-backed sheets and rest workflows.",
                "Ressourcen stellen Maximums- und Erholungsformeln für definitionsbasierte Bögen und Rastabläufe bereit."), "RulesetResourcesEditor", "Add Resource", resources, selectedResourceIndex, [](int next) {
                return RulesetResourceDefinition{"resource_" + std::to_string(next), "Resource " + std::to_string(next), "level + proficiency", "proficiency", "Long Rest", ""};
            }, [&](RulesetResourceDefinition &resource) {
                drawRulesetTextField("##MaximumFormula", "Maximum Formula", resource.maximumFormula, "level + proficiency");
                drawRulesetFormulaPreview(previewDefinition, resource.maximumFormula);
                drawRulesetTextField("##RecoveryFormula", "Recovery Formula", resource.recoveryFormula, "proficiency");
                drawRulesetFormulaPreview(previewDefinition, resource.recoveryFormula);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Monster Templates").c_str()))
        {
            drawRulesetListDetailEditor("Monster Templates", "Monster templates carry size references and challenge formulas for future encounter/content generation.", "RulesetMonsterTemplatesEditor", "Add Template", monsterTemplates, selectedMonsterTemplateIndex, [](int next) {
                return RulesetMonsterTemplateDefinition{"monster_template_" + std::to_string(next), "Monster Template " + std::to_string(next), "", "party_level + proficiency", ""};
            }, [&](RulesetMonsterTemplateDefinition &monsterTemplate) {
                drawRulesetTextField("##SizeId", "Size ID", monsterTemplate.sizeId, "medium", 180.0f);
                drawRulesetTextField("##ChallengeFormula", "Challenge Formula", monsterTemplate.challengeFormula, "party_level + proficiency");
                drawRulesetFormulaPreview(previewDefinition, monsterTemplate.challengeFormula);
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void drawRulesetValidationDiagnosticsPanel(const std::vector<RulesetValidationDiagnostic> &diagnostics,
                                           RulesetLabStep &currentStep,
                                           int &selectedAbilityIndex,
                                           const std::vector<RulesetAbilityScoreDefinition> &abilities,
                                           int &selectedSkillIndex,
                                           const std::vector<RulesetSkillDefinition> &skills,
                                           int &selectedDerivedStatIndex,
                                           const std::vector<RulesetDerivedStatDefinition> &derivedStats,
                                           int &selectedDefenseIndex,
                                           const std::vector<RulesetDefenseDefinition> &defenses,
                                           int &selectedResourceIndex,
                                           const std::vector<RulesetResourceDefinition> &resources,
                                           int &selectedMonsterTemplateIndex,
                                           const std::vector<RulesetMonsterTemplateDefinition> &monsterTemplates)
{
    if (diagnostics.empty())
    {
        ImGui::TextColored(ImVec4(0.45f, 0.82f, 0.78f, 1.0f), localizedUiLabel("No validation diagnostics.").c_str());
        return;
    }

    if (ImGui::BeginTable("RulesetValidationDiagnostics", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(rulesetStudioText("Issue", "Problem"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Path").c_str(), ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Message").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Action").c_str(), ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableHeadersRow();

        for (int index = 0; index < static_cast<int>(diagnostics.size()); ++index)
        {
            const RulesetValidationDiagnostic &diagnostic = diagnostics[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(gm::rulesets::formatDiagnosticCode(
                diagnostic.code, gm::localization::LocalizationService::instance().locale()));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(diagnostic.path.empty()
                                 ? "-"
                                 : gm::rulesets::formatDiagnosticPath(
                                       diagnostic.path,
                                       gm::localization::LocalizationService::instance().locale()));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(gm::rulesets::formatValidationDiagnostic(
                diagnostic, gm::localization::LocalizationService::instance().locale()));
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(index);
            if (drawSmallUiButton("Go", UiButtonRole::Primary, "Jump to the lab section that owns this diagnostic."))
            {
                currentStep = rulesetLabStepForDiagnosticPath(diagnostic.path);
                if (const int rowIndex = definitionIndexForDiagnosticPath(diagnostic.path, "abilityScores.", abilities); rowIndex >= 0)
                    selectedAbilityIndex = rowIndex;
                if (const int rowIndex = definitionIndexForDiagnosticPath(diagnostic.path, "skills.", skills); rowIndex >= 0)
                    selectedSkillIndex = rowIndex;
                if (const int rowIndex = definitionIndexForDiagnosticPath(diagnostic.path, "derivedStats.", derivedStats); rowIndex >= 0)
                    selectedDerivedStatIndex = rowIndex;
                if (const int rowIndex = definitionIndexForDiagnosticPath(diagnostic.path, "defenses.", defenses); rowIndex >= 0)
                    selectedDefenseIndex = rowIndex;
                if (const int rowIndex = definitionIndexForDiagnosticPath(diagnostic.path, "resources.", resources); rowIndex >= 0)
                    selectedResourceIndex = rowIndex;
                if (const int rowIndex = definitionIndexForDiagnosticPath(diagnostic.path, "monsterTemplates.", monsterTemplates); rowIndex >= 0)
                    selectedMonsterTemplateIndex = rowIndex;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}
}
