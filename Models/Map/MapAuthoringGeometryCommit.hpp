#pragma once

#include "Models/Map/MapAuthoringClassification.hpp"

inline bool finishGeneratedMapFeatureGeometryEdit(MapModel &map, const MapGeneratedFeature &before)
{
    if (map.width <= 0 || map.height <= 0) return false;
    const auto found = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
                                   [&](const auto &feature) { return feature.id == before.id; });
    if (found == map.generation.graph.features.end() || before.points.empty() || found->points.empty()) return false;
    if (before.kind == MapGeneratedFeatureKind::BuildingGroup && map.generation.recipe.generatorVersion >= 10)
        return moveGeneratedBuildingFootprint(map,before,*found);
    if (mapGeneratedPointFeatureTileKind(before) == MapTileKind::Empty)
    {
        if (before.kind == MapGeneratedFeatureKind::Road || before.kind == MapGeneratedFeatureKind::Trail)
        {
            auto original = map.generation.graph.features;
            original[static_cast<std::size_t>(found - map.generation.graph.features.begin())] = before;
            connectEditedMapRoad(map,static_cast<int>(found-map.generation.graph.features.begin()),before);
            reconnectGeneratedRoadJunctions(map, original);
            // Reconnection can use control correspondence only when the
            // counts agree. A repaired/drawn path can have a new count and
            // still needs its old raster footprint removed and reprojected.
            if(found->points.size()!=before.points.size())
                reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map,before);
        }
        else
            reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map, before);
        return true;
    }
    const MapFeaturePoint oldPoint = before.points.front(), newPoint = found->points.front();
    if(before.kind==MapGeneratedFeatureKind::Settlement && mapFeaturePointDistanceSquared(oldPoint,newPoint)>.000001f)
    {
        const auto *ground=mapCellAt(map,static_cast<int>(std::floor(newPoint.x)),static_cast<int>(std::floor(newPoint.y)));
        if(!ground || ground->kind==MapTileKind::Water || ground->kind==MapTileKind::Mountain ||
           ground->kind==MapTileKind::Building || ground->kind==MapTileKind::Wall || ground->kind==MapTileKind::Lava ||
           ground->kind==MapTileKind::Pit || ground->kind==MapTileKind::Chasm)
        { *found=before; return false; }
    }
    if(before.kind==MapGeneratedFeatureKind::Settlement)
        for(const auto &other:map.generation.graph.features)
            if(other.id!=before.id && other.kind==MapGeneratedFeatureKind::Settlement && !other.points.empty() &&
               mapFeaturePointDistanceSquared(newPoint,other.points.front())<1.0f &&
               mapFeaturePointDistanceSquared(oldPoint,other.points.front())>=1.0f)
            { *found=before; return false; }
    const int oldX = std::clamp(static_cast<int>(std::floor(oldPoint.x)), 0, map.width - 1);
    const int oldY = std::clamp(static_cast<int>(std::floor(oldPoint.y)), 0, map.height - 1);
    const int newX = std::clamp(static_cast<int>(std::floor(newPoint.x)), 0, map.width - 1);
    const int newY = std::clamp(static_cast<int>(std::floor(newPoint.y)), 0, map.height - 1);
    if (oldX != newX || oldY != newY)
    {
        const auto ground = mapVisualGroundKind(map, oldX, oldY);
        if (auto *cell = mapCellAt(map, oldX, oldY); cell && cell->kind == mapGeneratedPointFeatureTileKind(before))
            cell->kind = ground;
        // Another place may share this cell. Moving one must not erase the
        // other's glyph or any gameplay flags stored alongside the tile kind.
        for (const auto &other : map.generation.graph.features)
            if (other.id != before.id && !other.points.empty() &&
                static_cast<int>(std::floor(other.points.front().x)) == oldX &&
                static_cast<int>(std::floor(other.points.front().y)) == oldY)
                projectGeneratedPointFeatureClassificationToCells(map, other);
        projectGeneratedPointFeatureClassificationToCells(map, *found);
    }
    if (before.kind != MapGeneratedFeatureKind::Settlement) return true;
    const bool sharedOldCell=std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &other){
        return other.id!=before.id && other.kind==MapGeneratedFeatureKind::Settlement && !other.points.empty() &&
            static_cast<int>(std::floor(other.points.front().x))==oldX && static_cast<int>(std::floor(other.points.front().y))==oldY;
    });
    if(!sharedOldCell)
        for(auto &route:map.routes)
        {
            if((route.fromMapId.empty() || route.fromMapId==map.metadata.id) && route.startX==oldX && route.startY==oldY)
            { route.startX=newX; route.startY=newY; }
            if((route.toMapId.empty() || route.toMapId==map.metadata.id) && route.endX==oldX && route.endY==oldY)
            { route.endX=newX; route.endY=newY; }
        }
    const auto originalRoads = map.generation.graph.features;
    for (int i = 0; i < static_cast<int>(map.generation.graph.features.size()); ++i)
    {
        const auto road = map.generation.graph.features[i];
        if (mapGeneratedFeatureIsLocked(map,road) || road.points.size() < 2 ||
            (road.kind != MapGeneratedFeatureKind::Road && road.kind != MapGeneratedFeatureKind::Trail)) continue;
        bool attached=false;
        for (int endpoint : {0, static_cast<int>(road.points.size()) - 1})
        {
            if (mapFeaturePointDistanceSquared(road.points[endpoint], oldPoint) > .01f) continue;
            moveGeneratedMapFeatureSection(map, i, endpoint,
                {road.points[endpoint].x + newPoint.x - oldPoint.x, road.points[endpoint].y + newPoint.y - oldPoint.y}, road);
            attached=true;
            break;
        }
        if(!attached && mapFeaturePointDistanceSquared(nearestMapRoadPoint(road,oldPoint),oldPoint)<.0025f)
        {
            int point=-1;
            for(int p=1;p+1<static_cast<int>(road.points.size());++p)
                if(mapFeaturePointDistanceSquared(road.points[p],oldPoint)<.0025f) { point=p; break; }
            if(point<0)
                for(int p=0;p+1<static_cast<int>(road.points.size());++p)
                {
                    MapGeneratedFeature segment;segment.points={road.points[p],road.points[p+1]};
                    const auto q=nearestMapRoadPoint(segment,oldPoint);
                    if(mapFeaturePointDistanceSquared(q,oldPoint)>.0025f) continue;
                    const float length=std::sqrt(mapFeaturePointDistanceSquared(segment.points.front(),segment.points.back()));
                    if(length<.001f) continue;
                    point=insertGeneratedMapFeaturePoint(map.generation.graph.features[i],p,
                        std::sqrt(mapFeaturePointDistanceSquared(segment.points.front(),q))/length);
                    break;
                }
            if(point>=0)
            {
                const auto controls=map.generation.graph.features[i];
                moveGeneratedMapFeatureSection(map,i,point,newPoint,controls);
                reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map,road);
            }
        }
    }
    reconnectGeneratedRoadJunctions(map, originalRoads);
    // Add one short approach when an unconnected town is placed beside a road.
    // Preserve the through-road instead of dragging its centre under the town.
    bool connected=false;
    float nearest=1.25f*1.25f;
    MapFeaturePoint junction=newPoint;
    for(const auto &road:map.generation.graph.features)
    {
        if(road.points.size()<2 || (road.kind!=MapGeneratedFeatureKind::Road && road.kind!=MapGeneratedFeatureKind::Trail)) continue;
        connected |= mapFeaturePointDistanceSquared(newPoint,road.points.front())<.01f ||
                     mapFeaturePointDistanceSquared(newPoint,road.points.back())<.01f;
        const auto q=nearestMapRoadPoint(road,newPoint);
        const float d=mapFeaturePointDistanceSquared(q,newPoint);
        if(d<nearest) { nearest=d; junction=q; }
    }
    if(!connected && nearest<1.25f*1.25f && nearest>.01f)
    {
        MapGeneratedFeature approach;
        approach.id="manual_way_city_access_"+before.id+"_"+std::to_string(map.generation.graph.features.size());
        approach.name="Town approach"; approach.kind=MapGeneratedFeatureKind::Road;
        approach.materialId="fantasy_neutral.road.dirt"; approach.width=.65f;
        approach.gmOnly=before.gmOnly; approach.discovered=before.discovered;
        approach.points={junction,newPoint};
        map.generation.graph.features.push_back(approach);
        projectGeneratedWayFeatureToCells(map,approach,false);
    }
    return true;
}

