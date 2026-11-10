#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

// This log deliberately does not use stdout/stderr: a desktop launch can have
// no console, inherited pipes, or a different host than a terminal launch.
// Each process gets a separate file so a successful retry preserves the failure.
class StartupDiagnostics
{
public:
    bool open(const std::filesystem::path &dataRoot) noexcept;
    void record(std::string_view event, std::string_view detail = {}) noexcept;
    const std::filesystem::path &path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
    std::ofstream stream_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
};

// Check the backend result in Release as well as Debug. ImGui's lazy device
// creation otherwise reports failure only through an assertion in NewFrame.
template <typename Operation>
void requireStartupStep(StartupDiagnostics &diagnostics, std::string_view step,
                        Operation &&operation, const char *failureMessage)
{
    diagnostics.record(step, "begin");
    try
    {
        if (!std::forward<Operation>(operation)())
            throw std::runtime_error(failureMessage);
    }
    catch (const std::exception &error)
    {
        diagnostics.record(step, error.what());
        throw;
    }
    catch (...)
    {
        diagnostics.record(step, "unknown failure");
        throw;
    }
    diagnostics.record(step, "complete");
}
