#pragma once

#include "Views/Editor/Widgets/Map/MapVisualTerrain.hpp"

using MapGeneratedFeatureVisibilityFilter = std::function<bool(const MapGeneratedFeature &)>;

inline bool mapGeneratedFeatureVisibleForRender(const MapGeneratedFeature &feature,
                                                bool playerSafe,
                                                const MapGeneratedFeatureVisibilityFilter &filter)
{
    if (!playerSafe)
        return true;
    if (feature.gmOnly || !feature.discovered)
        return false;
    return !filter || filter(feature);
}

inline bool mapGeneratedPointSymbolVisibleForRender(const MapModel &map,
                                                    int x,
                                                    int y,
                                                    bool playerSafe,
                                                    const MapGeneratedFeatureVisibilityFilter &filter)
{
    if (!playerSafe)
        return true;
    const MapCellModel *anchorCell = mapCellAt(map, x, y);
    if (anchorCell == nullptr || anchorCell->fogged)
        return false;
    bool matched = false;
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if ((feature.kind != MapGeneratedFeatureKind::Settlement &&
             feature.kind != MapGeneratedFeatureKind::Landmark &&
             feature.kind != MapGeneratedFeatureKind::BuildingGroup &&
             feature.kind != MapGeneratedFeatureKind::TacticalSetpiece) ||
            feature.points.empty())
            continue;
        const int featureX = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, std::max(0, map.width - 1));
        const int featureY = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, std::max(0, map.height - 1));
        if (featureX != x || featureY != y)
            continue;
        matched = true;
        if (mapGeneratedFeatureVisibleForRender(feature, true, filter))
            return true;
    }
    // Imported and hand-painted semantic cells have no authoritative feature
    // graph and retain their historic raster symbols. On generated maps an
    // unmatched cell can be the stale projection of a feature removed by the
    // reviewed plan, so it must fail closed.
    return !matched && !map.generation.enabled;
}

inline bool mapPlayerSafeCellAreaIsRevealed(const MapModel &map,
                                            float minimumX,
                                            float minimumY,
                                            float maximumX,
                                            float maximumY)
{
    if (map.width <= 0 || map.height <= 0 ||
        !std::isfinite(minimumX) || !std::isfinite(minimumY) ||
        !std::isfinite(maximumX) || !std::isfinite(maximumY) ||
        maximumX <= minimumX || maximumY <= minimumY)
        return false;
    const int firstX = std::clamp(static_cast<int>(std::floor(minimumX)), 0, map.width - 1);
    const int firstY = std::clamp(static_cast<int>(std::floor(minimumY)), 0, map.height - 1);
    const int lastX = std::clamp(static_cast<int>(std::floor(std::nextafter(maximumX, minimumX))), 0, map.width - 1);
    const int lastY = std::clamp(static_cast<int>(std::floor(std::nextafter(maximumY, minimumY))), 0, map.height - 1);
    for (int y = firstY; y <= lastY; ++y)
        for (int x = firstX; x <= lastX; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (cell == nullptr || cell->fogged)
                return false;
        }
    return true;
}

inline const MapGeneratedFeature *mapGeneratedSettlementAt(const MapModel &map, int x, int y)
{
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::Settlement || feature.points.empty())
            continue;
        const int featureX = std::clamp(static_cast<int>(std::floor(feature.points.front().x)), 0, std::max(0, map.width - 1));
        const int featureY = std::clamp(static_cast<int>(std::floor(feature.points.front().y)), 0, std::max(0, map.height - 1));
        if (featureX == x && featureY == y)
            return &feature;
    }
    return nullptr;
}

inline const char *mapOverviewSettlementSemanticRole(const MapGeneratedFeature &feature)
{
    if (feature.id == "brief_capital" || feature.materialId.find("capital") != std::string::npos)
        return "capital";
    if (feature.id.rfind("brief_city_", 0) == 0 || feature.locked ||
        feature.materialId.find("city") != std::string::npos)
        return "city";
    if (feature.materialId.find("village") != std::string::npos)
        return "village";
    return "town";
}

