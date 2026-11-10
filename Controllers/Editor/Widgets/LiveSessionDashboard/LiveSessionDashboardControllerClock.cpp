#include "Controllers/Editor/Widgets/LiveSessionDashboard/LiveSessionDashboardControllerSupport.hpp"

void LiveSessionDashboardController::syncClockDraftsFromCampaign()
{
    CampaignClockModel clock = _currentCampaign.clock;
    InWorldTime::normalizeClock(clock);
    const InWorldDateTime dateTime = InWorldTime::currentDateTime(clock);

    _clockYearDraft = dateTime.year;
    _clockMonthDraft = dateTime.month;
    _clockDayDraft = dateTime.day;
    _clockHourDraft = dateTime.hour;
    _clockMinuteDraft = dateTime.minute;
    _clockMultiplierDraft = clock.multiplier;
    _calendarNameDraft = InWorldTime::localizedCalendarName(clock.calendar.name);
    _calendarHoursPerDayDraft = clock.calendar.hoursPerDay;
    _calendarDaylightHoursDraft = clock.calendar.daylightHours;
    _calendarNightHoursDraft = clock.calendar.nightHours;
    _calendarDayStartsAtHourDraft = clock.calendar.dayStartsAtHour;
    _calendarMonthCountDraft = static_cast<int>(clock.calendar.months.size());
    _calendarWeekdayCountDraft = static_cast<int>(clock.calendar.weekdays.size());
    _calendarMonthNameDrafts.clear();
    _calendarMonthDayDrafts.clear();
    _calendarWeekdayNameDrafts.clear();
    for (const CalendarMonthModel &month : clock.calendar.months)
    {
        _calendarMonthNameDrafts.push_back(InWorldTime::localizedMonthName(month.name));
        _calendarMonthDayDrafts.push_back(month.days);
    }
    for (const std::string &weekday : clock.calendar.weekdays)
        _calendarWeekdayNameDrafts.push_back(InWorldTime::localizedWeekdayName(weekday));
    if (_clockAdvanceDraft == "10 minutes" || _clockAdvanceDraft == "10 Minuten")
        _clockAdvanceDraft = gm::localization::trUi("10 minutes");
    resizeCalendarDrafts();
    _clockConfigDraftKey = clockDraftKey();
}

void LiveSessionDashboardController::resizeCalendarDrafts()
{
    _calendarMonthCountDraft = std::clamp(_calendarMonthCountDraft, 1, 60);
    _calendarWeekdayCountDraft = std::clamp(_calendarWeekdayCountDraft, 1, 30);

    const std::size_t monthCount = static_cast<std::size_t>(_calendarMonthCountDraft);
    while (_calendarMonthNameDrafts.size() < monthCount)
        _calendarMonthNameDrafts.push_back(std::string(gm::localization::trUi("Month")) + " " + std::to_string(_calendarMonthNameDrafts.size() + 1));
    while (_calendarMonthDayDrafts.size() < monthCount)
        _calendarMonthDayDrafts.push_back(30);
    _calendarMonthNameDrafts.resize(monthCount);
    _calendarMonthDayDrafts.resize(monthCount);
    for (std::size_t index = 0; index < monthCount; ++index)
    {
        if (_calendarMonthNameDrafts[index].empty())
            _calendarMonthNameDrafts[index] = std::string(gm::localization::trUi("Month")) + " " + std::to_string(index + 1);
        _calendarMonthDayDrafts[index] = std::clamp(_calendarMonthDayDrafts[index], 1, 1000);
    }

    const std::size_t weekdayCount = static_cast<std::size_t>(_calendarWeekdayCountDraft);
    while (_calendarWeekdayNameDrafts.size() < weekdayCount)
        _calendarWeekdayNameDrafts.push_back(std::string(gm::localization::trUi("Day")) + " " + std::to_string(_calendarWeekdayNameDrafts.size() + 1));
    _calendarWeekdayNameDrafts.resize(weekdayCount);
    for (std::size_t index = 0; index < weekdayCount; ++index)
    {
        if (_calendarWeekdayNameDrafts[index].empty())
            _calendarWeekdayNameDrafts[index] = std::string(gm::localization::trUi("Day")) + " " + std::to_string(index + 1);
    }

    _calendarHoursPerDayDraft = std::clamp(_calendarHoursPerDayDraft, 1, 240);
    _calendarDaylightHoursDraft = std::clamp(_calendarDaylightHoursDraft, 0, _calendarHoursPerDayDraft);
    _calendarNightHoursDraft = std::clamp(_calendarNightHoursDraft, 0, _calendarHoursPerDayDraft - _calendarDaylightHoursDraft);
    _calendarDayStartsAtHourDraft = std::clamp(_calendarDayStartsAtHourDraft, 0, _calendarHoursPerDayDraft - 1);
}

