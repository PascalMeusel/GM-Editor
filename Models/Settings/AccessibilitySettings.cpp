#include "Models/Settings/AccessibilitySettings.hpp"

#include "Models/Project/ApplicationPaths.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <system_error>
#include <vector>

namespace gm::settings
{
namespace
{
constexpr float kMinimumTextScale = 0.90f;
constexpr float kMaximumTextScale = 1.50f;

enum class SettingKey
{
    Other,
    TextScale,
    HighContrast,
    CompactTables,
    AlwaysShowFocus
};

std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    if (first >= last)
        return {};
    return std::string(first, last);
}

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

SettingKey settingKey(const std::string &line)
{
    const auto equals = line.find('=');
    if (equals == std::string::npos)
        return SettingKey::Other;

    const std::string key = lowerAscii(trim(line.substr(0, equals)));
    if (key == "text_scale")
        return SettingKey::TextScale;
    if (key == "high_contrast")
        return SettingKey::HighContrast;
    if (key == "compact_tables")
        return SettingKey::CompactTables;
    if (key == "always_show_focus")
        return SettingKey::AlwaysShowFocus;
    return SettingKey::Other;
}

std::string settingValue(const std::string &line)
{
    const auto equals = line.find('=');
    return equals == std::string::npos ? std::string{} : trim(line.substr(equals + 1));
}

bool parseTextScale(const std::string &value, float &parsed)
{
    std::istringstream stream(value);
    stream.imbue(std::locale::classic());
    if (!(stream >> parsed))
        return false;
    char trailing = '\0';
    if (stream >> trailing)
        return false;
    return std::isfinite(parsed) && parsed >= kMinimumTextScale && parsed <= kMaximumTextScale;
}

bool parseBoolean(const std::string &value, bool &parsed)
{
    const std::string normalized = lowerAscii(trim(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on")
    {
        parsed = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off")
    {
        parsed = false;
        return true;
    }
    return false;
}

void recordError(AccessibilitySettingsLoadResult &result, const std::string &message)
{
    if (result.error.empty())
        result.error = message;
}

bool setError(std::string *error, const std::string &message)
{
    if (error != nullptr)
        *error = message;
    return false;
}

std::string textScaleLine(float textScale)
{
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "text_scale=" << std::fixed << std::setprecision(2) << textScale;
    return output.str();
}
} // namespace

std::filesystem::path AccessibilitySettingsStore::defaultPath()
{
    return ApplicationPaths::settingsPath();
}

AccessibilitySettingsLoadResult AccessibilitySettingsStore::load(const std::filesystem::path &path)
{
    AccessibilitySettingsLoadResult result;
    std::error_code filesystemError;
    result.fileFound = std::filesystem::exists(path, filesystemError);
    if (filesystemError)
    {
        result.error = "Could not inspect accessibility settings: " + filesystemError.message();
        return result;
    }
    if (!result.fileFound)
        return result;
    if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError)
    {
        result.error = "Accessibility settings path is not a regular file.";
        return result;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        result.error = "Could not open accessibility settings for reading.";
        return result;
    }

    std::string line;
    while (std::getline(input, line))
    {
        switch (settingKey(line))
        {
        case SettingKey::TextScale:
        {
            float parsed = result.settings.textScale;
            if (parseTextScale(settingValue(line), parsed))
            {
                result.settings.textScale = parsed;
                result.textScaleFound = true;
            }
            else
            {
                recordError(result, "Unsupported text_scale value in accessibility settings.");
            }
            break;
        }
        case SettingKey::HighContrast:
        {
            bool parsed = result.settings.highContrast;
            if (parseBoolean(settingValue(line), parsed))
            {
                result.settings.highContrast = parsed;
                result.highContrastFound = true;
            }
            else
            {
                recordError(result, "Unsupported high_contrast value in accessibility settings.");
            }
            break;
        }
        case SettingKey::CompactTables:
        {
            bool parsed = result.settings.compactTables;
            if (parseBoolean(settingValue(line), parsed))
            {
                result.settings.compactTables = parsed;
                result.compactTablesFound = true;
            }
            else
            {
                recordError(result, "Unsupported compact_tables value in accessibility settings.");
            }
            break;
        }
        case SettingKey::AlwaysShowFocus:
        {
            bool parsed = result.settings.alwaysShowFocus;
            if (parseBoolean(settingValue(line), parsed))
            {
                result.settings.alwaysShowFocus = parsed;
                result.alwaysShowFocusFound = true;
            }
            else
            {
                recordError(result, "Unsupported always_show_focus value in accessibility settings.");
            }
            break;
        }
        case SettingKey::Other:
            break;
        }
    }

    if (!input.eof() && input.fail())
        recordError(result, "Could not finish reading accessibility settings.");
    return result;
}

bool AccessibilitySettingsStore::save(const std::filesystem::path &path,
                                      const AccessibilitySettings &settings,
                                      std::string *error)
{
    if (error != nullptr)
        error->clear();
    if (path.empty())
        return setError(error, "Accessibility settings path is empty.");
    if (!std::isfinite(settings.textScale) || settings.textScale < kMinimumTextScale ||
        settings.textScale > kMaximumTextScale)
        return setError(error, "Accessibility text scale is outside the supported range.");

    std::vector<std::string> lines;
    std::error_code filesystemError;
    const bool existed = std::filesystem::exists(path, filesystemError);
    if (filesystemError)
        return setError(error, "Could not inspect accessibility settings: " + filesystemError.message());

    if (existed)
    {
        if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError)
            return setError(error, "Accessibility settings path is not a regular file.");
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return setError(error, "Could not open accessibility settings for reading.");
        std::string line;
        while (std::getline(input, line))
            lines.push_back(line);
        if (!input.eof() && input.fail())
            return setError(error, "Could not finish reading accessibility settings.");
    }

    const std::string replacementLines[] = {
        {},
        textScaleLine(settings.textScale),
        std::string("high_contrast=") + (settings.highContrast ? "true" : "false"),
        std::string("compact_tables=") + (settings.compactTables ? "true" : "false"),
        std::string("always_show_focus=") + (settings.alwaysShowFocus ? "true" : "false")};
    bool replaced[] = {false, false, false, false, false};
    std::vector<std::string> updated;
    updated.reserve(lines.size() + 5);
    for (const std::string &line : lines)
    {
        const SettingKey key = settingKey(line);
        if (key == SettingKey::Other)
        {
            updated.push_back(line);
            continue;
        }

        const auto index = static_cast<std::size_t>(key);
        if (!replaced[index])
        {
            updated.push_back(replacementLines[index]);
            replaced[index] = true;
        }
    }

    if (updated.empty())
        updated.push_back("# GM Editor application settings");
    for (std::size_t index = 1; index < 5; ++index)
    {
        if (!replaced[index])
            updated.push_back(replacementLines[index]);
    }

    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError)
            return setError(error, "Could not create accessibility settings directory: " + filesystemError.message());
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::filesystem::path backup = path;
    backup += ".bak";

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            return setError(error, "Could not open temporary accessibility settings for writing.");
        for (const std::string &line : updated)
            output << line << '\n';
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary, filesystemError);
            return setError(error, "Could not write accessibility settings.");
        }
    }

    std::filesystem::remove(backup, filesystemError);
    filesystemError.clear();
    if (existed)
    {
        std::filesystem::rename(path, backup, filesystemError);
        if (filesystemError)
        {
            const std::string message = "Could not prepare accessibility settings update: " + filesystemError.message();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return setError(error, message);
        }
    }

    std::filesystem::rename(temporary, path, filesystemError);
    if (filesystemError)
    {
        const std::string message = "Could not replace accessibility settings: " + filesystemError.message();
        std::error_code ignored;
        if (existed)
            std::filesystem::rename(backup, path, ignored);
        std::filesystem::remove(temporary, ignored);
        return setError(error, message);
    }

    std::filesystem::remove(backup, filesystemError);
    return true;
}
} // namespace gm::settings
