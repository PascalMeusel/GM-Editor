#pragma once

#include "Models/Project/ProjectSaveService.hpp"

#include <filesystem>
#include <string>

struct ProjectSaveSettingsLoadResult
{
    ProjectSaveConfig config;
    bool fileFound = false;
    bool valuesFound = false;
    std::string error;

    bool succeeded() const { return error.empty(); }
};

namespace ProjectSaveSettingsStore
{
ProjectSaveSettingsLoadResult load(const std::filesystem::path &path,
                                   const ProjectSaveConfig &defaults = ProjectSaveConfig{});
bool save(const std::filesystem::path &path,
          const ProjectSaveConfig &config,
          std::string *error = nullptr);
} // namespace ProjectSaveSettingsStore
