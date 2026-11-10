#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

using namespace gm::map_creator_view_detail;

void MapCreatorView::drawPlayerReviewControls(bool generationBusy)
{
    MapModel &map = _controller._draftMap;

    ImGui::SeparatorText(uiTextForLocale("Visible to players", "Für Spieler sichtbar"));
    ImGui::TextWrapped(
        "%s",
        uiTextForLocale(
            "Choose map details, save, then inspect the preview. Private places and fog stay protected.",
            "Kartendetails wählen, speichern und die Vorschau prüfen. Private Orte und Nebel bleiben geschützt."));
    bool mapVisible = map.visibility != MapVisibility::GameMasterOnly;
    if (FormUi::Checkbox(uiTextForLocale("Map available to players", "Karte für Spieler verfügbar"), &mapVisible))
    {
        _controller.captureUndoSnapshot("Change map visibility");
        map.visibility = mapVisible ? MapVisibility::PlayersVisible : MapVisibility::GameMasterOnly;
    }
    ImGui::BeginDisabled(!mapVisible);
    const auto category = [&](const char *english, const char *german, MapPlayerReleaseCategory value) {
        bool enabled = mapPlayerReleaseCategoryEnabled(map.playerRelease, value);
        if (FormUi::Checkbox(uiTextForLocale(english, german), &enabled))
        {
            _controller.captureUndoSnapshot("Change player map details");
            setMapPlayerReleaseCategory(map.playerRelease, value, enabled);
        }
    };
    category("Places", "Orte", MapPlayerReleaseCategory::PlacesAndMarkers);
    category("Names", "Namen", MapPlayerReleaseCategory::Labels);
    category("Roads and routes", "Straßen und Routen", MapPlayerReleaseCategory::WaysAndRoutes);
    category("Political borders", "Politische Grenzen", MapPlayerReleaseCategory::PoliticalBorders);
    category("Tokens and notes", "Figurenmarker und Notizen", MapPlayerReleaseCategory::TokensAndAnnotations);
    ImGui::EndDisabled();
    ImGui::Spacing();
    if (!_controller.savedDraftReadyForPlayerReview())
        ImGui::TextWrapped("%s",
                           uiTextForLocale("Save to apply these changes to the preview and export.",
                                           "Speichere, um diese Änderungen in Vorschau und Export zu übernehmen."));
    drawWorldPlaceSharingHelp(map);
}

