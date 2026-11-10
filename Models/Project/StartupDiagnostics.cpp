#include "StartupDiagnostics.hpp"

#include <atomic>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
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

namespace
{
std::string singleLine(std::string_view value)
{
    std::string result(value);
    for (char &character : result)
        if (character == '\n' || character == '\r')
            character = ' ';
    return result;
}

unsigned long processId()
{
#if defined(_WIN32)
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}

std::string streamDescription()
{
#if defined(_WIN32)
    const auto kind = [](DWORD id) -> const char *
    {
        const HANDLE handle = GetStdHandle(id);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
            return "absent";
        switch (GetFileType(handle))
        {
        case FILE_TYPE_CHAR: return "character";
        case FILE_TYPE_DISK: return "file";
        case FILE_TYPE_PIPE: return "pipe";
        default: return "unknown";
        }
    };
    return std::string("stdout=") + kind(STD_OUTPUT_HANDLE) + " stderr=" + kind(STD_ERROR_HANDLE);
#else
    return std::string("stdout=") + (isatty(STDOUT_FILENO) ? "terminal" : "non-terminal") +
           " stderr=" + (isatty(STDERR_FILENO) ? "terminal" : "non-terminal");
#endif
}

std::string launchPresentationDescription()
{
#if defined(_WIN32)
    STARTUPINFOW info{};
    info.cb = sizeof(info);
    GetStartupInfoW(&info);
    return "startup_use_show_window=" + std::to_string((info.dwFlags & STARTF_USESHOWWINDOW) != 0) +
           " startup_show_command=" + std::to_string(info.wShowWindow);
#else
    return "startup_show_command=platform-default";
#endif
}
} // namespace

bool StartupDiagnostics::open(const std::filesystem::path &dataRoot) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stream_.is_open())
            return true;
        const std::filesystem::path directory = dataRoot / "Logs";
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error)
            return false;
        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
#if defined(_WIN32)
        gmtime_s(&utc, &time);
#else
        gmtime_r(&time, &utc);
#endif
        static std::atomic<unsigned long> sequence{0};
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count() % 1000000;
        std::ostringstream name;
        name << "startup-" << std::put_time(&utc, "%Y%m%d-%H%M%S") << '-'
             << micros << '-' << processId() << '-' << sequence.fetch_add(1) << ".log";
        const std::filesystem::path candidate = directory / name.str();
        stream_.clear();
        stream_.open(candidate, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!stream_)
            return false;
        path_ = candidate;
        started_ = std::chrono::steady_clock::now();
        stream_ << "startup_log_version=1\nstarted_at_utc=" << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ")
                << "\npid=" << processId() << '\n' << streamDescription() << '\n'
                << launchPresentationDescription() << '\n';
        stream_.flush();
        return static_cast<bool>(stream_);
    }
    catch (...)
    {
        // Diagnostics must never prevent launching or mask the original error.
        return false;
    }
}

void StartupDiagnostics::record(std::string_view event, std::string_view detail) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stream_.is_open() || !stream_)
            return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_).count();
        stream_ << elapsed << "ms " << singleLine(event);
        if (!detail.empty())
            stream_ << ' ' << singleLine(detail);
        stream_ << '\n';
        stream_.flush(); // Keep the last reached phase readable during a stall.
    }
    catch (...)
    {
    }
}
