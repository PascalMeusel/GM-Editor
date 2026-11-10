#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

void LiveSessionDashboardView::drawSessionTemplates()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveTemplates));
    if (!_controller.activeSession())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveStartForTemplates));
        return;
    }

    const std::vector<std::string> labels = _controller.sessionTemplateLabels();
    for (int i = 0; i < static_cast<int>(labels.size()); ++i)
    {
        ImGui::PushID(i);
        if (drawUiButton(labels[static_cast<std::size_t>(i)].c_str(), UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Apply this template to the active session draft fields."))
            _controller.applySessionTemplate(i);
        ImGui::PopID();
    }
}

void LiveSessionDashboardView::drawSessionRecap()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveRecap));
    if (drawCompletedRecap()) return;
    if (!_controller.activeSession())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveStartForRecap));
        return;
    }

    const std::string recapLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveRecap)) + "###LiveRecap";
    const std::string threadsLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveUnresolvedThreads)) + "###LiveUnresolvedThreads";
    const std::string lootLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveLootAwarded)) + "###LiveLootAwarded";
    const std::string xpLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveXpMilestones)) + "###LiveXpMilestones";
    const std::string npcsLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveNpcsMet)) + "###LiveNpcsMet";
    const std::string locationsLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveLocationsVisited)) + "###LiveLocationsVisited";
    const std::string hooksLabel = std::string(gm::localization::tr(gm::localization::TextId::LiveOpenHooks)) + "###LiveOpenHooks";
    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveSaveRecap), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Save recap, rewards, milestones, NPCs, locations, and open hooks."))
        _controller.saveSessionRecap();

    ImGui::TextUnformatted(recapLabel.c_str(), recapLabel.c_str() + recapLabel.find("###"));
    FormUi::InputTextMultiline(recapLabel.c_str() + recapLabel.find("###"), &_controller._recapDraft, ImVec2(-1.0f, 88.0f), ImGuiInputTextFlags_WordWrap);
    ImGui::TextUnformatted(threadsLabel.c_str(), threadsLabel.c_str() + threadsLabel.find("###"));
    FormUi::InputTextMultiline(threadsLabel.c_str() + threadsLabel.find("###"), &_controller._unresolvedThreadsDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);
    if (ImGui::CollapsingHeader(uiTextForLocale("Additional details", "Weitere Details")))
    {
        ImGui::TextUnformatted(lootLabel.c_str(), lootLabel.c_str() + lootLabel.find("###"));
        FormUi::InputTextMultiline(lootLabel.c_str() + lootLabel.find("###"), &_controller._lootAwardedDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);
        ImGui::TextUnformatted(xpLabel.c_str(), xpLabel.c_str() + xpLabel.find("###"));
        FormUi::InputTextMultiline(xpLabel.c_str() + xpLabel.find("###"), &_controller._xpMilestonesDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);
        ImGui::TextUnformatted(npcsLabel.c_str(), npcsLabel.c_str() + npcsLabel.find("###"));
        FormUi::InputTextMultiline(npcsLabel.c_str() + npcsLabel.find("###"), &_controller._npcsMetDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);
        ImGui::TextUnformatted(locationsLabel.c_str(), locationsLabel.c_str() + locationsLabel.find("###"));
        FormUi::InputTextMultiline(locationsLabel.c_str() + locationsLabel.find("###"), &_controller._locationsVisitedDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);
        ImGui::TextUnformatted(hooksLabel.c_str(), hooksLabel.c_str() + hooksLabel.find("###"));
        FormUi::InputTextMultiline(hooksLabel.c_str() + hooksLabel.find("###"), &_controller._openHooksDraft, ImVec2(-1.0f, 76.0f), ImGuiInputTextFlags_WordWrap);
    }
}

