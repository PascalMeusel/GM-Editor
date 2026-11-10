#include "Models/Project/ProjectSaveSettings.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
} // namespace

int main()
{
    std::error_code filesystemError;
    const std::filesystem::path root =
        (std::filesystem::current_path(filesystemError) / "project-save-settings-fixture").lexically_normal();
    std::filesystem::remove_all(root, filesystemError);
    std::filesystem::create_directories(root, filesystemError);
    const std::filesystem::path settingsPath = root / "GMEditor.settings";

    const ProjectSaveSettingsLoadResult defaults = ProjectSaveSettingsStore::load(settingsPath);
    bool ok = true;
    ok &= require(defaults.succeeded() && !defaults.fileFound && defaults.config.autosaveEnabled &&
                      defaults.config.autosaveIntervalSeconds == 10,
                  "new installations must default to autosave after ten seconds of inactivity");

    {
        std::ofstream output(settingsPath, std::ios::binary | std::ios::trunc);
        output << "# existing preferences\n"
               << "language=de\n"
               << "future_setting=keep-me\n"
               << "autosave_enabled=false\n"
               << "autosave_interval_seconds=45\n"
               << "backup_snapshots_enabled=false\n"
               << "recovery_snapshots_enabled=true\n"
               << "max_recovery_snapshots=7\n"
               << "max_backup_snapshots=12\n";
    }

    const ProjectSaveSettingsLoadResult loaded = ProjectSaveSettingsStore::load(settingsPath);
    ok &= require(loaded.succeeded() && loaded.fileFound && loaded.valuesFound,
                  "existing save preferences must load without ambiguity");
    ok &= require(!loaded.config.autosaveEnabled && loaded.config.autosaveIntervalSeconds == 45,
                  "autosave preferences must round-trip");
    ok &= require(!loaded.config.backupSnapshotsEnabled && loaded.config.recoverySnapshotsEnabled,
                  "snapshot switches must round-trip");
    ok &= require(loaded.config.maxRecoverySnapshots == 7 && loaded.config.maxBackupSnapshots == 12,
                  "snapshot retention limits must round-trip");

    ProjectSaveConfig updated = loaded.config;
    updated.autosaveEnabled = true;
    updated.autosaveIntervalSeconds = 90;
    updated.backupSnapshotsEnabled = true;
    updated.maxBackupSnapshots = 9;
    std::string saveError;
    ok &= require(ProjectSaveSettingsStore::save(settingsPath, updated, &saveError),
                  "valid save preferences must persist atomically");

    const std::string persisted = readFile(settingsPath);
    ok &= require(persisted.find("language=de") != std::string::npos &&
                      persisted.find("future_setting=keep-me") != std::string::npos,
                  "saving project preferences must preserve localization and future keys");
    ok &= require(persisted.find("autosave_enabled=true") != std::string::npos &&
                      persisted.find("autosave_interval_seconds=90") != std::string::npos,
                  "updated autosave preferences must be written");
    ok &= require(!std::filesystem::exists(settingsPath.string() + ".tmp") &&
                      !std::filesystem::exists(settingsPath.string() + ".bak"),
                  "successful saves must not leave staging files behind");

    {
        std::ofstream output(settingsPath, std::ios::binary | std::ios::trunc);
        output << "language=en\nautosave_enabled=perhaps\n";
    }
    const ProjectSaveSettingsLoadResult invalid = ProjectSaveSettingsStore::load(settingsPath);
    ok &= require(!invalid.succeeded(), "invalid persisted values must be reported instead of guessed");

    std::filesystem::remove_all(root, filesystemError);
    if (!ok)
        return 1;
    std::cout << "Project save settings tests passed.\n";
    return 0;
}
