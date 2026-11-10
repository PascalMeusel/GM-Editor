#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawDesignCandidates(bool stackedLayout)
{
    MapModel &map = _controller._draftMap;

    const int candidateCount = static_cast<int>(_controller._generation.candidates.size());
    _previewedGenerationCandidate = std::clamp(_previewedGenerationCandidate, 0, candidateCount - 1);
    int chosenCandidate = -1;
    int candidateFooterRows = 1;
    if (stackedLayout)
    {
        const ImGuiStyle &style = ImGui::GetStyle();
        const std::string reviewLabel = localizedUiLabel("Review facts");
        const std::string useLabel = localizedUiLabel("Use This Design");
        const int actionColumns = gm::mapui::mapCreatorPairedActionColumnCount(
            ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(reviewLabel.c_str()).x,
            ImGui::CalcTextSize(useLabel.c_str()).x, style.FramePadding.x, style.CellPadding.x);
        candidateFooterRows = actionColumns == 1 ? 2 : 1;
    }
    const float candidateActionHeight = ImGui::GetFrameHeightWithSpacing() * static_cast<float>(candidateFooterRows) +
                                        ImGui::GetStyle().ItemSpacing.y * 2.0f;
    if (_resetWorkflowScroll)
        ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("GeneratedDesignSelection", ImVec2(0.0f, -candidateActionHeight), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::SeparatorText(localizedUiLabel("Choose a design").c_str());
    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "Choose by overall shape, not by tiny details. You can change terrain, roads, names, and places afterward.")
            .c_str());
    if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
    {
        drawInlineLabelValue("Map type", localizedUiLabel(mapCreationIntentLabel(map.creationIntent)).c_str());
        drawInlineLabelValue("Map size", localizedUiLabel(sizePresetLabel(map.sizePreset)).c_str());
        ImGui::TreePop();
    }
    ImGui::TextDisabled("%s", localizedUiLabel("Map symbols").c_str());
    drawGenerationPreviewLegend();
    drawDisabledWrappedText(
        "The small previews hide labels so you can compare the overall shape. Select one to inspect its names and details below.");

    // All alternatives are real production-render previews. A text-only
    // selector made fundamentally different compositions impossible to
    // compare and hid rendering problems until after applying a design.
    const int selectorColumns = gm::mapui::mapCreatorCandidateColumnCount(
        ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing(), candidateCount, stackedLayout);
    if (ImGui::BeginTable("GeneratedDesignSelector", selectorColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings |
                              ImGuiTableFlags_PadOuterX))
    {
        for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
        {
            ImGui::TableNextColumn();
            ImGui::PushID(candidateIndex);
            const MapGenerationResult &candidate =
                _controller._generation.candidates[static_cast<std::size_t>(candidateIndex)];
            const bool selected = candidateIndex == _previewedGenerationCandidate;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, selected ? IM_COL32(47, 66, 66, 245) : IM_COL32(34, 37, 35, 230));
            ImGui::PushStyleColor(ImGuiCol_Border, selected ? IM_COL32(51, 157, 170, 255) : IM_COL32(82, 87, 82, 190));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, selected ? 2.0f : 1.0f);
            const float thumbnailHeight =
                std::clamp((ImGui::GetContentRegionAvail().x - 24.0f) * static_cast<float>(candidate.map.height) /
                               std::max(1, candidate.map.width),
                           160.0f, 380.0f);
            const float cardHeight = thumbnailHeight + ImGui::GetTextLineHeightWithSpacing() * 5.0f +
                                     ImGui::GetStyle().WindowPadding.y * 2.0f;
            ImGui::BeginChild("GenerationCandidateCard", ImVec2(0.0f, cardHeight), true,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const std::string cardTitle = formatLocalizedUi("Design %d", candidateIndex + 1);
            const std::string recommendedLabel = localizedUiLabel("Recommended");
            const bool recommendationFitsInline = ImGui::CalcTextSize(cardTitle.c_str()).x +
                                                      ImGui::CalcTextSize(recommendedLabel.c_str()).x +
                                                      ImGui::GetStyle().ItemSpacing.x <=
                                                  ImGui::GetContentRegionAvail().x;
            ImGui::TextUnformatted(cardTitle.c_str());
            if (candidateIndex == _controller._generation.recommendedGenerationCandidate)
            {
                if (recommendationFitsInline)
                    ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s", recommendedLabel.c_str());
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", generationCompositionLabel(candidate.map).c_str());
            ImGui::PopStyleColor();
            const std::uint64_t previewContentVersion =
                (_controller._generation.requestSerial << 8u) ^ static_cast<std::uint64_t>(candidateIndex + 1);
            if (drawGenerationMiniMap(candidate.map, _renderPlanCache, previewContentVersion,
                                      "GenerationCandidateThumbnail", thumbnailHeight, -1.0f, false, false))
                _previewedGenerationCandidate = candidateIndex;
            const int reviewNoteCount = static_cast<int>(generationPreviewReviewIssues(candidate).size());
            if (reviewNoteCount == 0)
            {
                ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s", localizedUiLabel("Ready to edit").c_str());
            }
            else
            {
                ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.34f, 1.0f),
                                   localizedUiLabel("Ready · %d review notes").c_str(), reviewNoteCount);
            }
            ImGui::TextDisabled(
                "%s", _controller.generationCandidateEligible(candidate)
                          ? uiTextForLocale("Required checks passed", "Erforderliche Prüfungen bestanden")
                          : uiTextForLocale("Requirements need review", "Anforderungen müssen geprüft werden"));
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemClicked())
                _previewedGenerationCandidate = candidateIndex;
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    const int candidateIndex = _previewedGenerationCandidate;
    const MapGenerationResult &candidate = _controller._generation.candidates[static_cast<std::size_t>(candidateIndex)];
    ImGui::Spacing();
    ImGui::SeparatorText(localizedUiLabel("Large map preview").c_str());
    const std::string selectedDesignTitle =
        formatLocalizedUi("Design %d", candidateIndex + 1) + " · " + generationCompositionLabel(candidate.map);
    ImGui::TextWrapped("%s", selectedDesignTitle.c_str());
    if (candidateIndex == _controller._generation.recommendedGenerationCandidate)
        ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s", localizedUiLabel("Recommended").c_str());
    const std::vector<const MapGenerationIssue *> reviewIssues = generationPreviewReviewIssues(candidate);
    const int reviewNoteCount = static_cast<int>(reviewIssues.size());
    ImGui::TextColored(
        reviewNoteCount == 0 ? ImVec4(0.35f, 0.78f, 0.66f, 1.0f) : ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s",
        localizedUiLabel(reviewNoteCount == 0 ? "Ready to edit" : "Ready to edit with review notes").c_str());

    if (_lastInteractivePreviewCandidate != candidateIndex)
    {
        _lastInteractivePreviewCandidate = candidateIndex;
        _designPreviewZoom = 1.0f;
        _resetDesignPreviewScroll = true;
    }

    const float toolbarWidth = ImGui::GetContentRegionAvail().x;
    const int toolbarColumns = toolbarWidth >= 880.0f ? 4 : (toolbarWidth >= 430.0f ? 2 : 1);
    if (ImGui::BeginTable("DesignPreviewToolbar", toolbarColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        if (drawUiButton("Fit map", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                         "Fit the complete design inside the preview."))
        {
            _designPreviewZoom = 1.0f;
            _resetDesignPreviewScroll = true;
        }

        ImGui::TableNextColumn();
        const bool canZoomOut = _designPreviewZoom > 1.001f;
        ImGui::BeginDisabled(!canZoomOut);
        const bool zoomOut = drawSmallUiButton("-", UiButtonRole::Secondary, "Zoom out");
        ImGui::EndDisabled();
        if (zoomOut && canZoomOut)
        {
            _designPreviewZoom = gm::mapui::mapCreatorPreviewZoomAfterStep(_designPreviewZoom, -1);
            _resetDesignPreviewScroll = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled(localizedUiLabel("Zoom: %d%%").c_str(),
                            static_cast<int>(std::lround(_designPreviewZoom * 100.0f)));
        ImGui::SameLine();
        const bool canZoomIn = _designPreviewZoom < 3.999f;
        ImGui::BeginDisabled(!canZoomIn);
        const bool zoomIn = drawSmallUiButton("+", UiButtonRole::Secondary, "Zoom in");
        ImGui::EndDisabled();
        if (zoomIn && canZoomIn)
        {
            _designPreviewZoom = gm::mapui::mapCreatorPreviewZoomAfterStep(_designPreviewZoom, 1);
            _resetDesignPreviewScroll = true;
        }

        ImGui::TableNextColumn();
        FormUi::Checkbox((localizedUiLabel("Labels") + "##DesignPreviewLabels").c_str(), &_designPreviewShowLabels);
        showHelpTooltip("Show or hide place and region names in this preview only.");

        ImGui::TableNextColumn();
        FormUi::Checkbox((localizedUiLabel("Political borders") + "##DesignPreviewBorders").c_str(),
                         &_designPreviewShowPoliticalBorders);
        showHelpTooltip("Show or hide nation borders in this preview only.");
        ImGui::EndTable();
    }
    drawDisabledWrappedText(
        "This is the map that opens in Edit. Drag to move it, then use the mouse wheel or zoom buttons to inspect labels and details.");

    const float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y - 136.0f, 360.0f, 680.0f);
    const std::uint64_t previewContentVersion =
        (_controller._generation.requestSerial << 8u) ^ static_cast<std::uint64_t>(candidateIndex + 1);
    const float fittedPreviewWidth = drawGenerationInteractivePreview(
        candidate.map, _renderPlanCache, previewContentVersion, "SelectedDesignPreview", previewHeight,
        _designPreviewZoom, _designPreviewShowLabels, _designPreviewShowPoliticalBorders, _resetDesignPreviewScroll);
    _resetDesignPreviewScroll = false;

    int visibleLabelCount = 0;
    for (const MapGeneratedFeature &feature : candidate.map.generation.graph.features)
        visibleLabelCount += !feature.name.empty();
    visibleLabelCount +=
        static_cast<int>(std::count_if(candidate.map.regions.begin(), candidate.map.regions.end(),
                                       [](const MapRegionModel &region) { return !region.name.empty(); }));
    visibleLabelCount +=
        static_cast<int>(std::count_if(candidate.map.routes.begin(), candidate.map.routes.end(),
                                       [](const MapRouteModel &route) { return !route.name.empty(); }));
    if (_designPreviewShowLabels &&
        gm::mapui::mapCreatorPreviewLabelsNeedZoom(visibleLabelCount, fittedPreviewWidth, _designPreviewZoom))
    {
        ImGui::TextColored(
            ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s",
            localizedUiLabel(
                "Labels are dense at this overview. Zoom in to read names, or hide labels while comparing the composition.")
                .c_str());
    }

    if (reviewNoteCount > 0 &&
        ImGui::TreeNodeEx(localizedUiLabel("Review notes").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto drawIssue = [](const MapGenerationIssue &issue) {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(issue)).c_str());
            const char *action = generationIssueAction(issue);
            if (action && action[0] != '\0')
            {
                ImGui::Indent();
                ImGui::TextDisabled("%s", localizedUiLabel(action).c_str());
                ImGui::Unindent();
            }
        };
        for (const MapGenerationIssue *issue : reviewIssues)
            if (issue)
                drawIssue(*issue);
        ImGui::TreePop();
    }
    // Counts and numeric scoring are useful for diagnosis, but they are
    // not the decision the GM needs to make. Keep the normal flow about
    // the visible composition and expose metrics on demand.
    if (ImGui::TreeNode(uiTextForLocale("Advanced quality details", "Erweiterte Qualitätsdetails")))
    {
        ImGui::Text("%s: %d/100", localizedUiLabel("Quality").c_str(), candidate.quality.score);
        const GenerationPreviewSummary summary = generationPreviewSummary(candidate.map);
        const int summaryColumns = ImGui::GetContentRegionAvail().x >= 470.0f ? 4 : 2;
        if (ImGui::BeginTable("SelectedDesignSummary", summaryColumns,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const std::array<std::pair<const char *, int>, 4> values = {{{"Roads", summary.roads},
                                                                         {"Rivers", summary.rivers},
                                                                         {"Settlements", summary.settlements},
                                                                         {"Landmarks", summary.landmarks}}};
            for (const auto &[label, value] : values)
            {
                ImGui::TableNextColumn();
                ImGui::Text("%s: %d", localizedUiLabel(label).c_str(), value);
            }
            ImGui::EndTable();
        }
        const auto strongest = generationPreviewStrength(candidate.quality);
        ImGui::Text("%s: %s %d%%", localizedUiLabel("Strength").c_str(), localizedUiLabel(strongest.first).c_str(),
                    strongest.second);
        ImGui::TreePop();
    }
    const int adjustedFactCount =
        static_cast<int>(std::count_if(candidate.validation.facts.begin(), candidate.validation.facts.end(),
                                       [](const MapGenerationFactValidation &fact) {
                                           return fact.fulfillment == MapGenerationFactFulfillment::Adjusted;
                                       }));
    if (adjustedFactCount > 0 && ImGui::TreeNode(localizedUiLabel("Adjusted preferences").c_str()))
    {
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel("The map remains valid, but these preferences were adjusted during repair.").c_str());
        for (const MapGenerationFactValidation &validation : candidate.validation.facts)
        {
            if (validation.fulfillment != MapGenerationFactFulfillment::Adjusted)
                continue;
            const auto fact =
                std::find_if(candidate.map.generation.recipe.facts.begin(), candidate.map.generation.recipe.facts.end(),
                             [&](const MapGenerationFact &entry) { return entry.id == validation.factId; });
            const std::string label = fact != candidate.map.generation.recipe.facts.end() && !fact->label.empty()
                                          ? fact->label
                                          : localizedUiLabel("Preferred fact");
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", label.c_str());
        }
        drawDisabledWrappedText("Review the adjustment or change the preferred fact to required.");
        ImGui::TreePop();
    }
    ImGui::EndChild();
    ImGui::Separator();
    const bool candidateAccepted = _controller.generationCandidateEligible(candidate);
    if (stackedLayout)
    {
        const UiButtonPairResult designActions = drawResponsiveButtonPair(
            "GeneratedDesignFooterActions", "Review facts", UiButtonRole::Secondary,
            "Return to the facts without changing your current map.", "Use This Design",
            candidateAccepted ? UiButtonRole::Primary : UiButtonRole::Secondary,
            "Apply this design and continue in Edit. Terrain, roads, names, and places remain editable.",
            candidateAccepted, "This design did not pass all required checks.");
        if (designActions.first)
            setWorkflowStep(1);
        if (designActions.second)
            chosenCandidate = candidateIndex;
    }
    else if (drawUiButton("Use This Design", candidateAccepted, "This design did not pass all required checks.",
                          candidateAccepted ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                          "Apply this design and continue in Edit. Terrain, roads, names, and places remain editable."))
    {
        chosenCandidate = candidateIndex;
    }
    if (chosenCandidate >= 0)
    {
        _controller.applyGenerationCandidate(chosenCandidate);
        setWorkflowStep(3);
        _fitCanvasOnNextDraw = true;
        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
    }
}

