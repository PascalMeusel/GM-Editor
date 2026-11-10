#include "Models/Map/MapGeneration.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include "Models/Map/MapTileSemantics.hpp"
#include "Models/Map/MapMountainPassRouting.hpp"
#include "Models/Map/MapRoadClearance.hpp"
#include "Models/Map/MapRegionComposition.hpp"
#include "Models/Map/MapWorldLandform.hpp"

std::string mapGenNormalizedElementText(const std::string &value)
{
    std::string normalized;
    normalized.reserve(value.size());
    bool pendingSpace = false;
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        const unsigned char character = static_cast<unsigned char>(value[index]);
        if (character == 0xC3u && index + 1 < value.size())
        {
            const unsigned char continuation = static_cast<unsigned char>(value[index + 1]);
            char replacement = '\0';
            if (continuation == 0x84u || continuation == 0xA4u)
                replacement = 'a';
            else if (continuation == 0x96u || continuation == 0xB6u)
                replacement = 'o';
            else if (continuation == 0x9Cu || continuation == 0xBCu)
                replacement = 'u';
            if (replacement != '\0')
            {
                if (pendingSpace && !normalized.empty())
                    normalized.push_back(' ');
                normalized.push_back(replacement);
                pendingSpace = false;
                ++index;
                continue;
            }
            if (continuation == 0x9Fu) // German sharp s
            {
                if (pendingSpace && !normalized.empty())
                    normalized.push_back(' ');
                normalized += "ss";
                pendingSpace = false;
                ++index;
                continue;
            }
        }
        if (std::isalnum(character) != 0)
        {
            if (pendingSpace && !normalized.empty())
                normalized.push_back(' ');
            normalized.push_back(static_cast<char>(std::tolower(character)));
            pendingSpace = false;
        }
        else
            pendingSpace = true;
    }
    return normalized;
}

