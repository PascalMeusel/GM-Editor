#include "Views/Editor/Widgets/UiAdaptiveFields.hpp"
#include "AudioQuickControlsView.hpp"

#include "Controllers/Editor/Widgets/AudioQuickControls/AudioQuickControlsController.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "imgui.h"
#include "Views/Editor/Widgets/UiTaskLayout.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
template <typename... Args>
std::string formatLocalizedUi(const char *englishFormat, Args... args)
{
    const std::string localizedFormat = localizedUiLabel(englishFormat);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), args...);
    if (required <= 0)
        return localizedFormat;

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1U, '\0');
    std::snprintf(buffer.data(), buffer.size(), localizedFormat.c_str(), args...);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

std::string audioDisplayName(const AudioAssetModel &asset)
{
    if (!asset.name.empty())
        return asset.name;
    if (!asset.originalFileName.empty())
        return asset.originalFileName;
    return localizedUiText("Untitled audio");
}
}

AudioQuickControlsView::AudioQuickControlsView(AudioQuickControlsController &controller)
    : DockableWidget("Audio Controls", 0, ImVec2(360.0f, 260.0f)), _controller(controller)
{
}

void AudioQuickControlsView::draw()
{
    if (_controller._saveFailed)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", _controller._saveErrorMessage.c_str());
        ImGui::PopStyleColor();
        if (drawUiButton("Retry Save", true, nullptr, UiButtonRole::Primary))
            _controller.retrySave();
    }

    if (_controller._currentCampaign.audioAssets.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No campaign audio has been imported yet.").c_str());
        return;
    }

    const AudioAssetModel *asset = _controller.getSelectedAsset();
    const bool active = _controller.isSelectedAssetActive();
    const bool paused = _controller.isSelectedAssetPaused();
    const std::vector<EntityId> activeIds = _controller._playbackService->activeAssetIds();
    const AudioQuickChannelSummary summary = _controller.channelSummary();
    const AudioQuickFileDiagnostics diagnostics = _controller.fileDiagnostics();
    const AudioQuickPlaylistSummary playlist = _controller.playlistSummary();
    const std::string mode = activeIds.empty()
                                 ? std::string(localizedUiText("Audio idle"))
                                 : formatLocalizedUi("Playing %d / paused %d", summary.playing, summary.paused);
    const std::string selection = asset
                                      ? formatLocalizedUi("Selected: %s", audioDisplayName(*asset).c_str())
                                      : std::string(localizedUiText("No selected audio"));
    const std::string nextAction = diagnostics.selectedMissing ? diagnostics.selectedMessage
                                  : (summary.paused > 0 ? localizedUiText("Resume paused audio after break.")
                                                        : (summary.playing > 0 ? localizedUiText("Pause all for break.") : (active ? localizedUiText("Pause or stop selected audio.") : localizedUiText("Play selected audio."))));
    UiStatusActionStrip strip;
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = nextAction.c_str();
    strip.help = "Selected controls affect one audio asset. Active playback actions affect every active channel.";
    if (diagnostics.selectedMissing) ImGui::TextWrapped("%s", diagnostics.selectedMessage.c_str());

    const std::string selectedLabel = asset ? audioDisplayName(*asset) : std::string(localizedUiText("Select audio"));
    if (FormUi::BeginCombo(localizedUiLabel("Audio").c_str(), selectedLabel.c_str()))
    {
        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.audioAssets.size()); ++i)
        {
            const AudioAssetModel &candidate = _controller._currentCampaign.audioAssets[static_cast<size_t>(i)];
            const std::string label = candidate.name.empty() ? candidate.originalFileName : candidate.name;
            const bool selected = i == _controller._selectedAssetIndex;
            if (ImGui::Selectable(label.c_str(), selected))
                _controller.selectAsset(i);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (FormUi::SliderFloat(localizedUiLabel("Volume").c_str(), &_controller._volume, 0.0f, 1.0f, "%.2f"))
        _controller.applySelectedVolume();
    FormUi::Checkbox(localizedUiLabel("Loop").c_str(), &_controller._loop);

    if (!active && drawUiButton("Play", asset != nullptr, "Select audio first.", UiButtonRole::Primary, ImVec2(70.0f, 0.0f)))
        _controller.playSelectedAsset();
    TaskUi::sameLineIfFits();
    if (active && drawUiButton(paused ? "Resume" : "Pause", active, "Selected audio is not active.", paused ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(82.0f, 0.0f)))
    {
        if (paused)
            _controller.resumeSelectedAsset();
        else
            _controller.pauseSelectedAsset();
    }
    TaskUi::sameLineIfFits();
    if (active && drawUiButton("Stop", active, "Selected audio is not active.", UiButtonRole::Secondary, ImVec2(70.0f, 0.0f)))
        _controller.stopSelectedAsset();

    if (asset && active)
        drawInlineLabelValue("State", localizedUiText(paused ? "Paused" : "Playing"));

    if (ImGui::TreeNode(localizedUiLabel("Quick Starts").c_str()))
    {
    if (ImGui::BeginTable("AudioMiniQuickStarts", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        if (drawUiButton("Start Music",
                         _controller.hasAssetOfType(AudioAssetType::Music),
                         "Import a music asset first.",
                         UiButtonRole::Primary,
                         ImVec2(-1.0f, 0.0f),
                         "Start the first available music asset as a looping table track."))
            _controller.playFirstAssetOfType(AudioAssetType::Music, true);
        ImGui::TableNextColumn();
        if (drawUiButton("Start Ambience",
                         _controller.hasAssetOfType(AudioAssetType::Ambience),
                         "Import an ambience asset first.",
                         UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f),
                         "Start the first available ambience asset as a loop."))
            _controller.playFirstAssetOfType(AudioAssetType::Ambience, true);
        ImGui::TableNextColumn();
        if (drawUiButton("Play Effect",
                         _controller.hasAssetOfType(AudioAssetType::Effect),
                         "Import an effect asset first.",
                         UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f),
                         "Play the first available sound effect once."))
            _controller.playFirstAssetOfType(AudioAssetType::Effect, false);
        ImGui::EndTable();
    }

    ImGui::TreePop();
    }
    if (diagnostics.missingAssets > 0)
    {
        ImGui::SeparatorText(localizedUiLabel("Diagnostics").c_str());
        const std::string diagnosticsText = diagnostics.missingAssets == 1
                                                ? formatLocalizedUi("1 audio file missing; %d can be restored from original source.", diagnostics.repairableAssets)
                                                : formatLocalizedUi("%d audio files missing; %d can be restored from original source.", diagnostics.missingAssets, diagnostics.repairableAssets);
        ImGui::TextColored(ImVec4(0.92f, 0.74f, 0.42f, 1.0f), "%s", diagnosticsText.c_str());
        if (!diagnostics.selectedMessage.empty())
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), diagnostics.selectedMessage.c_str());
        ImGui::TextDisabled(localizedUiLabel("Diagnostics stay passive during live play; repair or relink happens only when you try to play the missing asset.").c_str());
    }

    drawActivePlaybackActions(summary);
    if (ImGui::TreeNode(uiTextForLocale("Playback details", "Wiedergabedetails")))
    { drawPlaylistSnapshot(playlist); drawActiveChannels(); ImGui::TreePop(); }

    if (!_controller._statusMessage.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._statusMessage.c_str());

    if (!_controller._errorMessage.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("%s").c_str(), _controller._errorMessage.c_str());
}

