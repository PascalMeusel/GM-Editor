#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

void LiveSessionDashboardView::drawAudioControls()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveAudio));
    const AudioBreakCounts audioCounts = audioBreakCounts(_controller._playbackService);
    const std::string mode = gm::localization::tr(audioCounts.active > 0 ? gm::localization::TextId::LiveSharedPlaybackActive
                                                                        : gm::localization::TextId::LiveAudioIdle);
    const std::string playing = std::to_string(audioCounts.playing);
    const std::string paused = std::to_string(audioCounts.paused);
    const std::string selection = gm::localization::LocalizationService::instance().format(
        gm::localization::TextId::LiveAudioCounts,
        {playing, paused});
    const std::string next = gm::localization::tr(_controller._currentCampaign.audioAssets.empty()
                                                      ? gm::localization::TextId::LiveAudioLibraryNeeded
                                                      : gm::localization::TextId::LiveAudioToolsNext);
    UiStatusActionStrip strip;
    strip.mode = mode.c_str();
    strip.selection = selection.c_str();
    strip.nextAction = next.c_str();
    strip.help = "Playback, volume, looping, and stop controls stay in Live. Open the library only to import or repair campaign audio.";
    drawStatusActionStrip("LiveSessionAudioSharedStatus", strip);

    if (_controller._currentCampaign.audioAssets.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveAudioLibraryNeeded));
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveOpenAudioLibrary), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Import campaign music, ambience, voice, or effects."))
            _controller.openAudioLibrary();
    }
    else
    {
        const AudioAssetModel *asset = _controller.selectedAudioAsset();
        if (!asset)
        {
            _controller.selectAudioAsset(0);
            asset = _controller.selectedAudioAsset();
        }

        const char *selectedLabel = asset
                                        ? (asset->name.empty() ? asset->originalFileName.c_str() : asset->name.c_str())
                                        : gm::localization::trUi("Select audio");
        if (FormUi::BeginCombo(localizedUiLabel("##LiveAudioAsset").c_str(), selectedLabel))
        {
            for (int index = 0; index < static_cast<int>(_controller._currentCampaign.audioAssets.size()); ++index)
            {
                const AudioAssetModel &candidate = _controller._currentCampaign.audioAssets[static_cast<std::size_t>(index)];
                const std::string label = candidate.name.empty() ? candidate.originalFileName : candidate.name;
                const bool selected = index == _controller._selectedAudioAssetIndex;
                if (ImGui::Selectable(label.c_str(), selected))
                    _controller.selectAudioAsset(index);
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (FormUi::SliderFloat(localizedUiLabel("Volume##LiveAudio").c_str(), &_controller._audioVolume, 0.0f, 1.0f, "%.2f"))
            _controller.applySelectedAudioVolume();
        FormUi::Checkbox(localizedUiLabel("Loop##LiveAudio").c_str(), &_controller._audioLoop);

        const bool active = _controller.isSelectedAudioActive();
        const bool paused = _controller.isSelectedAudioPaused();
        const int audioActionColumns = ImGui::GetContentRegionAvail().x < 420.0f ? 2 : 4;
        if (ImGui::BeginTable("LiveAudioActions", audioActionColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (drawUiButton("Play##LiveAudio", asset != nullptr, "Select audio first.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f)))
                _controller.playSelectedAudio();
            ImGui::TableNextColumn();
            if (drawUiButton(paused ? "Resume##LiveAudio" : "Pause##LiveAudio", active, "Selected audio is not active.", paused ? UiButtonRole::Primary : UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            {
                if (paused)
                    _controller.resumeSelectedAudio();
                else
                    _controller.pauseSelectedAudio();
            }
            ImGui::TableNextColumn();
            if (drawUiButton("Stop##LiveAudio", active, "Selected audio is not active.", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
                _controller.stopSelectedAudio();
            ImGui::TableNextColumn();
            if (drawUiButton("Stop all##LiveAudio", audioCounts.active > 0, "No active audio.", UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f)))
                _controller.stopAllAudio();
            ImGui::EndTable();
        }

        if (drawSmallUiButton(gm::localization::tr(gm::localization::TextId::LiveOpenAudioLibrary), UiButtonRole::Advanced, "Import, organize, or repair campaign audio."))
            _controller.openAudioLibrary();
    }

    if (!_controller._audioStatusMessage.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._audioStatusMessage.c_str());
    if (!_controller._audioErrorMessage.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.36f, 0.30f, 1.0f), localizedUiLabel("%s").c_str(), _controller._audioErrorMessage.c_str());
}

void LiveSessionDashboardView::drawSessionNotes()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveNotes));
    const SessionModel *currentSession = _controller.activeSession();
    if (!currentSession)
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveStartForNotes));
        return;
    }

    // Inline saves replace campaign vectors. Keep this frame's list independent.
    const SessionModel snapshot = *currentSession;
    const SessionModel *session = &snapshot;

    FormUi::InputTextMultiline(localizedUiLabel("##LiveSessionScratchNotes").c_str(), &_controller._sessionSummaryDraft, ImVec2(-1.0f, 120.0f), ImGuiInputTextFlags_WordWrap);
    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveSaveNotes), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Save the live scratch notes for this session."))
        _controller.saveSessionSummary();

    if (!ImGui::CollapsingHeader(gm::localization::tr(gm::localization::TextId::LiveTimestampedNotes))) return;
    FormUi::InputTextMultiline(localizedUiLabel("##NewSessionNote").c_str(), &_controller._sessionNoteDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);

    const std::vector<EntityRegistryEntry> linkCandidates = _controller.noteLinkCandidates();
    const char *selectedLinkLabel = gm::localization::tr(gm::localization::TextId::LiveNoEntityLink);
    const auto selectedLink = std::find_if(linkCandidates.begin(), linkCandidates.end(), [&](const auto &entry) { return entry.id == _controller._selectedNoteEntityId; });
    if (selectedLink != linkCandidates.end()) selectedLinkLabel = selectedLink->displayName.c_str();
    if (FormUi::BeginCombo(gm::localization::tr(gm::localization::TextId::LiveLink), selectedLinkLabel))
    {
        if (ImGui::Selectable(gm::localization::tr(gm::localization::TextId::LiveNoEntityLink), _controller._selectedNoteEntityId.empty()))
            _controller._selectedNoteEntityId.clear();
        for (int i = 0; i < static_cast<int>(linkCandidates.size()); ++i)
        {
            const EntityRegistryEntry &entry = linkCandidates[static_cast<std::size_t>(i)];
            const std::string displayName = localizedRegistryFallback(entry.displayName);
            const std::string label = displayName + " (" + gm::localization::trUi(entry.type.c_str()) + ")";
            if (ImGui::Selectable(label.c_str(), entry.id == _controller._selectedNoteEntityId))
                _controller._selectedNoteEntityId = entry.id;
        }
        ImGui::EndCombo();
    }
    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveAddTimestampedNote), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Append this note with the current timestamp and optional entity link."))
        _controller.addSessionNote();

    if (session->notes.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoTimestampedNotes));
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    FormUi::InputTextWithHint(localizedUiLabel("##SessionNoteSearch").c_str(), gm::localization::tr(gm::localization::TextId::LiveSearchNotesHint), &_controller._sessionNoteSearch);
    showHelpTooltip(gm::localization::trUi("Filters saved timestamped notes without changing the session."));
    if (!_controller._sessionNoteSearch.empty())
    {
        const std::string clearLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveClear)) + "##SessionNoteSearch";
        if (drawSmallUiButton(clearLabel.c_str(), UiButtonRole::Secondary, "Clear the note search."))
            _controller._sessionNoteSearch.clear();
    }

    std::vector<int> visibleNoteIndices;
    visibleNoteIndices.reserve(session->notes.size());
    for (int i = static_cast<int>(session->notes.size()) - 1; i >= 0; --i)
    {
        const SessionNoteModel &note = session->notes[static_cast<std::size_t>(i)];
        if (noteMatchesSearch(note, _controller._sessionNoteSearch))
            visibleNoteIndices.push_back(i);
    }
    ImGui::TextDisabled(localizedUiLabel("%d of %d notes shown").c_str(), static_cast<int>(visibleNoteIndices.size()), static_cast<int>(session->notes.size()));
    if (visibleNoteIndices.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoMatchingNotes));
        return;
    }

    bool openSessionNoteRemoval = false;
    if (ImGui::BeginTable("LiveSessionNotes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveTime), ImGuiTableColumnFlags_WidthFixed, 142.0f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveNote));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveLink), ImGuiTableColumnFlags_WidthFixed, 140.0f);
        const float removeWidth = ImGui::CalcTextSize(gm::localization::tr(gm::localization::TextId::CommonRemove)).x +
                                  ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::TableSetupColumn(localizedUiLabel("").c_str(),
                                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, removeWidth);
        ImGui::TableHeadersRow();
        for (int i : visibleNoteIndices)
        {
            const SessionNoteModel &note = session->notes[static_cast<std::size_t>(i)];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(note.timestampUtc);
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(SessionControl::systemTextForDisplay(note.body));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(noteLinkLabel(note));
            ImGui::TableSetColumnIndex(3);
            if (drawSmallUiButton(gm::localization::tr(gm::localization::TextId::CommonRemove), UiButtonRole::Destructive, "Remove this timestamped note."))
            {
                _pendingSessionNoteRemovalId = note.metadata.id;
                openSessionNoteRemoval = true;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (openSessionNoteRemoval)
        ImGui::OpenPopup(gm::localization::tr(gm::localization::TextId::LiveRemoveSessionNote));

    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(gm::localization::tr(gm::localization::TextId::LiveRemoveSessionNote), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const auto selectedNote = std::find_if(session->notes.begin(), session->notes.end(), [&](const auto &note) { return note.metadata.id == _pendingSessionNoteRemovalId; });
        if (selectedNote == session->notes.end())
        {
            ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveNoSessionNoteSelection));
        }
        else
        {
            const SessionNoteModel &note = *selectedNote;
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveRemoveSessionNoteQuestion));
            drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveTime), note.timestampUtc.empty() ? "-" : note.timestampUtc.c_str());
            const std::string noteBody = note.body.empty() ? "-" : SessionControl::systemTextForDisplay(note.body);
            drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveNote), noteBody.c_str());
        }

        const UiModalAction action = drawModalActionButtonsEx(gm::localization::tr(gm::localization::TextId::CommonRemove),
                                                               UiButtonRole::Destructive,
                                                               gm::localization::tr(gm::localization::TextId::CommonCancel),
                                                               110.0f);
        if (action == UiModalAction::Confirm)
        {
            _controller.removeSessionNote(_pendingSessionNoteRemovalId);
            _pendingSessionNoteRemovalId.clear();
        }
        else if (action == UiModalAction::Cancel)
        {
            _pendingSessionNoteRemovalId.clear();
        }
        ImGui::EndPopup();
    }
}
