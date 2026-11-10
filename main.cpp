#include "Application.hpp"
#include "Models/Core/ProductInfo.hpp"
#include "Models/Project/ApplicationPaths.hpp"
#include "Models/Project/CrashReportService.hpp"
#include <iostream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace
{
std::string pathForDiagnosticOutput(const std::filesystem::path &path)
{
    // Command-line diagnostics are consumed by package/smoke tooling.  Use a
    // defined encoding instead of the Windows ANSI code page so non-ASCII
    // user-data paths survive redirected output.
    return path.u8string();
}

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string &value)
{
    if (value.empty())
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           value.data(), static_cast<int>(value.size()),
                                           nullptr, 0);
    if (length <= 0)
        return std::wstring(value.begin(), value.end());
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        value.data(), static_cast<int>(value.size()),
                        result.data(), length);
    return result;
}
#endif

void showFatalError(const std::string &message)
{
    const CrashReportResult report = CrashReportService::write(message);
    std::string visible = "GM Editor konnte nicht gestartet werden.\n"
                          "GM Editor could not start.\n\n" +
                          message;
    if (report.written)
        visible += "\n\nFehlerbericht / crash report:\n" + report.path.u8string();
    else if (!report.error.empty())
        visible += "\n\nDer Fehlerbericht konnte nicht gespeichert werden.\n" + report.error;

    std::cerr << visible << std::endl;
#if defined(_WIN32)
    const std::wstring wideVisible = utf8ToWide(visible);
    MessageBoxW(nullptr, wideVisible.c_str(), L"GM Editor - Startfehler / Startup Error", MB_OK | MB_ICONERROR);
#endif
}
} // namespace

int main(int argc, char **argv)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--version")
        {
            std::cout << ProductInfo::name << ' ' << ProductInfo::version << ' '
                      << ProductInfo::releaseChannel << '\n';
            return 0;
        }
        if (argument == "--print-paths")
        {
            const std::filesystem::path dataRoot = ApplicationPaths::writableRoot();
            std::cout << "executable_root=" << pathForDiagnosticOutput(ApplicationPaths::executableDirectory()) << '\n'
                      << "data_root=" << pathForDiagnosticOutput(dataRoot) << '\n'
                      << "database=" << pathForDiagnosticOutput(ApplicationPaths::databasePath()) << '\n'
                      << "settings=" << pathForDiagnosticOutput(ApplicationPaths::settingsPath()) << '\n'
                      << "crash_reports=" << pathForDiagnosticOutput(dataRoot / "Logs") << '\n';
            return 0;
        }
    }

    try
    {
        auto app = std::make_shared<Application>();
        app->run();
    }
    catch (const std::exception &e)
    {
        showFatalError(e.what());
        return 1;
    }
    catch (...)
    {
        showFatalError("Unknown fatal error / Unbekannter schwerwiegender Fehler");
        return 1;
    }
    return 0;
}
