#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ApplicationPaths
{
std::filesystem::path executableDirectory();
std::filesystem::path developmentRoot();
std::filesystem::path userDataRoot();
std::filesystem::path writableRoot();
std::vector<std::filesystem::path> runtimeSearchRoots();
std::filesystem::path databasePath();
std::filesystem::path settingsPath();
std::string storageDescription();
} // namespace ApplicationPaths

