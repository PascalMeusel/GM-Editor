#pragma once

#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"

#include <cstdio>
#include <string>

namespace gm::map_viewer_detail
{
extern const char *travelPaceItems;
const char *displayTravelPace(const std::string &pace);
void drawDisabledWrapped(const std::string &text);

template <typename... Args>
std::string formatLocalizedUi(const char *format, Args... args)
{
    const std::string localizedFormat = localizedUiLabel(format);
    const int required = std::snprintf(nullptr, 0, localizedFormat.c_str(), args...);
    if (required <= 0)
        return localizedFormat;
    std::string value(static_cast<std::size_t>(required), '\0');
    std::snprintf(value.data(), value.size() + 1, localizedFormat.c_str(), args...);
    return value;
}

const char *mapViewerModeLabel(MapViewerInteractionMode mode);
const char *atlasLevelLabel(MapLevel level);
const char *atlasStatusLabel(MapAtlasStatus status);
std::string coordinateLabel(int x, int y);
std::string localizedEdgeMaskLabel(int edgeMask);
std::string mapDisplayName(const MapModel &map);
const MapRouteModel *findRouteById(const MapModel &map, const EntityId &routeId);
std::string routeDisplayName(const MapRouteModel &route);
float travelProgressFraction(const PartyTravelPlanModel &travel);
bool hasActiveTravelRoute(const PartyTravelPlanModel &travel);
bool isTravelRouteComplete(const PartyTravelPlanModel &travel);
std::string formatMiles(float miles);
std::string travelProgressText(const PartyTravelPlanModel &travel);
std::string partyLocationText(const Campaign &campaign, const MapModel &map);
void drawPartyMarkerContext(const Campaign &campaign, const MapModel &map, bool pending, int x, int y);
std::string travelResourcesText(const PartyTravelPlanModel &travel);
std::string compactTravelNotes(const std::string &notes);
std::string travelRouteNameForMap(const PartyTravelPlanModel &travel, const MapModel &map);
std::string nextTravelActionText(const PartyTravelPlanModel &travel, const MapModel &map,
                                 bool moveMode, bool hasPendingMove);
void drawContextualVisualAssetRows(const MapModel &map, int x, int y);
bool drawAtlasDesignPreview(const MapModel &map, MapRenderPlanCache &renderPlanCache,
                            std::uint64_t contentVersion, const char *id,
                            float height, bool selected);
}
