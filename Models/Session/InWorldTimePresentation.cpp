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
constexpr std::array<std::string_view, 12> kEnglishMonths = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"};
constexpr std::array<std::string_view, 12> kGermanMonths = {
    "Januar", "Februar", "März", "April", "Mai", "Juni",
    "Juli", "August", "September", "Oktober", "November", "Dezember"};
constexpr std::array<std::string_view, 7> kEnglishWeekdays = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
constexpr std::array<std::string_view, 7> kGermanWeekdays = {
    "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"};

bool germanUi()
{
    return gm::localization::LocalizationService::instance().locale() == gm::localization::Locale::German;
}

template <std::size_t Size>
std::string localizedKnownName(std::string_view value,
                               const std::array<std::string_view, Size> &english,
                               const std::array<std::string_view, Size> &german)
{
    for (std::size_t index = 0; index < Size; ++index)
    {
        if (value == english[index] || value == german[index])
            return std::string(germanUi() ? german[index] : english[index]);
    }
    return std::string(value);
}

bool startsWithNumberedSystemName(std::string_view value,
                                  std::string_view englishPrefix,
                                  std::string_view germanPrefix,
                                  std::string &number)
{
    std::string_view suffix;
    if (value.rfind(englishPrefix, 0) == 0)
        suffix = value.substr(englishPrefix.size());
    else if (value.rfind(germanPrefix, 0) == 0)
        suffix = value.substr(germanPrefix.size());
    else
        return false;

    if (suffix.empty() || !std::all_of(suffix.begin(), suffix.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        }))
        return false;
    number.assign(suffix);
    return true;
}

bool isTextBoundary(char character)
{
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalnum(value) == 0 && character != '_';
}

void replaceTextToken(std::string &text, std::string_view source, std::string_view replacement)
{
    if (source.empty() || source == replacement)
        return;
    std::size_t offset = 0;
    while ((offset = text.find(source, offset)) != std::string::npos)
    {
        const bool leftBoundary = offset == 0 || isTextBoundary(text[offset - 1]);
        const std::size_t after = offset + source.size();
        const bool rightBoundary = after == text.size() || isTextBoundary(text[after]);
        if (!leftBoundary || !rightBoundary)
        {
            offset = after;
            continue;
        }
        text.replace(offset, source.size(), replacement);
        offset += replacement.size();
    }
}

void replaceNumberedSystemTokens(std::string &text, std::string_view sourcePrefix, std::string_view replacementPrefix)
{
    std::size_t offset = 0;
    while ((offset = text.find(sourcePrefix, offset)) != std::string::npos)
    {
        if (offset > 0 && !isTextBoundary(text[offset - 1]))
        {
            offset += sourcePrefix.size();
            continue;
        }
        std::size_t numberEnd = offset + sourcePrefix.size();
        while (numberEnd < text.size() && std::isdigit(static_cast<unsigned char>(text[numberEnd])) != 0)
            ++numberEnd;
        if (numberEnd == offset + sourcePrefix.size() ||
            (numberEnd < text.size() && !isTextBoundary(text[numberEnd])))
        {
            offset += sourcePrefix.size();
            continue;
        }
        text.replace(offset, sourcePrefix.size(), replacementPrefix);
        offset = numberEnd + replacementPrefix.size() - sourcePrefix.size();
    }
}

