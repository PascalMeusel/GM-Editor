#include "CharacterSheetView.hpp"
#include "Controllers/Editor/Widgets/CharacterSheet/CharacterSheetController.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include "Models/Rulesets/DND/DND5e.hpp"
#include "Models/Rulesets/DefinitionBackedRuleSet.hpp"
#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cctype>

void CharacterSheetView::drawAdvancement(bool open)
{
    const auto tr = [](const char *s) { return gm::localization::trUi(s); };
    if (open)
        ImGui::OpenPopup(tr("Level Up Character"));
    const auto available = ImGui::GetMainViewport()->WorkSize;
    const ImVec2 maximum(std::max(280.f, available.x - 32.f), std::max(260.f, available.y - 32.f));
    ImGui::SetNextWindowSize(ImVec2(std::min(760.f, maximum.x), std::min(650.f, maximum.y)), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(400.f, maximum.x), std::min(350.f, maximum.y)), maximum);
    if (!ImGui::BeginPopupModal(tr("Level Up Character"), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    auto &choice = _controller._levelUpChoices;
    auto &campaign = _controller._currentCampaign;
    const auto &base = _controller._levelUpBase;
    const auto spells = getAvailableSpells(campaign);
    const bool automatic = campaign.ruleSet && dynamic_cast<const DND5e *>(campaign.ruleSet.get());
    auto result = campaign.ruleSet ? CharacterAdvancement::preview(base, *campaign.ruleSet,
                                                                   _controller.advancementRequest(), &spells)
                                   : CharacterAdvancement::Preview{};
    const char *steps[] = {"Class and HP", "Choices", "Magic and resources", "Review"};
    ImGui::TextWrapped("%s — %s", base.nameBuffer, tr(steps[_controller._levelUpStep]));
    ImGui::TextDisabled("%d / 4", _controller._levelUpStep + 1);
    const float footer = ImGui::GetFrameHeightWithSpacing() * (_controller._levelUpStep == 3 ? 3.5f : 1.6f);
    ImGui::BeginChild("AdvancementBody", ImVec2(0, -footer), false);
    switch (_controller._levelUpStep)
    {
    case 0:
    {
        ImGui::TextWrapped("%s", tr("Choose the class receiving the new levels. Total level includes all classes."));
        FormUi::InputInt(tr("Target Level"), &_controller._levelUpTargetLevel);
        if (FormUi::BeginCombo(tr("Advancing class"), tr(choice.advancingClass.c_str())))
        {
            if (campaign.ruleSet)
                for (const auto &name : campaign.ruleSet->getClasses())
                    if (ImGui::Selectable(tr(name.c_str()), choice.advancingClass == name))
                        choice.advancingClass = name;
            ImGui::EndCombo();
        }
        for (const auto &[name, n] : CharacterAdvancement::classLevels(base))
            ImGui::Text("%s: %d", tr(name.c_str()), n);
        const bool fixed =
            campaign.ruleSet && campaign.ruleSet->advancementProfile(choice.advancingClass).fixedHitPointsPerLevel > 0;
        if (fixed)
            FormUi::Checkbox(tr("Override calculated HP gain"), &_controller._levelUpManualHitPoints);
        else
        {
            _controller._levelUpManualHitPoints = true;
            ImGui::TextWrapped("%s", tr("This class requires an explicit manual HP gain."));
        }
        if (_controller._levelUpManualHitPoints)
            FormUi::InputInt(tr("Manual class HP gain"), &_controller._levelUpHitPointGain);
        const bool de = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
        if (campaign.ruleSet && ImGui::CollapsingHeader(de ? "Berechnung und Quelle" : "Calculation and source"))
        {
            const auto profile = campaign.ruleSet->advancementProfile(choice.advancingClass);
            const int modifier = profile.hitPointAbility.empty() ? 0 : campaign.ruleSet->getAbilityModifier(base.attributes, profile.hitPointAbility);
            ImGui::TextWrapped("%s: %d; %s: %+d; %s: %d", de ? "Klassenwert pro Stufe" : "Class gain per level", profile.fixedHitPointsPerLevel,
                de ? "Attributsmodifikator" : "Ability modifier", modifier, de ? "Neue Stufen" : "Gained levels", result.gainedLevels);
            ImGui::TextWrapped("%s", de
                ? "Automatisch: max(1, Klassenwert + Modifikator) je neuer Stufe. Ein manueller Klassenwert ersetzt den automatischen Zuwachs; die endgültige Änderung steht unter Prüfung."
                : "Automatic: max(1, class gain + modifier) per gained level. Manual class HP replaces the automatic gain; Review shows the final change.");
            if (automatic) ImGui::TextWrapped("%s", de
                ? "Eine Konstitutionsänderung gilt zusätzlich für alle erreichten Stufen. Quelle: SRD 5.2.1, Klassen-TP (S. 23); gemischte SRD-Regelhilfe."
                : "A Constitution change also applies to all attained levels. Source: SRD 5.2.1 class hit points (p. 23); mixed SRD assistance.");
            else ImGui::TextWrapped("%s", de ? "Quelle: fixierte Regelwerksdefinition dieser Kampagne." : "Source: this campaign's pinned ruleset definition.");
        }
        FormUi::Checkbox(tr("Restore Health"), &_controller._levelUpRestoreHealth);
        if (automatic)
            ImGui::TextWrapped("%s", tr("Constitution adjustments are included for all attained levels. Spent slots and resources stay spent."));
        else
            ImGui::TextWrapped("%s", de ? "Verbrauchte Zauberplätze und Ressourcen bleiben verbraucht."
                                      : "Spent slots and resources stay spent.");
        break;
    }
    case 1:
    {
        ImGui::TextWrapped("%s", tr("Record class and subclass choices here. Confirm feat prerequisites and feature "
                                    "effects against your rules source."));
        FormUi::InputTextMultiline(tr("New features"), &_controller._levelUpFeatures,
                                   ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 3));
        if (const auto *defined = dynamic_cast<const DefinitionBackedRuleSet *>(campaign.ruleSet.get()))
        {
            if (FormUi::BeginCombo(tr("Feature from ruleset"), tr("Choose a feature")))
            {
                for (const auto &feature : defined->definition().features)
                    if (feature.level <= _controller._levelUpTargetLevel)
                        if (ImGui::Selectable(feature.name.c_str()))
                        {
                            if (!_controller._levelUpFeatures.empty())
                                _controller._levelUpFeatures += '\n';
                            _controller._levelUpFeatures += feature.name;
                        }
                ImGui::EndCombo();
            }
        }
        ImGui::TextWrapped("%s: %d", tr("Feat or ability improvement choices"), result.featChoices);
        FormUi::InputTextMultiline(tr("New feats"), &choice.newFeats,
                                   ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2));
        ImGui::TextWrapped("%s", tr("Enter ability increases, not final scores. Two points can replace a feat at an "
                                    "improvement milestone."));
        if (campaign.ruleSet)
            for (const auto &name : campaign.ruleSet->getAttributes())
            {
                auto i = base.attributes.find(name);
                int before = i == base.attributes.end() ? 10 : i->second;
                int increase = choice.abilityIncreases.count(name) ? choice.abilityIncreases.at(name) : 0;
                if (FormUi::InputInt(tr(name.c_str()), &increase))
                    choice.abilityIncreases[name] = increase;
                ImGui::TextDisabled("%d -> %lld", before, static_cast<long long>(before) + increase);
            }
        FormUi::InputTextMultiline(tr("Override reason"), &choice.overrideReason,
                                   ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2));
        ImGui::TextWrapped("%s", tr("Document homebrew, epic-boon or exceptional progression here. Overrides remain "
                                    "visible in the review."));
        break;
    }
    case 2:
    {
        if (automatic)
            FormUi::Checkbox(tr("Calculate standard spell slots"), &choice.automaticSlots);
        else
        {
            choice.automaticSlots = false;
            choice.automaticResources = false;
            ImGui::TextWrapped("%s", tr("Custom rules use the manual limits below."));
        }
        if (!choice.automaticSlots)
        {
            choice.slotMaximums.resize(10, 0);
            for (int level = 1; level <= 9; ++level)
            {
                auto label = std::string(tr("Spell slots")) + " " + std::to_string(level);
                FormUi::InputInt(label.c_str(), &choice.slotMaximums[level]);
            }
        }
        ImGui::SeparatorText(tr("Spells"));
        FormUi::InputText(tr("Find a spell"), &_controller._levelUpSpellSearch);
        auto lowered = [](std::string s)
        {
            for (auto &c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        const auto query = lowered(_controller._levelUpSpellSearch);
        if (query.empty())
            ImGui::TextWrapped("%s",
                               tr("Search the library to add a spell. Existing spells remain unless removed below."));
        else
            for (const auto &spell : spells)
                if (lowered(spell.name).find(query) != std::string::npos)
                {
                    ImGui::PushID(spell.metadata.id.c_str());
                    bool known = std::any_of(base.spellcasting.spells.begin(), base.spellcasting.spells.end(),
                                             [&](const auto &e) { return e.spellId == spell.metadata.id; });
                    auto found = std::find_if(choice.spellsToAdd.begin(), choice.spellsToAdd.end(),
                                              [&](const auto &e) { return e.spellId == spell.metadata.id; });
                    bool selected = found != choice.spellsToAdd.end();
                    ImGui::BeginDisabled(known);
                    if (FormUi::Checkbox(spell.name.c_str(), &selected))
                    {
                        if (selected)
                        {
                            CharacterSpellEntryModel e;
                            e.spellId = spell.metadata.id;
                            e.source = choice.advancingClass;
                            choice.spellsToAdd.push_back(e);
                        }
                        else
                            choice.spellsToAdd.erase(found);
                    }
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("%s: %d", tr("Level"), spell.level);
                    ImGui::PopID();
                }
        for (auto &entry : choice.spellsToAdd)
        {
            auto found = std::find_if(spells.begin(), spells.end(),
                                      [&](const auto &s) { return s.metadata.id == entry.spellId; });
            ImGui::PushID(entry.spellId.c_str());
            auto label = std::string(tr("Prepared")) + ": " + (found == spells.end() ? entry.spellId : found->name);
            FormUi::Checkbox(label.c_str(), &entry.prepared);
            ImGui::PopID();
        }
        for (const auto &entry : base.spellcasting.spells)
        {
            auto found = std::find_if(spells.begin(), spells.end(),
                                      [&](const auto &s) { return s.metadata.id == entry.spellId; });
            auto label = std::string(tr("Remove")) + ": " + (found == spells.end() ? entry.spellId : found->name);
            auto removed = std::find(choice.spellsToRemove.begin(), choice.spellsToRemove.end(), entry.spellId);
            bool selected = removed != choice.spellsToRemove.end();
            ImGui::PushID(entry.spellId.c_str());
            if (FormUi::Checkbox(label.c_str(), &selected))
            {
                if (selected)
                    choice.spellsToRemove.push_back(entry.spellId);
                else
                    choice.spellsToRemove.erase(removed);
            }
            ImGui::PopID();
        }
        ImGui::SeparatorText(tr("Resources"));
        if (automatic)
            FormUi::Checkbox(tr("Calculate standard resource limits"), &choice.automaticResources);
        ImGui::TextWrapped("%s", tr("Focus Points, Sorcery Points and Pact Magic have calculated limits. Add or "
                                    "override other resources below."));
        for (const auto &existing : base.resourceTrackers)
        {
            ImGui::PushID(existing.metadata.id.c_str());
            if (TaskUi::button((std::string(tr("Edit")) + ": " + existing.name).c_str()))
                if (std::none_of(choice.resourceChanges.begin(), choice.resourceChanges.end(),
                                 [&](const auto &r) { return r.metadata.id == existing.metadata.id; }))
                    choice.resourceChanges.push_back(existing);
            ImGui::PopID();
        }
        if (TaskUi::button(tr("Add resource")))
            choice.resourceChanges.emplace_back();
        int remove = -1;
        for (int i = 0; i < static_cast<int>(choice.resourceChanges.size()); ++i)
        {
            auto &item = choice.resourceChanges[i];
            ImGui::PushID(item.metadata.id.c_str());
            FormUi::InputText(tr("Resource name"), &item.name);
            FormUi::InputInt(tr("Maximum uses"), &item.maximum);
            FormUi::InputText(tr("Recharge rule"), &item.rechargeRule);
            if (TaskUi::button(tr("Discard resource change")))
                remove = i;
            ImGui::PopID();
        }
        if (remove >= 0)
            choice.resourceChanges.erase(choice.resourceChanges.begin() + remove);
        break;
    }
    case 3:
    {
        result = campaign.ruleSet
                     ? CharacterAdvancement::preview(base, *campaign.ruleSet, _controller.advancementRequest(), &spells)
                     : CharacterAdvancement::Preview{};
        if (!result.valid)
            ImGui::TextWrapped("%s", tr(result.error.c_str()));
        else
        {
            for (const auto &change : result.changes)
            {
                ImGui::SeparatorText(tr(change.label.c_str()));
                auto before = change.before, after = change.after;
                if (change.label == "Classes") {
                    const auto describe = [&](const CharacterModel &character) {
                        std::string text;
                        for (const auto &[name, level] : CharacterAdvancement::classLevels(character)) {
                            if (!text.empty()) text += " / ";
                            text += std::string(tr(name.c_str())) + " " + std::to_string(level);
                        }
                        return text;
                    };
                    before = describe(base); after = describe(result.proposed);
                }
                ImGui::TextWrapped("%s -> %s", before.empty() ? "—" : before.c_str(), after.empty() ? "—" : after.c_str());
                ImGui::TextDisabled("%s", tr(change.calculated ? "Calculated" : "Chosen or manual"));
            }
            if (result.proposed.spellcasting.enabled && campaign.ruleSet)
            {
                for (const auto &entry : result.proposed.spellcasting.spells)
                {
                    const auto cast =
                        CharacterAdvancement::spellcastingForEntry(result.proposed, *campaign.ruleSet, entry);
                    auto found = std::find_if(spells.begin(), spells.end(),
                                              [&](const auto &s) { return s.metadata.id == entry.spellId; });
                    if (found != spells.end())
                        ImGui::TextWrapped("%s: %s; %s", found->name.c_str(), tr(cast.ability.c_str()),
                            SpellcastingValueUi::spellValueText(campaign, result.proposed.attributes, cast, result.proposed.level, *found).c_str());
                }
            }
        }
        break;
    }
    }
    ImGui::EndChild();
    if (!_controller._errorMessage.empty())
        ImGui::TextWrapped("%s", _controller._errorMessage.c_str());
    if (_controller._levelUpStep == 3)
    {
        const auto key = CharacterAdvancement::reviewKey(_controller.advancementRequest());
        bool reviewed = choice.reviewToken == key;
        ImGui::BeginDisabled(!result.valid);
        if (FormUi::Checkbox(tr("I reviewed these changes and feature prerequisites"), &reviewed))
            choice.reviewToken = reviewed ? key : "";
        ImGui::EndDisabled();
    }
    if (_controller._levelUpStep > 0)
    {
        if (TaskUi::button(tr("Back")))
            --_controller._levelUpStep;
        TaskUi::sameLineForButton(tr("Next"));
    }
    if (_controller._levelUpStep < 3)
    {
        if (TaskUi::button(tr("Next")))
        {
            ++_controller._levelUpStep;
            _controller._errorMessage.clear();
        }
    }
    else
    {
        ImGui::BeginDisabled(!result.valid ||
                             choice.reviewToken != CharacterAdvancement::reviewKey(_controller.advancementRequest()));
        if (TaskUi::button(tr("Apply Level Up")))
        {
            _controller.applyLevelUpToSelectedCharacter();
            if (_controller._errorMessage.empty())
                ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
    }
    TaskUi::sameLineForButton(tr("Cancel"));
    if (TaskUi::button(tr("Cancel")))
    {
        _controller._levelUpPending = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
