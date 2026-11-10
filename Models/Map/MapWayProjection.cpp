#include "Models/Map/MapAuthoring.hpp"
#include "Models/Map/MapOverviewRenderPolicy.hpp"

void projectGeneratedWayFeatureToCells(MapModel &map, const MapGeneratedFeature &feature, bool erase,
                                       const std::vector<MapCellCoordinate> *affected)
{
    if (feature.points.size() < 2 ||
        (feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail &&
         feature.kind != MapGeneratedFeatureKind::River))
        return;
    const MapWayKind wayKind = mapGeneratedFeatureWayKind(feature);
    const auto restoredGround = [&](int x, int y) {
        auto ground = mapVisualGroundKind(map, x, y);
        if (!map.generation.continuousRendering ||
            (map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region) ||
            (ground != MapTileKind::Water && ground != MapTileKind::Bridge))
            return ground;
        std::array<int, 34> votes{};
        int best = 0;
        ground = MapTileKind::Dirt;
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
            {
                const auto *neighbor = mapCellAt(map, x + dx, y + dy);
                if (!neighbor || neighbor->wayKind != MapWayKind::None)
                    continue;
                const auto kind = neighbor->kind;
                if (kind != MapTileKind::Empty && kind != MapTileKind::Dirt && kind != MapTileKind::Forest &&
                    kind != MapTileKind::Mountain && kind != MapTileKind::Hill && kind != MapTileKind::Desert &&
                    kind != MapTileKind::Snow)
                    continue;
                const int count = ++votes[static_cast<int>(kind)];
                if (count > best)
                {
                    best = count;
                    ground = kind;
                }
            }
        return ground;
    };
    const auto applyTerrainWithoutDroppingAuthoredEdges = [](MapCellModel &cell, MapTileKind kind) {
        const int movementBlockedMask = cell.movementBlockedMask;
        const int lineOfSightBlockedMask = cell.lineOfSightBlockedMask;
        const int movementOneWayBlockedMask = cell.movementOneWayBlockedMask;
        const int lineOfSightOneWayBlockedMask = cell.lineOfSightOneWayBlockedMask;
        applyMapCellTerrainDefaults(cell, kind);
        cell.movementBlockedMask = movementBlockedMask;
        cell.lineOfSightBlockedMask = lineOfSightBlockedMask;
        cell.movementOneWayBlockedMask = movementOneWayBlockedMask;
        cell.lineOfSightOneWayBlockedMask = lineOfSightOneWayBlockedMask;
    };
    for (std::size_t index = 1; index < feature.points.size(); ++index)
    {
        const MapFeaturePoint &start = feature.points[index - 1];
        const MapFeaturePoint &end = feature.points[index];
        const float deltaX = end.x - start.x;
        const float deltaY = end.y - start.y;
        const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(deltaX), std::abs(deltaY)) * 2.0f)));
        for (int step = 0; step <= steps; ++step)
        {
            const float amount = static_cast<float>(step) / static_cast<float>(steps);
            const int x = std::clamp(static_cast<int>(std::floor(start.x + deltaX * amount)), 0, map.width - 1);
            const int y = std::clamp(static_cast<int>(std::floor(start.y + deltaY * amount)), 0, map.height - 1);
            if (affected && !std::binary_search(affected->begin(), affected->end(), MapCellCoordinate{x, y}))
                continue;
            MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell)
                continue;
            if (erase)
            {
                if (cell->wayKind == wayKind)
                {
                    if (cell->kind == MapTileKind::Water &&
                        (wayKind == MapWayKind::River || wayKind == MapWayKind::Canal))
                    {
                        const auto ground = restoredGround(x, y);
                        applyTerrainWithoutDroppingAuthoredEdges(*cell, ground);
                    }
                    else if (cell->kind == MapTileKind::Bridge && map.generation.continuousRendering &&
                             (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region))
                        applyTerrainWithoutDroppingAuthoredEdges(*cell, restoredGround(x, y));
                    cell->wayKind = MapWayKind::None;
                }
                continue;
            }
            if (wayKind == MapWayKind::River || wayKind == MapWayKind::Canal)
            {
                if ((feature.id.rfind("manual_stroke_", 0) == 0 ||
                     (map.generation.continuousRendering &&
                      (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region))) &&
                    mapAuthoringPreservesStandingWater(map, *cell, wayKind))
                    continue;
                applyTerrainWithoutDroppingAuthoredEdges(*cell, MapTileKind::Water);
                cell->wayKind = wayKind;
            }
            else
            {
                // An atlas road crossing newly painted standing water is a
                // repairable route conflict, not an instruction to raise a
                // string of Bridge cells out of the sea.
                if (map.generation.continuousRendering &&
                    (map.atlasLevel == MapLevel::World || map.atlasLevel == MapLevel::Region) &&
                    cell->kind == MapTileKind::Water && cell->wayKind == MapWayKind::None)
                    continue;
                if (cell->kind == MapTileKind::Water || cell->kind == MapTileKind::Swamp)
                    applyTerrainWithoutDroppingAuthoredEdges(*cell, MapTileKind::Bridge);
                cell->wayKind = wayKind;
            }
        }
    }
}

