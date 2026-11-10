#include "Views/Editor/Widgets/Session/GMScreen/GMScreenViewSupport.hpp"

void GMScreenView::drawHandoutRevealControls()
{
    ImGui::SeparatorText(gm::localization::trUi("Handouts"));
    if (ImGui::CollapsingHeader(uiTextForLocale("Prepare a handout","Handout vorbereiten"),ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputText(localizedUiLabel("Title##HandoutTitle").c_str(), &_controller._handoutTitleDraft);
        FormUi::InputTextMultiline(localizedUiLabel("Body##HandoutBody").c_str(), &_controller._handoutBodyDraft, ImVec2(-1.0f, 76.0f));
        ImGui::SetNextItemWidth(-1.0f);
        if(ImGui::Button(uiTextForLocale("Browse attachment…","Anhang auswählen…")))_controller.browseHandoutAttachment();
        if(!_controller._handoutFilePathDraft.empty())ImGui::TextWrapped("%s",_controller._handoutFilePathDraft.c_str());
        ImGui::SeparatorText(uiTextForLocale("Player-facing preview · Hidden","Spielervorschau · Verborgen"));
        ImGui::TextWrapped("%s",_controller._handoutTitleDraft.c_str());
        ImGui::TextWrapped("%s",_controller._handoutBodyDraft.c_str());
        const bool hasDraft = !_controller._handoutTitleDraft.empty() || !_controller._handoutBodyDraft.empty() || !_controller._handoutFilePathDraft.empty();
        if (drawUiButton("Add Handout", hasDraft, "Add a title, body, or file path before creating a handout.", UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Creates a campaign handout from the draft fields."))
            ImGui::OpenPopup(localizedUiLabel("Create Handout Review").c_str());
        setResponsiveAutoSizeWindow(620.0f, 400.0f);
        if (ImGui::BeginPopupModal(localizedUiLabel("Create Handout Review").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Create this handout for the campaign?"));
            const std::string title = _controller._handoutTitleDraft.empty()
                                          ? localizedUiLabel("Untitled Handout")
                                          : _controller._handoutTitleDraft;
            const std::string body = localizedUiLabel(_controller._handoutBodyDraft.empty() ? "No body text" : "Body text added");
            const std::string file = _controller._handoutFilePathDraft.empty()
                                         ? localizedUiLabel("No file")
                                         : _controller._handoutFilePathDraft;
            drawInlineLabelValue("Title", title.c_str());
            drawInlineLabelValue("Body", body.c_str());
            drawInlineLabelValue("File", file.c_str());
            if (drawModalActionButtons("Create", UiButtonRole::Primary))
                _controller.addHandout();
            ImGui::EndPopup();
        }
    }

    if (_controller._currentCampaign.handouts.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::trUi("No handouts available."));
        return;
    }

    const LocalPlayerDisplayModel &display = _controller._currentCampaign.localPlayerDisplay;
    const std::string revealHeader = localizedUiLabel("Reveal Handouts") + " (" + handoutStatusLabel(_controller._currentCampaign, display) + ")";
    if (!beginContextHeader(revealHeader.c_str(), !display.publicHandoutIds.empty(), "Reveal or hide existing handouts without keeping the whole list visible by default."))
        return;

    bool openHandoutRemoval = false;
    LocalPlayerDisplayModel &mutableDisplay = _controller._currentCampaign.localPlayerDisplay;
    for (int index = 0; index < static_cast<int>(_controller._currentCampaign.handouts.size()); ++index)
    {
        const HandoutModel &handout = _controller._currentCampaign.handouts[static_cast<std::size_t>(index)];
        ImGui::PushID(index);
        bool publicToPlayers = containsId(mutableDisplay.publicHandoutIds, handout.metadata.id);
        if (FormUi::Checkbox(gm::localization::trUi("Public"), &publicToPlayers))
            _controller.setHandoutPublic(handout.metadata.id, publicToPlayers);
        ImGui::SameLine();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), handout.title.empty() ? localizedUiText("Untitled Handout") : handout.title.c_str());
        if (!handout.body.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), handout.body.substr(0, 96).c_str());
            ImGui::PopStyleColor();
        }
        if (drawSmallUiButton("Remove", UiButtonRole::Destructive, "Remove this handout and clear it from the public display."))
        {
            _pendingHandoutRemovalIndex = index;
            openHandoutRemoval = true;
        }
        ImGui::PopID();
    }

    if (openHandoutRemoval)
        ImGui::OpenPopup(localizedUiLabel("Remove Handout Review").c_str());

    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("Remove Handout Review").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (_pendingHandoutRemovalIndex < 0 ||
            _pendingHandoutRemovalIndex >= static_cast<int>(_controller._currentCampaign.handouts.size()))
        {
            ImGui::TextUnformatted(localizedUiLabel("No handout is selected.").c_str());
        }
        else
        {
            const HandoutModel &handout = _controller._currentCampaign.handouts[static_cast<std::size_t>(_pendingHandoutRemovalIndex)];
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Remove this handout and clear it from the public display?"));
            const std::string handoutTitle = handout.title.empty() ? localizedUiLabel("Untitled Handout") : handout.title;
            drawInlineLabelValue("Handout", handoutTitle.c_str());
        }

        const UiModalAction action = drawModalActionButtonsEx("Remove", UiButtonRole::Destructive, "Cancel", 110.0f);
        if (action == UiModalAction::Confirm)
        {
            _controller.removeHandout(_pendingHandoutRemovalIndex);
            _pendingHandoutRemovalIndex = -1;
        }
        else if (action == UiModalAction::Cancel)
        {
            _pendingHandoutRemovalIndex = -1;
        }
        ImGui::EndPopup();
    }
}