bool mapGenExcludedElementMatches(const std::string &value, const std::vector<std::string> &excludedElements)
{
    const std::string normalizedValue = mapGenNormalizedElementText(value);
    if (normalizedValue.empty())
        return false;
    for (const std::string &excluded : excludedElements)
    {
        const std::string needle = mapGenNormalizedElementText(excluded);
        if (!needle.empty() && normalizedValue.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

bool mapGenNormalizedContainsToken(const std::string &normalized, const std::string &token)
{
    const std::string padded = " " + normalized + " ";
    return padded.find(" " + token + " ") != std::string::npos;
}

std::string mapGenSemanticElementKey(const std::string &value)
{
    const std::string normalized = mapGenNormalizedElementText(value);
    if (normalized.empty())
        return {};
    if (mapGenNormalizedContainsToken(normalized, "coast") || mapGenNormalizedContainsToken(normalized, "coastline") ||
        mapGenNormalizedContainsToken(normalized, "kuste") || mapGenNormalizedContainsToken(normalized, "kusten"))
        return "coast";
    if (mapGenNormalizedContainsToken(normalized, "island") || mapGenNormalizedContainsToken(normalized, "islands") ||
        mapGenNormalizedContainsToken(normalized, "insel") || mapGenNormalizedContainsToken(normalized, "inseln"))
        return "island";
    if (mapGenNormalizedContainsToken(normalized, "lake") || mapGenNormalizedContainsToken(normalized, "lakes") ||
        mapGenNormalizedContainsToken(normalized, "see") || mapGenNormalizedContainsToken(normalized, "seen") ||
        mapGenNormalizedContainsToken(normalized, "binnensee") ||
        mapGenNormalizedContainsToken(normalized, "binnenseen"))
        return "lake";
    if (mapGenNormalizedContainsToken(normalized, "river") || mapGenNormalizedContainsToken(normalized, "rivers") ||
        mapGenNormalizedContainsToken(normalized, "fluss") || mapGenNormalizedContainsToken(normalized, "flusse"))
        return "river";
    if (normalized == "mountain range" || normalized == "mountain ranges" ||
        mapGenNormalizedContainsToken(normalized, "mountain") ||
        mapGenNormalizedContainsToken(normalized, "mountains") ||
        mapGenNormalizedContainsToken(normalized, "gebirge") ||
        mapGenNormalizedContainsToken(normalized, "bergkette") ||
        mapGenNormalizedContainsToken(normalized, "bergketten") || mapGenNormalizedContainsToken(normalized, "berge"))
        return "mountain_range";
    if (mapGenNormalizedContainsToken(normalized, "forest") || mapGenNormalizedContainsToken(normalized, "forests") ||
        mapGenNormalizedContainsToken(normalized, "woodland") ||
        mapGenNormalizedContainsToken(normalized, "woodlands") || mapGenNormalizedContainsToken(normalized, "woods") ||
        mapGenNormalizedContainsToken(normalized, "wald") || mapGenNormalizedContainsToken(normalized, "walder") ||
        mapGenNormalizedContainsToken(normalized, "forst"))
        return "forest";
    if (mapGenNormalizedContainsToken(normalized, "road") || mapGenNormalizedContainsToken(normalized, "roads") ||
        mapGenNormalizedContainsToken(normalized, "highway") || mapGenNormalizedContainsToken(normalized, "highways") ||
        mapGenNormalizedContainsToken(normalized, "strasse") || mapGenNormalizedContainsToken(normalized, "strassen") ||
        mapGenNormalizedContainsToken(normalized, "fernstrasse") || mapGenNormalizedContainsToken(normalized, "weg") ||
        mapGenNormalizedContainsToken(normalized, "wege"))
        return "road";
    if (mapGenNormalizedContainsToken(normalized, "sea") || mapGenNormalizedContainsToken(normalized, "seas") ||
        mapGenNormalizedContainsToken(normalized, "ocean") || mapGenNormalizedContainsToken(normalized, "oceans") ||
        mapGenNormalizedContainsToken(normalized, "meer") || mapGenNormalizedContainsToken(normalized, "meere") ||
        mapGenNormalizedContainsToken(normalized, "ozean") || mapGenNormalizedContainsToken(normalized, "ozeane"))
        return "sea";
    if (mapGenNormalizedContainsToken(normalized, "water") || mapGenNormalizedContainsToken(normalized, "wasser"))
        return "water";
    return {};
}

std::string mapGenCanonicalExcludedSemanticKey(const std::string &value)
{
    const std::string key = mapGenSemanticElementKey(value);
    if (key.empty())
        return {};
    if (value == key)
        return key;
    const std::string normalized = mapGenNormalizedElementText(value);
    if (normalized.find(' ') == std::string::npos)
        return key;
    if (key == "mountain_range" && (normalized == "mountain range" || normalized == "mountain ranges"))
        return key;
    return {};
}

bool mapGenCellHasSemantic(const MapCellModel &cell, const std::string &key)
{
    if (key == "river")
        return cell.wayKind == MapWayKind::River || cell.wayKind == MapWayKind::Canal;
    if (key == "road")
        return cell.kind == MapTileKind::Road || cell.kind == MapTileKind::Bridge ||
               cell.wayKind == MapWayKind::Trail || cell.wayKind == MapWayKind::DirtRoad ||
               cell.wayKind == MapWayKind::StoneRoad;
    if (key == "forest")
        return cell.kind == MapTileKind::Forest;
    if (key == "mountain_range")
        return cell.kind == MapTileKind::Mountain;
    if (key == "water" || key == "sea" || key == "lake" || key == "coast" || key == "island")
        return cell.kind == MapTileKind::Water && cell.wayKind != MapWayKind::River &&
               cell.wayKind != MapWayKind::Canal;
    return false;
}

bool mapGenHasCoast(const MapModel &map)
{
    constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || !mapGenCellHasSemantic(*cell, "water"))
                continue;
            for (const auto &offset : offsets)
            {
                const MapCellModel *neighbor = mapCellAt(map, x + offset[0], y + offset[1]);
                if (neighbor && neighbor->kind != MapTileKind::Water)
                    return true;
            }
        }
    return false;
}

bool mapGenHasWaterComponent(const MapModel &map, bool mustTouchBoundary)
{
    if (map.width <= 0 || map.height <= 0)
        return false;
    std::vector<unsigned char> visited(static_cast<std::size_t>(map.width * map.height), 0);
    constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int startY = 0; startY < map.height; ++startY)
        for (int startX = 0; startX < map.width; ++startX)
        {
            const int startIndex = mapCellIndex(map, startX, startY);
            const MapCellModel *start = mapCellAt(map, startX, startY);
            if (!start || visited[static_cast<std::size_t>(startIndex)] || !mapGenCellHasSemantic(*start, "water"))
                continue;
            bool touchesBoundary = false;
            int componentSize = 0;
            std::queue<MapCellCoordinate> pending;
            pending.push({startX, startY});
            visited[static_cast<std::size_t>(startIndex)] = 1;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                ++componentSize;
                touchesBoundary = touchesBoundary || current.x == 0 || current.y == 0 || current.x == map.width - 1 ||
                                  current.y == map.height - 1;
                for (const auto &offset : offsets)
                {
                    const int x = current.x + offset[0], y = current.y + offset[1];
                    if (!isMapCellInside(map, x, y))
                        continue;
                    const int index = mapCellIndex(map, x, y);
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell || visited[static_cast<std::size_t>(index)] || !mapGenCellHasSemantic(*cell, "water"))
                        continue;
                    visited[static_cast<std::size_t>(index)] = 1;
                    pending.push({x, y});
                }
            }
            if (componentSize >= 2 && touchesBoundary == mustTouchBoundary)
                return true;
        }
    return false;
}

