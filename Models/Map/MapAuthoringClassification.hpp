#pragma once

#include "Models/Map/MapAuthoringGeometry.hpp"

struct MapGeneratedFeatureClassificationOption
{
    MapGeneratedFeatureKind kind = MapGeneratedFeatureKind::Landmark;
    const char *label = "Generated feature";
};

struct MapGeneratedFeatureAppearanceOption
{
    const char *label = "Style default";
    std::string materialId;
    float recommendedWidth = 0.0f;
};

inline std::vector<MapGeneratedFeatureClassificationOption>
mapGeneratedFeatureClassificationOptions(MapGeneratedFeatureKind kind)
{
    switch (kind)
    {
    case MapGeneratedFeatureKind::Road:
    case MapGeneratedFeatureKind::Trail:
        return {{MapGeneratedFeatureKind::Trail, "Trail"},
                {MapGeneratedFeatureKind::Road, "Road"}};
    case MapGeneratedFeatureKind::Settlement:
    case MapGeneratedFeatureKind::Landmark:
        return {{MapGeneratedFeatureKind::Settlement, "Settlement"},
                {MapGeneratedFeatureKind::Landmark, "Landmark"}};
    default:
        return {{kind, "Generated feature"}};
    }
}

inline std::vector<MapGeneratedFeatureAppearanceOption>
mapGeneratedFeatureAppearanceOptions(const MapGeneratedFeature &feature)
{
    switch (feature.kind)
    {
    case MapGeneratedFeatureKind::Road:
        return {{"Dirt road", "fantasy_neutral.road.dirt", 0.82f},
                {"Gravel road", "fantasy_neutral.road.gravel", 0.90f},
                {"Stone road", "fantasy_neutral.road.stone", 1.08f}};
    case MapGeneratedFeatureKind::Trail:
        return {{"Narrow trail", "fantasy_neutral.road.trail", 0.55f}};
    case MapGeneratedFeatureKind::River:
        return {{"Creek", "fantasy_neutral.water.creek", 0.60f},
                {"River", "fantasy_neutral.water.river", 1.00f},
                {"Deep river", "fantasy_neutral.water.river.deep", 1.60f},
                {"Canal", "fantasy_neutral.water.canal", 1.00f}};
    case MapGeneratedFeatureKind::Bridge:
        return {{"Light wooden bridge", "fantasy_neutral.bridge.wood", 0.90f},
                {"Heavy wooden bridge", "fantasy_neutral.bridge.wood.heavy", 1.40f},
                {"Rough stone bridge", "fantasy_neutral.bridge.stone", 1.15f},
                {"Dressed stone bridge", "fantasy_neutral.bridge.stone.dressed", 1.20f},
                {"Ford", "fantasy_neutral.bridge.ford", 1.10f},
                {"Pier", "fantasy_neutral.bridge.pier", 0.80f}};
    case MapGeneratedFeatureKind::Settlement:
        return {{"Capital", "fantasy_neutral.settlement.capital", 0.0f},
                {"City", "fantasy_neutral.settlement.city", 0.0f},
                {"Town", "fantasy_neutral.settlement.town", 0.0f},
                {"Village", "fantasy_neutral.settlement.village", 0.0f}};
    case MapGeneratedFeatureKind::Landmark:
        return {{"Discovery", "fantasy_neutral.landmark.discovery", 0.0f},
                {"Fortress", "fantasy_neutral.landmark.fortress", 0.0f},
                {"Ruin", "fantasy_neutral.landmark.ruin", 0.0f},
                {"Cave", "fantasy_neutral.landmark.cave", 0.0f}};
    case MapGeneratedFeatureKind::TacticalSetpiece:
        return {{"Cover", "fantasy_neutral.tactical.cover", 0.0f},
                {"Rubble", "fantasy_neutral.tactical.rubble", 0.0f},
                {"Barricade", "fantasy_neutral.tactical.barricade", 0.0f}};
    default:
        return {{"Style default", feature.materialId, 0.0f}};
    }
}

inline int mapGeneratedFeatureClassificationOptionIndex(const MapGeneratedFeature &feature)
{
    const std::vector<MapGeneratedFeatureClassificationOption> options =
        mapGeneratedFeatureClassificationOptions(feature.kind);
    for (int index = 0; index < static_cast<int>(options.size()); ++index)
        if (options[static_cast<std::size_t>(index)].kind == feature.kind)
            return index;
    return 0;
}

