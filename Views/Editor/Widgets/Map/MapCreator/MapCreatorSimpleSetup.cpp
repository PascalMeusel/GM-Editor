#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Models/Map/CurrentMapArtRecipe.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSimpleCreation()
{
    MapModel &map = _controller._draftMap;
    const bool busy = _controller.generationBusy();
    if (_controller._persistence.editingMapIndex >= 0 || mapContainsVisibleDraftContent(map))
    {
        if (drawUiButton(uiTextForLocale("Back to current map", "Zurück zur aktuellen Karte"), !busy,
                         "Generation is running; wait before changing steps.", UiButtonRole::Secondary))
        {
            setWorkflowStep(3);
            _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
            return;
        }
    }
    const float unit = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
    const auto drawIssues = [](const std::vector<MapGenerationIssue> &issues) {
        for (const auto &issue : issues)
        {
            ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(issue)).c_str());
            const char *action = generationIssueAction(issue);
            if (action && *action)
                ImGui::TextWrapped("%s", localizedUiLabel(action).c_str());
        }
    };
    const auto applyDimensions = [&]() {
        _draftWidth = std::clamp(_draftWidth, 4, 120);
        _draftHeight = std::clamp(_draftHeight, 4, 120);
        if (map.width == _draftWidth && map.height == _draftHeight)
            return;
        _controller.captureUndoSnapshot("Resize Map");
        map.width = _draftWidth;
        map.height = _draftHeight;
        _controller.resizeDraftCells();
    };
    const auto generate = [&]() {
        applyDimensions();
        if (!_keepGenerationSeed)
            _controller._generation.generatorSeed = std::random_device{}();
        _previewedGenerationCandidate = 0;
        _lastInteractivePreviewCandidate = -1;
        _resetDesignPreviewScroll = true;
        _controller.queueDraftGeneration();
        setWorkflowStep(2);
    };

    if (_workflowStep < 2)
    {
        drawStructuredCreation();
        return;
    }

    if (busy)
    {
        const float width = std::min(ImGui::GetContentRegionAvail().x, 640.0f * unit);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             std::max(0.0f, (ImGui::GetContentRegionAvail().x - width) * 0.5f));
        ImGui::BeginChild("SimpleGenerationProgress", ImVec2(width, 0));
        ImGui::Dummy(ImVec2(0.0f, std::max(20.0f, ImGui::GetContentRegionAvail().y * 0.25f)));
        ImGui::TextUnformatted(uiTextForLocale("Creating your designs…", "Deine Entwürfe entstehen…"));
        const std::string progress = _controller.generationProgressLabel();
        ImGui::ProgressBar(_controller.generationProgressFraction(), ImVec2(-1.0f, 0.0f));
        showHelpTooltip(progress.c_str());
        if (drawUiButton(uiTextForLocale("Cancel", "Abbrechen"), UiButtonRole::Secondary))
            _controller.cancelDraftGeneration();
        ImGui::EndChild();
        return;
    }
    if (_controller._generation.candidates.empty())
    {
        ImGui::TextWrapped("%s", uiTextForLocale("No design to choose yet.", "Noch kein Entwurf zur Auswahl."));
        drawIssues(_controller._generation.lastValidation.blockers);
        if (!_controller._integration.statusMessage.empty())
            ImGui::TextWrapped("%s", _controller._integration.statusMessage.c_str());
        if (drawUiButton(uiTextForLocale("Try again", "Erneut versuchen"), UiButtonRole::Primary))
            generate();
        if (drawUiButton(uiTextForLocale("Change options", "Optionen ändern"), UiButtonRole::Secondary))
            setWorkflowStep(1);
        return;
    }

    bool newDesigns = false;
    const int count = static_cast<int>(_controller._generation.candidates.size());
    _previewedGenerationCandidate = std::clamp(_previewedGenerationCandidate, 0, count - 1);
    const int switchColumns =
        std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / (145.0f * unit)), 1, count + 2);
    if (ImGui::BeginTable("SimpleDesignSwitches", switchColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        for (int index = 0; index < count; ++index)
        {
            ImGui::TableNextColumn();
            const std::string label = std::string(uiTextForLocale("Design ", "Entwurf ")) + std::to_string(index + 1);
            if (drawChoiceButton(label.c_str(), _previewedGenerationCandidate == index))
                _previewedGenerationCandidate = index;
        }
        ImGui::TableNextColumn();
        newDesigns = drawUiButton(uiTextForLocale("Shuffle designs", "Neue Varianten"), UiButtonRole::Secondary,
                                  ImVec2(-1.0f, 0.0f));
        ImGui::TableNextColumn();
        if (drawUiButton(uiTextForLocale("View", "Ansicht"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("SimpleDesignView");
        // Keep the popup in the same table/cell ID scope as its trigger.
        if (ImGui::BeginPopup("SimpleDesignView"))
        {
            FormUi::Checkbox(uiTextForLocale("Labels", "Beschriftungen"), &_designPreviewShowLabels);
            if (map.creationIntent == MapCreationIntent::WorldOverview)
                FormUi::Checkbox(uiTextForLocale("Political borders", "Politische Grenzen"),
                                 &_designPreviewShowPoliticalBorders);
            if (ImGui::MenuItem(uiTextForLocale("Fit map", "Karte einpassen")))
            {
                _designPreviewZoom = 1.0f;
                _resetDesignPreviewScroll = true;
            }
            ImGui::EndPopup();
        }
        ImGui::EndTable();
    }
    const int index = _previewedGenerationCandidate;
    const auto &candidate = _controller._generation.candidates[static_cast<std::size_t>(index)];
    const bool eligible = _controller.generationCandidateEligible(candidate);
    if (_lastInteractivePreviewCandidate != index)
    {
        _lastInteractivePreviewCandidate = index;
        _designPreviewZoom = 1.0f;
        _resetDesignPreviewScroll = true;
    }
    if (!eligible)
        drawIssues(candidate.validation.blockers);
    const int footerColumns =
        std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x /
                                    (ImGui::CalcTextSize(uiTextForLocale("Change options", "Optionen ändern")).x +
                                     ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().CellPadding.x * 2.0f)),
                   1, 3);
    const float footerHeight =
        ImGui::GetFrameHeightWithSpacing() * static_cast<float>(1 + (3 + footerColumns - 1) / footerColumns) + 12.0f;
    drawGenerationInteractivePreview(
        candidate.map, _renderPlanCache,
        (_controller._generation.requestSerial << 8u) ^ static_cast<std::uint64_t>(index + 1), "SimpleSelectedDesign",
        std::max(180.0f, ImGui::GetContentRegionAvail().y - footerHeight), _designPreviewZoom, _designPreviewShowLabels,
        _designPreviewShowPoliticalBorders, _resetDesignPreviewScroll);
    _resetDesignPreviewScroll = false;
    if (drawUiButton(uiTextForLocale("Use this design", "Diesen Entwurf verwenden"), eligible,
                     "This design did not pass all required checks.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
    {
        _controller.applyGenerationCandidate(index);
        setWorkflowStep(3);
        _fitCanvasOnNextDraw = true;
        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
        return;
    }
    if (ImGui::BeginTable("SimpleDesignActions", footerColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        newDesigns |= drawUiButton(uiTextForLocale("Shuffle designs", "Neue Varianten"), UiButtonRole::Secondary,
                                   ImVec2(-1.0f, 0.0f));
        ImGui::TableNextColumn();
        if (drawUiButton(uiTextForLocale("Change options", "Optionen ändern"), UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f)))
            setWorkflowStep(0);
        ImGui::TableNextColumn();
        if (drawUiButton(uiTextForLocale("Details", "Details"), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup("SimpleDesignDetails");
        setResponsiveAutoSizeWindow(520.0f, 300.0f);
        if (ImGui::BeginPopup("SimpleDesignDetails"))
        {
            ImGui::TextWrapped("%s", generationCompositionLabel(candidate.map).c_str());
            for (const auto *issue : generationPreviewReviewIssues(candidate))
                if (issue)
                    ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(*issue)).c_str());
            ImGui::Text("%s: %u", uiTextForLocale("Seed", "Zufallswert"), candidate.map.generation.recipe.seed);
            ImGui::EndPopup();
        }
        ImGui::EndTable();
    }
    if (newDesigns)
    {
        const auto oldSeed = _controller._generation.generatorSeed;
        _controller._generation.generatorSeed = std::random_device{}();
        if (_controller._generation.generatorSeed == oldSeed)
            ++_controller._generation.generatorSeed;
        // Explicit shuffle overrides seed pinning, preserving all other options.
        generate();
    }
}

void MapCreatorView::drawArtStyleControl(bool generationBusy, bool compact)
{
    MapModel &map = _controller._draftMap;

    const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
    const bool localArtwork = map.generation.recipe.generatorVersion >= 12 &&
        (map.atlasLevel == MapLevel::Tactical || map.atlasLevel == MapLevel::Site);
    const bool supportedArtwork = overview || localArtwork;
    if (!supportedArtwork && !compact)
        return;
    const bool dirty = _controller.hasUnsavedDraft();
    const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const char *saveStatus = dirty ? uiTextForLocale("Unsaved changes", "Nicht gespeichert")
                             : _controller._persistence.editingMapIndex >= 0 ? uiTextForLocale("Map saved", "Gespeichert")
                                                                 : uiTextForLocale("New map", "Neue Karte");
    if (supportedArtwork)
    {
        auto &recipe = map.generation.recipe;
        const bool currentArtwork = mapCreatorArtStyleMatches(recipe, MapCreatorArtStyle::Illustrated);
        ImGui::BeginDisabled(generationBusy || _imageTraceActive);
        if (currentArtwork)
            ImGui::TextUnformatted(overview ? uiTextForLocale("Illustrated atlas", "Illustrierter Atlas")
                                           : uiTextForLocale("Illustrated map", "Illustrierte Karte"));
        else if (drawUiButton(uiTextForLocale("Use current artwork", "Aktuelle Darstellung verwenden"),
                              UiButtonRole::Secondary, ImVec2(compact ? 0.0f : -1.0f, 0.0f)))
        {
            finishGeneratedFeatureGeometryEdit();
            _controller.updateDraftArtwork(uiTextForLocale("Change map art style", "Kartenstil ändern"));
        }
        showHelpTooltip(uiTextForLocale(
            "Updates the artwork while keeping your map layout. Undo restores the previous appearance; save to keep the change.",
            "Aktualisiert die Darstellung und behält den Kartenaufbau. Rückgängig stellt die vorherige Darstellung wieder her; Speichern übernimmt die Änderung."));
        if(currentArtwork && recipe.recipeVersion<kCurrentMapIllustrationRecipe && drawUiButton(
            uiTextForLocale("Update map artwork", "Kartendarstellung aktualisieren"),UiButtonRole::Secondary)) {
            finishGeneratedFeatureGeometryEdit();
            _controller.updateDraftArtwork(uiTextForLocale("Update map artwork", "Kartendarstellung aktualisieren"));
        }
        ImGui::EndDisabled();
        if (!compact)
            ImGui::TextWrapped(
                "%s",
                uiTextForLocale(
                    "Changes this draft's artwork. Save to keep it; Undo restores your previous choice.",
                    "Ändert die Darstellung dieses Entwurfs. Speichern übernimmt sie; Rückgängig stellt die vorherige Wahl wieder her."));
    }
    if (compact)
    {
        if (supportedArtwork &&
            ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(saveStatus).x <= rowRight)
            ImGui::SameLine();
        ImGui::TextColored(dirty ? ImVec4(0.98f, 0.74f, 0.34f, 1.0f) : ImVec4(0.45f, 0.78f, 0.66f, 1.0f), "%s",
                           saveStatus);
    }
}

void MapCreatorView::applyDraftDimensions()
{
    MapModel &map = _controller._draftMap;

    _draftWidth = std::clamp(_draftWidth, 4, 120);
    _draftHeight = std::clamp(_draftHeight, 4, 120);
    if (map.width == _draftWidth && map.height == _draftHeight)
        return;
    _controller.captureUndoSnapshot("Resize Map");
    map.width = _draftWidth;
    map.height = _draftHeight;
    _controller.resizeDraftCells();
    _fitCanvasOnNextDraw = true;
}

void MapCreatorView::useSelectedWorldIdea()
{

    const char *briefs[] = {
        "Three nations named Alderreach, Veyra, and Kharad share a great river. There is a large capital named Crosswater.",
        "Two nations named Arken and Liora. A vast forest shapes their borderlands. There is a capital named Greenwatch and cities named Harrowfield and Oldmere.",
        "Three nations named Skeld, Maravia, and Orun lie around a shattered coast. There is a large capital named Havenfall and cities named Stormport and Highpass."};
    _controller._generation.worldBrief = localizedUiLabel(briefs[std::clamp(_controller._generation.worldIdeaIndex, 0, 2)]);
    _controller.interpretWorldBrief();
    _controller._generation.worldEntryMode = 0;
}

void MapCreatorView::prepareWorldBriefForGeneration()
{
    MapModel &map = _controller._draftMap;

    if (map.creationIntent != MapCreationIntent::WorldOverview)
        return;
    if (_controller._generation.worldEntryMode == 1)
        useSelectedWorldIdea();
    else if (!_controller._generation.worldBriefParsed)
        _controller.interpretWorldBrief();
}
