#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorWorkflowFrame.hpp"

using namespace gm::map_creator_view_detail;

bool MapCreatorView::drawWorkflowNavigation(MapModel &map, bool generationBusy, bool authoring, bool stackedEditLayout)
{
    if (!authoring && !_advancedCreation && !_controller._history.hasRecoveryPreview && !_loadMapReviewRequested)
    {
        drawSimpleCreation();
        return true;
    }
    if (_controller._history.hasRecoveryPreview)
        _advancedCreation = true;
    if (!authoring && _advancedCreation &&
        drawUiButton(uiTextForLocale("Back to simple creation", "Zur einfachen Erstellung"), !generationBusy,
                     "Generation is already running.", UiButtonRole::Secondary))
        _advancedCreation = false;
    const char *workflowLabels[] = {uiTextForLocale("Describe", "Beschreiben"),
                                    uiTextForLocale("Review requirements", "Anforderungen prüfen"),
                                    uiTextForLocale("Choose design", "Entwurf wählen")};
    const char *authoringLabels[] = {uiTextForLocale("Edit", "Bearbeiten"),
                                     uiTextForLocale("Player preview", "Spielervorschau"),
                                     uiTextForLocale("Export", "Ausgeben")};
    const int authoringStages[] = {3, 5, 4};
    const char **visibleLabels = authoring ? authoringLabels : workflowLabels;
    float widestWorkflowButton = 0.0f;
    for (int labelIndex = 0; labelIndex < 3; ++labelIndex)
        widestWorkflowButton = std::max(widestWorkflowButton, ImGui::CalcTextSize(visibleLabels[labelIndex]).x);
    widestWorkflowButton += ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().CellPadding.x * 2.0f;
    const float workflowWidth = ImGui::GetContentRegionAvail().x;
    const int workflowColumns =
        std::min(3, gm::mapui::mapCreatorWorkflowColumnCount(workflowWidth, widestWorkflowButton));
    const float navigationWidth = authoring ? std::min(workflowWidth, widestWorkflowButton * 3.f + 24.f) : 0.f;
    if (ImGui::BeginTable("MapCreatorWorkflow", workflowColumns,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings,
                          ImVec2(navigationWidth, 0)))
    {
        for (int tab = 0; tab < 3; ++tab)
        {
            const int step = authoring ? authoringStages[tab] : tab;
            ImGui::TableNextColumn();
            const bool canChangeStep = (!generationBusy && !_imageTraceActive) || _workflowStep == step;
            if (drawChoiceButton(visibleLabels[tab], _workflowStep == step, canChangeStep,
                                 _imageTraceActive ? "Finish or cancel tracing before continuing."
                                                   : "Generation is running; wait before changing steps."))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.endBrushStroke();
                _controller.finishPropertyEditUndo();
                if (_workflowStep == 0 && step != 0)
                {
                    applyDraftDimensions();
                    if (step == 1)
                        prepareWorldBriefForGeneration();
                }
                setWorkflowStep(step);
                if (step != 3)
                    _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
            }
        }
        ImGui::EndTable();
    }
    if (authoring)
    {
        drawAuthoringToolbar();
        if (_workflowStep == 3 && ImGui::IsPopupOpen("MapAuthoringOptions"))
        {
            const float width = std::min(28.0f * ImGui::GetFontSize(), ImGui::GetMainViewport()->WorkSize.x - 32.0f);
            ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
        }
        if (_workflowStep == 3 && ImGui::BeginPopup("MapAuthoringOptions"))
        {
            ImGui::BeginDisabled(generationBusy || _imageTraceActive);
            ImGui::TextUnformatted(uiTextForLocale("Map name", "Kartenname"));
            ImGui::SetNextItemWidth(-1.0f);
            undoableInputText("##MapMenuName", map.name, "Edit Map Details");
            ImGui::EndDisabled();
            ImGui::Spacing();
            if (drawUiButton(uiTextForLocale("Choose another design", "Anderen Entwurf wählen"),
                             !generationBusy && !_imageTraceActive, "Finish the current edit first.",
                             UiButtonRole::Secondary, ImVec2(-1, 0)))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.endBrushStroke();
                _controller.finishPropertyEditUndo();
                _advancedCreation = false;
                setWorkflowStep(_controller._generation.candidates.empty() ? 0 : 2);
                ImGui::CloseCurrentPopup();
            }
            drawArtStyleControl(generationBusy, true);
            const bool editable = !generationBusy && !_imageTraceActive;
            if (drawUiButton(
                    uiTextForLocale("Attach to scene", "Mit Szene verknüpfen"),
                    editable && static_cast<bool>(_controller._integration.attachSavedMapToSceneHandler),
                    uiTextForLocale("Select a scene in Session first.", "Wähle zuerst eine Szene unter Sitzung."),
                    UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.endBrushStroke();
                _controller.finishPropertyEditUndo();
                _controller.attachDraftMapToScene();
            }
            if (_controller._integration.attachSavedMapToEncounterHandler && drawUiButton(
                    uiTextForLocale("Save and assign to encounter", "Speichern und Begegnung zuweisen"), editable,
                    "Finish the current edit first.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.endBrushStroke();
                _controller.finishPropertyEditUndo();
                _controller.attachDraftMapToEncounter();
            }
            if (drawUiButton(uiTextForLocale("Review for players", "Für Spieler prüfen"), editable,
                             "Finish the current edit first.", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            {
                finishGeneratedFeatureGeometryEdit();
                _controller.endBrushStroke();
                _controller.finishPropertyEditUndo();
                setWorkflowStep(5);
                if (_controller.saveDraftMap())
                    _controller.reviewSavedMapForPlayers();
            }
            if (ImGui::TreeNode(uiTextForLocale("More map actions", "Weitere Kartenaktionen")))
            {
                ImGui::BeginDisabled(!editable);
                drawPartialRegenerationControls();
                drawCheckpointControls();
                drawFogAndLayerPanel();
                drawImageTraceControls();
                if (drawUiButton(uiTextForLocale("Map setup", "Karteneinstellungen"), UiButtonRole::Secondary,
                                 ImVec2(-1, 0)))
                {
                    setWorkflowStep(0);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::EndPopup();
        }
        if (_workflowStep == 3 && stackedEditLayout)
            drawToolPalette();
        const std::string &status = _controller.draftStatusMessage();
        const bool attention =
            _controller._persistence.saveFailed || _controller._history.crashJournalWriteFailed ||
            status.find("failed") != std::string::npos || status.find("could not") != std::string::npos ||
            status.find("Could not") != std::string::npos || status.find("fehlgeschlagen") != std::string::npos ||
            status.find("konnte") != std::string::npos || status.find("nicht verknüpft") != std::string::npos;
        if (attention)
            ImGui::TextWrapped("%s", localizedUiLabel(status.c_str()).c_str());
    }
    else if (drawUiButton(uiTextForLocale("Edit current map", "Aktuelle Karte bearbeiten"), !generationBusy,
                          "Generation is already running.", UiButtonRole::Secondary, ImVec2(0.0f, 0.0f)))
    {
        applyDraftDimensions();
        setWorkflowStep(3);
        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
    }
    if (!authoring)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                _workflowStep == 0   ? "Start with whatever you already know. The generator fills the gaps."
                : _workflowStep == 1 ? "Review what must stay, what you prefer, and what may be filled automatically."
                : _workflowStep == 2 ? "Compare up to three valid map designs."
                : _workflowStep == 3 ? "Edit terrain and map objects on the canvas."
                : _workflowStep == 5 ? "Save this version before reviewing it with the player-safe filters."
                                     : "Save the map and create GM or player-safe output.")
                .c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (_controller._history.hasRecoveryPreview)
    {
        bool discardRecoveryRequested = false;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(67, 57, 35, 220));
        if (ImGui::BeginChild("MapRecoveryNotice", ImVec2(0.0f, 184.0f), true))
        {
            ImGui::TextWrapped(
                "%s", localizedUiLabel("Unsaved map changes from the last editing session are available.").c_str());
            if (!_controller._history.recoveryPreview.checkpoint.transactionLabel.empty())
                drawInlineLabelValue("Last edit", _controller._history.recoveryPreview.checkpoint.transactionLabel.c_str());
            const UiButtonPairResult recoveryActions =
                drawResponsiveButtonPair("MapRecoveryActions", "Restore unsaved changes", UiButtonRole::Primary,
                                         "Restore the recovery copy into this draft.", "Discard recovery",
                                         UiButtonRole::Destructive, "Permanently discard this recovery copy.");
            if (recoveryActions.first)
                _controller.restoreCrashRecovery();
            discardRecoveryRequested = recoveryActions.second;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (discardRecoveryRequested)
            ImGui::OpenPopup(localizedUiLabel("Discard Map Recovery").c_str());
        if (drawMapConfirmationModal("Discard Map Recovery", "Discard the unsaved map recovery copy?",
                                     uiTextForLocale("Discard", "Verwerfen")))
            _controller.discardCrashRecovery();
        ImGui::Separator();
    }
    return false;
}
