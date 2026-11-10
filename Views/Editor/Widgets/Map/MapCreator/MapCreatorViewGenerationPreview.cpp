#include "Views/Editor/Widgets/Map/MapCreator/MapCreatorViewDetails.hpp"

namespace gm::map_creator_view_detail
{
bool drawGenerationMiniMap(const MapModel &map, MapRenderPlanCache &renderPlanCache, std::uint64_t contentVersion,
                           const char *id, float height, float requestedWidth, bool showLabels,
                           bool showPoliticalBorders, const MapRenderPlanRequest *presentationRequest,
                           gm::mapui::MapCreatorPlayerPreviewCache *playerCache, const MapPngExportRect *outputView)
{
    const float width = std::max(120.0f, requestedWidth > 0.0f ? requestedWidth : ImGui::GetContentRegionAvail().x);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 previewMaximum(origin.x + width, origin.y + height);
    drawList->AddRectFilled(origin, previewMaximum, IM_COL32(30, 34, 31, 255), 6.0f);
    const bool playerPreview = presentationRequest && presentationRequest->audience == MapRenderAudience::Player;
    if (playerPreview && !gm::mapui::mapCreatorPlayerPreviewCanShowMap(map))
    {
        drawList->AddText(
            ImVec2(origin.x + 16.0f, origin.y + 16.0f), IM_COL32(190, 196, 192, 255),
            uiTextForLocale("This map is hidden from players.", "Diese Karte ist für Spieler verborgen."));
        return clicked;
    }

    const float contentGutter = gm::mapui::mapCreatorPreviewContentGutter(showLabels, width, height);
    const MapPngExportRect view = outputView ? *outputView : MapPngExportRect{0, 0, map.width, map.height};
    const auto frame = gm::mapui::mapCreatorPreviewFrame(map, view, width, height, contentGutter);
    if (!frame.valid)
    {
        drawList->AddText(ImVec2(origin.x + 16.0f, origin.y + 16.0f), IM_COL32(190, 196, 192, 255),
                          localizedUiText("The selected map view is invalid."));
        return clicked;
    }
    const float cellSize = frame.cellSize;
    const ImVec2 mapOrigin(origin.x + frame.mapX, origin.y + frame.mapY);
    const ImVec2 mapMaximum(mapOrigin.x + cellSize * map.width, mapOrigin.y + cellSize * map.height);
    const std::string locale(gm::localization::localeCode(gm::localization::LocalizationService::instance().locale()));
    MapRenderPlanRequest planRequest = !showLabels && !showPoliticalBorders
                                           ? gm::mapui::mapCreatorCandidateThumbnailRenderPlanRequest(locale)
                                           : gm::mapui::mapCreatorPreviewRenderPlanRequest(locale);
    planRequest.showPoliticalOverlay = showPoliticalBorders;
    if (!showLabels)
        planRequest.layers &= ~static_cast<std::uint32_t>(MapRenderPlanLabels);
    if (!showPoliticalBorders)
        planRequest.layers &= ~static_cast<std::uint32_t>(MapRenderPlanPolitical);
    if (presentationRequest)
        planRequest = *presentationRequest;
    const std::shared_ptr<const MapRenderPlan> cachedPlan =
        renderPlanCache.getOrCompile(map, planRequest, contentVersion, cellSize);
    const MapRenderPlan &plan = *cachedPlan;

    // Export previews use exact cell-bound crop edges. Keep all source
    // coordinates intact so background transforms and campaign consent are
    // evaluated on the original graph, then clip only the visible output.
    const ImVec2 clipMinimum = outputView ? ImVec2(origin.x + frame.clipX, origin.y + frame.clipY) : origin;
    const ImVec2 clipMaximum =
        outputView ? ImVec2(clipMinimum.x + frame.clipWidth, clipMinimum.y + frame.clipHeight) : previewMaximum;
    drawList->PushClipRect(clipMinimum, clipMaximum, true);
    bool authoritativeBackgroundDrawn = false;
    if (plan.backgroundVisible)
    {
        const CachedImageTexture &texture =
            getCachedImageTexture(ProjectAssets::resolveProjectPath(plan.backgroundInteractiveProjectRelativePath),
                                  plan.backgroundInteractiveContentHash);
        if (texture.loaded)
        {
            const MapNormalizedRect crop =
                map.importedReference.enabled ? map.importedReference.crop : MapNormalizedRect{0.0f, 0.0f, 1.0f, 1.0f};
            const float radians = -(map.importedReference.enabled ? map.importedReference.rotationDegrees : 0.0f) *
                                  0.01745329251994329577f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            const auto uv = [&](float x, float y) {
                const float centeredX = x - 0.5f;
                const float centeredY = y - 0.5f;
                const float rotatedX = cosine * centeredX - sine * centeredY + 0.5f;
                const float rotatedY = sine * centeredX + cosine * centeredY + 0.5f;
                return ImVec2(crop.x + rotatedX * crop.width, crop.y + rotatedY * crop.height);
            };
            drawList->AddImageQuad(texture.textureId, mapOrigin, ImVec2(mapMaximum.x, mapOrigin.y), mapMaximum,
                                   ImVec2(mapOrigin.x, mapMaximum.y), uv(0.0f, 0.0f), uv(1.0f, 0.0f), uv(1.0f, 1.0f),
                                   uv(0.0f, 1.0f), IM_COL32_WHITE);
            authoritativeBackgroundDrawn = true;
        }
    }

    // This is intentionally the same renderer used by the editor canvas and
    // player display.  The preview supplies no private terrain, line, symbol,
    // label or asset-selection implementation of its own.
    if (presentationRequest)
    {
        // An explicit output contract filters both audiences. GM copies may
        // also disable annotations/tokens/background or deliberately use fog.
        // Player terrain additionally contains private raster projections.
        gm::mapui::MapCreatorPlayerPreviewCache uncachedPlayerSource;
        auto &cache = playerCache ? *playerCache : uncachedPlayerSource;
        const MapModel &filtered = cache.filteredSource(map, plan);
        gm::mapui::drawMapCreatorOutputPreview(drawList, mapOrigin, filtered, plan, cellSize,
                                               authoritativeBackgroundDrawn,
                                               plan.applyFog ? getMapFogTexture(false).textureId : 0);
    }
    else
        renderMap(drawList, mapOrigin, map, plan, cellSize, authoritativeBackgroundDrawn);
    drawList->PopClipRect();
    drawList->AddRect(origin, previewMaximum, IM_COL32(105, 112, 105, 190), 6.0f);
    return clicked;
}

bool drawGenerationMiniMapFitted(const MapModel &map, MapRenderPlanCache &renderPlanCache, std::uint64_t contentVersion,
                                 const char *id, float maximumHeight, const MapRenderPlanRequest *presentationRequest,
                                 gm::mapui::MapCreatorPlayerPreviewCache *playerCache,
                                 const MapPngExportRect *outputView)
{
    const float availableWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x);
    const float aspect = static_cast<float>(std::max(1, outputView ? outputView->width : map.width)) /
                         static_cast<float>(std::max(1, outputView ? outputView->height : map.height));
    const float height =
        std::clamp(std::min(maximumHeight, availableWidth / std::max(0.1f, aspect)), 170.0f, maximumHeight);
    const float width = std::min(availableWidth, height * aspect);
    const float cursorX = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(cursorX + std::max(0.0f, (availableWidth - width) * 0.5f));
    const bool showLabels = !presentationRequest || (presentationRequest->layers & MapRenderPlanLabels) != 0;
    const bool showPolitical = presentationRequest && presentationRequest->showPoliticalOverlay;
    return drawGenerationMiniMap(map, renderPlanCache, contentVersion, id, height, width, showLabels, showPolitical,
                                 presentationRequest, playerCache, outputView);
}

