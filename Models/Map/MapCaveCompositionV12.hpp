#pragma once
#include "Models/Map/MapModels.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <queue>
#include <limits>
#include <functional>

// Generation-only surface access. Find dry routes from the existing two
// stair chambers to different map edges; excavate rock, never erase water,
// placed obstacles, stairs or locked cells. Existing saved rasters are untouched.
inline void mapComposeCaveEntrancesV12(MapModel& map,int generatorVersion)
{
    if(generatorVersion<12 || map.creationIntent!=MapCreationIntent::Cave || map.width<8 || map.height<8 ||
       !map.generation.recipe.lockedFeatureIds.empty())return;
    for(const auto& feature:map.generation.graph.features)if(feature.locked)return;
    std::vector<int> starts;
    for(int i=0;i<int(map.cells.size());++i)if(map.cells[i].kind==MapTileKind::Stairs)starts.push_back(i);
    if(starts.empty())return;
    constexpr int dx[]={1,0,-1,0},dy[]={0,1,0,-1};
    const auto edge=[&](int x,int y){return x==0?0:x==map.width-1?1:y==0?2:y==map.height-1?3:-1;};
    int previousEdge=-1;std::string mouths;
    for(std::size_t entry=0;entry<std::min<std::size_t>(2,starts.size());++entry) {
        std::vector<int> distance(map.cells.size(),std::numeric_limits<int>::max()),parent(map.cells.size(),-1);
        using Cost=std::pair<int,int>;std::priority_queue<Cost,std::vector<Cost>,std::greater<Cost>> queue;
        const int start=starts[entry];distance[start]=0;queue.push({0,start});int goal=-1;
        while(!queue.empty()) {
            const auto [cost,index]=queue.top();queue.pop();if(cost!=distance[index])continue;
            const int x=index%map.width,y=index/map.width,side=edge(x,y);
            if(side>=0 && side!=previousEdge && (side<2?(y>=2&&y<map.height-2):(x>=2&&x<map.width-2))) {goal=index;break;}
            for(int direction=0;direction<4;++direction) {
                const int xx=x+dx[direction],yy=y+dy[direction];const auto* cell=mapCellAt(map,xx,yy);
                if(!cell||cell->rotationLocked)continue;
                const auto kind=cell->kind;
                if(kind!=MapTileKind::Cave&&kind!=MapTileKind::Stairs&&kind!=MapTileKind::Wall&&kind!=MapTileKind::Empty)continue;
                const int next=yy*map.width+xx;
                const int candidate=cost+((kind==MapTileKind::Wall||kind==MapTileKind::Empty)?5:2);
                if(candidate<distance[next]){distance[next]=candidate;parent[next]=index;queue.push({candidate,next});}
            }
        }
        if(goal<0)continue;
        previousEdge=edge(goal%map.width,goal/map.width);
        if(!mouths.empty())mouths+=';';mouths+=std::to_string(goal%map.width)+","+std::to_string(goal/map.width);
        for(int index=goal;index>=0;index=parent[index]) {
            const int x=index%map.width,y=index/map.width;
            for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox) {
                if(ox*ox+oy*oy>1)continue;
                auto* cell=mapCellAt(map,x+ox,y+oy);
                if(!cell||cell->rotationLocked||(cell->kind!=MapTileKind::Wall&&cell->kind!=MapTileKind::Empty))continue;
                applyMapCellTerrainDefaults(*cell,MapTileKind::Cave);
            }
            // Mark the whole dry centreline, including its chamber approach,
            // so the new passage remains joined to the existing stair network.
            // An isolated way painted only in excavated rock would lower the
            // connectivity score despite traversable floor between segments.
            auto* centre=mapCellAt(map,x,y);
            if(centre && centre->wayKind==MapWayKind::None)centre->wayKind=MapWayKind::CaveTunnel;
            if(index==start)break;
        }
    }
    if(!mouths.empty()) {
        map.metadata.customProperties["cave.surface_entrances"]=mouths;
        map.metadata.customProperties["cave.composition_revision"]="surface_access_1";
    }
}

