#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawFogAndLayerPanel()
{
    using Tool = MapCreatorController::Tool;

    MapModel &map = _controller._draftMap;
    if (!beginSidePanel("Fog & Layers", false))
        return;

    ImGui::SeparatorText(localizedUiLabel("Map layers").c_str());
    drawDisabledWrappedText(
        "Choose where new tokens and overlays belong. Hidden layers stay on the map; locked layers are protected from editing.");

    std::vector<const char *> layerLabels;
    for (const MapLayerModel &entry : map.layers)
        layerLabels.push_back(entry.name.empty() ? localizedUiText("<Layer>") : entry.name.c_str());
    _controller._editing.selectedLayerIndex =
        std::clamp(_controller._editing.selectedLayerIndex, 0, static_cast<int>(layerLabels.size()) - 1);

    ImGui::TextUnformatted(localizedUiLabel("Active layer").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    FormUi::Combo("##MapCreatorActiveLayer", &_controller._editing.selectedLayerIndex, layerLabels.data(),
                  static_cast<int>(layerLabels.size()));
    MapLayerModel &layer = map.layers[_controller._editing.selectedLayerIndex];

    ImGui::TextUnformatted(localizedUiLabel("Layer name").c_str());
    ImGui::SetNextItemWidth(-1.0f);
    undoableInputText("##MapCreatorLayerName", layer.name, "Edit Layer");
    undoableCheckbox("Visible", layer.visible, "Edit Layer");
    ImGui::SameLine();
    undoableCheckbox("Locked", layer.locked, "Edit Layer");

    if (ImGui::TreeNode(localizedUiLabel("Layer order (advanced)").c_str()))
    {
        drawDisabledWrappedText("Higher drawing-order values place a layer above lower values.");
        ImGui::SetNextItemWidth(-1.0f);
        undoableInputInt("Drawing order", layer.sortOrder, "Edit Layer");
        ImGui::TreePop();
    }

    const UiButtonPairResult layerActions = drawResponsiveButtonPair(
        "MapLayerActions", "Add Layer", UiButtonRole::Secondary,
        "Add a map layer for future overlays and token placement.", "Remove Layer", UiButtonRole::Destructive,
        "Remove the selected map layer.", _controller._editing.selectedLayerIndex > 0, "The base layer cannot be removed.");
    if (layerActions.first)
        _controller.addLayer();
    if (layerActions.second)
        ImGui::OpenPopup(localizedUiLabel("Remove Map Layer").c_str());
    if (drawMapConfirmationModal("Remove Map Layer", "Remove the selected map layer?", "Remove"))
        _controller.removeSelectedLayer();

    ImGui::SeparatorText(localizedUiLabel("Map image reference").c_str());
    if (!map.importedReference.enabled)
    {
        if (!map.backgroundImagePath.empty())
        {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "%s",
                               localizedUiLabel("This older map still uses an external image file.").c_str());
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    "The image visible on the canvas is your preview. Copying it into the campaign keeps every cell, feature, name, fog state, and atlas link unchanged.")
                    .c_str());
            if (drawUiButton("Review and manage legacy image", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
                ImGui::OpenPopup(localizedUiLabel("Manage Legacy Map Image").c_str());
            if (ImGui::BeginPopupModal(localizedUiLabel("Manage Legacy Map Image").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    "%s",
                    localizedUiLabel(
                        "Copy the displayed source image into managed campaign storage? It remains GM-only. This map's geometry and gameplay content are not regenerated.")
                        .c_str());
                if (drawModalActionButtons("Copy into campaign", UiButtonRole::Primary))
                {
                    _controller.adoptLegacyReferenceImage();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::Spacing();
            ImGui::Separator();
        }
        ImGui::TextWrapped(
            "%s",
            localizedUiLabel(
                "No managed image is attached. Imported PNG and JPEG files are copied into the campaign and never depend on the original file path.")
                .c_str());
        if (drawUiButton("Import map image", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            _controller.browseForReferenceImage();
    }
    else
    {
        const MapImageImport::ManagedReferenceInspection referenceInspection =
            MapImageImport::inspectManagedReference(map.importedReference, false);
        const std::string imageName = map.importedReference.sourceFileName.empty()
                                          ? localizedUiLabel("Managed map image")
                                          : map.importedReference.sourceFileName;
        drawInlineLabelValue("Image", imageName.c_str());
        const std::string sourceDimensions = std::to_string(map.importedReference.sourceWidth) + " x " +
                                             std::to_string(map.importedReference.sourceHeight) + " px";
        drawInlineLabelValue("Source size", sourceDimensions.c_str());
        if (!referenceInspection.healthy)
        {
            ImGui::TextColored(
                ImVec4(0.95f, 0.40f, 0.30f, 1.0f), "%s",
                localizedUiLabel(MapImageImport::managedReferenceErrorMessage(referenceInspection.errorCode)).c_str());
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    "Choose a replacement image to repair the reference. Existing map content will not be changed.")
                    .c_str());
        }
        if (map.importedReference.gmOnly)
        {
            ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s",
                               localizedUiLabel("Safe default: the original image is hidden from players.").c_str());
        }
        else
        {
            ImGui::TextColored(
                ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "%s",
                localizedUiLabel("Player-visible image: secrets painted into the source image cannot be filtered.")
                    .c_str());
        }
        if (map.importedReference.gmOnly)
        {
            if (drawUiButton("Review player publishing", referenceInspection.healthy,
                             MapImageImport::managedReferenceErrorMessage(referenceInspection.errorCode),
                             UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                             "Review the original image before allowing it in player output."))
            {
                if (_controller.verifyReferenceImageForPlayerPublishing())
                    ImGui::OpenPopup(localizedUiLabel("Publish Original Map Image").c_str());
            }
        }
        else if (drawUiButton("Hide original image from players", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _controller.setReferenceImagePlayerVisible(false);
        }
        if (ImGui::BeginPopupModal(localizedUiLabel("Publish Original Map Image").c_str(), nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    "The complete source image will be visible to players. Secrets painted into its pixels cannot be hidden by layers or fog. Review the image first.")
                    .c_str());
            if (drawModalActionButtons("Allow in player output", UiButtonRole::Advanced))
            {
                if (_controller.setReferenceImagePlayerVisible(true))
                    ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (drawUiButton(
                referenceInspection.healthy ? "Replace map image" : "Repair map image", UiButtonRole::Secondary,
                ImVec2(-1.0f, 0.0f),
                "Replace only the managed image. Cells, features, fog, tokens, and atlas links remain unchanged."))
            _controller.browseForReferenceImage(true);

        if (ImGui::TreeNode(localizedUiLabel("Crop, rotate, and calibrate").c_str()))
        {
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    "Preview the crop, orientation, and optional distance scale here. The saved map changes only when you apply the settings.")
                    .c_str());

            ImGui::SeparatorText(localizedUiLabel("1. Choose the visible area").c_str());
            ImGui::TextWrapped("%s",
                               localizedUiLabel("Set how much of the displayed image to hide from each edge.").c_str());

            if (drawUiButton("Whole image", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                             "Restore all four image edges in this preview."))
            {
                gm::mapui::mapImageInspectorUseWholeImage(_controller._transfer.importCropX, _controller._transfer.importCropY,
                                                          _controller._transfer.importCropWidth, _controller._transfer.importCropHeight);
            }

            gm::mapui::MapImageCropMarginsPercent cropMargins =
                gm::mapui::mapImageCropMarginsPercent(_controller._transfer.importCropX, _controller._transfer.importCropY,
                                                      _controller._transfer.importCropWidth, _controller._transfer.importCropHeight);
            bool cropChanged = false;
            const int cropColumns = gm::mapui::mapImageInspectorFieldColumnCount(ImGui::GetContentRegionAvail().x,
                                                                                 ImGui::GetTextLineHeight());
            if (ImGui::BeginTable("MapImageCropMargins", cropColumns,
                                  ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
            {
                const auto marginSlider = [&](const char *label, const char *id, float &value, float maximum) {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(localizedUiLabel(label).c_str());
                    ImGui::SetNextItemWidth(-1.0f);
                    cropChanged |= FormUi::SliderFloat(id, &value, 0.0f, std::max(0.0f, maximum), "%.0f%%");
                };
                marginSlider("Left margin", "##MapImageCropLeft", cropMargins.left,
                             100.0f - gm::mapui::mapImageMinimumVisiblePercent - cropMargins.right);
                marginSlider("Right margin", "##MapImageCropRight", cropMargins.right,
                             100.0f - gm::mapui::mapImageMinimumVisiblePercent - cropMargins.left);
                marginSlider("Top margin", "##MapImageCropTop", cropMargins.top,
                             100.0f - gm::mapui::mapImageMinimumVisiblePercent - cropMargins.bottom);
                marginSlider("Bottom margin", "##MapImageCropBottom", cropMargins.bottom,
                             100.0f - gm::mapui::mapImageMinimumVisiblePercent - cropMargins.top);
                ImGui::EndTable();
            }
            if (cropChanged)
            {
                const MapNormalizedRect crop = gm::mapui::mapImageCropFromMarginsPercent(cropMargins);
                _controller._transfer.importCropX = crop.x;
                _controller._transfer.importCropY = crop.y;
                _controller._transfer.importCropWidth = crop.width;
                _controller._transfer.importCropHeight = crop.height;
                cropMargins = gm::mapui::mapImageCropMarginsPercent(crop.x, crop.y, crop.width, crop.height);
            }

            ImGui::TextUnformatted(localizedUiLabel("Crop preview").c_str());
            const float previewWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const int previewPixelsWide = map.importedReference.workingProxyWidth > 0
                                              ? map.importedReference.workingProxyWidth
                                              : map.importedReference.sourceWidth;
            const int previewPixelsHigh = map.importedReference.workingProxyHeight > 0
                                              ? map.importedReference.workingProxyHeight
                                              : map.importedReference.sourceHeight;
            const float sourceAspect =
                map.width > 0 && map.height > 0
                    ? std::clamp(static_cast<float>(map.width) / static_cast<float>(map.height), 0.5f, 3.0f)
                    : (previewPixelsWide > 0 && previewPixelsHigh > 0
                           ? std::clamp(static_cast<float>(previewPixelsWide) / static_cast<float>(previewPixelsHigh),
                                        0.5f, 3.0f)
                           : 1.5f);
            float diagramWidth = previewWidth;
            float diagramHeight = diagramWidth / sourceAspect;
            constexpr float maximumDiagramHeight = 150.0f;
            if (diagramHeight > maximumDiagramHeight)
            {
                diagramHeight = maximumDiagramHeight;
                diagramWidth = diagramHeight * sourceAspect;
            }
            const float diagramOffset = std::max(0.0f, (previewWidth - diagramWidth) * 0.5f);
            const ImVec2 cursorPosition = ImGui::GetCursorScreenPos();
            const ImVec2 diagramStart(cursorPosition.x + diagramOffset, cursorPosition.y);
            const ImVec2 diagramEnd(diagramStart.x + diagramWidth, diagramStart.y + diagramHeight);
            ImGui::InvisibleButton("##MapImageCropPreview", ImVec2(previewWidth, diagramHeight));
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(diagramStart, diagramEnd, IM_COL32(31, 36, 35, 255), 4.0f);
            const CachedImageTexture &cropPreviewTexture = getCachedImageTexture(
                ProjectAssets::resolveProjectPath(MapImageImport::preferredRenderProjectPath(map.importedReference)),
                MapImageImport::preferredRenderContentHash(map.importedReference));
            if (cropPreviewTexture.loaded)
            {
                const float radians = -_controller._transfer.importRotationDegrees * 0.01745329251994329577f;
                const float cosine = std::cos(radians);
                const float sine = std::sin(radians);
                const auto previewUv = [&](float x, float y) {
                    const float centeredX = x - 0.5f;
                    const float centeredY = y - 0.5f;
                    const float rotatedX = cosine * centeredX - sine * centeredY + 0.5f;
                    const float rotatedY = sine * centeredX + cosine * centeredY + 0.5f;
                    return ImVec2(_controller._transfer.importCropX + rotatedX * _controller._transfer.importCropWidth,
                                  _controller._transfer.importCropY + rotatedY * _controller._transfer.importCropHeight);
                };
                drawList->AddImageQuad(cropPreviewTexture.textureId, diagramStart, ImVec2(diagramEnd.x, diagramStart.y),
                                       diagramEnd, ImVec2(diagramStart.x, diagramEnd.y), previewUv(0.0f, 0.0f),
                                       previewUv(1.0f, 0.0f), previewUv(1.0f, 1.0f), previewUv(0.0f, 1.0f),
                                       IM_COL32_WHITE);
            }
            else
            {
                const ImVec2 visibleStart(diagramStart.x + diagramWidth * cropMargins.left / 100.0f,
                                          diagramStart.y + diagramHeight * cropMargins.top / 100.0f);
                const ImVec2 visibleEnd(diagramEnd.x - diagramWidth * cropMargins.right / 100.0f,
                                        diagramEnd.y - diagramHeight * cropMargins.bottom / 100.0f);
                drawList->AddRectFilled(visibleStart, visibleEnd, IM_COL32(44, 121, 132, 150), 3.0f);
                drawList->AddRect(visibleStart, visibleEnd, IM_COL32(229, 181, 76, 255), 3.0f, 0, 2.0f);
            }
            drawList->AddRect(diagramStart, diagramEnd, IM_COL32(112, 122, 119, 255), 4.0f);
            ImGui::TextWrapped("%s", formatLocalizedUi("Visible area: %.0f%% wide, %.0f%% high",
                                                       cropMargins.visibleWidth(), cropMargins.visibleHeight())
                                         .c_str());

            ImGui::SeparatorText(localizedUiLabel("2. Orient the image").c_str());
            const UiButtonPairResult rotationActions =
                drawResponsiveButtonPair("MapImageOrientationActions", "Turn left 90 degrees", UiButtonRole::Secondary,
                                         "Rotate this preview 90 degrees counter-clockwise.", "Turn right 90 degrees",
                                         UiButtonRole::Secondary, "Rotate this preview 90 degrees clockwise.");
            if (rotationActions.first)
                _controller._transfer.importRotationDegrees =
                    gm::mapui::mapImageInspectorRotateQuarterTurn(_controller._transfer.importRotationDegrees, -1);
            if (rotationActions.second)
                _controller._transfer.importRotationDegrees =
                    gm::mapui::mapImageInspectorRotateQuarterTurn(_controller._transfer.importRotationDegrees, 1);
            const bool rotationIsReset = std::isfinite(_controller._transfer.importRotationDegrees) &&
                                         std::abs(_controller._transfer.importRotationDegrees) < 0.0001f;
            if (drawUiButton("Reset rotation", !rotationIsReset, "The image is already upright in this preview.",
                             UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f), "Return the preview to zero degrees."))
                _controller._transfer.importRotationDegrees = 0.0f;
            const std::string rotationSummary = rotationIsReset ? localizedUiLabel("No rotation")
                                                                : formatLocalizedUi("Rotation preview: %+.0f degrees",
                                                                                    _controller._transfer.importRotationDegrees);
            drawDisabledWrappedText(rotationSummary.c_str());

            ImGui::SeparatorText(localizedUiLabel("3. Optional distance scale").c_str());
            ImGui::TextWrapped(
                "%s",
                localizedUiLabel(
                    "Add a real-world size only when distances on this map should be measurable. Atlas links do not require it.")
                    .c_str());
            FormUi::Checkbox(localizedUiLabel("Use real-world size").c_str(), &_controller._transfer.importSpatialCalibrated);
            if (_controller._transfer.importSpatialCalibrated)
            {
                const int sizeColumns = gm::mapui::mapImageInspectorFieldColumnCount(ImGui::GetContentRegionAvail().x,
                                                                                     ImGui::GetTextLineHeight());
                if (ImGui::BeginTable("MapImageRealSize", sizeColumns,
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(localizedUiLabel("Map width").c_str());
                    ImGui::SetNextItemWidth(-1.0f);
                    FormUi::InputFloat("##MapImageRealWidth", &_controller._transfer.importExtentWidth, 0.0f, 0.0f, "%.2f");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(localizedUiLabel("Map height").c_str());
                    ImGui::SetNextItemWidth(-1.0f);
                    FormUi::InputFloat("##MapImageRealHeight", &_controller._transfer.importExtentHeight, 0.0f, 0.0f, "%.2f");
                    ImGui::EndTable();
                }
                FormUi::Combo(localizedUiLabel("Distance unit").c_str(), &_controller._transfer.importDistanceUnitIndex,
                              localizedComboItems(distanceUnitItems).c_str());
            }

            const bool cropValid =
                gm::mapui::mapImageInspectorCropIsValid(_controller._transfer.importCropX, _controller._transfer.importCropY,
                                                        _controller._transfer.importCropWidth, _controller._transfer.importCropHeight);
            const bool calibrationValid = gm::mapui::mapImageInspectorCalibrationIsValid(
                _controller._transfer.importSpatialCalibrated, _controller._transfer.importExtentWidth, _controller._transfer.importExtentHeight,
                _controller._transfer.importDistanceUnitIndex);
            const bool hasPreviewChanges = gm::mapui::mapImageInspectorHasPendingChanges(
                map.importedReference, map.spatialReference, _controller._transfer.importCropX, _controller._transfer.importCropY,
                _controller._transfer.importCropWidth, _controller._transfer.importCropHeight, _controller._transfer.importRotationDegrees,
                _controller._transfer.importSpatialCalibrated, _controller._transfer.importExtentWidth, _controller._transfer.importExtentHeight,
                _controller._transfer.importDistanceUnitIndex);
            const char *applyBlocker =
                !cropValid ? "The visible area must be at least 1% wide and high."
                           : (!calibrationValid ? "Enter a positive width, height, and unit for the distance scale."
                                                : "No preview changes to apply.");
            ImGui::Spacing();
            ImGui::TextColored(
                hasPreviewChanges ? ImVec4(0.95f, 0.72f, 0.30f, 1.0f) : ImVec4(0.55f, 0.72f, 0.68f, 1.0f), "%s",
                localizedUiLabel(hasPreviewChanges ? "Preview only: Apply the settings to change the saved map."
                                                   : "These settings match the saved map.")
                    .c_str());
            const UiButtonPairResult calibrationActions = drawResponsiveButtonPair(
                "MapImageCalibrationActions", "Discard preview changes", UiButtonRole::Secondary,
                "Restore the crop, rotation, and distance scale stored on the map.", "Apply crop and scale",
                UiButtonRole::Primary,
                "Apply this reviewed crop, orientation, and optional distance scale as one undoable edit.",
                hasPreviewChanges && cropValid && calibrationValid, applyBlocker, hasPreviewChanges,
                "No preview changes to apply.");
            if (calibrationActions.first)
                _controller.syncReferenceCalibrationDraft();
            if (calibrationActions.second)
                _controller.applyReferenceCalibration();
            ImGui::TreePop();
        }
        if (drawUiButton("Remove image reference", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f),
                         "Remove the managed image from this map. Traced semantic features remain."))
            ImGui::OpenPopup(localizedUiLabel("Remove Map Image").c_str());
        if (drawMapConfirmationModal("Remove Map Image", "Remove the image reference but keep traced features?",
                                     "Remove"))
            _controller.removeReferenceImage();
    }

    ImGui::TreePop();
}
