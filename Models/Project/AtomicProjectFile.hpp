#pragma once

#include "Models/Map/MapFileIoPath.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace AtomicProjectFile
{
inline std::string uniqueSuffix()
{
    static std::atomic<std::uint64_t> sequence{0};
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
#if defined(_WIN32)
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
    return std::to_string(stamp) + "_" + std::to_string(process) + "_" + std::to_string(sequence.fetch_add(1));
}

// Write a sibling file exclusively, flush it, then replace the destination in
// one filesystem operation. An unsuccessful replacement preserves old bytes.
inline void write(const std::filesystem::path &destination, std::string_view contents)
{
    auto temporary = destination;
    temporary += ".tmp_" + uniqueSuffix();
    const auto nativeTemporary = mapFileIoPath(temporary);
    const auto nativeDestination = mapFileIoPath(destination);
#if defined(_WIN32)
    HANDLE file = CreateFileW(nativeTemporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw std::system_error(GetLastError(), std::system_category(), "Create metadata temporary file");
    try
    {
        while (!contents.empty())
        {
            const DWORD requested = static_cast<DWORD>((std::min)(contents.size(), std::size_t{1U << 20}));
            DWORD written = 0;
            if (!WriteFile(file, contents.data(), requested, &written, nullptr))
                throw std::system_error(GetLastError(), std::system_category(), "Write metadata");
            if (written == 0) throw std::runtime_error("Metadata write made no progress.");
            contents.remove_prefix(written);
        }
        if (!FlushFileBuffers(file))
            throw std::system_error(GetLastError(), std::system_category(), "Flush metadata");
        const BOOL closed = CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
        if (!closed) throw std::system_error(GetLastError(), std::system_category(), "Close metadata");
        if (!MoveFileExW(nativeTemporary.c_str(), nativeDestination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(GetLastError(), std::system_category(), "Replace metadata");
    }
    catch (...)
    {
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        DeleteFileW(nativeTemporary.c_str());
        throw;
    }
#else
    int file = ::open(nativeTemporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) throw std::system_error(errno, std::generic_category(), "Create metadata temporary file");
    try
    {
        while (!contents.empty())
        {
            const auto written = ::write(file, contents.data(), contents.size());
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::system_error(written < 0 ? errno : EIO, std::generic_category(), "Write metadata");
            contents.remove_prefix(static_cast<std::size_t>(written));
        }
        if (::fsync(file) != 0) throw std::system_error(errno, std::generic_category(), "Flush metadata");
        const int closed = ::close(file);
        file = -1;
        if (closed != 0) throw std::system_error(errno, std::generic_category(), "Close metadata");
        std::filesystem::rename(nativeTemporary, nativeDestination);
    }
    catch (...)
    {
        if (file >= 0) ::close(file);
        std::error_code ignored;
        std::filesystem::remove(nativeTemporary, ignored);
        throw;
    }
#endif
}
}