// Generation-only relocation: aggregate existing geological cover into small
// seams. Count, terrain properties and the semantic dry backbone are preserved.
inline void mapComposeCaveGeologyV12(MapModel &map,int generatorVersion,std::uint32_t seed)
{
    if(generatorVersion<12||!map.generation.recipe.lockedFeatureIds.empty())return;
    for(const auto &f:map.generation.graph.features)if(f.locked)return;
    mapComposeCaveEntrancesV12(map,generatorVersion);
    struct Point{int x,y;};
    const auto detail=[](MapTileKind k){return k==MapTileKind::Cover||k==MapTileKind::Rubble||k==MapTileKind::Hazard;};
    std::vector<Point> props;
    for(int y=2;y<map.height-2;++y)for(int x=2;x<map.width-2;++x)if(detail(mapCellAt(map,x,y)->kind))props.push_back({x,y});
    std::vector<bool> moved(props.size(),false);
    for(std::size_t i=0;i<props.size();++i) {
        if(moved[i])continue;const auto anchor=props[i];const auto kind=mapCellAt(map,anchor.x,anchor.y)->kind;
        const unsigned h=seed^unsigned(anchor.x)*73856093u^unsigned(anchor.y)*19349663u;
        int placed=0;
        for(std::size_t j=i+1;j<props.size()&&placed<2;++j) {
            if(moved[j])continue;const auto from=props[j];const auto *source=mapCellAt(map,from.x,from.y);
            if(source->kind!=kind||source->wayKind!=MapWayKind::None||source->rotationLocked)continue;
            const int distance=std::abs(from.x-anchor.x)+std::abs(from.y-anchor.y);if(distance<3||distance>7)continue;
            bool sameChamber=true;
            const int steps=std::max(std::abs(from.x-anchor.x),std::abs(from.y-anchor.y));
            for(int s=1;s<steps;++s) {
                const auto*c=mapCellAt(map,anchor.x+(from.x-anchor.x)*s/steps,anchor.y+(from.y-anchor.y)*s/steps);
                if(!c||(c->kind!=MapTileKind::Cave&&!detail(c->kind))){sameChamber=false;break;}
            }
            if(!sameChamber)continue;
            constexpr int offsets[][2]={{1,1},{-1,1},{-1,-1},{1,-1},{2,0},{0,2},{-2,0},{0,-2}};
            for(int attempt=0;attempt<8;++attempt) {
                const auto &offset=offsets[(attempt+h)%8];const int x=anchor.x+offset[0],y=anchor.y+offset[1];
                auto*target=mapCellAt(map,x,y);if(!target||target->kind!=MapTileKind::Cave||target->wayKind!=MapWayKind::None||target->rotationLocked)continue;
                bool clear=true;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                    const auto*n=mapCellAt(map,x+dx,y+dy);
                    if(!n||(n->kind!=MapTileKind::Cave&&!detail(n->kind))||n->wayKind!=MapWayKind::None)clear=false;
                }
                if(!clear)continue;
                const auto original=*source;applyMapCellTerrainDefaults(*mapCellAt(map,from.x,from.y),MapTileKind::Cave);*target=original;
                moved[j]=true;++placed;break;
            }
        }
    }
}

