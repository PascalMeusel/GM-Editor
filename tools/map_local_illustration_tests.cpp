#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapClassicIllustration.hpp"
#include "Models/Map/MapRenderPlan.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"
#include "Models/Map/MapDungeonInk.hpp"
#include "Controllers/Editor/Widgets/MapCreator/MapCreatorArtStylePolicy.hpp"
#include <iostream>
#include <cmath>

namespace {
bool insideTriangle(MapFeaturePoint p,const gm::map_render::AtlasInkPrimitive& ink) {
    if(!ink.filled||ink.points.size()!=3)return false;
    auto cross=[](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
    const float a=cross(ink.points[0],ink.points[1],p),b=cross(ink.points[1],ink.points[2],p),c=cross(ink.points[2],ink.points[0],p);
    return (a>=0&&b>=0&&c>=0)||(a<=0&&b<=0&&c<=0);
}
}
int main() {
    using namespace gm::map_render;
    int failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    MapModel map;map.width=12;map.height=12;map.scope=MapScope::Local;map.atlasLevel=MapLevel::Tactical;map.cells.resize(144);
    auto &recipe=map.generation.recipe;recipe.generatorVersion=12;recipe.scale=MapLevel::Tactical;
    applyMapCreatorArtStyle(recipe,MapCreatorArtStyle::Illustrated);
    check(usesIllustratedLocalV6(map)&&mapCreatorArtStyleMatches(recipe,MapCreatorArtStyle::Illustrated),"New local style cannot be selected consistently");
    for(auto &c:map.cells){c.kind=MapTileKind::Water;c.fogged=false;}
    for(int y=4;y<8;++y)for(int x=4;x<8;++x)map.cells[y*12+x].kind=MapTileKind::Forest;
    auto water=buildLocalIllustrationWaterInk(map,false);
    check(!water.empty(),"Water mesh missing");
    for(int y=0;y<12;++y)for(int x=0;x<12;++x){
        const MapFeaturePoint p{x+.5f,y+.5f};bool covered=false;
        for(const auto& primitive:water)covered|=insideTriangle(p,primitive);
        check(covered==(map.cells[y*12+x].kind==MapTileKind::Water),"Water mesh erased a dry island or missed a water cell centre");
    }
    check(!buildLocalIllustrationInk(map,false).empty(),"Forest island has no illustrated canopy");
    for(auto &c:map.cells)c.fogged=true;
    check(buildLocalIllustrationWaterInk(map,true).empty()&&buildLocalIllustrationInk(map,true).empty(),"Fully fogged terrain leaked ink");
    recipe.generatorVersion=11;recipe.styleVersion=5;recipe.styleSnapshotId="illustrated_atlas_v5";
    check(!usesIllustratedLocalV6(map)&&buildLocalIllustrationWaterInk(map,false).empty(),"Legacy local map opted into V6");
    recipe.scale=MapLevel::World;recipe.generatorVersion=12;applyMapCreatorArtStyle(recipe,MapCreatorArtStyle::Illustrated);
    check(recipe.styleVersion==5&&recipe.styleSnapshotId=="illustrated_atlas_v5","Approved world art changed with local artwork selection");
    recipe.scale=MapLevel::Region;applyMapCreatorArtStyle(recipe,MapCreatorArtStyle::Illustrated);map.atlasLevel=MapLevel::Region;
    MapGeneratedFeature town;town.kind=MapGeneratedFeatureKind::Settlement;town.points={{6,6}};map.generation.graph.features.push_back(town);
    check(detail::overviewForestPointNeedsClearance(map,{8,6}),"Regional town clearing still crowds its labels");
    recipe.generatorVersion=11;recipe.styleVersion=5;recipe.styleSnapshotId="illustrated_atlas_v5";
    check(!detail::overviewForestPointNeedsClearance(map,{8,6}),"New clearing changed legacy maps");
    map.scope=MapScope::Dungeon;
    for(auto &c:map.cells)c.kind=MapTileKind::Empty;
    const auto solid=illustratedLocalTerrainColor(map,2.8f,5.5f);
    for(int y=3;y<9;++y)for(int x=4;x<9;++x)mapCellAt(map,x,y)->kind=MapTileKind::Dirt;
    const auto outside=illustratedLocalTerrainColor(map,2.8f,5.5f);
    check(solid.r==outside.r&&solid.g==outside.g&&solid.b==outside.b,"Dungeon floor casts an outdoor terrain halo into solid void");
    map.creationIntent=MapCreationIntent::Cave;map.atlasLevel=MapLevel::Site;
    recipe.generatorVersion=12;recipe.styleVersion=6;recipe.styleSnapshotId="illustrated_atlas_v6";
    for(auto& cell:map.cells){cell.fogged=false;cell.kind=MapTileKind::Wall;}
    for(int y=3;y<9;++y)for(int x=3;x<9;++x)mapCellAt(map,x,y)->kind=MapTileKind::Cave;
    const auto caveWithoutStairs=buildNaturalCaveInk(map,false);
    auto* stairs=mapCellAt(map,5,5);stairs->kind=MapTileKind::Stairs;
    for(int turn=0;turn<4;++turn) {
        stairs->rotationQuarterTurns=turn;
        const auto caveWithStairs=buildNaturalCaveInk(map,false);
        check(caveWithStairs.size()>caveWithoutStairs.size(),"Cave entrance has no visible architecture");
        for(std::size_t i=caveWithoutStairs.size();i<caveWithStairs.size();++i)
            for(auto point:caveWithStairs[i].points)
                check(std::isfinite(point.x)&&std::isfinite(point.y)&&point.x>=5&&point.x<=6&&point.y>=5&&point.y<=6,
                      "Rotated cave stair illustration spills outside its semantic cell");
        check(stairs->kind==MapTileKind::Stairs,"Cave entrance drawing mutated traversability");
    }
    for(auto& cell:map.cells)cell.fogged=true;
    check(buildNaturalCaveInk(map,true).empty(),"Hidden cave entrance leaks through player fog");
    for(int kind=0;kind<=static_cast<int>(MapTileKind::Bridge);++kind) {
        const auto context=makeMapTileRenderContext(static_cast<MapTileKind>(kind),0,7,9);
        const auto ink=classicCellInk(context);
        check(!ink.empty(),"Classic terrain has no illustrated fallback");
        for(const auto& primitive:ink)for(auto point:primitive.points)
            check(std::isfinite(point.x)&&std::isfinite(point.y)&&point.x>=7&&point.x<=8&&point.y>=9&&point.y<=10,
                  "Classic illustration escaped its editable cell");
        check(context.kind==static_cast<MapTileKind>(kind),"Classic renderer mutated terrain");
    }
    MapModel crossing;crossing.width=4;crossing.height=4;resizeMapCells(crossing);
    for(auto& cell:crossing.cells)cell.wayKind=MapWayKind::DirtRoad;
    const auto crossInk=classicWayInk(makeMapWayRenderContext(crossing,1,1),makeMapTileRenderContext(crossing,1,1));
    for(const auto& primitive:crossInk)for(std::size_t i=1;i<primitive.points.size();++i)
        check(primitive.points[i].x==primitive.points[i-1].x||primitive.points[i].y==primitive.points[i-1].y,
              "Classic crossing reintroduced diagonal star spokes");
    auto oldRoad=makeMapTileRenderContext(MapTileKind::Road,0,1,1);
    MapWayRenderContext noWay;noWay.tileX=1;noWay.tileY=1;noWay.terrainKind=MapTileKind::Road;
    check(!classicWayInk(noWay,oldRoad).empty(),"Terrain-only legacy road disappeared");
    {
        MapModel worn;worn.width=12;worn.height=6;worn.scope=MapScope::Local;worn.atlasLevel=MapLevel::Tactical;resizeMapCells(worn);
        worn.generation.recipe.generatorVersion=12;worn.generation.recipe.scale=MapLevel::Tactical;
        applyMapCreatorArtStyle(worn.generation.recipe,MapCreatorArtStyle::Illustrated);
        MapGeneratedFeature road;road.id="worn-road";road.kind=MapGeneratedFeatureKind::Road;road.width=1.f;road.points={{1,3},{10,3}};
        worn.generation.recipe.recipeVersion=4;
        check(buildIllustratedLocalRoadDetailInk(worn,road,road.points).empty(),"Older road artwork must remain unchanged");
        worn.generation.recipe.recipeVersion=5;
        const auto detail=buildIllustratedLocalRoadDetailInk(worn,road,road.points);
        check(detail.size()>3,"New roads need visible but restrained wear detail");
        for(const auto& primitive:detail)for(const auto p:primitive.points)
            check(p.x>=1.f && p.x<=10.f && p.y>=2.f && p.y<=4.f,"Road wear escaped the route and could cover neighboring terrain");
        worn.generation.graph.features.push_back(road);
        MapRenderPlanRequest request;request.applyFog=false;
        const auto plan=compileMapRenderPlan(worn,request);
        bool retained=false;
        for(const auto& primitive:plan.primitives)if(primitive.sourceId==road.id)
            retained=retained || primitive.detailInk.size()==detail.size();
        check(retained,"Editor and export must receive the same precompiled road wear");
    }
    {
        MapModel roofs;roofs.width=8;roofs.height=8;roofs.scope=MapScope::Local;roofs.atlasLevel=MapLevel::Tactical;resizeMapCells(roofs);
        roofs.generation.recipe.generatorVersion=12;roofs.generation.recipe.scale=MapLevel::Tactical;
        applyMapCreatorArtStyle(roofs.generation.recipe,MapCreatorArtStyle::Illustrated);
        for(auto& cell:roofs.cells){cell.kind=MapTileKind::Dirt;cell.fogged=false;}
        for(const auto size : {std::pair{3,2},std::pair{2,3},std::pair{1,1}}) {
            for(auto& cell:roofs.cells)cell.kind=MapTileKind::Dirt;
            for(int y=2;y<2+size.second;++y)for(int x=2;x<2+size.first;++x)mapCellAt(roofs,x,y)->kind=MapTileKind::Building;
            roofs.generation.recipe.recipeVersion=4;
            const auto legacy=buildTacticalStructureInk(roofs);
            roofs.generation.recipe.recipeVersion=5;
            const auto refined=buildTacticalStructureInk(roofs);
            check(refined.size()>legacy.size(),"Shingle replacement missing on a supported roof orientation");
            for(const auto& primitive:refined)for(const auto p:primitive.points)
                check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=2&&p.x<=2+size.first&&p.y>=2&&p.y<=2+size.second,
                      "Replacement roof art escaped its blocked footprint");
        }
    }
    {
        MapModel botanical;botanical.width=12;botanical.height=12;
        botanical.scope=MapScope::Local;botanical.atlasLevel=MapLevel::Tactical;
        botanical.generation.recipe.generatorVersion=12;botanical.generation.recipe.scale=MapLevel::Tactical;
        applyMapCreatorArtStyle(botanical.generation.recipe,MapCreatorArtStyle::Illustrated);
        resizeMapCells(botanical);
        for(auto &cell:botanical.cells){cell.kind=MapTileKind::Forest;cell.fogged=false;}
        botanical.generation.recipe.recipeVersion=9;
        const auto old=buildLocalIllustrationInk(botanical,false);
        botanical.generation.recipe.recipeVersion=10;
        const auto fresh=buildLocalIllustrationInk(botanical,false);
        const auto repeated=buildLocalIllustrationInk(botanical,false);
        check(fresh.size()>old.size() && fresh.size()==repeated.size(),"Botanical crown detail must be present and deterministic");
        check(std::any_of(fresh.begin(),fresh.end(),[](const auto &p){return p.vertexColors.size()==3;}),
              "Botanical crown lacks its shared graduated surface");
        for(std::size_t i=0;i<fresh.size();++i) {
            const auto &a=fresh[i],&b=repeated[i];
            check(a.points.size()==b.points.size() && a.vertexColors.size()==b.vertexColors.size(),"Crown topology changed between equal builds");
            for(std::size_t j=0;j<a.points.size();++j) {
                const auto p=a.points[j];
                check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<=12&&p.y<=12,"Botanical canopy escaped its cleared footprint");
                check(p.x==b.points[j].x && p.y==b.points[j].y,"Botanical canopy placement is unstable");
            }
        }
        botanical.generation.recipe.recipeVersion=9;
        check(buildLocalIllustrationInk(botanical,false).size()==old.size(),"New foliage changed the old recipe");
        botanical.generation.recipe.recipeVersion=10;
        for(auto &cell:botanical.cells)cell.fogged=true;
        check(buildLocalIllustrationInk(botanical,true).empty(),"Recipe-10 foliage leaked through complete fog");

