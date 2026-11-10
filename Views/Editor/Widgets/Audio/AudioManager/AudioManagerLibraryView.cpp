#include "AudioManagerViewSupport.hpp"

void AudioManagerView::drawActiveChannels()
{
    drawSectionTitle("Active Channels");
    const std::vector<EntityId> activeIds = _controller._playbackService->activeAssetIds();
    if (drawUiButton("Stop All",
                     !activeIds.empty(),
                     "No active audio channels to stop.",
                     UiButtonRole::Destructive,
                     ImVec2(86.0f, 0.0f),
                     "Stop every active audio channel."))
        _controller.stopAllActiveAudio();

    if (activeIds.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No active audio channels.").c_str());
        return;
    }

    if (ImGui::BeginTable("ActiveAudioChannelsTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Channel").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn(localizedUiLabel("Asset").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(localizedUiLabel("State").c_str(), ImGuiTableColumnFlags_WidthFixed,
                                std::max(ImGui::CalcTextSize(localizedUiText("Paused")).x, ImGui::CalcTextSize(localizedUiText("Playing")).x));
        ImGui::TableSetupColumn("##Stop", ImGuiTableColumnFlags_WidthFixed, TaskUi::buttonWidth(localizedUiText("Stop")));
        ImGui::TableHeadersRow();

        for (const EntityId &assetId : activeIds)
        {
            const AudioAssetModel *asset = _controller.findAudioAssetById(assetId);
            const AudioAmbienceLayerModel *layer = asset ? nullptr : _controller.findAmbienceLayerById(assetId);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", localizedUiText(layer ? "Ambience Layer" : (asset ? audioAssetTypeLabel(asset->type) : "Unknown")));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), layer ? layer->name.c_str() : (asset ? asset->name.c_str() : assetId.c_str()));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(localizedUiText(_controller._playbackService->isPaused(assetId) ? "Paused" : "Playing"));
            ImGui::TableSetColumnIndex(3);
            const std::string stopId = "Stop##ActiveChannel" + assetId;
            if (drawSmallUiButton(stopId.c_str(), UiButtonRole::Secondary))
                _controller.stopActiveChannel(assetId);
        }

        ImGui::EndTable();
    }
}

void AudioManagerView::drawImportControls()
{
    drawSectionTitle("Import Audio");
    TaskUi::fieldLabel(localizedUiText("Source File"));
    FormUi::InputText("##Source File", &_controller._importSourcePath);
    TaskUi::sameLineIfFits();
    if (ImGui::Button(localizedUiLabel("Browse").c_str()))
        _controller.browseForAudioFile();

    TaskUi::fieldLabel(localizedUiText("Asset Name"));

    FormUi::InputText("##Asset Name", &_controller._importName);
    drawAudioTypeCombo(uiTextForLocale("Asset Type", "Audioart"), _controller._importType);
    TaskUi::fieldLabel(localizedUiText("Default Volume"));
    volumePercent("##Default Volume", _controller._importVolume);
    FormUi::Checkbox(localizedUiLabel("Loop by Default").c_str(), &_controller._importLoop);

    if (drawUiButton("Import to Campaign",
                     !_controller._importSourcePath.empty(),
                     "Choose or paste a source audio file before importing.",
                     UiButtonRole::Primary,
                     ImVec2(154.0f, 0.0f),
                     "Copy this audio file into the campaign and add it to the Library."))
        _controller.importAsset();
}

void AudioManagerView::drawBundledCatalog()
{
    drawSectionTitle("Bundled Audio Catalog");
    TaskUi::fieldLabel(localizedUiText("Catalog Search"));
    FormUi::InputText("##Catalog Search", &_controller._catalogSearch);

    const float catalogHeight = std::max(ImGui::GetFontSize() * 5.0f,
        ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() * 3.0f);
    ImGui::BeginChild("BundledAudioCatalog", ImVec2(0.0f, catalogHeight), true);
    if (_controller._catalogEntries.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No bundled audio catalog entries were found.").c_str());
        ImGui::EndChild();
        return;
    }

    const bool showMetadataColumns = TaskUi::hasRoomForColumns(48.0f);
    const float actionWidth = std::max(TaskUi::buttonWidth(localizedUiText("Add")),
                                      TaskUi::buttonWidth(uiTextForLocale("Added", "Hinzugefügt"))) + 8.0f;
    int matches = 0;
    if (ImGui::BeginTable("BundledAudioCatalogTable", showMetadataColumns ? 5 : 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Name").c_str(), ImGuiTableColumnFlags_WidthStretch);
        if (showMetadataColumns)
        {
            ImGui::TableSetupColumn(localizedUiLabel("Theme").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6.0f);
            ImGui::TableSetupColumn(localizedUiLabel("License").c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.0f);
        }
        ImGui::TableSetupColumn("##Add", ImGuiTableColumnFlags_WidthFixed, actionWidth);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(_controller._catalogEntries.size()); ++i)
        {
            const AudioCatalogEntry &entry = _controller._catalogEntries[static_cast<size_t>(i)];
            if (!_controller.catalogEntryMatchesFilter(entry))
                continue;
            ++matches;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), entry.name.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\n%s\n%s", entry.sourceTitle.c_str(), entry.author.c_str(), entry.sourceUrl.c_str());

            if (showMetadataColumns)
            {
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", entry.theme.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s", localizedUiText(audioAssetTypeLabel(entry.type)));
                ImGui::TableSetColumnIndex(3);
                ImGui::TextWrapped("%s", entry.license.c_str());
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::TextWrapped("%s · %s · %s", localizedUiText(audioAssetTypeLabel(entry.type)), entry.theme.c_str(), entry.license.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::TableSetColumnIndex(showMetadataColumns ? 4 : 1);
            const std::string buttonId = "Add##BundledAudio" + std::to_string(i);
            const bool alreadyAdded = _controller.catalogAssetAlreadyAdded(i);
            if (alreadyAdded)
                ImGui::BeginDisabled();
            const std::string actionLabel = alreadyAdded ? std::string(uiTextForLocale("Added", "Hinzugefügt")) + "##BundledAudio" + std::to_string(i) : buttonId;
            if (drawSmallUiButton(actionLabel.c_str(), UiButtonRole::Primary,
                                  alreadyAdded ? uiTextForLocale("Already in this campaign's library.", "Bereits in der Bibliothek dieser Kampagne.")
                                               : uiTextForLocale("Add this sound to the campaign library.", "Diesen Klang zur Kampagnenbibliothek hinzufügen.")))
                _controller.addCatalogAsset(i);
            if (alreadyAdded)
                ImGui::EndDisabled();
        }

        ImGui::EndTable();
    }
    if (matches == 0)
        ImGui::TextWrapped("%s", uiTextForLocale("No sounds match this search. Try a shorter name or clear the search.",
                                                "Keine passenden Klänge. Versuche einen kürzeren Namen oder leere die Suche."));
    ImGui::EndChild();
}

void AudioManagerView::drawAssetList()
{
    drawSectionTitle("Campaign Audio");
    ImGui::BeginChild("AudioAssetList", ImVec2(0.0f, TaskUi::listHeight(0.45f)), true);
    if (_controller._currentCampaign.audioAssets.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No sounds or music have been imported yet.").c_str());
        ImGui::EndChild();
        return;
    }

    for (int i = 0; i < static_cast<int>(_controller._currentCampaign.audioAssets.size()); ++i)
    {
        const AudioAssetModel &asset = _controller._currentCampaign.audioAssets[static_cast<size_t>(i)];
        const bool selected = i == _controller._selectedAssetIndex;
        const std::string label = asset.name.empty() ? asset.originalFileName : asset.name;
        if (ImGui::Selectable(label.c_str(), selected))
            _controller.selectAsset(i);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s", label.c_str(), localizedUiText(audioAssetTypeLabel(asset.type)));
    }
    ImGui::EndChild();
}

void AudioManagerView::drawSelectedAssetControls()
{
    AudioAssetModel *asset = _controller.getSelectedAsset();
    if (!asset)
    { ImGui::TextWrapped("%s", localizedUiText("Select an audio asset to control it.")); return; }

    ImGui::TextWrapped("%s", audioDisplayName(*asset).c_str());
    const std::filesystem::path resolvedPath = _controller.resolveAudioPath(*asset);
    asset->missingFile = !std::filesystem::exists(resolvedPath);
    if (asset->missingFile)
    {
        ImGui::TextWrapped("%s", uiTextForLocale("The audio file is missing. Relink it before playing.", "Die Audiodatei fehlt. Verknüpfe sie vor der Wiedergabe neu."));
        if (drawUiButton("Relink Missing File", UiButtonRole::Primary)) _controller.relinkSelectedAsset();
    }
    const bool active = _controller._playbackService->isActive(asset->metadata.id);
    const bool paused = _controller._playbackService->isPaused(asset->metadata.id);
    if (!active)
    {
        if (drawUiButton("Play", !asset->missingFile, "Relink the missing audio file before playback.", UiButtonRole::Primary))
            _controller.playSelectedAsset();
    }
    else
    {
        if (drawUiButton(paused ? "Resume" : "Pause", UiButtonRole::Primary))
        { if (paused) _controller.resumeActiveAsset(); else _controller.pauseActiveAsset(); }
        TaskUi::sameLineForButton(localizedUiText("Stop"));
        if (drawUiButton("Stop", UiButtonRole::Secondary)) _controller.stopActiveAsset();
    }
    TaskUi::fieldLabel(localizedUiText("Volume"));
    if (volumePercent("##SelectedAudioVolume", _controller._selectedVolume))
        _controller.applySelectedVolume();
    FormUi::Checkbox(localizedUiText("Loop"), &_controller._selectedLoop);
    if (drawUiButton("Save Settings", UiButtonRole::Secondary)) _controller.saveSelectedAssetSettings();

    if (ImGui::TreeNode(uiTextForLocale("Transitions", "Übergänge")))
    {
        TaskUi::fieldLabel(localizedUiText("Fade Seconds"));
        FormUi::SliderFloat("##FadeSeconds", &_controller._fadeSeconds, 0.0f, 12.0f, "%.1fs");
        if (drawUiButton("Fade In", !asset->missingFile, "Relink the missing audio file before playback.")) _controller.fadeInSelectedAsset();
        TaskUi::sameLineForButton(localizedUiText("Fade Out"));
        if (drawUiButton("Fade Out", active, "Selected audio is not active.")) _controller.fadeOutSelectedAsset();
        TaskUi::sameLineForButton(localizedUiText("Crossfade"));
        if (drawUiButton("Crossfade", !asset->missingFile, "Relink the missing audio file before playback.")) _controller.crossfadeToSelectedAsset();
        ImGui::TreePop();
    }
    if (ImGui::TreeNode(uiTextForLocale("File and settings", "Datei und Einstellungen")))
    {
        TaskUi::fieldLabel(localizedUiText("Name"));
        FormUi::InputText("##AudioName", &asset->name);
        TaskUi::fieldLabel(localizedUiText("Type"));
        drawAudioTypeCombo("##AudioType", asset->type);
        drawInlineLabelValue("Last Played", asset->lastPlayedAtUtc.empty() ? uiTextForLocale("Never", "Nie") : asset->lastPlayedAtUtc.c_str());
        ProjectAssetRepairPanel::AssetRepairPanelEntry repairEntry;
        repairEntry.kind = ProjectAssetImport::AssetKind::Audio; repairEntry.displayName = asset->name;
        repairEntry.storedPath = asset->filePath; repairEntry.resolvedPath = resolvedPath.string(); repairEntry.missing = asset->missingFile;
        if (ProjectAssetRepairPanel::draw("SelectedAudioAssetRepair", _controller._assetRepairPanelState, repairEntry))
            _controller.relinkSelectedAssetFromPath(_controller._assetRepairPanelState.replacementPath);
        if (drawUiButton("Save Settings##AudioDetails", UiButtonRole::Primary)) _controller.saveSelectedAssetSettings();
        if (drawUiButton("Remove from Campaign", UiButtonRole::Destructive)) ImGui::OpenPopup(localizedUiLabel("Remove Audio Asset").c_str());
        if (drawConfirmationModal("Remove Audio Asset", "Remove this audio asset from the campaign and every playlist that references it?", "Remove"))
            _controller.removeSelectedAsset();
        ImGui::TreePop();
    }
}