inline void mapComposeCavePassagesV12(MapModel &map,int generatorVersion,std::uint32_t seed)
{
    if(generatorVersion<12)return;
    struct Point {int x,y;};std::vector<Point> pockets;
    for(int y=3;y<map.height-3;++y)for(int x=3;x<map.width-3;++x)
    {
        const auto*c=mapCellAt(map,x,y);if(c->kind!=MapTileKind::Wall)continue;
        unsigned h=seed ^ unsigned(x)*73856093u ^ unsigned(y)*19349663u;h^=h>>16;h*=0x7feb352du;h^=h>>15;
        if(h%43!=0)continue;
        bool tunnel=false,stair=false,edge=false;
        for(int oy=-2;oy<=2;++oy)for(int ox=-2;ox<=2;++ox)
        {
            const auto*n=mapCellAt(map,x+ox,y+oy);
            if(n&&n->wayKind==MapWayKind::CaveTunnel)tunnel=true;
            if(n&&n->kind==MapTileKind::Stairs)stair=true;
            if(std::abs(ox)+std::abs(oy)==1&&n&&n->kind==MapTileKind::Cave)edge=true;
        }
        if(tunnel&&edge&&!stair)pockets.push_back({x,y});
    }
    for(auto p:pockets)for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox)
    {
        if(ox*ox+oy*oy>1)continue;
        auto*c=mapCellAt(map,p.x+ox,p.y+oy);
        if(c&&c->kind==MapTileKind::Wall)applyMapCellTerrainDefaults(*c,MapTileKind::Cave);
    }
}

