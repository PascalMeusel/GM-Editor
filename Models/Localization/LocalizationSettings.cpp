#include "Models/Localization/LocalizationSettings.hpp"
#include "Models/Project/ApplicationPaths.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace gm::localization
{
namespace
{
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

bool isLanguageLine(const std::string &line)
{
    const auto equals = line.find('=');
    if (equals == std::string::npos)
        return false;
    return lowerAscii(trim(line.substr(0, equals))) == "language";
}

const char *environmentValue(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : nullptr;
}

bool setError(std::string *error, const std::string &message)
{
    if (error != nullptr)
        *error = message;
    return false;
}
} // namespace

std::filesystem::path LocalizationSettingsStore::defaultPath()
{
    return ApplicationPaths::settingsPath();
}

LocaleSettingsLoadResult LocalizationSettingsStore::load(const std::filesystem::path &path)
{
    LocaleSettingsLoadResult result;
    std::error_code fileError;
    result.fileFound = std::filesystem::exists(path, fileError);
    if (fileError)
    {
        result.error = "Could not inspect localization settings: " + fileError.message();
        return result;
    }
    if (!result.fileFound)
        return result;
    if (!std::filesystem::is_regular_file(path, fileError) || fileError)
    {
        result.error = "Localization settings path is not a regular file.";
        return result;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        result.error = "Could not open localization settings for reading.";
        return result;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (!isLanguageLine(line))
            continue;

        const auto equals = line.find('=');
        const auto parsed = parseLocalePreference(trim(line.substr(equals + 1)));
        if (!parsed.has_value())
        {
            result.error = "Unsupported language value in localization settings.";
            return result;
        }
        result.preference = *parsed;
        result.valueFound = true;
    }

    if (!input.eof() && input.fail())
        result.error = "Could not finish reading localization settings.";
    return result;
}

bool LocalizationSettingsStore::save(const std::filesystem::path &path,
                                     LocalePreference preference,
                                     std::string *error)
{
    if (error != nullptr)
        error->clear();
    if (path.empty())
        return setError(error, "Localization settings path is empty.");

    std::vector<std::string> lines;
    std::error_code filesystemError;
    const bool existed = std::filesystem::exists(path, filesystemError);
    if (filesystemError)
        return setError(error, "Could not inspect localization settings: " + filesystemError.message());

    if (existed)
    {
        if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError)
            return setError(error, "Localization settings path is not a regular file.");
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return setError(error, "Could not open localization settings for reading.");
        std::string line;
        while (std::getline(input, line))
            lines.push_back(line);
        if (!input.eof() && input.fail())
            return setError(error, "Could not finish reading localization settings.");
    }

    const std::string languageLine = "language=" + std::string(preferenceCode(preference));
    bool replaced = false;
    for (std::string &line : lines)
    {
        if (isLanguageLine(line))
        {
            if (!replaced)
                line = languageLine;
            else
                line.clear();
            replaced = true;
        }
    }
    if (!replaced)
    {
        if (lines.empty())
            lines.push_back("# GM Editor application settings");
        lines.push_back(languageLine);
    }

    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError)
            return setError(error, "Could not create localization settings directory: " + filesystemError.message());
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::filesystem::path backup = path;
    backup += ".bak";

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            return setError(error, "Could not open temporary localization settings for writing.");
        for (const std::string &line : lines)
        {
            if (!line.empty())
                output << line << '\n';
        }
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary, filesystemError);
            return setError(error, "Could not write localization settings.");
        }
    }

    std::filesystem::remove(backup, filesystemError);
    filesystemError.clear();
    if (existed)
    {
        std::filesystem::rename(path, backup, filesystemError);
        if (filesystemError)
        {
            const std::string message = "Could not prepare localization settings update: " + filesystemError.message();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return setError(error, message);
        }
    }

    std::filesystem::rename(temporary, path, filesystemError);
    if (filesystemError)
    {
        const std::string message = "Could not replace localization settings: " + filesystemError.message();
        std::error_code ignored;
        if (existed)
            std::filesystem::rename(backup, path, ignored);
        std::filesystem::remove(temporary, ignored);
        return setError(error, message);
    }

    std::filesystem::remove(backup, filesystemError);
    return true;
}
} // namespace gm::localization