        for(auto &cell:botanical.cells){cell.kind=MapTileKind::Empty;cell.fogged=false;}
        MapGeneratedFeature road;road.id="reviewed-natural-road";road.kind=MapGeneratedFeatureKind::Road;
        road.points={{1,6},{11,6}};road.width=1.f;road.discovered=true;
        const auto roadInk=buildIllustratedLocalRoadDetailInk(botanical,road,road.points);
        check(std::any_of(roadInk.begin(),roadInk.end(),[](const auto &p){return p.vertexColors.size()==3;}),"Natural road surface missing");
        for(const auto &primitive:roadInk)for(const auto p:primitive.points)
            check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=.25f&&p.x<=11.75f&&p.y>=5.25f&&p.y<=6.75f,
                  "Natural road texture escaped its bounded corridor");
        check(buildIllustratedLocalRoadDetailInk(botanical,road,{{3,3},{3,3}}).empty(),"Zero-length road generated invalid surface geometry");
        const auto longRoad=buildIllustratedLocalRoadDetailInk(botanical,road,{{1,6},{100001,6}});
        check(longRoad.size()<30000,"Natural road mesh work is not bounded for long authored paths");
        botanical.visibility=MapVisibility::PlayersVisible;
        botanical.playerRelease.waysAndRoutes=true;
        botanical.generation.graph.features={road};
        MapRenderPlanRequest playerRequest;playerRequest.audience=MapRenderAudience::Player;
        const auto publicPlan=compileMapRenderPlan(botanical,playerRequest);
        check(mapRenderPlanContainsGeneratedGeometry(publicPlan,road.id),"Reviewed natural road absent from player plan");
        botanical.generation.graph.features.front().gmOnly=true;
        const auto hiddenPlan=compileMapRenderPlan(botanical,playerRequest);
        check(!mapRenderPlanContainsGeneratedGeometry(hiddenPlan,road.id),"Natural road material leaked a GM-only route");
        botanical.generation.graph.features.clear();
        const auto emptyPlan=compileMapRenderPlan(botanical,playerRequest);
        check(hiddenPlan.contentHash==emptyPlan.contentHash,"Private road changed the player's reviewed content hash");
    }
    {
        MapModel sanctuary;sanctuary.width=12;sanctuary.height=12;sanctuary.scope=MapScope::Local;
        sanctuary.atlasLevel=MapLevel::Tactical;sanctuary.generation.recipe.generatorVersion=12;
        sanctuary.generation.recipe.scale=MapLevel::Tactical;
        applyMapCreatorArtStyle(sanctuary.generation.recipe,MapCreatorArtStyle::Illustrated);
        sanctuary.generation.recipe.recipeVersion=10;resizeMapCells(sanctuary);
        for(auto &cell:sanctuary.cells){cell.kind=MapTileKind::Stone;cell.fogged=false;}
        MapGeneratedFeature apron;apron.kind=MapGeneratedFeatureKind::TacticalSetpiece;
        apron.materialId="fantasy_neutral.tactical.sanctuary";apron.width=4;apron.points={{6,6}};
        sanctuary.generation.graph.features={apron};
        for(int x=0;x<12;++x){auto *cell=mapCellAt(sanctuary,x,6);cell->kind=MapTileKind::Road;cell->wayKind=MapWayKind::DirtRoad;}
        const auto paving=buildTacticalStructureInk(sanctuary);
        check(!paving.empty(),"Weathered sanctuary has no stone surface");
        for(const auto &primitive:paving)if(primitive.filled) {
            MapFeaturePoint center{};
            for(const auto p:primitive.points){center.x+=p.x/primitive.points.size();center.y+=p.y/primitive.points.size();}
            check(std::floor(center.y)!=6.f,"Sanctuary paving covered an edited route");
        }
        sanctuary.generation.recipe.recipeVersion=9;
        const auto legacy=buildTacticalStructureInk(sanctuary);
        check(std::none_of(legacy.begin(),legacy.end(),[](const auto &p){return !p.vertexColors.empty();}),"New flagstone surface entered an older recipe");
    }
    {
        MapModel masonry;masonry.width=12;masonry.height=12;masonry.scope=MapScope::Local;
        masonry.atlasLevel=MapLevel::Tactical;masonry.generation.recipe.generatorVersion=12;
        masonry.generation.recipe.scale=MapLevel::Tactical;
        applyMapCreatorArtStyle(masonry.generation.recipe,MapCreatorArtStyle::Illustrated);
        masonry.metadata.customProperties["tactical.template_kind"]="5";resizeMapCells(masonry);
        for(auto &cell:masonry.cells){cell.kind=MapTileKind::Stone;cell.fogged=false;}
        masonry.generation.recipe.recipeVersion=9;
        check(buildLocalIllustrationInk(masonry,false).empty(),"Civic paving changed a pinned legacy map");
        masonry.generation.recipe.recipeVersion=10;
        check(!buildLocalIllustrationInk(masonry,false).empty(),"Current civic plaza has no material surface");
        masonry.visibility=MapVisibility::PlayersVisible;masonry.playerRelease.placesAndMarkers=true;
        MapGeneratedFeature privateBuilding;privateBuilding.id="private-plaza-building";
        privateBuilding.kind=MapGeneratedFeatureKind::BuildingGroup;privateBuilding.points={{4.5f,4.5f}};
        privateBuilding.gmOnly=true;privateBuilding.discovered=true;
        MapRenderPlanRequest player;player.audience=MapRenderAudience::Player;
        const auto emptyPlaza=compileMapRenderPlan(masonry,player);
        for(int y=4;y<7;++y)for(int x=4;x<7;++x)mapCellAt(masonry,x,y)->kind=MapTileKind::Building;
        masonry.generation.graph.features={privateBuilding};
        const auto hiddenPlaza=compileMapRenderPlan(masonry,player);
        const auto visiblePlaza=filterMapForRenderPlan(masonry,hiddenPlaza);
        check(std::all_of(visiblePlaza.cells.begin(),visiblePlaza.cells.end(),[](const auto &cell){return cell.kind==MapTileKind::Stone;}),
              "Private city building left a hole in the public paving");
        // The review hash intentionally includes authoritative source-cell
        // edits. Compare the released geometry, rather than requiring that
        // private authoring changes leave the review token unchanged.
        bool sameInk=emptyPlaza.atlasInk.size()==hiddenPlaza.atlasInk.size();
        for(std::size_t i=0;sameInk && i<emptyPlaza.atlasInk.size();++i) {
            const auto &a=emptyPlaza.atlasInk[i],&b=hiddenPlaza.atlasInk[i];
            sameInk=a.points.size()==b.points.size() && a.color.r==b.color.r && a.color.g==b.color.g && a.color.b==b.color.b;
            for(std::size_t j=0;sameInk && j<a.points.size();++j)
                sameInk=a.points[j].x==b.points[j].x && a.points[j].y==b.points[j].y;
        }
        check(sameInk && hiddenPlaza.atlasFeatureInk.empty(),"Private plaza building changed visible paving or left roof art");
        check(mapCellAt(masonry,5,5)->kind==MapTileKind::Building,"Player paving filtering changed authored geometry");
        masonry.generation.graph.features.clear();
        for(auto &cell:masonry.cells)cell.kind=MapTileKind::Stone;
        for(int y=0;y<12;++y) {
            mapCellAt(masonry,2,y)->kind=MapTileKind::Water;
            mapCellAt(masonry,4,y)->kind=MapTileKind::Building;
            mapCellAt(masonry,6,y)->kind=MapTileKind::Empty;
            mapCellAt(masonry,8,y)->fogged=true;
        }
        std::vector<AtlasInkPrimitive> paving;appendCivicPavingInk(paving,masonry,true);
        check(!paving.empty(),"Edited civic plaza lost all valid paving");
        for(const auto &primitive:paving)for(const auto p:primitive.points) {
            const auto *c=mapCellAt(masonry,int(std::floor(p.x)),int(std::floor(p.y)));
            check(c && c->kind==MapTileKind::Stone && !c->fogged,"Civic paving covered edited terrain or hidden ground");
        }
        for(auto &cell:masonry.cells)cell.fogged=true;
        paving.clear();appendCivicPavingInk(paving,masonry,true);
        check(paving.empty(),"Completely fogged civic paving leaked");
        masonry.metadata.customProperties["tactical.template_kind"]="3";
        for(auto &cell:masonry.cells){cell.kind=MapTileKind::Empty;cell.fogged=false;}
        for(int y=4;y<7;++y)for(int x=4;x<7;++x)mapCellAt(masonry,x,y)->kind=MapTileKind::Wall;
        mapCellAt(masonry,5,5)->kind=MapTileKind::Empty;
        for(int y=4;y<7;++y)for(int x=4;x<7;++x) {
            std::vector<AtlasInkPrimitive> wall;appendWeatheredRuinWallInk(wall,masonry,x,y,true);
            check(wall.empty()==(x==5 && y==5),"Weathered wall filled an edited opening or omitted an obstacle");
            for(const auto &primitive:wall)for(const auto p:primitive.points)
                check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=x&&p.x<=x+1&&p.y>=y&&p.y<=y+1,
                      "Weathered wall escaped its exact blocked cell");
        }
        mapCellAt(masonry,4,4)->fogged=true;
        std::vector<AtlasInkPrimitive> hiddenWall;appendWeatheredRuinWallInk(hiddenWall,masonry,4,4,true);
        check(hiddenWall.empty(),"Hidden wall emitted material ink");
        for(auto &cell:masonry.cells){cell.kind=MapTileKind::Stone;cell.fogged=false;}
        for(int y=5;y<=7;++y)for(int x=5;x<=7;++x)mapCellAt(masonry,x,y)->kind=MapTileKind::Water;
        std::vector<AtlasInkPrimitive> fountain;appendFittedFountainInk(fountain,masonry);
        check(!fountain.empty(),"Revealed central basin has no fitted fountain");
        const auto basinContours=localIllustrationWaterContours(masonry,true);
        check(!basinContours.empty() && fountain.front().points.size()==basinContours.front().size()+1,
              "Fountain coping is disconnected from the actual water contour");
        for(std::size_t i=0;i<basinContours.front().size();++i)
            check(fountain.front().points[i].x==basinContours.front()[i].x && fountain.front().points[i].y==basinContours.front()[i].y,
                  "Fountain edge moved independently of the water mesh");
        mapCellAt(masonry,5,5)->fogged=true;fountain.clear();appendFittedFountainInk(fountain,masonry);
        check(fountain.empty(),"Partially hidden fountain disclosed its complete outline");
        for(auto &cell:masonry.cells){cell.kind=MapTileKind::Water;cell.fogged=false;}
        fountain.clear();appendFittedFountainInk(fountain,masonry);
        check(fountain.empty(),"Edited lake acquired a misleading small fountain rim");
    }
    {
        MapModel underground;underground.width=12;underground.height=12;resizeMapCells(underground);
        underground.scope=MapScope::Dungeon;underground.atlasLevel=MapLevel::Site;
        underground.creationIntent=MapCreationIntent::Cave;
        auto &r=underground.generation.recipe;r.generatorVersion=12;r.recipeVersion=9;r.scale=MapLevel::Site;
        applyMapCreatorArtStyle(r,MapCreatorArtStyle::Illustrated);
        for(auto &cell:underground.cells){cell.kind=MapTileKind::Wall;cell.fogged=false;}
        for(int y=2;y<10;++y)for(int x=2;x<10;++x)mapCellAt(underground,x,y)->kind=MapTileKind::Cave;
        for(int y=5;y<7;++y)for(int x=5;x<7;++x)mapCellAt(underground,x,y)->kind=MapTileKind::Wall;
        const auto legacy=buildNaturalCaveFloorInk(underground,true);
        check(std::none_of(legacy.begin(),legacy.end(),[](const auto &p){return !p.vertexColors.empty();}),
              "Recipe 9 cave adopted the new material wash");
        r.recipeVersion=10;
        const auto current=buildNaturalCaveFloorInk(underground,true);
        check(std::any_of(current.begin(),current.end(),[](const auto &p){return p.vertexColors.size()==3;}),
              "Current cave floor has no material shading");
        const auto groundAt=[&](const auto &ink,MapFeaturePoint point){
            bool ground=false;
            for(const auto &p:ink)if(p.color.a==255 && insideTriangle(point,p))
                ground=p.color.r>100;
            return ground;
        };
        // Sample on both sides of the rounded boundary and solid island,
        // independently of the tessellation chosen for the material shader.
        for(int iy=0;iy<60;++iy)for(int ix=0;ix<60;++ix) {
            const MapFeaturePoint p{ix*.2f+.073f,iy*.2f+.091f};
            check(groundAt(legacy,p)==groundAt(current,p),"Cave material moved a floor edge or filled a solid island");
        }
        for(auto &cell:underground.cells)cell.fogged=true;
        check(buildNaturalCaveFloorInk(underground,true).empty(),"Cave wash disclosed hidden room geometry");
        underground.creationIntent=MapCreationIntent::Dungeon;
        for(auto &cell:underground.cells){cell.kind=MapTileKind::Room;cell.fogged=false;}
        mapCellAt(underground,5,5)->kind=MapTileKind::Wall;
        mapCellAt(underground,6,5)->fogged=true;
        const auto slabs=buildDungeonFloorInk(underground,true);
        // Architecture also belongs to this pass; identify pale floor faces
        // separately from the deliberately darker wall cross-sections.
        for(const auto &p:slabs)if(!p.vertexColors.empty() && p.color.r>=160 && p.color.g>=150) {
            MapFeaturePoint center{};for(const auto v:p.points){center.x+=v.x/p.points.size();center.y+=v.y/p.points.size();}
            const auto *cell=mapCellAt(underground,int(center.x),int(center.y));
            check(cell && cell->kind==MapTileKind::Room && !cell->fogged,
                  "Dungeon slab shading crossed a blocked or fogged cell");
        }
    }
    {
        MapModel roofs;roofs.width=12;roofs.height=12;resizeMapCells(roofs);
        roofs.scope=MapScope::Local;roofs.atlasLevel=MapLevel::Tactical;
        auto &r=roofs.generation.recipe;r.generatorVersion=12;r.scale=MapLevel::Tactical;
        applyMapCreatorArtStyle(r,MapCreatorArtStyle::Illustrated);
        for(const auto dimensions:{std::pair{1,1},std::pair{1,6},std::pair{6,1},std::pair{3,6},std::pair{6,3},std::pair{5,5}})
        {
            for(auto &cell:roofs.cells){cell.kind=MapTileKind::Empty;cell.fogged=false;}
            for(int y=2;y<2+dimensions.second;++y)for(int x=2;x<2+dimensions.first;++x)
                mapCellAt(roofs,x,y)->kind=MapTileKind::Building;
            // Exercise a concave authored footprint as well as rectangles.
            if(dimensions.first==5)mapCellAt(roofs,6,6)->kind=MapTileKind::Empty;
            r.recipeVersion=9;const auto before=buildTacticalStructureInk(roofs);
            r.recipeVersion=10;const auto after=buildTacticalStructureInk(roofs);
            check(std::none_of(before.begin(),before.end(),[](const auto &p){return !p.vertexColors.empty();}),
                  "Saved roof recipe adopted new material shading");
            check(std::any_of(after.begin(),after.end(),[](const auto &p){return p.vertexColors.size()==3;}),
                  "Current roofs lack continuous material shading");
            for(const auto &primitive:after)for(const auto point:primitive.points) {
                const auto *cell=mapCellAt(roofs,int(std::floor(point.x)),int(std::floor(point.y)));
                check(std::isfinite(point.x)&&std::isfinite(point.y)&&cell&&cell->kind==MapTileKind::Building,
                      "Roof material escaped an edited building footprint");
            }
            const auto covered=[&](const auto &ink,MapFeaturePoint point){
                return std::any_of(ink.begin(),ink.end(),[&](const auto &p){return insideTriangle(point,p);});
            };
            for(int y=0;y<48;++y)for(int x=0;x<48;++x) {
                const MapFeaturePoint point{x*.25f+.113f,y*.25f+.071f};
                check(covered(before,point)==covered(after,point),"Roof material changed the roof footprint");
            }
        }
    }
    std::cout<<"Local illustration: "<<failures<<" failures\n";return failures?1:0;
}
