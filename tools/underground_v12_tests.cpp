#include "Models/Map/MapGeneration.hpp"
#include "Models/Map/MapCaveInk.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Models/Map/MapCaveCompositionV12.hpp"
#include "Models/Map/MapDungeonCompositionV12.hpp"
#include <iostream>
#include <cmath>
#include <set>

static bool check(bool value,const char *message) {if(!value)std::cerr<<message<<'\n';return value;}
static int waterCount(const MapModel &m){return static_cast<int>(std::count_if(m.cells.begin(),m.cells.end(),[](const auto &c){return c.kind==MapTileKind::Water;}));}
static bool dryConnected(const MapModel& map,MapCellCoordinate start,MapCellCoordinate goal) {
    std::vector<MapCellCoordinate> queue{start};std::vector<bool> seen(map.cells.size(),false);
    seen[start.y*map.width+start.x]=true;
    constexpr int dx[]={1,0,-1,0},dy[]={0,1,0,-1};
    for(std::size_t i=0;i<queue.size();++i) {
        const auto p=queue[i];if(p.x==goal.x&&p.y==goal.y)return true;
        for(int d=0;d<4;++d) {
            const int x=p.x+dx[d],y=p.y+dy[d];const auto* cell=mapCellAt(map,x,y);
            if(!cell||cell->kind==MapTileKind::Water||!mapGenQualityTraversable(*cell)||seen[y*map.width+x])continue;
            seen[y*map.width+x]=true;queue.push_back({x,y});
        }
    }
    return false;
}
int main()
{
    bool ok=true;
    MapGenerationOptions o;o.generatorVersion=12;o.styleId="illustrated_atlas";o.scope=MapScope::Dungeon;o.scale=MapLevel::Tactical;o.creationIntent=MapCreationIntent::Cave;o.caveNetwork=true;o.width=48;o.height=36;o.tacticalDensity=30;o.routeDensity=55;o.waterLevel=65;
    for(unsigned seed=1;seed<=30;++seed)
    {
        o.seed=seed*74719;auto a=mapGenGenerateCandidate(o).map,b=mapGenGenerateCandidate(o).map;
        ok&=check(a.generation.recipe.styleVersion==6,"V12 local illustrated style was not pinned");
        std::vector<MapCellCoordinate> stairs;
        for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x)
        {
            const auto *c=mapCellAt(a,x,y),*d=mapCellAt(b,x,y);
            ok&=check(c->kind==d->kind&&c->wayKind==d->wayKind,"V12 basin not deterministic");
            if(c->kind==MapTileKind::Stairs)stairs.push_back({x,y});
        }
        ok&=check(stairs.size()==2,"Cave entrances changed");
        if(stairs.size()==2)ok&=check(mapGenCoordinatesConnected(a,stairs[0],stairs[1],false),"Cave dry traversal disconnected");
        std::set<int> openEdges;
        for(int y=0;y<a.height;++y)for(int x=0;x<a.width;++x) {
            if(x!=0&&y!=0&&x!=a.width-1&&y!=a.height-1)continue;
            if(mapCellAt(a,x,y)->kind!=MapTileKind::Cave)continue;
            openEdges.insert(x==0?0:x==a.width-1?1:y==0?2:3);
            if(!stairs.empty())ok&=check(dryConnected(a,stairs[0],{x,y}),"Cave mouth has no dry connection to its stairs");
        }
        ok&=check(openEdges.size()>=2,"Cave has no distinct surface entrance and exit");
        for(const auto &p:gm::map_render::buildNaturalCaveInk(a,false))for(auto q:p.points)
            ok&=check(std::isfinite(q.x)&&std::isfinite(q.y),"Nonfinite cave art geometry");
        for(auto &c:a.cells)c.fogged=true;
        ok&=check(gm::map_render::buildNaturalCaveFloorInk(a,true).empty()&&gm::map_render::buildNaturalCavePoolInk(a,true).empty()&&gm::map_render::buildNaturalCaveSymbolInk(a,true).empty()&&gm::map_render::buildNaturalCaveInk(a,true).empty(),"V6 cave art leaks through full fog");
    }
    // Controlled pocket fixture checks actual slider behavior and the protected
    // dry backbone independently of candidate ranking or random pool placement.
    MapModel fixture;fixture.width=30;fixture.height=24;fixture.generation.recipe.generatorVersion=12;fixture.generation.recipe.seed=417;
    resizeMapCells(fixture);mapGenClear(fixture,MapTileKind::Wall);
    for(int y=5;y<=18;++y)for(int x=4;x<=24;++x)mapGenSetCell(fixture,x,y,MapTileKind::Cave);
    for(int y=3;y<=20;++y){mapGenSetCell(fixture,14,y,MapTileKind::Cave);mapCellAt(fixture,14,y)->wayKind=MapWayKind::CaveTunnel;}
    mapGenSetCell(fixture,8,12,MapTileKind::Water);mapGenSetCell(fixture,14,4,MapTileKind::Stairs);
    auto low=fixture,high=fixture,legacy=fixture;legacy.generation.recipe.generatorVersion=11;
    auto entranceFixture=fixture;entranceFixture.creationIntent=MapCreationIntent::Cave;
    auto lockedEntrance=entranceFixture;lockedEntrance.generation.recipe.lockedFeatureIds.push_back("authored_chamber");
    mapComposeCaveEntrancesV12(lockedEntrance,12);
    auto legacyEntrance=entranceFixture;mapComposeCaveEntrancesV12(legacyEntrance,11);
    for(std::size_t i=0;i<fixture.cells.size();++i)
        ok&=check(lockedEntrance.cells[i].kind==fixture.cells[i].kind&&legacyEntrance.cells[i].kind==fixture.cells[i].kind,"Surface entrance excavation altered locked or legacy map");
    const int poolBefore=waterCount(entranceFixture);mapComposeCaveEntrancesV12(entranceFixture,12);
    ok&=check(waterCount(entranceFixture)==poolBefore&&mapCellAt(entranceFixture,14,4)->kind==MapTileKind::Stairs,"Surface entrance excavation erased water or stairs");
    mapComposeCaveBasinsV12(low,0);mapComposeCaveBasinsV12(high,75);mapComposeCaveBasinsV12(legacy,75);
    ok&=check(waterCount(low)==1&&waterCount(legacy)==1,"Zero-water/legacy composition was modified");
    ok&=check(waterCount(high)>35,"High water did not create a meaningful basin");
    for(int y=3;y<=20;++y)ok&=check(mapCellAt(high,14,y)->kind!=MapTileKind::Water,"Water replaced protected tunnel");
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)ok&=check(mapCellAt(high,14+dx,4+dy)->kind!=MapTileKind::Water,"Water flooded a stair approach");
    auto noPool=fixture;mapGenSetCell(noPool,8,12,MapTileKind::Cave);
    mapComposeCaveBasinsV12(noPool,50);
    ok&=check(waterCount(noPool)>10,"Positive water recipe with no initial seed stayed dry");
    o.creationIntent=MapCreationIntent::Dungeon;o.caveNetwork=false;o.waterLevel=0;
    auto geology=fixture;mapGenSetCell(geology,6,8,MapTileKind::Cover);mapGenSetCell(geology,10,8,MapTileKind::Cover);mapGenSetCell(geology,9,11,MapTileKind::Cover);
    const auto geologyBefore=geology;
    mapComposeCaveGeologyV12(geology,12,417);
    int rockCount=0,changedCells=0;
    for(std::size_t i=0;i<geology.cells.size();++i){rockCount+=geology.cells[i].kind==MapTileKind::Cover;changedCells+=geology.cells[i].kind!=geologyBefore.cells[i].kind;}
    ok&=check(rockCount==3&&changedCells>0,"Geology did not group existing cover with preserved count");
    for(int y=3;y<=20;++y)ok&=check(mapCellAt(geology,14,y)->kind==mapCellAt(geologyBefore,14,y)->kind,"Geology altered protected tunnel");
    auto legacyGeology=geologyBefore;mapComposeCaveGeologyV12(legacyGeology,11,417);
    for(std::size_t i=0;i<geology.cells.size();++i)ok&=check(legacyGeology.cells[i].kind==geologyBefore.cells[i].kind,"Legacy geology moved");
    auto dungeon=mapGenGenerateCandidate(o).map;
    const auto beforeFurniture=dungeon;
    const auto coverCount=[](const MapModel&m){return std::count_if(m.cells.begin(),m.cells.end(),[](const auto&c){return c.kind==MapTileKind::Cover;});};
    mapComposeDungeonFurnitureV12(dungeon,12,o.seed);
    ok&=check(coverCount(dungeon)==coverCount(beforeFurniture),"Room furnishing changed detail count");
    for(std::size_t i=0;i<dungeon.cells.size();++i) {
        if(beforeFurniture.cells[i].wayKind!=MapWayKind::None || beforeFurniture.cells[i].kind==MapTileKind::Door || beforeFurniture.cells[i].kind==MapTileKind::Stairs)
            ok&=check(dungeon.cells[i].kind==beforeFurniture.cells[i].kind,"Furniture changed authored traversal");
    }
    auto legacyFurniture=beforeFurniture;mapComposeDungeonFurnitureV12(legacyFurniture,11,o.seed);
    for(std::size_t i=0;i<dungeon.cells.size();++i)ok&=check(legacyFurniture.cells[i].kind==beforeFurniture.cells[i].kind,"Legacy furniture moved");
    const auto wallArt=gm::map_render::buildDungeonFloorInk(dungeon,false);
    ok&=check(!wallArt.empty(),"Missing illustrated dungeon walls/floor");
    for(const auto &p:wallArt)for(auto q:p.points)ok&=check(std::isfinite(q.x)&&std::isfinite(q.y),"Nonfinite dungeon wall corner");
    for(auto &c:dungeon.cells)c.fogged=true;
    ok&=check(gm::map_render::buildDungeonFloorInk(dungeon,true).empty()&&gm::map_render::buildDungeonPropInk(dungeon,true).empty()&&gm::map_render::buildDungeonArchitectureInk(dungeon,true).empty(),"V6 dungeon art leaks through full fog");
    std::cout<<(ok?"Underground V12 regressions passed\n":"Underground V12 regressions FAILED\n");return ok?0:1;
}