inline bool applyGeneratedMapFeatureClassification(
    MapModel &map,
    int featureIndex,
    MapGeneratedFeatureKind requestedKind)
{
    if (featureIndex < 0 || featureIndex >= static_cast<int>(map.generation.graph.features.size()))
        return false;
    MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
    if (feature.locked || feature.kind == requestedKind)
        return false;
    const std::vector<MapGeneratedFeatureClassificationOption> options =
        mapGeneratedFeatureClassificationOptions(feature.kind);
    if (std::none_of(options.begin(), options.end(), [&](const auto &option) {
            return option.kind == requestedKind;
        }))
        return false;

    const MapGeneratedFeature beforeEdit = feature;
    feature.kind = requestedKind;
    const std::vector<MapGeneratedFeatureAppearanceOption> profiles =
        mapGeneratedFeatureAppearanceOptions(feature);
    if (!profiles.empty())
    {
        feature.materialId = profiles.front().materialId;
        if (profiles.front().recommendedWidth > 0.0f)
            feature.width = profiles.front().recommendedWidth;
    }
    reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map, beforeEdit);
    projectGeneratedPointFeatureClassificationToCells(map, feature);
    return true;
}

inline bool applyGeneratedMapFeatureAppearance(
    MapModel &map,
    int featureIndex,
    const std::string &requestedMaterialId)
{
    if (featureIndex < 0 || featureIndex >= static_cast<int>(map.generation.graph.features.size()))
        return false;
    MapGeneratedFeature &feature = map.generation.graph.features[static_cast<std::size_t>(featureIndex)];
    if (feature.locked || feature.materialId == requestedMaterialId)
        return false;
    const std::vector<MapGeneratedFeatureAppearanceOption> options =
        mapGeneratedFeatureAppearanceOptions(feature);
    const auto requested = std::find_if(options.begin(), options.end(), [&](const auto &option) {
        return option.materialId == requestedMaterialId;
    });
    if (requested == options.end())
        return false;

    const MapGeneratedFeature beforeEdit = feature;
    feature.materialId = requested->materialId;
    if (requested->recommendedWidth > 0.0f)
        feature.width = requested->recommendedWidth;
    reprojectGeneratedMapWayFeaturesAfterGeometryEdit(map, beforeEdit);
    projectGeneratedPointFeatureClassificationToCells(map, feature);
    return true;
}

