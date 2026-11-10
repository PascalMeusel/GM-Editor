#pragma once

#include <filesystem>
#include <string>
#include <string_view>

inline std::filesystem::path mapCreatorFilesystemPathFromUtf8(
    std::string_view utf8Path)
{
    return std::filesystem::u8path(utf8Path.begin(), utf8Path.end());
}

inline std::string mapCreatorFilesystemPathToUtf8(
    const std::filesystem::path &path)
{
    return path.u8string();
}

inline bool mapCreatorManualPathIsReady(std::string_view utf8Path)
{
    return !utf8Path.empty() && !mapCreatorFilesystemPathFromUtf8(utf8Path).empty();
}

inline std::filesystem::path mapCreatorPathWithDefaultExtension(
    std::filesystem::path path,
    std::string_view extension)
{
    if (path.extension().empty() && !extension.empty())
        path.replace_extension(mapCreatorFilesystemPathFromUtf8(extension));
    return path;
}

inline bool mapCreatorManualExportMayWrite(bool pathReady,
                                           bool destinationExists,
                                           bool overwriteConfirmed)
{
    return pathReady && (!destinationExists || overwriteConfirmed);
}
