#pragma once

#include "Views/Editor/Widgets/Map/MapVisualTerrainCache.hpp"

inline bool mapTileIsLandBiomeForBlend(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty:
    case MapTileKind::Forest:
    case MapTileKind::Mountain:
    case MapTileKind::Hill:
    case MapTileKind::Dirt:
    case MapTileKind::Stone:
    case MapTileKind::Snow:
    case MapTileKind::Desert:
    case MapTileKind::Swamp:
        return true;
    default:
        return false;
    }
}

inline int mapLandBiomeBlendPriority(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Empty: return 0;
    case MapTileKind::Forest: return 1;
    case MapTileKind::Hill: return 2;
    case MapTileKind::Swamp: return 3;
    case MapTileKind::Dirt: return 4;
    case MapTileKind::Desert: return 5;
    case MapTileKind::Snow: return 6;
    case MapTileKind::Stone: return 7;
    case MapTileKind::Mountain: return 8;
    default: return 9;
    }
}

inline void drawMapLandBiomeBlendDecals(ImDrawList *drawList,
                                        ImVec2 origin,
                                        float cellSize,
                                        const MapModel &map)
{
    if (!drawList || cellSize <= 0.0f || map.scope == MapScope::Dungeon ||
        map.generation.recipe.styleVersion < 3 ||
        !mapStyleProductionSnapshotAvailable(map.generation.recipe.styleSnapshotId))
        return;

    const bool overview = map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region;
    const float extentCells = map.atlasLevel == MapLevel::World ? 4.4f :
                              (map.atlasLevel == MapLevel::Region ? 3.4f :
                               (map.atlasLevel == MapLevel::Site ? 1.65f : 1.4f));
    const int placementModulo = map.atlasLevel == MapLevel::World ? 7 :
                                (map.atlasLevel == MapLevel::Region ? 5 : 4);
    const int padding = std::max(2, static_cast<int>(std::ceil(extentCells * 0.6f)));
    const MapVisibleCellBounds visible = mapVisibleCellBounds(drawList, origin, cellSize, map, padding);
    const float extent = cellSize * extentCells;
    const ImU32 tint = overview ? IM_COL32(255, 255, 255, 58) : IM_COL32(255, 255, 255, 70);

    auto drawBoundary = [&](int x, int y, int neighborX, int neighborY, bool verticalBoundary) {
        const MapCellModel *cell = mapCellAt(map, x, y);
        const MapCellModel *neighbor = mapCellAt(map, neighborX, neighborY);
        if (!cell || !neighbor || cell->kind == neighbor->kind ||
            !mapTileIsLandBiomeForBlend(cell->kind) || !mapTileIsLandBiomeForBlend(neighbor->kind))
            return;

        const int firstKind = std::min(static_cast<int>(cell->kind), static_cast<int>(neighbor->kind));
        const int secondKind = std::max(static_cast<int>(cell->kind), static_cast<int>(neighbor->kind));
        const int salt = 2200 + firstKind * 37 + secondKind * 101 + (verticalBoundary ? 0 : 17);
        if (deterministicMapTileIndex(x, y, salt, placementModulo) != 0)
            return;

        ImVec2 center;
        float angle = 0.0f;
        if (verticalBoundary)
        {
            center = ImVec2(origin.x + static_cast<float>(x + 1) * cellSize,
                            origin.y + (static_cast<float>(y) + 0.5f) * cellSize);
        }
        else
        {
            center = ImVec2(origin.x + (static_cast<float>(x) + 0.5f) * cellSize,
                            origin.y + static_cast<float>(y + 1) * cellSize);
            angle = 1.57079632679f;
        }
        // Source masks transition from their softer land material on the left
        // to the denser material on the right. A half turn keeps that broad
        // visual direction stable for either cell ordering.
        if (mapLandBiomeBlendPriority(cell->kind) > mapLandBiomeBlendPriority(neighbor->kind))
            angle += 3.14159265359f;

        drawProductionAssetRoleRotated(
            drawList, center, extent, extent, angle, map,
            "blend_mask_or_decal", "organic_blend_mask", x, y, salt, tint,
            mapProductionAssetZoom(cellSize));
    };

    for (int y = visible.minY; y <= visible.maxY; ++y)
    {
        for (int x = visible.minX; x <= visible.maxX; ++x)
        {
            if (x + 1 < map.width)
                drawBoundary(x, y, x + 1, y, true);
            if (y + 1 < map.height)
                drawBoundary(x, y, x, y + 1, false);
        }
    }
}

