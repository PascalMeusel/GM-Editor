#pragma once
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "SpellLibraryView.hpp"
#include "Controllers/Editor/Widgets/SpellLibrary/SpellLibraryController.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
constexpr int kSchoolCount = 8;
constexpr int kResolutionCount = 5;

void drawCatalogSpellDetails(const SpellModel &spell, const std::string &source)
{
    ImGui::TextDisabled(localizedUiLabel("Source: %s").c_str(), source.c_str());
    ImGui::Separator();
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), spell.name.empty() ? "<Unnamed Spell>" : spell.name.c_str());
    ImGui::TextDisabled(localizedUiLabel("%s | %s | %s").c_str(), spellLevelLabel(spell.level).c_str(), spellSchoolLabel(spell.school), spellResolutionLabel(spell.resolution));
    ImGui::TextDisabled(localizedUiLabel("Cast: %s | Range: %s | Duration: %s").c_str(),
                        spell.castingTime.empty() ? "-" : spell.castingTime.c_str(),
                        spell.range.empty() ? "-" : spell.range.c_str(),
                        spell.duration.empty() ? "-" : spell.duration.c_str());
    if (!spell.area.empty())
        ImGui::TextDisabled(localizedUiLabel("Area/Target: %s").c_str(), spell.area.c_str());
    if (spell.concentration || spell.ritual)
    {
        std::string traits;
        if (spell.concentration)
            traits = localizedUiLabel("Concentration");
        if (spell.ritual)
            traits += (traits.empty() ? "" : " · ") + localizedUiLabel("Ritual");
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), traits.c_str());
    }
    if (!spell.classes.empty())
        ImGui::TextDisabled(localizedUiLabel("Classes/Sources: %s").c_str(), spell.classes.c_str());

    if (!spell.attackBonus.empty() || !spell.savingThrowAbility.empty() || !spell.damage.empty() || !spell.healing.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Resolution"));
        if (!spell.attackBonus.empty())
            ImGui::TextDisabled(localizedUiLabel("Attack: %s").c_str(), spell.attackBonus.c_str());
        if (!spell.savingThrowAbility.empty())
            ImGui::TextDisabled(localizedUiLabel("Save: %s").c_str(), spell.savingThrowAbility.c_str());
        if (!spell.damage.empty())
            ImGui::TextDisabled(localizedUiLabel("Damage: %s").c_str(), spell.damage.c_str());
        if (!spell.healing.empty())
            ImGui::TextDisabled(localizedUiLabel("Healing: %s").c_str(), spell.healing.c_str());
    }

    if (!spell.scaling.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Scaling"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), spell.scaling.c_str());
    }
    if (!spell.effectSummary.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Effect"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), spell.effectSummary.c_str());
    }
    if (!spell.description.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Notes"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), spell.description.c_str());
    }
}

void drawWarningList(const std::vector<std::string> &warnings)
{
    if (warnings.empty())
        return;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.71f, 0.36f, 1.0f));
    ImGui::TextUnformatted(gm::localization::trUi("Warnings"));
    ImGui::PopStyleColor();
    for (const std::string &warning : warnings)
        ImGui::BulletText("%s", gm::localization::trUi(warning.c_str()));
}

std::vector<std::string> hiddenResolutionFields(const SpellModel &spell)
{
    std::vector<std::string> fields;
    switch (spell.resolution)
    {
    case SpellResolutionType::Utility:
        if (!spell.attackBonus.empty())
            fields.push_back("Attack Bonus Override");
        if (!spell.savingThrowAbility.empty())
            fields.push_back("Saving Throw");
        if (!spell.damage.empty())
            fields.push_back("Damage");
        if (!spell.healing.empty())
            fields.push_back("Healing");
        break;
    case SpellResolutionType::SpellAttack:
        if (!spell.savingThrowAbility.empty())
            fields.push_back("Saving Throw");
        if (!spell.healing.empty())
            fields.push_back("Healing");
        break;
    case SpellResolutionType::SavingThrow:
        if (!spell.attackBonus.empty())
            fields.push_back("Attack Bonus Override");
        if (!spell.healing.empty())
            fields.push_back("Healing");
        break;
    case SpellResolutionType::Healing:
        if (!spell.attackBonus.empty())
            fields.push_back("Attack Bonus Override");
        if (!spell.savingThrowAbility.empty())
            fields.push_back("Saving Throw");
        if (!spell.damage.empty())
            fields.push_back("Damage");
        break;
    case SpellResolutionType::SummonOrZone:
        if (!spell.attackBonus.empty())
            fields.push_back("Attack Bonus Override");
        if (!spell.savingThrowAbility.empty())
            fields.push_back("Saving Throw");
        if (!spell.healing.empty())
            fields.push_back("Healing");
        break;
    }
    return fields;
}

std::string joinFieldNames(const std::vector<std::string> &fields)
{
    std::string result;
    for (const std::string &field : fields)
    {
        if (!result.empty())
            result += ", ";
        result += field;
    }
    return result;
}
}