// Keep the semantic cell projection in sync after a control-point edit without
// rebuilding the feature graph from cells.  Rebuilding from cells would replace
// stable generated IDs and discard names, locks and atlas lineage.  Erasing the
// former footprint first and replaying ways only within affected cells restores
// overlapping routes without altering remote river mouths or coastline.
void reprojectGeneratedMapWayFeaturesAfterGeometryEdit(MapModel &map, const MapGeneratedFeature &featureBeforeEdit)
{
    if (map.width <= 0 || map.height <= 0)
        return;
    if (featureBeforeEdit.kind != MapGeneratedFeatureKind::Road &&
        featureBeforeEdit.kind != MapGeneratedFeatureKind::Trail &&
        featureBeforeEdit.kind != MapGeneratedFeatureKind::River)
        return;
    std::vector<MapCellCoordinate> affected;
    const auto collect = [&](const MapGeneratedFeature &feature) {
        for (std::size_t i = 1; i < feature.points.size(); ++i)
        {
            const auto a = feature.points[i - 1], b = feature.points[i];
            const int steps =
                std::max(1, static_cast<int>(std::ceil(std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)) * 2.0f)));
            for (int s = 0; s <= steps; ++s)
            {
                const float t = static_cast<float>(s) / steps;
                addUniqueMapCellCoordinate(
                    affected, map, std::clamp(static_cast<int>(std::floor(a.x + (b.x - a.x) * t)), 0, map.width - 1),
                    std::clamp(static_cast<int>(std::floor(a.y + (b.y - a.y) * t)), 0, map.height - 1));
            }
        }
    };
    collect(featureBeforeEdit);
    for (const auto &feature : map.generation.graph.features)
        if (feature.id == featureBeforeEdit.id)
            collect(feature);
    affected = normalizeMapCellCoordinates(std::move(affected));
    // Capture the old rendered ground BEFORE erasing the river markers. The
    // generator also rasterizes diagonal shoulders and wider cells outside the
    // sampled centreline; once the neighbouring markers disappear these would
    // be mistaken for independent circular lakes. Restore only cells belonging
    // to this river's old footprint, never standing water or another river.
    std::vector<std::pair<MapCellCoordinate, MapTileKind>> oldRiverGround;
    if (featureBeforeEdit.kind == MapGeneratedFeatureKind::River && gm::map_render::overviewUsesAtlasTerrain(map))
    {
        auto beforeMap = map;
        for (auto &f : beforeMap.generation.graph.features)
            if (f.id == featureBeforeEdit.id)
                f = featureBeforeEdit;
        const auto distance = [](MapFeaturePoint p, const MapGeneratedFeature &f) {
            float best = std::numeric_limits<float>::max();
            for (std::size_t i = 1; i < f.points.size(); ++i)
                best =
                    std::min(best, gm::map_geometry::roadPointSegmentDistanceSquared(p, f.points[i - 1], f.points[i]));
            return best;
        };
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
            {
                const auto &cell = *mapCellAt(beforeMap, x, y);
                if (cell.kind != MapTileKind::Water && cell.kind != MapTileKind::Bridge)
                    continue;
                const MapFeaturePoint p{x + .5f, y + .5f};
                const float d = distance(p, featureBeforeEdit);
                if (d > 2.25f)
                    continue;
                if (std::any_of(beforeMap.generation.graph.features.begin(), beforeMap.generation.graph.features.end(),
                                [&](const auto &f) {
                                    return f.kind == MapGeneratedFeatureKind::River && f.id != featureBeforeEdit.id &&
                                           distance(p, f) < d;
                                }))
                    continue;
                const auto ground = gm::map_render::overviewGroundKindForRender(beforeMap, x, y);
                if (!gm::map_render::overviewGroundKindIsLand(ground))
                    continue;
                oldRiverGround.push_back({{x, y}, ground});
                addUniqueMapCellCoordinate(affected, map, x, y);
            }
    }
    affected = normalizeMapCellCoordinates(std::move(affected));
    projectGeneratedWayFeatureToCells(map, featureBeforeEdit, true);
    for (const auto &[p, ground] : oldRiverGround)
    {
        auto &cell = *mapCellAt(map, p.x, p.y);
        const auto movement = cell.movementBlockedMask, sight = cell.lineOfSightBlockedMask,
                   oneWay = cell.movementOneWayBlockedMask, sightOneWay = cell.lineOfSightOneWayBlockedMask;
        applyMapCellTerrainDefaults(cell, ground);
        cell.movementBlockedMask = movement;
        cell.lineOfSightBlockedMask = sight;
        cell.movementOneWayBlockedMask = oneWay;
        cell.lineOfSightOneWayBlockedMask = sightOneWay;
        if (cell.wayKind == MapWayKind::River || cell.wayKind == MapWayKind::Canal)
            cell.wayKind = MapWayKind::None;
    }
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::River)
            projectGeneratedWayFeatureToCells(map, feature, false, &affected);
    // Projection order must not depend on graph order. Roads over rivers
    // remain crossings, even when the river was created after the road.
    for (const MapGeneratedFeature &feature : map.generation.graph.features)
        if (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail)
            projectGeneratedWayFeatureToCells(map, feature, false, &affected);
}
