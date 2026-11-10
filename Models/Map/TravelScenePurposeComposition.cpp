#include "Models/Map/TravelSceneGenerator.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include "Models/Map/TravelSceneInternal.hpp"


namespace TravelSceneGenerator::detail
{
void composeTravelPurpose(MapModel &map, const TravelSceneRecipe &recipe, std::uint32_t seed)
{
    const bool german = usesGermanNarrative(recipe);
    int centerX = map.width / 2 + static_cast<int>(seed % 5u) - 2;
    int centerY = map.height / 2 + static_cast<int>((seed / 7u) % 5u) - 2;
    // Purpose markers are composed after terrain and hydrology. Move a focus
    // that landed on water to the nearest usable land cell instead of erasing
    // part of a generated river (which also invalidated its downhill profile).
    std::vector<MapCellCoordinate> routeCells;
    routeCells.reserve(static_cast<std::size_t>(map.width + map.height));
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x)
            if (const MapCellModel *cell = mapCellAt(map, x, y);
                cell && mapGenWayKindIsRoadLike(cell->wayKind))
                routeCells.push_back({x, y});

    int nearestLandDistance = std::numeric_limits<int>::max();
    MapCellCoordinate nearestLand{centerX, centerY};
    for (int y = 1; y < map.height - 1; ++y)
    {
        for (int x = 1; x < map.width - 1; ++x)
        {
            const MapCellModel *cell = mapCellAt(map, x, y);
            if (!cell || mapGenRiverInfluenceCell(cell) || cell->kind == MapTileKind::Bridge ||
                cell->movementCost >= 99 || mapGenIsStructureLike(cell->kind) ||
                (!routeCells.empty() && cell->wayKind != MapWayKind::None))
                continue;
            if(recipe.schemaVersion>=3 && recipe.purpose!=TravelScenePurpose::Obstacle) {
                bool footprintClear=true;
                for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox) {
                    const auto *adjacent=mapCellAt(map,x+ox,y+oy);
                    footprintClear=footprintClear && adjacent && !mapGenRiverInfluenceCell(adjacent) &&
                        adjacent->kind!=MapTileKind::Bridge && adjacent->movementCost<99 &&
                        !mapGenIsStructureLike(adjacent->kind) && adjacent->kind!=MapTileKind::Landmark;
                }
                if(!footprintClear)continue;
            }
            const int distanceFromIdeal = std::abs(x - centerX) + std::abs(y - centerY);
            int routeDistance = 0;
            if (!routeCells.empty())
            {
                routeDistance = map.width + map.height;
                for (const MapCellCoordinate &routePoint : routeCells)
                    routeDistance = std::min(routeDistance,
                                             std::abs(routePoint.x - x) +
                                                 std::abs(routePoint.y - y));
            }
            // Travel scenes should happen beside the route, not in an
            // arbitrary empty cell near the mathematical map centre.  Prefer
            // a one- or two-cell verge while retaining a deterministic centre
            // bias for equally good locations.
            const int score = distanceFromIdeal * 3 +
                              (routeCells.empty()
                                   ? 0
                                   : std::abs(routeDistance - 2) * 7 +
                                         std::max(0, routeDistance - 4) * 20);
            if (score < nearestLandDistance)
            {
                nearestLandDistance = score;
                nearestLand = {x, y};
            }
        }
    }
    centerX = nearestLand.x;
    centerY = nearestLand.y;
    if(recipe.schemaVersion>=3 && recipe.purpose==TravelScenePurpose::Obstacle) {
        int nearest=map.width+map.height;
        for(const auto &p:routeCells) {
            const auto *c=mapCellAt(map,p.x,p.y);
            if(!c || mapGenRiverInfluenceCell(c) || c->kind==MapTileKind::Bridge || mapGenIsStructureLike(c->kind))continue;
            const int distance=std::abs(p.x-nearestLand.x)+std::abs(p.y-nearestLand.y);
            if(distance<nearest){nearest=distance;centerX=p.x;centerY=p.y;}
        }
    }
    MapGeneratedFeature focus;
    focus.id = map.metadata.id + "::travel_focus";
    focus.kind = MapGeneratedFeatureKind::Landmark;
    focus.width = 1.0f;
    focus.locked = true;
    focus.points.push_back({centerX + 0.5f, centerY + 0.5f});
    const bool regionalOverview = map.atlasLevel == MapLevel::Region || map.atlasLevel == MapLevel::World;
    auto connectToTravelNetwork = [&](int originX, int originY) {
        MapCellCoordinate nearest{originX, originY};
        int bestDistance = map.width + map.height;
        for (int y = 0; y < map.height; ++y)
        {
            for (int x = 0; x < map.width; ++x)
            {
                const MapCellModel *cell = mapCellAt(map, x, y);
                if (!cell || !mapGenWayKindIsRoadLike(cell->wayKind))
                    continue;
                const int distance = std::abs(x - originX) + std::abs(y - originY);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    nearest = {x, y};
                }
            }
        }
        if (bestDistance >= map.width + map.height)
            nearest = {std::clamp(originX + 4, 0, map.width - 1), originY};
        const std::vector<MapCellCoordinate> path = mapGenV2FindRoadPath(map, {originX, originY}, nearest);
        for (const MapCellCoordinate &point : path)
        {
            MapCellModel *cell = mapCellAt(map, point.x, point.y);
            if (!cell || cell->kind == MapTileKind::Settlement)
                continue;
            if (cell->kind == MapTileKind::Water || cell->wayKind == MapWayKind::River)
            {
                const int waterwayElevation = cell->elevation;
                applyMapCellTerrainDefaults(*cell, MapTileKind::Bridge);
                cell->elevation = waterwayElevation;
            }
            cell->wayKind = MapWayKind::DirtRoad;
            cell->movementCost = 1;
            cell->movementBlockedMask = 0;
        }
        if (!mapGenCellHasStructureApproach(map, originX, originY))
        {
            // A diagonal first path step is connected for routing but is not a
            // readable building entrance. Guarantee one cardinal approach.
            const int directionX = nearest.x == originX ? 0 : (nearest.x > originX ? 1 : -1);
            const int directionY = directionX == 0 && nearest.y != originY ? (nearest.y > originY ? 1 : -1) : 0;
            MapCellModel *approach = mapCellAt(map, originX + directionX, originY + directionY);
            if (approach)
            {
                if (approach->kind == MapTileKind::Water || approach->wayKind == MapWayKind::River)
                {
                    const int waterwayElevation = approach->elevation;
                    applyMapCellTerrainDefaults(*approach, MapTileKind::Bridge);
                    approach->elevation = waterwayElevation;
                }
                else if (mapGenIsStructureLike(approach->kind))
                    applyMapCellTerrainDefaults(*approach, MapTileKind::Dirt);
                approach->wayKind = MapWayKind::DirtRoad;
                approach->movementCost = 1;
                approach->movementBlockedMask = 0;
            }
        }
    };
    switch (recipe.purpose)
    {
    case TravelScenePurpose::Quiet:
        focus.name = german ? "Geschützter Rastplatz" : "Sheltered Rest";
        focus.materialId = "fantasy_neutral.travel.rest";
        for (int y = centerY - 2; y <= centerY + 2; ++y)
            for (int x = centerX - 2; x <= centerX + 2; ++x)
                // A rest clearing may simplify surrounding vegetation, but it
                // must not erase a route, bridge, or sampled river cell. The
                // feature graph retains those points and their downhill order.
                if (MapCellModel *cell = mapCellAt(map, x, y);
                    cell && (recipe.schemaVersion < 3 || (x-centerX)*(x-centerX)+(y-centerY)*(y-centerY)<=5) &&
                    cell->kind != MapTileKind::Water && cell->kind != MapTileKind::Bridge &&
                    !mapGenRiverInfluenceCell(cell) && cell->wayKind == MapWayKind::None)
                    applyMapCellTerrainDefaults(*cell, MapTileKind::Dirt);
        break;
    case TravelScenePurpose::Discovery:
        focus.name = german ? "Entdeckung" : "Discovery";
        focus.materialId = "fantasy_neutral.travel.discovery";
        mapGenSetCell(map, centerX, centerY, MapTileKind::Landmark);
        break;
    case TravelScenePurpose::Social:
        focus.kind = MapGeneratedFeatureKind::Settlement;
        focus.name = german ? "Treffpunkt" : "Meeting Place";
        focus.materialId = "fantasy_neutral.travel.social";
        mapGenSetCell(map, centerX, centerY, MapTileKind::Settlement);
        connectToTravelNetwork(centerX, centerY);
        // Social scenes can contain a supporting hamlet from the landscape
        // pass. Connect it as well so the visible place never floats beside
        // the travel network without an entrance.
        for (int y = 0; y < map.height; ++y)
            for (int x = 0; x < map.width; ++x)
                if (const MapCellModel *cell = mapCellAt(map, x, y);
                    cell && cell->kind == MapTileKind::Settlement && !mapGenCellHasStructureApproach(map, x, y))
                    connectToTravelNetwork(x, y);
        break;
    case TravelScenePurpose::Obstacle:
        focus.name = german ? "Reisehindernis" : "Route Obstacle";
        focus.materialId = "fantasy_neutral.travel.obstacle";
        for (int offset = -3; !regionalOverview && offset <= 3; ++offset)
        {
            if (offset == 0)
                continue;
            if (MapCellModel *cell = mapCellAt(map, centerX, centerY + offset); cell && !mapGenRiverInfluenceCell(cell))
            {
                applyMapCellTerrainDefaults(*cell, MapTileKind::Rubble);
                cell->difficultTerrain = true;
                cell->blocksLineOfSight = std::abs(offset) >= 2;
            }
        }
        mapGenSetCell(map, centerX, centerY, MapTileKind::Landmark);
        break;
    case TravelScenePurpose::Danger:
        focus.name = german ? "Gefahrenstelle" : "Danger Site";
        focus.materialId = "fantasy_neutral.travel.danger";
        for (int index = 0; !regionalOverview && index < 6; ++index)
        {
            const int x = std::clamp(centerX + static_cast<int>((seed + index * 11u) % 7u) - 3, 1, map.width - 2);
            const int y = std::clamp(centerY + static_cast<int>((seed + index * 17u) % 7u) - 3, 1, map.height - 2);
            if (MapCellModel *cell = mapCellAt(map, x, y); cell && cell->wayKind == MapWayKind::None)
            {
                applyMapCellTerrainDefaults(*cell, index % 3 == 0 ? MapTileKind::Hazard : MapTileKind::Cover);
                cell->blocksLineOfSight = index % 2 == 0;
            }
        }
        break;
    }
    // Point features must have a matching semantic raster projection.  Quiet
    // rests and danger sites previously existed only in the graph, so the
    // shared render plan correctly suppressed them and the resulting scene
    // looked empty.  Keep social meetings as settlements; every other purpose
    // receives one visible, editable landmark anchor next to the route.
    if (focus.kind == MapGeneratedFeatureKind::Landmark)
        mapGenSetCell(map, centerX, centerY, MapTileKind::Landmark,
                      MapSymbolVariantMode::Individual);
    if(recipe.schemaVersion>=3 && recipe.purpose==TravelScenePurpose::Obstacle) {
        auto *anchor=mapCellAt(map,centerX,centerY);
        const auto road=[&](int x,int y){const auto *c=mapCellAt(map,x,y);return c && mapGenWayKindIsRoadLike(c->wayKind);};
        if(anchor){anchor->difficultTerrain=true;anchor->movementCost=2;anchor->rotationQuarterTurns=road(centerX-1,centerY)+road(centerX+1,centerY)>=road(centerX,centerY-1)+road(centerX,centerY+1)?1:0;}
    }
    map.generation.graph.features.push_back(std::move(focus));
    if(recipe.schemaVersion>=3) {
        map.metadata.customProperties["travel.schema"]=recipe.schemaVersion >= 4 ? "4" : "3";
        map.metadata.customProperties["travel.purpose"]=std::to_string(static_cast<int>(recipe.purpose));
        // A close scene needs a readable meeting space and a pass silhouette,
        // rather than peppered props on an oversized landscape.
        for(int y=centerY-1;y<=centerY+1;++y)for(int x=centerX-1;x<=centerX+1;++x) {
            auto *c=mapCellAt(map,x,y);
            if(c && c->kind==MapTileKind::Forest && c->wayKind==MapWayKind::None && !mapGenRiverInfluenceCell(c))
                applyMapCellTerrainDefaults(*c,MapTileKind::Dirt);
        }
        if(landscapeFor(recipe)==MapLandscapeProfile::MountainPass) {
            std::array<MapGeneratedFeature,2> ranges;
            for(int side=0;side<2;++side){ranges[side].id=map.metadata.id+"::travel_ridge_"+std::to_string(side);ranges[side].kind=MapGeneratedFeatureKind::MountainRange;ranges[side].width=1.f;ranges[side].materialId="fantasy_neutral.mountains";}
            for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x) {
                auto *c=mapCellAt(map,x,y);
                if(!c || c->wayKind!=MapWayKind::None || mapGenRiverInfluenceCell(c) || mapGenIsStructureLike(c->kind) || c->kind==MapTileKind::Landmark)continue;
                int roadDistance=map.width+map.height;
                for(const auto &r:routeCells)roadDistance=std::min(roadDistance,std::abs(r.x-x)+std::abs(r.y-y));
                const int crest=3+static_cast<int>(2.f*std::sin(x*.24f+static_cast<float>(seed%17)));
                const int edge=std::min(y,map.height-1-y);
                if(edge<=crest && roadDistance>=4 && std::abs(x-centerX)+std::abs(y-centerY)>5) {
                    applyMapCellTerrainDefaults(*c,edge+1<crest?MapTileKind::Mountain:MapTileKind::Hill);
                    c->elevation=std::max(c->elevation,edge+1<crest?3:1);
                    if(c->kind==MapTileKind::Mountain && edge==1 && x%3==1)ranges[y<map.height/2?0:1].points.push_back({x+.5f,y+.5f});
                }
            }
            for(auto &range:ranges)if(range.points.size()>1)map.generation.graph.features.push_back(std::move(range));
            if(recipe.purpose==TravelScenePurpose::Obstacle) {
                // Frame a pass obstacle with real local rock shoulders. A log
                // in an otherwise empty meadow offers no meaningful decision.
                const auto *focusCell=mapCellAt(map,centerX,centerY);
                const bool horizontal=focusCell && focusCell->rotationQuarterTurns%2;
                for(int side:{-1,1})for(int oy=-7;oy<=7;++oy)for(int ox=-7;ox<=7;++ox) {
                    const float along=horizontal?static_cast<float>(ox):static_cast<float>(oy);
                    const float across=horizontal?static_cast<float>(oy):static_cast<float>(ox);
                    const float offset=across-side*4.f;
                    const float shape=along*along/9.f+offset*offset/3.4f;
                    if(shape>1.f)continue;
                    auto *c=mapCellAt(map,centerX+ox,centerY+oy);
                    if(!c || c->wayKind!=MapWayKind::None || mapGenRiverInfluenceCell(c) || mapGenIsStructureLike(c->kind) || c->kind==MapTileKind::Landmark)continue;
                    applyMapCellTerrainDefaults(*c,shape<.72f?MapTileKind::Mountain:MapTileKind::Hill);
                    c->elevation=shape<.72f?3:1;
                }
            }
        }
    }
}
} // namespace TravelSceneGenerator::detail

