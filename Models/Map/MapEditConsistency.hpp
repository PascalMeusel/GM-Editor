#pragma once
#include "MapAuthoring.hpp"
#include "MapOverviewRenderPolicy.hpp"
#include <queue>
#include <limits>

struct MapEditConflict
{
    EntityId featureId;
    bool place = false;
    MapFeaturePoint point;
    std::string name;
    bool locked = false;
    bool waterFragment = false;
};

inline std::vector<MapEditConflict> inspectMapEditConflicts(const MapModel &map)
{
    std::vector<MapEditConflict> result;
    if (!map.generation.continuousRendering ||
        (map.atlasLevel != MapLevel::World && map.atlasLevel != MapLevel::Region)) return result;
    const auto ground = gm::map_render::buildOverviewGroundProjection(map);
    const auto wet = [&](MapFeaturePoint p) {
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        return isMapCellInside(map,x,y) && !gm::map_render::overviewGroundKindIsLand(
            gm::map_render::overviewGroundKindFromProjection(map,ground,x,y));
    };
    for (const auto &feature : map.generation.graph.features)
    {
        if (feature.points.empty()) continue;
        const bool place = feature.kind == MapGeneratedFeatureKind::Settlement;
        const bool road = feature.kind == MapGeneratedFeatureKind::Road || feature.kind == MapGeneratedFeatureKind::Trail;
        if (!place && !road) continue;
        MapFeaturePoint conflict = feature.points.front();
        bool found = place && wet(conflict);
        for (std::size_t i=1; road && !found && i<feature.points.size(); ++i)
        {
            const auto a=feature.points[i-1], b=feature.points[i];
            const int steps=std::max(1,static_cast<int>(std::ceil(std::hypot(b.x-a.x,b.y-a.y)*4.f)));
            for(int n=0;n<=steps;++n)
            {
                const float t=static_cast<float>(n)/steps;
                const MapFeaturePoint p{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
                if(wet(p)){conflict=p;found=true;break;}
            }
        }
        if(found) result.push_back({feature.id,place,conflict,feature.name,mapGeneratedFeatureIsLocked(map,feature)});
    }
    std::vector<bool> seen(map.cells.size(),false);
    const auto patchWater=[&](int x,int y){const auto *cell=mapCellAt(map,x,y);return cell &&
        cell->kind==MapTileKind::Water && cell->wayKind==MapWayKind::None && wet({x+.5f,y+.5f});};
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x)
    {
        const int start=y*map.width+x;
        if(seen[start] || !patchWater(x,y))continue;
        std::vector<int> component{start};seen[start]=true;bool edge=false;
        for(std::size_t n=0;n<component.size();++n)
        {
            const int cx=component[n]%map.width,cy=component[n]/map.width;
            edge|=cx==0||cy==0||cx==map.width-1||cy==map.height-1;
            for(const auto [dx,dy]:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}})
            {
                const int nx=cx+dx,ny=cy+dy;
                if(!isMapCellInside(map,nx,ny))continue;
                const int index=ny*map.width+nx;
                if(!seen[index] && patchWater(nx,ny)){seen[index]=true;component.push_back(index);}
            }
        }
        if(edge||component.size()>3)continue;
        const MapFeaturePoint p{x+.5f,y+.5f};
        bool used=false;
        for(const auto &feature:map.generation.graph.features)
            if(!feature.points.empty() && (feature.kind==MapGeneratedFeatureKind::River || feature.kind==MapGeneratedFeatureKind::Settlement) &&
               std::any_of(feature.points.begin(),feature.points.end(),[&](const auto &point){
                   const int index=static_cast<int>(std::floor(point.y))*map.width+static_cast<int>(std::floor(point.x));
                   return std::find(component.begin(),component.end(),index)!=component.end();})){used=true;break;}
        if(!used)result.push_back({"water_patch_"+std::to_string(start),false,p,"",false,true});
    }
    return result;
}

