#pragma once

#include <optional>
#include <string_view>

namespace gm::localization
{
enum class Locale
{
    English,
    German
};

enum class LocalePreference
{
    System,
    English,
    German
};

constexpr std::string_view localeCode(Locale locale)
{
    return locale == Locale::German ? "de" : "en";
}

constexpr std::string_view preferenceCode(LocalePreference preference)
{
    switch (preference)
    {
    case LocalePreference::German:
        return "de";
    case LocalePreference::English:
        return "en";
    case LocalePreference::System:
    default:
        return "system";
    }
}

std::optional<LocalePreference> parseLocalePreference(std::string_view value);
Locale detectSystemLocale();
Locale resolveLocale(LocalePreference preference);
} // namespace gm::localization
