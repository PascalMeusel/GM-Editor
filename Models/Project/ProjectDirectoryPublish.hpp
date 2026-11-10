#pragma once
#include <chrono>
#include <filesystem>
#include <thread>
#include <system_error>

namespace ProjectDirectoryPublish {
// Windows scanners can briefly retain a directory handle after its last writer
// closes. Retry only access/sharing failures, with a bounded 620 ms total delay.
// Missing paths, existing destinations and other errors retain normal failure.
inline void rename(const std::filesystem::path &source, const std::filesystem::path &destination)
{
    for (int attempt = 0; ; ++attempt) {
        std::error_code error;
        std::filesystem::rename(source, destination, error);
        if (!error) return;
#if defined(_WIN32)
        const bool temporaryAccess = error == std::errc::permission_denied ||
            (error.category() == std::system_category() && (error.value() == 32 || error.value() == 33));
        if (temporaryAccess && attempt < 5 && !std::filesystem::exists(destination)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20 << attempt));
            continue;
        }
#endif
        throw std::filesystem::filesystem_error("Publish project directory", source, destination, error);
    }
}
}
