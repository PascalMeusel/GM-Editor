#include "Models/Localization/LocaleFormatting.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

namespace gm::localization
{
namespace
{
std::string groupDigits(std::string digits, char separator)
{
    const std::size_t firstDigit = !digits.empty() && (digits[0] == '-' || digits[0] == '+') ? 1 : 0;
    for (std::size_t position = digits.size(); position > firstDigit + 3; position -= 3)
        digits.insert(position - 3, 1, separator);
    return digits;
}

std::tm localTime(std::chrono::system_clock::time_point value)
{
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(value);
    std::tm result{};
#if defined(_WIN32)
    localtime_s(&result, &timestamp);
#else
    localtime_r(&timestamp, &result);
#endif
    return result;
}

std::string join(const std::vector<std::string> &parts, const char *separator)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index > 0)
            output << separator;
        output << parts[index];
    }
    return output.str();
}

std::string unit(std::int64_t value, const char *singular, const char *plural)
{
    return std::to_string(value) + " " + (value == 1 ? singular : plural);
}
} // namespace

std::string formatInteger(std::int64_t value, Locale locale)
{
    return groupDigits(std::to_string(value), locale == Locale::German ? '.' : ',');
}

std::string formatNumber(double value, int fractionalDigits, Locale locale)
{
    if (!std::isfinite(value))
        return "—";

    fractionalDigits = std::clamp(fractionalDigits, 0, 9);
    std::ostringstream output;
    output << std::fixed << std::setprecision(fractionalDigits) << value;
    std::string rendered = output.str();
    const std::size_t decimal = rendered.find('.');
    const std::size_t integerEnd = decimal == std::string::npos ? rendered.size() : decimal;
    std::string integerPart = groupDigits(rendered.substr(0, integerEnd), locale == Locale::German ? '.' : ',');
    if (decimal == std::string::npos)
        return integerPart;
    return integerPart + (locale == Locale::German ? ',' : '.') + rendered.substr(decimal + 1);
}

std::string formatPercent(double ratio, int fractionalDigits, Locale locale)
{
    return formatNumber(ratio * 100.0, fractionalDigits, locale) + (locale == Locale::German ? " %" : "%");
}

std::string formatDate(std::chrono::system_clock::time_point value, Locale locale)
{
    const std::tm time = localTime(value);
    std::ostringstream output;
    if (locale == Locale::German)
    {
        output << std::setfill('0') << std::setw(2) << time.tm_mday << '.'
               << std::setw(2) << (time.tm_mon + 1) << '.' << (time.tm_year + 1900);
    }
    else
    {
        constexpr std::array<const char *, 12> months = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        const int month = std::clamp(time.tm_mon, 0, 11);
        output << months[static_cast<std::size_t>(month)] << ' ' << time.tm_mday << ", " << (time.tm_year + 1900);
    }
    return output.str();
}

std::string formatTime(std::chrono::system_clock::time_point value, Locale locale, bool includeSeconds)
{
    const std::tm time = localTime(value);
    std::ostringstream output;
    output << std::setfill('0');
    if (locale == Locale::German)
    {
        output << std::setw(2) << time.tm_hour << ':' << std::setw(2) << time.tm_min;
        if (includeSeconds)
            output << ':' << std::setw(2) << time.tm_sec;
        return output.str();
    }

    const int hour = time.tm_hour % 12 == 0 ? 12 : time.tm_hour % 12;
    output << hour << ':' << std::setw(2) << time.tm_min;
    if (includeSeconds)
        output << ':' << std::setw(2) << time.tm_sec;
    output << (time.tm_hour < 12 ? " AM" : " PM");
    return output.str();
}

std::string formatDuration(std::chrono::seconds value, Locale locale, bool compact)
{
    std::int64_t total = value.count();
    const bool negative = total < 0;
    std::uint64_t remaining = negative
                                  ? static_cast<std::uint64_t>(-(total + 1)) + 1U
                                  : static_cast<std::uint64_t>(total);
    const std::int64_t days = static_cast<std::int64_t>(remaining / 86400U);
    remaining %= 86400U;
    const std::int64_t hours = static_cast<std::int64_t>(remaining / 3600U);
    remaining %= 3600U;
    const std::int64_t minutes = static_cast<std::int64_t>(remaining / 60U);
    const std::int64_t seconds = static_cast<std::int64_t>(remaining % 60U);

    std::vector<std::string> parts;
    if (compact)
    {
        if (days > 0)
            parts.push_back(std::to_string(days) + (locale == Locale::German ? " T" : "d"));
        if (hours > 0)
            parts.push_back(std::to_string(hours) + (locale == Locale::German ? " Std" : "h"));
        if (minutes > 0)
            parts.push_back(std::to_string(minutes) + (locale == Locale::German ? " Min" : "m"));
        if (seconds > 0 || parts.empty())
            parts.push_back(std::to_string(seconds) + (locale == Locale::German ? " Sek" : "s"));
    }
    else if (locale == Locale::German)
    {
        if (days > 0)
            parts.push_back(unit(days, "Tag", "Tage"));
        if (hours > 0)
            parts.push_back(unit(hours, "Stunde", "Stunden"));
        if (minutes > 0)
            parts.push_back(unit(minutes, "Minute", "Minuten"));
        if (seconds > 0 || parts.empty())
            parts.push_back(unit(seconds, "Sekunde", "Sekunden"));
    }
    else
    {
        if (days > 0)
            parts.push_back(unit(days, "day", "days"));
        if (hours > 0)
            parts.push_back(unit(hours, "hour", "hours"));
        if (minutes > 0)
            parts.push_back(unit(minutes, "minute", "minutes"));
        if (seconds > 0 || parts.empty())
            parts.push_back(unit(seconds, "second", "seconds"));
    }

    return (negative ? "−" : "") + join(parts, compact ? " " : ", ");
}
} // namespace gm::localization
