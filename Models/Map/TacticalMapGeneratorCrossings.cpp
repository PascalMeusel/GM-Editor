#include "Models/Map/TacticalMapGenerator.hpp"
#include "Models/Map/MapTileSemantics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "Models/Map/TacticalMapGeneratorInternal.hpp"

namespace TacticalMapGeneratorInternal
{
void composeRiverCrossing(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Empty);
    const bool v8 = map.generation.recipe.generatorVersion >= 8;
    const bool v10 = map.generation.recipe.generatorVersion >= 10;
    const bool v11 = map.generation.recipe.generatorVersion >= 11;
    if (v8)
    {
        // Bank groves screen the longer footpath; the wagon bridge remains
        // exposed. These are actual LOS/cover cells, not decorative stamps.
        paintEllipse(map,map.width/4,v10?std::max(2,tacticalCrossingUpperY(map)-3):2,std::max(3,map.width/(v11?4+static_cast<int>((seed>>3)%3u):5)),std::max(2,map.height/(v11?3+static_cast<int>((seed>>5)%3u):4)),MapTileKind::Forest,seed,true);
        paintEllipse(map,map.width*3/4,v10?std::min(map.height-2,tacticalCrossingLowerY(map)+4):map.height-2,std::max(3,map.width/(v11?4+static_cast<int>((seed>>8)%3u):4)),std::max(2,map.height/(v11?3+static_cast<int>((seed>>10)%3u):4)),MapTileKind::Forest,seed^91u,true);
        paintEllipse(map,map.width/2-(v10?std::max(5,map.width/10):5),map.height/2,v10?std::max(3,map.width/10):3,v10?std::max(3,map.height/10):3,MapTileKind::Forest,seed^183u,true);
        paintEllipse(map,map.width/2+5,map.height/2+1,3,2,MapTileKind::Hill,seed^713u,true);
    }
    const int upper = tacticalCrossingUpperY(map);
    const int lower = tacticalCrossingLowerY(map);
    const bool causeway = v10 && ((seed >> 7) % 3u != 0u);
    const float amplitude=map.generation.recipe.generatorVersion>=12?std::clamp(map.width*.15f,3.f,12.f):v11?std::clamp(map.width/10.f,2.5f,8.f):(v10?std::clamp(map.width/16.f,2.3f,5.f):2.3f);
    const float frequency=v10?3.3f+(seed%20u)*.08f:4.2f;
    const float maximumOffset=v10?std::ceil(amplitude):2.f;
    const auto riverOffset=[&](float y) {return std::clamp(std::sin(y*frequency/std::max(1,map.height-1)+(seed%17)*.11f)*amplitude,-maximumOffset,maximumOffset);};
    std::vector<int> riverXs(static_cast<std::size_t>(map.height), map.width / 2);
    std::vector<Point> riverPoints;
    int riverX = map.width / 2 + static_cast<int>(seed % 3u) - 1;
    for (int y = 0; y < map.height; ++y)
    {
        if (y > 0 && tacticalHash(seed, y, riverX, 0x501u) % 7u == 0u)
            riverX += (tacticalHash(seed, y, riverX, 0x502u) & 1u) ? 1 : -1;
        if (v8)
            riverX = map.width/2 + static_cast<int>(std::lround(riverOffset(static_cast<float>(y))));
        riverX = std::clamp(riverX, map.width / 2 - static_cast<int>(maximumOffset), map.width / 2 + static_cast<int>(maximumOffset));
        riverXs[static_cast<std::size_t>(y)] = riverX;
        riverPoints.push_back({riverX, y});
        const int halfWidth = map.generation.recipe.generatorVersion>=12 ? 1 : (y + static_cast<int>(seed & 3u)) % 9 == 0 ? 2 : 1;
        for (int x = riverX - halfWidth; x <= riverX + halfWidth; ++x)
            setCell(map, x, y, MapTileKind::Water, MapWayKind::River);
    }
    addFeature(map, MapGeneratedFeatureKind::River, "river", "Crossing River", "fantasy_neutral.river.medium", 1.65f, riverPoints);
    if (v8)
        for (std::size_t y = 0; y < map.generation.graph.features.back().points.size(); ++y)
            if (static_cast<int>(y) != upper && static_cast<int>(y) != lower)
                map.generation.graph.features.back().points[y].x = map.width/2+.5f+
                    riverOffset(static_cast<float>(y));

