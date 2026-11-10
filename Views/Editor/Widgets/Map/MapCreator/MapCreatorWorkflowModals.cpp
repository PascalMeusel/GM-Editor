#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorWorkflowFrame.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawWorkflowConfirmationModals(const WorkflowFrame &frame)
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
    const std::string applyRegenerationPopup = localizedUiLabel("Apply regeneration changes");
            if (_openPartialRegenerationConfirmation)
            {
                ImGui::OpenPopup(applyRegenerationPopup.c_str());
                _openPartialRegenerationConfirmation = false;
            }
            if (ImGui::BeginPopupModal(applyRegenerationPopup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    "%s",
                    localizedUiLabel(
                        "Apply the reviewed change as one undoable edit? The current map will receive a recovery snapshot first.")
                        .c_str());
                if (drawModalActionButtons("Apply changes", UiButtonRole::Primary))
                    _controller.applyPartialRegeneration();
                ImGui::EndPopup();
            }

            const std::string restoreCheckpointPopup = localizedUiLabel("Restore map checkpoint");
            if (_openCheckpointRestoreConfirmation)
            {
                ImGui::OpenPopup(restoreCheckpointPopup.c_str());
                _openCheckpointRestoreConfirmation = false;
            }
            if (ImGui::BeginPopupModal(restoreCheckpointPopup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                const MapCheckpointRestorePreview &preview = _controller._history.namedCheckpointRestorePreview;
                ImGui::TextWrapped(
                    "%s",
                    localizedUiLabel("Restore this named map state? The current state remains available through Undo.")
                        .c_str());
                if (preview.valid)
                {
                    drawInlineLabelValue("Checkpoint", preview.checkpoint.name.c_str());
                    drawInlineLabelValue("Map", preview.package.mapName.c_str());
                    drawInlineLabelValue("Revision", std::to_string(preview.checkpoint.sourceRevision).c_str());
                }
                const UiModalAction action = drawModalActionButtonsEx("Restore checkpoint", UiButtonRole::Primary);
                if (action == UiModalAction::Confirm)
                    _controller.restoreNamedCheckpoint();
                else if (action == UiModalAction::Cancel)
                    _controller.discardNamedCheckpointRestorePreview();
                ImGui::EndPopup();
            }

            setResponsiveWindowSize(ImVec2(760.0f, 680.0f), ImVec2(400.0f, 340.0f));
            if (ImGui::BeginPopupModal(localizedUiLabel("Review Map Package").c_str(), nullptr,
                                       ImGuiWindowFlags_NoSavedSettings))
            {
                const MapPackagePreview &preview = _controller._transfer.mapPackagePreview;
                const bool canImport = mapCreatorMapPackageCanImport(preview.valid, preview.importEligible);
                const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2.4f;
                if (ImGui::BeginChild("MapPackageReviewBody",
                                      ImVec2(0.0f, std::max(190.0f, ImGui::GetContentRegionAvail().y - footerHeight)),
                                      false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
                {
                    ImGui::TextWrapped(
                        "%s",
                        localizedUiLabel(
                            "Import this package as an independent copy? The current draft changes only after confirmation and can be undone.")
                            .c_str());
                    if (canImport)
                    {
                        ImGui::TextColored(
                            ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s",
                            localizedUiLabel("Ready to import. Package contents and embedded assets passed validation.")
                                .c_str());
                    }
                    else
                    {
                        ImGui::TextColored(
                            ImVec4(0.95f, 0.40f, 0.30f, 1.0f), "%s",
                            localizedUiLabel(
                                preview.valid
                                    ? "Import blocked because the package contains conflicting internal identities."
                                    : "Import blocked because the package did not pass validation.")
                                .c_str());
                    }

                    if (preview.valid)
                    {
                        ImGui::SeparatorText(localizedUiLabel("Package overview").c_str());
                        drawInlineLabelValue("Map", preview.mapName.empty() ? localizedUiText("Unnamed draft")
                                                                            : preview.mapName.c_str());
                        drawInlineLabelValue("Map type",
                                             localizedUiText(mapCreationIntentLabel(preview.creationIntent)));
                        const std::string dimensions =
                            std::to_string(preview.width) + " × " + std::to_string(preview.height);
                        drawInlineLabelValue("Dimensions", dimensions.c_str());
                        const std::string packageSize = formatMapPackageByteSize(preview.packageBytes);
                        drawInlineLabelValue("Package size", packageSize.c_str());
                        const std::string assetCount =
                            gm::localization::formatInteger(static_cast<std::int64_t>(preview.embeddedAssetCount),
                                                            gm::localization::LocalizationService::instance().locale());
                        drawInlineLabelValue("Embedded assets", assetCount.c_str());
                        const std::string assetSize = formatMapPackageByteSize(preview.embeddedAssetBytes);
                        drawInlineLabelValue("Embedded asset size", assetSize.c_str());

                        ImGui::SeparatorText(localizedUiLabel("Editable content").c_str());
                        const auto drawCount = [](const char *label, std::size_t count) {
                            const std::string value = gm::localization::formatInteger(
                                static_cast<std::int64_t>(count),
                                gm::localization::LocalizationService::instance().locale());
                            drawInlineLabelValue(label, value.c_str());
                        };
                        drawCount("Generated features", preview.generatedFeatureCount);
                        drawCount("Manual notes and markers", preview.manualFeatureCount);
                        drawCount("Regions", preview.regionCount);
                        drawCount("Routes", preview.routeCount);
                        drawCount("Tokens", preview.tokenCount);
                        drawCount("Layers", preview.layerCount);

                        ImGui::SeparatorText(localizedUiLabel("Versions").c_str());
                        const std::string packageVersion =
                            mapPackageVersionLabel(static_cast<int>(preview.formatVersion));
                        const std::string generatorVersion = mapPackageVersionLabel(preview.generatorVersion);
                        const std::string recipeVersion = mapPackageVersionLabel(preview.recipeVersion);
                        const std::string styleVersion = mapPackageVersionLabel(preview.recipeStyleVersion);
                        const std::string graphVersion = mapPackageVersionLabel(preview.graphSchemaVersion);
                        drawInlineLabelValue("Package format", packageVersion.c_str());
                        drawInlineLabelValue("Map generator", generatorVersion.c_str());
                        drawInlineLabelValue("Generation recipe", recipeVersion.c_str());
                        drawInlineLabelValue("Art style", styleVersion.c_str());
                        drawInlineLabelValue("Editable map data", graphVersion.c_str());
                        drawDisabledWrappedText(
                            "Internal recipe and snapshot identifiers are preserved in the package but hidden from this normal review.");

                        ImGui::SeparatorText(localizedUiLabel("Player visibility").c_str());
                        drawInlineLabelValue("Map visibility", localizedUiText(mapPackageVisibilityLabel(preview)));
                        const std::string releaseCount =
                            gm::localization::formatInteger(
                                static_cast<std::int64_t>(preview.enabledPlayerReleaseCategoryCount),
                                gm::localization::LocalizationService::instance().locale()) +
                            " / 5";
                        drawInlineLabelValue("Optional player categories enabled", releaseCount.c_str());
                        if (preview.enabledPlayerReleaseCategoryCount == 0)
                        {
                            drawDisabledWrappedText(
                                "Terrain, coasts, and water remain available to the reviewed player view; all optional map details start hidden.");
                        }
                        else
                        {
                            ImGui::TextDisabled("%s", localizedUiLabel("Enabled for player-safe output").c_str());
                            if (preview.playerRelease.politicalBorders)
                                ImGui::BulletText("%s", localizedUiLabel("Political borders").c_str());
                            if (preview.playerRelease.waysAndRoutes)
                                ImGui::BulletText("%s", localizedUiLabel("Ways and routes").c_str());
                            if (preview.playerRelease.placesAndMarkers)
                                ImGui::BulletText("%s", localizedUiLabel("Places and markers").c_str());
                            if (preview.playerRelease.labels)
                                ImGui::BulletText("%s", localizedUiLabel("Labels").c_str());
                            if (preview.playerRelease.tokensAndAnnotations)
                                ImGui::BulletText("%s", localizedUiLabel("Tokens and annotations").c_str());
                        }

                        ImGui::SeparatorText(localizedUiLabel("Atlas import effect").c_str());
                        drawInlineLabelValue("Source atlas state",
                                             localizedUiText(mapPackageAtlasStatusLabel(preview.sourceAtlasStatus)));
                        if (preview.independentCopyDetachesAtlasParent)
                        {
                            ImGui::TextWrapped(
                                "%s",
                                localizedUiLabel(
                                    "This package references a parent map. The imported independent copy will be detached from that parent.")
                                    .c_str());
                        }
                        else
                        {
                            drawDisabledWrappedText("This package has no parent-map link. It will remain standalone.");
                        }
                        if (preview.sourceAtlasParentConflict)
                        {
                            ImGui::TextColored(
                                ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "%s",
                                localizedUiLabel(
                                    "The source contains inconsistent parent information. Independent import removes that link safely.")
                                    .c_str());
                        }
                        if (preview.independentCopyNormalizesAtlasStatus)
                        {
                            drawDisabledWrappedText("The imported copy receives a clean standalone atlas state.");
                        }
                        drawCount("Inherited feature links checked", preview.sourceFeatureLineageCount);
                        drawCount("External inherited features detached", preview.independentCopyDetachedLineageCount);
                        drawCount("Internal inherited features retained",
                                  preview.independentCopyRetainedInternalLineageCount);
                        if (preview.independentCopyDetachedLineageCount > 0)
                        {
                            drawDisabledWrappedText(
                                "Detached inherited features become editable in the independent copy. Internal links within this map remain connected.");
                        }

                        ImGui::SeparatorText(localizedUiLabel("Safety checks").c_str());
                        if (preview.embeddedAssetIntegrityVerified && preview.embeddedAssetConflictCount == 0)
                        {
                            ImGui::TextColored(
                                ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s",
                                localizedUiLabel(
                                    "All embedded asset hashes and package-internal asset roles are valid.")
                                    .c_str());
                        }
                        else
                        {
                            ImGui::TextColored(
                                ImVec4(0.95f, 0.40f, 0.30f, 1.0f), "%s",
                                localizedUiLabel("Embedded asset validation found a conflict. Import remains blocked.")
                                    .c_str());
                        }
                        if (preview.duplicateOwnedIdCount == 0)
                        {
                            ImGui::TextColored(
                                ImVec4(0.35f, 0.78f, 0.66f, 1.0f), "%s",
                                localizedUiLabel("No conflicting internal identities were found.").c_str());
                        }
                        else
                        {
                            const std::string conflictCount = gm::localization::formatInteger(
                                static_cast<std::int64_t>(preview.duplicateOwnedIdCount),
                                gm::localization::LocalizationService::instance().locale());
                            drawInlineLabelValue("Conflicting internal identities", conflictCount.c_str());
                        }
                        drawCount("Identity references checked", preview.ownedIdFieldCount);
                        drawCount("Unique internal identities", preview.uniqueOwnedIdCount);
                        drawCount("New internal identities on import", preview.independentCopyIdRemapCount);
                        drawCount("Managed image identities renewed", preview.independentCopyAssetIdRemapCount);
                        drawCount("Package-internal asset conflicts", preview.embeddedAssetConflictCount);
                        drawDisabledWrappedText(
                            "Only package-internal conflicts are shown here. Destination file conflicts are handled transactionally during import.");

                        if (!preview.assets.empty() &&
                            ImGui::CollapsingHeader(localizedUiLabel("Embedded asset details").c_str()))
                        {
                            for (std::size_t assetIndex = 0; assetIndex < preview.assets.size(); ++assetIndex)
                            {
                                const MapPackageAssetPreview &asset = preview.assets[assetIndex];
                                ImGui::PushID(static_cast<int>(assetIndex));
                                ImGui::BulletText("%s", localizedUiLabel(mapPackageAssetKindLabel(asset.kind)).c_str());
                                if (!asset.sourceFileName.empty())
                                    ImGui::TextWrapped("%s", asset.sourceFileName.c_str());
                                const std::string size = formatMapPackageByteSize(asset.byteCount);
                                ImGui::TextDisabled("%s", size.c_str());
                                ImGui::PopID();
                            }
                        }
                    }
                    else if (!_controller._integration.statusMessage.empty())
                    {
                        ImGui::TextWrapped("%s", localizedUiLabel(_controller._integration.statusMessage.c_str()).c_str());
                    }
                }
                ImGui::EndChild();

                const int actionColumns = ImGui::GetContentRegionAvail().x >= 520.0f ? 2 : 1;
                if (ImGui::BeginTable("MapPackageReviewActions", actionColumns,
                                      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableNextColumn();
                    if (drawUiButton(
                            "Import independent copy", canImport,
                            preview.valid
                                ? "Conflicting internal identities must be repaired before this package can be imported."
                                : "The package must pass validation before it can be imported.",
                            UiButtonRole::Primary, ImVec2(-1.0f, 0.0f),
                            "Create an undoable standalone copy with renewed internal identities."))
                    {
                        const EntityId previousDraftId = map.metadata.id;
                        _controller.confirmMapPackageImport();
                        if (map.metadata.id != previousDraftId)
                        {
                            _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
                            setWorkflowStep(3);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::TableNextColumn();
                    if (drawUiButton("Cancel", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                                     "Close this package review without changing the map."))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndTable();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal(localizedUiLabel("Export Legacy Map Draft").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    localizedUiLabel("Export an incomplete legacy-compatible draft to the configured path?").c_str());
                drawInlineLabelValue("Draft", map.name.empty() ? localizedUiText("Unnamed draft") : map.name.c_str());
                drawInlineLabelValue("Path", _controller._transfer.importExportPath.empty()
                                                 ? localizedUiText("No path set")
                                                 : _controller._transfer.importExportPath.c_str());
                if (drawModalActionButtons("Export", UiButtonRole::Primary))
                {
                    _controller.exportDraftMapLegacy();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal(localizedUiLabel("Import Legacy Map Draft").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped(
                    localizedUiLabel(
                        "Import an old, incomplete map draft from the configured path? The current draft will be replaced.")
                        .c_str());
                drawInlineLabelValue("Path", _controller._transfer.importExportPath.empty()
                                                 ? localizedUiText("No path set")
                                                 : _controller._transfer.importExportPath.c_str());
                if (drawModalActionButtons("Import", UiButtonRole::Advanced))
                {
                    const EntityId previousDraftId = map.metadata.id;
                    _controller.importDraftMapLegacy();
                    if (map.metadata.id != previousDraftId)
                    {
                        _controller.setActiveTool(MapCreatorController::Tool::InspectSelect);
                        setWorkflowStep(3);
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
}
