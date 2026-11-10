#pragma once

#include "Models/Map/MapRenderPlan.hpp"
#include <algorithm>
#include <string>
#include <optional>
#include <utility>

namespace gm::mapui
{
// The generator is a short creation flow. An existing map has three peer
// workspaces; retaining the legacy edit/export IDs preserves controller entrypoints.
inline constexpr int mapAuthoringEdit = 3;
inline constexpr int mapAuthoringExport = 4;
inline constexpr int mapAuthoringPlayerPreview = 5;

inline bool mapCreatorIsAuthoring(int stage) { return stage >= mapAuthoringEdit; }

inline bool mapCreatorShouldFitOnStageChange(int previous, int next)
{
    return next == mapAuthoringEdit && !mapCreatorIsAuthoring(previous);
}

inline bool mapCreatorActionFitsRow(float usedWidth, float actionWidth, float spacing, float availableWidth)
{
    return usedWidth > 0.0f && usedWidth + spacing + actionWidth <= availableWidth;
}

inline float mapCreatorEditingRailWidth(float availableWidth,float fontSize)
{
    return std::min(availableWidth*.34f,300.f*std::max(1.f,fontSize/16.f));
}

inline bool mapCreatorUsesStackedEditWorkspace(float width, float height, float fontSize)
{
    const float font = std::max(1.0f, fontSize);
    const float rail = mapCreatorEditingRailWidth(width, font);
    // A map is not a paragraph: its useful width need not grow at the same
    // rate as text. Keep genuinely narrow docks stacked, but avoid spending a
    // short window on a top palette plus a second, tiny scrolling inspector.
    const bool usefulColumns = rail >= std::max(280.0f, font * 8.0f) &&
        width - rail - 24.0f >= std::max(420.0f, font * 16.0f);
    if (!usefulColumns) return true;
    const bool shortWorkspace = height < font * 24.0f;
    return !shortWorkspace && width < 900.0f * std::max(1.0f, font / 20.0f);
}

inline bool mapCreatorExperimentalIntent(MapCreationIntent intent)
{
    return intent == MapCreationIntent::Settlement || intent == MapCreationIntent::Dungeon ||
           intent == MapCreationIntent::Cave;
}

inline float mapCreatorCompactInspectorHeight(float availableHeight)
{
    // Reserve most of a short window for the actual map, including at 150% UI scale.
    return std::min(280.0f, std::max(96.0f, availableHeight * 0.36f));
}

inline bool mapCreatorPlayerPreviewCanShowMap(const MapModel &map)
{
    return map.visibility == MapVisibility::PlayersVisible;
}

class MapCreatorPlayerPreviewCache
{
public:
    const MapModel &filteredSource(const MapModel &map, const MapRenderPlan &plan)
    {
        if (!_map || _contentHash != plan.contentHash || !mapRenderPlanMatchesMap(plan, *_map))
        {
            _map = filterMapForRenderPlan(map, plan);
            _contentHash = plan.contentHash;
        }
        return *_map;
    }
private:
    std::optional<MapModel> _map;
    std::uint64_t _contentHash = 0;
};

inline MapRenderPlanRequest mapCreatorPlayerPreviewRequest(std::string locale)
{
    MapRenderPlanRequest request;
    request.locale = std::move(locale);
    request.audience = MapRenderAudience::Player;
    request.layers = MapRenderPlanAll & ~static_cast<std::uint32_t>(MapRenderPlanGrid);
    request.applyFog = true;
    request.showPoliticalOverlay = false;
    request.showGrid = false;
    // compileMapRenderPlan still clamps this request to persisted publication
    // consent. Preview must never grant consent or mutate map/display state.
    return request;
}
}
