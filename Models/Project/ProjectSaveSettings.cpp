#include "ProjectSaveSettings.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <optional>
#include <system_error>
#include <vector>

namespace ProjectSaveSettingsStore
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
    return first < last ? std::string(first, last) : std::string{};
}

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::optional<bool> parseBoolean(const std::string &value)
{
    const std::string normalized = lowerAscii(trim(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on")
        return true;
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off")
        return false;
    return std::nullopt;
}

bool parseInteger(const std::string &value, int &parsed)
{
    try
    {
        std::size_t consumed = 0;
        const int result = std::stoi(trim(value), &consumed);
        if (consumed != trim(value).size())
            return false;
        parsed = result;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::pair<std::string, std::string> splitSetting(const std::string &line)
{
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos)
        return {};
    return {lowerAscii(trim(line.substr(0, equals))), trim(line.substr(equals + 1))};
}

bool isOwnedKey(const std::string &key)
{
    return key == "autosave_enabled" || key == "autosave_interval_seconds" ||
           key == "backup_snapshots_enabled" || key == "recovery_snapshots_enabled" ||
           key == "max_recovery_snapshots" || key == "max_backup_snapshots";
}

bool fail(std::string *error, const std::string &message)
{
    if (error)
        *error = message;
    return false;
}
} // namespace

ProjectSaveSettingsLoadResult load(const std::filesystem::path &path,
                                   const ProjectSaveConfig &defaults)
{
    ProjectSaveSettingsLoadResult result;
    result.config = defaults;

    std::error_code filesystemError;
    result.fileFound = std::filesystem::exists(path, filesystemError);
    if (filesystemError)
    {
        result.error = "Could not inspect application settings: " + filesystemError.message();
        return result;
    }
    if (!result.fileFound)
        return result;
    if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError)
    {
        result.error = "Application settings path is not a regular file.";
        return result;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        result.error = "Could not open application settings for reading.";
        return result;
    }

    std::string line;
    while (std::getline(input, line))
    {
        const auto [key, value] = splitSetting(line);
        if (!isOwnedKey(key))
            continue;
        result.valuesFound = true;

        if (key == "autosave_enabled" || key == "backup_snapshots_enabled" || key == "recovery_snapshots_enabled")
        {
            const std::optional<bool> parsed = parseBoolean(value);
            if (!parsed.has_value())
            {
                result.error = "Unsupported boolean value for " + key + ".";
                return result;
            }
            if (key == "autosave_enabled") result.config.autosaveEnabled = *parsed;
            if (key == "backup_snapshots_enabled") result.config.backupSnapshotsEnabled = *parsed;
            if (key == "recovery_snapshots_enabled") result.config.recoverySnapshotsEnabled = *parsed;
            continue;
        }

        int parsed = 0;
        if (!parseInteger(value, parsed))
        {
            result.error = "Unsupported integer value for " + key + ".";
            return result;
        }
        if (key == "autosave_interval_seconds") result.config.autosaveIntervalSeconds = std::max(10, parsed);
        if (key == "max_recovery_snapshots") result.config.maxRecoverySnapshots = std::clamp(parsed, 1, 50);
        if (key == "max_backup_snapshots") result.config.maxBackupSnapshots = std::clamp(parsed, 1, 50);
    }

    if (!input.eof() && input.fail())
        result.error = "Could not finish reading application settings.";
    return result;
}

bool save(const std::filesystem::path &path,
          const ProjectSaveConfig &config,
          std::string *error)
{
    if (error)
        error->clear();
    if (path.empty())
        return fail(error, "Application settings path is empty.");

    std::vector<std::string> retainedLines;
    std::error_code filesystemError;
    const bool existed = std::filesystem::exists(path, filesystemError);
    if (filesystemError)
        return fail(error, "Could not inspect application settings: " + filesystemError.message());
    if (existed)
    {
        if (!std::filesystem::is_regular_file(path, filesystemError) || filesystemError)
            return fail(error, "Application settings path is not a regular file.");
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return fail(error, "Could not open application settings for reading.");
        std::string line;
        while (std::getline(input, line))
        {
            const auto [key, value] = splitSetting(line);
            if (!isOwnedKey(key))
                retainedLines.push_back(line);
        }
        if (!input.eof() && input.fail())
            return fail(error, "Could not finish reading application settings.");
    }
    if (retainedLines.empty())
        retainedLines.push_back("# GM Editor application settings");

    retainedLines.push_back(std::string("autosave_enabled=") + (config.autosaveEnabled ? "true" : "false"));
    retainedLines.push_back("autosave_interval_seconds=" + std::to_string(std::max(10, config.autosaveIntervalSeconds)));
    retainedLines.push_back(std::string("backup_snapshots_enabled=") + (config.backupSnapshotsEnabled ? "true" : "false"));
    retainedLines.push_back(std::string("recovery_snapshots_enabled=") + (config.recoverySnapshotsEnabled ? "true" : "false"));
    retainedLines.push_back("max_recovery_snapshots=" + std::to_string(std::clamp(config.maxRecoverySnapshots, 1, 50)));
    retainedLines.push_back("max_backup_snapshots=" + std::to_string(std::clamp(config.maxBackupSnapshots, 1, 50)));

    if (path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path(), filesystemError);
        if (filesystemError)
            return fail(error, "Could not create application settings directory: " + filesystemError.message());
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::filesystem::path backup = path;
    backup += ".bak";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            return fail(error, "Could not open temporary application settings for writing.");
        for (const std::string &line : retainedLines)
            output << line << '\n';
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary, filesystemError);
            return fail(error, "Could not write application settings.");
        }
    }

    std::filesystem::remove(backup, filesystemError);
    filesystemError.clear();
    if (existed)
    {
        std::filesystem::rename(path, backup, filesystemError);
        if (filesystemError)
        {
            std::filesystem::remove(temporary, filesystemError);
            return fail(error, "Could not prepare application settings update.");
        }
    }
    std::filesystem::rename(temporary, path, filesystemError);
    if (filesystemError)
    {
        std::error_code ignored;
        if (existed)
            std::filesystem::rename(backup, path, ignored);
        std::filesystem::remove(temporary, ignored);
        return fail(error, "Could not replace application settings: " + filesystemError.message());
    }
    std::filesystem::remove(backup, filesystemError);
    return true;
}
} // namespace ProjectSaveSettingsStore
