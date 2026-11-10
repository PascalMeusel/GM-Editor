#include "SpellLibraryViewSupport.hpp"

void SpellLibraryView::drawCatalogBrowser()
{
    drawHelpText("Spell Overview", "Browse ruleset-provided spells and campaign custom spells together. Ruleset spells are read-only; copy one into the campaign when you want to customize it.");

    const std::vector<SpellLibraryController::CatalogEntry> entries = _controller.getCatalogEntries();
    const std::vector<std::string> levels = _controller.getCatalogLevels(entries);
    if (_controller._selectedCatalogLevelIndex >= static_cast<int>(levels.size()))
        _controller._selectedCatalogLevelIndex = 0;

    TaskUi::fieldLabel(gm::localization::trUi("Search Catalog"));
    FormUi::InputText("##SpellSearch", &_controller._catalogSearch);
    if (_controller._selectedCatalogSourceIndex != 0 || _controller._selectedCatalogLevelIndex != 0)
    {
        if (drawUiButton(uiTextForLocale("Clear active filters", "Aktive Filter zurücksetzen")))
        { _controller._selectedCatalogSourceIndex = 0; _controller._selectedCatalogLevelIndex = 0; }
    }
    if (ImGui::TreeNode(uiTextForLocale("Filters", "Filter")))
    {
    const char *sourceFilters[] = {gm::localization::trUi("All Sources"), gm::localization::trUi("Ruleset"), gm::localization::trUi("Campaign")};
    ImGui::SetNextItemWidth(180.0f);
    FormUi::Combo(gm::localization::trUi("Source"), &_controller._selectedCatalogSourceIndex, sourceFilters, 3);
    TaskUi::sameLineIfFits();
    ImGui::SetNextItemWidth(180.0f);
    if (FormUi::BeginCombo(gm::localization::trUi("Level"), levels[_controller._selectedCatalogLevelIndex].c_str()))
    {
        for (int i = 0; i < static_cast<int>(levels.size()); ++i)
        {
            const bool selected = _controller._selectedCatalogLevelIndex == i;
            if (ImGui::Selectable(levels[i].c_str(), selected))
            {
                _controller._selectedCatalogLevelIndex = i;
                _controller._selectedCatalogIndex = 0;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::TreePop();
    }

    const std::vector<SpellLibraryController::CatalogEntry> visibleEntries = _controller.getVisibleCatalogEntries(entries, levels);
    if (_controller._selectedCatalogIndex >= static_cast<int>(visibleEntries.size()))
        _controller._selectedCatalogIndex = 0;

    const bool catalogWide = TaskUi::hasRoomForColumns(44.0f);
    if (ImGui::BeginTable("SpellCatalogLayout", catalogWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(gm::localization::trUi("Spells"), ImGuiTableColumnFlags_WidthStretch, 0.58f);
        if (catalogWide) ImGui::TableSetupColumn(gm::localization::trUi("Details"), ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        if (visibleEntries.empty())
        {
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No spells match the current filters."));
        }
        else if (ImGui::BeginTable("SpellCatalogItemsWithSource", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(catalogWide ? 0.95f : 0.4f, 30.0f))))
        {
            ImGui::TableSetupColumn(localizedUiText("Spell"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn(localizedUiText("Level"));
        ImGui::TableSetupColumn(localizedUiText("Source"));
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(visibleEntries.size()); ++i)
            {
                const SpellLibraryController::CatalogEntry &entry = visibleEntries[static_cast<std::size_t>(i)];
                const SpellModel &spell = entry.spell;
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(spell.name.empty() ? "<Unnamed Spell>" : spell.name.c_str(), _controller._selectedCatalogIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                    _controller._selectedCatalogIndex = i;
                showHelpTooltip(spell.description.empty() ? spell.effectSummary.c_str() : spell.description.c_str());
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(spellLevelLabel(spell.level));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped(localizedUiLabel(entry.campaignCustom ? "Campaign" : "Ruleset"));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::TableNextColumn();
        if (!visibleEntries.empty())
        {
            const SpellLibraryController::CatalogEntry &selectedEntry = visibleEntries[static_cast<std::size_t>(_controller._selectedCatalogIndex)];
            if (selectedEntry.campaignCustom)
            {
                if (drawUiButton("Open In Editor", UiButtonRole::Primary))
                { _controller.selectCatalogCustomSpell(selectedEntry); _openCustomEditor = true; }
                showHelpTooltip("Load this campaign spell into the custom spell editor tab.");
            }
            else
            {
                if (drawUiButton("Copy To Campaign", UiButtonRole::Primary))
                    _controller.copyCatalogSpellToCampaign(selectedEntry);
                showHelpTooltip("Create a campaign-owned copy that can be edited, assigned, and saved with this campaign.");
            }
            ImGui::Separator();
            drawCatalogSpellDetails(selectedEntry.spell, selectedEntry.source);
        }
        ImGui::EndTable();
    }
}

void SpellLibraryView::drawSpellList()
{
    if (!_controller._retainedDrafts.empty() && FormUi::BeginCombo("##RetainedSpellDrafts",
        gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German ? "Entwurf fortsetzen" : "Resume a draft"))
    {
        EntityId selected;
        for (const auto &entry : _controller._retainedDrafts)
        {
            const std::string label = entry.second.spell.name + "##" + entry.first;
            if (ImGui::Selectable(label.c_str())) selected = entry.first;
        }
        ImGui::EndCombo();
        if (!selected.empty()) { _controller.retainDraft(); _controller.restoreDraft(selected); }
    }
    drawHelpText("Campaign Spell Library", "Create the spells available in this campaign. Characters can then learn, prepare, and cast them from their sheets.");
    if (drawUiButton("New Spell", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Start a fresh campaign spell draft."))
        _controller.createNewDraft();

    ImGui::Separator();
    if (_controller._currentCampaign.customSpells.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No custom spells yet."));
        return;
    }

    if (ImGui::BeginTable("SpellDirectoryTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, TaskUi::listHeight(0.4f))))
    {
        ImGui::TableSetupColumn(gm::localization::trUi("Spell"), ImGuiTableColumnFlags_WidthStretch, 0.54f);
        ImGui::TableSetupColumn(gm::localization::trUi("Level"), ImGuiTableColumnFlags_WidthFixed, 86.0f);
        ImGui::TableSetupColumn(gm::localization::trUi("School"), ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.customSpells.size()); ++i)
        {
            const SpellModel &spell = _controller._currentCampaign.customSpells[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(spell.name.c_str(), _controller._selectedSpellIndex == i, ImGuiSelectableFlags_SpanAllColumns))
                _controller.selectSpell(i);
            showHelpTooltip(spell.description.empty() ? spell.effectSummary.c_str() : spell.description.c_str());
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(spellLevelLabel(spell.level));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(spellSchoolLabel(spell.school));
        }
        ImGui::EndTable();
    }
}
