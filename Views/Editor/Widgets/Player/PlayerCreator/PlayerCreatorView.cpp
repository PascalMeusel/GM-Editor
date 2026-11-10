#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "PlayerCreatorView.hpp"
#include "Controllers/Editor/Widgets/PlayerCreator/PlayerCreatorController.hpp"
#include "Models/Rulesets/IRuleSet.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui.h"
#include "Views/Editor/Widgets/UiPortrait.hpp"
#include <algorithm>
#include "Models/Character/CharacterDraftGenerator.hpp"
#include "Models/Character/CharacterHitPointEstimate.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"
#include <random>

namespace
{
void fieldLabel(const char *label)
{
    ImGui::TextUnformatted(localizedUiLabel(label).c_str());
    ImGui::SetNextItemWidth(-1.0f);
}

void renderCombo(const char *label, const std::vector<std::string> &items, std::string &current)
{
    ImGui::PushID(label);
    fieldLabel(label);
    if (FormUi::BeginCombo("##Choice", localizedUiLabel(current.empty() ? "<Unset>" : current.c_str()).c_str()))
    {
        for (const auto &item : items)
        {
            const bool selected = current == item;
            if (DraftEditSignal::record(ImGui::Selectable(localizedUiLabel(item.c_str()).c_str(), selected))) current = item;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();
}

void drawPlayerSpellSlotRows(CharacterSpellcastingModel &spellcasting)
{
    normalizeSpellcasting(spellcasting);
    if (ImGui::BeginTable("PlayerCreatorSpellSlots", 3, ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Level").c_str(), ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Max").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Used").c_str());
        ImGui::TableHeadersRow();
        for (int level = 1; level <= 9; ++level)
        {
            ImGui::PushID(level);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", level);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt("##Max", &spellcasting.slotMaximums[static_cast<std::size_t>(level)]);
            spellcasting.slotMaximums[static_cast<std::size_t>(level)] = std::max(0, spellcasting.slotMaximums[static_cast<std::size_t>(level)]);
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt("##Used", &spellcasting.slotUsed[static_cast<std::size_t>(level)]);
            spellcasting.slotUsed[static_cast<std::size_t>(level)] = std::clamp(spellcasting.slotUsed[static_cast<std::size_t>(level)], 0, spellcasting.slotMaximums[static_cast<std::size_t>(level)]);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
}

PlayerCreatorView::PlayerCreatorView(PlayerCreatorController &controller)
    : DockableWidget("Player Creator", 0, ImVec2(560.0f, 650.0f)), _controller(controller)
{
}

void PlayerCreatorView::draw()
{
    DraftEditSignal::Scope draftEdits([this] { _controller.markDraftEdited(); });

    if (!_controller._currentCampaign.ruleSet)
    {
        ImGui::TextWrapped("%s", localizedUiLabel("No active ruleset is available for this campaign.").c_str());
        return;
    }
    CharacterModel &model = _controller.getModel();
    const float unit = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const float width = std::min(ImGui::GetContentRegionAvail().x, 760.0f * unit);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f));
    ImGui::BeginChild("PlayerDraft", ImVec2(width, 0));
    _controller._creationStep = std::clamp(_controller._creationStep, 0, 2);
    const char *steps[] = {"Identity", "Abilities/HP", "Review"};
    if (ImGui::BeginTable("PlayerSteps", 3, ImGuiTableFlags_SizingStretchSame))
    {
        for (int step = 0; step < 3; ++step)
        {
            ImGui::TableNextColumn();
            if (drawUiButton(steps[step], _controller._creationStep == step ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(-1, 0)))
                _controller._creationStep = step;
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (!_controller._saveError.empty()) ImGui::TextWrapped("%s", _controller._saveError.c_str());
    // Keep this row present before and after the first edit. Inserting a new
    // discard button above a focused field used to make the entire form jump.
    {
        const std::string discardTitle = std::string(uiTextForLocale(
            "Discard player draft", "Spielfigurenentwurf verwerfen")) + "###DiscardPlayerDraft";
        ImGui::BeginDisabled(!_controller.hasUnsavedDraft());
        if (drawUiButton(uiTextForLocale("Discard draft", "Entwurf verwerfen"), UiButtonRole::Secondary))
            ImGui::OpenPopup(discardTitle.c_str());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", uiTextForLocale("Not added to the party yet", "Noch nicht zur Gruppe hinzugefügt"));
        if (beginBoundedConfirmationModal(discardTitle.c_str()))
        {
            ImGui::TextWrapped("%s", uiTextForLocale(
                "Discard this unfinished player? The draft has not been added to the campaign.",
                "Diese unfertige Spielfigur verwerfen? Der Entwurf wurde der Kampagne noch nicht hinzugefügt."));
            if (drawUiButton(uiTextForLocale("Discard draft", "Entwurf verwerfen"), UiButtonRole::Destructive))
            {
                _controller.discardDraft();
                _controller.openPlayerOverview();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (drawUiButton("Cancel", UiButtonRole::Secondary))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::Spacing();
    }
    bool create = false;
    if (_lastStep != _controller._creationStep) ImGui::SetNextWindowScroll(ImVec2(0, 0));
    _lastStep = _controller._creationStep;
    ImGui::BeginChild("PlayerDraftFields", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.4f));
    if (_controller._creationStep == 0)
    {
        fieldLabel("Name");
        FormUi::InputText("##Name", model.nameBuffer, sizeof(model.nameBuffer));
        const auto previousRace = model.selectedRace;
        renderCombo("Species", _controller._currentCampaign.ruleSet->getRaces(), model.selectedRace);
        if(model.selectedRace != previousRace)
        {
            auto recipe = gm::portraits::read(model.metadata);
            recipe.species = gm::portraits::speciesForRace(model.selectedRace);
            gm::portraits::write(model.metadata, recipe);
        }
        renderCombo("Class", _controller._currentCampaign.ruleSet->getClasses(), model.selectedClass);
        const std::string suggestionTitle = std::string(uiTextForLocale(
            "Suggest player identity", "Identität vorschlagen")) + "###SuggestPlayerIdentity";
        if (drawUiButton(uiTextForLocale("Suggest identity...", "Identität vorschlagen..."), UiButtonRole::Secondary))
            ImGui::OpenPopup(suggestionTitle.c_str());
        if (beginBoundedConfirmationModal(suggestionTitle.c_str()))
        {
            ImGui::TextWrapped("%s", uiTextForLocale("Replace name, background, backstory and portrait with a new suggestion?", "Name, Hintergrund, Vorgeschichte und Porträt durch einen neuen Vorschlag ersetzen?"));
            const bool generate = drawUiButton(
                uiTextForLocale("Generate identity", "Identität erzeugen"), UiButtonRole::Primary);
            ImGui::SameLine();
            const bool cancel = drawUiButton("Cancel", UiButtonRole::Secondary);
            if (generate)
            {
                gm::creation::Generator generator(std::random_device{}());
                generator.playerIdentity(model);
                std::vector<std::string> existingNames;
                for(const auto &player : _controller._currentCampaign.characters) existingNames.push_back(player.nameBuffer);
                for(const auto &npc : _controller._currentCampaign.npcs) existingNames.push_back(npc.name);
                const auto name = generator.uniqueName(model.nameBuffer, existingNames);
                std::snprintf(model.nameBuffer, sizeof(model.nameBuffer), "%s", name.c_str());
                _controller.markDraftEdited();
                ImGui::CloseCurrentPopup();
            }
            else if (cancel)
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (drawUiButton(uiTextForLocale("Match portrait to species", "Porträt an Spezies anpassen"), UiButtonRole::Secondary))
        {
            auto recipe = gm::portraits::read(model.metadata);
            recipe.species = gm::portraits::speciesForRace(model.selectedRace);
            gm::portraits::write(model.metadata, recipe);
            _controller.markDraftEdited();
        }
        if(ImGui::TreeNode(uiTextForLocale("Choose portrait","Porträt wählen"))) {PortraitUi::editor(model.metadata);ImGui::TreePop();}
        else PortraitUi::preview(model.metadata,120.f*unit);
        if (_controller._currentCampaign.ruleSet->levelsExist())
        {
            fieldLabel("Level");
            ImGui::SetNextItemWidth(160.0f * unit);
            FormUi::InputInt("##Level", &model.level);
            model.level = std::max(1, model.level);
        }
        if (ImGui::TreeNode(localizedUiLabel("Background").c_str()))
        {
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputTextWithHint("##Background", localizedUiText("Example: Folk Hero, Acolyte, Smuggler"), &model.background);
            ImGui::TreePop();
        }
    }
    else if (_controller._creationStep == 1)
    {
        if (ImGui::BeginTable("PlayerAbilities", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
            for (auto &attribute : model.attributes)
            {
                ImGui::PushID(attribute.first.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextWrapped("%s", localizedUiLabel(attribute.first.c_str()).c_str());
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                FormUi::InputInt("##Score", &attribute.second);
                if (ImGui::IsItemDeactivatedAfterEdit()) attribute.second = std::clamp(attribute.second, 1, 30);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        const int suggestedHp = estimateNewCharacterMaxHitPoints(model, _controller._currentCampaign.ruleSet.get());
        fieldLabel("Max HP");
        ImGui::SetNextItemWidth(180.0f * unit);
        FormUi::InputInt("##MaxHP", &model.maxHitPoints);
        model.maxHitPoints = std::max(0, model.maxHitPoints);
        fieldLabel("Current HP");
        ImGui::SetNextItemWidth(180.0f * unit);
        FormUi::InputInt("##CurrentHP", &model.currentHitPoints);
        model.currentHitPoints = std::max(0, model.currentHitPoints);
        if (model.maxHitPoints > 0) model.currentHitPoints = std::clamp(model.currentHitPoints, 0, model.maxHitPoints);
        if (drawUiButton(uiTextForLocale("Use HP estimate", "TP-Schätzung übernehmen"), UiButtonRole::Secondary))
            model.maxHitPoints = model.currentHitPoints = suggestedHp;
        ImGui::SameLine();
        ImGui::TextDisabled("%d", suggestedHp);
        showHelpTooltip(uiTextForLocale("Uses the active ruleset's class progression when it explicitly provides starting HP; otherwise this is a generic level and Constitution estimate. You can always enter a manual value.", "Verwendet die Klassenprogression des aktiven Regelwerks, wenn sie Start-TP ausdrücklich angibt; andernfalls ist dies eine allgemeine Schätzung aus Stufe und Konstitution. Ein manueller Wert ist jederzeit möglich."));
        if (ImGui::TreeNode(localizedUiLabel("Spellcasting").c_str()))
        {
            FormUi::Checkbox(localizedUiLabel("Uses Magic").c_str(), &model.spellcasting.enabled);
            if (model.spellcasting.enabled)
            {
                fieldLabel("Spellcasting Ability");
                FormUi::InputText("##SpellAbility", &model.spellcasting.ability);
                SpellcastingValueUi::drawDraftFields(_controller._currentCampaign, model.attributes, model.spellcasting, model.level);
                drawPlayerSpellSlotRows(model.spellcasting);
            }
            ImGui::TreePop();
        }
    }
    else
    {
        const auto summary = [&]()
        {
            ImGui::TextWrapped("%s", model.nameBuffer[0] ? model.nameBuffer : localizedUiLabel("Unnamed player draft").c_str());
            drawInlineLabelValue("Species", localizedUiLabel(model.selectedRace.empty() ? "<Unset>" : model.selectedRace.c_str()).c_str());
            drawInlineLabelValue("Class", localizedUiLabel(model.selectedClass.empty() ? "<Unset>" : model.selectedClass.c_str()).c_str());
            drawInlineLabelValue("Level", std::to_string(model.level).c_str());
            drawInlineLabelValue(model.maxHitPoints > 0 ? localizedUiText("Max HP") : uiTextForLocale("Estimated max HP", "Geschätzte maximale TP"),
                std::to_string(model.maxHitPoints > 0 ? model.maxHitPoints : estimateNewCharacterMaxHitPoints(model, _controller._currentCampaign.ruleSet.get())).c_str());
            if (!model.background.empty())
                drawInlineLabelValue("Background", localizedUiLabel(model.background.c_str()).c_str());
            if (model.spellcasting.enabled) drawInlineLabelValue("Spellcasting", localizedUiLabel("Enabled").c_str());
        };
        if (ImGui::GetContentRegionAvail().x >= 430.0f * unit &&
            ImGui::BeginTable("PlayerReviewSummary", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("portrait", ImGuiTableColumnFlags_WidthFixed, 125.0f * unit);
            ImGui::TableSetupColumn("summary", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn(); PortraitUi::preview(model.metadata, 112.0f * unit);
            ImGui::TableNextColumn(); summary();
            ImGui::EndTable();
        }
        else { summary(); PortraitUi::preview(model.metadata, 88.0f * unit); }
        if (ImGui::TreeNode(localizedUiLabel("Portrait / Story").c_str()))
        {
            fieldLabel("Portrait Asset");
            FormUi::InputTextWithHint("##Portrait", localizedUiText("Project-relative portrait path or asset id"), &model.portraitAssetId);
            showHelpTooltip("Full portrait file import is available from Character Sheets after the character is created.");
            fieldLabel("Backstory");
            FormUi::InputTextMultiline("##Backstory", &model.backstory, ImVec2(-1, 160 * unit), ImGuiInputTextFlags_WordWrap);
            ImGui::TreePop();
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (_controller._creationStep > 0)
    {
        if (drawUiButton("Back", UiButtonRole::Secondary)) --_controller._creationStep;
        ImGui::SameLine();
    }
    if (_controller._creationStep == 2)
        create = drawUiButton("Create Player", UiButtonRole::Primary);
    else if (drawUiButton(_controller._creationStep == 0 ? "Next: Abilities/HP" : "Next: Review", UiButtonRole::Primary))
        ++_controller._creationStep;
    ImGui::EndChild();
    if (create && _controller.savePlayer()) _controller.hide();
}
