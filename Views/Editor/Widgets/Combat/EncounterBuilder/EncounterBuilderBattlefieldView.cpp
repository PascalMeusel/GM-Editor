#include "EncounterBuilderViewSupport.hpp"

void EncounterBuilderView::drawBattlefield()
{
    EncounterModel &encounter = _controller._draftEncounter;
    if (encounter.theaterOfMind)
    {
        ImGui::TextWrapped("%s", uiTextForLocale("This encounter uses theatre of the mind.", "Diese Begegnung nutzt das Kopfkino."));
        if (!ImGui::TreeNode(uiTextForLocale("Prepare an optional map", "Optionale Karte vorbereiten"))) return;
        ImGui::TreePop();
    }
    if (_controller._mapAuthoringHandler)
    {
        if (ImGui::Button(uiTextForLocale("Create map in World", "Karte in Welt erstellen")))
            _controller.openSharedMapAuthoring(true);
        if (encounter.tacticalMapId.has_value())
        {
            if (ImGui::Button(uiTextForLocale("Edit assigned map in World", "Zugewiesene Karte in Welt bearbeiten")))
                _controller.openSharedMapAuthoring(false);
        }
        ImGui::TextWrapped("%s", uiTextForLocale(
            "Save the map in World, then assign it back to this encounter.",
            "Speichere die Karte in Welt und weise sie danach dieser Begegnung zu."));
    }
    const auto eligibleMapCount = std::count_if(
        _controller._currentCampaign.maps.begin(), _controller._currentCampaign.maps.end(),
        [](const MapModel &map) { return isEncounterScaleMap(map); });
    if (eligibleMapCount == 0)
    {
        ImGui::TextDisabled("%s", uiTextForLocale(
            "No dungeon, encounter, or city maps exist yet.",
            "Noch keine Dungeon-, Begegnungs- oder Stadtkarten vorhanden."));
    }
    else
    {
        drawFieldLabel("Battlefield map");
        const auto current = std::find_if(
            _controller._currentCampaign.maps.begin(), _controller._currentCampaign.maps.end(),
            [&](const MapModel &map) {
                return map.metadata.id == encounter.tacticalMapId && isEncounterScaleMap(map);
            });
        const std::string currentName = current == _controller._currentCampaign.maps.end()
            ? uiTextForLocale("Standalone battlefield · choose a map", "Eigenes Schlachtfeld · Karte wählen")
            : current->name + " · " + localizedUiLabel(encounterMapTypeLabel(*current));
        ImGui::SetNextItemWidth(-1.0f);
        if (FormUi::BeginCombo("##BattlefieldMap", currentName.c_str()))
        {
            for (int i = 0; i < static_cast<int>(_controller._currentCampaign.maps.size()); ++i)
            {
                const auto &map = _controller._currentCampaign.maps[i];
                if (!isEncounterScaleMap(map))
                    continue;
                const std::string option = map.name + " · " +
                                           localizedUiLabel(encounterMapTypeLabel(map)) +
                                           "##" + map.metadata.id;
                if (ImGui::Selectable(option.c_str(), map.metadata.id == encounter.tacticalMapId))
                {
                    _controller._selectedAtlasMapIndex = i;
                    _controller.useSelectedAtlasTacticalMap();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("%s", uiTextForLocale(
            "Dungeon, encounter, and city maps only. World maps stay in the atlas.",
            "Nur Dungeon-, Begegnungs- und Stadtkarten. Weltkarten bleiben im Atlas."));
        if (!encounter.sourceMapId.empty())
            ImGui::TextDisabled(localizedUiLabel("Linked to atlas source revision %llu").c_str(), static_cast<unsigned long long>(encounter.sourceMapRevision));
    }

    ImGui::SetNextItemOpen(!encounter.tacticalMapId.has_value(), ImGuiCond_Once);
    if (ImGui::TreeNode(uiTextForLocale("Create a battle map", "Kampfkarte erstellen")))
    {
    const char *templateItems = "Open Ground\0Forest Road\0River Crossing\0Ruins\0Village Street\0City Square\0Cave\0Stone Dungeon\0";
    drawFieldLabel("Environment");
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::Combo("##TacticalEnvironment", &_controller._tacticalTemplateIndex, localizedComboItems(templateItems).c_str());
    ImGui::SetNextItemOpen(_showTacticalOptions, ImGuiCond_Always);
    _showTacticalOptions = ImGui::TreeNode(uiTextForLocale("Options##BattleMap", "Optionen##BattleMap"));
    if (_showTacticalOptions)
    {
    drawFieldLabel("Map size");
    if (ImGui::BeginTable("TacticalMapSizePresets", 3,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        const char *labels[] = {"Small", "Standard", "Large"};
        for (int preset = 0; preset < 3; ++preset)
        {
            ImGui::TableNextColumn();
            if (FormUi::RadioButton(localizedUiLabel(labels[preset]).c_str(),
                                   _controller._tacticalSizePresetIndex == preset))
            {
                _controller._tacticalSizePresetIndex = preset;
                constexpr int widths[] = {30, 40, 60};
                constexpr int heights[] = {22, 30, 45};
                _controller._tacticalWidth = widths[preset];
                _controller._tacticalHeight = heights[preset];
            }
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("%s", _controller._tacticalSizePresetIndex == 2
        ? uiTextForLocale("More room for movement and multiple approaches.", "Mehr Platz für Bewegung und verschiedene Zugänge.")
        : uiTextForLocale("Choose the area needed for this encounter.", "Wähle die benötigte Fläche für diese Begegnung."));
    ImGui::TextUnformatted(uiTextForLocale("Additional cover", "Zusätzliche Deckung"));
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::SliderInt("##TacticalGeneratorCoverDensity", &_controller._tacticalCoverDensity, 0, 60);
    showHelpTooltip("Template-defining terrain remains when additional cover is zero.");
    ImGui::TextUnformatted(uiTextForLocale("Objectives", "Ziele"));
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::SliderInt("##TacticalGeneratorObjectives", &_controller._tacticalObjectiveCount, 1, 3);
    if (ImGui::TreeNode(localizedUiLabel("Advanced##TacticalGenerator").c_str()))
    {
        if (ImGui::BeginTable("TacticalMapExactSize", 2,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextColumn();
            drawFieldLabel("Width");
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt("##TacticalGeneratorWidth", &_controller._tacticalWidth))
                _controller._tacticalSizePresetIndex = 3;
            if (ImGui::IsItemDeactivatedAfterEdit())
                _controller._tacticalWidth = std::clamp(_controller._tacticalWidth, 12, 80);
            ImGui::TableNextColumn();
            drawFieldLabel("Height");
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::InputInt("##TacticalGeneratorHeight", &_controller._tacticalHeight))
                _controller._tacticalSizePresetIndex = 3;
            if (ImGui::IsItemDeactivatedAfterEdit())
                _controller._tacticalHeight = std::clamp(_controller._tacticalHeight, 10, 80);
            ImGui::EndTable();
        }
        FormUi::Checkbox(localizedUiLabel("Keep generation seed").c_str(), &_controller._keepTacticalSeed);
        ImGui::BeginDisabled(!_controller._keepTacticalSeed);
        drawFieldLabel("Seed");
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputScalar("##TacticalGeneratorSeed", ImGuiDataType_U32, &_controller._tacticalSeed);
        ImGui::EndDisabled();
        FormUi::Checkbox(localizedUiLabel("Use hex grid").c_str(), &_controller._useAdvancedHexGrid);
        ImGui::TextDisabled(localizedUiLabel("Square is recommended for the first Fantasy/Neutral release.").c_str());
        ImGui::TreePop();
    }
    ImGui::TreePop();
    }
    if (drawUiButton("Generate 3 battle maps", !_controller._tacticalGeneration.valid(),
                     "Generation is already running.", UiButtonRole::Primary, ImVec2(240.0f, 0.0f),
                     "Generate deterministic battlefields with two safe start zones, cover, and line-of-sight breaks."))
    {
        _controller.generateTacticalCandidates();
        if (_controller._tacticalGeneration.valid()) _showTacticalOptions = false;
    }
    if (_controller._tacticalGeneration.valid())
    {
        if (drawUiButton("Cancel generation"))
        {
            _controller.cancelTacticalGeneration();
            _controller._tacticalStatus = gm::localization::trUi("Generation cancelled.");
        }
    }

    if (!_controller._tacticalCandidates.empty())
    {
        const int count = static_cast<int>(_controller._tacticalCandidates.size());
        _controller._selectedTacticalCandidate = std::clamp(_controller._selectedTacticalCandidate, 0, count - 1);
        if (ImGui::BeginTable("TacticalDesignSwitches", std::min(count, std::max(1,
            static_cast<int>(ImGui::GetContentRegionAvail().x / (ImGui::GetFontSize() * 9.0f)))),
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            for (int index = 0; index < count; ++index)
            {
                ImGui::TableNextColumn();
                const std::string label = localizedUiLabel("Design") + " " + std::to_string(index + 1);
                if (ImGui::Selectable(label.c_str(), _controller._selectedTacticalCandidate == index, 0,
                    ImVec2(0.0f, ImGui::GetFrameHeight()))) _controller._selectedTacticalCandidate = index;
            }
            ImGui::EndTable();
        }
        const auto &candidate = _controller._tacticalCandidates[static_cast<std::size_t>(_controller._selectedTacticalCandidate)];
        for (const auto &issue : candidate.readinessIssues)
            ImGui::TextWrapped("%s", localizedUiLabel(issue.c_str()).c_str());
        drawTacticalCandidatePreview(candidate.map, _previewPlans, "##TacticalPreview", true);
        showHelpTooltip("A/B = start zones · Diamond = objective");
        if (drawUiButton("Use selected battle map", candidate.readinessIssues.empty(),
                         "Resolve battlefield readiness issues before using this design.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                         "Add the tactical map to the atlas and link its cells to this encounter."))
            _controller.useSelectedTacticalCandidate();
    }
    ImGui::TreePop();
    }

    if (ImGui::TreeNode(uiTextForLocale("Map details", "Kartendetails")))
    {
    const int metricColumnCount = ImGui::GetContentRegionAvail().x < 720.0f ? 3 : 5;
    if (ImGui::BeginTable("EncounterBuilderMetrics", metricColumnCount, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Map").c_str());
        const std::string mapContextName = encounter.mapContextName.empty() ? localizedUiLabel("Standalone") : encounter.mapContextName;
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), mapContextName.c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Grid").c_str());
        ImGui::Text(localizedUiLabel("%d x %d").c_str(), encounter.battlefieldWidth, encounter.battlefieldHeight);
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Scale").c_str());
        ImGui::Text(localizedUiLabel("%d ft").c_str(), encounter.cellSizeFeet);
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Participants").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), static_cast<int>(encounter.participants.size()));
        ImGui::TableNextColumn();
        ImGui::TextDisabled(localizedUiLabel("Round").c_str());
        ImGui::Text(localizedUiLabel("%d").c_str(), encounter.round);
        ImGui::EndTable();
    }

    ImGui::TreePop();
    }
}
