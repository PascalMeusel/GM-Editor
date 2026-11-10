#pragma once

#include "Views/Editor/Widgets/Map/MapVisualVisibility.hpp"
#include "Views/Editor/Widgets/Map/MapTypographyFont.hpp"

inline void drawMapContinuousSymbols(ImDrawList *drawList, ImVec2 origin, float cellSize, const MapModel &map,
                                     bool playerSafe, bool showSymbols, bool showLabels,
                                     const MapGeneratedFeatureVisibilityFilter &featureFilter,
                                     std::string_view locale = {}, std::vector<ImVec4> *sharedLabelBounds = nullptr,
                                     const MapRenderPlan *presentation = nullptr)
{
    if (!map.generation.continuousRendering && map.generation.graph.features.empty())
        return;
    const auto labelCorridors=showLabels && map.generation.recipe.recipeVersion>=4
        ? mapPresentationCorridors(map,featureFilter,presentation) : std::vector<gm::map_geometry::MapRoadCorridor>{};
    const MapVisibleCellBounds visible = mapVisibleCellBounds(drawList, origin, cellSize, map);
    if (showSymbols)
    for (int y = visible.minY; y <= visible.maxY; ++y)
    {
        for (int x = visible.minX; x <= visible.maxX; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;
            if(gm::map_render::travelPurposeAt(map,x,y)) continue;
            if(gm::map_render::tacticalCoverInkAt(map,x,y)) continue;
                        if(cell->kind==MapTileKind::Hazard && gm::map_render::tacticalShrineAt(map,x,y)) continue;
                        if(cell->kind==MapTileKind::Cover && gm::map_render::tacticalWaystoneAt(map,x,y)) continue;
            if ((cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building ||
                 cell->kind == MapTileKind::Landmark || cell->kind == MapTileKind::Cover ||
                 cell->kind == MapTileKind::Rubble || cell->kind == MapTileKind::Hazard) &&
                !mapGeneratedPointSymbolVisibleForRender(map, x, y, playerSafe, featureFilter))
                continue;
            if (playerSafe &&
                (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building ||
                 cell->kind == MapTileKind::Landmark || cell->kind == MapTileKind::Cover ||
                 cell->kind == MapTileKind::Rubble || cell->kind == MapTileKind::Hazard) &&
                !mapPlayerSafeCellAreaIsRevealed(map,
                                                 static_cast<float>(x) - 0.5f,
                                                 static_cast<float>(y) - 0.5f,
                                                 static_cast<float>(x) + 1.5f,
                                                 static_cast<float>(y) + 1.5f))
                continue;
            const ImVec2 center(origin.x + (x + 0.5f) * cellSize, origin.y + (y + 0.5f) * cellSize);
            if (cell->kind == MapTileKind::Landmark && gm::map_render::settlementPoiAt(map,x,y)) continue;
                        if (cell->kind == MapTileKind::Settlement || cell->kind == MapTileKind::Building)
            {
                if (cell->kind == MapTileKind::Building && gm::map_render::supportsFootprintRoofInk(map)) continue;
                if (cell->kind == MapTileKind::Settlement && gm::map_render::usesSettlementSiteInk(map)) continue;
                const bool productionSnapshot = map.generation.recipe.styleVersion >= 3 &&
                                                mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId);
                if (productionSnapshot)
                {
                    const int anchorSalt = cell->kind == MapTileKind::Building ? 3203 : 3101;
                    if (!mapCellIsLocalStampAnchor(map, x, y, cell->kind, 1, anchorSalt))
                        continue;
                }
                if ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)))
                {
                    // Continental maps need intentional atlas symbols, not a
                    // tiny aerial painting with transparent padding. Buildings
                    // belong to Site/Tactical; settlements are semantic glyphs.
                    if (cell->kind == MapTileKind::Settlement && !(gm::map_render::usesIllustratedAtlasV3(map) || (gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map))))
                        drawMapOverviewSettlementGlyph(
                            drawList, center, cellSize,
                            mapOverviewSettlementSemanticRole(map, x, y));
                    continue;
                }
                if (productionSnapshot)
                {
                    const MapRenderScaleProfile &profile = mapRenderScaleProfile(map.atlasLevel);
                    if (cell->kind == MapTileKind::Building && !profile.allowIndividualBuildings)
                        continue;
                    const std::string role = ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) || map.atlasLevel == MapLevel::Region) ? "map_symbol" : "settlement_stamp";
                    const std::string semantic = role == "map_symbol"
                        ? (cell->kind == MapTileKind::Settlement ? mapOverviewSettlementSemanticRole(map, x, y) : "town")
                        : mapSettlementVisualSemanticRole(map, cell->kind);
                    float extent = cellSize * (cell->kind == MapTileKind::Settlement
                                                    ? profile.settlementStampCells
                                                    : profile.buildingStampCells);
                    if ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) && cell->kind == MapTileKind::Settlement)
                    {
                        // Capitals and major cities are the primary anchors at
                        // continental scale. Give them a restrained size step
                        // rather than surrounding every icon with a UI badge.
                        if (semantic == "capital")
                            extent *= 1.22f;
                        else if (semantic == "city")
                            extent *= 1.10f;
                    }
                    const bool drawn = cell->kind == MapTileKind::Building
                        ? drawProductionAssetRoleRotated(drawList, center, extent, extent,
                                                        mapBuildingStreetRotation(map, x, y), map,
                                                        role, semantic, x, y, static_cast<int>(cell->kind),
                                                        IM_COL32_WHITE, mapProductionAssetZoom(cellSize))
                        : drawProductionAssetRole(drawList,
                                                  ImVec2(center.x - extent * 0.5f, center.y - extent * 0.5f),
                                                  ImVec2(center.x + extent * 0.5f, center.y + extent * 0.5f), map,
                                                  role, semantic, x, y, static_cast<int>(cell->kind),
                                                  (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map))
                                                      ? IM_COL32(244, 235, 210, 230)
                                                      : IM_COL32_WHITE,
                                                  mapProductionAssetZoom(cellSize));
                    if (drawn)
                    {
                        if ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) &&
                            (semantic == "capital" || semantic == "city"))
                        {
                            const float radius = cellSize * (semantic == "capital" ? 1.04f : 0.91f);
                            drawList->AddCircle(center, radius, IM_COL32(230, 198, 118, 215),
                                                28, std::max(1.25f, cellSize * 0.075f));
                            if (semantic == "capital")
                                drawList->AddCircle(center, radius + cellSize * 0.16f,
                                                    IM_COL32(68, 51, 31, 195), 28,
                                                    std::max(1.0f, cellSize * 0.055f));
                        }
                        continue;
                    }
                }
                const float scale = cell->kind == MapTileKind::Settlement ? 1.0f : 0.72f;
                const ImVec2 min(center.x - cellSize * 0.25f * scale, center.y - cellSize * 0.08f * scale);
                const ImVec2 max(center.x + cellSize * 0.25f * scale, center.y + cellSize * 0.25f * scale);
                drawList->AddRectFilled(min, max, IM_COL32(210, 187, 137, 255), 1.5f);
                drawList->AddTriangleFilled(ImVec2(center.x, center.y - cellSize * 0.34f * scale),
                                            ImVec2(center.x - cellSize * 0.33f * scale, center.y - cellSize * 0.04f * scale),
                                            ImVec2(center.x + cellSize * 0.33f * scale, center.y - cellSize * 0.04f * scale), IM_COL32(113, 65, 42, 255));
                drawList->AddRectFilled(ImVec2(center.x - cellSize * 0.05f, center.y + cellSize * 0.08f), ImVec2(center.x + cellSize * 0.05f, center.y + cellSize * 0.25f * scale), IM_COL32(87, 58, 39, 255));
            }
            else if (cell->kind == MapTileKind::Landmark)
            {
                if ((gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map))) continue;
                if ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)))
                {
                    if (!mapWorldPointIsLand(map, static_cast<float>(x) + 0.5f,
                                             static_cast<float>(y) + 0.5f))
                        continue;
                    // The legacy generic "discovery" artwork is a close-up
                    // planted bridge. At continental scale that appeared as a
                    // random bush/tree prop. Use a compact cartographic mark;
                    // the adjacent label carries the landmark's meaning.
                    const float radius = std::max(3.0f, cellSize * 0.30f);
                    drawList->AddQuadFilled(
                        ImVec2(center.x, center.y - radius),
                        ImVec2(center.x + radius, center.y),
                        ImVec2(center.x, center.y + radius),
                        ImVec2(center.x - radius, center.y),
                        IM_COL32(222, 174, 76, 245));
                    drawList->AddQuad(
                        ImVec2(center.x, center.y - radius),
                        ImVec2(center.x + radius, center.y),
                        ImVec2(center.x, center.y + radius),
                        ImVec2(center.x - radius, center.y),
                        IM_COL32(62, 43, 24, 235), 1.5f);
                    continue;
                }
                if (drawProductionAssetRole(drawList, ImVec2(center.x - cellSize * 0.7f, center.y - cellSize * 0.7f),
                                            ImVec2(center.x + cellSize * 0.7f, center.y + cellSize * 0.7f), map,
                                            "map_symbol", mapOverviewLandmarkSemanticRole(map, x, y), x, y, 1701,
                                            IM_COL32_WHITE, mapProductionAssetZoom(cellSize)))
                    continue;
                drawList->AddCircleFilled(center, cellSize * 0.28f, IM_COL32(210, 169, 71, 255), 18);
                drawList->AddCircle(center, cellSize * 0.33f, IM_COL32(78, 53, 31, 245), 18, 2.0f);
                drawList->AddText(ImVec2(center.x - 4.0f, center.y - 8.0f), IM_COL32(43, 34, 24, 255), "*");
            }
            else if (map.atlasLevel == MapLevel::Tactical && !gm::map_render::usesNaturalCaveInk(map) && !gm::map_render::usesDungeonMasonryInk(map) &&
                     (cell->kind == MapTileKind::Cover || cell->kind == MapTileKind::Rubble || cell->kind == MapTileKind::Hazard))
            {
                const char *semantic = cell->kind == MapTileKind::Rubble ? "rubble" : (cell->kind == MapTileKind::Hazard ? "barricade" : "well_log_cover");
                drawProductionAssetRole(drawList, ImVec2(center.x - cellSize * 0.725f, center.y - cellSize * 0.725f),
                                        ImVec2(center.x + cellSize * 0.725f, center.y + cellSize * 0.725f), map,
                                        "tactical_prop", semantic, x, y, 1801,
                                        IM_COL32_WHITE, mapProductionAssetZoom(cellSize));
            }
        }
    }

    const MapRenderScaleProfile &scaleProfile = mapRenderScaleProfile(map.atlasLevel);
    if (!showLabels || !scaleProfile.showEntityLabels || cellSize < 5.0f)
        return;

    struct MapLabelObstacle
    {
        const MapGeneratedFeature *feature = nullptr;
        ImVec4 bounds;
    };
    std::vector<const MapGeneratedFeature *> labelFeatures;
    std::vector<MapLabelObstacle> settlementObstacles;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        const bool settlement = feature.kind == MapGeneratedFeatureKind::Settlement;
        const bool landmark = feature.kind == MapGeneratedFeatureKind::Landmark;
        if ((!settlement && !landmark) || feature.points.empty() ||
            !mapGeneratedFeatureVisibleForRender(feature, playerSafe, featureFilter))
            continue;
        const bool preciseSymbol=map.generation.recipe.recipeVersion>=5 && gm::map_render::usesIllustratedAtlas(map);
        if(preciseSymbol) {
            const auto anchor=feature.points.front();
            const auto* cell=mapCellAt(map,static_cast<int>(std::floor(anchor.x)),static_cast<int>(std::floor(anchor.y)));
            if(cell && (!playerSafe || !cell->fogged)) {
                const auto box=gm::map_render::illustratedAtlasSymbolLabelBounds(anchor,settlement);
                settlementObstacles.push_back({&feature,ImVec4(origin.x+box[0]*cellSize,origin.y+box[1]*cellSize,
                    origin.x+box[2]*cellSize,origin.y+box[3]*cellSize)});
            }
        }
        if(feature.name.empty() || !mapIsSemanticCartographicName(feature.name))continue;
        labelFeatures.push_back(&feature);
        if (!preciseSymbol && (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) && settlement)
        {
            const ImVec2 center(origin.x + feature.points.front().x * cellSize,
                                origin.y + feature.points.front().y * cellSize);
            const gm::map_render::OverviewSettlementGlyphStyle glyphStyle =
                gm::map_render::overviewSettlementGlyphStyle(
                    gm::map_render::overviewSettlementGlyphKind(
                        mapOverviewSettlementSemanticRole(feature)));
            const float halfExtent = cellSize *
                                     (glyphStyle.outerRadiusCells + 0.16f);
            settlementObstacles.push_back({&feature,
                ImVec4(center.x - halfExtent, center.y - halfExtent,
                       center.x + halfExtent, center.y + halfExtent)});
        }
    }
    std::stable_sort(labelFeatures.begin(), labelFeatures.end(),
                     [](const MapGeneratedFeature *left, const MapGeneratedFeature *right) {
        const int leftPriority = left->kind == MapGeneratedFeatureKind::Settlement
                                     ? (std::string_view(mapOverviewSettlementSemanticRole(*left)) == "capital" ? 3 : 2)
                                     : 1;
        const int rightPriority = right->kind == MapGeneratedFeatureKind::Settlement
                                      ? (std::string_view(mapOverviewSettlementSemanticRole(*right)) == "capital" ? 3 : 2)
                                      : 1;
        return leftPriority > rightPriority;
    });

    std::vector<ImVec4> localLabelBounds;
    auto &occupiedLabels = sharedLabelBounds ? *sharedLabelBounds : localLabelBounds;
    int labelsDrawn = 0;
    for (const MapGeneratedFeature *featurePointer : labelFeatures)
    {
        const MapGeneratedFeature &feature = *featurePointer;
        const bool settlement = feature.kind == MapGeneratedFeatureKind::Settlement;
        const bool landmark = feature.kind == MapGeneratedFeatureKind::Landmark;
        if (landmark && (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) &&
            !mapWorldPointIsLand(map, feature.points.front().x, feature.points.front().y))
            continue;
        if (landmark && map.atlasLevel == MapLevel::Site && cellSize < 16.0f)
            continue;
        const ImVec2 anchor(origin.x + feature.points.front().x * cellSize,
                            origin.y + feature.points.front().y * cellSize);
        if (anchor.x < drawList->GetClipRectMin().x - 100.0f || anchor.y < drawList->GetClipRectMin().y - 24.0f ||
            anchor.x > drawList->GetClipRectMax().x + 100.0f || anchor.y > drawList->GetClipRectMax().y + 24.0f)
            continue;
        const std::string label = localizedGeneratedMapName(feature.name, locale);
        const float fontSize = std::clamp(
            std::max(ImGui::GetFontSize() * (settlement ? 1.08f : 0.90f),
                     cellSize * (settlement ? 0.90f : 0.70f)),
            (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) ? (settlement ? 14.0f : 12.0f) : 10.0f,
            settlement ? 23.0f : 19.0f);
        ImFont *labelFont=mapLabelFont(map);
        const ImVec2 textSize = labelFont->CalcTextSizeA(fontSize, 10000.0f, 0.0f, label.c_str());
        float clearanceCells = settlement ? 1.62f : 1.05f;
        if (settlement && (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)))
        {
            const gm::map_render::OverviewSettlementGlyphStyle glyphStyle =
                gm::map_render::overviewSettlementGlyphStyle(
                    gm::map_render::overviewSettlementGlyphKind(
                        mapOverviewSettlementSemanticRole(feature)));
            clearanceCells = glyphStyle.outerRadiusCells + 0.42f;
        }
        if (settlement && (gm::map_render::usesIllustratedAtlasV4(map) || gm::map_render::usesIllustratedAtlasV5(map)))
            clearanceCells=std::max(clearanceCells,gm::map_render::usesIllustratedAtlasV5(map)?1.60f:1.30f);
        const float clearance = cellSize * clearanceCells;
        std::vector<ImVec2> candidates = {
            {anchor.x - textSize.x * 0.5f, anchor.y - clearance - textSize.y},
            {anchor.x - textSize.x * 0.5f, anchor.y + clearance},
            {anchor.x + clearance, anchor.y - textSize.y * 0.5f},
            {anchor.x - clearance - textSize.x, anchor.y - textSize.y * 0.5f},
            {anchor.x + clearance, anchor.y - clearance - textSize.y},
            {anchor.x - clearance - textSize.x, anchor.y + clearance},
            {anchor.x + clearance, anchor.y + clearance},
            {anchor.x - clearance - textSize.x, anchor.y - clearance - textSize.y}};
        if(map.generation.recipe.recipeVersion>=5) {
            const auto nearby=candidates;
            for(const auto p:nearby) {
                const float dx=p.x+textSize.x*.5f-anchor.x,dy=p.y+textSize.y*.5f-anchor.y;
                const float length=std::max(.001f,std::hypot(dx,dy));
                candidates.push_back({p.x+cellSize*dx/length,p.y+cellSize*dy/length});
            }
        }
        bool placed = false;
        ImVec2 textPosition{};
        ImVec4 bounds{};
        float selectedTerrainCost = std::numeric_limits<float>::max();
        for (ImVec2 candidate : candidates)
        {
            if (map.generation.recipe.generatorVersion >= 11)
            {
                const ImVec2 lower(std::max(origin.x,drawList->GetClipRectMin().x)+5.f,
                                   std::max(origin.y,drawList->GetClipRectMin().y)+3.f);
                const ImVec2 upper(std::min(origin.x+map.width*cellSize,drawList->GetClipRectMax().x)-textSize.x-5.f,
                                   std::min(origin.y+map.height*cellSize,drawList->GetClipRectMax().y)-textSize.y-3.f);
                if(upper.x<lower.x || upper.y<lower.y)continue;
                candidate.x=std::clamp(candidate.x,lower.x,upper.x);
                candidate.y=std::clamp(candidate.y,lower.y,upper.y);
            }
            else
            {
            candidate.x = std::clamp(candidate.x, drawList->GetClipRectMin().x + 3.0f,
                                     std::max(drawList->GetClipRectMin().x + 3.0f,
                                              drawList->GetClipRectMax().x - textSize.x - 3.0f));
            candidate.y = std::clamp(candidate.y, drawList->GetClipRectMin().y + 3.0f,
                                     std::max(drawList->GetClipRectMin().y + 3.0f,
                                              drawList->GetClipRectMax().y - textSize.y - 3.0f));
            }
            const ImVec4 candidateBounds(candidate.x - 5.0f, candidate.y - 3.0f,
                                         candidate.x + textSize.x + 5.0f,
                                         candidate.y + textSize.y + 3.0f);
            if(!gm::map_render::overviewLabelBoxFitsMap(map,
                (candidateBounds.x-origin.x)/cellSize,(candidateBounds.y-origin.y)/cellSize,
                (candidateBounds.z-origin.x)/cellSize,(candidateBounds.w-origin.y)/cellSize))continue;
            if (playerSafe &&
                !mapPlayerSafeCellAreaIsRevealed(
                    map,
                    (candidateBounds.x - origin.x) / cellSize,
                    (candidateBounds.y - origin.y) / cellSize,
                    (candidateBounds.z - origin.x) / cellSize,
                    (candidateBounds.w - origin.y) / cellSize))
                continue;
            const auto overlapsBounds = [&](const ImVec4 &other, float margin) {
                return candidateBounds.x < other.z + margin && candidateBounds.z + margin > other.x &&
                       candidateBounds.y < other.w + margin && candidateBounds.w + margin > other.y;
            };
            const bool labelOverlap = std::any_of(
                occupiedLabels.begin(), occupiedLabels.end(),
                [&](const ImVec4 &other) { return overlapsBounds(other, 4.0f); });
            const bool symbolOverlap = std::any_of(
                settlementObstacles.begin(), settlementObstacles.end(),
                [&](const MapLabelObstacle &obstacle) {
                    return obstacle.feature != &feature && overlapsBounds(obstacle.bounds, 3.0f);
                });
            if (labelOverlap || symbolOverlap)
                continue;
            if (gm::map_render::usesIllustratedAtlasV5(map) &&
                !gm::map_render::illustratedAtlasLabelClearsOwnSymbol(feature.points.front(),
                    (candidateBounds.x-origin.x)/cellSize,(candidateBounds.y-origin.y)/cellSize,
                    (candidateBounds.z-origin.x)/cellSize,(candidateBounds.w-origin.y)/cellSize,settlement)) continue;
            const float terrainCost = gm::map_render::usesIllustratedAtlasV5(map)
                ? gm::map_render::illustratedAtlasLabelPlacementCost(map,feature.points.front(),
                    (candidateBounds.x-origin.x)/cellSize,(candidateBounds.y-origin.y)/cellSize,
                    (candidateBounds.z-origin.x)/cellSize,(candidateBounds.w-origin.y)/cellSize,&labelCorridors,
                    presentation?&presentation->labelInkMask:nullptr) : 0.f;
            if (terrainCost < selectedTerrainCost)
            { textPosition=candidate; bounds=candidateBounds; placed=true; selectedTerrainCost=terrainCost; }
            if (terrainCost <= 0.f) break;
        }
        if (!placed)
            continue;
        if (gm::map_render::usesIllustratedAtlasV5(map))
        {
            const auto leader=gm::map_render::illustratedAtlasLabelLeader(feature.points.front(),
                (bounds.x-origin.x)/cellSize,(bounds.y-origin.y)/cellSize,
                (bounds.z-origin.x)/cellSize,(bounds.w-origin.y)/cellSize);
            if (leader.size()==2)
                drawList->AddLine(ImVec2(origin.x+leader[0].x*cellSize,origin.y+leader[0].y*cellSize),
                    ImVec2(origin.x+leader[1].x*cellSize,origin.y+leader[1].y*cellSize),IM_COL32(104,94,67,150),1.f);
        }
        occupiedLabels.push_back(bounds);
        drawMapTextWithHalo(drawList, labelFont, fontSize, textPosition,
                            (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) ? IM_COL32(48, 48, 35, 255) :
                                (settlement ? IM_COL32(250, 240, 216, 255) : IM_COL32(231, 229, 207, 245)),
                            (map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) ? (map.atlasLevel == MapLevel::Region && map.generation.recipe.styleVersion == 6 ? IM_COL32(235, 230, 193, 250) : IM_COL32(224, 221, 178, 190)) : IM_COL32(31, 28, 22, 220),
                            std::clamp(fontSize * 0.105f, 1.15f, 1.75f), label.c_str());
        if (++labelsDrawn >= ((map.atlasLevel == MapLevel::World || gm::map_render::usesIllustratedAtlas(map)) ? 18 : 10))
            break;
    }
}