bool mapGenHasIsland(const MapModel &map)
{
    if (map.width <= 0 || map.height <= 0)
        return false;
    std::vector<unsigned char> visited(static_cast<std::size_t>(map.width * map.height), 0);
    constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (int startY = 0; startY < map.height; ++startY)
        for (int startX = 0; startX < map.width; ++startX)
        {
            const int startIndex = mapCellIndex(map, startX, startY);
            const MapCellModel *start = mapCellAt(map, startX, startY);
            if (!start || visited[static_cast<std::size_t>(startIndex)] || start->kind == MapTileKind::Water)
                continue;
            bool touchesBoundary = false, touchesWater = false;
            int componentSize = 0;
            std::queue<MapCellCoordinate> pending;
            pending.push({startX, startY});
            visited[static_cast<std::size_t>(startIndex)] = 1;
            while (!pending.empty())
            {
                const MapCellCoordinate current = pending.front();
                pending.pop();
                ++componentSize;
                touchesBoundary = touchesBoundary || current.x == 0 || current.y == 0 || current.x == map.width - 1 ||
                                  current.y == map.height - 1;
                for (const auto &offset : offsets)
                {
                    const int x = current.x + offset[0], y = current.y + offset[1];
                    if (!isMapCellInside(map, x, y))
                        continue;
                    const MapCellModel *cell = mapCellAt(map, x, y);
                    if (!cell)
                        continue;
                    if (mapGenCellHasSemantic(*cell, "water"))
                    {
                        touchesWater = true;
                        continue;
                    }
                    if (cell->kind == MapTileKind::Water)
                        continue;
                    const int index = mapCellIndex(map, x, y);
                    if (visited[static_cast<std::size_t>(index)])
                        continue;
                    visited[static_cast<std::size_t>(index)] = 1;
                    pending.push({x, y});
                }
            }
            if (componentSize >= 4 && !touchesBoundary && touchesWater)
                return true;
        }
    return false;
}

