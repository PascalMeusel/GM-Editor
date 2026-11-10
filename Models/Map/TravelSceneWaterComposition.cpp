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
int travelWaterCompositionV4(const TravelSceneRecipe &recipe)
{
    if(recipe.schemaVersion<4)return 0;
    std::string biome=recipe.biome;
    std::transform(biome.begin(),biome.end(),biome.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(biome.find("coast")!=std::string::npos || biome.find("küste")!=std::string::npos || biome.find("kueste")!=std::string::npos)return 1;
    if(biome.find("lake")!=std::string::npos || biome.find("see")!=std::string::npos)return 2;
    if(biome.find("river")!=std::string::npos || biome.find("stream")!=std::string::npos || biome.find("fluss")!=std::string::npos || biome.find("bach")!=std::string::npos)return 3;
    return 0;
}
bool composeTravelWaterV4(MapModel &map,const TravelSceneRecipe &recipe,unsigned seed)
{
    const int mode=travelWaterCompositionV4(recipe);if(!mode)return true;
    std::vector<MapCellCoordinate> protectedCells;
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        const auto *cell=mapCellAt(map,x,y);
        if(cell->wayKind!=MapWayKind::None || mapGenIsStructureLike(cell->kind) || cell->kind==MapTileKind::Landmark)
            protectedCells.push_back({x,y});
    }
    const auto clear=[&](int x,int y){return std::none_of(protectedCells.begin(),protectedCells.end(),[&](auto p){return std::abs(p.x-x)<=2 && std::abs(p.y-y)<=2;});};
    for(const auto &feature:map.generation.graph.features)if(feature.locked)
        for(const auto &point:feature.points)protectedCells.push_back({int(point.x),int(point.y)});
    if(mode==3) {
        const float phase=float(seed%17u)*.12f;
        const auto channel=[&](int y){return map.width*.56f+std::sin(float(y)/std::max(1,map.height-1)*5.4f+phase)*map.width*.10f;};
        // Explicit river scenes cross the whole vignette; a short random
        // peripheral stream cannot support a meaningful crossing decision.
        for(int y=0;y<map.height;++y)for(int x=int(channel(y))-1;x<=int(channel(y))+1;++x) {
            const auto *cell=mapCellAt(map,x,y);
            if(!cell || mapGenIsStructureLike(cell->kind) || cell->kind==MapTileKind::Landmark)return false;
            for(const auto &feature:map.generation.graph.features)if(feature.locked)
                for(const auto &point:feature.points)if(int(point.x)==x && int(point.y)==y)return false;
        }
        std::vector<int> crossings;
        MapGeneratedFeature river;river.id=map.metadata.id+"::travel_river";river.kind=MapGeneratedFeatureKind::River;
        river.name=usesGermanNarrative(recipe)?"Reisefluss":"Journey River";river.materialId="fantasy_neutral.river.medium";river.width=1.45f;
        for(int y=0;y<map.height;++y) {
            const float centre=channel(y);const int cx=int(centre);bool crossing=false;
            for(int x=cx-1;x<=cx+1;++x) {
                auto *cell=mapCellAt(map,x,y);if(!cell)continue;
                const bool road=mapGenWayKindIsRoadLike(cell->wayKind);
                if(mapGenIsStructureLike(cell->kind) || cell->kind==MapTileKind::Landmark)continue;
                applyMapCellTerrainDefaults(*cell,road?MapTileKind::Bridge:MapTileKind::Water);
                cell->wayKind=road?MapWayKind::DirtRoad:MapWayKind::River;cell->elevation=map.height-1-y;
                crossing=crossing||road;
            }
            river.points.push_back({centre+.5f,y+.5f});if(crossing)crossings.push_back(y);
        }
        map.generation.graph.features.push_back(std::move(river));
        for(std::size_t i=0;i<crossings.size();) {
            std::size_t end=i+1;while(end<crossings.size() && crossings[end]<=crossings[end-1]+2)++end;
            const int y=crossings[(i+end-1)/2],cx=int(channel(y));
            for(int x=cx-1;x<=cx+1;++x){auto *c=mapCellAt(map,x,y);applyMapCellTerrainDefaults(*c,MapTileKind::Bridge);c->wayKind=MapWayKind::DirtRoad;c->elevation=map.height-1-y;}
            MapGeneratedFeature bridge;bridge.id=map.metadata.id+"::travel_bridge_"+std::to_string(i);bridge.kind=MapGeneratedFeatureKind::Bridge;
            bridge.name=usesGermanNarrative(recipe)?"Reisebrücke":"Journey Bridge";bridge.materialId="fantasy_neutral.travel.bridge";bridge.width=1.f;
            bridge.points={{cx-1.f,y+.5f},{cx+2.f,y+.5f}};map.generation.graph.features.push_back(std::move(bridge));i=end;
        }
        map.metadata.customProperties["travel.water_composition"]="river";return true;
    }
    const bool north=(seed%2u)==0u;
    float basinX=map.width*.7f,basinY=map.height*.78f;
    const float rx=map.width*(.17f+(seed%7u)*.009f),ry=map.height*(.14f+((seed/7u)%5u)*.01f);
    if(mode==2) {
        int best=-1;
        for(float fx:{.25f,.5f,.75f})for(float fy:{.22f,.78f}) {
            int score=0;for(int y=1;y<map.height-1;++y)for(int x=1;x<map.width-1;++x) {
                const float dx=(x+.5f-map.width*fx)/rx,dy=(y+.5f-map.height*fy)/ry;
                if(dx*dx+dy*dy<1.f && clear(x,y))++score;
            }
            if(score>best){best=score;basinX=map.width*fx;basinY=map.height*fy;}
        }
    }
    const float phase=float(seed%97u)*.065f;
    const auto beforeCoast=mode==1?map.cells:std::vector<MapCellModel>{};
    for(int y=0;y<map.height;++y)for(int x=0;x<map.width;++x) {
        bool water=false;
        if(mode==1) {
            const float shore=map.height*(.23f+.07f*std::sin(x*.13f+phase)+.035f*std::sin(x*.31f-phase));
            water=north?y+.5f<shore:map.height-y-.5f<shore;
        } else {
            const float dx=(x+.5f-basinX)/rx,dy=(y+.5f-basinY)/ry;
            const float angle=std::atan2(dy,dx),edge=1.f+.17f*std::sin(angle*3+phase)+.07f*std::cos(angle*5-phase);
            water=dx*dx+dy*dy<edge*edge && x>0 && y>0 && x<map.width-1 && y<map.height-1;
        }
        if(water && clear(x,y)) {
            auto *cell=mapCellAt(map,x,y);applyMapCellTerrainDefaults(*cell,MapTileKind::Water);cell->elevation=0;
        }
    }
    if(mode==1) {
        // Protected routes can cut tiny accidental pools from a new shoreline.
        // Retain only newly painted water connected to the open sea; authored
        // pre-existing water and protected dry cells are never changed here.
        std::vector<unsigned char> reached(map.cells.size(),0);
        std::vector<int> frontier;
        const auto visit=[&](int x,int y){
            const auto *cell=mapCellAt(map,x,y);if(!cell || cell->kind!=MapTileKind::Water)return;
            const int index=y*map.width+x;if(!reached[index]){reached[index]=1;frontier.push_back(index);}
        };
        for(int x=0;x<map.width;++x){visit(x,0);visit(x,map.height-1);}
        for(int y=0;y<map.height;++y){visit(0,y);visit(map.width-1,y);}
        for(std::size_t i=0;i<frontier.size();++i){const int x=frontier[i]%map.width,y=frontier[i]/map.width;visit(x-1,y);visit(x+1,y);visit(x,y-1);visit(x,y+1);}
        for(std::size_t i=0;i<map.cells.size();++i)
            if(map.cells[i].kind==MapTileKind::Water && beforeCoast[i].kind!=MapTileKind::Water && !reached[i])map.cells[i]=beforeCoast[i];
    }
    map.metadata.customProperties["travel.water_composition"]=mode==1?"coast":"lake";
    return true;
}
} // namespace TravelSceneGenerator::detail

