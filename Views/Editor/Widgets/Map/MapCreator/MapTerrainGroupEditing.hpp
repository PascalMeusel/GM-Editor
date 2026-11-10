#pragma once
#include "Models/Map/MapAuthoring.hpp"

namespace gm::mapui
{
inline bool selectableTerrainKind(MapTileKind kind)
{
    switch(kind) {
    case MapTileKind::Forest: case MapTileKind::Mountain: case MapTileKind::Hill:
    case MapTileKind::Swamp: case MapTileKind::Snow: case MapTileKind::Desert:
    case MapTileKind::Dirt: case MapTileKind::Stone: return true;
    default: return false;
    }
}
struct TerrainGroupSelection
{
    std::vector<MapCellCoordinate> cells;
    std::vector<unsigned char> mask;
    MapTileKind kind=MapTileKind::Empty;
    int width=0, height=0;
    // Remember the connected patch before a box clips it, including after a move.
    std::size_t sourceCellCount=0;
    bool isPartial() const { return cells.size()<sourceCellCount; }
    bool contains(int x,int y) const {
        return x>=0 && y>=0 && x<width && y<height &&
            static_cast<std::size_t>(y*width+x)<mask.size() && mask[y*width+x];
    }
};
inline TerrainGroupSelection terrainSelectionFromCells(const MapModel &map, MapTileKind kind,
                                                       std::vector<MapCellCoordinate> cells)
{
    TerrainGroupSelection result;
    result.width=map.width;result.height=map.height;result.kind=kind;
    result.mask.resize(map.cells.size());
    for(auto p:cells) if(mapCellAt(map,p.x,p.y) && !result.contains(p.x,p.y))
    {result.cells.push_back(p);result.mask[p.y*map.width+p.x]=1;}
    result.sourceCellCount=result.cells.size();
    return result;
}
inline TerrainGroupSelection translatedTerrainSelection(const MapModel &map,
    const TerrainGroupSelection &selection,int dx,int dy)
{
    auto cells=selection.cells;
    for(auto &p:cells){p.x+=dx;p.y+=dy;}
    auto result=terrainSelectionFromCells(map,selection.kind,std::move(cells));
    result.sourceCellCount=selection.sourceCellCount;
    return result;
}
inline bool terrainKindInGroup(MapTileKind group,MapTileKind cell)
{
    if(group==MapTileKind::Mountain || group==MapTileKind::Hill)
        return cell==MapTileKind::Mountain || cell==MapTileKind::Hill;
    return group==cell;
}
inline TerrainGroupSelection selectTerrainGroup(const MapModel &map,int x,int y)
{
    TerrainGroupSelection result;
    const auto *anchor=mapCellAt(map,x,y);
    if(!anchor || !selectableTerrainKind(anchor->kind)) return result;
    result.width=map.width; result.height=map.height; result.kind=anchor->kind;
    result.mask.resize(map.cells.size());
    result.cells.push_back({x,y}); result.mask[y*map.width+x]=1;
    for(std::size_t next=0;next<result.cells.size();++next)
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            const int xx=result.cells[next].x+dx, yy=result.cells[next].y+dy;
            const auto *cell=mapCellAt(map,xx,yy);
            if(cell && terrainKindInGroup(result.kind,cell->kind) && !result.contains(xx,yy)) {
                result.mask[yy*map.width+xx]=1; result.cells.push_back({xx,yy});
            }
        }
    result.sourceCellCount=result.cells.size();
    return result;
}
inline bool terrainGroupCanMove(const MapModel &map,const TerrainGroupSelection &group,int dx,int dy)
{
    if(group.cells.empty() || map.width!=group.width || map.height!=group.height) return false;
    for(const auto p:group.cells) {
        const auto *source=mapCellAt(map,p.x,p.y), *dest=mapCellAt(map,p.x+dx,p.y+dy);
        if(!source || !terrainKindInGroup(group.kind,source->kind) || !dest) return false;
        if((dx || dy) && dest->wayKind!=MapWayKind::None) return false;
        if(!group.contains(p.x+dx,p.y+dy)) {
            switch(dest->kind) {
            case MapTileKind::Empty: case MapTileKind::Dirt: case MapTileKind::Snow:
            case MapTileKind::Desert: case MapTileKind::Stone: break;
            default: return false;
            }
        }
    }
    for(const auto &feature:map.generation.graph.features) {
        if(!mapGeneratedFeatureIsLocked(map,feature)) continue;
        for(const auto p:feature.points) {
            const int x=static_cast<int>(std::floor(p.x)),y=static_cast<int>(std::floor(p.y));
            if(group.contains(x,y) || group.contains(x-dx,y-dy)) return false;
        }
    }
    return true;
}
inline bool moveTerrainGroup(MapModel &map,const TerrainGroupSelection &group,int dx,int dy)
{
    if(!terrainGroupCanMove(map,group,dx,dy)) return false;
    if(!dx && !dy) return true;
    std::vector<MapCellModel> sources;
    for(const auto p:group.cells) sources.push_back(*mapCellAt(map,p.x,p.y));
    for(const auto p:group.cells) {
        auto &cell=*mapCellAt(map,p.x,p.y);
        applyMapCellTerrainDefaults(cell,MapTileKind::Empty); cell.elevation=0;
    }
    for(std::size_t i=0;i<group.cells.size();++i) {
        const auto p=group.cells[i]; auto &cell=*mapCellAt(map,p.x+dx,p.y+dy);
        // Only terrain travels: fog, lighting, routes and encounter state stay
        // at their authored positions. Terrain rules set the new blockers.
        applyMapCellTerrainDefaults(cell,sources[i].kind); cell.elevation=sources[i].elevation;
    }
    for(auto &feature:map.generation.graph.features) {
        if(feature.kind!=MapGeneratedFeatureKind::Vegetation && feature.kind!=MapGeneratedFeatureKind::MountainRange) continue;
        if(feature.points.empty() || !std::all_of(feature.points.begin(),feature.points.end(),[&](auto p){
            return group.contains(static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y))); })) continue;
        for(auto &p:feature.points) { p.x+=dx; p.y+=dy; }
    }
    return true;
}
}