inline const char *mapOverviewSettlementSemanticRole(const MapModel &map, int x, int y)
{
    const MapGeneratedFeature *feature = mapGeneratedSettlementAt(map, x, y);
    return feature ? mapOverviewSettlementSemanticRole(*feature) : "town";
}

inline void drawMapOverviewSettlementGlyph(ImDrawList *drawList,
                                           ImVec2 center,
                                           float cellSize,
                                           std::string_view semanticRole)
{
    if (!drawList || cellSize <= 0.0f)
        return;
    const gm::map_render::OverviewSettlementGlyphKind kind =
        gm::map_render::overviewSettlementGlyphKind(semanticRole);
    const gm::map_render::OverviewSettlementGlyphStyle style =
        gm::map_render::overviewSettlementGlyphStyle(kind);
    const ImU32 ink = IM_COL32(
        gm::map_render::kOverviewSettlementInk.r,
        gm::map_render::kOverviewSettlementInk.g,
        gm::map_render::kOverviewSettlementInk.b,
        gm::map_render::kOverviewSettlementInk.a);
    const ImU32 paper = IM_COL32(
        gm::map_render::kOverviewSettlementPaper.r,
        gm::map_render::kOverviewSettlementPaper.g,
        gm::map_render::kOverviewSettlementPaper.b,
        gm::map_render::kOverviewSettlementPaper.a);
    const ImU32 accent = IM_COL32(
        gm::map_render::kOverviewSettlementAccent.r,
        gm::map_render::kOverviewSettlementAccent.g,
        gm::map_render::kOverviewSettlementAccent.b,
        gm::map_render::kOverviewSettlementAccent.a);
    const float outerRadius = std::max(3.0f, cellSize * style.outerRadiusCells);
    const float innerRadius = std::max(1.5f, cellSize * style.innerRadiusCells);
    const float line = std::max(1.0f, cellSize * 0.075f);
    drawList->AddCircleFilled(center, outerRadius + line * 0.75f, ink, 28);
    drawList->AddCircleFilled(center, outerRadius, paper, 28);
    drawList->AddCircle(center, outerRadius, ink, 28, line);

    if (kind == gm::map_render::OverviewSettlementGlyphKind::Town)
    {
        drawList->AddCircle(center, innerRadius, ink, 22,
                            std::max(0.9f, line * 0.72f));
        drawList->AddCircleFilled(center, std::max(1.2f, cellSize * 0.095f),
                                  accent, 16);
        return;
    }
    if (kind == gm::map_render::OverviewSettlementGlyphKind::City)
    {
        drawList->AddCircle(center, innerRadius, ink, 24,
                            std::max(1.0f, line * 0.85f));
        drawList->AddCircleFilled(center, std::max(1.7f, cellSize * 0.14f),
                                  accent, 18);
        return;
    }

    drawList->AddCircle(center, innerRadius, ink, 26,
                        std::max(1.0f, line * 0.78f));
    constexpr float diagonal = 0.70710678118f;
    constexpr float directions[][2] = {
        {1.0f, 0.0f}, {diagonal, diagonal}, {0.0f, 1.0f},
        {-diagonal, diagonal}, {-1.0f, 0.0f}, {-diagonal, -diagonal},
        {0.0f, -1.0f}, {diagonal, -diagonal}};
    for (const auto &direction : directions)
    {
        const ImVec2 end(center.x + direction[0] * innerRadius * 0.88f,
                         center.y + direction[1] * innerRadius * 0.88f);
        drawList->AddLine(center, end, ink, line + 1.1f);
        drawList->AddLine(center, end, accent, std::max(1.0f, line * 0.68f));
    }
    drawList->AddCircleFilled(center, std::max(2.0f, cellSize * 0.15f), ink, 18);
    drawList->AddCircleFilled(center, std::max(1.1f, cellSize * 0.075f), accent, 16);
}