    auto bridgeRoute = [&](int index, int y) {
        const int crossingX = riverXs[static_cast<std::size_t>(y)];
        // Keep the editable road spline perpendicular while it actually spans
        // the channel. A single control point in the river could make a short
        // map's spline appear to run along a diagonal river bank.
        const int approachY = std::clamp(map.height / 2 + static_cast<int>(seed % 3u) - 1, 2, map.height - 3);
        std::vector<Point> points{{1, approachY},
                                  {map.width / 4, y + (index == 0 ? -1 : 1)},
                                  {crossingX - 3, y},
                                  {crossingX + 3, y},
                                  {map.width * 3 / 4, y + (index == 0 ? 1 : -1)},
                                  {map.width - 2, approachY}};
        if (v8)
        {
            const int forkX = std::max(3,v10?map.width/5-1:map.width/4-1), joinX = std::min(map.width-4,v10?map.width*4/5+1:map.width*3/4+1);
            if (index == 0)
                points = v10 ? std::vector<Point>{{forkX,lower},{forkX+2,lower-2},{std::max(forkX+3,crossingX-8),upper+2},
                          {crossingX-5,upper+1},{crossingX-3,upper},{crossingX+3,upper},
                          {std::min(joinX-3,crossingX+8),upper+2},{joinX-1,lower-2},{joinX,lower-1}} :
                          std::vector<Point>{{forkX,lower},{forkX-1,upper+2},{crossingX-4,upper},{crossingX+3,upper},
                          {joinX-1,upper+2},{joinX,lower-1}};
            else
                points = {{0,lower+1},{forkX,lower},{crossingX-3,lower},{crossingX+3,lower},
                          {joinX,lower-1},{map.width-1,lower-2}};
        }
        if(v10 && map.generation.recipe.recipeVersion>=6 && index==0 && map.width>=20 && map.height>=16) {
            // Three distinct route relationships: a loop, a fork with separate
            // exits, or independent bridge approaches. Keep the channel anchors.
            const unsigned layout=tacticalHash(seed,7,87)%3u;
            if(layout==1u) {
                points.resize(6);
                points.push_back({map.width*4/5,upper+1});
                points.push_back({map.width-1,upper+2});
            } else if(layout==2u) {
                points={{0,upper-2},{map.width/5,upper-1},{crossingX-3,upper},
                        {crossingX+3,upper},{map.width*4/5,upper+1},{map.width-1,upper+2}};
            }
        }
        if(v11 && index==1 && map.width>=48) {
            // Long wagon approaches should follow the terrain rather than a
            // ruler. Keep bridge anchors perpendicular and the fork/join
            // shared with the side trail; bend only the dry inland spans.
            const int forkX=std::max(3,map.width/5-1),joinX=std::min(map.width-4,map.width*4/5+1);
            const int bend=std::clamp(map.height/22,1,3);
            points={{0,lower+1},{forkX,lower}};
            const auto appendInlandBend=[&](int ax,int ay,int bx,int by,int amplitude) {
                const int steps=std::max(4,(bx-ax)/2);
                for(int i=1;i<=steps;++i) {
                    const float t=i/static_cast<float>(steps);
                    const int x=ax+static_cast<int>(std::lround((bx-ax)*t));
                    const int y=std::min(map.height-3,static_cast<int>(std::lround(ay+(by-ay)*t+
                        amplitude*.5f*(1.f-std::cos(t*6.2831853f)))));
                    if(points.back().x!=x || points.back().y!=y) points.push_back({x,y});
                }
            };
            appendInlandBend(forkX,lower,crossingX-3,lower,bend);
            points.push_back({crossingX+3,lower});
            appendInlandBend(crossingX+3,lower,joinX,lower-1,std::max(1,bend-static_cast<int>(seed%2u)));
            points.push_back({map.width-1,lower-2});
        }
        carveRoute(map, "crossing_route_" + std::to_string(index + 1), "Tactical Route " + std::to_string(index + 1),
                   points, v8 && index == 1 ? 1 : 0, MapTileKind::Dirt, v8 && index == 0 ? MapWayKind::Trail : MapWayKind::DirtRoad, true);
        for (int x = crossingX - 2; x <= crossingX + 2; ++x)
            setCell(map, x, y, MapTileKind::Bridge, MapWayKind::DirtRoad);
        addFeature(map, MapGeneratedFeatureKind::Bridge, "bridge_" + std::to_string(index + 1),
                   v10 ? (index == 0 ? (causeway ? "Old Stone Causeway" : "Woodland Footbridge") : "Wagon Bridge") : (index == 0 ? "Upper Bridge" : "Lower Bridge"),
                   causeway && index == 0 ? "fantasy_neutral.tactical.bridge.stone_causeway" : "fantasy_neutral.bridge.wood",
                   v8 && index == 0 ? .65f : 1.0f, {{crossingX - 2, y}, {crossingX + 2, y}});
        addRouteZone(map, index, y);
    };
    bridgeRoute(0, upper);
    bridgeRoute(1, lower);
    addChokepoint(map, riverXs[static_cast<std::size_t>(upper)], upper, "The upper bridge is a narrow but direct crossing");
}

