#include "Models/Project/ApplicationPaths.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
void setEnvironment(const char *name, const std::string &value)
{
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void clearEnvironment(const char *name)
{
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}
} // namespace

int main()
{
    std::error_code error;
    const std::filesystem::path root =
        (std::filesystem::current_path(error) / "application-paths-fixture").lexically_normal();
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);

    clearEnvironment("TTRPG_DB_PATH");
    clearEnvironment("TTRPG_SETTINGS_PATH");
    setEnvironment("TTRPG_DATA_ROOT", root.string());

    bool ok = true;
    ok &= require(ApplicationPaths::userDataRoot().lexically_normal() == root,
                  "TTRPG_DATA_ROOT must override the platform user-data directory");
    ok &= require(ApplicationPaths::writableRoot().lexically_normal() == root,
                  "TTRPG_DATA_ROOT must be the writable project root");
    ok &= require(ApplicationPaths::databasePath().lexically_normal() == root / "TTRPG.db",
                  "the default database must live under the writable root");
    ok &= require(ApplicationPaths::settingsPath().lexically_normal() == root / "GMEditor.settings",
                  "settings must live under the writable root");
    ok &= require(!ApplicationPaths::runtimeSearchRoots().empty() &&
                      ApplicationPaths::runtimeSearchRoots().front().lexically_normal() == root,
                  "relative campaign assets must resolve from writable storage first");

    const std::filesystem::path databaseOverride = root / "portable" / "library.db";
    setEnvironment("TTRPG_DB_PATH", databaseOverride.string());
    ok &= require(ApplicationPaths::databasePath().lexically_normal() == databaseOverride,
                  "TTRPG_DB_PATH must remain a supported portable/test override");
    ok &= require(std::filesystem::is_directory(databaseOverride.parent_path()),
                  "database parent directories must be created before SQLite opens the file");

    const std::filesystem::path settingsOverride = root / "preferences" / "custom.settings";
    setEnvironment("TTRPG_SETTINGS_PATH", settingsOverride.string());
    ok &= require(ApplicationPaths::settingsPath().lexically_normal() == settingsOverride,
                  "TTRPG_SETTINGS_PATH must remain an exact override");
    ok &= require(std::filesystem::is_directory(settingsOverride.parent_path()),
                  "settings parent directories must be created before saving");

    clearEnvironment("TTRPG_DB_PATH");
    clearEnvironment("TTRPG_SETTINGS_PATH");
    clearEnvironment("TTRPG_DATA_ROOT");
    const auto originalDirectory = std::filesystem::current_path();
    const auto developer = root / "source";
    std::filesystem::create_directories(developer / "Resources");
    std::filesystem::create_directories(developer / "Models");
    std::ofstream(developer / "CMakeLists.txt") << "# Disposable developer checkout";
#if defined(_WIN32)
    setEnvironment("LOCALAPPDATA", (root / "user-data").string());
    const auto expectedStorage = root / "user-data" / "GM Editor";
#else
    setEnvironment("XDG_DATA_HOME", (root / "user-data").string());
    const auto expectedStorage = root / "user-data" / "gm-editor";
#endif
    std::filesystem::current_path(developer);
    ok &= require(ApplicationPaths::developmentRoot() == developer,
                  "development assets must remain discoverable independently of user storage");
    ok &= require(ApplicationPaths::writableRoot() == expectedStorage &&
                      ApplicationPaths::databasePath() == expectedStorage / "TTRPG.db" &&
                      ApplicationPaths::settingsPath() == expectedStorage / "GMEditor.settings",
                  "launching from a source checkout must put writable data in user storage");
    ok &= require(!std::filesystem::exists(developer / "TTRPG.db") &&
                      !std::filesystem::exists(developer / "GMEditor.settings"),
                  "path discovery must not dirty the developer checkout");
    std::filesystem::current_path(originalDirectory);
    std::filesystem::remove_all(root, error);

    if (!ok)
        return 1;
    std::cout << "Application path tests passed.\n";
    return 0;
}
