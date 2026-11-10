#pragma once
#include "Models/Map/MapAuthoring.hpp"

inline MapModel crossingEditFixture()
{
    MapModel map;
    map.width=24; map.height=28; resizeMapCells(map);
    map.atlasLevel=MapLevel::World; map.visibility=MapVisibility::PlayersVisible;
    map.generation.continuousRendering=true;
    map.generation.recipe.generatorVersion=12;
    map.generation.recipe.styleId="illustrated_atlas";
    map.generation.recipe.styleVersion=5;
    map.generation.recipe.styleSnapshotId="illustrated_atlas_v5";
    for(auto &cell:map.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
    MapGeneratedFeature road;
    road.id="road-to-move";road.kind=MapGeneratedFeatureKind::Road;
    road.points={{3.5f,.5f},{3.5f,27.5f}};road.width=.82f;
    MapGeneratedFeature river;
    river.id="generated-river";river.kind=MapGeneratedFeatureKind::River;
    river.points={{15.5f,0.f},{15.5f,5.5f},{5.5f,15.5f},{15.5f,25.5f},{15.5f,28.f}};
    river.width=2.f;
    // Deliberately put water after roads in the graph; projection must not
    // inherit this insertion order when a geometry edit is committed.
    map.generation.graph.features={road,river};
    projectGeneratedWayFeatureToCells(map,river,false);
    projectGeneratedWayFeatureToCells(map,road,false);
    // Reproduce the generator's diagonal Water/None helper cells.
    const auto before=map.cells;
    for(int y=0;y<map.height-1;++y)for(int x=0;x<map.width;++x)
        for(int dx:{-1,1})
        {
            if(x+dx<0 || x+dx>=map.width)continue;
            if(before[y*map.width+x].wayKind!=MapWayKind::River ||
               before[(y+1)*map.width+x+dx].wayKind!=MapWayKind::River)continue;
            for(auto p:{MapCellCoordinate{x+dx,y},MapCellCoordinate{x,y+1}})
                if(mapCellAt(map,p.x,p.y)->kind==MapTileKind::Dirt)
                    mapCellAt(map,p.x,p.y)->kind=MapTileKind::Water;
        }
    return map;
}
