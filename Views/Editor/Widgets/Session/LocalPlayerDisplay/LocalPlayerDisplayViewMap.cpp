#include "Views/Editor/Widgets/Session/LocalPlayerDisplay/LocalPlayerDisplayViewSupport.hpp"
#include "Views/Editor/Widgets/Map/MapTokenLegend.hpp"

void LocalPlayerDisplayView::drawBlackoutSurface()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(origin,
                            ImVec2(origin.x + std::max(1.0f, available.x),
                                   origin.y + std::max(1.0f, available.y)),
                            IM_COL32(1, 1, 2, 255));
    ImGui::Dummy(ImVec2(std::max(1.0f, available.x), std::max(1.0f, available.y)));
}

void LocalPlayerDisplayView::drawScenePanel()
{
    const PlayerSafeStoryScene &scene = _controller._reviewCache.playerScene;
    if (!scene.available)
        return;

    ImGui::SeparatorText(localizedUiLabel("Scene").c_str());
    const std::string sceneTitle = scene.title.empty() ? localizedUiLabel("Current Scene") : scene.title;
    ImGui::TextWrapped(localizedUiLabel("%s").c_str(), sceneTitle.c_str());
    if (!scene.playerSummary.empty())
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), scene.playerSummary.c_str());
    if (!scene.readAloudText.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), scene.readAloudText.c_str());
    }
}

