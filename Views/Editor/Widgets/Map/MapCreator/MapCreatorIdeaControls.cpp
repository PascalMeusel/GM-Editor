#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawIdeaControls(bool generationBusy)
{
    MapModel &map = _controller._draftMap;

    ImGui::SeparatorText(
        localizedUiLabel(map.creationIntent == MapCreationIntent::WorldOverview ? "Create a world map" : "Map setup")
            .c_str());
    undoableInputText("Name", map.name, "Edit Map Details");
    drawArtStyleControl(generationBusy);
    const MapCreationIntent currentIntent = map.creationIntent;
    const auto intentLabel = [](MapCreationIntent intent) {
        std::string label = localizedUiLabel(mapCreationIntentLabel(intent));
        if (gm::mapui::mapCreatorExperimentalIntent(intent))
            label += uiTextForLocale(" (Experimental)", " (Experimentell)");
        return label;
    };
    const std::string currentIntentLabel = intentLabel(currentIntent);
    if (FormUi::BeginCombo(localizedUiLabel("Map Type").c_str(), currentIntentLabel.c_str()))
    {
        for (int intentIndex = 0; intentIndex < mapCreatorNormalCreationIntentCount; ++intentIndex)
        {
            const MapCreationIntent intent = static_cast<MapCreationIntent>(intentIndex);
            const bool selected = currentIntent == intent;
            if (ImGui::Selectable(intentLabel(intent).c_str(), selected))
            {
                _controller.captureUndoSnapshot("Edit Map Details");
                _controller._generation.selectedCreationIntentIndex = intentIndex;
                _controller.syncCreationIntentFromIndex(true);
                _draftWidth = map.width;
                _draftHeight = map.height;
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (gm::mapui::mapCreatorExperimentalIntent(map.creationIntent))
        ImGui::TextWrapped(
            "%s", uiTextForLocale("Experimental generator. Review the layout carefully before using it in a session.",
                                  "Experimenteller Generator. Prüfe den Aufbau vor dem Einsatz am Spieltisch."));
    if (currentIntent == MapCreationIntent::TacticalEncounter)
        drawDisabledWrappedText(
            "This existing tactical map remains fully editable. New battle maps are normally prepared with their encounter context.");
    else
        drawDisabledWrappedText(
            "Battle maps are created in Encounters so objectives, participants, start zones, and terrain stay together.");
    if (drawUiButton("Prepare battle map in Encounters", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Open encounter preparation without changing or saving this map draft."))
        _controller.openEncounterPreparation();
    if (currentIntent != MapCreationIntent::TacticalEncounter &&
        ImGui::TreeNode(localizedUiLabel("Standalone tactical map (advanced)").c_str()))
    {
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "Use this only when no encounter context is needed. The map can still be attached to an encounter later.")
                .c_str());
        if (drawUiButton("Create standalone tactical map", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f),
                         "Switch this draft to the tactical map preset as an explicit advanced choice."))
        {
            _controller.captureUndoSnapshot("Edit Map Details");
            _controller._generation.selectedCreationIntentIndex = static_cast<int>(MapCreationIntent::TacticalEncounter);
            _controller.syncCreationIntentFromIndex(true);
            _draftWidth = map.width;
            _draftHeight = map.height;
        }
        ImGui::TreePop();
    }
    if (FormUi::Combo(localizedUiLabel("Map size").c_str(), &_controller._generation.selectedSizePresetIndex,
                      localizedComboItems(sizePresetItems).c_str()))
    {
        _controller.captureUndoSnapshot("Resize Map");
        _controller.syncCreationIntentFromIndex(true);
        _draftWidth = map.width;
        _draftHeight = map.height;
    }
    if (map.sizePreset != MapSizePreset::Custom)
    {
        drawDisabledWrappedText(sizePresetDescription(map.sizePreset));
    }
    ImGui::Spacing();
    if (drawUiButton("Import existing map image", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Copy a PNG or JPEG into this campaign and use it as an editable, traceable map reference."))
        _controller.browseForReferenceImage();
    drawDisabledWrappedText(
        "Imported images start GM-only. You decide explicitly when the original image is safe for players.");
    if (map.creationIntent == MapCreationIntent::WorldOverview)
    {
        ImGui::TextWrapped(localizedUiLabel("What do you already know about the world?").c_str());
        const char *entryLabels[] = {"Use my notes", "Suggest ideas", "Empty world"};
        float requiredEntryWidth = ImGui::GetStyle().CellPadding.x * 6.0f + ImGui::GetStyle().FramePadding.x * 6.0f;
        for (const char *entryLabel : entryLabels)
            requiredEntryWidth += ImGui::CalcTextSize(localizedUiLabel(entryLabel).c_str()).x;
        const int entryColumns = ImGui::GetContentRegionAvail().x >= requiredEntryWidth ? 3 : 1;
        if (ImGui::BeginTable("WorldEntryModes", entryColumns,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            for (int mode = 0; mode < 3; ++mode)
            {
                ImGui::TableNextColumn();
                if (drawChoiceButton(entryLabels[mode], _controller._generation.worldEntryMode == mode))
                    _controller._generation.worldEntryMode = mode;
            }
            ImGui::EndTable();
        }

        if (_controller._generation.worldEntryMode == 0)
        {
            ImGui::TextWrapped(
                localizedUiLabel("Write only what you know. Names, numbers, and short sentences are enough.").c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped(
                localizedUiLabel("Example: There are three nations named X, Y, and Z, and a large capital named Typo.")
                    .c_str());
            ImGui::PopStyleColor();
            if (FormUi::InputTextMultiline("##WorldBrief", &_controller._generation.worldBrief, ImVec2(-1.0f, 104.0f),
                                           ImGuiInputTextFlags_WordWrap))
                _controller._generation.worldBriefParsed = false;
            if (drawUiButton("Understand notes", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                _controller.interpretWorldBrief();

            if (_controller._generation.worldBriefParsed)
            {
                ImGui::TextColored(
                    ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s",
                    localizedUiLabel("Notes parsed. Review recognised facts and unresolved notes.").c_str());
            }
        }
        else if (_controller._generation.worldEntryMode == 1)
        {
            ImGui::TextWrapped(
                localizedUiLabel("Choose a premise. You can rename every nation and place before generating.").c_str());
            const char *ideaTitles[] = {"River kingdoms", "Forest frontier", "Shattered coast"};
            const char *ideaBodies[] = {
                "Three rival realms share one great river and an independent trade capital.",
                "Two old kingdoms border a vast forest where new settlements and ruins compete for space.",
                "Island nations, stormy channels, ports, and an inland realm connected by a mountain pass."};
            for (int idea = 0; idea < 3; ++idea)
            {
                ImGui::PushID(3000 + idea);
                if (ImGui::Selectable(localizedUiLabel(ideaTitles[idea]).c_str(), _controller._generation.worldIdeaIndex == idea,
                                      0, ImVec2(0.0f, 32.0f)))
                    _controller._generation.worldIdeaIndex = idea;
                ImGui::TextWrapped("%s", localizedUiLabel(ideaBodies[idea]).c_str());
                ImGui::PopID();
            }
            if (drawUiButton("Use selected idea", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                useSelectedWorldIdea();
        }
        else
        {
            ImGui::TextWrapped(
                localizedUiLabel(
                    "Start with an editable generated world and name places later. Coastlines, terrain, rivers, settlements, and roads are filled automatically.")
                    .c_str());
            _controller._generation.worldBrief.clear();
            _controller._generation.briefNationNames.clear();
            _controller._generation.briefCityNames.clear();
            _controller._generation.briefFacts.clear();
            _controller._generation.briefCapitalName.clear();
            _controller._generation.worldBriefParsed = true;
        }
    }
    if (ImGui::TreeNode(localizedUiLabel("Map settings and existing maps").c_str()))
    {
#if !defined(_WIN32)
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "Enter or paste a complete .ttrpgmappkg path. This replaces the unavailable native file browser on this platform.")
                .c_str());
        FormUi::InputTextWithHint(localizedUiLabel("Map package path").c_str(), "/path/to/map.ttrpgmappkg",
                                  &_controller._transfer.importExportPath);
#endif
        if (drawUiButton("Import editable map package", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                         "Choose or enter a .ttrpgmappkg file and review it before replacing this draft."))
        {
            _controller.browseForMapPackageImport();
            if (_controller._transfer.confirmMapPackageImport)
                ImGui::OpenPopup(localizedUiLabel("Review Map Package").c_str());
        }
        if (drawUiButton("New Map", UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f),
                         "Start a clean draft map. Unsaved edits in the current draft will be replaced."))
            ImGui::OpenPopup(localizedUiLabel("New Map Review").c_str());
        if (!_controller._currentCampaign.maps.empty())
        {
            std::vector<const char *> mapLabels;
            for (const auto &existingMap : _controller._currentCampaign.maps)
                mapLabels.push_back(existingMap.name.empty() ? localizedUiText("<Unnamed Map>")
                                                             : existingMap.name.c_str());
            if (_controller._persistence.selectedMapIndex < 0)
                _controller._persistence.selectedMapIndex = 0;
            FormUi::Combo(localizedUiLabel("Load Existing").c_str(), &_controller._persistence.selectedMapIndex, mapLabels.data(),
                          static_cast<int>(mapLabels.size()));
            if (drawUiButton("Load Map", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                             "Load this map into the editor. Unsaved changes in the current draft will be replaced."))
                ImGui::OpenPopup(localizedUiLabel("Load Map Review").c_str());
        }
        undoableInputTextMultiline("Description", map.description, ImVec2(-1.0f, 72.0f), "Edit Map Details");
        if (static_cast<MapSizePreset>(_controller._generation.selectedSizePresetIndex) == MapSizePreset::Custom)
        {
            if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
            {
                ImGui::TextUnformatted(localizedUiLabel("Exact cell dimensions").c_str());
                FormUi::InputInt(localizedUiLabel("Width").c_str(), &_draftWidth);
                FormUi::InputInt(localizedUiLabel("Height").c_str(), &_draftHeight);
                drawDisabledWrappedText("4 to 120 cells per side; applied when you continue.");
                ImGui::TreePop();
            }
        }
        undoableCheckbox("Main Map", map.isMainMap, "Edit Map Details");
        if (!map.isMainMap)
        {
            const std::vector<const char *> rawParentLabels = _controller.getParentMapLabels();
            std::vector<std::string> parentLabelStorage;
            parentLabelStorage.reserve(rawParentLabels.size());
            for (const char *label : rawParentLabels)
                parentLabelStorage.push_back(localizedModelPickerLabel(label));
            std::vector<const char *> parentLabels;
            parentLabels.reserve(parentLabelStorage.size());
            for (const std::string &label : parentLabelStorage)
                parentLabels.push_back(label.c_str());
            if (_controller._editing.selectedParentIndex >= static_cast<int>(parentLabels.size()))
                _controller._editing.selectedParentIndex = 0;
            FormUi::Combo(localizedUiLabel("Parent").c_str(), &_controller._editing.selectedParentIndex, parentLabels.data(),
                          static_cast<int>(parentLabels.size()));
            ImGui::TextWrapped("%s", uiTextForLocale(
                "A parent here groups the map only. To set its exact location, open Atlas, select an area on the parent, then place this existing map.",
                "Eine übergeordnete Karte ordnet die Karte hier nur zu. Für die genaue Position öffne den Atlas, wähle einen Bereich auf der übergeordneten Karte und platziere diese bestehende Karte."));
        }
        ImGui::TreePop();
    }
}
