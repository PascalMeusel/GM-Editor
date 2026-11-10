#include "Models/Localization/LocalizationService.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace gm::localization
{
namespace
{
std::string normalizedLocaleName(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    value = first < last ? std::string(first, last) : std::string{};
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    std::replace(value.begin(), value.end(), '_', '-');
    return value;
}

std::string systemLocaleName()
{
    constexpr const char *environmentNames[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char *name : environmentNames)
    {
        if (const char *value = std::getenv(name); value != nullptr && value[0] != '\0')
            return value;
    }

#if defined(_WIN32)
    const LANGID language = GetUserDefaultUILanguage();
    return PRIMARYLANGID(language) == LANG_GERMAN ? "de" : "en";
#endif
    return "en";
}
} // namespace

std::optional<LocalePreference> parseLocalePreference(std::string_view value)
{
    const std::string normalized = normalizedLocaleName(std::string(value));
    if (normalized == "system" || normalized == "auto" || normalized == "default")
        return LocalePreference::System;
    if (normalized == "en" || normalized.rfind("en-", 0) == 0 || normalized == "english")
        return LocalePreference::English;
    if (normalized == "de" || normalized.rfind("de-", 0) == 0 || normalized == "german" || normalized == "deutsch")
        return LocalePreference::German;
    return std::nullopt;
}

Locale detectSystemLocale()
{
    const std::string normalized = normalizedLocaleName(systemLocaleName());
    return normalized == "de" || normalized.rfind("de-", 0) == 0 ? Locale::German : Locale::English;
}

Locale resolveLocale(LocalePreference preference)
{
    if (preference == LocalePreference::German)
        return Locale::German;
    if (preference == LocalePreference::English)
        return Locale::English;
    return detectSystemLocale();
}

LocalizationService &LocalizationService::instance()
{
    static LocalizationService service;
    return service;
}

bool LocalizationService::initialize(const std::filesystem::path &settingsPath, std::string *error)
{
    const LocaleSettingsLoadResult loaded = LocalizationSettingsStore::load(settingsPath);
    if (!loaded.succeeded())
    {
        if (error != nullptr)
            *error = loaded.error;
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _settingsPath = settingsPath;
    _preference = loaded.valueFound ? loaded.preference : LocalePreference::System;
    _locale = resolveLocale(_preference);
    ++_revision;
    if (error != nullptr)
        error->clear();
    return true;
}

bool LocalizationService::setPreference(LocalePreference preference, bool persist, std::string *error)
{
    std::filesystem::path path;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        path = _settingsPath.empty() ? LocalizationSettingsStore::defaultPath() : _settingsPath;
    }

    if (persist && !LocalizationSettingsStore::save(path, preference, error))
        return false;

    std::lock_guard<std::mutex> lock(_mutex);
    const Locale resolved = resolveLocale(preference);
    const bool changed = _preference != preference || _locale != resolved || _settingsPath != path;
    _preference = preference;
    _locale = resolved;
    _settingsPath = std::move(path);
    if (changed)
        ++_revision;
    if (error != nullptr)
        error->clear();
    return true;
}

void LocalizationService::refreshSystemLocale()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_preference != LocalePreference::System)
        return;
    const Locale detected = detectSystemLocale();
    if (_locale != detected)
    {
        _locale = detected;
        ++_revision;
    }
}

LocalePreference LocalizationService::preference() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _preference;
}

Locale LocalizationService::locale() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _locale;
}

std::uint64_t LocalizationService::revision() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _revision;
}

std::filesystem::path LocalizationService::settingsPath() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _settingsPath;
}

const char *LocalizationService::text(TextId id) const
{
    const std::string_view value = translation(locale(), id);
    return value.empty() ? "" : value.data();
}

const char *LocalizationService::uiText(const char *english) const
{
    if (english == nullptr)
        return "";
    const std::string_view value = uiLiteralTranslation(locale(), english);
    return value.empty() ? english : value.data();
}

std::string LocalizationService::systemMessage(std::string_view english) const
{
    return uiSystemMessageTranslation(locale(), english);
}

std::string LocalizationService::format(TextId id, std::initializer_list<std::string_view> arguments) const
{
    std::string result(translation(locale(), id));
    std::size_t index = 0;
    for (const std::string_view argument : arguments)
    {
        const std::string placeholder = "{" + std::to_string(index++) + "}";
        std::size_t offset = 0;
        while ((offset = result.find(placeholder, offset)) != std::string::npos)
        {
            result.replace(offset, placeholder.size(), argument.data(), argument.size());
            offset += argument.size();
        }
    }
    return result;
}

std::string LocalizationService::quantity(TextId singular, TextId plural, std::int64_t count) const
{
    return std::to_string(count) + " " + text(count == 1 ? singular : plural);
}
} // namespace gm::localization