void AudioQuickControlsView::drawPlaylistSnapshot(const AudioQuickPlaylistSummary &summary)
{
    if (!summary.active)
        return;

    ImGui::SeparatorText(localizedUiLabel("Playlist").c_str());
    drawInlineLabelValue("Queue", summary.playlistName.c_str());
    drawInlineLabelValue("Flags", summary.flagsText.c_str());
    const std::string activeText = formatLocalizedUi("%d active / %d queued", summary.activeQueuedCount, summary.queueSize);
    drawInlineLabelValue("State", activeText.c_str());
    if (summary.transitionActive)
        drawInlineLabelValue("Transition", summary.transitionText.empty() ? localizedUiText("Active") : summary.transitionText.c_str());

    if (ImGui::BeginTable("AudioMiniPlaylistSnapshot", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Current").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Previous").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Last").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Next").c_str());
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        tableTextWrapped(summary.currentText);
        ImGui::TableSetColumnIndex(1);
        tableTextWrapped(summary.previousText);
        ImGui::TableSetColumnIndex(2);
        tableTextWrapped(summary.lastText);
        ImGui::TableSetColumnIndex(3);
        tableTextWrapped(summary.nextText);
        ImGui::EndTable();
    }
}

void AudioQuickControlsView::drawActivePlaybackActions(const AudioQuickChannelSummary &summary)
{
    const int activeCount = summary.playing + summary.paused;
    if (activeCount > 0)
    {
    ImGui::SeparatorText(localizedUiLabel("Active Playback").c_str());

    const char *pauseReason = activeCount == 0 ? "No active audio channels to pause." : "All active audio channels are already paused.";
    if (drawUiButton("Pause All for Break",
                     summary.playing > 0,
                     pauseReason,
                     UiButtonRole::Advanced,
                     ImVec2(155.0f, 0.0f),
                     "Pause every currently playing music, ambience, and effect channel without clearing the table."))
    {
        ImGui::OpenPopup(localizedUiLabel("Pause Audio For Break").c_str());
    }

    TaskUi::sameLineIfFits();
    const char *resumeReason = activeCount == 0 ? "No active audio channels to resume." : "No paused audio channels to resume.";
    if (drawUiButton("Resume After Break",
                     summary.paused > 0,
                     resumeReason,
                     UiButtonRole::Primary,
                     ImVec2(155.0f, 0.0f),
                     "Resume every paused audio channel after the table break."))
    {
        ImGui::OpenPopup(localizedUiLabel("Resume Audio After Break").c_str());
    }

    if (drawUiButton("Stop All",
                     activeCount > 0,
                     "No active audio channels to stop.",
                     UiButtonRole::Destructive,
                     ImVec2(-1.0f, 0.0f),
                     "Stop and clear every active audio channel."))
    {
        ImGui::OpenPopup(localizedUiLabel("Stop All Audio").c_str());
    }

    }
    // Keep an already-open confirmation reachable if a track finishes while
    // the user is reviewing it. Only the idle action strip is hidden.
    if (beginBoundedConfirmationModal(localizedUiLabel("Pause Audio For Break").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Pause every currently playing audio channel for a table break?").c_str());
        drawInlineLabelValue("Playing", std::to_string(summary.playing).c_str());
        drawInlineLabelValue("Already paused", std::to_string(summary.paused).c_str());
        ImGui::TextDisabled(localizedUiLabel("This keeps channels loaded so they can be resumed after the break.").c_str());
        if (drawModalActionButtons("Pause All", UiButtonRole::Advanced))
            _controller.pauseAllForBreak();
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Resume Audio After Break").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Resume every paused audio channel after the table break?").c_str());
        drawInlineLabelValue("Paused", std::to_string(summary.paused).c_str());
        drawInlineLabelValue("Playing", std::to_string(summary.playing).c_str());
        if (drawModalActionButtons("Resume All", UiButtonRole::Primary))
            _controller.resumePausedAudio();
        ImGui::EndPopup();
    }

    if (beginBoundedConfirmationModal(localizedUiLabel("Stop All Audio").c_str()))
    {
        ImGui::TextWrapped(localizedUiLabel("Stop and clear every active audio channel?").c_str());
        drawInlineLabelValue("Active channels", std::to_string(activeCount).c_str());
        drawInlineLabelValue("Playing", std::to_string(summary.playing).c_str());
        drawInlineLabelValue("Paused", std::to_string(summary.paused).c_str());
        ImGui::TextDisabled(localizedUiLabel("Use Pause All for breaks. Stop All is for clearing the current soundscape.").c_str());
        if (drawModalActionButtons("Stop All", UiButtonRole::Destructive))
            _controller.stopAllAudio();
        ImGui::EndPopup();
    }

    if (ImGui::BeginTable("AudioMiniChannelSummary", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Music").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Ambience").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("Effects").c_str());
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        tableTextWrapped(summary.musicText);
        ImGui::TableSetColumnIndex(1);
        tableTextWrapped(summary.ambienceText);
        ImGui::TableSetColumnIndex(2);
        tableTextWrapped(summary.effectsText);
        ImGui::EndTable();
    }
}