float drawGenerationInteractivePreview(const MapModel &map, MapRenderPlanCache &renderPlanCache,
                                       std::uint64_t contentVersion, const char *id, float viewportHeight, float &zoom,
                                       bool showLabels, bool showPoliticalBorders, bool resetScroll,
                                       const MapRenderPlanRequest *presentationRequest,
                                       gm::mapui::MapCreatorPlayerPreviewCache *playerCache)
{
    const float availableWidth = std::max(160.0f, ImGui::GetContentRegionAvail().x);
    const float usableHeight = std::max(180.0f, viewportHeight);
    const float aspect = static_cast<float>(std::max(1, map.width)) / static_cast<float>(std::max(1, map.height));
    const float viewportWidth = gm::mapui::mapCreatorPreviewViewportWidth(availableWidth, usableHeight, aspect);
    const float fittedHeight = std::min(usableHeight - 12.0f, (viewportWidth - 12.0f) / std::max(0.1f, aspect));
    const float fittedWidth = fittedHeight * aspect;
    zoom = std::clamp(zoom, 1.0f, 4.0f);
    float contentWidth = std::max(120.0f, fittedWidth * zoom);
    float contentHeight = std::max(120.0f, fittedHeight * zoom);

    ImGui::PushID(id);
    const float viewportCursorX = ImGui::GetCursorPosX();
    if (viewportWidth < availableWidth)
        ImGui::SetCursorPosX(viewportCursorX + (availableWidth - viewportWidth) * 0.5f);
    ImGui::BeginChild("InteractiveDesignViewport", ImVec2(viewportWidth, usableHeight), true,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const float childWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float childHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    const bool viewportHovered = ImGui::IsWindowHovered();
    if (resetScroll)
    {
        ImGui::SetScrollX(std::max(0.0f, (contentWidth - childWidth) * 0.5f));
        ImGui::SetScrollY(std::max(0.0f, (contentHeight - childHeight) * 0.5f));
    }
    else if (viewportHovered && std::abs(ImGui::GetIO().MouseWheel) > 0.001f)
    {
        const float nextZoom = gm::mapui::mapCreatorPreviewZoomAfterWheel(zoom, ImGui::GetIO().MouseWheel);
        if (std::abs(nextZoom - zoom) > 0.001f)
        {
            const float nextContentWidth = std::max(120.0f, fittedWidth * nextZoom);
            const float nextContentHeight = std::max(120.0f, fittedHeight * nextZoom);
            const ImVec2 contentViewportMin(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x,
                                            ImGui::GetWindowPos().y + ImGui::GetStyle().WindowPadding.y);
            const float pointerX = std::clamp(ImGui::GetIO().MousePos.x - contentViewportMin.x, 0.0f, childWidth);
            const float pointerY = std::clamp(ImGui::GetIO().MousePos.y - contentViewportMin.y, 0.0f, childHeight);
            ImGui::SetScrollX(gm::mapui::mapCreatorPreviewScrollForPointerAnchor(ImGui::GetScrollX(), pointerX,
                                                                                 contentWidth, nextContentWidth));
            ImGui::SetScrollY(gm::mapui::mapCreatorPreviewScrollForPointerAnchor(ImGui::GetScrollY(), pointerY,
                                                                                 contentHeight, nextContentHeight));
            zoom = nextZoom;
            contentWidth = nextContentWidth;
            contentHeight = nextContentHeight;
        }
    }
    if (contentWidth < childWidth)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (childWidth - contentWidth) * 0.5f);
    if (contentHeight < childHeight)
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (childHeight - contentHeight) * 0.5f);

    drawGenerationMiniMap(map, renderPlanCache, contentVersion, "InteractiveDesignMap", contentHeight, contentWidth,
                          showLabels, showPoliticalBorders, presentationRequest, playerCache);
    const bool mapHovered = ImGui::IsItemHovered();
    const bool draggingMap = (mapHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) ||
                             (viewportHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f));
    if (draggingMap)
    {
        const ImVec2 drag = ImGui::GetIO().MouseDelta;
        ImGui::SetScrollX(ImGui::GetScrollX() - drag.x);
        ImGui::SetScrollY(ImGui::GetScrollY() - drag.y);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    ImGui::EndChild();
    ImGui::SetCursorPosX(viewportCursorX);
    // The preview can be the table's final item. Register the restored cursor
    // with an item so a short/empty preview never extends a parent implicitly.
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    ImGui::PopID();
    return fittedWidth;
}

