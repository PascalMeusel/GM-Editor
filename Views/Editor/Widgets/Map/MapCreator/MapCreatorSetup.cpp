#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawSetupPreview(ImVec2 size)
{
    // A cheap single draft uses the same generator and renderer as accepted
    // designs. It is an approximation, not a candidate with passed checks.
    const auto &draft = _controller._draftMap;
    std::string key = draft.metadata.id + ":" + std::to_string(static_cast<int>(draft.creationIntent)) + ":" +
                      std::to_string(static_cast<int>(draft.generation.recipe.landform));
    for (const auto value :
         {draft.width, draft.height, _controller._generation.generatorWaterLevel, _controller._generation.generatorForestDensity,
          _controller._generation.generatorMountainDensity, _controller._generation.generatorSettlementDensity,
          _controller._generation.generatorRequestedSettlements, _controller._generation.generatorRequestedLandmarks,
          _controller._generation.generatorRouteDensity, _controller._generation.generatorTacticalDensity,
          _controller._generation.generatorLandscapeIndex, _controller._generation.generatorPurposeIndex, _controller._generation.travelPurposeIndex,
          _controller._generation.travelBiomeIndex, _controller._generation.travelDangerLevel,
          static_cast<int>(_controller._generation.travelCombatReady)})
        key += ":" + std::to_string(value);
    key += ":" + std::to_string(_controller._generation.generatorSeed) + ":" + draft.generation.recipe.styleId;
    for (const auto &entity : _controller._generation.setupEntities)
        key += ":" + entity.id + ":" + entity.name + ":" + entity.nationId;
    if (key != _setupPreviewObservedKey)
    {
        _setupPreviewObservedKey = key;
        _setupPreviewChangedAt = ImGui::GetTime();
        if (_setupPreviewCancel)
            _setupPreviewCancel->store(true);
    }
    if (_setupPreviewJob.valid() && _setupPreviewJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            auto map = _setupPreviewJob.get();
            if (!_discardSetupPreviewResult && _setupPreviewPendingKey == key)
            {
                _setupPreviewMap = std::move(map);
                _setupPreviewReadyKey = key;
                ++_setupPreviewRevision;
            }
        }
        catch (...)
        {
            if (!_discardSetupPreviewResult && _setupPreviewPendingKey == key)
                _setupPreviewFailedKey = key;
        }
    }
    if (!_setupPreviewJob.valid() && _setupPreviewReadyKey != key && _setupPreviewFailedKey != key &&
        ImGui::GetTime() - _setupPreviewChangedAt >= .25)
    {
        _discardSetupPreviewResult = false;
        _controller.synchronizeStructuredSetup();
        auto options = _controller.buildGenerationOptions();
        options.qualityAttempts = 1;
        options.progressCallback = {};
        _setupPreviewCancel = std::make_shared<std::atomic_bool>(false);
        options.cancellationRequested = [cancel = _setupPreviewCancel] { return cancel->load(); };
        _setupPreviewPendingKey = key;
        const auto travelRecipe =
            buildStandaloneTravelRecipe(options, _controller._generation.travelPurposeIndex, _controller._generation.travelBiomeIndex,
                                        _controller._generation.travelDangerLevel, _controller._generation.travelCombatReady);
        _setupPreviewJob = std::async(std::launch::async, [options, travelRecipe] {
            if (options.creationIntent == MapCreationIntent::TravelScene)
            {
                auto batch = generateStandaloneTravelMapBatch(options, travelRecipe, 1);
                if (batch.candidates.empty())
                    throw std::runtime_error("No travel preview available");
                return std::move(batch.candidates.front().map);
            }
            if (options.creationIntent == MapCreationIntent::TacticalEncounter)
            {
                auto batch = TacticalMapGenerator::generateBatch(options, nullptr, 1);
                if (batch.candidates.empty())
                    throw std::runtime_error("No tactical preview available");
                return std::move(batch.candidates.front().map);
            }
            return mapGenGenerateCandidate(options).map;
        });
    }
    if (_setupPreviewMap && _setupPreviewReadyKey == key)
        drawGenerationMiniMap(*_setupPreviewMap, _renderPlanCache, _setupPreviewRevision, "##LiveSetupApproximation",
                              size.y, size.x, false, false);
    else
    {
        const auto origin = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                                                  IM_COL32(30, 34, 31, 255), 6);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(origin.x + 16, origin.y + 16), IM_COL32(195, 201, 194, 255),
            _setupPreviewFailedKey == key
                ? uiTextForLocale("Preview unavailable. You can still create designs.",
                                  "Vorschau nicht verfügbar. Du kannst weiterhin Entwürfe erstellen.")
                : uiTextForLocale("Updating preview…", "Vorschau wird aktualisiert…"));
        ImGui::Dummy(size);
    }
}