inline void drawMapOverviewRelief(ImDrawList *drawList,
                                  ImVec2 origin,
                                  float cellSize,
                                  const MapModel &map,
                                  const MapVisibleCellBounds &visible)
{
    if (!drawList || (map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region) ||
        visible.maxX < visible.minX || visible.maxY < visible.minY)
        return;

    // The currently accepted mountain sheets contain clipped neighbouring
    // fragments and therefore remain disabled.  Connected contour strokes keep
    // ranges legible at atlas scale without replacing them with tactical peaks.
    for (int y = visible.minY; y <= visible.maxY; ++y)
    {
        int runStart = -1;
        bool containsMountain = false;
        auto flushRun = [&](int endX) {
            if (runStart < 0)
                return;
            const float left = origin.x + (static_cast<float>(runStart) + 0.12f) * cellSize;
            const float right = origin.x + (static_cast<float>(endX) - 0.12f) * cellSize;
            const float baseline = origin.y + (static_cast<float>(y) + 0.64f) * cellSize;
            const float rise = cellSize * (containsMountain ? 0.42f : 0.25f);
            const ImU32 shadow = containsMountain ? IM_COL32(50, 49, 42, 145) : IM_COL32(70, 70, 47, 115);
            const ImU32 light = containsMountain ? IM_COL32(174, 166, 135, 155) : IM_COL32(151, 145, 91, 130);
            drawList->AddBezierCubic(
                ImVec2(left, baseline), ImVec2(left + (right - left) * 0.28f, baseline - rise),
                ImVec2(left + (right - left) * 0.68f, baseline - rise * 0.86f), ImVec2(right, baseline),
                shadow, std::max(1.6f, cellSize * 0.075f), 16);
            drawList->AddBezierCubic(
                ImVec2(left, baseline - cellSize * 0.045f),
                ImVec2(left + (right - left) * 0.28f, baseline - rise - cellSize * 0.045f),
                ImVec2(left + (right - left) * 0.68f, baseline - rise * 0.86f - cellSize * 0.045f),
                ImVec2(right, baseline - cellSize * 0.045f),
                light, std::max(1.0f, cellSize * 0.026f), 16);
            runStart = -1;
            containsMountain = false;
        };

        for (int x = visible.minX; x <= visible.maxX + 1; ++x)
        {
            const MapTileKind kind = x <= visible.maxX ? mapVisualGroundKind(map, x, y) : MapTileKind::Empty;
            const bool relief = kind == MapTileKind::Hill || kind == MapTileKind::Mountain;
            if (relief)
            {
                if (runStart < 0)
                    runStart = x;
                containsMountain = containsMountain || kind == MapTileKind::Mountain;
            }
            else
            {
                flushRun(x);
            }
        }
    }
}

inline void drawWorldPainterlySurface(ImDrawList *drawList,
                                      ImVec2 origin,
                                      float cellSize,
                                      const MapModel &map)
{
    if (!drawList || map.atlasLevel != MapLevel::World ||
        map.generation.recipe.styleVersion < 3 || cellSize <= 0.0f)
        return;
    const ImVec2 mapMax(origin.x + map.width * cellSize,
                        origin.y + map.height * cellSize);
    // The neutral source is deliberately global: one quad, one set of UVs,
    // and therefore no repeated tile seam. Features and coastlines are drawn
    // afterwards and retain their full contrast.
    drawProductionAssetRole(
        drawList, origin, mapMax, map,
        "world_surface_modulation", "watercolor",
        0, 0, 0,
        IM_COL32(255, 255, 255,
                 gm::map_render::kOverviewPainterlySurfaceOpacity),
        mapProductionAssetZoom(cellSize));
}

inline void drawMapDungeonMasonryBoundary(ImDrawList *drawList, ImVec2 origin, float cellSize, const MapModel &map)
{
    if (!drawList || !gm::map_render::usesDungeonMasonryInk(map)) return;
    // Match the PNG perimeter pass: the boundary belongs to exposed floor
    // edges, never a stamp on the neighbouring rock cell. Draw after every
    // floor tile so later cells cannot cover a previously drawn wall.
    constexpr int offsets[][2]={{0,-1},{1,0},{0,1},{-1,0}};
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x)
    {
        const auto *cell=mapCellAt(map,x,y);
        if(!cell || (cell->kind!=MapTileKind::Room && cell->kind!=MapTileKind::Cave &&
                     cell->kind!=MapTileKind::Door && cell->kind!=MapTileKind::Stairs)) continue;
        for(int side=0;side<4;++side)
        {
            const auto *neighbor=mapCellAt(map,x+offsets[side][0],y+offsets[side][1]);
            if(neighbor && neighbor->kind!=MapTileKind::Wall && neighbor->kind!=MapTileKind::Empty) continue;
            const float l=origin.x+x*cellSize,t=origin.y+y*cellSize,r=l+cellSize,b=t+cellSize;
            ImVec2 a(l,t),z(r,t);
            if(side==1) { a={r,t};z={r,b}; }
            else if(side==2) { a={l,b};z={r,b}; }
            else if(side==3) { a={l,t};z={l,b}; }
            drawList->AddLine(a,z,IM_COL32(48,44,38,255),std::max(2.f,cellSize/3.f));
            drawList->AddLine(a,z,IM_COL32(130,118,94,255),std::max(1.f,cellSize/7.f));
        }
    }
}
