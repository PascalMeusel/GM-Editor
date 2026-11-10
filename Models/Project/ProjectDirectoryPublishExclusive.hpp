#pragma once
#include "Models/Map/MapFileIoPath.hpp"
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace ProjectDirectoryPublish
{
// Publish an owned, complete directory in one no-replace operation. Callers
// validate its inventory, context, destination parents and storage boundaries.
// No retry or delay here: a sharing failure leaves staging available for an
// explicit retry. Plain POSIX rename would replace an existing empty directory.
inline void renameExclusive(const std::filesystem::path &source,
                            const std::filesystem::path &destination)
{
    constexpr auto operation = "Publish directory without replacing existing files";
    if (source.empty() || destination.empty())
        throw std::filesystem::filesystem_error(operation, source, destination,
            std::make_error_code(std::errc::invalid_argument));
    const auto nativeSource = mapFileIoPath(source);
    const auto nativeDestination = mapFileIoPath(destination);
    std::error_code error;
    const auto status = std::filesystem::symlink_status(nativeSource, error);
    if (error || !std::filesystem::is_directory(status))
        throw std::filesystem::filesystem_error(operation, source, destination,
            error ? error : std::make_error_code(std::errc::not_a_directory));
#if defined(_WIN32)
    // MoveFileW requires an absent destination and a same-volume directory.
    // https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-movefilew
    if (MoveFileW(nativeSource.c_str(), nativeDestination.c_str())) return;
    const auto nativeError = GetLastError();
    error = std::error_code(static_cast<int>(nativeError), std::system_category());
#elif defined(__linux__) && defined(SYS_renameat2)
    // The kernel enforces absence atomically, including an empty destination.
    // https://man7.org/linux/man-pages/man2/rename.2.html
    if (::syscall(SYS_renameat2, AT_FDCWD, nativeSource.c_str(), AT_FDCWD,
                  nativeDestination.c_str(), RENAME_NOREPLACE) == 0) return;
    const int nativeError = errno;
    error = std::error_code(nativeError, std::generic_category());
#else
    // Never substitute a check-then-rename sequence that could replace data.
    error = std::make_error_code(std::errc::operation_not_supported);
#endif
    throw std::filesystem::filesystem_error(operation, source, destination, error);
}
}