void MapCreatorView::drawFactReviewCards(const MapGenerationValidationReport &requestValidation)
{
    ImGui::SeparatorText(localizedUiLabel("Interpreted facts").c_str());
    drawDisabledWrappedText(
        "Every recognized statement stays visible here. Choose whether it is required, preferred, or left for the generator to fill automatically.");

    if (_controller._generation.briefFacts.empty())
    {
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "No facts were recognized. Your original text remains attached as a note and the generator may fill the world automatically.")
                .c_str());
    }
    else
    {
        const std::string localizedStrengthItems = localizedComboItems(factStrengthItems);
        for (std::size_t factIndex = 0; factIndex < _controller._generation.briefFacts.size(); ++factIndex)
        {
            // Copy the current presentation state because an ImGui interaction
            // may update the controller-owned vector before this frame ends.
            const MapGenerationFact fact = _controller._generation.briefFacts[factIndex];
            const MapGenerationFactValidation *factValidation =
                MapFactsPresentation::validationForFact(requestValidation, fact);
            ImGui::PushID(static_cast<int>(factIndex));
            ImGui::Separator();

            const bool needsReview = MapFactsPresentation::needsReview(fact);
            const ImVec4 stateColor = fact.type == MapGenerationFactType::Note ? ImVec4(0.93f, 0.48f, 0.36f, 1.0f)
                                      : needsReview                            ? ImVec4(0.92f, 0.72f, 0.28f, 1.0f)
                                                                               : ImVec4(0.35f, 0.78f, 0.66f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
            ImGui::TextUnformatted(localizedUiLabel(MapFactsPresentation::typeLabel(fact.type)).c_str());
            ImGui::PopStyleColor();

            const std::string rawValue = MapFactsPresentation::displayValue(fact);
            const std::string displayValue =
                MapFactsPresentation::displayValueIsUiLiteral(fact) ? localizedUiLabel(rawValue.c_str()) : rawValue;
            ImGui::TextWrapped("%s", displayValue.c_str());

            const std::string recognition = formatLocalizedUi(
                "Recognition: %s (%d%%)", localizedUiLabel(MapFactsPresentation::recognitionLabel(fact)).c_str(),
                MapFactsPresentation::confidencePercent(fact));
            ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
            ImGui::TextWrapped("%s", recognition.c_str());
            ImGui::PopStyleColor();

            const char *confirmationLabel =
                fact.type == MapGenerationFactType::Note ? "I reviewed this note" : "Interpretation confirmed";
            const float confirmationWidth = ImGui::CalcTextSize(localizedUiLabel(confirmationLabel).c_str()).x +
                                            ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
            const float twoColumnMinimum =
                std::max(230.0f, confirmationWidth) * 2.0f + ImGui::GetStyle().CellPadding.x * 4.0f;
            const int controlColumns = ImGui::GetContentRegionAvail().x >= twoColumnMinimum ? 2 : 1;
            if (ImGui::BeginTable("FactReviewControls", controlColumns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableNextColumn();
                int strengthIndex = static_cast<int>(fact.strength);
                ImGui::SetNextItemWidth(-1.0f);
                if (FormUi::Combo(localizedUiLabel("Fact priority").c_str(), &strengthIndex,
                                  localizedStrengthItems.c_str()))
                {
                    _controller.setBriefFactStrength(
                        factIndex, static_cast<MapGenerationFactStrength>(std::clamp(strengthIndex, 0, 2)));
                }

                ImGui::TableNextColumn();
                bool confirmed = fact.confirmed;
                if (FormUi::Checkbox(localizedUiLabel(confirmationLabel).c_str(), &confirmed))
                    _controller.setBriefFactConfirmed(factIndex, confirmed);
                if (fact.type == MapGenerationFactType::Note)
                    showHelpTooltip(
                        "Reviewing a note keeps it visible, but does not turn unsupported prose into an executable map fact.");
                ImGui::EndTable();
            }

            ImGui::TextDisabled("%s", localizedUiLabel("From your note").c_str());
            ImGui::TextWrapped("%s", fact.sourceText.empty() ? localizedUiText("No source text available.")
                                                             : fact.sourceText.c_str());

            if (fact.type == MapGenerationFactType::Note)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, stateColor);
                ImGui::TextWrapped(
                    "%s",
                    localizedUiLabel(
                        "This text could not be converted into a reliable map rule. Keep it as context or clarify the original note.")
                        .c_str());
                ImGui::PopStyleColor();
                if (fact.strength != MapGenerationFactStrength::Automatic &&
                    drawUiButton("Keep as optional note", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                                 "Keep this text as context without requiring the generator to execute it."))
                {
                    _controller.setBriefFactStrength(factIndex, MapGenerationFactStrength::Automatic);
                    _controller.setBriefFactConfirmed(factIndex, true);
                }
            }

            if (factValidation && !factValidation->issueCode.empty())
            {
                const MapGenerationIssue issue{factValidation->issueCode,
                                               factValidation->fulfillment == MapGenerationFactFulfillment::Unsatisfied
                                                   ? MapGenerationIssueSeverity::Critical
                                                   : MapGenerationIssueSeverity::Warning,
                                               {},
                                               {}};
                const ImVec4 issueColor = issue.severity == MapGenerationIssueSeverity::Critical
                                              ? ImVec4(0.93f, 0.45f, 0.34f, 1.0f)
                                              : ImVec4(0.92f, 0.72f, 0.28f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, issueColor);
                ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(issue)).c_str());
                ImGui::PopStyleColor();
                ImGui::TextDisabled("%s", localizedUiLabel("Suggested action").c_str());
                ImGui::TextWrapped("%s", localizedUiLabel(generationIssueAction(issue)).c_str());
            }
            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText(localizedUiLabel("Generation check").c_str());
    if (requestValidation.eligible)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.78f, 0.66f, 1.0f));
        ImGui::TextUnformatted(localizedUiLabel("Ready to generate").c_str());
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", localizedUiLabel("All required facts are internally consistent.").c_str());
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.45f, 0.34f, 1.0f));
    ImGui::TextUnformatted(localizedUiLabel("Resolve before generating").c_str());
    ImGui::PopStyleColor();
    ImGui::TextWrapped("%s",
                       localizedUiLabel("The generator cannot start until these required facts are resolved.").c_str());
    for (const MapGenerationIssue &issue : requestValidation.blockers)
    {
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", localizedUiLabel(generationIssueMessage(issue)).c_str());
        ImGui::Indent();
        ImGui::TextDisabled("%s", localizedUiLabel("Suggested action").c_str());
        ImGui::TextWrapped("%s", localizedUiLabel(generationIssueAction(issue)).c_str());
        ImGui::Unindent();
    }

    if (MapFactsPresentation::hasBlockingUnrecognizedNote(requestValidation) &&
        drawUiButton("Keep unrecognized notes as optional", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Keep unrecognized text as context without requiring the generator to execute it."))
    {
        _controller.makeUnrecognizedBriefNotesOptional();
    }
    if (drawUiButton("Edit original notes", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                     "Return to the Idea step and clarify the original text."))
    {
        setWorkflowStep(0);
    }
}
