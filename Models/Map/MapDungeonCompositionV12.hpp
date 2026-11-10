#pragma once
#include "Models/Map/MapModels.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Arrange existing collision-bearing furniture at useful room edges. This is
// a placement pass, not extra random clutter: the detail slider keeps its count,
// and authored corridor cells plus doorway/stair approaches are never occupied.
inline void mapComposeDungeonFurnitureV12(MapModel &map,int generatorVersion,std::uint32_t seed)
{
    if(generatorVersion<12 || map.creationIntent!=MapCreationIntent::Dungeon)return;
    struct Point {int x,y;int score;};
    std::vector<bool> handled(map.cells.size(),false);
    for(const auto &room:map.generation.graph.features)
    {
        if(room.materialId!="fantasy_neutral.dungeon.room"||room.points.empty())continue;
        float l=room.points.front().x,r=l,t=room.points.front().y,b=t;
        for(auto p:room.points){l=std::min(l,p.x);r=std::max(r,p.x);t=std::min(t,p.y);b=std::max(b,p.y);}
        const int left=std::max(1,static_cast<int>(std::floor(l))),right=std::min(map.width-2,static_cast<int>(std::ceil(r)));
        const int top=std::max(1,static_cast<int>(std::floor(t))),bottom=std::min(map.height-2,static_cast<int>(std::ceil(b)));
        std::vector<Point> furniture,slots;
        const unsigned hash=seed ^ unsigned(left)*73856093u ^ unsigned(top)*19349663u;
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x)
        {
            const auto *c=mapCellAt(map,x,y);const auto index=y*map.width+x;
            if(c->kind==MapTileKind::Cover&&!handled[index]){furniture.push_back({x,y,0});handled[index]=true;}
            if(c->kind!=MapTileKind::Room||c->wayKind!=MapWayKind::None)continue;
            bool clear=true;
            for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox)
            {
                const auto*n=mapCellAt(map,x+ox,y+oy);
                if(!n||n->kind==MapTileKind::Wall||n->kind==MapTileKind::Empty||n->kind==MapTileKind::Door||n->kind==MapTileKind::Stairs||n->wayKind!=MapWayKind::None)clear=false;
            }
            if(!clear)continue;
            const int edge=(hash%4==0?y-top:hash%4==1?right-x:hash%4==2?bottom-y:x-left);
            const int along=(hash%2==0?x-left:y-top);
            slots.push_back({x,y,edge*100+along});
        }
        std::sort(slots.begin(),slots.end(),[](auto a,auto b){return a.score<b.score;});
        std::vector<Point> placed;
        std::size_t moved=0;
        for(auto slot:slots)
        {
            if(moved>=furniture.size())break;
            if(std::any_of(placed.begin(),placed.end(),[&](auto p){return std::abs(p.x-slot.x)+std::abs(p.y-slot.y)<2;}))continue;
            const auto from=furniture[moved++];
            applyMapCellTerrainDefaults(*mapCellAt(map,from.x,from.y),MapTileKind::Room);
            applyMapCellTerrainDefaults(*mapCellAt(map,slot.x,slot.y),MapTileKind::Cover);
            handled[slot.y*map.width+slot.x]=true;placed.push_back(slot);
        }
    }
}
