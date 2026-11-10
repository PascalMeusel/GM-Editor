#pragma once
#include "Views/Editor/Widgets/Map/MapCreator/MapObjectEditing.hpp"
#include "Views/Editor/Widgets/Map/MapCreator/MapTerrainGroupEditing.hpp"
#include "Models/Map/MapRoutePresentation.hpp"

namespace gm::mapui {
struct MapSelectionBox {
    float left,top,right,bottom;
    bool contains(MapFeaturePoint p) const {return p.x>=left && p.x<=right && p.y>=top && p.y<=bottom;}
};
inline MapSelectionBox mapSelectionBox(MapFeaturePoint a,MapFeaturePoint b) {
    return {std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)};
}
inline bool selectionBoxIntersectsSegment(MapSelectionBox box,MapFeaturePoint a,MapFeaturePoint b) {
    float lo=0,hi=1;
    const auto clip=[&](float p,float q) {
        if(std::abs(p)<.000001f) return q>=0;
        const float r=q/p;
        if(p<0) lo=std::max(lo,r); else hi=std::min(hi,r);
        return lo<=hi;
    };
    const float dx=b.x-a.x,dy=b.y-a.y;
    return clip(-dx,a.x-box.left) && clip(dx,box.right-a.x) && clip(-dy,a.y-box.top) && clip(dy,box.bottom-a.y);
}
inline bool selectionBoxIntersectsFeature(MapSelectionBox box,const MapGeneratedFeature &feature) {
    if(feature.points.empty()) return false;
    if(feature.points.size()==1) return box.contains(feature.points.front());
    for(std::size_t p=1;p<feature.points.size();++p)
        if(selectionBoxIntersectsSegment(box,feature.points[p-1],feature.points[p])) return true;
    return feature.closed && (selectionBoxIntersectsSegment(box,feature.points.back(),feature.points.front()) ||
        mapObjectContainsPoint(feature,{(box.left+box.right)*.5f,(box.top+box.bottom)*.5f}));
}
enum class AreaObjectKind { Generated,Terrain,Route,Marker,Token,Region };
struct AreaSelectionCandidate {
    AreaObjectKind kind;
    int index=-1,x=0,y=0;
    int category=0; // 1 places, 2 ways, 3 terrain, 4 other
    std::string name,type;
    std::vector<MapFeaturePoint> previewPoints;
    TerrainGroupSelection terrain;
    bool selected=true;
};
inline std::vector<AreaSelectionCandidate> collectMapAreaSelection(const MapModel &map,MapSelectionBox box) {
    std::vector<AreaSelectionCandidate> result;
    for(int i=0;i<static_cast<int>(map.generation.graph.features.size());++i) {
        const auto &feature=map.generation.graph.features[i];
        if(!mapObjectCanMove(feature)) continue;
        auto presented=feature;
        presented.points=mapFeaturePresentationPoints(map,feature);
        if(!selectionBoxIntersectsFeature(box,presented)) continue;
        const bool place=feature.kind==MapGeneratedFeatureKind::Settlement || feature.kind==MapGeneratedFeatureKind::Landmark || feature.kind==MapGeneratedFeatureKind::BuildingGroup;
        const bool way=feature.kind==MapGeneratedFeatureKind::Road || feature.kind==MapGeneratedFeatureKind::Trail || feature.kind==MapGeneratedFeatureKind::River || feature.kind==MapGeneratedFeatureKind::Bridge;
        const char *type="Object";
        switch(feature.kind) {
        case MapGeneratedFeatureKind::Settlement: type="City";break;
        case MapGeneratedFeatureKind::Landmark: type="Landmark";break;
        case MapGeneratedFeatureKind::BuildingGroup: type="Building";break;
        case MapGeneratedFeatureKind::Road: type="Road";break;
        case MapGeneratedFeatureKind::Trail: type="Trail";break;
        case MapGeneratedFeatureKind::River: type="River";break;
        case MapGeneratedFeatureKind::Bridge: type="Bridge";break;
        default: break;
        }
        result.push_back({AreaObjectKind::Generated,i,0,0,place?1:way?2:4,feature.name,type});
        result.back().previewPoints=std::move(presented.points);
    }
    std::vector<unsigned char> seen(map.cells.size());
    const int x0=std::max(0,static_cast<int>(std::floor(box.left))),x1=std::min(map.width-1,static_cast<int>(std::floor(box.right)));
    const int y0=std::max(0,static_cast<int>(std::floor(box.top))),y1=std::min(map.height-1,static_cast<int>(std::floor(box.bottom)));
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
        if(seen[y*map.width+x]) continue;
        const auto group=selectTerrainGroup(map,x,y);
        if(group.cells.empty()) continue;
        for(const auto p:group.cells) seen[p.y*map.width+p.x]=1;
        std::vector<MapCellCoordinate> clipped;
        for(const auto p:group.cells) if(box.contains({p.x+.5f,p.y+.5f})) clipped.push_back(p);
        if(clipped.empty()) continue;
        result.push_back({AreaObjectKind::Terrain,-1,x,y,3,mapTileKindLabel(group.kind),"Terrain area"});
        result.back().terrain=terrainSelectionFromCells(map,group.kind,std::move(clipped));
        result.back().terrain.sourceCellCount=group.cells.size();
    }
    // Background ground is not an object behind every forest or road. Keep
    // it available only when the box contains no foreground terrain/object.
    const bool foreground=std::any_of(result.begin(),result.end(),[](const auto &i){
        return i.kind!=AreaObjectKind::Terrain || (i.terrain.kind!=MapTileKind::Dirt && i.terrain.kind!=MapTileKind::Stone);
    });
    if(foreground) result.erase(std::remove_if(result.begin(),result.end(),[](const auto &i){
        return i.kind==AreaObjectKind::Terrain && (i.terrain.kind==MapTileKind::Dirt || i.terrain.kind==MapTileKind::Stone);
    }),result.end());
    for(int i=0;i<static_cast<int>(map.routes.size());++i) {
        MapGeneratedFeature path;path.points=gm::map_route_presentation::build(map,map.routes[i]).points;
        if(selectionBoxIntersectsFeature(box,path)) {
            result.push_back({AreaObjectKind::Route,i,0,0,2,map.routes[i].name,"Travel route"});
            result.back().previewPoints=std::move(path.points);
        }
    }
    for(int i=0;i<static_cast<int>(map.features.size());++i) {
        const auto &f=map.features[i];
        if(box.contains({f.x+.5f,f.y+.5f})) result.push_back({AreaObjectKind::Marker,i,0,0,1,f.name,"Marker"});
    }
    for(int i=0;i<static_cast<int>(map.tokens.size());++i) {
        const auto &f=map.tokens[i];
        if(box.contains({f.x+.5f,f.y+.5f})) result.push_back({AreaObjectKind::Token,i,0,0,4,f.name,"Token"});
    }
    for(int i=0;i<static_cast<int>(map.regions.size());++i) {
        const auto &r=map.regions[i];
        if(r.x+r.width>=box.left && r.x<=box.right && r.y+r.height>=box.top && r.y<=box.bottom)
            result.push_back({AreaObjectKind::Region,i,0,0,4,r.name,"Region"});
    }
    return result;
}
// Selection is additive across categories: every intersected object starts
// selected, and the sidebar toggles membership without changing document data.
inline std::size_t selectedAreaCount(const std::vector<AreaSelectionCandidate> &items) {
    return std::count_if(items.begin(),items.end(),[](const auto &i){return i.selected;});
}
inline bool areaSelectionContains(const MapModel &map,const std::vector<AreaSelectionCandidate> &items,
                                  MapFeaturePoint p,float radius) {
    for(const auto &i:items) if(i.selected) {
        if(i.kind==AreaObjectKind::Terrain && i.terrain.contains(int(std::floor(p.x)),int(std::floor(p.y))))return true;
        if(i.kind==AreaObjectKind::Generated && i.index>=0 && i.index<int(map.generation.graph.features.size())) {
            auto f=map.generation.graph.features[i.index];f.points=i.previewPoints;
            if(selectionBoxIntersectsFeature({p.x-radius,p.y-radius,p.x+radius,p.y+radius},f))return true;
        }
        if(i.kind==AreaObjectKind::Route)for(std::size_t j=1;j<i.previewPoints.size();++j)
            if(gm::map_geometry::roadPointSegmentDistanceSquared(p,i.previewPoints[j-1],i.previewPoints[j])<=radius*radius)return true;
        if(i.kind==AreaObjectKind::Marker && i.index<int(map.features.size())) {
            const auto &v=map.features[i.index];if(std::hypot(p.x-v.x-.5f,p.y-v.y-.5f)<=radius)return true;
        }
        if(i.kind==AreaObjectKind::Token && i.index<int(map.tokens.size())) {
            const auto &v=map.tokens[i.index];if(std::hypot(p.x-v.x-.5f,p.y-v.y-.5f)<=radius)return true;
        }
        if(i.kind==AreaObjectKind::Region && i.index<int(map.regions.size())) {
            const auto &v=map.regions[i.index];if(p.x>=v.x && p.y>=v.y && p.x<=v.x+v.width && p.y<=v.y+v.height)return true;
        }
    }return false;
}
// One atomic document edit on release; invalid destinations retain everything.
inline bool moveAreaSelection(MapModel &map,std::vector<AreaSelectionCandidate> &items,int dx,int dy) {
    if(!selectedAreaCount(items) || (!dx && !dy))return false;
    auto next=map;
    const auto inside=[&](float x,float y){return x>=0 && y>=0 && x<map.width && y<map.height;};
    // Clear selected way projections together before testing terrain targets.
    // Their old road/river cells must not obstruct a group that moves them away.
    std::vector<int> movingWays;
    for(const auto &i:items)if(i.selected && i.kind==AreaObjectKind::Generated) {
        if(i.index<0 || i.index>=int(next.generation.graph.features.size()))return false;
        const auto &f=map.generation.graph.features[i.index];
        if(mapGeneratedFeatureIsLocked(map,f))return false;
        if(f.kind==MapGeneratedFeatureKind::Road || f.kind==MapGeneratedFeatureKind::Trail || f.kind==MapGeneratedFeatureKind::River) {
            movingWays.push_back(i.index);next.generation.graph.features[i.index].points.clear();
        }
    }
    for(int index:movingWays)reprojectGeneratedMapWayFeaturesAfterGeometryEdit(next,map.generation.graph.features[index]);
    for(int index:movingWays)next.generation.graph.features[index].points=map.generation.graph.features[index].points;

    // Move selected terrain patches together, including overlapping source and
    // destination footprints; moving them one by one can overwrite a neighbour.
    std::vector<MapCellCoordinate> terrainCells;
    for(const auto &i:items)if(i.selected && i.kind==AreaObjectKind::Terrain)
        terrainCells.insert(terrainCells.end(),i.terrain.cells.begin(),i.terrain.cells.end());
    terrainCells=normalizeMapCellCoordinates(std::move(terrainCells));
    const auto inTerrain=[&](int x,int y){return std::binary_search(terrainCells.begin(),terrainCells.end(),MapCellCoordinate{x,y});};
    for(auto p:terrainCells) {
        const auto *dest=mapCellAt(next,p.x+dx,p.y+dy);
        if(!dest || dest->wayKind!=MapWayKind::None)return false;
        if(!inTerrain(p.x+dx,p.y+dy) && dest->kind!=MapTileKind::Empty && dest->kind!=MapTileKind::Dirt &&
            dest->kind!=MapTileKind::Snow && dest->kind!=MapTileKind::Desert && dest->kind!=MapTileKind::Stone)return false;
    }
    for(const auto &f:map.generation.graph.features)if(mapGeneratedFeatureIsLocked(map,f))
        for(auto p:f.points)if(inTerrain(int(p.x),int(p.y)) || inTerrain(int(p.x)-dx,int(p.y)-dy))return false;
    for(auto p:terrainCells){auto &c=*mapCellAt(next,p.x,p.y);applyMapCellTerrainDefaults(c,MapTileKind::Empty);c.elevation=0;}
    for(auto p:terrainCells){const auto &src=*mapCellAt(map,p.x,p.y);auto &dst=*mapCellAt(next,p.x+dx,p.y+dy);applyMapCellTerrainDefaults(dst,src.kind);dst.elevation=src.elevation;}
    for(auto &f:next.generation.graph.features)if((f.kind==MapGeneratedFeatureKind::Vegetation || f.kind==MapGeneratedFeatureKind::MountainRange) &&
        !f.points.empty() && std::all_of(f.points.begin(),f.points.end(),[&](auto p){return inTerrain(int(p.x),int(p.y));}))
        for(auto &p:f.points){p.x+=dx;p.y+=dy;}
    for(const auto &i:items) if(i.selected) {
        if(i.kind==AreaObjectKind::Generated) {
            if(i.index<0 || i.index>=int(next.generation.graph.features.size()))return false;
            const auto &before=map.generation.graph.features[i.index];
            if(mapGeneratedFeatureIsLocked(map,before))return false;
            for(auto p:before.points)if(p.x+dx<0 || p.y+dy<0 || p.x+dx>map.width || p.y+dy>map.height)return false;
            if(!moveMapObject(next,i.index,before,{float(dx),float(dy)}))return false;
        } else if(i.kind==AreaObjectKind::Terrain) {
            // The complete terrain union was applied above.
        } else if(i.kind==AreaObjectKind::Marker) {
            if(i.index<0 || i.index>=int(next.features.size()))return false;
            auto &v=next.features[i.index];if(!inside(v.x+dx,v.y+dy))return false;v.x+=dx;v.y+=dy;
        } else if(i.kind==AreaObjectKind::Token) {
            if(i.index<0 || i.index>=int(next.tokens.size()))return false;
            auto &v=next.tokens[i.index];if(!inside(v.x+dx,v.y+dy))return false;v.x+=dx;v.y+=dy;
        } else if(i.kind==AreaObjectKind::Region) {
            if(i.index<0 || i.index>=int(next.regions.size()))return false;
            auto &v=next.regions[i.index];if(!inside(v.x+dx,v.y+dy) || v.x+dx+v.width>map.width || v.y+dy+v.height>map.height)return false;v.x+=dx;v.y+=dy;
        } else if(i.kind==AreaObjectKind::Route) {
            if(i.index<0 || i.index>=int(next.routes.size()))return false;
            auto &v=next.routes[i.index];if(!inside(v.startX+dx,v.startY+dy) || !inside(v.endX+dx,v.endY+dy))return false;
            v.startX+=dx;v.startY+=dy;v.endX+=dx;v.endY+=dy;
        }
    }
    for(const auto &i:items)if(i.selected && i.kind==AreaObjectKind::Generated)
        if(!finishGeneratedMapFeatureGeometryEdit(next,map.generation.graph.features[i.index]))return false;
    map=std::move(next);
    for(auto &i:items)if(i.selected) {
        if(i.kind==AreaObjectKind::Terrain)i.terrain=translatedTerrainSelection(map,i.terrain,dx,dy);
        if(i.kind==AreaObjectKind::Route)for(auto &p:i.previewPoints){p.x+=dx;p.y+=dy;}
        if(i.kind==AreaObjectKind::Generated)i.previewPoints=mapFeaturePresentationPoints(map,map.generation.graph.features[i.index]);
    }
    return true;
}
}