// Explicit repairs only: coast painting never silently relocates campaign places.
// Work on a copy so an impossible route or locked feature cannot partially mutate a map.
inline bool repairMapEditConflict(MapModel &map, const EntityId &id)
{
    const auto conflicts=inspectMapEditConflicts(map);
    const auto conflict=std::find_if(conflicts.begin(),conflicts.end(),[&](const auto &c){return c.featureId==id;});
    if(conflict==conflicts.end() || conflict->locked) return false;
    MapModel draft=map;
    if(conflict->waterFragment)
    {
        std::vector<MapCellCoordinate> cells{{static_cast<int>(conflict->point.x),static_cast<int>(conflict->point.y)}};
        for(std::size_t n=0;n<cells.size();++n)
            for(const auto [dx,dy]:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}})
            {
                const int x=cells[n].x+dx,y=cells[n].y+dy;const auto *cell=mapCellAt(draft,x,y);
                if(cell && cell->kind==MapTileKind::Water && cell->wayKind==MapWayKind::None)
                    addUniqueMapCellCoordinate(cells,draft,x,y);
            }
        if(cells.size()>3)return false;
        MapTileKind fill=MapTileKind::Dirt;
        for(const auto &p:cells)for(const auto [dx,dy]:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}})
            if(const auto *cell=mapCellAt(draft,p.x+dx,p.y+dy);cell &&
                (cell->kind==MapTileKind::Empty || cell->kind==MapTileKind::Desert || cell->kind==MapTileKind::Dirt || cell->kind==MapTileKind::Snow))fill=cell->kind;
        for(const auto &p:cells)applyMapCellTerrainDefaults(*mapCellAt(draft,p.x,p.y),fill);
        map=std::move(draft);return true;
    }
    auto found=std::find_if(draft.generation.graph.features.begin(),draft.generation.graph.features.end(),
        [&](const auto &f){return f.id==id;});
    if(found==draft.generation.graph.features.end())return false;
    const auto before=*found;
    if(conflict->place)
    {
        for(const auto &road:draft.generation.graph.features)
            if(road.points.size()>1 && (road.kind==MapGeneratedFeatureKind::Road || road.kind==MapGeneratedFeatureKind::Trail) &&
               mapGeneratedFeatureIsLocked(draft,road) &&
               mapFeaturePointDistanceSquared(nearestMapRoadPoint(road,before.points.front()),before.points.front())<.0025f)return false;
        const auto ground=gm::map_render::buildOverviewGroundProjection(draft);
        float best=std::numeric_limits<float>::max();MapFeaturePoint destination{};bool available=false;
        for(int y=0;y<draft.height;++y)for(int x=0;x<draft.width;++x)
        {
            const auto *cell=mapCellAt(draft,x,y);
            if(!gm::map_render::overviewGroundKindIsLand(gm::map_render::overviewGroundKindFromProjection(draft,ground,x,y)))continue;
            if(!cell || cell->kind==MapTileKind::Water || cell->kind==MapTileKind::Mountain ||
               cell->kind==MapTileKind::Building || cell->kind==MapTileKind::Wall || cell->kind==MapTileKind::Lava ||
               cell->kind==MapTileKind::Pit || cell->kind==MapTileKind::Chasm)continue;
            const MapFeaturePoint p{x+.5f,y+.5f};
            if(std::any_of(draft.generation.graph.features.begin(),draft.generation.graph.features.end(),[&](const auto &f){
                return f.id!=id && f.kind==MapGeneratedFeatureKind::Settlement && !f.points.empty() &&
                    mapFeaturePointDistanceSquared(p,f.points.front())<1.f;}))continue;
            const float distance=mapFeaturePointDistanceSquared(p,before.points.front());
            if(distance<best){best=distance;destination=p;available=true;}
        }
        if(!available)return false;
        found->points.front()=destination;
    }
    else
    {
        const auto ground=gm::map_render::buildOverviewGroundProjection(draft);
        const auto land=[&](int x,int y){return isMapCellInside(draft,x,y) && gm::map_render::overviewGroundKindIsLand(
            gm::map_render::overviewGroundKindFromProjection(draft,ground,x,y));};
        // Keep every existing incoming junction as a waypoint along the original road.
        std::vector<std::pair<float,MapFeaturePoint>> anchors{{0.f,before.points.front()}};
        float total=0.f;
        for(std::size_t i=1;i<before.points.size();++i)
        {
            MapGeneratedFeature segment;segment.points={before.points[i-1],before.points[i]};
            const float length=std::sqrt(mapFeaturePointDistanceSquared(segment.points.front(),segment.points.back()));
            for(const auto &other:draft.generation.graph.features)
                if(other.id!=id && other.kind==MapGeneratedFeatureKind::Settlement && !other.points.empty())
                {
                    const auto p=other.points.front(),q=nearestMapRoadPoint(segment,p);
                    if(mapFeaturePointDistanceSquared(q,p)<.0025f)
                        anchors.push_back({total+std::sqrt(mapFeaturePointDistanceSquared(segment.points.front(),q)),p});
                }
            for(const auto &other:draft.generation.graph.features)
                if(other.id!=id && other.points.size()>1 &&
                   (other.kind==MapGeneratedFeatureKind::Road || other.kind==MapGeneratedFeatureKind::Trail))
                    for(const auto &p:{other.points.front(),other.points.back()})
                    {
                        const auto q=nearestMapRoadPoint(segment,p);
                        if(mapFeaturePointDistanceSquared(q,p)<.0025f)
                            anchors.push_back({total+std::sqrt(mapFeaturePointDistanceSquared(segment.points.front(),q)),p});
                    }
            total+=length;
        }
        anchors.push_back({total,before.points.back()});
        std::sort(anchors.begin(),anchors.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        std::vector<MapFeaturePoint> routed;
        for(std::size_t part=1;part<anchors.size();++part)
        {
            const auto start=anchors[part-1].second,finish=anchors[part].second;
            if(mapFeaturePointDistanceSquared(start,finish)<.00001f)continue;
            const int sx=static_cast<int>(std::floor(start.x)),sy=static_cast<int>(std::floor(start.y));
            const int tx=static_cast<int>(std::floor(finish.x)),ty=static_cast<int>(std::floor(finish.y));
            if(!land(sx,sy)||!land(tx,ty))return false;
            const int source=sy*draft.width+sx,target=ty*draft.width+tx,count=draft.width*draft.height;
            std::vector<float> distance(count,std::numeric_limits<float>::max());std::vector<int> parent(count,-1);
            using Node=std::pair<float,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>> queue;
            distance[source]=0;queue.push({0.0f,source});
            while(!queue.empty())
            {
                const auto [cost,index]=queue.top();queue.pop();
                if(cost>distance[index])continue;if(index==target)break;
                const int x=index%draft.width,y=index/draft.width;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
                {
                    if((!dx&&!dy)||!land(x+dx,y+dy)||(dx&&dy&&(!land(x+dx,y)||!land(x,y+dy))))continue;
                    const int next=(y+dy)*draft.width+x+dx;
                    float extra=(dx&&dy)?1.414214f:1.f;
                    if(mapCellAt(draft,x+dx,y+dy)->kind==MapTileKind::Mountain)extra*=3.f;
                    // Prefer a little shoreline clearance without making narrow land impassable.
                    if(!land(x+dx-1,y+dy)||!land(x+dx+1,y+dy)||!land(x+dx,y+dy-1)||!land(x+dx,y+dy+1))extra+=2.f;
                    if(cost+extra<distance[next]){distance[next]=cost+extra;parent[next]=index;queue.push({distance[next],next});}
                }
            }
            if(distance[target]==std::numeric_limits<float>::max())return false;
            std::vector<MapFeaturePoint> path;
            for(int at=target;at!=-1;at=parent[at]){path.push_back({at%draft.width+.5f,at/draft.width+.5f});if(at==source)break;}
            std::reverse(path.begin(),path.end());path.front()=start;
            if(path.size()==1)path.push_back(finish);else path.back()=finish;
            const auto safeSegment=[&](MapFeaturePoint a,MapFeaturePoint b){
                const int steps=std::max(1,static_cast<int>(std::ceil(std::hypot(b.x-a.x,b.y-a.y)*8.f)));
                for(int n=0;n<=steps;++n){const float t=static_cast<float>(n)/steps;
                    if(!land(static_cast<int>(std::floor(a.x+(b.x-a.x)*t)),static_cast<int>(std::floor(a.y+(b.y-a.y)*t))))return false;}
                return true;
            };
            // Round raster routing corners while preserving each explicit
            // town/junction anchor and checking every replacement chord.
            for(int pass=0;pass<3;++pass)
            {
                std::vector<MapFeaturePoint> rounded{path.front()};
                for(std::size_t i=1;i+1<path.size();++i)
                {
                    const auto a=path[i-1],p=path[i],b=path[i+1];
                    const MapFeaturePoint enter{p.x*.75f+a.x*.25f,p.y*.75f+a.y*.25f};
                    const MapFeaturePoint leave{p.x*.75f+b.x*.25f,p.y*.75f+b.y*.25f};
                    if(safeSegment(rounded.back(),enter)&&safeSegment(enter,leave)&&safeSegment(leave,b))
                    {rounded.push_back(enter);rounded.push_back(leave);}
                    else rounded.push_back(p);
                }
                rounded.push_back(path.back());path=std::move(rounded);
            }
            if(!routed.empty())path.erase(path.begin());routed.insert(routed.end(),path.begin(),path.end());
        }
        if(routed.size()<2)return false;
        found->points=std::move(routed);
    }
    if(!finishGeneratedMapFeatureGeometryEdit(draft,before))return false;
    const auto remaining=inspectMapEditConflicts(draft);
    if(std::any_of(remaining.begin(),remaining.end(),[&](const auto &c){return c.featureId==id;}))return false;
    map=std::move(draft);return true;
}
