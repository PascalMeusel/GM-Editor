#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

namespace gm::map_viewer_detail
{
const char *travelPaceItems = "Slow\0Normal\0Fast\0";

const char *displayTravelPace(const std::string &pace)
{
    // Older campaigns could persist experimental pace names such as "Careful".
    // The released travel model supports three paces and treats unknown values as
    // normal, so the summary must match the selected combo instead of exposing a
    // stale implementation value.
    if (pace == "Slow")
        return "Slow";
    if (pace == "Fast")
        return "Fast";
    return "Normal";
}

void drawDisabledWrapped(const std::string &text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

const char *mapViewerModeLabel(MapViewerInteractionMode mode)
{
    switch (mode)
    {
    case MapViewerInteractionMode::InspectSelect:
        return "Inspect / Select";
    case MapViewerInteractionMode::MoveParty:
        return uiTextForLocale("Place party marker", "Gruppenmarker setzen");
    case MapViewerInteractionMode::SelectDetailArea:
        return "Select Detail Area";
    case MapViewerInteractionMode::RevealFog:
        return "Reveal Fog";
    case MapViewerInteractionMode::HideFog:
        return "Hide with Fog";
    case MapViewerInteractionMode::PingPlayers:
        return "Ping Players";
    }
    return "Inspect / Select";
}

const char *atlasLevelLabel(MapLevel level)
{
    switch (level)
    {
    case MapLevel::World: return "World";
    case MapLevel::Region: return "Region";
    case MapLevel::Site: return "Site";
    case MapLevel::Tactical: return "Tactical";
    }
    return "World";
}

const char *atlasStatusLabel(MapAtlasStatus status)
{
    switch (status)
    {
    case MapAtlasStatus::Standalone: return "Standalone";
    case MapAtlasStatus::Linked: return "Current";
    case MapAtlasStatus::Stale:
    case MapAtlasStatus::UpdateAvailable: return "Update available";
    case MapAtlasStatus::Conflict: return "Conflict";
    case MapAtlasStatus::Unpositioned: return "Unpositioned";
    case MapAtlasStatus::Invalid: return "Invalid";
    }
    return "Standalone";
}

std::string coordinateLabel(int x, int y)
{
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

std::string localizedEdgeMaskLabel(int edgeMask)
{
    const int mask = normalizeMapEdgeMask(edgeMask);
    std::string label;
    const auto append = [&](const char *direction) {
        if (!label.empty())
            label += " / ";
        label += localizedUiLabel(direction);
    };
    if ((mask & 1) != 0) append("North Edge");
    if ((mask & 2) != 0) append("East Edge");
    if ((mask & 4) != 0) append("South Edge");
    if ((mask & 8) != 0) append("West Edge");
    return label.empty() ? localizedUiLabel("None") : label;
}

std::string mapDisplayName(const MapModel &map)
{
    return map.name.empty() ? localizedUiLabel("<Unnamed Map>") : map.name;
}

const MapRouteModel *findRouteById(const MapModel &map, const EntityId &routeId)
{
    if (routeId.empty())
        return nullptr;
    const auto routeIt = std::find_if(map.routes.begin(), map.routes.end(), [&](const MapRouteModel &route) {
        return route.metadata.id == routeId;
    });
    return routeIt == map.routes.end() ? nullptr : &*routeIt;
}

std::string routeDisplayName(const MapRouteModel &route)
{
    return route.name.empty() ? localizedUiLabel("<Unnamed Route>") : route.name;
}

float travelProgressFraction(const PartyTravelPlanModel &travel)
{
    if (travel.distanceMiles <= 0.0f)
        return 0.0f;
    return std::clamp(travel.milesTraveled / travel.distanceMiles, 0.0f, 1.0f);
}

bool hasActiveTravelRoute(const PartyTravelPlanModel &travel)
{
    return !travel.routeId.empty() && travel.distanceMiles > 0.0f;
}

bool isTravelRouteComplete(const PartyTravelPlanModel &travel)
{
    return hasActiveTravelRoute(travel) && travel.milesTraveled >= travel.distanceMiles;
}

std::string formatMiles(float miles)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(std::fabs(miles) < 10.0f ? 1 : 0) << miles;
    return stream.str();
}

std::string travelProgressText(const PartyTravelPlanModel &travel)
{
    if (!hasActiveTravelRoute(travel))
        return localizedUiLabel("No active progress");

    const int percent = static_cast<int>(std::round(travelProgressFraction(travel) * 100.0f));
    const std::string traveled = formatMiles(travel.milesTraveled);
    const std::string total = formatMiles(travel.distanceMiles);
    return formatLocalizedUi("%s / %s mi (%d%%)", traveled.c_str(), total.c_str(), percent);
}

std::string partyLocationText(const Campaign &campaign, const MapModel &map)
{
    if (campaign.partyLocation.mapId == map.metadata.id)
        return coordinateLabel(campaign.partyLocation.x, campaign.partyLocation.y);
    return localizedUiLabel("On another map");
}

void drawPartyMarkerContext(const Campaign &campaign, const MapModel &map, bool pending, int x, int y)
{
    const auto &location = campaign.partyLocation;
    const auto source = std::find_if(campaign.maps.begin(), campaign.maps.end(),
        [&](const MapModel &candidate) { return candidate.metadata.id == location.mapId; });
    const std::string current = source == campaign.maps.end() ? uiTextForLocale("Not placed", "Nicht gesetzt") :
        mapDisplayName(*source) + " · " + coordinateLabel(location.x, location.y);
    const std::string target = mapDisplayName(map);
    const char *choose = uiTextForLocale("choose a cell", "Feld wählen");
    const auto currentRow = [&](const std::string &value) {
        return formatLocalizedUi(uiTextForLocale("Current: %s", "Aktuell: %s"), value.c_str()); };
    const auto targetRow = [&](const std::string &value) {
        return formatLocalizedUi(uiTextForLocale("Destination: %s · %s", "Ziel: %s · %s"), target.c_str(), value.c_str()); };
    // Reserve the widest possible coordinate before a destination is picked.
    // Names, prompts and post-save current location are included in the same
    // fixed measurement so choosing/cancelling a destination cannot move the canvas.
    char digit = '0';
    for (char candidate = '1'; candidate <= '9'; ++candidate)
    { const char a[]{candidate, 0}, b[]{digit, 0}; if (ImGui::CalcTextSize(a).x > ImGui::CalcTextSize(b).x) digit = candidate; }
    const std::string widest = "(" + std::string(std::to_string(std::max(0, map.width - 1)).size(), digit) + ", " +
        std::string(std::to_string(std::max(0, map.height - 1)).size(), digit) + ")";
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const auto height = [&](const std::string &text) { return ImGui::CalcTextSize(text.c_str(), nullptr, false, width).y; };
    const float firstHeight = std::max(height(currentRow(current)), height(currentRow(target + " · " + widest)));
    const float secondHeight = std::max(height(targetRow(choose)), height(targetRow(widest)));
    if (ImGui::BeginChild("PartyMarkerContext", ImVec2(0, firstHeight + secondHeight + ImGui::GetStyle().ItemSpacing.y),
        ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::TextWrapped("%s", currentRow(current).c_str());
        ImGui::TextWrapped("%s", targetRow(pending ? coordinateLabel(x, y) : choose).c_str());
    }
    ImGui::EndChild();
}

std::string travelResourcesText(const PartyTravelPlanModel &travel)
{
    const std::string pace = localizedUiLabel(displayTravelPace(travel.travelPace));
    return formatLocalizedUi("%s pace, watch %d, %d rations, %d checks",
                             pace.c_str(),
                             travel.watches,
                             travel.rationsUsed,
                             travel.randomEncounterChecks);
}

std::string compactTravelNotes(const std::string &notes)
{
    if (notes.empty())
        return "";

    std::string compact = notes;
    std::replace(compact.begin(), compact.end(), '\n', ' ');
    static constexpr std::size_t maxLength = 120;
    if (compact.size() > maxLength)
        compact = compact.substr(0, maxLength - 3) + "...";
    return compact;
}

std::string travelRouteNameForMap(const PartyTravelPlanModel &travel, const MapModel &map)
{
    if (!hasActiveTravelRoute(travel))
        return localizedUiLabel("No active route");

    if (travel.currentMapId == map.metadata.id)
    {
        if (const MapRouteModel *route = findRouteById(map, travel.routeId))
            return routeDisplayName(*route);
    }

    return travel.routeName.empty() ? localizedUiLabel("<Unnamed Route>") : travel.routeName;
}

std::string nextTravelActionText(const PartyTravelPlanModel &travel, const MapModel &map, bool moveMode, bool /*hasPendingMove*/)
{
    if (moveMode)
        return uiTextForLocale("Choose a cell, then save the party location.",
            "Wähle ein Feld und speichere die Gruppenposition.");

    if (!hasActiveTravelRoute(travel))
        return localizedUiLabel(map.routes.empty() ? "Create a route before starting travel." : "Select a route, then Start Route.");

    if (isTravelRouteComplete(travel))
        return localizedUiLabel("Route complete. Start another route when ready.");

    return localizedUiLabel("Advance the next travel watch.");
}

void drawContextualVisualAssetRows(const MapModel &map, int x, int y)
{
    const std::vector<MapStyleContextualAssetResolution> resolutions =
        resolveMapStyleContextualAssetsForCellVisual(map, x, y);
    if (resolutions.empty())
        return;

    ImGui::Spacing();
    if (!beginAdvancedTreeNode("Visual Assets"))
        return;
    for (const MapStyleContextualAssetResolution &resolution : resolutions)
    {
        const std::string assetLabel = resolution.selectedAssetId.empty() ? localizedUiLabel("<none>") : resolution.selectedAssetId;
        ImGui::BulletText("%s", assetLabel.c_str());
        ImGui::Indent();
        if (!resolution.candidatePreviewAssetId.empty())
            ImGui::TextDisabled(localizedUiLabel("Candidate Preview: %s").c_str(), resolution.candidatePreviewAssetId.c_str());
        if (!resolution.workOrderId.empty())
            ImGui::TextDisabled(localizedUiLabel("Work Order: %s").c_str(), resolution.workOrderId.c_str());
        if (!resolution.fallbackReason.empty())
            ImGui::TextDisabled(localizedUiLabel("Fallback: %s").c_str(), resolution.fallbackReason.c_str());
        if (!resolution.qaLabel.empty())
            ImGui::TextDisabled(localizedUiLabel("QA: %s").c_str(), resolution.qaLabel.c_str());
        ImGui::Unindent();
    }
    ImGui::TreePop();
}

bool drawAtlasDesignPreview(const MapModel &map, MapRenderPlanCache &renderPlanCache,
                            std::uint64_t contentVersion, const char *id,
                            float height, bool selected)
{
    const float width = std::max(150.0f, ImGui::GetContentRegionAvail().x);
    const ImVec2 previewMin = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    const ImVec2 previewMax(previewMin.x + width, previewMin.y + height);
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(previewMin, previewMax, IM_COL32(28, 32, 29, 255), 6.0f);

    const float cellSize = std::max(0.1f, std::min(
        (width - 8.0f) / static_cast<float>(std::max(1, map.width)),
        (height - 8.0f) / static_cast<float>(std::max(1, map.height))));
    const ImVec2 mapOrigin(
        previewMin.x + (width - cellSize * static_cast<float>(map.width)) * 0.5f,
        previewMin.y + (height - cellSize * static_cast<float>(map.height)) * 0.5f);

    MapRenderPlanRequest request = gm::mapui::mapCreatorPreviewRenderPlanRequest(
        std::string(gm::localization::localeCode(
            gm::localization::LocalizationService::instance().locale())));
    const std::shared_ptr<const MapRenderPlan> cachedPlan =
        renderPlanCache.getOrCompile(map, request, contentVersion, cellSize);
    const MapRenderPlan &plan = *cachedPlan;

    drawList->PushClipRect(previewMin, previewMax, true);
    renderMap(drawList, mapOrigin, map, plan, cellSize);
    drawList->PopClipRect();
    drawList->AddRect(previewMin, previewMax,
                      selected ? IM_COL32(51, 157, 170, 255) : IM_COL32(101, 108, 101, 210),
                      6.0f, 0, selected ? 2.5f : 1.0f);
    return clicked;
}
}
