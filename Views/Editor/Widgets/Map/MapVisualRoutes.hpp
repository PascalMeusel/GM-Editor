#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapClassicIllustration.hpp"
#include "Views/Editor/Widgets/Map/MapAtlasInkDrawing.hpp"
#include "Models/Map/MapTacticalCoverInk.hpp"
#include "Models/Map/MapSettlementSiteInk.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Views/Editor/Widgets/Map/MapCanvasInteraction.hpp"
#pragma once
#include "Models/Map/MapRenderTimings.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapTravelInk.hpp"

#include "Models/Map/MapGeneratedNamePresentation.hpp"
#include "Models/Map/MapAtlasWaterInk.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapOverviewLabelLod.hpp"
#include "Models/Map/MapRuntimeVisualAssets.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapRoutePresentation.hpp"
#include "Views/Editor/Widgets/Map/MapTileAtlas.hpp"
#include "Views/Editor/Widgets/UiHelp.hpp"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

struct MapVisibleCellBounds
{
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;
};

inline const MapRenderPrimitive *mapRoutePrimitiveInPlan(
    const MapRenderPlan &plan,
    const EntityId &routeId)
{
    const auto found = std::find_if(
        plan.primitives.begin(), plan.primitives.end(),
        [&](const MapRenderPrimitive &primitive) {
            return primitive.kind == MapRenderPrimitiveKind::Route &&
                   primitive.sourceId == routeId;
        });
    return found == plan.primitives.end() ? nullptr : &*found;
}

inline gm::map_route_presentation::Path mapRoutePresentationForPlan(
    const MapModel &map,
    const MapRouteModel &route,
    const MapRenderPlan *plan = nullptr)
{
    if (plan)
    {
        if (const MapRenderPrimitive *primitive =
                mapRoutePrimitiveInPlan(*plan, route.metadata.id))
        {
            gm::map_route_presentation::Path path;
            path.source = gm::map_route_presentation::sourceFromAssetRole(
                primitive->assetRole);
            path.points = primitive->points;
            return path;
        }
    }
    return gm::map_route_presentation::build(map, route);
}

inline ImVec2 mapRoutePresentationScreenPoint(ImVec2 origin,
                                               float cellSize,
                                               MapFeaturePoint point)
{
    return {origin.x + point.x * cellSize,
            origin.y + point.y * cellSize};
}

inline void drawMapRoutePresentationPath(
    ImDrawList *drawList,
    ImVec2 origin,
    float cellSize,
    const gm::map_route_presentation::Path &path,
    ImU32 color,
    float thickness)
{
    if (!drawList || cellSize <= 0.0f || path.points.size() < 2)
        return;
    const bool fallback = path.isFallback();
    const float dashLength = std::clamp(cellSize * 0.42f, 4.0f, 12.0f);
    const float gapLength = std::clamp(cellSize * 0.24f, 3.0f, 8.0f);
    for (std::size_t index = 1; index < path.points.size(); ++index)
    {
        const ImVec2 start = mapRoutePresentationScreenPoint(
            origin, cellSize, path.points[index - 1]);
        const ImVec2 end = mapRoutePresentationScreenPoint(
            origin, cellSize, path.points[index]);
        if (!fallback)
        {
            drawList->AddLine(start, end, color, thickness);
            continue;
        }
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.001f)
            continue;
        for (float offset = 0.0f; offset < length; offset += dashLength + gapLength)
        {
            const float from = offset / length;
            const float to = std::min(length, offset + dashLength) / length;
            drawList->AddLine(
                ImVec2(start.x + dx * from, start.y + dy * from),
                ImVec2(start.x + dx * to, start.y + dy * to),
                color, thickness);
        }
    }
    if (path.source == gm::map_route_presentation::Source::DirectFallback)
    {
        const float radius = std::clamp(cellSize * 0.13f, 2.5f, 6.0f);
        drawList->AddCircle(
            mapRoutePresentationScreenPoint(origin, cellSize, path.points.front()),
            radius, color, 12, std::max(1.0f, thickness * 0.5f));
        drawList->AddCircle(
            mapRoutePresentationScreenPoint(origin, cellSize, path.points.back()),
            radius, color, 12, std::max(1.0f, thickness * 0.5f));
    }
}

