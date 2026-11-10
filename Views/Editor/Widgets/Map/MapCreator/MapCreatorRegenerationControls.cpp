#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawPartialRegenerationControls()
{
    MapModel &map = _controller._draftMap;
    if (!ImGui::CollapsingHeader(localizedUiLabel("Regenerate part of map").c_str()))
        return;

    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "Choose one feature family. Nothing changes until you review and confirm the before-and-after preview.")
            .c_str());
    const char *familyItems = "Rivers\0Roads\0Places\0Biomes\0";
    FormUi::Combo(localizedUiLabel("Feature family").c_str(), &_regenerationFamilyIndex,
                  localizedComboItems(familyItems).c_str());
    FormUi::Checkbox(localizedUiLabel("Only inside a selected rectangle").c_str(), &_regenerationUseArea);
    if (_regenerationUseArea)
    {
        const std::string chooseAreaLabel =
            _regenerationPickingArea ? "Cancel area selection" : "Select rectangle on map";
        if (drawUiButton(chooseAreaLabel.c_str(), UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _regenerationPickingArea = !_regenerationPickingArea;
            _regenerationAreaDragging = false;
            if (_regenerationPickingArea)
                _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
        }
        const int minimumX = std::min(_regenerationAreaStartX, _regenerationAreaEndX);
        const int minimumY = std::min(_regenerationAreaStartY, _regenerationAreaEndY);
        const int maximumX = std::max(_regenerationAreaStartX, _regenerationAreaEndX);
        const int maximumY = std::max(_regenerationAreaStartY, _regenerationAreaEndY);
        const std::string area =
            formatLocalizedUi("Cells %d,%d to %d,%d", minimumX + 1, minimumY + 1, maximumX + 1, maximumY + 1);
        ImGui::TextDisabled("%s", area.c_str());
        if (_regenerationPickingArea)
            ImGui::TextWrapped("%s", localizedUiLabel("Drag over the map to choose the affected rectangle.").c_str());
    }

    const MapGenerationSelection selection = partialRegenerationSelection();
    const auto matchesFamily = [&](MapGeneratedFeatureKind kind) {
        return mapGenerationSelectionMatchesKind(selection, kind);
    };
    int familyFeatureCount = 0;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        familyFeatureCount += matchesFamily(feature.kind) ? 1 : 0;
    if (familyFeatureCount > 0 && ImGui::TreeNode(localizedUiLabel("Keep individual features").c_str()))
    {
        ImGui::TextWrapped(
            "%s", localizedUiLabel("Locked features remain unchanged even when they cross the selected area.").c_str());
        ImGui::BeginChild("PartialRegenerationLocks",
                          ImVec2(0.0f, std::min(154.0f, 32.0f + familyFeatureCount * 28.0f)), true);
        for (const MapGeneratedFeature &feature : map.generation.graph.features)
        {
            if (!matchesFamily(feature.kind))
                continue;
            ImGui::PushID(feature.id.c_str());
            bool locked = feature.locked || std::find(map.generation.recipe.lockedFeatureIds.begin(),
                                                      map.generation.recipe.lockedFeatureIds.end(),
                                                      feature.id) != map.generation.recipe.lockedFeatureIds.end();
            std::string displayName = feature.name;
            if (displayName.empty())
            {
                switch (feature.kind)
                {
                case MapGeneratedFeatureKind::River:
                    displayName = localizedUiLabel("River");
                    break;
                case MapGeneratedFeatureKind::Road:
                    displayName = localizedUiLabel("Road");
                    break;
                case MapGeneratedFeatureKind::Trail:
                    displayName = localizedUiLabel("Trail");
                    break;
                case MapGeneratedFeatureKind::Settlement:
                    displayName = localizedUiLabel("Settlement");
                    break;
                case MapGeneratedFeatureKind::Landmark:
                    displayName = localizedUiLabel("Landmark");
                    break;
                case MapGeneratedFeatureKind::Vegetation:
                    displayName = localizedUiLabel("Vegetation");
                    break;
                case MapGeneratedFeatureKind::MountainRange:
                    displayName = localizedUiLabel("Mountain range");
                    break;
                case MapGeneratedFeatureKind::WaterBody:
                    displayName = localizedUiLabel("Water body");
                    break;
                default:
                    displayName = localizedUiLabel("Generated feature");
                    break;
                }
            }
            if (FormUi::Checkbox(displayName.c_str(), &locked))
                _controller.setGeneratedFeatureLocked(feature.id, locked);
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::TreePop();
    }

    if (_controller._generation.hasPendingPatch)
    {
        const MapGenerationPatchPreview &patch = _controller._generation.pendingPatch;
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s", localizedUiLabel("Change preview ready").c_str());
        ImGui::TextWrapped("%s", formatLocalizedUi("%d removed · %d added · %d locked preserved",
                                                   static_cast<int>(patch.removedFeatureIds.size()),
                                                   static_cast<int>(patch.addedFeatureIds.size()),
                                                   static_cast<int>(patch.preservedLockedFeatureIds.size()))
                                     .c_str());
        const UiButtonPairResult actions =
            drawResponsiveButtonPair("PartialRegenerationPreviewActions", "Apply changes", UiButtonRole::Primary,
                                     "Apply this reviewed patch as one undoable map edit.", "Discard preview",
                                     UiButtonRole::Secondary, "Keep the current map and discard this preview.");
        if (actions.first)
            _openPartialRegenerationConfirmation = true;
        if (actions.second)
            _controller.discardPartialRegeneration();
    }
    else if (drawUiButton("Create change preview", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                          "Generate a replacement in the background without changing this map."))
    {
        _controller.queuePartialRegeneration(selection);
    }
}

void MapCreatorView::drawCheckpointControls()
{
    if (!ImGui::CollapsingHeader(localizedUiLabel("Map checkpoints").c_str()))
        return;
    ImGui::TextWrapped(
        "%s",
        localizedUiLabel("Name important map states before experiments. Up to 20 checkpoints are kept for this map.")
            .c_str());
    FormUi::InputTextWithHint("##MapCheckpointName", localizedUiLabel("Checkpoint name").c_str(),
                              &_controller._history.newCheckpointName);
    ImGui::BeginDisabled(_controller._history.newCheckpointName.empty());
    if (drawUiButton("Create checkpoint", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        _controller.createNamedCheckpoint(_controller._history.newCheckpointName);
    ImGui::EndDisabled();

    if (_controller._history.namedCheckpoints.empty())
    {
        ImGui::TextDisabled("%s", localizedUiLabel("No named checkpoints yet.").c_str());
        return;
    }

    ImGui::BeginChild("NamedMapCheckpoints", ImVec2(0.0f, 170.0f), true);
    for (int index = 0; index < static_cast<int>(_controller._history.namedCheckpoints.size()); ++index)
    {
        const MapCheckpointInfo &checkpoint = _controller._history.namedCheckpoints[static_cast<std::size_t>(index)];
        ImGui::PushID(checkpoint.id.c_str());
        ImGui::TextWrapped("%s", checkpoint.name.empty() ? localizedUiLabel("Unnamed checkpoint").c_str()
                                                         : checkpoint.name.c_str());
        ImGui::TextDisabled("%s", formatLocalizedUi("Revision %d · %s", static_cast<int>(checkpoint.sourceRevision),
                                                    checkpoint.createdAtUtc.c_str())
                                      .c_str());
        if (drawUiButton("Restore", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
        {
            _controller.previewNamedCheckpointRestore(index);
            if (_controller._history.hasNamedCheckpointRestorePreview)
                _openCheckpointRestoreConfirmation = true;
        }
        if (index + 1 < static_cast<int>(_controller._history.namedCheckpoints.size()))
            ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void MapCreatorView::drawPartialRegenerationPreview()
{
    const MapGenerationPatchPreview &patch = _controller._generation.pendingPatch;
    ImGui::SeparatorText(localizedUiLabel("Review map changes").c_str());
    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "The current map is shown beside the proposed result. Tokens, notes, fog, layers and locked features stay attached.")
            .c_str());
    const bool sideBySide = ImGui::GetContentRegionAvail().x >= 760.0f;
    if (ImGui::BeginTable("PartialRegenerationDiff", sideBySide ? 2 : 1,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(localizedUiLabel("Before").c_str());
        drawGenerationMiniMapFitted(patch.before, _renderPlanCache, (_controller._generation.requestSerial << 2u) ^ 1u,
                                    "PartialRegenerationBefore", sideBySide ? 420.0f : 290.0f);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(localizedUiLabel("After").c_str());
        drawGenerationMiniMapFitted(patch.after, _renderPlanCache, (_controller._generation.requestSerial << 2u) ^ 2u,
                                    "PartialRegenerationAfter", sideBySide ? 420.0f : 290.0f);
        ImGui::EndTable();
    }
    ImGui::TextWrapped("%s", formatLocalizedUi("%d removed · %d added · %d locked preserved",
                                               static_cast<int>(patch.removedFeatureIds.size()),
                                               static_cast<int>(patch.addedFeatureIds.size()),
                                               static_cast<int>(patch.preservedLockedFeatureIds.size()))
                                 .c_str());
    const UiButtonPairResult actions =
        drawResponsiveButtonPair("PartialRegenerationDiffActions", "Apply changes", UiButtonRole::Primary,
                                 "Apply this reviewed patch as one undoable map edit.", "Back to map",
                                 UiButtonRole::Secondary, "Discard the preview without changing the map.");
    if (actions.first)
        _openPartialRegenerationConfirmation = true;
    if (actions.second)
        _controller.discardPartialRegeneration();
}