void MapCreatorView::drawExportControls(bool generationBusy)
{
    MapModel &map = _controller._draftMap;

    ImGui::SeparatorText(localizedUiLabel("PNG output").c_str());
    if (_controller._transfer.exportPlayerSafe &&
        drawUiButton(uiTextForLocale("Change player visibility", "Spielersichtbarkeit ändern"), UiButtonRole::Secondary,
                     ImVec2(-1.0f, 0.0f)))
        setWorkflowStep(5);
    int audienceIndex = _controller._transfer.exportPlayerSafe ? 0 : 1;
    if (FormUi::Combo(localizedUiLabel("Copy for").c_str(), &audienceIndex,
                      localizedComboItems(pngExportAudienceItems).c_str()))
        _controller._transfer.exportPlayerSafe = audienceIndex == 0;

    ImGui::TextUnformatted(localizedUiLabel("Export area").c_str());
    if (FormUi::RadioButton(localizedUiLabel("Whole map").c_str(), !_controller._transfer.exportCurrentViewport))
        _controller._transfer.exportCurrentViewport = false;
    ImGui::SameLine();
    ImGui::BeginDisabled(!_controller._transfer.exportViewportValid);
    if (FormUi::RadioButton(localizedUiLabel("Current view").c_str(), _controller._transfer.exportCurrentViewport))
        _controller._transfer.exportCurrentViewport = true;
    ImGui::EndDisabled();
    if (!_controller._transfer.exportViewportValid)
        drawDisabledWrappedText("Open the map canvas once to make the current view available for export.");

    FormUi::Combo(localizedUiLabel("Output size").c_str(), &_controller._transfer.exportPngSizePresetIndex,
                  localizedComboItems(pngExportSizeItems).c_str());
    _controller._transfer.exportPngSizePresetIndex =
        std::clamp(_controller._transfer.exportPngSizePresetIndex, static_cast<int>(MapPngExportSizePreset::Screen),
                   static_cast<int>(MapPngExportSizePreset::Custom));
    if (_controller._transfer.exportPngSizePresetIndex == static_cast<int>(MapPngExportSizePreset::Custom))
        FormUi::SliderInt(localizedUiLabel("Detail per cell").c_str(), &_controller._transfer.exportPixelsPerCell,
                          kMapPngMinimumPixelsPerCell, kMapPngMaximumPixelsPerCell,
                          localizedUiLabel("%d px/cell").c_str());

    const MapPngExportSizing exportSizing = _controller.selectedPngExportSizing();
    if (exportSizing.valid)
    {
        const std::string resolution = formatLocalizedUi("%d x %d pixels at %d px/cell", exportSizing.widthPixels,
                                                         exportSizing.heightPixels, exportSizing.pixelsPerCell);
        ImGui::TextWrapped("%s", resolution.c_str());
        showHelpTooltip(formatMapExportMemory(exportSizing.uncompressedBytes).c_str());
        if (exportSizing.uncompressedBytes > 512ull * 1024ull * 1024ull)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.67f, 0.28f, 1.0f));
            ImGui::TextWrapped(
                "%s", localizedUiLabel(
                          "This is a very large PNG. Export can take longer and temporarily use additional memory.")
                          .c_str());
            ImGui::PopStyleColor();
        }
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.38f, 0.31f, 1.0f));
        ImGui::TextWrapped("%s", localizedUiLabel(exportSizing.error.c_str()).c_str());
        ImGui::PopStyleColor();
        drawDisabledWrappedText("Choose a smaller output size. A PNG edge may be at most 16384 pixels.");
    }

    if (ImGui::CollapsingHeader(uiTextForLocale("Layers and visibility", "Ebenen und Sichtbarkeit")))
    {
        MapCreatorController::PngExportVisibilitySettings &pngSettings =
            _controller._transfer.exportPlayerSafe ? _controller._transfer.exportPlayerSettings : _controller._transfer.exportGmSettings;
        ImGui::Spacing();
        ImGui::TextUnformatted(localizedUiLabel("Visible layers").c_str());
        const int optionColumns = ImGui::GetContentRegionAvail().x >= 430.0f ? 2 : 1;
        if (ImGui::BeginTable("PngVisibleLayers", optionColumns,
                              ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const auto option = [](const char *label, bool &value) {
                ImGui::TableNextColumn();
                FormUi::Checkbox(localizedUiLabel(label).c_str(), &value);
            };
            option("Grid", pngSettings.showGrid);
            option("Labels", pngSettings.showLabels);
            option("Political borders", pngSettings.showPoliticalOverlay);
            option("Annotations", pngSettings.showAnnotations);
            option("Routes", pngSettings.showRoutes);
            option("Tokens", pngSettings.showTokens);
            option("Imported image background", pngSettings.showBackground);
            ImGui::TableNextColumn();
            if (_controller._transfer.exportPlayerSafe)
            {
                bool forcedFog = true;
                ImGui::BeginDisabled();
                FormUi::Checkbox(localizedUiLabel("Apply fog").c_str(), &forcedFog);
                ImGui::EndDisabled();
            }
            else
                FormUi::Checkbox(localizedUiLabel("Apply fog").c_str(), &pngSettings.applyFog);
            ImGui::EndTable();
        }

        if (_controller._transfer.exportPlayerSafe)
        {
            drawDisabledWrappedText(
                "Player-safe export always applies fog and filters hidden, undiscovered, and GM-only content even when a layer is enabled.");
            drawWorldPlaceSharingHelp(map);
        }
        else
            drawDisabledWrappedText("The GM copy can include private preparation data. Fog is off by default.");
    }
    if (!_controller._transfer.exportPlayerSafe)
        drawDisabledWrappedText("The GM copy can include private preparation data. Fog is off by default.");

    if (!_controller._transfer.lastExportPath.empty())
    {
        ImGui::TextUnformatted(uiTextForLocale("Last successful export", "Letzte erfolgreiche Ausgabe"));
        ImGui::TextWrapped("%s", _controller._transfer.lastExportPath.c_str());
        if (drawSmallUiButton(uiTextForLocale("Copy file path", "Dateipfad kopieren"), UiButtonRole::Secondary))
            ImGui::SetClipboardText(_controller._transfer.lastExportPath.c_str());
    }
#if !defined(_WIN32)
    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "Enter or paste the complete PNG destination. Existing files are never replaced without a native confirmation.")
            .c_str());
    FormUi::InputTextWithHint(localizedUiLabel("PNG destination path").c_str(), "/path/to/map.png",
                              &_controller._transfer.manualPngExportPath);