void MapCreatorView::drawGenerationConflicts()
{
    MapModel &map = _controller._draftMap;

    ImGui::BeginChild("GenerationConflictPanel", ImGui::GetContentRegionAvail(), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::SeparatorText(localizedUiLabel("No valid design yet").c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.45f, 0.34f, 1.0f));
    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "None of the generated maps satisfied every required fact and quality check. Your current map was not changed.")
            .c_str());
    ImGui::PopStyleColor();
    if (_controller._generation.rejectedGenerationCandidateCount > 0)
        ImGui::TextDisabled(localizedUiLabel("%d invalid alternatives were rejected before preview.").c_str(),
                            _controller._generation.rejectedGenerationCandidateCount);

    bool showedIssue = false;
    for (const MapGenerationIssue &issue : _controller._generation.lastValidation.blockers)
    {
        showedIssue = true;
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.93f, 0.45f, 0.34f, 1.0f), "%s", localizedUiLabel("Required check failed").c_str());
        ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(issue)).c_str());
        const char *action = generationIssueAction(issue);
        if (action && action[0] != '\0')
            drawDisabledWrappedValue(localizedUiLabel("Suggested fix") + ": " + localizedUiLabel(action));
    }
    for (const MapGenerationIssue &issue : _controller._generation.lastQuality.issues)
    {
        if (issue.severity != MapGenerationIssueSeverity::Critical)
            continue;
        showedIssue = true;
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.93f, 0.45f, 0.34f, 1.0f), "%s", localizedUiLabel("Required check failed").c_str());
        ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(issue)).c_str());
        const char *action = generationIssueAction(issue);
        if (action && action[0] != '\0')
            drawDisabledWrappedValue(localizedUiLabel("Suggested fix") + ": " + localizedUiLabel(action));
    }
    if (!showedIssue)
        ImGui::TextWrapped(
            "%s",
            uiTextForLocale(
                "No design passed every required check. Review the requirements or try new designs.",
                "Kein Entwurf hat alle erforderlichen Prüfungen bestanden. Prüfe die Anforderungen oder erstelle neue Entwürfe."));

    ImGui::Spacing();
    const UiButtonPairResult conflictActions = drawResponsiveButtonPair(
        "GenerationConflictActions", "Review facts", UiButtonRole::Secondary,
        "Return to the facts without changing your current map.", "Try new designs", UiButtonRole::Primary,
        "Run the deterministic design search again with the current facts.");
    if (conflictActions.first)
        setWorkflowStep(1);
    if (conflictActions.second)
        _controller.queueDraftGeneration();

    const bool canGrow = map.sizePreset != MapSizePreset::Large;
    if (drawUiButton("Use a larger map and try again", canGrow, "The largest map preset is already selected.",
                     UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Increase the preset while keeping every fact, then create new designs."))
    {
        _controller._generation.selectedSizePresetIndex =
            static_cast<int>(map.sizePreset == MapSizePreset::Small ? MapSizePreset::Standard : MapSizePreset::Large);
        _controller.syncCreationIntentFromIndex(true);
        _draftWidth = map.width;
        _draftHeight = map.height;
        _controller.queueDraftGeneration();
    }
    ImGui::EndChild();
}
