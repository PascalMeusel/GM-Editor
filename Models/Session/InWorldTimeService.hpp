#pragma once

#include "Models/Session/InWorldTimeModel.hpp"

#include <string>
#include <string_view>

namespace InWorldTime
{
CampaignCalendarModel defaultGregorianCalendar();
void normalizeCalendar(CampaignCalendarModel &calendar);
void normalizeClock(CampaignClockModel &clock);

InWorldDateTime localSystemDateTime();
InWorldDateTime currentDateTime(const CampaignClockModel &clock);
long long toAbsoluteMinutes(const InWorldDateTime &dateTime, const CampaignCalendarModel &calendar);
InWorldDateTime fromAbsoluteMinutes(long long absoluteMinutes, const CampaignCalendarModel &calendar);

std::string formatDateTime(const CampaignClockModel &clock);
std::string phaseLabel(const CampaignClockModel &clock);

// The clock and calendar are persisted with stable, language-neutral defaults.
// These presentation helpers translate only known system-provided names; custom
// calendar, month, and weekday names always pass through unchanged.
std::string localizedCalendarName(std::string_view name);
std::string localizedMonthName(std::string_view name);
std::string localizedWeekdayName(std::string_view name);
std::string localizeDateTimeText(std::string_view text);
std::string formatDateTimeForDisplay(const CampaignClockModel &clock);
std::string phaseLabelForDisplay(const CampaignClockModel &clock);
std::string currentUtcAnchor();

void setUseSystemTime(CampaignClockModel &clock);
void setDateTime(CampaignClockModel &clock, const InWorldDateTime &dateTime, bool keepRunning = true);
void advanceMinutes(CampaignClockModel &clock, long long minutes);
void setRunning(CampaignClockModel &clock, bool running);
void setMultiplier(CampaignClockModel &clock, double multiplier);

bool parseDurationMinutes(const std::string &text, const CampaignCalendarModel &calendar, long long &minutes);
}
