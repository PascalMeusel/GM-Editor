#pragma once
#include "MainMenuRulesetDraftSupport.hpp"

namespace
{
template <typename T, typename CreateDefault, typename DrawExtra>
void drawRulesetListDetailEditor(const char *title,
                                 const char *help,
                                 const char *tableId,
                                 const char *addLabel,
                                 std::vector<T> &definitions,
                                 int &selectedIndex,
                                 CreateDefault createDefault,
                                 DrawExtra drawExtra)
{
    ImGui::SeparatorText(localizedUiLabel(title).c_str());
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiText(help));

    if (drawUiButton(addLabel, UiButtonRole::Primary, ImVec2(160.0f, 0.0f), "Add a new ruleset definition row."))
    {
        definitions.push_back(createDefault(static_cast<int>(definitions.size()) + 1));
        selectedIndex = static_cast<int>(definitions.size()) - 1;
    }
    continueRulesetActionRow("Remove", 100.0f);
    if (drawUiButton("Remove",
                     selectedIndex >= 0 && selectedIndex < static_cast<int>(definitions.size()),
                     "Select a row before removing it.",
                     UiButtonRole::Destructive,
                     ImVec2(100.0f, 0.0f),
                     "Remove the selected ruleset definition row."))
    {
        definitions.erase(definitions.begin() + selectedIndex);
        selectedIndex = definitions.empty() ? -1 : std::clamp(selectedIndex, 0, static_cast<int>(definitions.size()) - 1);
    }

    if (definitions.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No rows yet.").c_str());
        return;
    }

    selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(definitions.size()) - 1);
    if (ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn(localizedUiLabel("List").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn(localizedUiLabel("Detail").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.66f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const std::string listId = std::string("##") + tableId + "List";
        if (ImGui::BeginListBox(listId.c_str(), ImVec2(-1.0f, 190.0f)))
        {
            for (int index = 0; index < static_cast<int>(definitions.size()); ++index)
            {
                const T &definition = definitions[static_cast<std::size_t>(index)];
                std::string label = fallbackLabel(definition.name, "Untitled") + "##" + tableId + std::to_string(index);
                if (!definition.id.empty())
                    label = fallbackLabel(definition.name, definition.id.c_str()) + "##" + tableId + std::to_string(index);
                if (ImGui::Selectable(label.c_str(), selectedIndex == index))
                    selectedIndex = index;
            }
            ImGui::EndListBox();
        }

        ImGui::TableSetColumnIndex(1);
        T &selected = definitions[static_cast<std::size_t>(selectedIndex)];
        ImGui::PushID(tableId);
        drawRulesetTextField("##StableId", "Stable ID", selected.id, "lowercase_id");
        drawRulesetTextField("##DisplayLabel", "Label", selected.name, "Display label");
        drawExtra(selected);
        drawRulesetMultilineField("##Description", "Description", selected.description);
        ImGui::PopID();
        ImGui::EndTable();
    }
}

void drawRulesetCoreListsEditor(std::vector<RulesetAbilityScoreDefinition> &abilities,
                                int &selectedAbilityIndex,
                                std::vector<RulesetSkillDefinition> &skills,
                                int &selectedSkillIndex,
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
                                int &selectedSpellTraditionIndex)
{
    ImGui::SeparatorText(localizedUiLabel("Core Ruleset Lists").c_str());
    ImGui::TextWrapped("%s", rulesetStudioText(
        "Define the abilities, skills, and other lists for your ruleset. Not every list changes play yet; only supported rules are applied in the game.",
        "Lege Attribute, Fertigkeiten und weitere Listen für dein Regelwerk fest. Noch nicht jede Liste wirkt sich auf das Spiel aus; nur unterstützte Regeln werden angewendet."));

    if (ImGui::BeginTabBar("RulesetCoreListTabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        if (ImGui::BeginTabItem(localizedUiLabel("Abilities").c_str()))
        {
            drawRulesetListDetailEditor("Ability Scores", "Ability rows define formula symbols such as strength and strength.mod.", "RulesetAbilitiesEditor", "Add Ability", abilities, selectedAbilityIndex, [](int next) {
                return RulesetAbilityScoreDefinition{"ability_" + std::to_string(next), "Ability " + std::to_string(next), "ABL", ""};
            }, [](RulesetAbilityScoreDefinition &ability) {
                drawRulesetTextField("##Abbreviation", "Abbreviation", ability.abbreviation, "STR", 120.0f);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Skills").c_str()))
        {
            drawRulesetListDetailEditor("Skills", "Skills can reference an ability score ID; diagnostics jump back here when the reference is missing.", "RulesetSkillsEditor", "Add Skill", skills, selectedSkillIndex, [](int next) {
                return RulesetSkillDefinition{"skill_" + std::to_string(next), "Skill " + std::to_string(next), "", ""};
            }, [](RulesetSkillDefinition &skill) {
                drawRulesetTextField("##AbilityScoreId", "Ability Score ID", skill.abilityScoreId, "strength");
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Currencies").c_str()))
        {
            drawRulesetListDetailEditor("Currencies", "Currency rows define denomination labels and base-unit values for inventory workflows.", "RulesetCurrenciesEditor", "Add Currency", currencies, selectedCurrencyIndex, [](int next) {
                RulesetCurrencyDefinition currency;
                currency.id = "currency_" + std::to_string(next);
                currency.name = "Currency " + std::to_string(next);
                currency.code = "cur";
                currency.baseUnitValue = 1;
                return currency;
            }, [](RulesetCurrencyDefinition &currency) {
                drawRulesetTextField("##Code", "Code", currency.code, "gp", 120.0f);
                drawRulesetIntegerField("##BaseUnitValue", "Base Unit Value", currency.baseUnitValue, 120.0f);
                currency.baseUnitValue = std::max(1, currency.baseUnitValue);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Sizes").c_str()))
        {
            drawRulesetListDetailEditor("Sizes", "Sizes define default battlefield space.", "RulesetSizesEditor", "Add Size", sizes, selectedSizeIndex, [](int next) {
                RulesetSizeDefinition size;
                size.id = "size_" + std::to_string(next);
                size.name = "Size " + std::to_string(next);
                size.spaceFeet = 5;
                return size;
            }, [](RulesetSizeDefinition &size) {
                drawRulesetIntegerField("##SpaceFeet", "Space Feet", size.spaceFeet, 120.0f);
                size.spaceFeet = std::max(0, size.spaceFeet);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Speeds").c_str()))
        {
            drawRulesetListDetailEditor("Speeds", "Speed rows name movement modes and units.", "RulesetSpeedsEditor", "Add Speed", speeds, selectedSpeedIndex, [](int next) {
                return RulesetSpeedDefinition{"speed_" + std::to_string(next), "Speed " + std::to_string(next), "feet", ""};
            }, [](RulesetSpeedDefinition &speed) {
                drawRulesetTextField("##Unit", "Unit", speed.unit, "feet", 160.0f);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Conditions").c_str()))
        {
            drawRulesetListDetailEditor("Conditions", "Conditions collect table-facing rules text and ending guidance.", "RulesetConditionsEditor", "Add Condition", conditions, selectedConditionIndex, [](int next) {
                return RulesetConditionDefinition{"condition_" + std::to_string(next), "Condition " + std::to_string(next), "", {}, ""};
            }, [](RulesetConditionDefinition &condition) {
                std::string effects = joinCommaSeparatedIds(condition.mechanicalEffects);
                if (drawRulesetTextField("##MechanicalEffects", "Mechanical Effects", effects, "disadvantage, cannot_move"))
                    condition.mechanicalEffects = splitCommaSeparatedIds(effects);
                drawRulesetTextField("##EndCondition", "End Condition", condition.endCondition, "Save succeeds");
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Classes").c_str()))
        {
            drawRulesetListDetailEditor("Classes", "Class rows can reference primary ability score IDs.", "RulesetClassesEditor", "Add Class", classes, selectedClassIndex, [](int next) {
                return RulesetClassDefinition{"class_" + std::to_string(next), "Class " + std::to_string(next), "d8", {}, ""};
            }, [](RulesetClassDefinition &classDefinition) {
                drawRulesetTextField("##HitDie", "Hit Die", classDefinition.hitDie, "d8", 120.0f);
                std::string ids = joinCommaSeparatedIds(classDefinition.primaryAbilityScoreIds);
                if (drawRulesetTextField("##PrimaryAbilityIds", "Primary Ability IDs", ids, "strength, dexterity"))
                    classDefinition.primaryAbilityScoreIds = splitCommaSeparatedIds(ids);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Ancestries").c_str()))
        {
            drawRulesetListDetailEditor("Ancestries / Species", "Ancestry rows can reference size and speed rows.", "RulesetAncestriesEditor", "Add Ancestry", ancestries, selectedAncestryIndex, [](int next) {
                return RulesetAncestryDefinition{"ancestry_" + std::to_string(next), "Ancestry " + std::to_string(next), "", "", ""};
            }, [](RulesetAncestryDefinition &ancestry) {
                drawRulesetTextField("##SizeId", "Size ID", ancestry.sizeId, "medium", 180.0f);
                drawRulesetTextField("##SpeedId", "Speed ID", ancestry.speedId, "walk", 180.0f);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Backgrounds").c_str()))
        {
            drawRulesetListDetailEditor("Backgrounds", "Background rows can reference skill IDs.", "RulesetBackgroundsEditor", "Add Background", backgrounds, selectedBackgroundIndex, [](int next) {
                return RulesetBackgroundDefinition{"background_" + std::to_string(next), "Background " + std::to_string(next), {}, ""};
            }, [](RulesetBackgroundDefinition &background) {
                std::string ids = joinCommaSeparatedIds(background.skillIds);
                if (drawRulesetTextField("##SkillIds", "Skill IDs", ids, "athletics, perception"))
                    background.skillIds = splitCommaSeparatedIds(ids);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Features").c_str()))
        {
            drawRulesetListDetailEditor("Features", "Feature rows define level-gated class, ancestry, background, item, or custom features.", "RulesetFeaturesEditor", "Add Feature", features, selectedFeatureIndex, [](int next) {
                RulesetFeatureDefinition feature;
                feature.id = "feature_" + std::to_string(next);
                feature.name = "Feature " + std::to_string(next);
                feature.featureType = "class";
                feature.level = 1;
                return feature;
            }, [](RulesetFeatureDefinition &feature) {
                drawRulesetTextField("##FeatureType", "Feature Type", feature.featureType, "class", 160.0f);
                drawRulesetIntegerField("##Level", "Level", feature.level, 120.0f);
                feature.level = std::max(1, feature.level);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Equipment").c_str()))
        {
            drawRulesetListDetailEditor("Equipment Categories", "Equipment categories define inventory grouping and slot hints.", "RulesetEquipmentCategoriesEditor", "Add Category", equipmentCategories, selectedEquipmentCategoryIndex, [](int next) {
                return RulesetEquipmentCategoryDefinition{"equipment_" + std::to_string(next), "Equipment " + std::to_string(next), "", ""};
            }, [](RulesetEquipmentCategoryDefinition &category) {
                drawRulesetTextField("##SlotType", "Slot Type", category.slotType, "Main Hand", 180.0f);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(localizedUiLabel("Spell Traditions").c_str()))
        {
            drawRulesetListDetailEditor("Spell Traditions", "Spell traditions can reference their casting ability score.", "RulesetSpellTraditionsEditor", "Add Tradition", spellTraditions, selectedSpellTraditionIndex, [](int next) {
                return RulesetSpellTraditionDefinition{"tradition_" + std::to_string(next), "Tradition " + std::to_string(next), "", ""};
            }, [](RulesetSpellTraditionDefinition &tradition) {
                drawRulesetTextField("##AbilityScoreId", "Ability Score ID", tradition.abilityScoreId, "intelligence", 180.0f);
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
}
