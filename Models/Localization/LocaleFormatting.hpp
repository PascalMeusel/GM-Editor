#pragma once

#include "Models/Localization/Locale.hpp"

#include <chrono>
#include <cstdint>
#include <string>

namespace gm::localization
{
std::string formatInteger(std::int64_t value, Locale locale);
std::string formatNumber(double value, int fractionalDigits, Locale locale);
std::string formatPercent(double ratio, int fractionalDigits, Locale locale);
std::string formatDate(std::chrono::system_clock::time_point value, Locale locale);
std::string formatTime(std::chrono::system_clock::time_point value, Locale locale, bool includeSeconds = false);
std::string formatDuration(std::chrono::seconds value, Locale locale, bool compact = false);
} // namespace gm::localization