inline const char *mapOverviewLandmarkSemanticRole(const MapModel &map, int x, int y)
{
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
    {
        if (feature.kind != MapGeneratedFeatureKind::Landmark || feature.points.empty())
            continue;
        const int featureX = std::clamp(static_cast<int>(std::floor(feature.points.front().x)),
                                        0, std::max(0, map.width - 1));
        const int featureY = std::clamp(static_cast<int>(std::floor(feature.points.front().y)),
                                        0, std::max(0, map.height - 1));
        if (featureX != x || featureY != y)
            continue;
        if (feature.materialId.find("fortress") != std::string::npos)
            return "fortress";
        if (feature.materialId.find("ruin") != std::string::npos)
            return "ruin";
        if (feature.materialId.find("cave") != std::string::npos)
            return "cave";
        return "discovery";
    }
    return "discovery";
}

inline bool mapCellIsLocalStampAnchor(const MapModel &map, int x, int y, MapTileKind kind, int radius, int salt)
{
    const MapCellModel *cell = mapCellAt(map, x, y);
    if (!cell || cell->kind != kind)
        return false;

    // Multi-cell settlement semantics describe one continuous built-up area,
    // not one oversized illustration per cell. Choose a deterministic local
    // minimum so nearby cells share a single painterly stamp while isolated
    // settlements always remain visible.
    constexpr int scoreRange = 104729;
    const int centerScore = deterministicMapTileIndex(x, y, salt, scoreRange);
    for (int neighborY = std::max(0, y - radius); neighborY <= std::min(map.height - 1, y + radius); ++neighborY)
    {
        for (int neighborX = std::max(0, x - radius); neighborX <= std::min(map.width - 1, x + radius); ++neighborX)
        {
            if (neighborX == x && neighborY == y)
                continue;
            const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
            if (!neighbor || neighbor->kind != kind)
                continue;
            const int neighborScore = deterministicMapTileIndex(neighborX, neighborY, salt, scoreRange);
            if (neighborScore < centerScore ||
                (neighborScore == centerScore && (neighborY < y || (neighborY == y && neighborX < x))))
                return false;
        }
    }
    return true;
}

inline float mapBuildingStreetRotation(const MapModel &map, int x, int y)
{
    auto roadLike = [](const MapCellModel *cell) {
        return cell && (cell->wayKind == MapWayKind::Trail || cell->wayKind == MapWayKind::DirtRoad ||
                        cell->wayKind == MapWayKind::StoneRoad);
    };
    int horizontal = 0;
    int vertical = 0;
    for (int oy = -2; oy <= 2; ++oy)
    {
        for (int ox = -2; ox <= 2; ++ox)
        {
            const MapCellModel *candidate = mapCellAt(map, x + ox, y + oy);
            if (!roadLike(candidate))
                continue;
            const bool eastWest = roadLike(mapCellAt(map, x + ox - 1, y + oy)) ||
                                  roadLike(mapCellAt(map, x + ox + 1, y + oy));
            const bool northSouth = roadLike(mapCellAt(map, x + ox, y + oy - 1)) ||
                                    roadLike(mapCellAt(map, x + ox, y + oy + 1));
            horizontal += eastWest ? 1 : 0;
            vertical += northSouth ? 1 : 0;
        }
    }
    return vertical > horizontal ? 1.57079632679f : 0.0f;
}

inline bool mapWorldPointIsLand(const MapModel &map, float x, float y);

inline void drawMapTextWithHalo(ImDrawList *drawList,
                                ImFont *font,
                                float fontSize,
                                ImVec2 position,
                                ImU32 textColor,
                                ImU32 haloColor,
                                float haloRadius,
                                const char *text)
{
    if (!drawList || !font || !text || *text == '\0')
        return;
    static const ImVec2 directions[] = {
        {-1.0f, -1.0f}, {0.0f, -1.0f}, {1.0f, -1.0f},
        {-1.0f, 0.0f},                    {1.0f, 0.0f},
        {-1.0f, 1.0f},  {0.0f, 1.0f},   {1.0f, 1.0f}};
    for (const ImVec2 direction : directions)
        drawList->AddText(font, fontSize,
                          ImVec2(position.x + direction.x * haloRadius,
                                 position.y + direction.y * haloRadius),
                          haloColor, text);
    drawList->AddText(font, fontSize, position, textColor, text);
}
