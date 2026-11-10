#include "SpellLibraryViewSupport.hpp"

void SpellLibraryView::drawSpellEditor()
{
    DraftEditSignal::Scope draftEdits([this] { _controller.markDraftEdited(); });
    SpellModel &spell = _controller._draftSpell;
    drawHelpText("Spell Definition", "This is rules metadata plus table-facing notes. It supports D&D-style cantrips, leveled spells, attacks, saves, rituals, concentration, and scaling.");
    const std::string editorMode = gm::localization::trUi(_controller._selectedSpellIndex >= 0 ? "Edit Campaign Spell" : "Create Campaign Spell");
    const std::string editorSelection = spell.name.empty() ? gm::localization::trUi("Unnamed spell draft") : spell.name;
    const bool blockingValidation = _controller.draftHasBlockingValidationErrors();
    UiStatusActionStrip editorStrip;
    editorStrip.mode = editorMode.c_str();
    editorStrip.selection = editorSelection.c_str();
    editorStrip.nextAction = gm::localization::trUi(blockingValidation ? "Fix required fields before saving." : "Review warnings, then save the spell.");
    editorStrip.help = "The editor changes which resolution fields are shown based on Use Type, so only fields relevant to table use stay visible.";
    if (blockingValidation) ImGui::TextWrapped("%s", editorStrip.nextAction);

    if (!_controller._statusMessage.empty())
        ImGui::TextColored(ImVec4(0.48f, 0.78f, 0.54f, 1.0f), localizedUiLabel("%s").c_str(), _controller._statusMessage.c_str());
    TaskUi::fieldLabel(localizedUiText("Name"));
    FormUi::InputText("##Name", &spell.name);
    TaskUi::fieldLabel(localizedUiText("Spell Level"));
    FormUi::InputInt("##Spell Level", &spell.level);
    showHelpTooltip("Use 0 for a cantrip, 1-9 for leveled spells.");
    drawSchoolCombo("School", spell.school);
    drawResolutionCombo("Use Type", spell.resolution);
    const std::string guidance = _controller.resolutionGuidance(spell.resolution);
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), guidance.c_str());
    showHelpTooltip(guidance.c_str());

    TaskUi::fieldLabel(localizedUiText("Casting Time"));

    FormUi::InputText("##Casting Time", &spell.castingTime);
    TaskUi::fieldLabel(localizedUiText("Range"));
    FormUi::InputText("##Range", &spell.range);
    TaskUi::fieldLabel(localizedUiText("Duration"));
    FormUi::InputText("##Duration", &spell.duration);
    TaskUi::fieldLabel(localizedUiText("Area or Target"));
    FormUi::InputText("##Area or Target", &spell.area);
    FormUi::Checkbox(gm::localization::trUi("Concentration"), &spell.concentration);
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(gm::localization::trUi("Ritual"), &spell.ritual);

    if (ImGui::TreeNode(gm::localization::trUi("Components")))
    {
    FormUi::Checkbox(localizedUiLabel("Verbal").c_str(), &spell.verbal);
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(localizedUiLabel("Somatic").c_str(), &spell.somatic);
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(localizedUiLabel("Material").c_str(), &spell.material);
    if (spell.material)
    {
        TaskUi::fieldLabel(localizedUiText("Material Details"));
        FormUi::InputText("##Material Details", &spell.materialDescription);
    }

    ImGui::TreePop();
    }
    ImGui::SeparatorText(gm::localization::trUi("Rules"));
    TaskUi::fieldLabel(localizedUiText("Classes"));
    FormUi::InputText("##Classes", &spell.classes);
    showHelpTooltip("Comma-separated class/source list, such as Wizard, Cleric, Warlock, Domain, Patron, or custom tradition.");
    if (spell.resolution == SpellResolutionType::SpellAttack)
    {
        TaskUi::fieldLabel(localizedUiText("Attack Bonus Override"));
        FormUi::InputText("##Attack Bonus Override", &spell.attackBonus);
        showHelpTooltip("Optional. Leave empty to use the caster's spell attack bonus.");
        TaskUi::fieldLabel(localizedUiText("Damage"));
        FormUi::InputText("##Damage", &spell.damage);
        showHelpTooltip("Roll text such as 2d10 force. Put non-damage riders in Effect Summary.");
    }
    else if (spell.resolution == SpellResolutionType::SavingThrow)
    {
        TaskUi::fieldLabel(localizedUiText("Saving Throw"));
        FormUi::InputText("##Saving Throw", &spell.savingThrowAbility);
        showHelpTooltip("Ability name such as Dexterity, Constitution, Wisdom, or a homebrew defense.");
        TaskUi::fieldLabel(localizedUiText("Damage"));
        FormUi::InputText("##Damage", &spell.damage);
        showHelpTooltip("Failed-save damage such as 3d6 fire. Put half-damage or rider notes in Effect Summary.");
    }
    else if (spell.resolution == SpellResolutionType::Healing)
    {
        TaskUi::fieldLabel(localizedUiText("Healing"));
        FormUi::InputText("##Healing", &spell.healing);
        showHelpTooltip("Healing roll or fixed amount, such as 1d8 + spellcasting modifier.");
    }
    else if (spell.resolution == SpellResolutionType::SummonOrZone)
    {
        TaskUi::fieldLabel(localizedUiText("Damage or Zone Roll"));
        FormUi::InputText("##Damage or Zone Roll", &spell.damage);
        showHelpTooltip("Optional ongoing damage or zone roll. Use Effect Summary for placement, duration, and trigger notes.");
    }
    const std::vector<std::string> hiddenFields = hiddenResolutionFields(spell);
    if (!hiddenFields.empty())
    {
        const std::string hiddenText = "Hidden for " + std::string(spellResolutionLabel(spell.resolution)) + ": " + joinFieldNames(hiddenFields) + ". Switch Use Type to edit or clear them.";
        ImGui::TextColored(ImVec4(0.94f, 0.71f, 0.36f, 1.0f), localizedUiLabel("%s").c_str(), hiddenText.c_str());
    }
    if(ImGui::TreeNode(localizedUiText("Scaling / Higher Level"))) {
        FormUi::InputText("##Scaling / Higher Level", &spell.scaling);
        ImGui::TreePop();
    }
    TaskUi::fieldLabel(localizedUiText("Effect Summary"));
    FormUi::InputTextMultiline("##Effect Summary", &spell.effectSummary, ImVec2(-1.0f, 82.0f));
    const std::string resolutionPreview = _controller.resolutionSummary(spell);
    ImGui::TextDisabled(localizedUiLabel("%s").c_str(), resolutionPreview.c_str());
    if(ImGui::TreeNode(localizedUiText("Full Notes"))) {
        FormUi::InputTextMultiline("##Full Notes", &spell.description, ImVec2(-1.0f, 110.0f));
        ImGui::TreePop();
    }

    const std::vector<std::string> warnings = _controller.validateDraftSpell();
    drawWarningList(warnings);

    const char *saveLabel = _controller._selectedSpellIndex >= 0 ? "Save Spell" : "Create Spell";
    if (drawUiButton(saveLabel,
                     !blockingValidation,
                     "Name and level 0-9 are required before saving.",
                     UiButtonRole::Primary,
                     ImVec2(140.0f, 0.0f),
                     "Save this spell to the campaign library."))
        _controller.saveDraft();
    TaskUi::sameLineIfFits();
    if (_controller._selectedSpellIndex >= 0 && drawUiButton("Delete Spell", UiButtonRole::Destructive))
        ImGui::OpenPopup(localizedUiLabel("Delete Spell").c_str());

    if (beginBoundedConfirmationModal(localizedUiLabel("Delete Spell").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Delete this spell and remove it from every character spellbook?").c_str());
        if (drawModalActionButtons("Delete", UiButtonRole::Destructive, "Cancel", 110.0f))
            _controller.deleteSelectedSpell();
        ImGui::EndPopup();
    }
}

void SpellLibraryView::drawSchoolCombo(const char *label, SpellSchool &school)
{
    if (FormUi::BeginCombo(gm::localization::trUi(label), spellSchoolLabel(school)))
    {
        for (int i = 0; i < kSchoolCount; ++i)
        {
            const SpellSchool candidate = static_cast<SpellSchool>(i);
            const bool selected = school == candidate;
            if (DraftEditSignal::record(ImGui::Selectable(spellSchoolLabel(candidate), selected)))
                school = candidate;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void SpellLibraryView::drawResolutionCombo(const char *label, SpellResolutionType &resolution)
{
    if (FormUi::BeginCombo(gm::localization::trUi(label), spellResolutionLabel(resolution)))
    {
        for (int i = 0; i < kResolutionCount; ++i)
        {
            const SpellResolutionType candidate = static_cast<SpellResolutionType>(i);
            const bool selected = resolution == candidate;
            if (DraftEditSignal::record(ImGui::Selectable(spellResolutionLabel(candidate), selected)))
                resolution = candidate;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}