CampaignCalendarModel LiveSessionDashboardController::draftCalendar() const
{
    CampaignCalendarModel calendar;
    calendar.name = _calendarNameDraft.empty() ? gm::localization::trUi("Campaign Calendar") : _calendarNameDraft;
    calendar.hoursPerDay = _calendarHoursPerDayDraft;
    calendar.daylightHours = _calendarDaylightHoursDraft;
    calendar.nightHours = _calendarNightHoursDraft;
    calendar.dayStartsAtHour = _calendarDayStartsAtHourDraft;

    const int monthCount = std::clamp(_calendarMonthCountDraft, 1, 60);
    for (int index = 0; index < monthCount; ++index)
    {
        CalendarMonthModel month;
        if (index < static_cast<int>(_calendarMonthNameDrafts.size()))
            month.name = _calendarMonthNameDrafts[static_cast<std::size_t>(index)];
        if (index < static_cast<int>(_calendarMonthDayDrafts.size()))
            month.days = _calendarMonthDayDrafts[static_cast<std::size_t>(index)];
        if (month.name.empty())
            month.name = std::string(gm::localization::trUi("Month")) + " " + std::to_string(index + 1);
        month.days = std::clamp(month.days, 1, 1000);
        calendar.months.push_back(month);
    }

    const int weekdayCount = std::clamp(_calendarWeekdayCountDraft, 1, 30);
    for (int index = 0; index < weekdayCount; ++index)
    {
        std::string weekday = index < static_cast<int>(_calendarWeekdayNameDrafts.size())
                                  ? _calendarWeekdayNameDrafts[static_cast<std::size_t>(index)]
                                  : std::string{};
        if (weekday.empty())
            weekday = std::string(gm::localization::trUi("Day")) + " " + std::to_string(index + 1);
        calendar.weekdays.push_back(weekday);
    }

    InWorldTime::normalizeCalendar(calendar);
    return calendar;
}

InWorldDateTime LiveSessionDashboardController::draftDateTime() const
{
    CampaignCalendarModel calendar = draftCalendar();
    InWorldDateTime dateTime;
    dateTime.year = std::max(1, _clockYearDraft);
    dateTime.month = std::clamp(_clockMonthDraft, 1, static_cast<int>(calendar.months.size()));
    dateTime.day = std::clamp(_clockDayDraft, 1, calendar.months[static_cast<std::size_t>(dateTime.month - 1)].days);
    dateTime.hour = std::clamp(_clockHourDraft, 0, calendar.hoursPerDay - 1);
    dateTime.minute = std::clamp(_clockMinuteDraft, 0, 59);
    return dateTime;
}

void LiveSessionDashboardController::advanceSessionClock()
{
    const auto result = SessionPlan::advanceClock(_currentCampaign, _clockAdvanceDraft, _clockReasonDraft,
                                                  sessionPersist("advance game time"));
    if (!acceptSessionResult(result))
    {
        _clockStatusMessage = result.status == SessionPlan::Status::Invalid
            ? gm::localization::tr(gm::localization::TextId::LiveClockInvalidAmount) : _sessionPlanMessage;
        // Keep the amount and reason for a retry. The transaction also keeps
        // the clock and both note collections at their last committed values.
        return;
    }

    _clockReasonDraft.clear();
    _clockStatusMessage = gm::localization::tr(activeSession() ? gm::localization::TextId::LiveClockAdvancedSaved
                                                               : gm::localization::TextId::LiveClockAdvanced);
    syncClockDraftsFromCampaign();
}

void LiveSessionDashboardController::adjustSessionClock()
{
    const auto result = SessionPlan::setClockDateTime(_currentCampaign, draftDateTime(), _clockReasonDraft,
                                                     sessionPersist("set game time"));
    if (!acceptSessionResult(result))
    {
        _clockStatusMessage = result.status == SessionPlan::Status::Invalid
            ? gm::localization::tr(gm::localization::TextId::LiveClockSetFailed) : _sessionPlanMessage;
        return;
    }

    _clockReasonDraft.clear();
    _clockStatusMessage = gm::localization::tr(gm::localization::TextId::LiveClockSet);
    syncClockDraftsFromCampaign();
}

void LiveSessionDashboardController::saveCalendarSettings()
{
    if (!acceptSessionResult(SessionPlan::setClockCalendar(_currentCampaign, draftCalendar(), draftDateTime(),
                                                           _clockMultiplierDraft, sessionPersist("update campaign calendar"))))
    {
        _clockStatusMessage = _sessionPlanMessage;
        return;
    }
    _clockStatusMessage = gm::localization::tr(gm::localization::TextId::LiveCalendarSaved);
    syncClockDraftsFromCampaign();
}

void LiveSessionDashboardController::toggleClockRunning()
{
    const bool advancing = _currentCampaign.clock.useSystemTime || _currentCampaign.clock.running;
    if (!acceptSessionResult(SessionPlan::setClockRunning(_currentCampaign, !advancing,
            sessionPersist(advancing ? "pause game clock" : "run game clock"))))
    {
        _clockStatusMessage = _sessionPlanMessage;
        return;
    }
    _clockStatusMessage = gm::localization::tr(advancing ? gm::localization::TextId::LiveClockPausedStatus
                                                        : gm::localization::TextId::LiveClockRunningStatus);
    syncClockDraftsFromCampaign();
}

void LiveSessionDashboardController::applyClockMultiplier()
{
    if (!acceptSessionResult(SessionPlan::setClockMultiplier(_currentCampaign, _clockMultiplierDraft,
                                                             sessionPersist("update game clock speed"))))
    {
        _clockStatusMessage = _sessionPlanMessage;
        return;
    }
    _clockStatusMessage = gm::localization::tr(gm::localization::TextId::LiveClockSpeedSaved);
    syncClockDraftsFromCampaign();
}

void LiveSessionDashboardController::resetClockToSystemTime()
{
    if (!acceptSessionResult(SessionPlan::resetClockToSystemTime(_currentCampaign, sessionPersist("reset game clock to system time"))))
    {
        _clockStatusMessage = _sessionPlanMessage;
        return;
    }
    _clockStatusMessage = gm::localization::tr(gm::localization::TextId::LiveClockSystemStatus);
    syncClockDraftsFromCampaign();
}
