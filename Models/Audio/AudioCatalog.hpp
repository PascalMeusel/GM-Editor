#pragma once

#include "Models/Audio/AudioModels.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct AudioCatalogEntry
{
    std::string id;
    std::string name;
    AudioAssetType type = AudioAssetType::Other;
    std::string theme;
    std::filesystem::path relativePath;
    float defaultVolume = 1.0f;
    bool loop = false;
    std::string sourceTitle;
    std::string author;
    std::string license;
    std::string sourceUrl;
};

std::vector<AudioCatalogEntry> loadBundledAudioCatalog(const std::filesystem::path &projectRoot, std::string &errorMessage);