inline int mapGeneratedFeatureAppearanceOptionIndex(const MapGeneratedFeature &feature)
{
    const std::vector<MapGeneratedFeatureAppearanceOption> options =
        mapGeneratedFeatureAppearanceOptions(feature);
    for (int index = 0; index < static_cast<int>(options.size()); ++index)
        if (options[static_cast<std::size_t>(index)].materialId == feature.materialId)
            return index;
    return -1;
}

inline MapTileKind mapGeneratedPointFeatureTileKind(const MapGeneratedFeature &feature)
{
    if (feature.kind == MapGeneratedFeatureKind::Settlement)
        return MapTileKind::Settlement;
    if (feature.kind == MapGeneratedFeatureKind::Landmark)
        return MapTileKind::Landmark;
    if (feature.kind == MapGeneratedFeatureKind::BuildingGroup)
        return MapTileKind::Building;
    if (feature.kind == MapGeneratedFeatureKind::TacticalSetpiece)
    {
        if (feature.materialId.find("rubble") != std::string::npos)
            return MapTileKind::Rubble;
        if (feature.materialId.find("hazard") != std::string::npos)
            return MapTileKind::Hazard;
        return MapTileKind::Cover;
    }
    return MapTileKind::Empty;
}

inline void projectGeneratedPointFeatureClassificationToCells(
    MapModel &map,
    const MapGeneratedFeature &feature)
{
    if (feature.points.empty())
        return;
    const MapTileKind tileKind = mapGeneratedPointFeatureTileKind(feature);
    if (tileKind == MapTileKind::Empty)
        return;
    const int x = std::clamp(static_cast<int>(std::floor(feature.points.front().x)),
                             0, std::max(0, map.width - 1));
    const int y = std::clamp(static_cast<int>(std::floor(feature.points.front().y)),
                             0, std::max(0, map.height - 1));
    if (MapCellModel *cell = mapCellAt(map, x, y))
        cell->kind = tileKind;
}

// Road junctions are endpoints attached to another route's display geometry.
// Preserve their original segment/amount while an approach is reshaped. Looking
// up the attachment in the immutable geometry avoids frame-dependent drift.
inline void reconnectGeneratedRoadJunctions(MapModel &map,
                                            const std::vector<MapGeneratedFeature> &original)
{
    auto &features = map.generation.graph.features;
    if (features.size() != original.size()) return;
    const auto isRoad = [&](const MapGeneratedFeature &feature) {
        return !mapGeneratedFeatureIsLocked(map,feature) && feature.points.size() >= 2 &&
            (feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail);
    };
    std::vector<int> queue;
    std::vector<unsigned char> anchored(features.size(), 0);
    for (int i = 0; i < static_cast<int>(features.size()); ++i)
    {
        if (!isRoad(features[i]) || features[i].points.size() != original[i].points.size()) continue;
        bool changed = false;
        for (std::size_t p = 0; p < features[i].points.size(); ++p)
            changed |= mapFeaturePointDistanceSquared(features[i].points[p], original[i].points[p]) > .000001f;
        if (changed) queue.push_back(i);
        if (mapFeaturePointDistanceSquared(features[i].points.front(), original[i].points.front()) > .000001f) anchored[i] |= 1;
        if (mapFeaturePointDistanceSquared(features[i].points.back(), original[i].points.back()) > .000001f) anchored[i] |= 2;
    }
    for (std::size_t next = 0; next < queue.size(); ++next)
    {
        const int parent = queue[next];
        for (int child = 0; child < static_cast<int>(features.size()); ++child)
        {
            if (child == parent || !isRoad(features[child])) continue;
            for (int end = 0; end < 2; ++end)
            {
                if (anchored[child] & (1 << end)) continue;
                const int endpoint = end ? static_cast<int>(original[child].points.size()) - 1 : 0;
                const auto point = original[child].points[endpoint];
                float closestSquared = .0025f; // Exact joins, allowing float/save rounding.
                int segment = -1;
                float amount = 0;
                for (int p = 0; p + 1 < static_cast<int>(original[parent].points.size()); ++p)
                {
                    const auto a = original[parent].points[p], b = original[parent].points[p + 1];
                    const float dx = b.x - a.x, dy = b.y - a.y;
                    const float length = dx * dx + dy * dy;
                    const float t = length > .000001f
                        ? std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length, 0.0f, 1.0f) : 0.0f;
                    const float distance = mapFeaturePointDistanceSquared(point, {a.x + dx * t, a.y + dy * t});
                    if (distance <= closestSquared) { closestSquared = distance; segment = p; amount = t; }
                }
                if (segment < 0) continue;
                const auto a = original[parent].points[segment], b = original[parent].points[segment + 1];
                const auto movedA = features[parent].points[segment], movedB = features[parent].points[segment + 1];
                const MapFeaturePoint displacement{
                    (movedA.x - a.x) * (1 - amount) + (movedB.x - b.x) * amount,
                    (movedA.y - a.y) * (1 - amount) + (movedB.y - b.y) * amount};
                if (displacement.x * displacement.x + displacement.y * displacement.y <= .000001f) continue;
                const auto current = features[child];
                moveGeneratedMapFeatureSection(map, child, endpoint,
                    {point.x + displacement.x, point.y + displacement.y}, current);
                anchored[child] |= 1 << end;
                queue.push_back(child);
            }
        }
    }
    for (int i = 0; i < static_cast<int>(features.size()); ++i)
        if (isRoad(features[i]) && std::find(queue.begin(), queue.end(), i) != queue.end())
            reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map, original[i]);
}

