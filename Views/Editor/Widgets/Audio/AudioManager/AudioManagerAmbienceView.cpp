#include "AudioManagerViewSupport.hpp"

void AudioManagerView::drawAmbienceLayers()
{
    TaskUi::fieldLabel(uiTextForLocale("Layer name","Name der Ebene"));
    FormUi::InputText("##NewLayerName",&_controller._newLayerName);
    chooseAudio(_controller._currentCampaign,"##NewLayerAudio",_controller._newLayerAssetId);
    volumePercent(uiTextForLocale("Initial volume","Anfangslautstärke"),_controller._newLayerVolume);
    FormUi::Checkbox(uiTextForLocale("Loop","Wiederholen"),&_controller._newLayerLoop);
    ImGui::TextDisabled("%s",uiTextForLocale("Saving does not start playback","Speichern startet keine Wiedergabe"));
    drawSectionTitle("Ambience Layers");
    if (drawUiButton("Add Ambience Layer", UiButtonRole::Primary, ImVec2(158.0f, 0.0f), "Create a controllable ambience layer using campaign ambience assets."))
        _controller.addAmbienceLayer();

    if (_controller._currentCampaign.audioAmbienceLayers.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No ambience layers yet.").c_str());
        return;
    }

    if (ImGui::BeginTable("AudioAmbienceLayerTable", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Layer").c_str(), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn(localizedUiLabel("Asset").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(localizedUiLabel("Playback").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("State").c_str(), ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 174.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.audioAmbienceLayers.size()); ++i)
        {
            AudioAmbienceLayerModel &layer = _controller._currentCampaign.audioAmbienceLayers[static_cast<size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputText(("##LayerName" + std::to_string(i)).c_str(), &layer.name);

            ImGui::TableSetColumnIndex(1);
            const AudioAssetModel *selectedAsset = _controller.findAudioAssetById(layer.audioAssetId);
            const std::string preview = selectedAsset ? selectedAsset->name : "Choose ambience";
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::BeginCombo(("##LayerAsset" + std::to_string(i)).c_str(), preview.c_str()))
            {
                for (const AudioAssetModel &asset : _controller._currentCampaign.audioAssets)
                {
                    if (asset.type != AudioAssetType::Ambience)
                        continue;
                    const bool selected = asset.metadata.id == layer.audioAssetId;
                    if (ImGui::Selectable(asset.name.c_str(), selected))
                        layer.audioAssetId = asset.metadata.id;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1.0f);
            if (volumePercent(("##LayerVolume" + std::to_string(i)).c_str(), layer.volume))
                _controller.applyAmbienceLayerVolume(i);
            ImGui::TextDisabled(localizedUiLabel("Loop").c_str());
            TaskUi::sameLineIfFits();
            FormUi::Checkbox(("##LayerLoop" + std::to_string(i)).c_str(), &layer.loop);

            ImGui::TableSetColumnIndex(3);
            const bool playing = _controller._playbackService->isActive(layer.metadata.id);
            ImGui::TextUnformatted(localizedUiText(playing ? "Playing" : "Idle"));

            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton(("Play##Layer" + std::to_string(i)).c_str()))
                _controller.playAmbienceLayer(i);
            TaskUi::sameLineIfFits();
            if (ImGui::SmallButton(("Stop##Layer" + std::to_string(i)).c_str()))
                _controller.stopAmbienceLayer(i);
            TaskUi::sameLineIfFits();
            if (ImGui::SmallButton(("Save##Layer" + std::to_string(i)).c_str()))
                _controller.saveAmbienceLayer(i);
            TaskUi::sameLineIfFits();
            const std::string removeLayerPopupId = "Remove Ambience Layer##Confirm" + std::to_string(i);
            if (ImGui::SmallButton(("Remove##Layer" + std::to_string(i)).c_str()))
                ImGui::OpenPopup(removeLayerPopupId.c_str());
            if (drawConfirmationModal(removeLayerPopupId.c_str(), "Remove this ambience layer?", "Remove"))
            {
                _controller.removeAmbienceLayer(i);
                break;
            }
        }

        ImGui::EndTable();
    }
}

void AudioManagerView::drawAudioTriggers()
{
    drawSectionTitle("Audio Triggers");
    TaskUi::fieldLabel(localizedUiText("Trigger Name"));
    FormUi::InputText("##Trigger Name", &_controller._newTriggerName);
    TaskUi::sameLineIfFits();
    if (FormUi::BeginCombo(localizedUiLabel("Source").c_str(), localizedUiText(_controller._newTriggerSourceType.c_str())))
    {
        for (const char *sourceType : {"StoryBeat", "MapRegion", "CombatEvent", "SessionEvent"})
        {
            const bool selected = _controller._newTriggerSourceType == sourceType;
            if (ImGui::Selectable(localizedUiText(sourceType), selected)) {
                _controller._newTriggerSourceType = sourceType;
                _controller._newTriggerSourceId.clear();
                _controller._newTriggerEventName = defaultTriggerEvent(sourceType);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SeparatorText(uiTextForLocale("When","Wenn"));
    chooseTriggerContext(_controller._currentCampaign,_controller._newTriggerSourceType,_controller._newTriggerSourceId,_controller._newTriggerEventName);
    ImGui::SeparatorText(uiTextForLocale("Then play","Dann abspielen"));
    chooseAudio(_controller._currentCampaign,"##NewTriggerSound",_controller._newTriggerAssetId);
    volumePercent(uiTextForLocale("Volume","Lautstärke"),_controller._newTriggerVolume);
    TaskUi::sameLineIfFits();
    if (drawUiButton("Add Trigger",
                     !_controller._currentCampaign.audioAssets.empty(),
                     "Import or add campaign audio before creating a trigger.",
                     UiButtonRole::Primary,
                     ImVec2(104.0f, 0.0f),
                     "Create a trigger that can play campaign audio for the selected source/event."))
        _controller.addAudioTrigger();

    if (_controller._currentCampaign.audioTriggers.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("No audio triggers configured.").c_str());
        return;
    }

    if (ImGui::BeginTable("AudioTriggerTable", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(localizedUiLabel("Trigger").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn(localizedUiLabel("Source").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn(localizedUiLabel("Event / Audio").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn(localizedUiLabel("Playback").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.audioTriggers.size()); ++i)
        {
            AudioTriggerModel &trigger = _controller._currentCampaign.audioTriggers[static_cast<size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputText(("##TriggerName" + std::to_string(i)).c_str(), &trigger.name);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled(localizedUiLabel("Source").c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::BeginCombo(("##TriggerSource" + std::to_string(i)).c_str(), localizedUiText(trigger.sourceType.c_str())))
            {
                for (const char *sourceType : {"StoryBeat", "MapRegion", "CombatEvent", "SessionEvent"})
                {
                    const bool selected = trigger.sourceType == sourceType;
                    if (ImGui::Selectable(localizedUiText(sourceType), selected)) {
                        trigger.sourceType = sourceType;
                        trigger.sourceEntityId.clear();
                        trigger.eventName = defaultTriggerEvent(sourceType);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PushID(i);
            chooseTriggerContext(_controller._currentCampaign,trigger.sourceType,trigger.sourceEntityId,trigger.eventName);
            ImGui::PopID();

            ImGui::TableSetColumnIndex(2);

            ImGui::TextDisabled(localizedUiLabel("Audio").c_str());
            const AudioAssetModel *asset = _controller.findAudioAssetById(trigger.audioAssetId);
            const std::string preview = asset ? asset->name : std::string(localizedUiText("Choose audio"));
            ImGui::SetNextItemWidth(-1.0f);
            if (FormUi::BeginCombo(("##TriggerAudio" + std::to_string(i)).c_str(), preview.c_str()))
            {
                for (const AudioAssetModel &candidate : _controller._currentCampaign.audioAssets)
                {
                    const bool selected = candidate.metadata.id == trigger.audioAssetId;
                    if (ImGui::Selectable(candidate.name.c_str(), selected))
                        trigger.audioAssetId = candidate.metadata.id;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled(localizedUiLabel("Volume").c_str());
            ImGui::SetNextItemWidth(-1.0f);
            volumePercent(("##TriggerVolume" + std::to_string(i)).c_str(), trigger.volume);
            FormUi::Checkbox(("Loop##Trigger" + std::to_string(i)).c_str(), &trigger.loop);
            TaskUi::sameLineIfFits();
            FormUi::Checkbox(("On##Trigger" + std::to_string(i)).c_str(), &trigger.enabled);

            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton(("Fire##Trigger" + std::to_string(i)).c_str()))
                _controller.fireAudioTrigger(i);
            if (ImGui::SmallButton(("Save##Trigger" + std::to_string(i)).c_str()))
                _controller.saveAudioTrigger(i);
            const std::string removeTriggerPopupId = "Remove Audio Trigger##Confirm" + std::to_string(i);
            if (ImGui::SmallButton(("Remove##Trigger" + std::to_string(i)).c_str()))
                ImGui::OpenPopup(removeTriggerPopupId.c_str());
            if (drawConfirmationModal(removeTriggerPopupId.c_str(), "Remove this audio trigger?", "Remove"))
            {
                _controller.removeAudioTrigger(i);
                break;
            }
        }

        ImGui::EndTable();
    }
}