#endif
    if (ImGui::CollapsingHeader(uiTextForLocale("Universal VTT / GM transfer", "Universal VTT / Spielleiter-Transfer")))
    {
        ImGui::TextWrapped("%s", uiTextForLocale(
            "Whole tactical square map, including hidden areas: image, grid, sight-blocking walls and door tiles. Uses the selected output detail (up to 4096 pixels per side). For another GM, not player sharing.",
            "Ganze taktische Rasterkarte, einschließlich verborgener Bereiche: Bild, Raster, Sichtblockaden und Türkacheln. Nutzt die gewählte Detailstufe (bis 4096 Pixel je Seite). Für andere Spielleiter, nicht zur Spielerfreigabe."));
        ImGui::TextWrapped("%s", uiTextForLocale(
            "Omits tokens, notes, labels, fog, lights, elevation, movement-only/one-way rules and region blockers. Door tiles start closed; edge-preset doors remain walls. Check these settings in the destination VTT.",
            "Ohne Figuren, Notizen, Namen, Nebel, Licht, Höhen, reine Bewegungs-/Einwegregeln und Regionsblockaden. Türkacheln beginnen geschlossen; Kantentüren bleiben Wände. Prüfe diese Einstellungen im Ziel-VTT."));
#if !defined(_WIN32)
        FormUi::InputText(uiTextForLocale("UVTT destination", "UVTT-Ziel"), &_controller._transfer.manualUvttExportPath);
#endif
        ImGui::BeginDisabled(generationBusy || _controller._transfer.pngExportTask.busy() ||
                             map.atlasLevel != MapLevel::Tactical || map.gridType != GridType::Square);
        if (drawUiButton(uiTextForLocale("Export Universal VTT", "Universal VTT exportieren"), UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f)))
            _controller.exportDraftUvtt();
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader(uiTextForLocale("Large maps / tiled export", "Große Karten / gekachelte Ausgabe")))
    {
        drawDisabledWrappedText(
            "Large output is split into PNG tiles plus a JSON manifest. Tiled export always uses the whole map.");
#if !defined(_WIN32)
        FormUi::InputTextWithHint(localizedUiLabel("Tiled PNG manifest path").c_str(), "/path/to/map_tiles.json",
                                  &_controller._transfer.manualTiledPngManifestPath);
#endif
        if (drawUiButton("Export as tiled PNGs", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f),
                         "Choose a destination. Existing files require confirmation."))
            _controller.exportDraftPngTiled(_controller._transfer.exportPlayerSafe, false);
        if (ImGui::TreeNode(localizedUiLabel("Advanced").c_str()))
        {
            if (drawUiButton("Replace existing tiled PNG export", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f),
                             "Choose a destination. Existing files require confirmation."))
                ImGui::OpenPopup(localizedUiLabel("Replace tiled PNG export?").c_str());
            ImGui::TreePop();
        }
    }
    if (ImGui::BeginPopupModal(localizedUiLabel("Replace tiled PNG export?").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s", localizedUiLabel("Existing tiled PNG files may be replaced. Continue?").c_str());
        if (drawModalActionButtons("Replace existing tiled PNG export", UiButtonRole::Destructive))
            _controller.exportDraftPngTiled(_controller._transfer.exportPlayerSafe, true);
        ImGui::EndPopup();
    }

    ImGui::SeparatorText(localizedUiLabel("Editable map package").c_str());
    ImGui::TextWrapped(
        "%s",
        localizedUiLabel(
            "A map package keeps the complete editable map, generated features, fog, tokens, image reference, and version information.")
            .c_str());
#if !defined(_WIN32)
    ImGui::TextWrapped("%s",
                       localizedUiLabel("Enter or paste a complete .ttrpgmappkg path for import or export.").c_str());
    FormUi::InputTextWithHint(localizedUiLabel("Map package path").c_str(), "/path/to/map.ttrpgmappkg",
                              &_controller._transfer.importExportPath);
#endif
    const UiButtonPairResult packageActions = drawResponsiveButtonPair(
        "MapPackageTransferActions", "Export map package", UiButtonRole::Secondary,
        "Choose or enter where to save a portable .ttrpgmappkg file.", "Import map package", UiButtonRole::Secondary,
        "Choose or enter a package and review its contents before replacing this draft.");
    if (packageActions.first)
        _controller.browseForMapPackageExport();
    if (packageActions.second)
    {
        _controller.browseForMapPackageImport();
        if (_controller._transfer.confirmMapPackageImport)
            ImGui::OpenPopup(localizedUiLabel("Review Map Package").c_str());
    }

    if (ImGui::CollapsingHeader(localizedUiLabel("Legacy map transfer").c_str()))
    {
        drawDisabledWrappedText(
            "Legacy .ttrpgmap files omit parts of the V5 map model. Use them only for older projects; .ttrpgmappkg is the normal format.");
        FormUi::InputText(localizedUiLabel("Import/Export Path").c_str(), &_controller._transfer.importExportPath);
        const UiButtonPairResult transferActions = drawResponsiveButtonPair(
            "MapDraftTransferActions", "Export legacy .ttrpgmap", UiButtonRole::Advanced,
            "Export an incomplete legacy-compatible draft.", "Import legacy .ttrpgmap", UiButtonRole::Advanced,
            "Import an old map draft from the configured path. The current draft will be replaced.");
        if (transferActions.first)
            ImGui::OpenPopup(localizedUiLabel("Export Legacy Map Draft").c_str());
        if (transferActions.second)
            ImGui::OpenPopup(localizedUiLabel("Import Legacy Map Draft").c_str());
    }
}