inline bool moveGeneratedBuildingFootprint(MapModel &map, const MapGeneratedFeature &before, MapGeneratedFeature &after)
{
    const int oldX = static_cast<int>(std::floor(before.points.front().x));
    const int oldY = static_cast<int>(std::floor(before.points.front().y));
    const int dx = static_cast<int>(std::floor(after.points.front().x)) - oldX;
    const int dy = static_cast<int>(std::floor(after.points.front().y)) - oldY;
    const auto reject = [&]() { after = before; return false; };
    if (mapGeneratedFeatureIsLocked(map,before)) return reject();
    const auto *anchor = mapCellAt(map, oldX, oldY);
    if (!anchor || anchor->kind != MapTileKind::Building) return reject();
    if (!dx && !dy) { after.points = before.points; return true; }
    std::vector<MapCellCoordinate> cells{{oldX,oldY}};
    std::vector<bool> owned(map.cells.size(), false);
    owned[static_cast<std::size_t>(oldY * map.width + oldX)] = true;
    constexpr int directions[][2]{{1,0},{-1,0},{0,1},{0,-1}};
    for (std::size_t next = 0; next < cells.size(); ++next)
        for (const auto &direction : directions)
        {
            const int x = cells[next].x + direction[0], y = cells[next].y + direction[1];
            const auto *cell = mapCellAt(map, x, y);
            if (!cell || cell->kind != MapTileKind::Building) continue;
            const auto index = static_cast<std::size_t>(y * map.width + x);
            if (!owned[index]) { owned[index] = true; cells.push_back({x,y}); }
        }
    // A terrain edit may have joined two independently private buildings.
    // Require a distinct footprint before moving either object's anchor.
    for (const auto &feature : map.generation.graph.features)
        if (feature.id != before.id && feature.kind == MapGeneratedFeatureKind::BuildingGroup && !feature.points.empty())
        {
            const int x = static_cast<int>(std::floor(feature.points.front().x)), y = static_cast<int>(std::floor(feature.points.front().y));
            if (isMapCellInside(map,x,y) && owned[static_cast<std::size_t>(y*map.width+x)]) return reject();
        }
    std::vector<MapCellModel> originals;
    std::vector<MapCellModel> destinations;
    std::vector<MapTileKind> ground;
    for (const auto &position : cells)
    {
        const int x = position.x + dx, y = position.y + dy;
        const auto *destination = mapCellAt(map,x,y);
        if (!destination) return reject();
        if (!owned[static_cast<std::size_t>(y*map.width+x)])
        {
            if (destination->wayKind != MapWayKind::None) return reject();
            switch (destination->kind)
            {
            case MapTileKind::Empty: case MapTileKind::Dirt: case MapTileKind::Stone:
            case MapTileKind::Snow: case MapTileKind::Desert: case MapTileKind::Forest:
            case MapTileKind::Hill: case MapTileKind::Room: case MapTileKind::Cave: break;
            default: return reject();
            }
        }
        // Keep neighboring roofs separate, including private/public objects.
        for (const auto &direction : directions)
        {
            const int nx=x+direction[0], ny=y+direction[1];
            const auto *neighbor=mapCellAt(map,nx,ny);
            if (neighbor && neighbor->kind==MapTileKind::Building && !owned[static_cast<std::size_t>(ny*map.width+nx)]) return reject();
        }
        originals.push_back(*mapCellAt(map,position.x,position.y));
        destinations.push_back(*destination);
        ground.push_back(mapVisualGroundKind(map,position.x,position.y));
    }
    for (std::size_t i=0;i<cells.size();++i)
    {
        applyMapCellTerrainDefaults(*mapCellAt(map,cells[i].x,cells[i].y),ground[i]);
        mapCellAt(map,cells[i].x,cells[i].y)->elevation=originals[i].elevation;
    }
    for (std::size_t i=0;i<cells.size();++i)
    {
        auto &destination=*mapCellAt(map,cells[i].x+dx,cells[i].y+dy);
        // Fog, lighting and terrain elevation belong to the destination cells.
        // Building movement/visibility properties travel with its real footprint.
        destination=originals[i];
        destination.fogged=destinations[i].fogged;
        destination.lighting=destinations[i].lighting;
        destination.elevation=destinations[i].elevation;
    }
    after.points=before.points;
    for (auto &point:after.points) { point.x+=dx; point.y+=dy; }
    return true;
}

