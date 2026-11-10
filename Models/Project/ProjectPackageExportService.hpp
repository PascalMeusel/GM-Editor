#pragma once

#include "Models/Campaign/Campaign.hpp"

#include <filesystem>
#include <string>

namespace ProjectPackageExport
{
struct ExportRequest
{
    std::filesystem::path destinationDirectory;
    bool overwriteExisting = false;
};

struct ExportResult
{
    bool success = false;
    std::string errorMessage;
    std::filesystem::path exportPath;
    std::filesystem::path manifestPath;
    std::filesystem::path databasePath;
    std::uintmax_t fileCount = 0;
    std::uintmax_t byteCount = 0;
};

std::filesystem::path defaultExportDirectory();
std::filesystem::path packageExportPath(const Campaign &campaign, const std::filesystem::path &destinationDirectory);
ExportResult exportCampaignPackage(const Campaign &campaign, const ExportRequest &request);
}
