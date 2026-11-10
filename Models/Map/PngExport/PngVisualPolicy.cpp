#include "Models/Map/PngExport/PngVisualPolicy.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"
#include "Models/Map/MapStyleRegistry.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace MapPngExportRaster
{
float visualBuildingStreetRotation(const MapModel &map, int x, int y)
{
    auto roadLike = [](const MapCellModel *cell) {
        return cell && (cell->wayKind == MapWayKind::Trail || cell->wayKind == MapWayKind::DirtRoad ||
                        cell->wayKind == MapWayKind::StoneRoad);
    };
    int horizontal = 0;
    int vertical = 0;
    for (int oy = -2; oy <= 2; ++oy)
        for (int ox = -2; ox <= 2; ++ox)
        {
            if (!roadLike(mapCellAt(map, x + ox, y + oy)))
                continue;
            horizontal += roadLike(mapCellAt(map, x + ox - 1, y + oy)) ||
                          roadLike(mapCellAt(map, x + ox + 1, y + oy));
            vertical += roadLike(mapCellAt(map, x + ox, y + oy - 1)) ||
                        roadLike(mapCellAt(map, x + ox, y + oy + 1));
        }
    return vertical > horizontal ? 1.57079632679f : 0.0f;
}

std::vector<MapFeaturePoint> offsetFeaturePoints(const std::vector<MapFeaturePoint> &points, float offset)
{
    std::vector<MapFeaturePoint> result;
    result.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        const MapFeaturePoint &previous = points[index == 0 ? index : index - 1];
        const MapFeaturePoint &next = points[index + 1 < points.size() ? index + 1 : index];
        const float dx = next.x - previous.x;
        const float dy = next.y - previous.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.0001f)
            result.push_back(points[index]);
        else
            result.push_back({points[index].x - dy / length * offset,
                              points[index].y + dx / length * offset});
    }
    return result;
}

const char *terrainSemanticRole(MapLevel level, MapTileKind kind)
{
    // A World forest is a biome, not a close-up forest floor. Use the macro
    // grass painting only as subdued texture and tint it into the forest base
    // below; this also preserves immutable V3 through V3.3 snapshot selection.
    if (level == MapLevel::World && kind == MapTileKind::Forest)
        return "grass_temperate";
    switch (kind)
    {
    case MapTileKind::Water: return "water_shallow";
    case MapTileKind::Dirt: return "earth";
    case MapTileKind::Stone:
    case MapTileKind::Room: return "dressed_stone_floor";
    case MapTileKind::Cave: return "cave_floor";
    case MapTileKind::Forest: return "forest_floor";
    case MapTileKind::Swamp: return "mud";
    case MapTileKind::Road: return "gravel";
    default: return "grass_temperate";
    }
}

int scaleClusterModulo(MapLevel level, MapTileKind kind)
{
    const MapRenderScaleProfile &profile = mapRenderScaleProfile(level);
    const float spacing = kind == MapTileKind::Forest ? profile.vegetationClusterDistance : profile.mountainClusterDistance;
    return std::max(3, static_cast<int>(std::round(spacing * spacing * 0.82f)));
}

Pixel terrainColor(MapTileKind kind)
{
    switch (kind)
    {
    case MapTileKind::Forest: return {45, 73, 55, 255};
    case MapTileKind::Mountain: return {91, 91, 78, 255};
    case MapTileKind::Hill: return {105, 106, 65, 255};
    case MapTileKind::Stone: return {111, 104, 87, 255};
    case MapTileKind::Dirt: return {135, 105, 67, 255};
    case MapTileKind::Swamp: return {61, 84, 64, 255};
    case MapTileKind::Desert: return {161, 137, 83, 255};
    case MapTileKind::Snow: return {184, 190, 178, 255};
    case MapTileKind::Ice: return {128, 171, 177, 255};
    case MapTileKind::Water: return {41, 126, 151, 255};
    case MapTileKind::Room: return {145, 132, 105, 255};
    case MapTileKind::Wall: return {46, 47, 47, 255};
    case MapTileKind::Door: return {137, 91, 52, 255};
    case MapTileKind::Cave: return {89, 78, 66, 255};
    case MapTileKind::Stairs: return {176, 159, 124, 255};
    case MapTileKind::Hazard: return {171, 85, 56, 255};
    case MapTileKind::Lava: return {191, 68, 31, 255};
    case MapTileKind::Pit:
    case MapTileKind::Chasm: return {24, 26, 27, 255};
    case MapTileKind::Rubble: return {104, 94, 79, 255};
    case MapTileKind::Cover: return {117, 91, 61, 255};
    case MapTileKind::Bridge: return {137, 100, 61, 255};
    default: return {74, 96, 53, 255};
    }
}

Pixel blended(Pixel a, Pixel b, float amount)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    auto channel = [&](std::uint8_t left, std::uint8_t right) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(left + (right - left) * amount)), 0, 255));
    };
    return {channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), 255};
}

bool generatedPointSymbolVisible(const MapModel &map, int x, int y, bool playerSafe)
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
        if (!feature.gmOnly && feature.discovered)
            return true;
    }
    // A generated map's feature graph is authoritative. If the reviewed plan
    // removed a private point feature, its raster projection may still retain
    // a Settlement/Landmark/etc. cell. Treat that unmatched projection as
    // private instead of accidentally resurrecting it in PNG output. Legacy
    // hand-authored raster maps have no enabled generation metadata and keep
    // their historic cell symbols.
    return !matched && !map.generation.enabled;
}

const MapGeneratedFeature *generatedSettlementAt(const MapModel &map, int x, int y)
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

const char *overviewSettlementSemanticRole(const MapGeneratedFeature &feature)
{
    if (feature.id == "brief_capital" || feature.materialId.find("capital") != std::string::npos)
        return "capital";
    if (feature.id.rfind("brief_city_", 0) == 0 || feature.locked ||
        feature.materialId.find("city") != std::string::npos)
        return "city";
    return "town";
}

const char *overviewSettlementSemanticRole(const MapModel &map, int x, int y)
{
    const MapGeneratedFeature *feature = generatedSettlementAt(map, x, y);
    return feature ? overviewSettlementSemanticRole(*feature) : "town";
}


}
