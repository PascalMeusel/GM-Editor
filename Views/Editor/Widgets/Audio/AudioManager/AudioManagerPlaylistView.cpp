#include "AudioManagerViewSupport.hpp"

void AudioManagerView::drawPlaylistControls()
{
    drawSectionTitle("Playlists");

    TaskUi::fieldLabel(localizedUiText("New Playlist"));

    FormUi::InputText("##New Playlist", &_controller._newPlaylistName);
    if(ImGui::TreeNode(uiTextForLocale("Choose tracks before creating","Titel vor dem Erstellen auswählen"))) {
        for(const auto &asset:_controller._currentCampaign.audioAssets) {
            auto &queue=_controller._newPlaylistAssets;
            const auto found=std::find(queue.begin(),queue.end(),asset.metadata.id);bool selected=found!=queue.end();
            if(FormUi::Checkbox((asset.name+"##"+asset.metadata.id).c_str(),&selected)){if(selected)queue.push_back(asset.metadata.id);else queue.erase(found);}
        }
        ImGui::Text("%d %s",int(_controller._newPlaylistAssets.size()),uiTextForLocale("tracks selected · playback stays stopped","Titel gewählt · Wiedergabe bleibt gestoppt"));
        ImGui::TreePop();
    }
    TaskUi::sameLineIfFits();
    if (drawUiButton("Create Playlist", UiButtonRole::Primary, ImVec2(128.0f, 0.0f), "Create a playlist container for ordered or shuffled playback."))
        _controller.createPlaylist();

    const bool playlistWide = TaskUi::hasRoomForColumns(70.0f);
    if (playlistWide) ImGui::Columns(2, "AudioPlaylistColumns", true);
    ImGui::BeginChild("AudioPlaylistList", ImVec2(0.0f, TaskUi::listHeight(playlistWide ? 0.5f : 0.25f)), true);
    if (_controller._currentCampaign.audioPlaylists.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No playlists yet.").c_str());
    }
    else
    {
        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.audioPlaylists.size()); ++i)
        {
            const AudioPlaylistModel &playlist = _controller._currentCampaign.audioPlaylists[static_cast<size_t>(i)];
            const bool selected = i == _controller._selectedPlaylistIndex;
            if (ImGui::Selectable(playlist.name.c_str(), selected))
                _controller.selectPlaylist(i);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%d queued assets", static_cast<int>(playlist.audioAssetIds.size()));
        }
    }
    ImGui::EndChild();

    if (playlistWide) ImGui::NextColumn();
    AudioPlaylistModel *playlist = _controller.getSelectedPlaylist();
    if (!playlist)
    {
        ImGui::TextWrapped(localizedUiLabel("Select or create a playlist to edit its queue.").c_str());
        if (playlistWide) ImGui::Columns(1);
        return;
    }

    const PlaylistQueueSummary queueSummary = summarizePlaylistQueue(_controller, *playlist);
    const int activeQueueCount = _controller.activePlaylistQueueCount();
    const AudioAssetModel *currentQueuedAsset = _controller.getCurrentQueuedAsset();
    const AudioAssetModel *previousQueuedAsset = _controller.getPreviousQueuedAsset();
    const AudioAssetModel *nextQueuedAsset = _controller.getNextQueuedAsset();
    const AudioAssetModel *lastQueuedAsset = _controller.getLastQueuedAsset();
    const std::vector<EntityId> activeIds = _controller._playbackService->activeAssetIds();
    const std::string queueDisabled = playlistDisabledReason(queueSummary);
    const bool hasPlayableQueueItems = queueSummary.playableItems > 0;

    std::string mode = "Queue " + std::to_string(queueSummary.playableItems) + "/" + std::to_string(queueSummary.totalItems) + " playable";
    mode += " | Active " + std::to_string(activeQueueCount);

    std::string selectionState = currentQueuedAsset ? "Current: " + audioDisplayName(*currentQueuedAsset) : "Current: none";
    if (!currentQueuedAsset && lastQueuedAsset)
        selectionState = "Last: " + audioDisplayName(*lastQueuedAsset);
    selectionState += previousQueuedAsset ? " | Previous: " + audioDisplayName(*previousQueuedAsset) : " | Previous: none";
    selectionState += nextQueuedAsset ? " | Next: " + audioDisplayName(*nextQueuedAsset) : " | Next: none";

    std::string flags = "Shuffle " + std::string(onOffLabel(playlist->shuffle)) +
                        " | Repeat " + onOffLabel(playlist->repeat) +
                        " | One-Shot " + onOffLabel(playlist->oneShot) +
                        " | Crossfade " + formatSeconds(playlist->crossfadeSeconds);
    UiStatusActionStrip playlistStrip;
    playlistStrip.mode = mode.c_str();
    playlistStrip.selection = selectionState.c_str();
    playlistStrip.nextAction = flags.c_str();
    playlistStrip.help = "Current and next are based on the active queue when known. Play Queue and Scene Transition rebuild the queue, so shuffle may choose a new first item.";
    ImGui::TextWrapped("%s", playlist->name.c_str());

    if (queueSummary.missingAssets > 0 || queueSummary.missingFiles > 0)
    {
        std::string issue = "Queue issues:";
        if (queueSummary.missingAssets > 0)
            issue += " " + std::to_string(queueSummary.missingAssets) + " missing reference" + (queueSummary.missingAssets == 1 ? "" : "s") + ".";
        if (queueSummary.missingFiles > 0)
            issue += " " + std::to_string(queueSummary.missingFiles) + " missing file" + (queueSummary.missingFiles == 1 ? "" : "s") + ".";
        ImGui::TextColored(ImVec4(0.95f, 0.64f, 0.28f, 1.0f), localizedUiLabel("%s").c_str(), issue.c_str());
    }

    if (ImGui::TreeNode(uiTextForLocale("Edit playlist", "Wiedergabeliste bearbeiten")))
    {
    TaskUi::fieldLabel(localizedUiText("Playlist Name"));

    FormUi::InputText("##Playlist Name", &playlist->name);
    FormUi::Checkbox(localizedUiLabel("Shuffle").c_str(), &playlist->shuffle);
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(localizedUiLabel("Repeat").c_str(), &playlist->repeat);
    TaskUi::sameLineIfFits();
    FormUi::Checkbox(localizedUiLabel("One-Shot").c_str(), &playlist->oneShot);
    TaskUi::fieldLabel(localizedUiText("Crossfade"));
    FormUi::SliderFloat("##Crossfade", &playlist->crossfadeSeconds, 0.0f, 12.0f, "%.1fs");
    const std::string actionFadeLabel = formatSeconds(_controller._fadeSeconds);
    drawInlineLabelValue("Scene Transition Fade", actionFadeLabel.c_str());
    drawInlineHelpMarker("Scene Transition currently uses the shared Fade Seconds value from Library playback controls. The playlist Crossfade value is still saved with this playlist.", "?", "PlaylistSceneTransitionFade");

    const AudioAssetModel *selectedAsset = _controller.getSelectedAsset();
    if (drawUiButton("Add Selected Asset",
                     selectedAsset != nullptr,
                     "Select an audio asset in the Library tab before adding it to this playlist.",
                     UiButtonRole::Primary,
                     ImVec2(142.0f, 0.0f),
                     "Append the selected Library asset to this playlist."))
        _controller.addSelectedAssetToPlaylist();
    TaskUi::sameLineIfFits();
    if (drawUiButton("Save Playlist", UiButtonRole::Secondary, ImVec2(112.0f, 0.0f), "Save playlist name, playback flags, and queue order."))
        _controller.saveSelectedPlaylist();
    TaskUi::sameLineIfFits();
    if (drawUiButton("Delete Playlist", UiButtonRole::Destructive, ImVec2(116.0f, 0.0f), "Remove this playlist. Audio assets stay in the campaign library."))
        ImGui::OpenPopup(localizedUiLabel("Delete Playlist").c_str());
    if (drawConfirmationModal("Delete Playlist", "Delete this playlist and its queue?"))
    {
        _controller.removeSelectedPlaylist();
        ImGui::TreePop();
        if (playlistWide) ImGui::Columns(1);
        return;
    }

    ImGui::TreePop();
    }
    ImGui::BeginChild("AudioPlaylistQueue", ImVec2(0.0f, 154.0f), true);
    if (playlist->audioAssetIds.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("Queue is empty.").c_str());
    }
    else if (ImGui::BeginTable("AudioPlaylistQueueTable", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Queue").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(localizedUiLabel("Type").c_str(), ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Availability").c_str(), ImGuiTableColumnFlags_WidthFixed, 104.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Move").c_str(), ImGuiTableColumnFlags_WidthFixed, 82.0f);
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(playlist->audioAssetIds.size()); ++i)
        {
            const AudioAssetModel *asset = _controller.findAudioAssetById(playlist->audioAssetIds[static_cast<size_t>(i)]);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), asset ? asset->name.c_str() : "(missing asset)");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(localizedUiText(asset ? audioAssetTypeLabel(asset->type) : "Missing"));
            ImGui::TableSetColumnIndex(2);
            if (!asset)
                ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("Missing ref").c_str());
            else if (!isPlaylistAssetPlayable(asset))
                ImGui::TextColored(ImVec4(0.95f, 0.64f, 0.28f, 1.0f), localizedUiLabel("Missing file").c_str());
            else if (_controller._playbackService->isActive(asset->metadata.id))
                ImGui::TextColored(ImVec4(0.42f, 0.86f, 0.58f, 1.0f), localizedUiLabel("Active").c_str());
            else
                ImGui::TextUnformatted(localizedUiLabel("Ready").c_str());
            ImGui::TableSetColumnIndex(3);
            const std::string upId = "^##PlaylistAssetUp" + std::to_string(i);
            const std::string downId = "v##PlaylistAssetDown" + std::to_string(i);
            if (drawSmallUiButton(upId.c_str(), UiButtonRole::IconOnly, "Move this playlist item earlier."))
                _controller.moveSelectedPlaylistAsset(i, -1);
            TaskUi::sameLineIfFits();
            if (drawSmallUiButton(downId.c_str(), UiButtonRole::IconOnly, "Move this playlist item later."))
                _controller.moveSelectedPlaylistAsset(i, 1);
            ImGui::TableSetColumnIndex(4);
            const std::string removeId = "Remove##PlaylistAsset" + std::to_string(i);
            const std::string removePopupId = "Remove Playlist Item##Confirm" + std::to_string(i);
            if (drawSmallUiButton(removeId.c_str(), UiButtonRole::Destructive, "Remove this playlist item after confirmation."))
                ImGui::OpenPopup(removePopupId.c_str());
            if (drawConfirmationModal(removePopupId.c_str(), "Remove this item from the playlist?", "Remove"))
            {
                _controller.removeAssetFromSelectedPlaylist(i);
                break;
            }
        }

        ImGui::EndTable();
    }
    ImGui::EndChild();

    const std::string firstPlayableName = queueSummary.firstPlayable ? audioDisplayName(*queueSummary.firstPlayable) : "no playable item";
    const std::string playQueuePreview = playlist->shuffle
                                             ? "Rebuilds a shuffled queue and starts its first playable item."
                                             : "Rebuilds the listed queue and starts " + firstPlayableName + ".";
    const std::string oneShotPreview = playlist->shuffle
                                           ? "Builds a one-item shuffled queue and plays that item without looping."
                                           : "Builds a one-item queue and plays " + firstPlayableName + " without looping.";
    const std::string previousPreview = previousQueuedAsset ? "Plays the previous queued item: " + audioDisplayName(*previousQueuedAsset) + "." : "No previous item is available; repeat is off or the queue is empty.";
    const std::string nextPreview = nextQueuedAsset ? "Plays the next queued item: " + audioDisplayName(*nextQueuedAsset) + "." : "No next item is available; repeat is off or the queue is empty.";
    const std::string stopPreview = "Stops " + std::to_string(activeQueueCount) + " active queued channel" + (activeQueueCount == 1 ? "" : "s") + " and clears the current queue position.";
    const std::string scenePreview = buildSceneTransitionPreview(_controller, *playlist, queueSummary.firstPlayable, activeIds, _controller._fadeSeconds);

    if (ImGui::TreeNode(uiTextForLocale("Playback details", "Wiedergabedetails")))
    {
    if (ImGui::BeginTable("AudioPlaylistActionPreviewTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Action").c_str(), ImGuiTableColumnFlags_WidthFixed, 118.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Will Do").c_str(), ImGuiTableColumnFlags_WidthStretch);

        auto drawActionRow = [](const char *action, const std::string &description) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(action);
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(description);
        };

        drawActionRow("Play Queue", playQueuePreview);
        drawActionRow("Play One-Shot", oneShotPreview);
        drawActionRow("Previous", previousPreview);
        drawActionRow("Next", nextPreview);
        drawActionRow("Stop Queue", stopPreview);
        drawActionRow("Scene Transition", scenePreview);
        ImGui::EndTable();
    }

    ImGui::TreePop();
    }
    if (drawUiButton("Play Queue", hasPlayableQueueItems, queueDisabled.c_str(), UiButtonRole::Primary, ImVec2(92.0f, 0.0f), playQueuePreview.c_str()))
        _controller.playSelectedPlaylist();
    TaskUi::sameLineIfFits();
    if (drawUiButton("Play One-Shot", hasPlayableQueueItems, queueDisabled.c_str(), UiButtonRole::Secondary, ImVec2(108.0f, 0.0f), oneShotPreview.c_str()))
        _controller.playSelectedPlaylistOneShot();
    TaskUi::sameLineIfFits();
    if (drawUiButton("Scene Transition", hasPlayableQueueItems, queueDisabled.c_str(), UiButtonRole::Advanced, ImVec2(128.0f, 0.0f), scenePreview.c_str()))
        ImGui::OpenPopup(localizedUiLabel("Scene Transition Preview").c_str());
    if (drawConfirmationModal("Scene Transition Preview", scenePreview.c_str(), "Transition"))
        _controller.sceneTransitionToSelectedPlaylist();
    TaskUi::sameLineIfFits();
    const bool canPlayPrevious = previousQueuedAsset != nullptr;
    const std::string previousDisabled = _controller._playlistQueue.empty() ? "Start or rebuild the playlist queue first." : "Queue is at the start and repeat is off.";
    if (drawUiButton("Previous", canPlayPrevious, previousDisabled.c_str(), UiButtonRole::Secondary, ImVec2(84.0f, 0.0f), previousPreview.c_str()))
        _controller.playPreviousQueuedAsset();
    TaskUi::sameLineIfFits();
    const bool canPlayNext = nextQueuedAsset != nullptr;
    const std::string nextDisabled = _controller._playlistQueue.empty() ? "Start or rebuild the playlist queue first." : "Queue is at the end and repeat is off.";
    if (drawUiButton("Next", canPlayNext, nextDisabled.c_str(), UiButtonRole::Secondary, ImVec2(62.0f, 0.0f), nextPreview.c_str()))
        _controller.playNextQueuedAsset();
    TaskUi::sameLineIfFits();
    const bool canStopQueue = !_controller._playlistQueue.empty() && (activeQueueCount > 0 || _controller._queuedAssetIndex >= 0);
    const std::string stopDisabled = _controller._playlistQueue.empty() ? "No playlist queue is built." : "No queued playlist item is currently active or selected.";
    if (drawUiButton("Stop Queue", canStopQueue, stopDisabled.c_str(), UiButtonRole::Destructive, ImVec2(94.0f, 0.0f), stopPreview.c_str()))
        _controller.stopPlaylistQueue();

    if (!_controller._playlistQueue.empty())
    {
        ImGui::Text(localizedUiLabel("Active queue: %d / %d").c_str(),
                    _controller._queuedAssetIndex < 0 ? 0 : _controller._queuedAssetIndex + 1,
                    static_cast<int>(_controller._playlistQueue.size()));
    }

    if (playlistWide) ImGui::Columns(1);
}
