#include "SpellLibraryViewSupport.hpp"

SpellLibraryView::SpellLibraryView(SpellLibraryController &controller)
    : DockableWidget("Spell Library", 0, ImVec2(1060.0f, 720.0f)), _controller(controller)
{
}

void SpellLibraryView::draw()
{
    if (!ImGui::BeginTabBar("SpellLibraryTabs"))
        return;

    if (ImGui::BeginTabItem(gm::localization::trUi("Catalog Browser"), nullptr, _openCatalog ? ImGuiTabItemFlags_SetSelected : 0))
    {
        _openCatalog = false;
        drawCatalogBrowser();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem(gm::localization::trUi("Import"), nullptr, _openImport ? ImGuiTabItemFlags_SetSelected : 0))
    {
        _openImport = false;
        drawImportPanel();
        ImGui::EndTabItem();
    }

    if (!ImGui::BeginTabItem(gm::localization::trUi("Custom Spell Editor"), nullptr, _openCustomEditor ? ImGuiTabItemFlags_SetSelected : 0))
    {
        ImGui::EndTabBar();
        return;
    }

    _openCustomEditor = false;
    const bool editorWide = TaskUi::hasRoomForColumns();
    if (ImGui::BeginTable("SpellLibraryLayout", editorWide ? 2 : 1, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(gm::localization::trUi("Spell Directory"), ImGuiTableColumnFlags_WidthStretch, 0.34f);
        if (editorWide) ImGui::TableSetupColumn(gm::localization::trUi("Spell Editor"), ImGuiTableColumnFlags_WidthStretch, 0.66f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawSpellList();
        ImGui::TableNextColumn();
        drawSpellEditor();
        ImGui::EndTable();
    }
    ImGui::EndTabItem();
    ImGui::EndTabBar();
}

void SpellLibraryView::drawImportPanel()
{
    drawHelpText("Spell Import", "Paste SRD-compatible spell data as CSV with a header row or flat JSON spell objects. Imported spells become campaign custom spells with source and license attribution metadata.");
    const std::string importSelection = _controller._importPreviewReady ? std::to_string(_controller._importPreviewRows.size()) + " spell(s) previewed" : "No preview yet";
    UiStatusActionStrip importStrip;
    importStrip.mode = "Guided Import";
    importStrip.selection = importSelection.c_str();
    importStrip.nextAction = _controller._importPreviewReady ? "Review warnings, then Apply Import." : "Paste data and Preview Import before changing the campaign.";
    importStrip.help = "Import is preview-first so duplicate updates, attribution, and validation warnings are visible before spells are created or overwritten.";
    drawStatusActionStrip("SpellImportStatus", importStrip);

    if (!_controller._statusMessage.empty())
        ImGui::TextColored(ImVec4(0.48f, 0.78f, 0.54f, 1.0f), localizedUiLabel("%s").c_str(), _controller._statusMessage.c_str());
    if (!_controller._importPreviewMessage.empty())
        ImGui::TextColored(ImVec4(0.94f, 0.71f, 0.36f, 1.0f), localizedUiLabel("%s").c_str(), _controller._importPreviewMessage.c_str());
    if (!_controller._importReceiptMessage.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Import Receipt"));
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._importReceiptMessage.c_str());
    }

    TaskUi::fieldLabel(localizedUiText("Source / Attribution"));

    FormUi::InputText("##Source / Attribution", &_controller._importSource);
    showHelpTooltip("Example: SRD 5.1 CC-BY-4.0, your publisher pack, or your table's homebrew document.");
    TaskUi::fieldLabel(localizedUiText("License"));
    FormUi::InputText("##License", &_controller._importLicense);
    showHelpTooltip("Stored on imported spells as attribution metadata.");
    TaskUi::fieldLabel(localizedUiText("Spell Data"));
    if (FormUi::InputTextMultiline("##Spell Data", &_controller._importText, ImVec2(-1.0f, 360.0f)))
    {
        _controller._importPreviewReady = false; _controller._importPreviewRows.clear();
        _controller._importPreviewMessage.clear(); _controller._importReceiptMessage.clear();
    }
    if (!_controller._importText.empty() && ImGui::Button(uiTextForLocale("Discard import text", "Importtext verwerfen")))
        ImGui::OpenPopup("DiscardSpellImport");
    if (ImGui::BeginPopupModal("DiscardSpellImport", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(uiTextForLocale("Discard the pending import text and preview?", "Ausstehenden Importtext und Vorschau verwerfen?"));
        if (drawModalActionButtons("Discard", UiButtonRole::Advanced, "Cancel", 110.0f))
        {
            _controller._importText.clear(); _controller._importPreviewRows.clear();
            _controller._importPreviewReady = false; _controller._importPreviewMessage.clear();
        }
        ImGui::EndPopup();
    }
    showHelpTooltip("CSV fields can include name, level, school, resolution, casting_time, range, duration, concentration, ritual, classes, attack_bonus, save, damage, healing, area, effect, scaling, description. JSON uses similar camelCase or snake_case fields.");
    if (drawUiButton("Preview Import", UiButtonRole::Advanced, ImVec2(160.0f, 0.0f), "Parse the pasted data and show what will be added or updated."))
        _controller.previewImportFromText();
    TaskUi::sameLineIfFits();
    if (drawUiButton("Apply Import", _controller._importPreviewReady, "Preview imported spells before applying them.", UiButtonRole::Primary, ImVec2(160.0f, 0.0f), "Create or update the previewed campaign spells."))
        ImGui::OpenPopup(localizedUiLabel("Apply Spell Import").c_str());
    if (ImGui::BeginPopupModal(localizedUiLabel("Apply Spell Import").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        int added = 0;
        int updated = 0;
        int warnings = 0;
        for (const SpellLibraryController::ImportPreviewRow &row : _controller._importPreviewRows)
        {
            if (row.updatesExisting)
                ++updated;
            else
                ++added;
            warnings += static_cast<int>(row.warnings.size());
        }

        ImGui::TextWrapped(localizedUiLabel("Apply the current spell import preview to this campaign?").c_str());
        drawInlineLabelValue("New spells", std::to_string(added).c_str());
        drawInlineLabelValue("Updates", std::to_string(updated).c_str());
        drawInlineLabelValue("Warnings", std::to_string(warnings).c_str());
        drawInlineLabelValue("Source", _controller._importSource.empty() ? localizedUiText("Unspecified") : _controller._importSource.c_str());
        drawInlineLabelValue("License", _controller._importLicense.empty() ? localizedUiText("Unspecified") : _controller._importLicense.c_str());
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller.importRollbackNotice().c_str());
        if (drawModalActionButtons("Apply Import", UiButtonRole::Primary))
            _controller.applyImportPreview();
        ImGui::EndPopup();
    }
    drawMutedHelpText(_controller.importRollbackNotice().c_str(), "Rollback messaging is stored with the import preview and receipt.");

    if (!_controller._importPreviewRows.empty())
    {
        ImGui::SeparatorText(gm::localization::trUi("Import Preview"));
        const std::string rollbackPlan = _controller.importPreviewRollbackPlan();
        if (!rollbackPlan.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), rollbackPlan.c_str());
        if (ImGui::BeginTable("SpellImportPreview", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, ImVec2(-1.0f, 220.0f)))
        {
            ImGui::TableSetupColumn(gm::localization::trUi("Spell"), ImGuiTableColumnFlags_WidthStretch, 0.28f);
            ImGui::TableSetupColumn(gm::localization::trUi("Level"), ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableSetupColumn(gm::localization::trUi("Use"), ImGuiTableColumnFlags_WidthStretch, 0.18f);
            ImGui::TableSetupColumn(gm::localization::trUi("Actions"), ImGuiTableColumnFlags_WidthStretch, 0.22f);
            ImGui::TableSetupColumn(gm::localization::trUi("Warnings"), ImGuiTableColumnFlags_WidthStretch, 0.32f);
            ImGui::TableHeadersRow();
            for (const SpellLibraryController::ImportPreviewRow &row : _controller._importPreviewRows)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                tableTextWrapped(row.spell.name.empty() ? "<Unnamed Spell>" : row.spell.name);
                ImGui::TableSetColumnIndex(1);
                tableTextWrapped(spellLevelLabel(row.spell.level));
                ImGui::TableSetColumnIndex(2);
                tableTextWrapped(spellResolutionLabel(row.spell.resolution));
                ImGui::TableSetColumnIndex(3);
                tableTextWrapped(row.action);
                ImGui::TableSetColumnIndex(4);
                if (row.warnings.empty())
                    tableTextWrapped("Ready");
                else
                    tableTextWrapped(joinFieldNames(row.warnings));
            }
            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText(gm::localization::trUi("CSV Example"));
    ImGui::TextWrapped(localizedUiLabel("name,level,school,resolution,casting_time,range,duration,concentration,ritual,classes,damage,save,area,effect,description").c_str());
    ImGui::TextWrapped(localizedUiLabel("Burning Hands,1,Evocation,Saving Throw,1 Action,Self,Instantaneous,false,false,\"Sorcerer, Wizard\",3d6 fire,Dexterity,15-foot cone,Flames fill a cone,SRD-compatible summary text").c_str());
}
