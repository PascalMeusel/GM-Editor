#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "NPCCreatorView.hpp"
#include "Controllers/Editor/Widgets/NPCCreator/NPCCreatorController.hpp"
#include "misc/cpp/imgui_stdlib.h"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "imgui.h"
#include "Views/Editor/Widgets/UiPortrait.hpp"
#include "Views/Editor/Widgets/Magic/SpellcastingValueUi.hpp"
#include <algorithm>
#include <string>

namespace
{
const char *npcUi(const char *en, const char *de)
{
    return gm::localization::LocalizationService::instance().locale() ==
        gm::localization::Locale::German ? de : en;
}

void textField(const char *label, std::string &value, float lines = 0)
{
    ImGui::PushID(label);
    ImGui::TextUnformatted(localizedUiLabel(label).c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if (lines > 0)
        FormUi::InputTextMultiline("##Value", &value,
            ImVec2(-1.0f, ImGui::GetTextLineHeightWithSpacing() * lines), ImGuiInputTextFlags_WordWrap);
    else
        FormUi::InputText("##Value", &value);
    ImGui::PopID();
}
}

NPCCreatorView::NPCCreatorView(NPCCreatorController &controller)
    : DockableWidget("NPC Creator", 0, ImVec2(640.0f, 760.0f)), _controller(controller)
{
}

void NPCCreatorView::draw()
{
    DraftEditSignal::Scope draftEdits([this] { _controller.markDraftEdited(); });

    const float unit = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const float width = std::min(ImGui::GetContentRegionAvail().x, 760.0f * unit);
    const float formX=ImGui::GetCursorPosX() + std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f);
    ImGui::SetCursorPosX(formX);
    ImGui::BeginChild("ActorDraft", ImVec2(width, -ImGui::GetFrameHeightWithSpacing() - 8.0f));
    const char *templates[] = {"Person", "Creature", "Faction Entity", "Deity/Patron", "Monster Stat Block"};
    int selectedTemplate = _controller._selectedEntryType == 1 ? 4 : std::clamp(_controller._selectedNpcCategory, 0, 3);
    ImGui::SetNextItemWidth(-1.0f);
    if (FormUi::BeginCombo("##ActorTemplate", localizedUiLabel(templates[selectedTemplate]).c_str()))
    {
        for (int index = 0; index < 5; ++index)
            if (ImGui::Selectable(localizedUiLabel(templates[index]).c_str(), selectedTemplate == index))
            {
                _controller._selectedEntryType = index == 4 ? 1 : 0;
                if (index != 4 && _controller._selectedNpcCategory != index)
                {
                    _controller._selectedNpcCategory = index;
                    _controller.markDraftEdited();
                }
            }
        ImGui::EndCombo();
    }
    const bool monsterDraft = _controller._selectedEntryType == 1;
    const char *createLabel=monsterDraft?npcUi("Create monster","Monster erstellen"):
        selectedTemplate==2?npcUi("Create faction","Fraktion erstellen"):
        selectedTemplate==3?npcUi("Create deity or patron","Gottheit oder Patron erstellen"):
        npcUi("Create person","Person erstellen");
    ImGui::SeparatorText(createLabel);
    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    bool createClicked = false;
    const char *ideasLabel = npcUi("Ideas…", "Ideen…");
    const float ideasWidth = ImGui::CalcTextSize(ideasLabel).x + ImGui::GetStyle().FramePadding.x * 2;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + ideasWidth <= rowRight)
        ImGui::SameLine();
    if (drawUiButton(ideasLabel, UiButtonRole::Secondary)) ImGui::OpenPopup("ActorDraftIdeas");
    bool generatedDetails = false;
    if (ImGui::BeginPopup("ActorDraftIdeas"))
    {
        if (!monsterDraft)
        {
            ImGui::TextUnformatted(npcUi("Species for new draft", "Spezies für neuen Entwurf"));
            if (ImGui::BeginCombo("##GenerationSpecies", _controller._generationSpecies.empty() ? npcUi("Random from campaign", "Zufällig aus Kampagne") : _controller._generationSpecies.c_str()))
            {
                if (ImGui::Selectable(npcUi("Random from campaign", "Zufällig aus Kampagne"), _controller._generationSpecies.empty())) _controller._generationSpecies.clear();
                for(const auto *species : gm::portraits::speciesNames)
                    if(ImGui::Selectable(species, _controller._generationSpecies == species)) _controller._generationSpecies = species;
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("%s", npcUi("Generate replaces the current draft.", "Erzeugen ersetzt den aktuellen Entwurf."));
        }
        if (ImGui::MenuItem(localizedUiLabel(monsterDraft ? "Generate Monster Draft" : "Generate NPC Draft").c_str()))
        {
            if (monsterDraft) _controller.generateMonsterDraft();
            else _controller.generateNpcDraft();
            generatedDetails = true;
        }
        ImGui::Separator();
        if (!monsterDraft)
        {
            if (ImGui::MenuItem(localizedUiLabel("Reroll Name").c_str())) _controller.rerollNpcName();
            if (ImGui::MenuItem(npcUi("Reroll scenario (role, goal, resources, location, secret)", "Szenario neu würfeln (Rolle, Ziel, Mittel, Ort, Geheimnis)"))) { _controller.rerollNpcRole(); generatedDetails = true; }
            if (ImGui::MenuItem(localizedUiLabel("Reroll Secret").c_str())) { _controller.rerollNpcSecret(); generatedDetails = true; }
        }
        else
        {
            if (ImGui::MenuItem(localizedUiLabel("Reroll Role").c_str())) _controller.rerollMonsterRoleTags();
            if (ImGui::MenuItem(localizedUiLabel("Reroll Hooks").c_str())) _controller.rerollMonsterHooks();
            if (ImGui::MenuItem(localizedUiLabel("Reroll Numbers").c_str())) _controller.rerollMonsterNumbers();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(localizedUiLabel("Open NPC & Monster Overview").c_str())) _controller.openNPCOverview();
        ImGui::EndPopup();
    }
    if (!_controller._saveError.empty())
        ImGui::TextWrapped("%s", _controller._saveError.c_str());

    if (monsterDraft)
    {
        MonsterDefinitionModel &monster = _controller.getMonsterModel();
        MonsterActionModel &action = _controller.getMonsterAction();
        const bool monsterColumns=width>=650.f*unit;
        if(ImGui::BeginTable("MonsterIdentity",monsterColumns?2:1,ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Details",ImGuiTableColumnFlags_WidthStretch,2.f);
            if(monsterColumns)ImGui::TableSetupColumn("Portrait",ImGuiTableColumnFlags_WidthStretch,1.f);
            ImGui::TableNextColumn();
            textField("Name", monster.name);
            textField("Type", monster.type);
            FormUi::InputInt(localizedUiLabel("Armor Class").c_str(), &monster.armorClass);
            FormUi::InputInt(localizedUiLabel("Hit Points").c_str(), &monster.hitPoints);
            if(monsterColumns)ImGui::TableNextColumn();
            if(ImGui::TreeNode(npcUi("Choose portrait","Porträt wählen"))) {PortraitUi::editor(monster.metadata);ImGui::TreePop();}
            else PortraitUi::preview(monster.metadata,std::min(120.f*unit,ImGui::GetContentRegionAvail().x));
            ImGui::EndTable();
        }
        monster.armorClass = std::max(1, monster.armorClass);
        monster.hitPoints = std::max(1, monster.hitPoints);
        if (ImGui::TreeNode(npcUi("More combat details", "Weitere Kampfwerte")))
        {
            textField("Size", monster.size);
            textField("Challenge", monster.challengeRating);
            FormUi::InputInt(localizedUiLabel("Speed ft").c_str(), &monster.speedFeet);
            FormUi::InputInt(localizedUiLabel("Armor Damage Reduction").c_str(), &monster.armorDamageReduction);
            monster.speedFeet = std::max(0, monster.speedFeet);
            monster.armorDamageReduction = std::max(0, monster.armorDamageReduction);
            for (auto &attribute : monster.attributes)
                FormUi::InputInt(attribute.first.c_str(), &attribute.second);
            ImGui::TreePop();
        }
        if (generatedDetails) ImGui::SetNextItemOpen(true);
        if (ImGui::TreeNode(localizedUiLabel("Traits").c_str()))
        {
            textField("Traits", monster.traits, 4);
            textField("Recharge Actions", monster.rechargeActions, 3);
            ImGui::TreePop();
        }
        ImGui::SeparatorText(localizedUiLabel("Actions").c_str());
        for (const auto &existingAction : monster.actions)
            ImGui::BulletText("%s  %s  %s", existingAction.name.c_str(), existingAction.attackBonus.c_str(), existingAction.damage.c_str());
        if (ImGui::TreeNodeEx(localizedUiLabel("Add Action").c_str(), monster.actions.empty() ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            DraftEditSignal::Scope actionEdits([this] { _controller._monsterActionEdited = true; _controller.markDraftEdited(); });
            textField("Action Name", action.name);
            textField("Attack Bonus", action.attackBonus);
            textField("Damage", action.damage);
            FormUi::InputInt(localizedUiLabel("Reach ft").c_str(), &action.reachFeet);
            FormUi::InputInt(localizedUiLabel("Range ft").c_str(), &action.rangeFeet);
            textField("Action Notes", action.notes, 3);
            action.reachFeet = std::max(0, action.reachFeet);
            action.rangeFeet = std::max(action.reachFeet, action.rangeFeet);
            if (drawUiButton("Add Action", UiButtonRole::Secondary)) _controller.addMonsterAction();
            ImGui::TreePop();
        }
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::SetCursorPosX(formX);
        createClicked=drawUiButton(createLabel,UiButtonRole::Primary);
        if (createClicked && _controller.saveMonster()) _controller.hide();
        return;
    }

    NPCModel &model = _controller.getModel();
    const bool identityColumns=selectedTemplate<2 && width>=650.f*unit;
    const bool identityTable=ImGui::BeginTable("ActorIdentity",identityColumns?2:1,ImGuiTableFlags_SizingStretchProp);
    if(identityTable) {
    ImGui::TableSetupColumn("Identity",ImGuiTableColumnFlags_WidthStretch,.68f);
    if(identityColumns)ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthStretch,.32f);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(localizedUiLabel("Name").c_str());
    const char *suggest=npcUi("Suggest name","Namen vorschlagen");
    const float suggestWidth=ImGui::CalcTextSize(suggest).x+ImGui::GetStyle().FramePadding.x*2;
    const bool besideName=ImGui::GetContentRegionAvail().x>suggestWidth+140.f*unit;
    ImGui::SetNextItemWidth(besideName?ImGui::GetContentRegionAvail().x-suggestWidth-ImGui::GetStyle().ItemSpacing.x:-1.f);
    FormUi::InputText("##ActorName",&model.name);
    if(besideName)ImGui::SameLine();
    if(ImGui::Button(suggest))_controller.rerollNpcName();
    textField(selectedTemplate==2?npcUi("Purpose","Zweck"):selectedTemplate==3?npcUi("Domain","Domäne"):"Role", model.role);
    textField("Description", model.description, 5);
    if(selectedTemplate<2) {
        if(identityColumns)ImGui::TableNextColumn();
        ImGui::SeparatorText(npcUi("Preview","Vorschau"));
        ImGui::TextWrapped("%s",model.name.c_str());
        ImGui::TextWrapped("%s",model.role.c_str());
        if(ImGui::TreeNode(npcUi("Choose portrait","Porträt wählen"))) {PortraitUi::editor(model.metadata);ImGui::TreePop();}
        else PortraitUi::preview(model.metadata,std::min(168.f*unit,ImGui::GetContentRegionAvail().x));
        ImGui::TextWrapped("%s",model.description.c_str());
    } else {
        textField(selectedTemplate==2?npcUi("Goals","Ziele"):npcUi("Tenets","Leitsätze"),model.goals,3);
        textField(selectedTemplate==2?npcUi("Members and relationships","Mitglieder und Beziehungen"):npcUi("Followers and relationships","Anhänger und Beziehungen"),model.relationships,3);
    }
    ImGui::EndTable();
    }
    if (generatedDetails) ImGui::SetNextItemOpen(true);
    if (ImGui::TreeNode(npcUi("Motives and secrets", "Motive und Geheimnisse")))
    {
        textField("Attitude", model.attitudeTowardParty);
        textField("Goals", model.goals, 3);
        textField("Secrets", model.secrets, 3);
        textField("Resources", model.resources, 2);
        textField("Relationships", model.relationships, 3);
        textField("Location", model.threatLocation);
        textField("Faction", model.faction);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode(localizedUiLabel("Dialogue Notes").c_str()))
    {
        textField("Dialogue Notes", model.dialogueNotes, 5);
        ImGui::TreePop();
    }
    if (!model.attributes.empty() && ImGui::TreeNode(localizedUiLabel("Ability Scores").c_str()))
    {
        for (auto &attribute : model.attributes)
            FormUi::InputInt(attribute.first.c_str(), &attribute.second);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode(localizedUiLabel("Magic").c_str()))
    {
    FormUi::Checkbox(localizedUiLabel("Uses Magic").c_str(), &model.spellcasting.enabled);
    if (model.spellcasting.enabled)
    {
        FormUi::InputText(localizedUiLabel("Spellcasting Ability").c_str(), &model.spellcasting.ability);
        SpellcastingValueUi::drawDraftFields(_controller._currentCampaign, model.attributes, model.spellcasting, 1);
        normalizeSpellcasting(model.spellcasting);
        if (ImGui::BeginTable("NpcCreatorSlots", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn(localizedUiLabel("Level").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Max").c_str());
            ImGui::TableSetupColumn(localizedUiLabel("Used").c_str());
            ImGui::TableHeadersRow();
            for (int level = 1; level <= 9; ++level)
            {
                ImGui::PushID(level);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(localizedUiLabel("%d").c_str(), level);
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-1.0f);
                FormUi::InputInt(localizedUiLabel("##Max").c_str(), &model.spellcasting.slotMaximums[static_cast<std::size_t>(level)]);
                model.spellcasting.slotMaximums[static_cast<std::size_t>(level)] = std::max(0, model.spellcasting.slotMaximums[static_cast<std::size_t>(level)]);
                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-1.0f);
                FormUi::InputInt(localizedUiLabel("##Used").c_str(), &model.spellcasting.slotUsed[static_cast<std::size_t>(level)]);
                model.spellcasting.slotUsed[static_cast<std::size_t>(level)] = std::clamp(model.spellcasting.slotUsed[static_cast<std::size_t>(level)], 0, model.spellcasting.slotMaximums[static_cast<std::size_t>(level)]);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if (!_controller._currentCampaign.customSpells.empty())
        {
            if (_controller._selectedNpcSpellIndex >= static_cast<int>(_controller._currentCampaign.customSpells.size()))
                _controller._selectedNpcSpellIndex = 0;
            const SpellModel &selectedSpell = _controller._currentCampaign.customSpells[static_cast<std::size_t>(_controller._selectedNpcSpellIndex)];
            if (FormUi::BeginCombo(localizedUiLabel("Prepared Spell").c_str(), selectedSpell.name.c_str()))
            {
                for (int i = 0; i < static_cast<int>(_controller._currentCampaign.customSpells.size()); ++i)
                {
                    const SpellModel &spell = _controller._currentCampaign.customSpells[static_cast<std::size_t>(i)];
                    const bool selected = _controller._selectedNpcSpellIndex == i;
                    if (DraftEditSignal::record(ImGui::Selectable(spell.name.c_str(), selected)))
                        _controller._selectedNpcSpellIndex = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button(localizedUiLabel("Add Prepared Spell").c_str()))
                _controller.addNpcSpell();
        }
        for (const CharacterSpellEntryModel &entry : model.spellcasting.spells)
        {
            const SpellModel *spell = findSpellById(_controller._currentCampaign.customSpells, entry.spellId);
            ImGui::BulletText("%s", spell ? spell->name.c_str() : localizedUiText("<Missing Spell>"));
        }
    }

        ImGui::TreePop();
    }
    ImGui::TextDisabled("%s",npcUi("GM secrets stay private","SL-Geheimnisse bleiben privat"));
    ImGui::EndChild();
    ImGui::SetCursorPosX(formX);
    createClicked=drawUiButton(createLabel,UiButtonRole::Primary);
    if (createClicked && _controller.saveNPC()) _controller.hide();
}