void LocalPlayerDisplayView::drawMapPanel(const MapModel *map, const MapRenderPlan *plan, bool mapOnly,
                                         bool fillAvailableHeight)
{
    if (!mapOnly)
        ImGui::SeparatorText(localizedUiLabel("Map").c_str());
    if (map == nullptr || plan == nullptr || plan->audience != MapRenderAudience::Player ||
        !mapRenderPlanMatchesMap(*plan, *map))
    {
        ImGui::TextDisabled(localizedUiLabel("No public map selected.").c_str());
        return;
    }

    // The controller owns this immutable reviewed projection and rebuilds it
    // only when the complete review-source fingerprint changes. Campaign
    // entities and raw map collections must never add unreviewed overlays.

    const auto backgroundAsset=_controller._publicationAssets.image(plan->backgroundInteractiveProjectRelativePath,
        plan->backgroundInteractiveContentHash);

    if (!mapOnly)
    {
        const std::string mapName = map->name.empty() ? localizedUiLabel("Map") : map->name;
        ImGui::TextWrapped(localizedUiLabel("%s").c_str(), mapName.c_str());
    }
    // The reviewed cache has already removed hidden tokens. Use only that
    // projection, and respect both token and name permissions for this key.
    gm::mapui::drawMapTokenLegend(*map, *plan);
    // Map descriptions predate the player-safety model and are commonly used
    // for GM preparation notes. Do not expose them until a dedicated public
    // description field exists.

    const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float availableHeight = mapOnly || fillAvailableHeight
                                      ? std::max(1.0f, ImGui::GetContentRegionAvail().y - 4.0f)
                                      : std::max(300.0f, ImGui::GetContentRegionAvail().y * 0.58f);
    // Fit inside the child padding and border, not the outer panel. Otherwise
    // a nominal 1x overview gains scrollbars and loses its outermost labels.
    const ImGuiStyle &style = ImGui::GetStyle();
    const float insetWidth = 2.0f * (style.WindowPadding.x + style.ChildBorderSize) + 1.0f;
    const float insetHeight = 2.0f * (style.WindowPadding.y + style.ChildBorderSize) + 1.0f;
    const float fittedCellSize = std::min(std::max(1.0f, availableWidth - insetWidth) / std::max(1, map->width),
                                         std::max(1.0f, availableHeight - insetHeight) / std::max(1, map->height));
    // Overview maps should open fitted instead of as a tiny viewport into a
    // huge horizontally scrolling canvas. Tactical maps retain readable cells
    // for tokens and measurements and may therefore scroll at large sizes.
    const float minimumCellSize = map->atlasLevel == MapLevel::Tactical ? 6.0f : 1.0f;
    const float cameraZoom = std::clamp(_controller._currentCampaign.localPlayerDisplay.publicCamera.zoom, 0.5f, 8.0f);
    const float cellSize = std::clamp(fittedCellSize * cameraZoom, minimumCellSize, 96.0f);
    const ImVec2 canvasSize(cellSize * map->width, cellSize * map->height);

    const float childHeight = mapOnly || fillAvailableHeight ? availableHeight :
        std::min(canvasSize.y + insetHeight, availableHeight);
    const bool canvasVisible=ImGui::BeginChild("PlayerDisplayMapCanvas", ImVec2(0.0f, childHeight), !mapOnly, ImGuiWindowFlags_HorizontalScrollbar);
    const LocalPlayerDisplayCameraState &camera = _controller._currentCampaign.localPlayerDisplay.publicCamera;
    const float visibleWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float visibleHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    const float scrollRangeX = std::max(0.0f, canvasSize.x - visibleWidth);
    const float scrollRangeY = std::max(0.0f, canvasSize.y - visibleHeight);
    ImGui::SetScrollX(std::clamp(camera.normalizedCenterX * canvasSize.x - visibleWidth * 0.5f, 0.0f, scrollRangeX));
    ImGui::SetScrollY(std::clamp(camera.normalizedCenterY * canvasSize.y - visibleHeight * 0.5f, 0.0f, scrollRangeY));
    // Letterbox fitted maps symmetrically. This also keeps a square encounter
    // centered on a wide table display; zoomed canvases retain the same scroll
    // and normalized-camera behavior as before.
    if(mapOnly)
    {
        const ImVec2 position=ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(position.x+std::max(0.f,(visibleWidth-canvasSize.x)*.5f),
                                  position.y+std::max(0.f,(visibleHeight-canvasSize.y)*.5f)));
    }
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    // Round road caps, foliage and edge labels may extend beyond their map
    // coordinates. Keep the fitted map's letterbox clean, and intersect with
    // the child clip so zoomed maps still respect the scrolling viewport.
    drawList->PushClipRect(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), true);
    const bool continuousRendering = map->generation.continuousRendering;
    const bool showGrid = plan->showGrid &&
                          (plan->layers & MapRenderPlanGrid) != 0;
    drawList->AddRectFilled(origin, ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y), IM_COL32(32, 36, 40, 255));
    const auto imageStatus=[&](const char *message){
        drawList->AddText(nullptr,0.f,ImVec2(origin.x+4,origin.y+4),ImGui::GetColorU32(ImGuiCol_Text),message,
            nullptr,std::max(1.f,canvasSize.x-8));
    };
    if(plan->backgroundVisible&&map->importedReference.enabled&&!backgroundAsset.valid()){
        // Retain the reserved canvas, but no fallback map or raw-path lookup.
        imageStatus(uiTextForLocale("The published map image is unavailable. The GM can repair it in Map Edit.",
            "Das veröffentlichte Kartenbild ist nicht verfügbar. Die SL kann es im Karteneditor reparieren."));
        drawList->PopClipRect();ImGui::Dummy(canvasSize);ImGui::EndChild();return;
    }
    bool importedBackgroundDrawn = false;
    const char *backgroundStatus=nullptr;
    if (canvasVisible&&plan->backgroundVisible && map->importedReference.enabled && !map->importedReference.gmOnly&&
        ImGui::IsRectVisible(origin,ImVec2(origin.x+canvasSize.x,origin.y+canvasSize.y)))
    {
        const CachedImageTexture &texture = getAsyncImageTexture(backgroundAsset,static_cast<std::uint64_t>(ImGui::GetFrameCount()));
        if (texture.loaded&&texture.textureId&&texture.width>0&&texture.height>0)
        {
            const MapNormalizedRect &crop = map->importedReference.crop;
            const float radians = -map->importedReference.rotationDegrees * 0.01745329251994329577f;
            const float cosine = std::cos(radians);
            const float sine = std::sin(radians);
            const auto uv = [&](float x, float y) {
                const float centeredX = x - 0.5f;
                const float centeredY = y - 0.5f;
                const float rotatedX = cosine * centeredX - sine * centeredY + 0.5f;
                const float rotatedY = sine * centeredX + cosine * centeredY + 0.5f;
                return ImVec2(crop.x + rotatedX * crop.width, crop.y + rotatedY * crop.height);
            };
            drawList->AddImageQuad(texture.textureId,
                                   origin, ImVec2(origin.x + canvasSize.x, origin.y),
                                   ImVec2(origin.x + canvasSize.x, origin.y + canvasSize.y),
                                   ImVec2(origin.x, origin.y + canvasSize.y),
                                   uv(0.0f, 0.0f), uv(1.0f, 0.0f), uv(1.0f, 1.0f), uv(0.0f, 1.0f));
            importedBackgroundDrawn = true;
        }
        else backgroundStatus=!texture.loadAttempted?uiTextForLocale("Loading shared map image…","Geteiltes Kartenbild wird geladen…"):
            uiTextForLocale("Shared map image is unavailable.","Das geteilte Kartenbild ist nicht verfügbar.");
    }
    const bool planDrivenRendering = continuousRendering || importedBackgroundDrawn ||
                                     !map->generation.graph.features.empty();
    if (planDrivenRendering)
        renderMap(drawList, origin, *map, *plan, cellSize, importedBackgroundDrawn);

    for (int y = 0; y < map->height; ++y)
    {
        for (int x = 0; x < map->width; ++x)
        {
            const auto visibleCellAt = [&](int cellX, int cellY) -> const MapCellModel * {
                const MapCellModel *candidate = mapCellAt(*map, cellX, cellY);
                return candidate && !candidate->fogged ? candidate : nullptr;
            };
            const ImVec2 min(origin.x + x * cellSize, origin.y + y * cellSize);
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            const MapCellModel *cell = mapCellAt(*map, x, y);
            if (cell && cell->fogged)
            {
                drawList->AddRectFilled(min, max, IM_COL32(24, 28, 32, 255));
                if (!planDrivenRendering && showGrid)
                    drawMapTileGrid(drawList, min, max, cellSize);
                continue;
            }

            if (planDrivenRendering)
                continue;

            drawMapCellResolvedSemanticLayers(
                drawList,
                min,
                max,
                cell,
                x,
                y,
                visibleCellAt(x, y - 1),
                visibleCellAt(x + 1, y),
                visibleCellAt(x, y + 1),
                visibleCellAt(x - 1, y),
                visibleCellAt(x + 1, y - 1),
                visibleCellAt(x + 1, y + 1),
                visibleCellAt(x - 1, y + 1),
                visibleCellAt(x - 1, y - 1),
                false,
                false,
                continuousRendering);
            if (showGrid)
                drawMapTileGrid(drawList, min, max, cellSize);
        }
    }

    if ((plan->layers & MapRenderPlanAnnotations) != 0)
    {
        for (const MapRegionModel &region : map->regions)
        {
            const ImVec2 minimum(origin.x + region.x * cellSize, origin.y + region.y * cellSize);
            drawList->AddRect(minimum,
                              ImVec2(minimum.x + region.width * cellSize, minimum.y + region.height * cellSize),
                              IM_COL32(94, 190, 169, 210), 1.0f);
        }
    }

    if ((plan->layers & MapRenderPlanRoutes) != 0)
    {
        for (const MapRouteModel &route : map->routes)
        {
            const gm::map_route_presentation::Path presentationPath =
                mapRoutePresentationForPlan(*map, route, plan);
            drawMapRoutePresentationPath(
                drawList, origin, cellSize, presentationPath,
                IM_COL32(235, 210, 83, 210),
                std::max(1.0f, cellSize * 0.10f));
        }
    }

    const std::vector<MapFeatureModel> &publicFeatures = map->features;
    if ((plan->layers & MapRenderPlanAnnotations) != 0)
    for (const MapFeatureModel &feature : publicFeatures)
    {
        const float centerX = origin.x + (feature.x + 0.5f) * cellSize;
        const float centerY = origin.y + (feature.y + 0.5f) * cellSize;
        const float radius = std::clamp(cellSize * 0.28f, 3.0f, 8.0f);
        const ImVec2 center(centerX, centerY);
        const ImU32 marker = IM_COL32(103, 211, 156, 245);
        drawList->AddQuadFilled(ImVec2(center.x, center.y - radius),
                                ImVec2(center.x + radius, center.y),
                                ImVec2(center.x, center.y + radius),
                                ImVec2(center.x - radius, center.y), marker);
        drawList->AddQuad(ImVec2(center.x, center.y - radius),
                          ImVec2(center.x + radius, center.y),
                          ImVec2(center.x, center.y + radius),
                          ImVec2(center.x - radius, center.y), IM_COL32(23, 31, 29, 245), 1.5f);
        if ((plan->layers & MapRenderPlanLabels) != 0 &&
            cellSize >= 11.0f && !feature.name.empty())
            drawList->AddText(ImVec2(center.x + radius + 3.0f,
                                     center.y - ImGui::GetTextLineHeight() * 0.5f),
                              IM_COL32(238, 241, 234, 245), feature.name.c_str());
    }

    const auto legendTokens = gm::mapui::orderedLegendTokens(*map);
    if ((plan->layers & MapRenderPlanTokens) != 0)
    for (std::size_t tokenIndex = 0; tokenIndex < legendTokens.size(); ++tokenIndex)
    {
        const MapTokenModel &token = *legendTokens[tokenIndex];
        const float centerX = origin.x + (token.x + 0.5f) * cellSize;
        const float centerY = origin.y + (token.y + 0.5f) * cellSize;
        drawList->AddCircleFilled(ImVec2(centerX, centerY), cellSize * 0.30f * std::max(0.5f, token.size), IM_COL32(92, 192, 235, 255), 24);
        drawList->AddCircle(ImVec2(centerX, centerY), cellSize * 0.33f * std::max(0.5f, token.size), IM_COL32(245, 250, 255, 255), 24, 2.0f);
        if ((plan->layers & MapRenderPlanLabels) != 0)
        {
            const std::string label = std::to_string(tokenIndex + 1);
            const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
            drawList->AddText(ImVec2(centerX - labelSize.x * 0.5f, centerY - labelSize.y * 0.5f),
                              IM_COL32(18, 22, 25, 255), label.c_str());
        }
    }

    // A final fail-closed fog layer sits above every reviewed map object,
    // including long routes and region outlines crossing unrevealed cells.
    if (plan->applyFog)
    {
        const auto fogTexture=getMapFogTexture(false).textureId;
        drawOpaqueMapFog(drawList,fogTexture,origin,cellSize,*map,0,0,map->width-1,map->height-1);
    }

    const LocalPlayerDisplayPingState &ping = _controller._currentCampaign.localPlayerDisplay.ping;
    if (ping.active)
    {
        const float mapX = std::min(ping.normalizedX, std::nextafter(1.0f, 0.0f)) * static_cast<float>(map->width);
        const float mapY = std::min(ping.normalizedY, std::nextafter(1.0f, 0.0f)) * static_cast<float>(map->height);
        if (playerSafeMapPositionIsRevealed(*map, mapX, mapY))
        {
            const ImVec2 center(origin.x + ping.normalizedX * canvasSize.x,
                                origin.y + ping.normalizedY * canvasSize.y);
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 7.0f);
            const float radius = std::clamp(cellSize * (0.45f + pulse * 0.18f), 10.0f, 34.0f);
            drawList->AddCircle(center, radius, IM_COL32(244, 190, 64, 255), 40, 4.0f);
            drawList->AddCircle(center, radius * 0.45f, IM_COL32(255, 239, 173, 235), 32, 2.0f);
        }
    }

    if(backgroundStatus)imageStatus(backgroundStatus);
    drawList->PopClipRect();
    ImGui::Dummy(canvasSize);
    ImGui::EndChild();

    if (mapOnly)
        return;

    if (!publicFeatures.empty())
        ImGui::TextDisabled("%s", localizedUiLabel("Diamond = revealed location").c_str());

    if (!publicFeatures.empty())
    {
        ImGui::TextColored(kMutedText, localizedUiLabel("Revealed Features").c_str());
        for (const MapFeatureModel &feature : publicFeatures)
        {
            const std::string featureName = feature.name.empty() ? localizedUiLabel(mapFeatureKindLabel(feature.kind)) : feature.name;
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped(localizedUiLabel("%s").c_str(), featureName.c_str());
            if (!feature.description.empty())
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), feature.description.c_str());
        }
    }
}
