#include "Models/Project/StartupDiagnostics.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream content;
    content << stream.rdbuf();
    return content.str();
}

struct Fixture
{
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gm-editor-startup-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { std::filesystem::create_directories(root); }
    ~Fixture()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};

void testFailureIsFlushedAndRetryPreservesEvidence(const std::filesystem::path &root)
{
    StartupDiagnostics failedLaunch;
    require(failedLaunch.open(root), "Could not open the startup log.");
    bool renderLoopEntered = false;
    bool caught = false;
    try
    {
        requireStartupStep(failedLaunch, "imgui.platform", [] { return true; }, "platform unavailable");
        requireStartupStep(failedLaunch, "imgui.device_objects", [] { return false; }, "shader creation failed");
        renderLoopEntered = true;
    }
    catch (const std::runtime_error &error)
    {
        caught = std::string(error.what()) == "shader creation failed";
    }
    require(caught && !renderLoopEntered, "A failed graphics initialization must stop before the render loop in Release too.");
    // Read before destroying the writer, as a second process would during a stall.
    const std::string failedContent = readFile(failedLaunch.path());
    require(failedContent.find("imgui.platform complete") != std::string::npos &&
            failedContent.find("imgui.device_objects begin") != std::string::npos &&
            failedContent.find("imgui.device_objects shader creation failed") != std::string::npos &&
            failedContent.find("imgui.device_objects complete") == std::string::npos,
            "The last reached graphics phase was not flushed accurately.");
    require(failedContent.find("pid=") != std::string::npos &&
            failedContent.find("started_at_utc=") != std::string::npos &&
            failedContent.find("stdout=") != std::string::npos &&
            failedContent.find("stderr=") != std::string::npos &&
            failedContent.find("startup_show_command=") != std::string::npos,
            "Missing launch/standard-handle metadata.");

    StartupDiagnostics retry;
    require(retry.open(root), "Could not open a retry log.");
    requireStartupStep(retry, "imgui.device_objects", [] { return true; }, "shader creation failed");
    retry.record("frame.0.swap.returned");
    require(retry.path() != failedLaunch.path(), "A retry reused the failed launch's file.");
    require(readFile(failedLaunch.path()) == failedContent, "A retry overwrote failure evidence.");
    require(readFile(retry.path()).find("frame.0.swap.returned") != std::string::npos,
            "Successful first-frame progress was not flushed.");
}

void testOriginalExceptionAndOneLineDetails(const std::filesystem::path &root)
{
    struct BackendFailure : std::runtime_error { using std::runtime_error::runtime_error; };
    StartupDiagnostics diagnostics;
    require(diagnostics.open(root), "Could not open the exception log.");
    bool caughtOriginal = false;
    try
    {
        requireStartupStep(diagnostics, "renderer", []() -> bool { throw BackendFailure("driver failure"); }, "fallback");
    }
    catch (const BackendFailure &)
    {
        caughtOriginal = true;
    }
    require(caughtOriginal, "Startup error handling sliced or replaced the original exception.");
    diagnostics.record("glfw.error", "line one\r\nforged-phase complete");
    const std::string content = readFile(diagnostics.path());
    require(content.find("renderer driver failure") != std::string::npos,
            "A throwing backend failed without a diagnostic.");
    require(content.find("\nforged-phase") == std::string::npos,
            "Multiline backend text can forge a startup milestone.");
}

void testUnavailableLogNeverBlocksStartup(const std::filesystem::path &root)
{
    const auto blocked = root / "regular-file";
    { std::ofstream file(blocked); file << "unchanged"; }
    StartupDiagnostics diagnostics;
    require(!diagnostics.open(blocked), "A file cannot serve as the log directory.");
    diagnostics.record("startup.begin");
    bool initialized = false;
    requireStartupStep(diagnostics, "renderer", [&] { initialized = true; return true; }, "failure");
    require(initialized, "Unavailable diagnostics prevented startup.");
    require(readFile(blocked) == "unchanged", "Diagnostics modified the blocking file.");
}
} // namespace

int main()
{
    try
    {
        Fixture fixture;
        testFailureIsFlushedAndRetryPreservesEvidence(fixture.root);
        testOriginalExceptionAndOneLineDetails(fixture.root);
        testUnavailableLogNeverBlocksStartup(fixture.root);
        std::cout << "Startup diagnostics tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
