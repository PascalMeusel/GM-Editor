#include "MapCreatorInternals.hpp"
#include "Models/Map/MapImageImportService.hpp"
#include "Models/Localization/LocalizationService.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwchar>
#include <iomanip>
#include <iterator>
#include <sstream>
#if defined(_WIN32)
#include <Windows.h>
#include <commdlg.h>
#endif

namespace gm::map_creator_detail {
int clampOptionalIndex(int index, int size)
{
    if (index < 0 || size <= 0)
        return -1;
    return std::min(index, size - 1);
}

const std::vector<MapWayKind> &wayPalette()
{
    static const std::vector<MapWayKind> palette = {
        MapWayKind::Trail,
        MapWayKind::DirtRoad,
        MapWayKind::StoneRoad,
        MapWayKind::River,
        MapWayKind::Canal,
        MapWayKind::DungeonCorridor,
        MapWayKind::CaveTunnel};
    return palette;
}

std::string sanitizeFileName(std::string value)
{
    for (char &character : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(character)) && character != '-' && character != '_')
            character = '_';
    }
    return value.empty() ? "map" : value;
}

std::string localizedMapImageImportError(const std::string &errorCode)
{
    if (gm::localization::LocalizationService::instance().locale() ==
        gm::localization::Locale::German)
        return gm::localization::trSystemMessage(errorCode);
    return MapImageImport::errorMessage(errorCode);
}

#if defined(_WIN32)
std::wstring mapCreatorUtf8ToWide(std::string_view utf8)
{
    std::wstring wide;
    if (utf8.empty())
        return wide;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          utf8.data(), static_cast<int>(utf8.size()),
                                          nullptr, 0);
    if (count <= 0)
        return wide;
    wide.resize(static_cast<std::size_t>(count));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            utf8.data(), static_cast<int>(utf8.size()),
                            wide.data(), count) != count)
        wide.clear();
    return wide;
}

std::wstring mapCreatorWideDialogFilter(const char *filter)
{
    std::wstring result;
    if (!filter)
        return result;
    const char *cursor = filter;
    while (*cursor != '\0')
    {
        const std::size_t length = std::strlen(cursor);
        const std::wstring segment =
            mapCreatorUtf8ToWide(std::string_view(cursor, length));
        if (segment.empty() && length != 0)
            return {};
        result.append(segment);
        result.push_back(L'\0');
        cursor += length + 1;
    }
    result.push_back(L'\0');
    return result;
}
#endif

bool browseForOpenPath(const char *filter, std::filesystem::path &path)
{
#if defined(_WIN32)
    const std::wstring wideFilter = mapCreatorWideDialogFilter(filter);
    if (wideFilter.empty())
        return false;

    wchar_t fileName[32768] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = wideFilter.c_str();
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = static_cast<DWORD>(std::size(fileName));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog))
        return false;
    path = std::filesystem::path(fileName);
    return true;
#else
    (void)filter;
    (void)path;
    return false;
#endif
}

bool browseForSavePath(const char *filter, const char *defaultExtension,
                       const std::string &suggestedName, std::filesystem::path &path)
{
#if defined(_WIN32)
    const std::wstring wideFilter = mapCreatorWideDialogFilter(filter);
    const std::wstring wideSuggestedName = mapCreatorUtf8ToWide(suggestedName);
    const std::wstring wideDefaultExtension =
        mapCreatorUtf8ToWide(defaultExtension ? defaultExtension : "");
    if (wideFilter.empty() || (!suggestedName.empty() && wideSuggestedName.empty()) ||
        (defaultExtension && *defaultExtension != '\0' && wideDefaultExtension.empty()))
        return false;

    wchar_t fileName[32768] = {};
    if (!wideSuggestedName.empty())
        wcsncpy_s(fileName, std::size(fileName), wideSuggestedName.c_str(), _TRUNCATE);
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = wideFilter.c_str();
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = static_cast<DWORD>(std::size(fileName));
    dialog.lpstrDefExt = wideDefaultExtension.empty() ? nullptr : wideDefaultExtension.c_str();
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog))
        return false;
    path = std::filesystem::path(fileName);
    return true;
#else
    (void)filter;
    (void)defaultExtension;
    (void)suggestedName;
    (void)path;
    return false;
#endif
}

const char *generatorPresetLabel(int presetIndex)
{
    switch (std::clamp(presetIndex, 0, 4))
    {
    case 0:
        return "City";
    case 1:
        return "Forest";
    case 2:
        return "Mountain Area";
    case 3:
        return "Dungeon";
    case 4:
        return "Cave";
    }
    return "Generator";
}

const char *mountainPatternLabel(MapMountainPattern pattern)
{
    switch (pattern)
    {
    case MapMountainPattern::Mixed:
        return "mixed mountains";
    case MapMountainPattern::Ranges:
        return "ranges";
    case MapMountainPattern::IndividualPeaks:
        return "individual peaks";
    }
    return "mountains";
}

const char *roadNetworkLabel(MapRoadNetworkMode mode)
{
    switch (mode)
    {
    case MapRoadNetworkMode::None:
        return "no roads";
    case MapRoadNetworkMode::Sparse:
        return "sparse roads";
    case MapRoadNetworkMode::Connected:
        return "connected roads";
    case MapRoadNetworkMode::Grid:
        return "grid roads";
    }
    return "roads";
}

const char *qualitySearchLabel(int qualityIndex)
{
    switch (std::clamp(qualityIndex, 0, 2))
    {
    case 0:
        return "Draft (best of 12)";
    case 1:
        return "Balanced (best of 24)";
    case 2:
        return "Polished (best of 48)";
    }
    return "Balanced";
}

MapCreationIntent inferredCreationIntent(const MapModel &map)
{
    switch (map.atlasLevel)
    {
    case MapLevel::World:
        return MapCreationIntent::WorldOverview;
    case MapLevel::Region:
        return MapCreationIntent::RegionExploration;
    case MapLevel::Tactical:
        return MapCreationIntent::TacticalEncounter;
    case MapLevel::Site:
        if (map.mapKind == MapKind::Settlement)
            return MapCreationIntent::Settlement;
        if (map.mapKind == MapKind::Dungeon || map.scope == MapScope::Dungeon)
            return MapCreationIntent::Dungeon;
        return MapCreationIntent::TravelScene;
    }
    return MapCreationIntent::RegionExploration;
}

MapSizePreset inferredSizePreset(MapCreationIntent intent, int width, int height)
{
    for (const MapSizePreset preset : {MapSizePreset::Small, MapSizePreset::Standard, MapSizePreset::Large})
    {
        const MapDimensions dimensions = mapDimensionsForPreset(intent, preset);
        if (dimensions.width == width && dimensions.height == height)
            return preset;
    }
    return MapSizePreset::Custom;
}

std::string localizedGenerationStage(const std::string &stage)
{
    if (stage.empty())
        return {};
    const std::string exact = gm::localization::trUi(stage.c_str());
    if (exact != stage)
        return exact;
    constexpr const char *candidatePrefix = "Evaluating candidate ";
    if (stage.rfind(candidatePrefix, 0) == 0)
        return std::string(gm::localization::trUi("Evaluating candidate")) + " " + stage.substr(std::char_traits<char>::length(candidatePrefix));
    return stage;
}

std::string elapsedGenerationLabel(std::chrono::steady_clock::time_point startedAt)
{
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt).count();
    std::ostringstream label;
    if (elapsedMs < 1000)
        label << elapsedMs << " ms";
    else
        label << std::fixed << std::setprecision(1) << (static_cast<double>(elapsedMs) / 1000.0) << " s";
    return label.str();
}
}
