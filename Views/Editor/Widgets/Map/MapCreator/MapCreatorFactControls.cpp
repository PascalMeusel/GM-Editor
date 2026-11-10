#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawFactsControls(bool generationBusy)
{
    MapModel &map = _controller._draftMap;

    ImGui::SeparatorText(localizedUiLabel("Review facts").c_str());
    bool namedFactsChanged = false;
    if (map.creationIntent == MapCreationIntent::WorldOverview && _controller._generation.worldBriefParsed)
    {
        const bool hasNamedFacts = !_controller._generation.briefNationNames.empty() || !_controller._generation.briefCapitalName.empty() ||
                                   !_controller._generation.briefCityNames.empty();
        ImGui::PushStyleColor(ImGuiCol_Text, hasNamedFacts ? ImVec4(0.35f, 0.78f, 0.66f, 1.0f)
                                                           : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(hasNamedFacts
                                 ? "Named facts are required. The generator may fill everything marked automatic."
                                 : "No named facts were found. You can still create a world and adjust it afterward.")
                .c_str());
        ImGui::PopStyleColor();
        int nationToRemove = -1;
        const std::string removeLabel = localizedUiLabel("Remove");
        const float removeButtonWidth = ImGui::CalcTextSize(removeLabel.c_str()).x +
                                        ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        for (int nationIndex = 0; nationIndex < static_cast<int>(_controller._generation.briefNationNames.size()); ++nationIndex)
        {
            ImGui::PushID(nationIndex);
            ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s", localizedUiLabel("Must").c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s %d", localizedUiLabel("Nation").c_str(), nationIndex + 1);
            const float rowWidth = ImGui::GetContentRegionAvail().x;
            const bool inlineRemove = rowWidth >= 120.0f + removeButtonWidth;
            ImGui::SetNextItemWidth(inlineRemove ? rowWidth - removeButtonWidth : -1.0f);
            namedFactsChanged |= FormUi::InputText(
                "##NationFact", &_controller._generation.briefNationNames[static_cast<std::size_t>(nationIndex)]);
            if (inlineRemove)
                ImGui::SameLine();
            const bool removePressed = inlineRemove
                                           ? ImGui::SmallButton(removeLabel.c_str())
                                           : drawUiButton("Remove", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f));
            if (removePressed)
                nationToRemove = nationIndex;
            ImGui::PopID();
        }
        if (nationToRemove >= 0)
        {
            _controller._generation.briefNationNames.erase(_controller._generation.briefNationNames.begin() + nationToRemove);
            namedFactsChanged = true;
        }
        if (drawUiButton("Add nation", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _controller._generation.briefNationNames.push_back(localizedUiLabel("New nation"));
            namedFactsChanged = true;
        }

        ImGui::TextColored(_controller._generation.briefCapitalName.empty() ? ImVec4(0.35f, 0.78f, 0.66f, 1.0f)
                                                                 : ImVec4(0.88f, 0.68f, 0.34f, 1.0f),
                           "%s",
                           localizedUiLabel(_controller._generation.briefCapitalName.empty() ? "Automatic" : "Must").c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", localizedUiLabel("Known capital").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        namedFactsChanged |= FormUi::InputText("##CapitalFact", &_controller._generation.briefCapitalName);

        int cityToRemove = -1;
        for (int cityIndex = 0; cityIndex < static_cast<int>(_controller._generation.briefCityNames.size()); ++cityIndex)
        {
            ImGui::PushID(1000 + cityIndex);
            ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s", localizedUiLabel("Must").c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s %d", localizedUiLabel("City").c_str(), cityIndex + 1);
            const float rowWidth = ImGui::GetContentRegionAvail().x;
            const bool inlineRemove = rowWidth >= 120.0f + removeButtonWidth;
            ImGui::SetNextItemWidth(inlineRemove ? rowWidth - removeButtonWidth : -1.0f);
            namedFactsChanged |=
                FormUi::InputText("##CityFact", &_controller._generation.briefCityNames[static_cast<std::size_t>(cityIndex)]);
            if (inlineRemove)
                ImGui::SameLine();
            const bool removePressed = inlineRemove
                                           ? ImGui::SmallButton(removeLabel.c_str())
                                           : drawUiButton("Remove", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f));
            if (removePressed)
                cityToRemove = cityIndex;
            ImGui::PopID();
        }
        if (cityToRemove >= 0)
        {
            _controller._generation.briefCityNames.erase(_controller._generation.briefCityNames.begin() + cityToRemove);
            namedFactsChanged = true;
        }
        if (drawUiButton("Add city", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _controller._generation.briefCityNames.push_back(localizedUiLabel("New city"));
            namedFactsChanged = true;
        }

        const bool requiresSharedRiver = std::any_of(_controller._generation.briefFacts.begin(), _controller._generation.briefFacts.end(),
                                                     [](const MapGenerationFact &fact) {
                                                         return fact.type == MapGenerationFactType::SharedRiver &&
                                                                fact.strength == MapGenerationFactStrength::Must;
                                                     });
        if (requiresSharedRiver)
        {
            ImGui::TextColored(ImVec4(0.88f, 0.68f, 0.34f, 1.0f), "%s", localizedUiLabel("Must").c_str());
            ImGui::SameLine();
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel("One continuous great river connects all named nations to a valid outlet.").c_str());
        }

        ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s", localizedUiLabel("Automatic").c_str());
        ImGui::SameLine();
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "Coasts, terrain, additional places, roads, and details are filled in around your required facts.")
                .c_str());
        ImGui::Spacing();
    }
    if (namedFactsChanged)
    {
        _controller.synchronizeBriefFactDrafts();
        _controller.invalidateGenerationRequest();
    }
    MapGenerationValidationReport requestValidation = _controller.validateDraftGenerationRequest();
    drawFactReviewCards(requestValidation);
    // Fact controls update immediately during this frame. Refresh the
    // inexpensive preflight before enabling the generation action.
    requestValidation = _controller.validateDraftGenerationRequest();
    if (map.scope == MapScope::Dungeon)
    {
        drawSectionTitle("1. Dungeon recipe");
        FormUi::Combo(localizedUiLabel("Map recipe").c_str(), &_controller._generation.selectedGeneratorIndex,
                      localizedComboItems(generatorItems).c_str());
    }
    else
    {
        ImGui::TextWrapped(
            localizedUiLabel("The generator keeps your facts and proposes a complete, editable map around them.")
                .c_str());
        FormUi::Combo(localizedUiLabel("Landscape profile").c_str(), &_controller._generation.generatorLandscapeIndex,
                      localizedComboItems(generationLandscapeItems).c_str());
        int cultureIndex = nameCultureIndex(map.generation.recipe.nameCulture);
        if (FormUi::Combo(localizedUiLabel("Name culture").c_str(), &cultureIndex,
                          localizedComboItems(nameCultureItems).c_str()))
        {
            _controller.captureUndoSnapshot("Change Name Culture");
            map.generation.recipe.nameCulture = nameCultureIds[std::clamp(cultureIndex, 0, 3)];
            map.revision += 1;
            map.metadata.touch();
            _controller.invalidateGenerationRequest();
        }
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel("Generated place and landmark names follow this culture. Your own names never change.")
                .c_str());
        if (ImGui::TreeNode(localizedUiLabel("Adjust what gets filled in").c_str()))
        {
            FormUi::SliderInt(localizedUiLabel("Settlements").c_str(), &_controller._generation.generatorRequestedSettlements, 0,
                              8);
            FormUi::SliderInt(localizedUiLabel("Landmarks").c_str(), &_controller._generation.generatorRequestedLandmarks, 0, 12);
            FormUi::SliderInt(localizedUiLabel("Map edge connections").c_str(), &_controller._generation.generatorEdgeConnections,
                              0, 4);
            ImGui::TreePop();
        }
    }

    if (ImGui::TreeNode(localizedUiLabel("Advanced terrain and route settings").c_str()))
    {
        FormUi::InputScalar(localizedUiLabel("Base seed").c_str(), ImGuiDataType_U32, &_controller._generation.generatorSeed);
        FormUi::Combo(localizedUiLabel("Quality").c_str(), &_controller._generation.generatorQualityIndex,
                      localizedComboItems(generationQualityItems).c_str());
        const int qualityIndex = std::clamp(_controller._generation.generatorQualityIndex, 0, 2);
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                           localizedUiText(generationQualityDescriptions[qualityIndex]));
        FormUi::SliderInt(localizedUiLabel("Water").c_str(), &_controller._generation.generatorWaterLevel, 0, 100);
        FormUi::SliderInt(localizedUiLabel("Mountains").c_str(), &_controller._generation.generatorMountainDensity, 0, 100);
        FormUi::SliderInt(localizedUiLabel("Forests").c_str(), &_controller._generation.generatorForestDensity, 0, 100);
        FormUi::SliderInt(localizedUiLabel("Routes").c_str(), &_controller._generation.generatorRouteDensity, 0, 100);
        FormUi::SliderInt(localizedUiLabel("Tactics").c_str(), &_controller._generation.generatorTacticalDensity, 0, 100);
        FormUi::Combo(localizedUiLabel("Mountain Shape").c_str(), &_controller._generation.generatorMountainPatternIndex,
                      localizedComboItems(mountainShapeItems).c_str());
        FormUi::Combo(localizedUiLabel("Road Network").c_str(), &_controller._generation.generatorRoadNetworkIndex,
                      localizedComboItems(roadNetworkItems).c_str());
        ImGui::TreePop();
    }
    if (!_controller._generation.candidates.empty() && !generationBusy)
    {
        ImGui::SeparatorText(localizedUiLabel("Choose a design").c_str());
        ImGui::TextWrapped(
            localizedUiLabel(
                "The three large previews are shown on the right. Choose the composition you like; everything remains editable.")
                .c_str());
    }

    if (_controller._generation.hasLastQuality && !generationBusy &&
        ImGui::TreeNode(uiTextForLocale("Advanced quality details", "Erweiterte Qualitätsdetails")))
    {
        const MapGenerationQualityReport &quality = _controller._generation.lastQuality;
        const std::string selected =
            std::to_string(quality.selectedCandidateIndex + 1) + "/" + std::to_string(quality.candidatesEvaluated);
        if (!_controller._generation.candidates.empty())
        {
            const std::string score = std::to_string(quality.score) + "/100";
            drawInlineLabelValue("Last quality", score.c_str());
        }
        else
        {
            drawInlineLabelValue("Last search", localizedUiText("No valid design"));
        }
        drawInlineLabelValue("Selected variation", selected.c_str());
        const std::string checks = localizedUiLabel("Roads") + " " + std::to_string(quality.roadEfficiencyPercent) +
                                   "% / " + localizedUiLabel("Rivers") + " " +
                                   std::to_string(quality.hydrologyPercent) + "% / " + localizedUiLabel("Composition") +
                                   " " + std::to_string(quality.compositionPercent) + "%";
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), checks.c_str());
        for (const MapGenerationIssue &issue : quality.issues)
        {
            const ImVec4 color = issue.severity == MapGenerationIssueSeverity::Critical
                                     ? ImVec4(0.93f, 0.45f, 0.34f, 1.0f)
                                     : ImVec4(0.92f, 0.72f, 0.28f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), localizedUiLabel(generationIssueMessage(issue)).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::TreePop();
    }

    auto finishGeneratedFeatureProjection = [&]() {
        if (!_hasPendingGeneratedFeatureProjection)
            return;
        finishGeneratedMapFeatureGeometryEdit(map, _pendingGeneratedFeatureOriginal);
        _controller.markDraftEdited();
        _pendingGeneratedFeatureProjectionId.clear();
        _hasPendingGeneratedFeatureProjection = false;
    };
    bool generatedFeatureProjectionItemActive = false;
    if (map.generation.continuousRendering &&
        ImGui::CollapsingHeader(localizedUiLabel("Generated feature controls").c_str()))
    {
        ImGui::TextWrapped(
            localizedUiLabel(
                "Lock important places before generating new designs. Width changes are shown immediately.")
                .c_str());
        int shown = 0;
        for (MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (shown >= 60)
                break;
            if (feature.kind != MapGeneratedFeatureKind::Settlement &&
                feature.kind != MapGeneratedFeatureKind::Landmark && feature.kind != MapGeneratedFeatureKind::Road &&
                feature.kind != MapGeneratedFeatureKind::Trail && feature.kind != MapGeneratedFeatureKind::River)
                continue;
            ImGui::PushID(feature.id.c_str());
            ImGui::Separator();
            const std::string featureLabel =
                feature.name.empty() ? localizedUiLabel("Generated feature") : localizedGeneratedFeatureName(feature);
            ImGui::TextUnformatted(featureLabel.c_str());
            if (feature.kind == MapGeneratedFeatureKind::Settlement ||
                feature.kind == MapGeneratedFeatureKind::Landmark)
            {
                bool locked = feature.locked;
                if (FormUi::Checkbox(localizedUiLabel("Keep position for next designs").c_str(), &locked))
                {
                    _controller.beginPropertyEditUndo("Lock Generated Feature");
                    feature.locked = locked;
                }
                ImGui::BeginDisabled(feature.locked);
                bool gmOnly = feature.gmOnly;
                if (FormUi::Checkbox(localizedUiLabel("GM Only").c_str(), &gmOnly))
                {
                    _controller.beginPropertyEditUndo("Edit Generated Feature");
                    feature.gmOnly = gmOnly;
                }
                bool discovered = feature.discovered;
                if (FormUi::Checkbox(localizedUiLabel("Discovered").c_str(), &discovered))
                {
                    _controller.beginPropertyEditUndo("Edit Generated Feature");
                    feature.discovered = discovered;
                }
                ImGui::EndDisabled();
            }
            else
            {
                ImGui::BeginDisabled(feature.locked);
                float width = feature.width;
                if (FormUi::SliderFloat(localizedUiLabel("Width").c_str(), &width, 0.35f,
                                        feature.kind == MapGeneratedFeatureKind::River ? 2.4f : 1.5f, "%.2f"))
                {
                    _controller.beginPropertyEditUndo("Edit Generated Feature");
                    feature.width = width;
                }
                if (feature.points.size() >= 2 && ImGui::TreeNode(localizedUiLabel("Control points").c_str()))
                {
                    ImGui::TextWrapped(
                        localizedUiLabel("Move control points to reshape this feature. Changes are undoable.").c_str());
                    const std::size_t pointLimit = std::min<std::size_t>(feature.points.size(), 32);
                    for (std::size_t pointIndex = 0; pointIndex < pointLimit; ++pointIndex)
                    {
                        ImGui::PushID(static_cast<int>(pointIndex));
                        float point[2] = {feature.points[pointIndex].x, feature.points[pointIndex].y};
                        const std::string label = localizedUiLabel("Point %d");
                        char pointLabel[64]{};
                        std::snprintf(pointLabel, sizeof(pointLabel), label.c_str(), static_cast<int>(pointIndex + 1));
                        const bool pointMoved =
                            FormUi::DragFloat2(pointLabel, point, 0.05f, 0.0f,
                                               static_cast<float>(std::max(map.width, map.height)), "%.2f");
                        if (pointMoved)
                        {
                            _controller.beginPropertyEditUndo("Move Generated Control Point");
                            if (!_hasPendingGeneratedFeatureProjection ||
                                _pendingGeneratedFeatureProjectionId != feature.id)
                            {
                                _pendingGeneratedFeatureProjectionId = feature.id;
                                _pendingGeneratedFeatureOriginal = feature;
                                _hasPendingGeneratedFeatureProjection = true;
                            }
                            feature.points[pointIndex].x = std::clamp(point[0], 0.0f, static_cast<float>(map.width));
                            feature.points[pointIndex].y = std::clamp(point[1], 0.0f, static_cast<float>(map.height));
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit() && _hasPendingGeneratedFeatureProjection &&
                            _pendingGeneratedFeatureProjectionId == feature.id)
                        {
                            // The feature geometry itself previews continuously. Rebuilding every
                            // route cell on every drag tick caused avoidable stalls on large maps,
                            // so project the final geometry once when the edit is released.
                            finishGeneratedFeatureProjection();
                        }
                        else if (_hasPendingGeneratedFeatureProjection &&
                                 _pendingGeneratedFeatureProjectionId == feature.id && ImGui::IsItemActive())
                            generatedFeatureProjectionItemActive = true;
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::EndDisabled();
                if (feature.locked)
                    ImGui::TextWrapped(
                        "%s",
                        localizedUiLabel("This feature is locked. Unlock it before changing its geometry.").c_str());
            }
            ImGui::PopID();
            ++shown;
        }
        if (!ImGui::IsAnyItemActive())
            _controller.finishPropertyEditUndo();
        if (drawUiButton("Rebuild Paths From Grid", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _controller.captureUndoSnapshot("Rebuild Generated Paths");
            rebuildContinuousMapWayFeaturesFromCells(map);
        }
    }
    if (_hasPendingGeneratedFeatureProjection && !generatedFeatureProjectionItemActive)
    {
        finishGeneratedFeatureProjection();
        _controller.finishPropertyEditUndo();
    }

    ImGui::Spacing();
    if (drawUiButton("Edit current map without generation", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
    {
        setWorkflowStep(3);
        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
    }
}
