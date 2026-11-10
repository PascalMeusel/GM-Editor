#include "ProjectAssetService.hpp"

#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/ProjectPackageLayout.hpp"
#include "Models/Project/ProjectAssetRepairService.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <set>
#include <vector>

namespace
{
std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool looksLikeProjectRoot(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path / "Resources"))
        return false;
    return std::filesystem::exists(path / "CMakeLists.txt") ||
           std::filesystem::exists(path / "TTRPG.db") ||
           std::filesystem::exists(path / "Resources" / "Campaigns");
}

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

std::filesystem::path findRootFrom(std::filesystem::path current)
{
    while (!current.empty())
    {
        if (looksLikeProjectRoot(current))
            return current;
        if (current == current.parent_path())
            break;
        current = current.parent_path();
    }
    return {};
}
}

namespace ProjectAssets
{
std::string sanitizePathSegment(const std::string &value)
{
    std::string sanitized;
    for (const char c : value)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            sanitized.push_back(c);
        else if (c == ' ')
            sanitized.push_back('_');
    }
    return sanitized.empty() ? "campaign" : sanitized;
}

std::filesystem::path findProjectRoot()
{
    return ApplicationPaths::writableRoot();
}

std::filesystem::path resolveProjectPath(const std::string &storedPath)
{
    const std::filesystem::path path(storedPath);
    if (path.is_absolute())
        return path;

    const std::filesystem::path projectRoot = findProjectRoot();
    const std::vector<std::filesystem::path> roots = ApplicationPaths::runtimeSearchRoots();

    for (const std::filesystem::path &root : roots)
    {
        const std::filesystem::path candidate = root / path;
        if (std::filesystem::exists(candidate))
            return candidate;
    }

    return projectRoot / path;
}

std::filesystem::path campaignAssetDirectory(const Campaign &campaign, const std::string &assetFolder)
{
    return ProjectPackageLayout::assetDirectory(campaign, assetFolder);
}

std::string copyFileToCampaignAssetFolder(const Campaign &campaign, const std::filesystem::path &sourcePath, const EntityId &assetId, const std::string &assetFolder, const std::string &fallbackExtension)
{
    const std::filesystem::path targetDirectory = campaignAssetDirectory(campaign, assetFolder);
    std::filesystem::create_directories(targetDirectory);

    std::filesystem::path extension = sourcePath.extension();
    if (extension.empty())
        extension = fallbackExtension;

    const std::filesystem::path destination = targetDirectory / (sanitizePathSegment(assetId) + extension.string());
    std::filesystem::copy_file(sourcePath, destination, std::filesystem::copy_options::overwrite_existing);

    const std::filesystem::path root = findProjectRoot();
    try
    {
        return std::filesystem::relative(destination, root).generic_string();
    }
    catch (const std::exception &)
    {
        return destination.generic_string();
    }
}

bool isSupportedAudioFile(const std::filesystem::path &sourcePath)
{
    static const std::set<std::string> supportedExtensions{
        ".wav", ".mp3", ".ogg", ".flac", ".aiff", ".aif", ".wma", ".mid", ".midi"};
    return supportedExtensions.count(lowerCopy(sourcePath.extension().string())) > 0;
}

std::filesystem::path campaignAudioDirectory(const Campaign &campaign)
{
    return campaignAssetDirectory(campaign, "Audio");
}

std::string copyAudioFileToCampaign(const Campaign &campaign, const std::filesystem::path &sourcePath, const EntityId &assetId)
{
    return copyFileToCampaignAssetFolder(campaign, sourcePath, assetId, "Audio", ".audio");
}

std::filesystem::path resolveAudioPath(const AudioAssetModel &asset)
{
    return resolveProjectPath(asset.filePath);
}

std::filesystem::path resolveOriginalSourcePath(const AudioAssetModel &asset)
{
    std::filesystem::path sourcePath(asset.originalSourcePath);
    if (sourcePath.empty() || sourcePath.is_absolute())
        return sourcePath;
    return findProjectRoot() / sourcePath;
}

bool repairAudioAssetFromSource(Campaign &campaign, AudioAssetModel &asset, const std::filesystem::path &sourcePath, std::string &errorMessage)
{
    ProjectAssetRepair::AssetRepairRequest request;
    request.kind = ProjectAssetImport::AssetKind::Audio;
    request.assetId = asset.metadata.id;
    request.replacementPath = sourcePath;
    request.displayName = asset.name.empty() ? sourcePath.stem().string() : asset.name;
    request.sourceReference = sourcePath.string();

    const ProjectAssetRepair::AssetRepairResult result = ProjectAssetRepair::repairAsset(campaign, request);
    if (result.success)
    {
        asset.originalSourcePath = sourcePath.string();
        asset.originalFileName = sourcePath.filename().string();
        if (asset.name.empty())
            asset.name = sourcePath.stem().string();
        asset.filePath = result.importedAsset.projectPath;
        asset.metadata.touch();
        errorMessage.clear();
        return true;
    }

    errorMessage = result.errorMessage;
    return false;
}
}
