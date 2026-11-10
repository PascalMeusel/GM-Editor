#pragma once

#include <filesystem>
#include <string>

struct CrashReportResult
{
    bool written = false;
    std::filesystem::path path;
    std::string error;
};

namespace CrashReportService
{
CrashReportResult write(const std::string &summary,
                        const std::string &details = std::string());
}
