#include "Models/Project/CrashReportService.hpp"
#include "Models/Core/ProductInfo.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
void setEnvironment(const char *name, const std::string &value)
{
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

std::string readFile(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}
} // namespace

int main()
{
    if (std::string(ProductInfo::windowTitle) != "GM Editor 0.9.0 Community Preview")
    {
        std::cerr << "Release window title is not pinned to the 0.9.0 Community Preview identity.\n";
        return 1;
    }

    const std::filesystem::path root = std::filesystem::temp_directory_path() /
                                       "gm-editor-crash-report-tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    setEnvironment("TTRPG_DATA_ROOT", root.string());

    const CrashReportResult result = CrashReportService::write("deliberate test failure", "test details");
    if (!result.written || result.path != root / "Logs" / "last-crash.txt")
    {
        std::cerr << "Crash report was not written to the configured data root.\n";
        return 1;
    }

    const std::string contents = readFile(result.path);
    if (contents.find("version=0.9.0") == std::string::npos ||
        contents.find("summary=deliberate test failure") == std::string::npos ||
        contents.find("details=test details") == std::string::npos)
    {
        std::cerr << "Crash report is missing release or diagnostic metadata.\n";
        return 1;
    }

    std::filesystem::remove_all(root, error);
    std::cout << "Crash report tests passed.\n";
    return 0;
}