void GMScreenView::drawSessionFlow()
{
    if (_controller._currentCampaign.presentationModeActive) return;
    ImGui::SeparatorText(gm::localization::trUi("Live Session"));
    const SessionModel *activeSession = _controller.activeSession();
    const bool breakActive = _controller.isSessionBreakActive();
    const AudioBreakCounts audioCounts = audioBreakCounts(_controller._playbackService);
    const std::string playing = std::to_string(audioCounts.playing);
    const std::string paused = std::to_string(audioCounts.paused);
    const std::string audioLine = audioCounts.active > 0
                                      ? gm::localization::LocalizationService::instance().format(
                                            gm::localization::TextId::LiveAudioCounts,
                                            {playing, paused})
                                      : gm::localization::tr(gm::localization::TextId::LiveIdle);
    drawSessionStatusStrip("GMScreenSessionFlowStatus");
    if (activeSession != nullptr)
    {
        ImGui::Text(localizedUiLabel("Active: %s").c_str(), activeSession->title.c_str());
        if (!activeSession->startedAtUtc.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled(localizedUiLabel("Started %s").c_str(), activeSession->startedAtUtc.c_str());
        }
        ImGui::TextDisabled(localizedUiLabel("Audio: %s").c_str(), audioLine.c_str());
        if (breakActive)
        {
            if (drawUiButton("Resume Table", UiButtonRole::Primary, ImVec2(140.0f, 0.0f), "Records resume markers in the live session notes and timeline."))
                ImGui::OpenPopup(localizedUiLabel("GM Resume Live Session").c_str());
            ImGui::SameLine();
            if (drawUiButton("Resume + Audio",
                             audioCounts.paused > 0,
                             "No paused audio channels to resume.",
                             UiButtonRole::Secondary,
                             ImVec2(150.0f, 0.0f),
                             "Resume the table and every paused audio channel."))
                ImGui::OpenPopup(localizedUiLabel("GM Resume Live Session With Audio").c_str());
        }
        else
        {
            if (drawUiButton("Start Break", UiButtonRole::Advanced, ImVec2(140.0f, 0.0f), "Records break markers in the live session notes and timeline."))
                ImGui::OpenPopup(localizedUiLabel("GM Start Session Break").c_str());
            ImGui::SameLine();
            if (drawUiButton("Break + Audio",
                             audioCounts.playing > 0,
                             "No playing audio channels to pause.",
                             UiButtonRole::Secondary,
                             ImVec2(140.0f, 0.0f),
                             "Start the break and pause every playing audio channel."))
                ImGui::OpenPopup(localizedUiLabel("GM Start Session Break With Audio").c_str());
        }
        ImGui::SameLine();
        if (drawUiButton("End Session", UiButtonRole::Destructive))
            ImGui::OpenPopup(localizedUiLabel("GM End Live Session").c_str());
        showHelpTooltip("Ends the current live session and records an end timestamp.");
    }
    else
    {
        if (const auto *prepared = SessionPlan::nextSession(_controller._currentCampaign))
            ImGui::TextWrapped("%s: %s", uiTextForLocale("Next session", "Nächste Sitzung"), prepared->title.c_str());
        if (drawUiButton("Start Session", UiButtonRole::Primary))
        { _controller.reviewSessionStart(); ImGui::OpenPopup(localizedUiLabel("GM Start Live Session").c_str()); }
        showHelpTooltip("Creates a session record, marks it live, and snapshots the current party as participants.");
    }

    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(localizedUiLabel("GM Start Live Session").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto &review = _controller._sessionStartReview;
        if (!review.valid) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        ImGui::TextWrapped("%s", uiTextForLocale("Start the reviewed session in Setup with the current party?", "Die geprüfte Sitzung mit der aktuellen Gruppe in Vorbereitung starten?"));
        if (review.create) FormUi::InputText(uiTextForLocale("Session title", "Sitzungstitel"), &review.title);
        else drawInlineLabelValue("Title", review.title.c_str());
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveInitialState),
                             uiTextForLocale("Setup", "Vorbereitung"));
        if (!_controller._sessionOperationError.empty()) ImGui::TextWrapped("%s", _controller._sessionOperationError.c_str());
        if (drawUiButton("Start", UiButtonRole::Primary))
            if (_controller.startSession()) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        ImGui::SameLine();
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::CommonCancel), UiButtonRole::Secondary)) { review = {}; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("GM Start Session Break").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Mark the table as on break and add break markers to the session notes and timeline."));
        const std::string sessionTitle = activeSession ? activeSession->title : localizedUiLabel("No active session");
        drawInlineLabelValue("Session", sessionTitle.c_str());
        drawInlineLabelValue("Audio", gm::localization::tr(gm::localization::TextId::LiveLeaveAudioUnchanged));
        if (drawModalActionButtons("Start Break", UiButtonRole::Advanced))
            _controller.startSessionBreak();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("GM Start Session Break With Audio").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Mark the table as on break and pause every playing audio channel."));
        const std::string sessionTitle = activeSession ? activeSession->title : localizedUiLabel("No active session");
        drawInlineLabelValue("Session", sessionTitle.c_str());
        drawInlineLabelValue("Audio", audioLine.c_str());
        if (drawModalActionButtons("Break + Audio", UiButtonRole::Advanced))
            _controller.startSessionBreak(true);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("GM Resume Live Session").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Resume play and record resume markers in the active session."));
        const std::string sessionTitle = activeSession ? activeSession->title : localizedUiLabel("No active session");
        drawInlineLabelValue("Session", sessionTitle.c_str());
        drawInlineLabelValue("Audio", gm::localization::tr(gm::localization::TextId::LiveLeaveAudioUnchanged));
        if (drawModalActionButtons("Resume", UiButtonRole::Primary))
            _controller.resumeSessionFromBreak();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("GM Resume Live Session With Audio").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("Resume play and restart every audio channel that was paused for the break."));
        const std::string sessionTitle = activeSession ? activeSession->title : localizedUiLabel("No active session");
        drawInlineLabelValue("Session", sessionTitle.c_str());
        drawInlineLabelValue("Audio", audioLine.c_str());
        if (drawModalActionButtons("Resume + Audio", UiButtonRole::Primary))
            _controller.resumeSessionFromBreak(true);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(localizedUiLabel("GM End Live Session").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::trUi("End the active live session and record the end timestamp."));
        const std::string sessionTitle = activeSession ? activeSession->title : localizedUiLabel("No active session");
        drawInlineLabelValue("Session", sessionTitle.c_str());
        const std::string sessionState = breakActive
                                             ? std::string(gm::localization::tr(gm::localization::TextId::LiveBreak)) + " / " +
                                                   gm::localization::tr(gm::localization::TextId::LivePaused)
                                             : gm::localization::tr(gm::localization::TextId::LiveRunning);
        drawInlineLabelValue("State", sessionState.c_str());
        if (drawModalActionButtons("End Session", UiButtonRole::Destructive))
            _controller.endActiveSession();
        ImGui::EndPopup();
    }

    if (!_controller._currentCampaign.sessions.empty())
    {
        const SessionModel &latestSession = _controller._currentCampaign.sessions.front();
        ImGui::TextDisabled(localizedUiLabel("%s: %d").c_str(), gm::localization::trUi("Tracked sessions"), static_cast<int>(_controller._currentCampaign.sessions.size()));
        ImGui::SameLine();
        ImGui::TextDisabled(localizedUiLabel("%s: %s").c_str(), gm::localization::trUi("Latest"), latestSession.title.c_str());
    }
}
