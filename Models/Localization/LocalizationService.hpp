#pragma once

#include "Models/Localization/LocalizationSettings.hpp"
#include "Models/Localization/TranslationCatalog.hpp"

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>

namespace gm::localization
{
class LocalizationService
{
public:
    static LocalizationService &instance();

    bool initialize(const std::filesystem::path &settingsPath = LocalizationSettingsStore::defaultPath(),
                    std::string *error = nullptr);
    bool setPreference(LocalePreference preference, bool persist = true, std::string *error = nullptr);
    void refreshSystemLocale();

    LocalePreference preference() const;
    Locale locale() const;
    std::uint64_t revision() const;
    std::filesystem::path settingsPath() const;

    const char *text(TextId id) const;
    const char *uiText(const char *english) const;
    std::string systemMessage(std::string_view english) const;
    std::string format(TextId id, std::initializer_list<std::string_view> arguments) const;
    std::string quantity(TextId singular, TextId plural, std::int64_t count) const;

private:
    LocalizationService() = default;

    mutable std::mutex _mutex;
    LocalePreference _preference = LocalePreference::System;
    Locale _locale = Locale::English;
    std::uint64_t _revision = 1;
    std::filesystem::path _settingsPath;
};

inline const char *tr(TextId id)
{
    return LocalizationService::instance().text(id);
}

inline const char *trUi(const char *english)
{
    return LocalizationService::instance().uiText(english);
}

inline std::string trSystemMessage(std::string_view english)
{
    return LocalizationService::instance().systemMessage(english);
}
} // namespace gm::localization