// Generated names are persisted in an English, stable form so generator and
// QA logic never depend on the active locale. Translate only known generated
// prefixes at presentation time; arbitrary GM-authored names pass through.
inline std::string localizedGeneratedMapName(const std::string &name,
                                             std::string_view locale = {})
{
    if (locale.empty())
        locale = gm::localization::LocalizationService::instance().locale() ==
                         gm::localization::Locale::German
                     ? "de"
                     : "en";
    return gm::map_presentation::generatedName(name, locale);
}

inline bool mapGeneratedNameHasNumericSuffix(const std::string &name, const char *prefix)
{
    return prefix && gm::map_presentation::hasNumericGeneratedSuffix(name, prefix);
}

// Only semantically meaningful, concise place names belong on the finished
// atlas. Generator placeholders and unmatched fragments from the GM's prose
// remain available in the recipe/inspector, but must not become map labels.
inline bool mapIsSemanticCartographicName(const std::string &name)
{
    if (name.empty() || name.size() > 64 || name.find('\n') != std::string::npos ||
        name.find('\r') != std::string::npos || name.find(';') != std::string::npos)
        return false;
    if (name == "Required feature" || name == "Erforderliches Element" ||
        mapGeneratedNameHasNumericSuffix(name, "Settlement") ||
        mapGeneratedNameHasNumericSuffix(name, "Siedlung") ||
        mapGeneratedNameHasNumericSuffix(name, "Landmark") ||
        mapGeneratedNameHasNumericSuffix(name, "Landmarke"))
        return false;

    const std::size_t words = static_cast<std::size_t>(std::count(name.begin(), name.end(), ' ')) + 1;
    if (words > 7 || name.back() == '.' || name.back() == '!' || name.back() == '?')
        return false;

    std::string lowered;
    lowered.reserve(name.size() + 2);
    lowered.push_back(' ');
    for (const unsigned char character : name)
        lowered.push_back(static_cast<char>(std::tolower(character)));
    lowered.push_back(' ');
    constexpr const char *noteWords[] = {
        " share ", " shares ", " shared ", " has ", " have ", " is ", " are ",
        " there ", " with ", " without ", " contains ", " include ", " includes ",
        " teilen ", " teilt ", " gibt ", " haben ", " hat ", " sind ", " ist ",
        " mit ", " ohne ", " enthält ", " enthaelt ", " soll ", " sollen "};
    return std::none_of(std::begin(noteWords), std::end(noteWords), [&](const char *word) {
        return lowered.find(word) != std::string::npos;
    });
}

inline MapVisibleCellBounds mapVisibleCellBounds(ImDrawList *drawList, ImVec2 origin, float cellSize, const MapModel &map, int padding = 1)
{
    if (!drawList || cellSize <= 0.0f || map.width <= 0 || map.height <= 0)
        return {};
    const ImVec2 clipMin = drawList->GetClipRectMin();
    const ImVec2 clipMax = drawList->GetClipRectMax();
    MapVisibleCellBounds bounds;
    bounds.minX = std::clamp(static_cast<int>(std::floor((clipMin.x - origin.x) / cellSize)) - padding, 0, map.width - 1);
    bounds.minY = std::clamp(static_cast<int>(std::floor((clipMin.y - origin.y) / cellSize)) - padding, 0, map.height - 1);
    bounds.maxX = std::clamp(static_cast<int>(std::ceil((clipMax.x - origin.x) / cellSize)) + padding, 0, map.width - 1);
    bounds.maxY = std::clamp(static_cast<int>(std::ceil((clipMax.y - origin.y) / cellSize)) + padding, 0, map.height - 1);
    if (clipMax.x < origin.x || clipMax.y < origin.y || clipMin.x > origin.x + map.width * cellSize || clipMin.y > origin.y + map.height * cellSize)
        return {};
    return bounds;
}
