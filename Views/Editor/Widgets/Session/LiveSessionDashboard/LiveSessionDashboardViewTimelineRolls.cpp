#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

void LiveSessionDashboardView::drawTimelineControls()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveTimeline));
    if (!_controller._currentCampaign.presentationModeActive)
    {
        const StoryBeatModel *beat = _controller.activeStoryBeat();
        if (_controller._timelineDateDraft.empty() && beat && !beat->inWorldDate.empty())
            _controller._timelineDateDraft = beat->inWorldDate;
        FormUi::InputText(gm::localization::tr(gm::localization::TextId::LiveEvent), &_controller._timelineTitleDraft);
        FormUi::InputText(gm::localization::tr(gm::localization::TextId::LiveInWorldDate), &_controller._timelineDateDraft);
        FormUi::InputTextMultiline(gm::localization::tr(gm::localization::TextId::CommonDescription), &_controller._timelineDescriptionDraft, ImVec2(-1.0f, 58.0f), ImGuiInputTextFlags_WordWrap);
        if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveAddTimelineEvent), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Add this event to the campaign timeline."))
            _controller.addTimelineEvent();
    }

    if (_controller._currentCampaign.timelineEvents.empty())
    {
        ImGui::TextDisabled(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveNoTimelineEvents));
    }
    else if (ImGui::BeginTable("LiveTimelineEvents", _controller._currentCampaign.presentationModeActive ? 3 : 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveDate), ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveEvent));
        ImGui::TableSetupColumn(gm::localization::tr(gm::localization::TextId::LiveNotes));
        if (!_controller._currentCampaign.presentationModeActive)
            ImGui::TableSetupColumn(localizedUiLabel("").c_str(), ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(gm::localization::tr(gm::localization::TextId::CommonRemove)).x + ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f);
        ImGui::TableHeadersRow();
        bool openTimelineEventRemoval = false;
        for (int i = 0; i < static_cast<int>(_controller._currentCampaign.timelineEvents.size()); ++i)
        {
            const TimelineEventModel &event = _controller._currentCampaign.timelineEvents[static_cast<std::size_t>(i)];
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            tableTextWrapped(InWorldTime::localizeDateTimeText(event.inWorldDate));
            ImGui::TableSetColumnIndex(1);
            tableTextWrapped(SessionControl::systemTextForDisplay(event.title));
            ImGui::TableSetColumnIndex(2);
            tableTextWrapped(SessionControl::systemTextForDisplay(event.description));
            if (!_controller._currentCampaign.presentationModeActive)
            {
                ImGui::TableSetColumnIndex(3);
                if (drawSmallUiButton(gm::localization::tr(gm::localization::TextId::CommonRemove), UiButtonRole::Destructive, "Remove this timeline event."))
                {
                    _pendingTimelineEventRemovalIndex = i;
                    openTimelineEventRemoval = true;
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (openTimelineEventRemoval)
            ImGui::OpenPopup(gm::localization::tr(gm::localization::TextId::LiveRemoveTimelineEvent));
        setResponsiveAutoSizeWindow(620.0f, 400.0f);
        if (ImGui::BeginPopupModal(gm::localization::tr(gm::localization::TextId::LiveRemoveTimelineEvent), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (_pendingTimelineEventRemovalIndex < 0 ||
                _pendingTimelineEventRemovalIndex >= static_cast<int>(_controller._currentCampaign.timelineEvents.size()))
            {
                ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveNoTimelineSelection));
            }
            else
            {
                const TimelineEventModel &event = _controller._currentCampaign.timelineEvents[static_cast<std::size_t>(_pendingTimelineEventRemovalIndex)];
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), gm::localization::tr(gm::localization::TextId::LiveRemoveTimelineQuestion));
                const std::string eventDate = event.inWorldDate.empty() ? "-" : InWorldTime::localizeDateTimeText(event.inWorldDate);
                const std::string eventTitle = event.title.empty() ? "-" : SessionControl::systemTextForDisplay(event.title);
                drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveDate), eventDate.c_str());
                drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveEvent), eventTitle.c_str());
            }

            const UiModalAction action = drawModalActionButtonsEx(gm::localization::tr(gm::localization::TextId::CommonRemove),
                                                                   UiButtonRole::Destructive,
                                                                   gm::localization::tr(gm::localization::TextId::CommonCancel),
                                                                   110.0f);
            if (action == UiModalAction::Confirm)
            {
                _controller.removeTimelineEvent(_pendingTimelineEventRemovalIndex);
                _pendingTimelineEventRemovalIndex = -1;
            }
            else if (action == UiModalAction::Cancel)
            {
                _pendingTimelineEventRemovalIndex = -1;
            }
            ImGui::EndPopup();
        }
    }
}

