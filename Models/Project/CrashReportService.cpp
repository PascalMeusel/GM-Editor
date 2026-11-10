#include "CrashReportService.hpp"

#include "Models/Core/ProductInfo.hpp"
#include "Models/Project/ApplicationPaths.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace CrashReportService
{
namespace
{
std::string utcTimestamp()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}
} // namespace

CrashReportResult write(const std::string &summary, const std::string &details)
{
    CrashReportResult result;
    result.path = ApplicationPaths::writableRoot() / "Logs" / "last-crash.txt";

    std::error_code filesystemError;
    std::filesystem::create_directories(result.path.parent_path(), filesystemError);
    if (filesystemError)
    {
        result.error = "Could not create the log directory: " + filesystemError.message();
        return result;
    }

    std::filesystem::path temporaryPath = result.path;
    temporaryPath += ".writing";
    std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        result.error = "Could not open the crash report for writing.";
        return result;
    }

    stream << "product=" << ProductInfo::name << '\n'
           << "version=" << ProductInfo::version << '\n'
           << "channel=" << ProductInfo::releaseChannel << '\n'
           << "occurred_at_utc=" << utcTimestamp() << '\n'
           << "data_root=" << ApplicationPaths::writableRoot().lexically_normal().string() << '\n'
           << "summary=" << summary << '\n';
    if (!details.empty())
        stream << "details=" << details << '\n';
    stream.flush();
    if (!stream)
    {
        result.error = "Could not finish writing the crash report.";
        stream.close();
        std::filesystem::remove(temporaryPath, filesystemError);
        return result;
    }
    stream.close();

    std::filesystem::remove(result.path, filesystemError);
    filesystemError.clear();
    std::filesystem::rename(temporaryPath, result.path, filesystemError);
    if (filesystemError)
    {
        result.error = "Could not publish the crash report: " + filesystemError.message();
        std::filesystem::remove(temporaryPath, filesystemError);
        return result;
    }

    result.written = true;
    return result;
}
} // namespace CrashReportService
