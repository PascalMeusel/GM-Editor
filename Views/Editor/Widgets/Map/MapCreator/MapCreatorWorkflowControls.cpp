#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorWorkflowFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawWorkflowStageControls(const WorkflowFrame &frame, bool stackedLayout)
{
    MapModel &map = *frame.map;
    const bool generationBusy = frame.generationBusy;
    const bool authoring = frame.authoring;
    const bool showIdea = frame.showIdea;
    const bool showFacts = frame.showFacts;
    const bool showDesigns = frame.showDesigns;
    const bool showEdit = frame.showEdit;
    const bool showExport = frame.showExport;
    const bool showPlayerPreview = frame.showPlayerPreview;
    const bool hasPersistentPrimaryAction = frame.hasPersistentPrimaryAction;
    const float persistentActionHeight = frame.persistentActionHeight;
    const bool useStackedMapLayout = frame.useStackedMapLayout;
            ImGui::BeginDisabled(generationBusy);
            if (showIdea)
            {
                drawIdeaControls(generationBusy);
            }

            if (showFacts)
            {
                drawFactsControls(generationBusy);
            }
            if (showDesigns)
            {
                ImGui::SeparatorText(localizedUiLabel("Design review").c_str());
                ImGui::TextWrapped(
                    "%s",
                    localizedUiLabel(
                        "Compare the complete maps on the right. Only designs that satisfy every required fact can be applied.")
                        .c_str());
                if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
                {
                    drawInlineLabelValue("Map type",
                                         localizedUiLabel(mapCreationIntentLabel(map.creationIntent)).c_str());
                    drawInlineLabelValue("Map size", localizedUiLabel(sizePresetLabel(map.sizePreset)).c_str());
                    ImGui::TreePop();
                }
                if (!_controller._generation.candidates.empty() && !generationBusy)
                {
                    ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.66f, 1.0f),
                                       localizedUiLabel("%d valid designs ready").c_str(),
                                       static_cast<int>(_controller._generation.candidates.size()));
                    if (_controller._generation.rejectedGenerationCandidateCount > 0)
                        ImGui::TextDisabled(localizedUiLabel("%d invalid alternatives rejected").c_str(),
                                            _controller._generation.rejectedGenerationCandidateCount);
                }
                if (drawUiButton("Review facts", !generationBusy, "Generation is running; wait before changing steps.",
                                 UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                                 "Return to the facts without changing your current map."))
                    setWorkflowStep(1);
            }
            ImGui::EndDisabled();

            const MapModel *selectedMap = nullptr;
            if (_controller._persistence.selectedMapIndex >= 0 &&
                _controller._persistence.selectedMapIndex < static_cast<int>(_controller._currentCampaign.maps.size()))
            {
                selectedMap =
                    &_controller._currentCampaign.maps[static_cast<std::size_t>(_controller._persistence.selectedMapIndex)];
            }

            if (ImGui::BeginPopupModal(localizedUiLabel("New Map Review").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    localizedUiLabel("Start a clean draft map. Unsaved edits in the current draft will be replaced.")
                        .c_str());
                drawInlineLabelValue("Current draft",
                                     map.name.empty() ? localizedUiText("Unnamed draft") : map.name.c_str());
                if (drawModalActionButtons("Start New", UiButtonRole::Advanced))
                    _controller.startNewMap();
                ImGui::EndPopup();
            }

            if (_loadMapReviewRequested)
            {
                ImGui::OpenPopup(localizedUiLabel("Load Map Review").c_str());
                _loadMapReviewRequested = false;
            }
            if (ImGui::BeginPopupModal(localizedUiLabel("Load Map Review").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    localizedUiLabel(
                        "Load this map into the editor. Unsaved changes in the current draft will be replaced.")
                        .c_str());
                drawInlineLabelValue("Selected", selectedMap
                                                     ? (selectedMap->name.empty() ? localizedUiText("Unnamed map")
                                                                                  : selectedMap->name.c_str())
                                                     : localizedUiText("No map selected"));
                if (selectedMap)
                {
                    const std::string dimensions =
                        std::to_string(selectedMap->width) + " x " + std::to_string(selectedMap->height);
                    drawInlineLabelValue("Size", dimensions.c_str());
                }
                if (drawModalActionButtons("Load Map", UiButtonRole::Primary))
                {
                    _controller.loadSelectedMap();
                    _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
                    setWorkflowStep(3);
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal(localizedUiLabel("Apply Map Size").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    localizedUiLabel(
                        "Resize the draft grid. Existing cells may be cropped or expanded to fit the new dimensions.")
                        .c_str());
                const std::string dimensions = std::to_string(_draftWidth) + " x " + std::to_string(_draftHeight);
                drawInlineLabelValue("New size", dimensions.c_str());
                if (drawModalActionButtons("Apply Size", UiButtonRole::Advanced))
                    applyDraftDimensions();
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal(localizedUiLabel("Generate Map Layout").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    localizedUiLabel(
                        "Generate ranked editable designs. The current draft remains unchanged until you choose one.")
                        .c_str());
                if (map.scope == MapScope::Dungeon)
                {
                    const int generatorCount =
                        static_cast<int>(sizeof(generatorItemLabels) / sizeof(generatorItemLabels[0]));
                    const int generatorIndex = std::clamp(_controller._generation.selectedGeneratorIndex, 0, generatorCount - 1);
                    drawInlineLabelValue("Generator", localizedUiText(generatorItemLabels[generatorIndex]));
                }
                else
                {
                    const char *landscapeLabels[] = {"River Valley", "Forest Frontier", "Highlands", "Mountain Pass",
                                                     "Lake Country"};
                    drawInlineLabelValue("Map type",
                                         localizedUiLabel(mapCreationIntentLabel(map.creationIntent)).c_str());
                    drawInlineLabelValue("Map size", localizedUiLabel(sizePresetLabel(map.sizePreset)).c_str());
                    drawInlineLabelValue(
                        "Landscape",
                        localizedUiLabel(landscapeLabels[std::clamp(_controller._generation.generatorLandscapeIndex, 0, 4)])
                            .c_str());
                }
                drawInlineLabelValue("Seed", std::to_string(_controller._generation.generatorSeed).c_str());
                const int qualityIndex = std::clamp(_controller._generation.generatorQualityIndex, 0, 2);
                const char *searchDescription =
                    qualityIndex == 0 ? "Best 3 of 12 candidates"
                                      : (qualityIndex == 1 ? "Best 3 of 24 candidates" : "Best 3 of 48 candidates");
                drawInlineLabelValue("Search", localizedUiText(searchDescription));
                const std::string density =
                    localizedUiLabel("Water") + " " + std::to_string(_controller._generation.generatorWaterLevel) + " / " +
                    localizedUiLabel("Mountains") + " " + std::to_string(_controller._generation.generatorMountainDensity) +
                    " / " + localizedUiLabel("Forests") + " " + std::to_string(_controller._generation.generatorForestDensity);
                drawInlineLabelValue("Density", density.c_str());
                if (drawModalActionButtons(map.scope == MapScope::Dungeon ? "Generate" : "Generate Designs",
                                           UiButtonRole::Primary))
                    _controller.queueDraftGeneration();
                ImGui::EndPopup();
            }

            if (generationBusy)
            {
                const std::string progressLabel = _controller.generationProgressLabel();
                const float progressFraction = _controller.generationProgressFraction();
                const std::string visibleProgress =
                    progressLabel.empty() ? localizedUiLabel("Generating...") : progressLabel;
                ImGui::ProgressBar(progressFraction >= 0.0f ? progressFraction : -static_cast<float>(ImGui::GetTime()),
                                   ImVec2(-1.0f, 0.0f), visibleProgress.c_str());
                const std::string generationDetail = _controller.generationDetailLabel();
                if (!generationDetail.empty())
                    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), generationDetail.c_str());
                if (drawUiButton(
                        "Cancel generation", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                        "Invalidate this request. A worker already running may finish, but its result will never be applied."))
                    _controller.cancelDraftGeneration();
            }

            ImGui::BeginDisabled(generationBusy);
            if (showEdit)
            {
                ImGui::BeginDisabled(_imageTraceActive);
                _drawingCompactInspector = stackedLayout;
                const bool areaSelected = !_areaCandidates.empty() && _areaSelectionSerial==_controller._editing.authoringSerial;
                if(areaSelected) drawAreaSelectionSidebar();
                if (!stackedLayout && (!areaSelected || ImGui::CollapsingHeader(uiTextForLocale("Tools", "Werkzeuge"))))
                    drawToolPalette(true);
                ImGui::Spacing();
                if (ImGui::BeginTable("MapSidebarPages", 2,
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableNextColumn();
                    if (drawChoiceButton(uiTextForLocale("Tool settings", "Werkzeug"), _sidebarPage == 0))
                        _sidebarPage = 0;
                    ImGui::TableNextColumn();
                    if (drawChoiceButton(uiTextForLocale("Map contents", "Karteninhalt"), _sidebarPage == 1))
                        _sidebarPage = 1;
                    ImGui::EndTable();
                }
                ImGui::Spacing();
                if (_sidebarPage == 1)
                    drawMapOutline();
                else
                {
                    if (_controller.hasSelectedObject() || !_areaCandidates.empty())
                    {
                        drawMapSidePanels();
                        drawToolControls();
                    }
                    else
                    {
                        drawToolControls();
                        drawMapSidePanels();
                    }
                }
                _drawingCompactInspector = false;
                ImGui::EndDisabled();
                if (_imageTraceActive)
                    drawImageTraceControls();
            }
            if (showPlayerPreview)
            {
                drawPlayerReviewControls(generationBusy);
            }
            if (showExport)
            {
                drawExportControls(generationBusy);
            }
            ImGui::EndDisabled();

}
