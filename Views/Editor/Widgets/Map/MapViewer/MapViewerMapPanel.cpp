#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewDetails.hpp"
#include "Views/Editor/Widgets/Map/MapViewer/MapViewerViewInternal.hpp"

using namespace gm::map_viewer_detail;

void MapViewerView::drawMapSidePanel(MapModel &map)
{
    const PartyTravelPlanModel &travel = _controller._currentCampaign.partyTravel;
    const bool travelRelevant =
        !map.routes.empty() ||
        travel.currentMapId == map.metadata.id ||
        _controller._currentCampaign.partyLocation.mapId == map.metadata.id;

    const bool creatingDetail = _controller._interactionMode == MapViewerInteractionMode::SelectDetailArea;
    drawAtlasSidePanel(map);
    if (creatingDetail)
        return;

    if (beginContextTreeNode("Inspect", true, "Click a cell to see the terrain, overlays, tokens, and map art attached to that cell."))
    {
        if (isMapCellInside(map, _controller._selectedCellX, _controller._selectedCellY))
        {
            const MapCellModel *cell = mapCellAt(map, _controller._selectedCellX, _controller._selectedCellY);
            ImGui::Text(localizedUiLabel("Cell: %s").c_str(), coordinateLabel(_controller._selectedCellX, _controller._selectedCellY).c_str());
            const std::string terrainLabel = localizedUiLabel(cell ? mapTileKindLabel(cell->kind) : "Empty");
            ImGui::Text(localizedUiLabel("Terrain: %s").c_str(), terrainLabel.c_str());
            if (cell)
            {
                ImGui::Text(localizedUiLabel("Move Cost: %d").c_str(), cell->movementCost);
                ImGui::Text(localizedUiLabel("Elevation: %+d").c_str(), cell->elevation);
                if (ImGui::TreeNode(uiTextForLocale("Movement and sight edges", "Bewegungs- und Sichtkanten")))
                {
                    const std::string blockedMoveEdges = localizedEdgeMaskLabel(cell->movementBlockedMask);
                    const std::string blockedSightEdges = localizedEdgeMaskLabel(cell->lineOfSightBlockedMask);
                    const std::string oneWayMoveEdges = localizedEdgeMaskLabel(cell->movementOneWayBlockedMask);
                    const std::string oneWaySightEdges = localizedEdgeMaskLabel(cell->lineOfSightOneWayBlockedMask);
                    ImGui::TextWrapped(localizedUiLabel("Blocked Move Edges: %s").c_str(), blockedMoveEdges.c_str());
                    ImGui::TextWrapped(localizedUiLabel("Blocked Sight Edges: %s").c_str(), blockedSightEdges.c_str());
                    ImGui::TextWrapped(localizedUiLabel("One-way Move Edges: %s").c_str(), oneWayMoveEdges.c_str());
                    ImGui::TextWrapped(localizedUiLabel("One-way Sight Edges: %s").c_str(), oneWaySightEdges.c_str());
                    ImGui::TreePop();
                }
                const std::string fogLabel = localizedUiLabel(cell->fogged ? "Set" : "Clear");
                ImGui::Text(localizedUiLabel("Fog: %s").c_str(), fogLabel.c_str());
                if (cell->difficultTerrain)
                    ImGui::TextColored(ImVec4(0.92f, 0.74f, 0.42f, 1.0f), localizedUiLabel("Difficult terrain").c_str());
                if (cell->blocksLineOfSight)
                    ImGui::TextColored(ImVec4(0.92f, 0.42f, 0.38f, 1.0f), localizedUiLabel("Blocks line of sight").c_str());
            }

            bool foundOverlay = false;
            for (const MapRegionModel &region : map.regions)
            {
                const bool insideRegion =
                    _controller._selectedCellX >= region.x &&
                    _controller._selectedCellY >= region.y &&
                    _controller._selectedCellX < region.x + region.width &&
                    _controller._selectedCellY < region.y + region.height;
                if (insideRegion)
                {
                    foundOverlay = true;
                    const std::string regionName = region.name.empty() ? localizedUiLabel("<Unnamed>") : localizedGeneratedMapName(region.name);
                    const std::string gmSuffix = region.gmOnly ? localizedUiLabel(" (GM)") : std::string{};
                    ImGui::BulletText(localizedUiLabel("Region: %s%s").c_str(), regionName.c_str(), gmSuffix.c_str());
                }
            }
            for (const MapFeatureModel &feature : map.features)
            {
                if (feature.x == _controller._selectedCellX && feature.y == _controller._selectedCellY)
                {
                    foundOverlay = true;
                    const std::string featureName = feature.name.empty() ? localizedUiLabel(mapFeatureKindLabel(feature.kind)) : feature.name;
                    ImGui::BulletText(localizedUiLabel("Feature: %s").c_str(), featureName.c_str());
                }
            }
            for (MapGeneratedFeature &feature : map.generation.graph.features)
            {
                // Point landmarks use the same cell as their illustrated icon.
                // Long rivers and area polygons are not selected by their first vertex.
                if ((feature.kind != MapGeneratedFeatureKind::Settlement &&
                     feature.kind != MapGeneratedFeatureKind::Landmark) || feature.points.empty() ||
                    static_cast<int>(std::floor(feature.points.front().x)) != _controller._selectedCellX ||
                    static_cast<int>(std::floor(feature.points.front().y)) != _controller._selectedCellY)
                    continue;
                foundOverlay = true;
                ImGui::PushID(feature.id.c_str());
                ImGui::Separator();
                const std::string name = feature.name.empty() ? localizedUiLabel("<Unnamed>") : feature.name;
                ImGui::TextWrapped("%s", name.c_str());
                bool shared = feature.discovered && !feature.gmOnly;
                if (FormUi::Checkbox(uiTextForLocale("Share this place with players", "Diesen Ort mit Spielern teilen"), &shared))
                {
                    feature.discovered = shared;
                    feature.gmOnly = !shared;
                    _controller.saveCampaign();
                }
                ImGui::TextWrapped("%s", uiTextForLocale(
                    "Also enable Places and Names in the player display, then review before showing the map.",
                    "Aktiviere auch Orte und Namen in der Spieleransicht und prüfe die Vorschau vor dem Anzeigen."));
                ImGui::PopID();
            }
            for (const MapTokenModel &token : map.tokens)
            {
                if (static_cast<int>(token.x) == _controller._selectedCellX && static_cast<int>(token.y) == _controller._selectedCellY)
                {
                    foundOverlay = true;
                    const std::string tokenName = token.name.empty() ? localizedUiLabel(mapTokenKindLabel(token.kind)) : token.name;
                    ImGui::BulletText(localizedUiLabel("Token: %s").c_str(), tokenName.c_str());
                }
            }
            if (!foundOverlay)
                drawDisabledWrapped(localizedUiLabel("No region, feature, or token on this cell."));
        }
        else
        {
            ImGui::TextWrapped(localizedUiLabel("Click a map cell to inspect terrain, fog, regions, features, and tokens.").c_str());
        }
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(_openTravelWorkflowRequested || travelRelevant,
                          _openTravelWorkflowRequested ? ImGuiCond_Always : ImGuiCond_Once);
    if (ImGui::TreeNodeEx(localizedUiText("Travel")))
    {
        if (_openTravelWorkflowRequested) { ImGui::SetScrollHereY(0.1f); _openTravelWorkflowRequested = false; }
        const bool acceptanceAttempted = drawTravelWorkflowPanel(map);
        ImGui::TreePop();
        // Acceptance and rollback may replace campaign storage. Do not use
        // the previous map reference again in this frame.
        if (acceptanceAttempted) return;
    }

    if (beginAdvancedTreeNode("Fog & Layers / Advanced", "Batch fog and layer controls affect the full map, so they stay behind an advanced drawer."))
    {
        if (drawUiButton("Reveal All Fog", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup(localizedUiLabel("Reveal All Fog").c_str());
        if (drawUiButton("Set All Fog", UiButtonRole::Secondary, ImVec2(-1.0f, 0.0f)))
            ImGui::OpenPopup(localizedUiLabel("Set All Fog").c_str());

        if (ImGui::BeginPopupModal(localizedUiLabel("Reveal All Fog").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(localizedUiLabel("Reveal every fogged cell on this map?").c_str());
            drawInlineLabelValue("Map", map.name.empty() ? localizedUiText("Untitled map") : map.name.c_str());
            if (drawModalActionButtons("Reveal All", UiButtonRole::Advanced))
                _controller.revealAllFogOnSelectedMap();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal(localizedUiLabel("Set All Fog").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(localizedUiLabel("Set fog on every cell on this map?").c_str());
            drawInlineLabelValue("Map", map.name.empty() ? localizedUiText("Untitled map") : map.name.c_str());
            if (drawModalActionButtons("Set Fog", UiButtonRole::Advanced))
                _controller.clearAllFogOnSelectedMap();
            ImGui::EndPopup();
        }

        for (const MapLayerModel &layer : map.layers)
        {
            const std::string layerName = layer.name.empty() ? localizedUiLabel("Layer") : layer.name;
            const std::string hiddenSuffix = layer.visible ? std::string{} : localizedUiLabel(" (hidden)");
            ImGui::BulletText("%s%s", layerName.c_str(), hiddenSuffix.c_str());
        }
        ImGui::TreePop();
    }

    if (beginContextTreeNode("Tokens & Notes", !map.tokens.empty() || !map.features.empty(), "Open when auditing every token and feature on this map instead of inspecting one cell."))
    {
        for (const MapTokenModel &token : map.tokens)
        {
            const std::string tokenName = token.name.empty() ? localizedUiLabel("Token") : token.name;
            const std::string tokenKind = localizedUiLabel(mapTokenKindLabel(token.kind));
            const std::string hiddenSuffix = token.hiddenFromPlayers ? localizedUiLabel(" hidden") : std::string{};
            ImGui::BulletText("%s (%s) %.1f, %.1f%s", tokenName.c_str(), tokenKind.c_str(), token.x, token.y, hiddenSuffix.c_str());
        }
        for (const MapFeatureModel &feature : map.features)
        {
            ImGui::Separator();
            const std::string featureKind = localizedUiLabel(mapFeatureKindLabel(feature.kind));
            const std::string featureName = feature.name.empty() ? localizedUiLabel("<Unnamed>") : feature.name;
            ImGui::Text(localizedUiLabel("%s: %s").c_str(), featureKind.c_str(), featureName.c_str());
            if (!feature.description.empty())
                ImGui::TextWrapped(localizedUiLabel("%s").c_str(), feature.description.c_str());
            if (!feature.gmNotes.empty())
                ImGui::TextColored(ImVec4(0.92f, 0.74f, 0.42f, 1.0f), localizedUiLabel("GM: %s").c_str(), feature.gmNotes.c_str());
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode(localizedUiLabel("Legend").c_str()))
    {
        renderTileLegend(paletteForMapScope(map.scope), "No legend entries for this map type.");
        ImGui::TreePop();
    }
}