void MapCreatorView::drawExportPreview()
{
    MapModel &map = _controller._draftMap;

    const MapPngExportOptions options = _controller.selectedPngExportOptions(_controller._transfer.exportPlayerSafe);
    ImGui::SeparatorText(options.playerSafe ? uiTextForLocale("Player-safe PNG preview", "Vorschau der Spieler-PNG")
                                            : uiTextForLocale("GM PNG preview", "Vorschau der Spielleiter-PNG"));
    drawInlineLabelValue("Export area",
                         localizedUiText(_controller._transfer.exportCurrentViewport ? "Current view" : "Whole map"));
    const std::optional<MapModel> playerMap =
        options.playerSafe ? std::optional<MapModel>{_controller.currentPlayerOutputMap()} : std::nullopt;
    const MapModel &outputMap = playerMap ? *playerMap : map;
    MapRenderPlanRequest request = mapPngExportRenderPlanRequest(options);
    if (options.playerSafe)
        request.additionalGeneratedFeatureVisibility = [this](const MapGeneratedFeature &feature) {
            return mapCreatorPlayerOutputFeatureIsPublic(_controller._currentCampaign, _controller._draftMap, feature);
        };
    const MapPngExportRect view =
        _controller._transfer.exportCurrentViewport
            ? (_controller._transfer.exportViewportValid ? _controller._transfer.exportViewportRect : MapPngExportRect{})
            : MapPngExportRect{0, 0, outputMap.width, outputMap.height};
    const float previewHeight = std::clamp(ImGui::GetContentRegionAvail().y - 16.0f, 170.0f, 520.0f);
    drawGenerationMiniMapFitted(outputMap, _renderPlanCache, _controller._editing.authoringSerial,
                                "SelectedPngOutputPreview", previewHeight, &request, &_playerPreviewCache, &view);
}