void LiveSessionDashboardView::drawSessionLifecycleControls()
{
    if (_controller._currentCampaign.presentationModeActive) { drawGmOnlyNotice("Session"); return; }
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveSessionState));
    const SessionModel *session = _controller.activeSession();
    const bool breakActive = _controller.isSessionBreakActive();
    const AudioBreakCounts audioCounts = audioBreakCounts(_controller._playbackService);
    const bool german = gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
    const std::string audioLine = audioCounts.active > 0
                                      ? std::to_string(audioCounts.playing) + (german ? " läuft / " : " playing / ") +
                                            std::to_string(audioCounts.paused) + (german ? " pausiert" : " paused")
                                      : gm::localization::tr(gm::localization::TextId::LiveIdle);
    if (ImGui::TreeNode(uiTextForLocale("State details", "Statusdetails")))
    {
    drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveSession),
                         session ? session->title.c_str() : gm::localization::tr(gm::localization::TextId::LiveNotStarted));
    drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveState),
                         session ? (breakActive ? gm::localization::tr(gm::localization::TextId::LiveBreak)
                                                : gm::localization::tr(gm::localization::TextId::LiveRunning))
                                 : gm::localization::tr(gm::localization::TextId::LivePrepState));
    drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveAudio), audioLine.c_str());
    ImGui::TreePop();
    }
    if (!session)
    {
        if (const auto *prepared = SessionPlan::nextSession(_controller._currentCampaign))
            ImGui::TextWrapped("%s: %s", german ? "Nächste Sitzung" : "Next session", prepared->title.c_str());
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveStartSession), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Creates a live session using the current party."))
        { _controller.reviewSessionStart(); ImGui::OpenPopup(gm::localization::tr(gm::localization::TextId::LiveStartSession)); }
    }
    else if (breakActive)
    {
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveResumeTable), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Records resume markers in the live session notes and timeline."))
            { _controller.resumeSessionFromBreak(); return; }
        if (audioCounts.paused > 0 && drawUiButton(gm::localization::tr(gm::localization::TextId::LiveResumeAudio),
                         audioCounts.paused > 0,
                         "No paused audio channels to resume.",
                         UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f),
                         "Resume the table and every paused audio channel."))
            { _controller.resumeSessionFromBreak(true); return; }
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveEndSession), UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f), "Ends the current live session and records an end timestamp."))
            ImGui::OpenPopup(gm::localization::tr(gm::localization::TextId::LiveEndSession));
    }
    else
    {
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveStartBreak), UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Records break markers in the live session notes and timeline."))
            { _controller.startSessionBreak(); return; }
        if (audioCounts.playing > 0 && drawUiButton(gm::localization::tr(gm::localization::TextId::LiveBreakPauseAudio),
                         audioCounts.playing > 0,
                         "No playing audio channels to pause.",
                         UiButtonRole::Secondary,
                         ImVec2(-1.0f, 0.0f),
                         "Start the break and pause every playing audio channel."))
            { _controller.startSessionBreak(true); return; }
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveEndSession), UiButtonRole::Destructive, ImVec2(-1.0f, 0.0f), "Ends the current live session and records an end timestamp."))
            ImGui::OpenPopup(gm::localization::tr(gm::localization::TextId::LiveEndSession));
    }

    if (!_controller._sessionPlanMessage.empty()) ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._sessionPlanMessage.c_str());
    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(gm::localization::tr(gm::localization::TextId::LiveStartSession), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto &review = _controller._sessionReview.start;
        if (!review.valid) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        ImGui::TextWrapped("%s", uiTextForLocale("Start the reviewed session in Setup with the current party?", "Die geprüfte Sitzung mit der aktuellen Gruppe in Vorbereitung starten?"));
        if (review.create) FormUi::InputText(uiTextForLocale("Session title", "Sitzungstitel"), &review.title);
        else drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveTitle), review.title.c_str());
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveInitialState), uiTextForLocale("Setup", "Vorbereitung"));
        if (!_controller._sessionPlanMessage.empty()) ImGui::TextWrapped("%s", _controller._sessionPlanMessage.c_str());
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveStart), UiButtonRole::Primary))
            if (_controller.startSession()) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        ImGui::SameLine();
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::CommonCancel), UiButtonRole::Secondary))
        { review = {}; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    setResponsiveAutoSizeWindow(620.0f, 400.0f);
    if (ImGui::BeginPopupModal(gm::localization::tr(gm::localization::TextId::LiveEndSession), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveEndDialogBody));
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveSession), session ? session->title.c_str() : gm::localization::tr(gm::localization::TextId::LiveNoActiveSession));
        drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveState), breakActive ? gm::localization::tr(gm::localization::TextId::LiveBreak) : gm::localization::tr(gm::localization::TextId::LiveRunning));
        if (!_controller._sessionPlanMessage.empty()) ImGui::TextWrapped("%s", _controller._sessionPlanMessage.c_str());
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveEndSession), UiButtonRole::Destructive)) {
            _controller.endActiveSession();
            if (!_controller.activeSession()) ImGui::CloseCurrentPopup();
            ImGui::EndPopup(); return;
        }
        ImGui::SameLine();
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::CommonCancel), UiButtonRole::Secondary)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
