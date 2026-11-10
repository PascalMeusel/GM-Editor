#include "Views/Editor/Widgets/Session/LiveSessionDashboard/LiveSessionDashboardViewSupport.hpp"

void LiveSessionDashboardView::drawClockCalendarControls()
{
    ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveGameTime));
    CampaignClockModel &clock = _controller._currentCampaign.clock;
    const bool clockAdvancing = clock.useSystemTime || clock.running;
    const std::string clockMode = clock.useSystemTime ? gm::localization::tr(gm::localization::TextId::LiveSystemTime)
                                                     : (clock.running ? gm::localization::tr(gm::localization::TextId::LiveRunning)
                                                                      : gm::localization::tr(gm::localization::TextId::LivePaused));
    const std::string clockState = _controller.currentClockPhaseLabel() + " / " + clockMode + " / " + formatClockSpeed(clock.multiplier);
    drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveCurrent), _controller.currentInWorldClockLabel().c_str());
    drawInlineLabelValue(gm::localization::tr(gm::localization::TextId::LiveState), clockState.c_str());

    if (!_controller._currentCampaign.presentationModeActive)
    {
        const float clockControlsWidth = ImGui::GetContentRegionAvail().x;
        const int runControlColumns = clockControlsWidth >= 620.0f ? 3 : 1;
        if (ImGui::BeginTable("LiveSessionClockRunControls", runControlColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const auto placeRunControl = [runControlColumns](int index) {
                if (index % runControlColumns == 0)
                    ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(index % runControlColumns);
            };
            placeRunControl(0);
            if (drawUiButton(clockAdvancing
                                 ? gm::localization::tr(gm::localization::TextId::LivePauseClock)
                                 : gm::localization::tr(gm::localization::TextId::LiveRunClock),
                             clockAdvancing ? UiButtonRole::Secondary : UiButtonRole::Primary,
                             ImVec2(-1.0f, 0.0f),
                             "Toggle automatic in-world time progression."))
                _controller.toggleClockRunning();
            placeRunControl(1);
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputDouble("##ClockSpeedMultiplier", &_controller._clockMultiplierDraft, 0.1, 1.0, "%.2f");
            showHelpTooltip(gm::localization::trUi("Time speed multiplier for the in-world clock."));
            if (drawSmallUiButton(gm::localization::tr(gm::localization::TextId::LiveApplySpeed), UiButtonRole::Advanced, "Save the in-world clock speed."))
                _controller.applyClockMultiplier();
            placeRunControl(2);
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveUseSystemTime), UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Return the game clock to local system time."))
                _controller.resetClockToSystemTime();
            ImGui::EndTable();
        }

        ImGui::SeparatorText(gm::localization::tr(gm::localization::TextId::LiveDateTime));
        const int dateTimeColumns = clockControlsWidth >= 720.0f ? 5 : (clockControlsWidth >= 360.0f ? 2 : 1);
        if (ImGui::BeginTable("LiveSessionClockDateTime", dateTimeColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const auto placeDateTimeControl = [dateTimeColumns](int index) {
                if (index % dateTimeColumns == 0)
                    ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(index % dateTimeColumns);
            };
            placeDateTimeControl(0);
            ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveYear));
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("##ClockYear").c_str(), &_controller._clockYearDraft, 1, 10);
            placeDateTimeControl(1);
            ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveMonth));
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("##ClockMonth").c_str(), &_controller._clockMonthDraft, 1, 3);
            placeDateTimeControl(2);
            ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveDay));
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("##ClockDay").c_str(), &_controller._clockDayDraft, 1, 7);
            placeDateTimeControl(3);
            ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveHour));
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("##ClockHour").c_str(), &_controller._clockHourDraft, 1, 4);
            placeDateTimeControl(4);
            ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveMinute));
            ImGui::SetNextItemWidth(-1.0f);
            FormUi::InputInt(localizedUiLabel("##ClockMinute").c_str(), &_controller._clockMinuteDraft, 1, 10);
            ImGui::EndTable();
        }

        FormUi::InputText(gm::localization::tr(gm::localization::TextId::LiveCalendar), &_controller._calendarNameDraft);
        const int calendarShapeColumns = clockControlsWidth >= 720.0f ? 4 : (clockControlsWidth >= 520.0f ? 2 : 1);
        if (ImGui::BeginTable("LiveSessionCalendarShape", calendarShapeColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const auto placeCalendarControl = [calendarShapeColumns](int index) {
                if (index % calendarShapeColumns == 0)
                    ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(index % calendarShapeColumns);
            };
            placeCalendarControl(0);
            if (FormUi::InputInt(gm::localization::trUi("Months"), &_controller._calendarMonthCountDraft))
                _controller.resizeCalendarDrafts();
            placeCalendarControl(1);
            if (FormUi::InputInt(gm::localization::trUi("Weekdays"), &_controller._calendarWeekdayCountDraft))
                _controller.resizeCalendarDrafts();
            placeCalendarControl(2);
            if (FormUi::InputInt(gm::localization::trUi("Hours / Day"), &_controller._calendarHoursPerDayDraft))
                _controller.resizeCalendarDrafts();
            placeCalendarControl(3);
            if (FormUi::InputInt(gm::localization::trUi("Day Starts"), &_controller._calendarDayStartsAtHourDraft))
                _controller.resizeCalendarDrafts();
            placeCalendarControl(4);
            if (FormUi::InputInt(gm::localization::trUi("Daylight"), &_controller._calendarDaylightHoursDraft))
                _controller.resizeCalendarDrafts();
            placeCalendarControl(5);
            if (FormUi::InputInt(gm::localization::trUi("Night"), &_controller._calendarNightHoursDraft))
                _controller.resizeCalendarDrafts();
            ImGui::EndTable();
        }

        if (beginAdvancedHeader("Months / Calendar Shape", "Rename months and change month lengths only when editing the campaign calendar structure."))
        {
            _controller.resizeCalendarDrafts();
            if (ImGui::BeginTable("LiveSessionCalendarMonths", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings, ImVec2(-1.0f, 170.0f)))
            {
                ImGui::TableSetupColumn(gm::localization::trUi("Name"), ImGuiTableColumnFlags_WidthStretch, 0.72f);
                ImGui::TableSetupColumn(gm::localization::trUi("Days"), ImGuiTableColumnFlags_WidthStretch, 0.28f);
                ImGui::TableHeadersRow();
                for (int monthIndex = 0; monthIndex < _controller._calendarMonthCountDraft; ++monthIndex)
                {
                    ImGui::PushID(monthIndex);
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::SetNextItemWidth(-1.0f);
                    FormUi::InputText(localizedUiLabel("##MonthName").c_str(), &_controller._calendarMonthNameDrafts[static_cast<std::size_t>(monthIndex)]);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-1.0f);
                    if (FormUi::InputInt(localizedUiLabel("##MonthDays").c_str(), &_controller._calendarMonthDayDrafts[static_cast<std::size_t>(monthIndex)], 1, 7))
                        _controller.resizeCalendarDrafts();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        if (beginAdvancedHeader("Weekdays / Calendar Shape", "Rename weekdays only when editing the campaign calendar structure."))
        {
            _controller.resizeCalendarDrafts();
            if (ImGui::BeginTable("LiveSessionCalendarWeekdays", 1, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoSavedSettings, ImVec2(-1.0f, 130.0f)))
            {
                ImGui::TableSetupColumn(gm::localization::trUi("Name"));
                ImGui::TableHeadersRow();
                for (int dayIndex = 0; dayIndex < _controller._calendarWeekdayCountDraft; ++dayIndex)
                {
                    ImGui::PushID(dayIndex);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-1.0f);
                    FormUi::InputText(localizedUiLabel("##WeekdayName").c_str(), &_controller._calendarWeekdayNameDrafts[static_cast<std::size_t>(dayIndex)]);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        ImGui::TextUnformatted(gm::localization::tr(gm::localization::TextId::LiveChangeNote));
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputTextWithHint(localizedUiLabel("##GameTimeChangeNote").c_str(), gm::localization::tr(gm::localization::TextId::LiveOptionalSessionNote), &_controller._clockReasonDraft);
        ImGui::SetNextItemWidth(-1.0f);
        FormUi::InputTextWithHint(localizedUiLabel("##AdvanceGameTimeBy").c_str(), gm::localization::trUi("10 minutes, 2 hours, 1 day"), &_controller._clockAdvanceDraft);

        const int clockActionColumns = clockControlsWidth >= 620.0f ? 3 : 1;
        if (ImGui::BeginTable("LiveSessionClockControls", clockActionColumns, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
        {
            const auto placeClockAction = [clockActionColumns](int index) {
                if (index % clockActionColumns == 0)
                    ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(index % clockActionColumns);
            };
            placeClockAction(0);
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveSetTime), UiButtonRole::Primary, ImVec2(-1.0f, 0.0f), "Save the selected in-world date and time."))
                _controller.adjustSessionClock();
            placeClockAction(1);
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveAdvanceTime),
                             !_controller._clockAdvanceDraft.empty(),
                             "Enter an amount to advance.",
                             UiButtonRole::Secondary,
                             ImVec2(-1.0f, 0.0f),
                             "Advance the in-world clock from the current value."))
                _controller.advanceSessionClock();
            placeClockAction(2);
            if (drawUiButton(gm::localization::tr(gm::localization::TextId::LiveSaveCalendar), UiButtonRole::Advanced, ImVec2(-1.0f, 0.0f), "Save calendar structure and the selected in-world date and time."))
                _controller.saveCalendarSettings();
            ImGui::EndTable();
        }

        if (!_controller._clockStatusMessage.empty())
            ImGui::TextDisabled(localizedUiLabel("%s").c_str(), _controller._clockStatusMessage.c_str());
    }
}