bool containsFormattedClockTime(std::string_view text)
{
    for (std::size_t position = text.find(':'); position != std::string_view::npos; position = text.find(':', position + 1))
    {
        if (position >= 2 && position + 2 < text.size() &&
            std::isdigit(static_cast<unsigned char>(text[position - 2])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[position - 1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[position + 1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(text[position + 2])) != 0)
            return true;
    }
    return false;
}

std::string ordinalDay(int day)
{
    return std::to_string(std::max(1, day));
}
}

namespace InWorldTime
{
std::string localizedCalendarName(std::string_view name)
{
    if (name == "Gregorian Calendar" || name == "Gregorianischer Kalender")
        return germanUi() ? "Gregorianischer Kalender" : "Gregorian Calendar";
    if (name == "Campaign Calendar" || name == "Kampagnenkalender")
        return germanUi() ? "Kampagnenkalender" : "Campaign Calendar";
    return std::string(name);
}

std::string localizedMonthName(std::string_view name)
{
    const std::string known = localizedKnownName(name, kEnglishMonths, kGermanMonths);
    if (std::string_view(known) != name)
        return known;

    std::string number;
    if (startsWithNumberedSystemName(name, "Month ", "Monat ", number))
        return std::string(germanUi() ? "Monat " : "Month ") + number;
    return std::string(name);
}

std::string localizedWeekdayName(std::string_view name)
{
    const std::string known = localizedKnownName(name, kEnglishWeekdays, kGermanWeekdays);
    if (std::string_view(known) != name)
        return known;

    std::string number;
    if (startsWithNumberedSystemName(name, "Day ", "Tag ", number))
        return std::string(germanUi() ? "Tag " : "Day ") + number;
    return std::string(name);
}

std::string localizeDateTimeText(std::string_view input)
{
    // Timeline dates may be user-authored prose. Only the canonical clock
    // shape (which always contains HH:MM) is safe to translate token by token.
    if (!containsFormattedClockTime(input))
        return std::string(input);

    std::string text(input);
    const bool german = germanUi();
    for (std::size_t index = 0; index < kEnglishWeekdays.size(); ++index)
    {
        replaceTextToken(text, kEnglishWeekdays[index], german ? kGermanWeekdays[index] : kEnglishWeekdays[index]);
        replaceTextToken(text, kGermanWeekdays[index], german ? kGermanWeekdays[index] : kEnglishWeekdays[index]);
    }
    for (std::size_t index = 0; index < kEnglishMonths.size(); ++index)
    {
        replaceTextToken(text, kEnglishMonths[index], german ? kGermanMonths[index] : kEnglishMonths[index]);
        replaceTextToken(text, kGermanMonths[index], german ? kGermanMonths[index] : kEnglishMonths[index]);
    }
    replaceTextToken(text, "Gregorian Calendar", german ? "Gregorianischer Kalender" : "Gregorian Calendar");
    replaceTextToken(text, "Gregorianischer Kalender", german ? "Gregorianischer Kalender" : "Gregorian Calendar");
    replaceTextToken(text, "Campaign Calendar", german ? "Kampagnenkalender" : "Campaign Calendar");
    replaceTextToken(text, "Kampagnenkalender", german ? "Kampagnenkalender" : "Campaign Calendar");
    replaceNumberedSystemTokens(text, german ? "Month " : "Monat ", german ? "Monat " : "Month ");
    replaceNumberedSystemTokens(text, german ? "Day " : "Tag ", german ? "Tag " : "Day ");
    return text;
}

std::string formatDateTimeForDisplay(const CampaignClockModel &inputClock)
{
    CampaignClockModel clock = inputClock;
    normalizeClock(clock);
    const InWorldDateTime dateTime = currentDateTime(clock);
    const long long absoluteMinutes = toAbsoluteMinutes(dateTime, clock.calendar);
    const long long absoluteDays = absoluteMinutes / (static_cast<long long>(clock.calendar.hoursPerDay) * kMinutesPerHour);
    const std::string weekday = clock.calendar.weekdays.empty()
                                    ? std::string{}
                                    : localizedWeekdayName(clock.calendar.weekdays[static_cast<std::size_t>(absoluteDays % clock.calendar.weekdays.size())]);
    const CalendarMonthModel &month = clock.calendar.months[static_cast<std::size_t>(std::clamp(dateTime.month, 1, static_cast<int>(clock.calendar.months.size())) - 1)];

    std::ostringstream stream;
    if (!weekday.empty())
        stream << weekday << ", ";
    stream << ordinalDay(dateTime.day) << " " << localizedMonthName(month.name) << " " << dateTime.year << " ";
    stream << std::setfill('0') << std::setw(2) << dateTime.hour << ":" << std::setw(2) << dateTime.minute;
    if (!clock.calendar.name.empty())
        stream << " (" << localizedCalendarName(clock.calendar.name) << ")";
    return stream.str();
}

std::string phaseLabelForDisplay(const CampaignClockModel &clock)
{
    return gm::localization::trUi(phaseLabel(clock).c_str());
}
}

