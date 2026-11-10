#pragma once

#include "Models/Project/ProjectAssetImportService.hpp"

#include <string>

namespace ProjectAssetRepairPanel
{
struct AssetRepairPanelState
{
    std::string replacementPath;
};

struct AssetRepairPanelEntry
{
    ProjectAssetImport::AssetKind kind = ProjectAssetImport::AssetKind::GenericFile;
    std::string displayName;
    std::string storedPath;
    std::string resolvedPath;
    bool missing = false;
};

bool draw(const char *id, AssetRepairPanelState &state, const AssetRepairPanelEntry &entry);
}
