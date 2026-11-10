#pragma once
#include "Models/Map/MapModels.hpp"
#include "Models/Map/Generation/MapGenerationTypes.hpp"
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gm::map_creator_detail {
int clampOptionalIndex(int index, int size);
const std::vector<MapWayKind> &wayPalette();
std::string sanitizeFileName(std::string value);
std::string localizedMapImageImportError(const std::string &errorCode);
#if defined(_WIN32)
std::wstring mapCreatorUtf8ToWide(std::string_view utf8);
#endif
#if defined(_WIN32)
std::wstring mapCreatorWideDialogFilter(const char *filter);
#endif
bool browseForOpenPath(const char *filter, std::filesystem::path &path);
bool browseForSavePath(const char *filter, const char *defaultExtension,
                       const std::string &suggestedName, std::filesystem::path &path);
const char *generatorPresetLabel(int presetIndex);
const char *mountainPatternLabel(MapMountainPattern pattern);
const char *roadNetworkLabel(MapRoadNetworkMode mode);
const char *qualitySearchLabel(int qualityIndex);
MapCreationIntent inferredCreationIntent(const MapModel &map);
MapSizePreset inferredSizePreset(MapCreationIntent intent, int width, int height);
std::string localizedGenerationStage(const std::string &stage);
std::string elapsedGenerationLabel(std::chrono::steady_clock::time_point startedAt);
}
