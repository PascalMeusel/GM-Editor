#include "ApplicationPaths.hpp"

#include <algorithm>
#include <cstdlib>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ApplicationPaths
{
namespace
{
std::filesystem::path environmentPath(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? std::filesystem::path(value) : std::filesystem::path();
}

std::filesystem::path absoluteOrOriginal(const std::filesystem::path &path)
{
    if (path.empty() || path.is_absolute())
        return path;
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return error ? path : absolute;
}

std::filesystem::path findDevelopmentRootFrom(std::filesystem::path current)
{
    std::error_code error;
    current = absoluteOrOriginal(current);
    while (!current.empty())
    {
        const bool hasBuildFile = std::filesystem::is_regular_file(current / "CMakeLists.txt", error);
        error.clear();
        const bool hasModels = std::filesystem::is_directory(current / "Models", error);
        error.clear();
        const bool hasResources = std::filesystem::is_directory(current / "Resources", error);
        error.clear();
        if (hasBuildFile && hasModels && hasResources)
            return current;

        const std::filesystem::path parent = current.parent_path();
        if (parent.empty() || parent == current)
            break;
        current = parent;
    }
    return {};
}

void appendUnique(std::vector<std::filesystem::path> &paths, const std::filesystem::path &candidate)
{
    if (candidate.empty())
        return;
    const std::filesystem::path normalized = absoluteOrOriginal(candidate).lexically_normal();
    if (std::find(paths.begin(), paths.end(), normalized) == paths.end())
        paths.push_back(normalized);
}

void ensureDirectory(const std::filesystem::path &directory)
{
    if (directory.empty())
        return;
    std::error_code error;
    std::filesystem::create_directories(directory, error);
}

void migrateClosedLegacyDatabase(const std::filesystem::path &target)
{
    std::error_code error;
    if (target.empty() || std::filesystem::exists(target, error))
        return;

    std::vector<std::filesystem::path> candidates;
    appendUnique(candidates, executableDirectory() / "TTRPG.db");
    const std::filesystem::path current = std::filesystem::current_path(error);
    error.clear();
    appendUnique(candidates, current / "TTRPG.db");
    appendUnique(candidates, developmentRoot() / "TTRPG.db");

    const std::filesystem::path normalizedTarget = absoluteOrOriginal(target).lexically_normal();
    for (const std::filesystem::path &candidate : candidates)
    {
        if (candidate == normalizedTarget || !std::filesystem::is_regular_file(candidate, error))
        {
            error.clear();
            continue;
        }
        const std::uintmax_t size = std::filesystem::file_size(candidate, error);
        if (error || size == 0)
        {
            error.clear();
            continue;
        }

        ensureDirectory(normalizedTarget.parent_path());
        std::filesystem::path staging = normalizedTarget;
        staging += ".migrating";
        std::filesystem::remove(staging, error);
        error.clear();
        std::filesystem::copy_file(candidate, staging, std::filesystem::copy_options::none, error);
        if (error)
        {
            error.clear();
            continue;
        }
        std::filesystem::rename(staging, normalizedTarget, error);
        if (!error)
            return;
        std::filesystem::remove(staging, error);
        error.clear();
    }
}

void copyLegacyFileIfMissing(const std::filesystem::path &source, const std::filesystem::path &target)
{
    std::error_code error;
    if (source.empty() || target.empty() ||
        std::filesystem::exists(target, error) ||
        !std::filesystem::is_regular_file(source, error))
        return;
    ensureDirectory(target.parent_path());
    std::filesystem::copy_file(source, target, std::filesystem::copy_options::none, error);
}

void copyLegacyDirectoryWithoutOverwrite(const std::filesystem::path &source,
                                         const std::filesystem::path &target)
{
    std::error_code error;
    if (source.empty() || target.empty() || source.lexically_normal() == target.lexically_normal() ||
        !std::filesystem::is_directory(source, error))
        return;
    ensureDirectory(target);
    std::filesystem::copy(source,
                          target,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::skip_existing,
                          error);
}

void migrateLegacyUserContent(const std::filesystem::path &targetRoot)
{
    const std::filesystem::path executable = executableDirectory();
    if (executable.empty() || executable.lexically_normal() == targetRoot.lexically_normal())
        return;

    copyLegacyDirectoryWithoutOverwrite(executable / "Resources" / "Campaigns",
                                        targetRoot / "Resources" / "Campaigns");
    copyLegacyDirectoryWithoutOverwrite(executable / "Resources" / "Backups",
                                        targetRoot / "Resources" / "Backups");
    copyLegacyDirectoryWithoutOverwrite(executable / "Resources" / "Recovery",
                                        targetRoot / "Resources" / "Recovery");
    copyLegacyDirectoryWithoutOverwrite(executable / "dist" / "CampaignPackages",
                                        targetRoot / "dist" / "CampaignPackages");
}
} // namespace

std::filesystem::path executableDirectory()
{
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (size == buffer.size())
    {
        buffer.resize(buffer.size() * 2);
        size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    if (size > 0)
    {
        buffer.resize(size);
        return std::filesystem::path(buffer).parent_path();
    }
#else
    std::vector<char> buffer(4096);
    ssize_t size = readlink("/proc/self/exe", buffer.data(), buffer.size());
    while (size == static_cast<ssize_t>(buffer.size()))
    {
        buffer.resize(buffer.size() * 2);
        size = readlink("/proc/self/exe", buffer.data(), buffer.size());
    }
    if (size > 0)
        return std::filesystem::path(std::string(buffer.data(), static_cast<std::size_t>(size))).parent_path();
#endif
    return {};
}

std::filesystem::path developmentRoot()
{
    std::error_code markerError;
    const std::filesystem::path executable = executableDirectory();
    if (!executable.empty() &&
        std::filesystem::is_regular_file(executable / "gm_editor.install.json", markerError))
        return {};

    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    std::filesystem::path root = error ? std::filesystem::path() : findDevelopmentRootFrom(current);
    if (!root.empty())
        return root;
    return findDevelopmentRootFrom(executable);
}

std::filesystem::path userDataRoot()
{
    if (const std::filesystem::path configured = environmentPath("TTRPG_DATA_ROOT"); !configured.empty())
        return absoluteOrOriginal(configured);

#if defined(_WIN32)
    std::filesystem::path base = environmentPath("LOCALAPPDATA");
    if (base.empty())
        base = environmentPath("APPDATA");
    if (base.empty())
    {
        const std::filesystem::path profile = environmentPath("USERPROFILE");
        if (!profile.empty())
            base = profile / "AppData" / "Local";
    }
    if (!base.empty())
        return absoluteOrOriginal(base) / "GM Editor";
#else
    std::filesystem::path base = environmentPath("XDG_DATA_HOME");
    if (base.empty())
    {
        const std::filesystem::path home = environmentPath("HOME");
        if (!home.empty())
            base = home / ".local" / "share";
    }
    if (!base.empty())
        return absoluteOrOriginal(base) / "gm-editor";
#endif

    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return (error ? std::filesystem::path(".") : current) / "GMEditorData";
}

std::filesystem::path writableRoot()
{
    if (const std::filesystem::path configured = environmentPath("TTRPG_DATA_ROOT"); !configured.empty())
        return absoluteOrOriginal(configured);

    if (const std::filesystem::path database = environmentPath("TTRPG_DB_PATH"); !database.empty())
    {
        const std::filesystem::path absoluteDatabase = absoluteOrOriginal(database);
        if (absoluteDatabase.has_parent_path())
            return absoluteDatabase.parent_path();
    }

    // Source discovery is for read-only assets, never writable campaign data.
    return userDataRoot();
}

std::vector<std::filesystem::path> runtimeSearchRoots()
{
    std::vector<std::filesystem::path> roots;
    appendUnique(roots, writableRoot());
    appendUnique(roots, executableDirectory());
    appendUnique(roots, developmentRoot());
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    if (!error)
    {
        appendUnique(roots, current);
        appendUnique(roots, current.parent_path());
    }
    return roots;
}

std::filesystem::path databasePath()
{
    if (const std::filesystem::path configured = environmentPath("TTRPG_DB_PATH"); !configured.empty())
    {
        const std::filesystem::path path = absoluteOrOriginal(configured);
        ensureDirectory(path.parent_path());
        return path;
    }

    const std::filesystem::path path = writableRoot() / "TTRPG.db";
    ensureDirectory(path.parent_path());
    if (developmentRoot().empty())
    {
        migrateClosedLegacyDatabase(path);
        migrateLegacyUserContent(writableRoot());
    }
    return path;
}

std::filesystem::path settingsPath()
{
    if (const std::filesystem::path configured = environmentPath("TTRPG_SETTINGS_PATH"); !configured.empty())
    {
        const std::filesystem::path path = absoluteOrOriginal(configured);
        ensureDirectory(path.parent_path());
        return path;
    }
    const std::filesystem::path path = writableRoot() / "GMEditor.settings";
    ensureDirectory(path.parent_path());
    if (developmentRoot().empty())
        copyLegacyFileIfMissing(executableDirectory() / "GMEditor.settings", path);
    return path;
}

std::string storageDescription()
{
    return writableRoot().lexically_normal().u8string();
}
} // namespace ApplicationPaths