void composeRuin(MapModel &map, std::uint32_t seed)
{
    fill(map, MapTileKind::Dirt);
    const int upper = std::max(2, map.height / 3);
    const int lower = std::min(map.height - 3, map.height * 2 / 3);
    const int left = std::max(5, map.width / 3);
    const int right = std::min(map.width - 6, map.width * 2 / 3);
    const int top = std::max(2, map.height / 4);
    const int bottom = std::min(map.height - 3, map.height * 3 / 4);
    paintRect(map, left, top, right - left + 1, bottom - top + 1, MapTileKind::Stone);
    for (int x = left; x <= right; ++x)
    {
        if (x != map.width / 2 && x != map.width / 2 + 1)
        {
            setCell(map, x, top, MapTileKind::Wall);
            if ((x + static_cast<int>(seed)) % 5 != 0)
                setCell(map, x, bottom, MapTileKind::Wall);
        }
    }
    for (int y = top; y <= bottom; ++y)
    {
        if (y != upper && y != lower)
        {
            setCell(map, left, y, MapTileKind::Wall);
            if ((y + static_cast<int>(seed)) % 6 != 0)
                setCell(map, right, y, MapTileKind::Wall);
        }
    }
    carveRoute(map, "ruin_courtyard", "Tactical Route 1", {{1, lower}, {left - 1, lower}, {map.width / 2, map.height / 2}, {right + 1, upper}, {map.width - 2, upper}},
               0, MapTileKind::Stone, MapWayKind::Trail);
    carveRoute(map, "ruin_flank", "Tactical Route 2", {{1, upper}, {left - 2, top - 1}, {right + 2, top - 1}, {map.width - 2, lower}},
               0, MapTileKind::Dirt, MapWayKind::Trail);
    connectRouteEnds(map, upper, lower, MapTileKind::Dirt, MapWayKind::Trail);
    setCell(map, left + 1, map.height / 2, MapTileKind::Rubble);
    setCell(map, right - 1, map.height / 2, MapTileKind::Rubble);
    setCell(map, map.width / 2, top + 1, MapTileKind::Hazard);
    addChokepoint(map, map.width / 2, top, "Broken arch into the ruined courtyard");
    addRouteZone(map, 0, lower);
    addRouteZone(map, 1, upper);
}