inline MapFeaturePoint nearestMapRoadPoint(const MapGeneratedFeature &road, MapFeaturePoint p)
{
    MapFeaturePoint best = road.points.front();
    float distance = mapFeaturePointDistanceSquared(best, p);
    for (std::size_t i=1; i<road.points.size(); ++i)
    {
        const auto a=road.points[i-1], b=road.points[i];
        const float dx=b.x-a.x, dy=b.y-a.y, length=dx*dx+dy*dy;
        const float t=length>.000001f ? std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length,0.f,1.f) : 0.f;
        const MapFeaturePoint q{a.x+dx*t,a.y+dy*t};
        const float d=mapFeaturePointDistanceSquared(p,q);
        if(d<distance) { distance=d; best=q; }
    }
    return best;
}

inline void connectEditedMapRoad(MapModel &map, int index, const MapGeneratedFeature &before, float roadSnapRadius=.65f)
{
    for (bool last : {false,true})
    {
        auto road=map.generation.graph.features[index];
        const int end=last?static_cast<int>(road.points.size())-1:0;
        const int oldEnd=last?static_cast<int>(before.points.size())-1:0;
        if(end>=static_cast<int>(road.points.size())) continue;
        const auto position=road.points[end];
        MapFeaturePoint target=position;
        float nearest=1.25f*1.25f;
        bool cityFound=false;
        const MapGeneratedFeature *oldCity=nullptr;
        for(const auto &city:map.generation.graph.features)
        {
            if(city.kind!=MapGeneratedFeatureKind::Settlement || city.points.empty()) continue;
            if(mapFeaturePointDistanceSquared(before.points[oldEnd],city.points.front())<.01f) oldCity=&city;
            const float d=mapFeaturePointDistanceSquared(position,city.points.front());
            if(d<nearest) { nearest=d; target=city.points.front(); cityFound=true; }
        }
        // Existing town entrances stay attached while the road's body moves.
        if(!cityFound && oldCity) { target=oldCity->points.front(); cityFound=true; }
        if(!cityFound)
        {
            nearest=roadSnapRadius*roadSnapRadius;
            for(const auto &other:map.generation.graph.features)
            {
                if(other.id==road.id || other.points.size()<2 ||
                   (other.kind!=MapGeneratedFeatureKind::Road && other.kind!=MapGeneratedFeatureKind::Trail)) continue;
                const auto q=nearestMapRoadPoint(other,position);
                const float d=mapFeaturePointDistanceSquared(position,q);
                if(d<nearest) { nearest=d; target=q; }
            }
        }
        if(mapFeaturePointDistanceSquared(position,target)>.000001f)
        {
            moveGeneratedMapFeatureSection(map,index,end,target,road);
            // A snap should end in a clean approach, not leave a tiny hook
            // curling back across the host road. Retain control count so
            // attached junctions keep their control-point correspondence.
            auto &joined=map.generation.graph.features[index];
            if(joined.points.size()>2 && !mapGeneratedFeatureIsLocked(map,joined))
            {
                const int step=end==0?1:-1;
                const auto inner=joined.points[end+step*2];
                auto &approach=joined.points[end+step];
                const float dx=inner.x-target.x,dy=inner.y-target.y;
                if(mapFeaturePointDistanceSquared(approach,target)<.75f*.75f &&
                   (approach.x-target.x)*dx+(approach.y-target.y)*dy<0.f)
                    approach={target.x+dx*.3f,target.y+dy*.3f};
            }
        }
    }
}