void LiveSessionDashboardView::drawQuickRolls()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveQuickRolls));
    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveRollD20), UiButtonRole::Secondary, ImVec2(94.0f, 0.0f), "Roll a quick d20 and keep the result visible."))
        _controller._lastD20Roll = _controller.rollDie(20);
    ImGui::SameLine();
    ImGui::Text(localizedUiLabel("d20: %s").c_str(), _controller._lastD20Roll > 0 ? std::to_string(_controller._lastD20Roll).c_str() : "-");
    if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveRollD100), UiButtonRole::Secondary, ImVec2(94.0f, 0.0f), "Roll a quick d100 and keep the result visible."))
        _controller._lastD100Roll = _controller.rollDie(100);
    ImGui::SameLine();
    ImGui::Text(localizedUiLabel("d100: %s").c_str(), _controller._lastD100Roll > 0 ? std::to_string(_controller._lastD100Roll).c_str() : "-");

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    ImGui::SeparatorText(uiTextForLocale("Campaign tables", "Kampagnentabellen"));
    if (_controller._currentCampaign.randomTables.empty())
    {
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(),
                           uiTextForLocale("Create reusable random tables in Campaign > Compendium.",
                                           "Erstelle wiederverwendbare Zufallstabellen unter Kampagne > Kompendium."));
        return;
    }
    std::vector<const char *> labels;
    labels.reserve(_controller._currentCampaign.randomTables.size());
    for (const RandomTableModel &table : _controller._currentCampaign.randomTables)
        labels.push_back(table.name.empty() ? uiTextForLocale("Unnamed table", "Unbenannte Tabelle") : table.name.c_str());
    _controller._selectedRandomTableIndex = std::clamp(
        _controller._selectedRandomTableIndex, 0, static_cast<int>(_controller._currentCampaign.randomTables.size()) - 1);
    FormUi::Combo(uiTextForLocale("Table", "Tabelle"), &_controller._selectedRandomTableIndex, labels.data(),
                  static_cast<int>(labels.size()));
    if (drawUiButton(uiTextForLocale("Roll table", "Tabelle würfeln"), UiButtonRole::Primary))
        _controller.rollSelectedRandomTable();
    if (!_controller._randomTableRollHistory.empty())
    {
        const RandomTableRollResult &latest = _controller._randomTableRollHistory.front();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), RandomTables::formatRoll(latest).c_str());
        if (!latest.gmNotes.empty())
            drawInlineLabelValue(uiTextForLocale("GM notes", "SL-Notizen"), latest.gmNotes.c_str());
        if (drawSmallUiButton(uiTextForLocale("Add to session notes", "Zu Sitzungsnotizen hinzufügen"),
                              UiButtonRole::Secondary))
            _controller.saveLatestRandomTableRollToNotes();
        TaskUi::sameLineIfFits();
        ImGui::BeginDisabled(!latest.playerSafe);
        if (drawSmallUiButton(uiTextForLocale("Create player handout", "Spieler-Handout erstellen"),
                              UiButtonRole::Secondary))
            _controller.createLatestRandomTableHandout();
        ImGui::EndDisabled();
        if (_controller._randomTableRollHistory.size() > 1 &&
            ImGui::CollapsingHeader(uiTextForLocale("Recent table rolls", "Letzte Tabellenwürfe")))
        {
            for (std::size_t index = 1; index < _controller._randomTableRollHistory.size(); ++index)
                ImGui::BulletText("%s", RandomTables::formatRoll(_controller._randomTableRollHistory[index]).c_str());
        }
    }
    if (!_controller._randomTableStatus.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), _controller._randomTableStatus.c_str());
}

void LiveSessionDashboardView::drawClockAndRolls()
{
    drawSessionLifecycleControls();
    drawClockCalendarControls();
    drawTimelineControls();
    drawQuickRolls();
}