void composeRuinV8(MapModel &map, std::uint32_t seed)
{
    fill(map,MapTileKind::Empty);
    const bool v9=map.generation.recipe.generatorVersion>=9;
    const int variant=v9?static_cast<int>((seed>>5)%3):0;
    const int left=std::max(5,map.width/4-(variant==1?1:0)), right=std::min(map.width-5,map.width*3/4+(variant==2?1:0));
    const int top=std::max(3,map.height/4-(v9?static_cast<int>((seed>>9)%3):0)),
              bottom=std::min(map.height-3,map.height*3/4+(variant==1?1:0));
    const int centre=map.height/2;
    paintEllipse(map,2,2,std::max(3,map.width/5),std::max(2,map.height/4),MapTileKind::Forest,seed,true);
    paintEllipse(map,map.width-3,map.height-2,std::max(3,map.width/5),std::max(2,map.height/4),MapTileKind::Forest,seed^187u,true);
    paintRect(map,left,top,right-left+1,bottom-top+1,MapTileKind::Stone);
    // A broken eastern annex makes the former building's footprint legible.
    paintRect(map,right-1,top+2,4,std::max(3,centre-top),MapTileKind::Stone);
    for (int x=left;x<=right;++x)
    {
        if (std::abs(x-map.width/2)>1) setCell(map,x,top,MapTileKind::Wall);
        if (x<left+4 || x>right-3) setCell(map,x,bottom,MapTileKind::Wall);
    }
    for (int y=top+1;y<bottom;++y)
    {
        if (std::abs(y-centre)>1) setCell(map,left,y,MapTileKind::Wall);
        if (y>centre+1 || y<top+2) setCell(map,right,y,MapTileKind::Wall);
    }
    // Broken masonry opens additional sight lines without closing the nave.
    for (int x : {left+2,right-3})
    {
        setCell(map,x,top,MapTileKind::Rubble);
        setCell(map,x,top+1,MapTileKind::Rubble);
    }
    setCell(map,left,top+2,MapTileKind::Rubble);
    setCell(map,right,bottom-2,MapTileKind::Rubble);
    // Four surviving pillars provide readable firing lanes and flank cover;
    // the central aisle remains wide enough for larger creature footprints.
    for (int x : {left+4,right-4})
        for (int y : {top+3,bottom-3})
        {
            setCell(map,x,y,MapTileKind::Wall);
            setCell(map,x+1,y,MapTileKind::Rubble);
        }
    for (int x=map.width/2-1;x<=map.width/2+1;++x)
        setCell(map,x,top+1,MapTileKind::Rubble);
    setCell(map,right+1,top+3,MapTileKind::Wall);
    setCell(map,right+2,top+4,MapTileKind::Rubble);
    setCell(map,left+2,bottom-1,MapTileKind::Rubble);
    if (v9)
    {
        // Different surviving masonry changes the cover and flanking lanes,
        // without filling the central aisle or either deployment half.
        const int brokenX=variant==1?left+4:right-4;
        setCell(map,brokenX,top+3,MapTileKind::Rubble);
        if (variant==2)
        {
            for (int x=left+1;x<left+4;++x) setCell(map,x,bottom-2,MapTileKind::Wall);
            setCell(map,left,bottom-1,MapTileKind::Rubble);
        }
        else
        {
            const int annexX=variant==1?left+2:right-2;
            for (int y=top+2;y<top+4;++y) setCell(map,annexX,y,MapTileKind::Wall);
            setCell(map,annexX,top+4,MapTileKind::Rubble);
        }
    }
    carveRoute(map,"ruin_aisle","Tactical Route 1",{{0,centre+1},{left-2,centre},{right+2,centre},{map.width-1,centre-1}},
               0,MapTileKind::Stone,MapWayKind::Trail);
    carveRoute(map,"ruin_outer","Tactical Route 2",{{left-2,centre},{left-3,top-1},{left,top-2},
               {right+1,top-2},{right+3,top+1},{right+2,centre}},0,MapTileKind::Dirt,MapWayKind::Trail);
    addChokepoint(map,left,centre,"Broken nave entrance with pillars and a covered outer approach");
    addRouteZone(map,0,centre); addRouteZone(map,1,top-2);
}
}