void MapCreatorView::drawPlayerPreview(bool stackedLayout, bool generationBusy)
{
    // Compact layouts omit the separate control rail. Keep its release controls
    // reachable here, without consuming the entire preview with a tall form.
    if (stackedLayout)
    {
        const char *visibilityPopup = uiTextForLocale("Player map details###PlayerMapDetails",
                                                      "Spieler-Kartendetails###PlayerMapDetails");
        if (drawUiButton(uiTextForLocale("Player visibility…", "Spielersichtbarkeit…"),
                         UiButtonRole::Secondary))
            ImGui::OpenPopup(visibilityPopup);
        const char *visibilityStatus = _controller._draftMap.visibility == MapVisibility::GameMasterOnly
            ? uiTextForLocale("Hidden from players", "Für Spieler verborgen")
            : uiTextForLocale("Available to players", "Für Spieler verfügbar");
        if (ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - ImGui::GetItemRectMax().x >
            ImGui::CalcTextSize(visibilityStatus).x + 16.0f)
            ImGui::SameLine();
        ImGui::TextDisabled("%s", visibilityStatus);

        setResponsiveWindowSize(ImVec2(600.0f, 570.0f), ImVec2(360.0f, 340.0f));
        if (ImGui::BeginPopupModal(visibilityPopup, nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            ImGui::BeginChild("PlayerMapDetailsBody", ImVec2(0, std::max(100.0f, ImGui::GetContentRegionAvail().y - footer)));
            drawPlayerReviewControls(generationBusy);
            ImGui::TextWrapped("%s", uiTextForLocale(
                "Saving updates this preview. The separate player window stays hidden until you review and show it.",
                "Speichern aktualisiert diese Vorschau. Das separate Spielerfenster bleibt bis zur Prüfung und Freigabe verborgen."));
            ImGui::EndChild();
            if (drawUiButton(uiTextForLocale("Save and preview", "Speichern und Vorschau"), !generationBusy,
                             "Generation is already running.", UiButtonRole::Primary))
            {
                _controller.saveDraftMap();
                if (!_controller.hasUnsavedDraft()) ImGui::CloseCurrentPopup();
            }
            TaskUi::sameLineForButton(uiTextForLocale("Done", "Fertig"));
            if (drawUiButton(uiTextForLocale("Done", "Fertig"), UiButtonRole::Secondary))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    ImGui::SeparatorText(uiTextForLocale("Current player permissions", "Aktuelle Spielerfreigaben"));
    const MapModel playerMap = _controller.currentPlayerOutputMap();
    MapRenderPlanRequest request = gm::mapui::mapCreatorPlayerPreviewRequest(
        std::string(gm::localization::localeCode(gm::localization::LocalizationService::instance().locale())));
    request.additionalGeneratedFeatureVisibility = [this](const MapGeneratedFeature &feature) {
        return mapCreatorPlayerOutputFeatureIsPublic(_controller._currentCampaign, _controller._draftMap, feature);
    };
    drawGenerationInteractivePreview(
        playerMap, _renderPlanCache, _controller._editing.authoringSerial, "MapAuthoringPlayerPreview",
        std::max(180.0f, ImGui::GetContentRegionAvail().y -
                             (stackedLayout ? ImGui::GetFrameHeightWithSpacing() + 16.0f : 8.0f)),
        _playerPreviewZoom, true, false, _resetWorkflowScroll, &request, &_playerPreviewCache);
    if (stackedLayout)
    {
        const bool saved = _controller.savedDraftReadyForPlayerReview();
        if (drawUiButton(saved ? "Review player view" : "Save Map", !generationBusy, "Generation is already running.",
                         UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
        {
            if (saved)
                _controller.reviewSavedMapForPlayers();
            else
                _controller.saveDraftMap();
        }
    }
}
