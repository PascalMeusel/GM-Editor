#pragma once

#include "Models/Localization/Locale.hpp"

#include <filesystem>
#include <string>

namespace gm::localization
{
struct LocaleSettingsLoadResult
{
    LocalePreference preference = LocalePreference::System;
    bool fileFound = false;
    bool valueFound = false;
    std::string error;

    bool succeeded() const { return error.empty(); }
};

class LocalizationSettingsStore
{
public:
    static std::filesystem::path defaultPath();
    static LocaleSettingsLoadResult load(const std::filesystem::path &path);
    static bool save(const std::filesystem::path &path, LocalePreference preference, std::string *error = nullptr);
};
} // namespace gm::localization