// Grow an existing connected pool into a chamber-scale basin. The protected
// semantic tunnel backbone and stair approaches stay dry; a shallow stone bank
// joins any newly excavated lake edge back to the original cavern floor.
inline void mapComposeCaveBasinsV12(MapModel &map, int waterLevel, int generatorVersion=0, std::uint32_t seed=0)
{
    if (generatorVersion==0) generatorVersion=map.generation.recipe.generatorVersion;
    if (seed==0) seed=map.generation.recipe.seed;
    if (generatorVersion < 12 || waterLevel < 25) return;
    struct Point { int x, y; };
    std::vector<Point> seeds;
    for (int y=2;y<map.height-2;++y) for (int x=2;x<map.width-2;++x)
        if (mapCellAt(map,x,y)->kind==MapTileKind::Water) seeds.push_back({x,y});
    if (seeds.empty()) {
        // A positive water recipe must not silently become dry because the
        // initial sparse pool pass found no seed. Pick a safe chamber pocket.
        int best=-1;Point pocket{-1,-1};
        for(int y=3;y<map.height-3;++y)for(int x=3;x<map.width-3;++x) {
            const auto*c=mapCellAt(map,x,y);if(c->kind!=MapTileKind::Cave)continue;
            bool safe=true;int room=0;
            for(int oy=-2;oy<=2;++oy)for(int ox=-2;ox<=2;++ox) {
                const auto*n=mapCellAt(map,x+ox,y+oy);
                if(n&&(n->kind==MapTileKind::Stairs||n->wayKind!=MapWayKind::None))safe=false;
                if(n&&n->kind==MapTileKind::Cave)++room;
            }
            if(!safe)continue;
            const int score=room*100+static_cast<int>((seed^unsigned(x)*197u^unsigned(y)*397u)%97);
            if(score>best){best=score;pocket={x,y};}
        }
        if(pocket.x<0)return;
        applyMapCellTerrainDefaults(*mapCellAt(map,pocket.x,pocket.y),MapTileKind::Water);
        seeds.push_back(pocket);
    }
    const std::uint32_t hash=seed*2654435761u;
    auto center=seeds[hash%seeds.size()];
    const float radius=std::min(std::min(map.width,map.height)*.18f, 1.5f+waterLevel*.065f);
    const float phase=(hash%628)*.01f;
    const float stretch=.78f+((hash>>10)%30)*.01f;
    // Prefer a pocket with room for a broad basin over one pressed against the
    // image boundary. Ranking changes no seed/request and does not reroll.
    int bestArea=-1;
    const int drift=std::max(1,static_cast<int>(radius*.38f));
    for(std::size_t i=0;i<seeds.size();++i) for(int cy=-drift;cy<=drift;++cy)for(int cx=-drift;cx<=drift;++cx) {
        if(cx*cx+cy*cy>drift*drift)continue;
        const auto origin=seeds[(i+hash%seeds.size())%seeds.size()];
        const Point candidate{origin.x+cx,origin.y+cy};
        const auto *candidateCell=mapCellAt(map,candidate.x,candidate.y);
        if(!candidateCell||candidateCell->kind==MapTileKind::Stairs||candidateCell->wayKind!=MapWayKind::None)continue;
        int area=0;
        const int range=static_cast<int>(std::ceil(radius+1));
        for(int oy=-range;oy<=range;++oy)for(int ox=-range;ox<=range;++ox) {
            if(ox*ox+oy*oy>radius*radius)continue;
            const int x=candidate.x+ox,y=candidate.y+oy;
            if(x<2||y<2||x>=map.width-2||y>=map.height-2)continue;
            bool clear=true;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                const auto*c=mapCellAt(map,x+dx,y+dy);
                if(!c||c->kind==MapTileKind::Stairs||c->wayKind!=MapWayKind::None)clear=false;
            }
            area+=clear;
        }
        if(area>bestArea){bestArea=area;center=candidate;}
    }
    std::vector<Point> basin;
    for (int y=2;y<map.height-2;++y) for (int x=2;x<map.width-2;++x)
    {
        const auto *cell=mapCellAt(map,x,y);
        if (cell->kind!=MapTileKind::Cave && cell->kind!=MapTileKind::Wall && cell->kind!=MapTileKind::Water) continue;
        const float dx=(x-center.x)/stretch,dy=(y-center.y)*stretch;
        const float angle=std::atan2(dy,dx);
        const float edge=radius*(1.f+.16f*std::sin(angle*3+phase)+.08f*std::cos(angle*5-phase));
        if(dx*dx+dy*dy>edge*edge) continue;
        bool protectedFloor=false;
        // Widen the already protected dry backbone into softly varying
        // banks. Never shrink its previous one-cell safety margin.
        const float bank=1.8f+.38f*std::sin(x*.61f+y*.47f+phase);
        for(int oy=-2;oy<=2;++oy) for(int ox=-2;ox<=2;++ox)
        {
            if((std::abs(ox)>1||std::abs(oy)>1) && ox*ox+oy*oy>bank*bank)continue;
            const auto *neighborCell=mapCellAt(map,x+ox,y+oy);
            if(neighborCell && (neighborCell->kind==MapTileKind::Stairs || neighborCell->wayKind!=MapWayKind::None)) protectedFloor=true;
        }
        if(!protectedFloor) basin.push_back({x,y});
    }
    // Keep only four-connected additions reached from original water. This
    // prevents a protected tunnel from splitting the proposal into new ponds.
    std::vector<bool> selected(map.cells.size(),false),reached(map.cells.size(),false);
    for(auto p:basin) selected[p.y*map.width+p.x]=true;
    std::vector<Point> queue;
    for(auto p:basin) if(mapCellAt(map,p.x,p.y)->kind==MapTileKind::Water) {queue.push_back(p);reached[p.y*map.width+p.x]=true;}
    for(std::size_t i=0;i<queue.size();++i) for(auto d:std::vector<Point>{{1,0},{-1,0},{0,1},{0,-1}})
    {
        const Point p{queue[i].x+d.x,queue[i].y+d.y};
        if(p.x<1||p.y<1||p.x>=map.width-1||p.y>=map.height-1) continue;
        const auto index=p.y*map.width+p.x;
        if(selected[index]&&!reached[index]){reached[index]=true;queue.push_back(p);}
    }
    for(auto p:queue)
    {
        applyMapCellTerrainDefaults(*mapCellAt(map,p.x,p.y),MapTileKind::Water);
        for(int oy=-1;oy<=1;++oy) for(int ox=-1;ox<=1;++ox)
        {
            const int x=p.x+ox,y=p.y+oy;
            auto *cell=mapCellAt(map,x,y);
            if(x>0&&y>0&&x<map.width-1&&y<map.height-1&&cell&&cell->kind==MapTileKind::Wall)
                applyMapCellTerrainDefaults(*cell,MapTileKind::Cave);
        }
    }
}