void AudioQuickControlsView::drawActiveChannels()
{
    ImGui::SeparatorText(localizedUiLabel("Active Channels").c_str());
    const std::vector<EntityId> activeIds = _controller._playbackService->activeAssetIds();
    if (activeIds.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("No active audio channels.").c_str());
        return;
    }

    if (ImGui::BeginTable("AudioMiniActiveChannels", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Channel").c_str(), ImGuiTableColumnFlags_WidthFixed, 92.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Now Playing").c_str());
        ImGui::TableSetupColumn(localizedUiLabel("State").c_str(), ImGuiTableColumnFlags_WidthFixed, 68.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Actions").c_str(), ImGuiTableColumnFlags_WidthFixed, 124.0f);
        ImGui::TableHeadersRow();

        for (const EntityId &assetId : activeIds)
        {
            const AudioAssetModel *activeAsset = _controller.findAudioAssetById(assetId);
            const AudioAmbienceLayerModel *layer = activeAsset ? nullptr : _controller.findAmbienceLayerById(assetId);
            const std::string channel = localizedUiText(layer ? "Ambience" : (activeAsset ? audioAssetTypeLabel(activeAsset->type) : "Unknown"));
            const std::string label = layer ? (layer->name.empty() ? std::string(localizedUiText("Ambience layer")) : layer->name)
                                            : (activeAsset ? audioDisplayName(*activeAsset) : assetId);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(channel);
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(label);
            ImGui::TableSetColumnIndex(2);
            const bool paused = _controller._playbackService->isPaused(assetId);
            ImGui::TextUnformatted(localizedUiText(paused ? "Paused" : "Playing"));
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(assetId.c_str());
            if (paused)
            {
                if (drawSmallUiButton("Resume", UiButtonRole::Primary, "Resume this active audio channel."))
                    _controller.resumeChannel(assetId);
            }
            else
            {
                if (drawSmallUiButton("Pause", UiButtonRole::Secondary, "Pause this active audio channel."))
                    _controller.pauseChannel(assetId);
            }
            TaskUi::sameLineIfFits();
            if (drawSmallUiButton("Stop", UiButtonRole::Destructive, "Stop this audio channel."))
                _controller.stopChannel(assetId);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}