std::string localizedGeneratedFeatureName(const MapGeneratedFeature &feature)
{
    return localizedGeneratedMapName(feature.name);
}

const char *generatedFeatureKindLabel(MapGeneratedFeatureKind kind)
{
    switch (kind)
    {
    case MapGeneratedFeatureKind::TerrainRegion:
        return "Terrain region";
    case MapGeneratedFeatureKind::River:
        return "River";
    case MapGeneratedFeatureKind::Road:
        return "Road";
    case MapGeneratedFeatureKind::Trail:
        return "Trail";
    case MapGeneratedFeatureKind::Settlement:
        return "Settlement";
    case MapGeneratedFeatureKind::Landmark:
        return "Landmark";
    case MapGeneratedFeatureKind::Bridge:
        return "Bridge";
    case MapGeneratedFeatureKind::Vegetation:
        return "Vegetation";
    case MapGeneratedFeatureKind::MountainRange:
        return "Mountain range";
    case MapGeneratedFeatureKind::Coastline:
        return "Coastline";
    case MapGeneratedFeatureKind::WaterBody:
        return "Water body";
    case MapGeneratedFeatureKind::Nation:
        return "Nation";
    case MapGeneratedFeatureKind::District:
        return "District";
    case MapGeneratedFeatureKind::BuildingGroup:
        return "Building group";
    case MapGeneratedFeatureKind::TacticalSetpiece:
        return "Tactical setpiece";
    }
    return "Generated feature";
}
} // namespace gm::map_creator_view_detail
