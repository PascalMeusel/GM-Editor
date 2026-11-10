#include "AudioManagerViewSupport.hpp"

AudioManagerView::AudioManagerView(AudioManagerController &controller)
    : DockableWidget("Audio Controller", 0, ImVec2(760.0f, 560.0f)), _controller(controller)
{
}

void AudioManagerView::draw()
{
    if (_controller._saveFailed)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.48f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", _controller._saveErrorMessage.c_str());
        ImGui::PopStyleColor();
        if (drawUiButton("Retry Save", UiButtonRole::Primary)) _controller.retrySave();
    }
    drawManagerStatusStrip();

    if (ImGui::BeginTabBar("AudioManagerTabs"))
    {
        if (ImGui::BeginTabItem(localizedUiLabel("Library").c_str()))
        {
            drawLibraryTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(localizedUiLabel("Playlists").c_str()))
        {
            drawPlaylistControls();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(uiTextForLocale("Setup", "Einrichtung")))
        {
            if (ImGui::TreeNode(localizedUiLabel("Volume Groups").c_str()))
            { drawVolumeGroups(); ImGui::TreePop(); }
            if (ImGui::TreeNode(localizedUiLabel("Ambience Layers").c_str()))
            { drawAmbienceLayers(); ImGui::TreePop(); }
            if (ImGui::TreeNode(localizedUiLabel("Triggers").c_str()))
            { drawAudioTriggers(); ImGui::TreePop(); }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(uiTextForLocale("Now playing", "Aktive Wiedergabe")))
        { drawActiveChannels(); ImGui::EndTabItem(); }

        ImGui::EndTabBar();
    }

    drawStatusMessages();
    drawAddAudioDialog();
}

void AudioManagerView::drawManagerStatusStrip()
{
    const auto active = _controller._playbackService->activeAssetIds();
    if (!active.empty())
    {
        ImGui::TextWrapped(uiTextForLocale("Active sounds: %d", "Aktive Klänge: %d"), static_cast<int>(active.size()));
        TaskUi::sameLineForButton(localizedUiText("Stop All"));
        if (drawUiButton("Stop All##AudioHeader", UiButtonRole::Destructive))
            _controller.stopAllActiveAudio();
    }
    if (_controller._currentCampaign.audioAssets.empty())
        ImGui::TextWrapped("%s", uiTextForLocale("Add music or sounds to get started.", "Füge Musik oder Geräusche hinzu, um zu beginnen."));
}

void AudioManagerView::drawLibraryTab()
{
    if (drawUiButton(uiTextForLocale("Add audio", "Audio hinzufügen"), UiButtonRole::Primary))
        _openAddAudioDialog = true;
    ImGui::Spacing();
    if (TaskUi::hasRoomForColumns(46.0f))
    {
        if (ImGui::BeginTable("AudioLibraryLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Library", ImGuiTableColumnFlags_WidthStretch, 0.38f);
            ImGui::TableSetupColumn("Playback", ImGuiTableColumnFlags_WidthStretch, 0.62f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            drawAssetList();
            ImGui::TableNextColumn();
            drawSelectedAssetControls();
            ImGui::EndTable();
        }
    }
    else
    {
        TaskUi::fieldLabel(uiTextForLocale("Campaign audio", "Audio der Kampagne"));
        const auto *selected = _controller.getSelectedAsset();
        const std::string preview = selected ? audioDisplayName(*selected) : uiTextForLocale("Choose a sound", "Klang wählen");
        if (FormUi::BeginCombo("##CampaignAudioSelection", preview.c_str()))
        {
            for (int i = 0; i < static_cast<int>(_controller._currentCampaign.audioAssets.size()); ++i)
            {
                const auto &asset = _controller._currentCampaign.audioAssets[static_cast<std::size_t>(i)];
                ImGui::PushID(asset.metadata.id.c_str());
                if (ImGui::Selectable(audioDisplayName(asset).c_str(), i == _controller._selectedAssetIndex))
                    _controller.selectAsset(i);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        drawSelectedAssetControls();
    }
}

void AudioManagerView::drawAddAudioDialog()
{
    const std::string title = std::string(uiTextForLocale("Add audio", "Audio hinzufügen")) + "###AddCampaignAudio";
    if (_openAddAudioDialog)
    {
        ImGui::OpenPopup(title.c_str());
        _openAddAudioDialog = false;
    }
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                 viewport->WorkPos.y + viewport->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(1100.0f, viewport->WorkSize.x * 0.9f),
                                  std::min(780.0f, viewport->WorkSize.y * 0.9f)), ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::BeginPopupModal(title.c_str(), &open, ImGuiWindowFlags_NoSavedSettings)) return;
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("AddAudioContents", ImVec2(0, -footerHeight));
    if (_controller._saveFailed)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.48f, 0.30f, 1.0f), "%s", uiTextForLocale("Audio changes are not saved.", "Audioänderungen sind nicht gespeichert."));
        ImGui::TextWrapped("%s", _controller._saveErrorMessage.c_str());
        if (drawUiButton("Retry Save", UiButtonRole::Primary)) _controller.retrySave();
    }
    if (ImGui::BeginTabBar("AddAudioSource"))
    {
        if (ImGui::BeginTabItem(uiTextForLocale("Included sounds", "Mitgelieferte Klänge")))
        {
            ImGui::TextWrapped("%s", uiTextForLocale("Add sounds to this campaign. Return to the library to play them.",
                "Füge Klänge zur Kampagne hinzu. Zur Wiedergabe kehrst du zur Bibliothek zurück."));
            drawBundledCatalog();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(uiTextForLocale("Your audio files", "Eigene Audiodateien")))
        { drawImportControls(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    drawStatusMessages();
    ImGui::EndChild();
    ImGui::Separator();
    if (drawUiButton(uiTextForLocale("Return to library", "Zur Bibliothek"), UiButtonRole::Primary, ImVec2(-1, 0)))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void AudioManagerView::drawStatusMessages()
{
    if (!_controller._statusMessage.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._statusMessage.c_str());
    }

    if (!_controller._errorMessage.empty())
    {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.36f, 0.30f, 1.0f));
        ImGui::TextWrapped("%s", _controller._errorMessage.c_str());
        ImGui::PopStyleColor();
    }
}

void AudioManagerView::drawVolumeGroups()
{
    AudioVolumeGroupsModel &groups = _controller._currentCampaign.audioVolumeGroups;
    const auto volume = [](const char *label, float &value) {
        TaskUi::fieldLabel(localizedUiText(label));
        ImGui::PushID(label);
        volumePercent("##Volume", value);
        ImGui::PopID();
    };
    volume("Master", groups.master); volume("Music", groups.music);
    volume("Ambience", groups.ambience); volume("Effects", groups.effects); volume("Voice", groups.voice);
    if (drawUiButton("Save Volume Groups", UiButtonRole::Primary)) _controller.saveVolumeGroups();
}
