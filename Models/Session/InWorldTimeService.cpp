#include "InWorldTimeService.hpp"

#include "Models/Core/EntityMetadata.hpp"
#include "Models/Localization/LocalizationService.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
constexpr long long kMinutesPerHour = 60;

std::vector<CalendarMonthModel> defaultMonths()
{
    return {{"January", 31},
            {"February", 28},
            {"March", 31},
            {"April", 30},
            {"May", 31},
            {"June", 30},
            {"July", 31},
            {"August", 31},
            {"September", 30},
            {"October", 31},
            {"November", 30},
            {"December", 31}};
}

std::vector<std::string> defaultWeekdays()
{
    return {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string trimCopy(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    return first >= last ? std::string{} : std::string(first, last);
}

int daysPerYear(const CampaignCalendarModel &calendar)
{
    int total = 0;
    for (const CalendarMonthModel &month : calendar.months)
        total += std::max(1, month.days);
    return std::max(1, total);
}

std::chrono::system_clock::time_point parseUtcTimestamp(const std::string &value)
{
    if (value.size() < 20)
        return std::chrono::system_clock::now();

    std::tm parsed{};
    std::istringstream stream(value);
    stream >> std::get_time(&parsed, "%Y-%m-%dT%H:%M:%SZ");
    if (stream.fail())
        return std::chrono::system_clock::now();

#if defined(_WIN32)
    const std::time_t timestamp = _mkgmtime(&parsed);
#else
    const std::time_t timestamp = timegm(&parsed);
#endif
    if (timestamp == static_cast<std::time_t>(-1))
        return std::chrono::system_clock::now();
    return std::chrono::system_clock::from_time_t(timestamp);
}

std::string ordinalDay(int day)
{
    return std::to_string(std::max(1, day));
}

long long clampAbsoluteMinutes(long long value)
{
    return std::max(0LL, value);
}
}

namespace InWorldTime
{
CampaignCalendarModel defaultGregorianCalendar()
{
    CampaignCalendarModel calendar;
    calendar.name = "Gregorian Calendar";
    calendar.months = defaultMonths();
    calendar.weekdays = defaultWeekdays();
    calendar.hoursPerDay = 24;
    calendar.daylightHours = 12;
    calendar.nightHours = 12;
    calendar.dayStartsAtHour = 6;
    return calendar;
}

void normalizeCalendar(CampaignCalendarModel &calendar)
{
    if (calendar.name.empty())
        calendar.name = "Campaign Calendar";
    if (calendar.months.empty())
        calendar.months = defaultMonths();
    if (calendar.weekdays.empty())
        calendar.weekdays = defaultWeekdays();

    calendar.hoursPerDay = std::clamp(calendar.hoursPerDay, 1, 240);
    calendar.daylightHours = std::clamp(calendar.daylightHours, 0, calendar.hoursPerDay);
    calendar.nightHours = std::clamp(calendar.nightHours, 0, calendar.hoursPerDay - calendar.daylightHours);
    calendar.dayStartsAtHour = std::clamp(calendar.dayStartsAtHour, 0, calendar.hoursPerDay - 1);

    for (std::size_t index = 0; index < calendar.months.size(); ++index)
    {
        CalendarMonthModel &month = calendar.months[index];
        if (month.name.empty())
            month.name = "Month " + std::to_string(index + 1);
        month.days = std::clamp(month.days, 1, 1000);
    }
    for (std::size_t index = 0; index < calendar.weekdays.size(); ++index)
    {
        if (calendar.weekdays[index].empty())
            calendar.weekdays[index] = "Day " + std::to_string(index + 1);
    }
}

void normalizeClock(CampaignClockModel &clock)
{
    normalizeCalendar(clock.calendar);
    clock.multiplier = std::clamp(clock.multiplier, 0.0, 10000.0);
    clock.anchorAbsoluteMinutes = clampAbsoluteMinutes(clock.anchorAbsoluteMinutes);
    if (clock.anchorUtc.empty())
        clock.anchorUtc = currentUtcTimestamp();
}

InWorldDateTime localSystemDateTime()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    InWorldDateTime dateTime;
    dateTime.year = localTime.tm_year + 1900;
    dateTime.month = localTime.tm_mon + 1;
    dateTime.day = localTime.tm_mday;
    dateTime.hour = localTime.tm_hour;
    dateTime.minute = localTime.tm_min;
    return dateTime;
}

long long toAbsoluteMinutes(const InWorldDateTime &dateTime, const CampaignCalendarModel &inputCalendar)
{
    CampaignCalendarModel calendar = inputCalendar;
    normalizeCalendar(calendar);

    const int year = std::max(1, dateTime.year);
    const int month = std::clamp(dateTime.month, 1, static_cast<int>(calendar.months.size()));
    const int day = std::clamp(dateTime.day, 1, calendar.months[static_cast<std::size_t>(month - 1)].days);
    const int hour = std::clamp(dateTime.hour, 0, calendar.hoursPerDay - 1);
    const int minute = std::clamp(dateTime.minute, 0, 59);

    long long days = static_cast<long long>(year - 1) * daysPerYear(calendar);
    for (int index = 0; index < month - 1; ++index)
        days += calendar.months[static_cast<std::size_t>(index)].days;
    days += day - 1;
    return days * calendar.hoursPerDay * kMinutesPerHour + hour * kMinutesPerHour + minute;
}

InWorldDateTime fromAbsoluteMinutes(long long absoluteMinutes, const CampaignCalendarModel &inputCalendar)
{
    CampaignCalendarModel calendar = inputCalendar;
    normalizeCalendar(calendar);

    absoluteMinutes = clampAbsoluteMinutes(absoluteMinutes);
    const long long minutesPerDay = static_cast<long long>(calendar.hoursPerDay) * kMinutesPerHour;
    long long totalDays = absoluteMinutes / minutesPerDay;
    long long minutesIntoDay = absoluteMinutes % minutesPerDay;

    const int yearDays = daysPerYear(calendar);
    InWorldDateTime result;
    result.year = static_cast<int>(totalDays / yearDays) + 1;
    int dayOfYear = static_cast<int>(totalDays % yearDays);
    result.month = 1;
    for (std::size_t index = 0; index < calendar.months.size(); ++index)
    {
        const int monthDays = calendar.months[index].days;
        if (dayOfYear < monthDays)
        {
            result.month = static_cast<int>(index) + 1;
            result.day = dayOfYear + 1;
            break;
        }
        dayOfYear -= monthDays;
    }
    result.hour = static_cast<int>(minutesIntoDay / kMinutesPerHour);
    result.minute = static_cast<int>(minutesIntoDay % kMinutesPerHour);
    return result;
}

InWorldDateTime currentDateTime(const CampaignClockModel &inputClock)
{
    CampaignClockModel clock = inputClock;
    normalizeClock(clock);
    if (clock.useSystemTime)
        return localSystemDateTime();

    long long absoluteMinutes = clock.anchorAbsoluteMinutes;
    if (clock.running && clock.multiplier > 0.0)
    {
        const auto anchor = parseUtcTimestamp(clock.anchorUtc);
        const auto elapsed = std::chrono::system_clock::now() - anchor;
        const double realMinutes = std::chrono::duration<double, std::ratio<60>>(elapsed).count();
        const double gameMinutes = realMinutes * clock.multiplier;
        absoluteMinutes += static_cast<long long>(std::floor(gameMinutes));
    }
    return fromAbsoluteMinutes(absoluteMinutes, clock.calendar);
}

std::string formatDateTime(const CampaignClockModel &inputClock)
{
    CampaignClockModel clock = inputClock;
    normalizeClock(clock);
    const InWorldDateTime dateTime = currentDateTime(clock);
    const long long absoluteMinutes = toAbsoluteMinutes(dateTime, clock.calendar);
    const long long absoluteDays = absoluteMinutes / (static_cast<long long>(clock.calendar.hoursPerDay) * kMinutesPerHour);
    const std::string weekday = clock.calendar.weekdays.empty()
                                    ? std::string{}
                                    : clock.calendar.weekdays[static_cast<std::size_t>(absoluteDays % clock.calendar.weekdays.size())];
    const CalendarMonthModel &month = clock.calendar.months[static_cast<std::size_t>(std::clamp(dateTime.month, 1, static_cast<int>(clock.calendar.months.size())) - 1)];

    std::ostringstream stream;
    if (!weekday.empty())
        stream << weekday << ", ";
    stream << ordinalDay(dateTime.day) << " " << month.name << " " << dateTime.year << " ";
    stream << std::setfill('0') << std::setw(2) << dateTime.hour << ":" << std::setw(2) << dateTime.minute;
    if (!clock.calendar.name.empty())
        stream << " (" << clock.calendar.name << ")";
    return stream.str();
}

std::string phaseLabel(const CampaignClockModel &inputClock)
{
    CampaignClockModel clock = inputClock;
    normalizeClock(clock);
    const InWorldDateTime dateTime = currentDateTime(clock);
    const int hour = ((dateTime.hour - clock.calendar.dayStartsAtHour) % clock.calendar.hoursPerDay + clock.calendar.hoursPerDay) % clock.calendar.hoursPerDay;
    if (hour < clock.calendar.daylightHours)
        return "Day";
    if (hour < clock.calendar.daylightHours + clock.calendar.nightHours)
        return "Night";
    return "Twilight";
}

std::string currentUtcAnchor()
{
    return currentUtcTimestamp();
}

void setUseSystemTime(CampaignClockModel &clock)
{
    clock.useSystemTime = true;
    clock.running = true;
    clock.multiplier = 1.0;
    clock.anchorUtc = currentUtcTimestamp();
    clock.anchorAbsoluteMinutes = toAbsoluteMinutes(localSystemDateTime(), clock.calendar);
}

void setDateTime(CampaignClockModel &clock, const InWorldDateTime &dateTime, bool keepRunning)
{
    normalizeClock(clock);
    clock.useSystemTime = false;
    clock.anchorAbsoluteMinutes = toAbsoluteMinutes(dateTime, clock.calendar);
    clock.anchorUtc = currentUtcTimestamp();
    clock.running = keepRunning;
}

void advanceMinutes(CampaignClockModel &clock, long long minutes)
{
    normalizeClock(clock);
    const long long current = toAbsoluteMinutes(currentDateTime(clock), clock.calendar);
    clock.useSystemTime = false;
    clock.anchorAbsoluteMinutes = clampAbsoluteMinutes(current + minutes);
    clock.anchorUtc = currentUtcTimestamp();
}

void setRunning(CampaignClockModel &clock, bool running)
{
    normalizeClock(clock);
    const long long current = toAbsoluteMinutes(currentDateTime(clock), clock.calendar);
    clock.useSystemTime = false;
    clock.anchorAbsoluteMinutes = current;
    clock.anchorUtc = currentUtcTimestamp();
    clock.running = running;
}

void setMultiplier(CampaignClockModel &clock, double multiplier)
{
    normalizeClock(clock);
    const bool wasRunning = clock.running;
    const long long current = toAbsoluteMinutes(currentDateTime(clock), clock.calendar);
    clock.useSystemTime = false;
    clock.anchorAbsoluteMinutes = current;
    clock.anchorUtc = currentUtcTimestamp();
    clock.multiplier = std::clamp(multiplier, 0.0, 10000.0);
    clock.running = wasRunning;
}

bool parseDurationMinutes(const std::string &text, const CampaignCalendarModel &inputCalendar, long long &minutes)
{
    CampaignCalendarModel calendar = inputCalendar;
    normalizeCalendar(calendar);
    const std::string trimmed = trimCopy(text);
    if (trimmed.empty())
        return false;

    std::istringstream stream(trimmed);
    double amount = 0.0;
    std::string unit;
    stream >> amount;
    stream >> unit;
    if (stream.fail() || amount <= 0.0)
        return false;
    unit = lowerCopy(unit);

    double factor = 1.0;
    if (unit.empty() || unit == "minute" || unit == "minutes" || unit == "min" || unit == "mins" || unit == "m" || unit == "minuten")
        factor = 1.0;
    else if (unit == "hour" || unit == "hours" || unit == "hr" || unit == "hrs" || unit == "h" || unit == "stunde" || unit == "stunden" || unit == "std")
        factor = 60.0;
    else if (unit == "day" || unit == "days" || unit == "d" || unit == "tag" || unit == "tage" || unit == "tagen")
        factor = static_cast<double>(calendar.hoursPerDay) * 60.0;
    else if (unit == "watch" || unit == "watches" || unit == "wache" || unit == "wachen")
        factor = std::min(4.0, static_cast<double>(calendar.hoursPerDay)) * 60.0;
    else if (unit == "week" || unit == "weeks" || unit == "w" || unit == "woche" || unit == "wochen")
        factor = static_cast<double>(std::max(1, static_cast<int>(calendar.weekdays.size()))) * calendar.hoursPerDay * 60.0;
    else if (unit == "month" || unit == "months" || unit == "monat" || unit == "monate" || unit == "monaten")
        factor = static_cast<double>(calendar.months.front().days) * calendar.hoursPerDay * 60.0;
    else if (unit == "year" || unit == "years" || unit == "y" || unit == "jahr" || unit == "jahre" || unit == "jahren" || unit == "j")
        factor = static_cast<double>(daysPerYear(calendar)) * calendar.hoursPerDay * 60.0;
    else
        return false;

    minutes = static_cast<long long>(std::llround(amount * factor));
    return minutes > 0;
}
}
