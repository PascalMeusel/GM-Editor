#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorWorkflowFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawWorkflowLayout(const WorkflowFrame &frame)
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
    if (ImGui::BeginTable("MapCreatorLayout", useStackedMapLayout ? 1 : 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const bool stackedLayout = useStackedMapLayout;
        const bool showSeparateControlRail =
            gm::mapui::mapCreatorShowsSeparateControlRail(stackedLayout, showDesigns) &&
            !(stackedLayout && showEdit && !_showCompactInspector) && !(stackedLayout && showPlayerPreview);
        const float inspectorScale = std::max(1.0f, ImGui::GetFontSize() / 16.0f);
        const float minimumControlWidth = showEdit ? std::min(360.0f * inspectorScale, availableWidth * 0.42f)
                                                   : std::min(340.0f, availableWidth * 0.58f);
        const float maximumControlWidth =
            showEdit ? std::max(minimumControlWidth, std::min(420.0f * inspectorScale, availableWidth * 0.42f))
                     : std::max(minimumControlWidth, std::min(440.0f, availableWidth - 280.0f));
        const float controlWidth = showEdit
                                       ? gm::mapui::mapCreatorEditingRailWidth(availableWidth, ImGui::GetFontSize())
                                       : std::clamp(availableWidth * 0.36f, minimumControlWidth, maximumControlWidth);
        // Reapply the responsive edit width each frame, including after restoring an old
        // narrow splitter size. The canvas keeps most of the wide workspace.
        const ImGuiTableColumnFlags controlFlags =
            stackedLayout ? ImGuiTableColumnFlags_WidthStretch
                          : ImGuiTableColumnFlags_WidthFixed | (showEdit ? ImGuiTableColumnFlags_NoResize : 0);
        ImGui::TableSetupColumn(localizedUiLabel("Controls").c_str(), controlFlags, controlWidth);
        if (!stackedLayout)
            ImGui::TableSetupColumn(localizedUiLabel("Canvas").c_str());
        if (showSeparateControlRail)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const float stackedControlsMinimum =
                showDesigns ? std::max(150.0f, ImGui::GetTextLineHeightWithSpacing() * 7.0f) : 240.0f;
            const float stackedControlsMaximum = showDesigns ? std::max(stackedControlsMinimum, 230.0f) : 390.0f;
            const float stackedControlsFraction = showDesigns ? 0.30f : 0.44f;
            const float availableControlsHeight = ImGui::GetContentRegionAvail().y;
            const float compactInspectorHeight = gm::mapui::mapCreatorCompactInspectorHeight(availableControlsHeight);
            const float controlsHeight = stackedLayout && showEdit ? compactInspectorHeight
                                         : stackedLayout
                                             ? std::clamp(ImGui::GetContentRegionAvail().y * stackedControlsFraction,
                                                          stackedControlsMinimum, stackedControlsMaximum)
                                             : ImGui::GetContentRegionAvail().y;
            const float controlsScrollHeight =
                std::max(showEdit ? 80.0f : 140.0f, controlsHeight - persistentActionHeight);
            if (_resetWorkflowScroll || (showEdit && _resetInspectorScroll))
                ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
            if (showEdit)
                _resetInspectorScroll = false;
            ImGui::BeginChild("MapCreatorControlsScroll", ImVec2(0.0f, controlsScrollHeight), false,
                              ImGuiWindowFlags_None);

            drawWorkflowStageControls(frame, stackedLayout);
            drawWorkflowConfirmationModals(frame);
            drawWorkflowControlFooter(frame);
            ImGui::EndChild();
            if (hasPersistentPrimaryAction)
            {
                ImGui::Separator();
                ImGui::PushID("MapCreatorPersistentPrimaryAction");
                if (showIdea)
                {
                    const char *label =
                        map.creationIntent == MapCreationIntent::WorldOverview ? "Review facts" : "Apply and continue";
                    if (drawUiButton(label, !generationBusy, "Generation is already running.", UiButtonRole::Primary,
                                     ImVec2(-1.0f, 0.0f)))
                    {
                        prepareWorldBriefForGeneration();
                        applyDraftDimensions();
                        setWorkflowStep(1);
                    }
                }
                else if (showFacts)
                {
                    const MapGenerationValidationReport requestValidation =
                        _controller.validateDraftGenerationRequest();
                    if (drawUiButton(
                            map.scope == MapScope::Dungeon ? "Generate" : "Generate 3 Designs",
                            !generationBusy && requestValidation.eligible,
                            generationBusy ? "Generation is already running."
                                           : "Resolve the fact conflicts before generating.",
                            UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                            "Generate ranked editable designs. The current draft remains unchanged until you choose one."))
                    {
                        prepareWorldBriefForGeneration();
                        setWorkflowStep(2);
                        _controller.queueDraftGeneration();
                    }
                }
                else if (showExport)
                {
                    const MapPngExportSizing sizing = _controller.selectedPngExportSizing();
                    const char *label = _controller._transfer.exportPlayerSafe ? "Export Player-safe PNG" : "Export GM PNG";
                    if (drawUiButton(label, !generationBusy && sizing.valid,
                                     generationBusy ? "Generation is already running." : "Choose a smaller output size before exporting.",
                                     UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                                     "Choose a destination. Existing files require confirmation."))
                        _controller.exportDraftPng(_controller._transfer.exportPlayerSafe);
                }
                else if (showPlayerPreview)
                {
                    const bool readyForPlayerReview = _controller.savedDraftReadyForPlayerReview();
                    if (mapCreatorStepFivePrimaryAction(readyForPlayerReview) ==
                        MapCreatorStepFivePrimaryAction::ReviewPlayerView)
                    {
                        const UiButtonPairResult completionActions = drawResponsiveButtonPair(
                            "MapCreatorCompletionActions",
                            uiTextForLocale("Open player window review", "Spielerfenster prüfen"),
                            UiButtonRole::Secondary,
                            "Prepare this saved map in blackout and open the fail-closed GM/player review.",
                            "Open in World", UiButtonRole::Secondary,
                            "Leave Map Creator and open the saved map in the World workspace.");
                        if (completionActions.first)
                            _controller.reviewSavedMapForPlayers();
                        if (completionActions.second)
                            _controller.openSavedMapInWorld();
                    }
                    else if (drawUiButton("Save Map", !generationBusy, "Generation is already running.",
                                          UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                                          "Save this exact version before preparing a player-safe review."))
                    {
                        _controller.saveDraftMap();
                    }
                }
                ImGui::PopID();
            }
        }
        // A one-column ImGui table needs an explicit next row before drawing
        // the full-width canvas. In the design step there is intentionally no
        // separate control row: facts navigation lives in the sticky footer.
        if (stackedLayout && showSeparateControlRail)
            ImGui::TableNextRow();
        else if (!showSeparateControlRail)
            ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (showEdit)
        {
            if (_controller._generation.hasPendingPatch)
                drawPartialRegenerationPreview();
            else
                drawMapCanvas(useStackedMapLayout);
        }
        else if (showExport)
        {
            drawExportPreview();
        }
        else if (showPlayerPreview)
        {
            drawPlayerPreview(stackedLayout, generationBusy);
        }
        else if (showDesigns && generationBusy)
        {
            ImGui::BeginChild("GenerationInProgressPanel", ImGui::GetContentRegionAvail(), false,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            const float topSpacing = std::max(28.0f, ImGui::GetContentRegionAvail().y * 0.28f);
            ImGui::Dummy(ImVec2(0.0f, topSpacing));
            ImGui::SeparatorText(localizedUiLabel("Creating valid designs").c_str());
            const std::string progressLabel = _controller.generationProgressLabel();
            ImGui::ProgressBar(_controller.generationProgressFraction(), ImVec2(-1.0f, 0.0f),
                               progressLabel.empty() ? localizedUiText("Generating...") : progressLabel.c_str());
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    "The current map remains unchanged while terrain, rivers, places, and routes are checked in the background.")
                    .c_str());
            if (drawUiButton("Cancel generation", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                _controller.cancelDraftGeneration();
            ImGui::EndChild();
        }
        else if (showDesigns && !_controller._generation.candidates.empty() && !generationBusy)
        {
            drawDesignCandidates(stackedLayout);
        }
        else if (showDesigns && !generationBusy && _controller._generation.hasLastQuality &&
                 _controller._generation.candidates.empty())
        {
            drawGenerationConflicts();
        }
        else if ((_workflowStep == 0 || _workflowStep == 1 || _workflowStep == 2) && _controller._persistence.editingMapIndex < 0 &&
                 !mapContainsVisibleDraftContent(map))
        {
            drawPendingMapPreview(_workflowStep >= 1);
        }
        else
        {
            ImGui::SeparatorText(localizedUiLabel("Map preview").c_str());
            const float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y - 16.0f, 160.0f, 520.0f);
            drawGenerationMiniMapFitted(map, _renderPlanCache, map.revision, "CurrentMapWorkflowPreview",
                                        std::max(170.0f, previewHeight));
        }
        ImGui::EndTable();
    }
    _resetWorkflowScroll = false;
}
