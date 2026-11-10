#pragma once

#include <filesystem>
#include <system_error>

// Both filesystem metadata operations and CRT streams need an extended
// native path to reliably support deep paths on Windows. Keep this conversion
// at the I/O boundary so stored and displayed paths stay portable.
inline std::filesystem::path mapFileIoPath(const std::filesystem::path &path)
{
#if defined(_WIN32)
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error).lexically_normal();
    if (error)
        return path;
    absolute.make_preferred();
    const std::wstring native = absolute.native();
    if (native.rfind(L"\\\\?\\", 0) == 0 || native.rfind(L"\\\\.\\", 0) == 0)
        return absolute;
    if (native.rfind(L"\\\\", 0) == 0)
        return std::filesystem::path(L"\\\\?\\UNC\\" + native.substr(2));
    return std::filesystem::path(L"\\\\?\\" + native);
#else
    return path;
#endif
}
