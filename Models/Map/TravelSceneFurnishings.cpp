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
void composeTravelFurnishingsV4(MapModel &map, const TravelSceneRecipe &recipe)
{
    if (recipe.schemaVersion < 4) return;
    const auto focus = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(), [](const auto &f) {
        return f.materialId.rfind("fantasy_neutral.travel.", 0) == 0 && !f.points.empty() &&
               (f.kind==MapGeneratedFeatureKind::Landmark || f.kind==MapGeneratedFeatureKind::Settlement);
    });
    if (focus == map.generation.graph.features.end()) return;
    const int cx=static_cast<int>(focus->points.front().x), cy=static_cast<int>(focus->points.front().y);
    const bool gmOnly=focus->gmOnly, discovered=focus->discovered;
    // Furnish a place, rather than scattering unrelated stamps over the map.
    // The objects occupy real dry cover cells and leave the route untouched.
    const std::array<MapCellCoordinate,12> offsets{{{-3,-2},{3,2},{-2,3},{3,-2},{-4,0},{0,4},{4,1},{1,-4},{-2,0},{2,0},{0,-2},{0,2}}};
    int placed=0;
    const unsigned arrangement=recipe.seed%4u;
    for(const auto &baseOffset:offsets) {
        MapCellCoordinate offset=baseOffset;
        for(unsigned turn=0;turn<arrangement;++turn)offset={-offset.y,offset.x};
        const int x=cx+offset.x,y=cy+offset.y;
        auto *cell=mapCellAt(map,x,y);
        const bool clearing=recipe.purpose==TravelScenePurpose::Quiet || recipe.purpose==TravelScenePurpose::Social;
        if(!cell || x<1 || y<1 || x>=map.width-1 || y>=map.height-1 || cell->wayKind!=MapWayKind::None ||
           (cell->kind!=MapTileKind::Empty && cell->kind!=MapTileKind::Dirt && cell->kind!=MapTileKind::Stone && !(clearing && cell->kind==MapTileKind::Forest))) continue;
        if(std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){
            return f.locked && std::any_of(f.points.begin(),f.points.end(),[&](auto p){return std::abs(p.x-(x+.5f))<1.6f && std::abs(p.y-(y+.5f))<1.6f;});
        }))continue;
        if(std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){
            return !f.points.empty() && std::hypot(f.points.front().x-(x+.5f),f.points.front().y-(y+.5f))<1.2f;
        }))continue;
        if(clearing && cell->kind==MapTileKind::Forest)
            for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox) {
                auto *adjacent=mapCellAt(map,x+ox,y+oy);
                if(adjacent && adjacent->kind==MapTileKind::Forest && adjacent->wayKind==MapWayKind::None)
                    applyMapCellTerrainDefaults(*adjacent,MapTileKind::Dirt);
            }
        applyMapCellTerrainDefaults(*cell,MapTileKind::Cover);
        cell->blocksLineOfSight=false;
        MapGeneratedFeature prop;
        prop.id=map.metadata.id+"::travel_furnishing_"+std::to_string(placed);
        prop.kind=MapGeneratedFeatureKind::TacticalSetpiece;
        const bool stone=recipe.purpose==TravelScenePurpose::Discovery || recipe.purpose==TravelScenePurpose::Obstacle;
        const bool bedroll=recipe.purpose==TravelScenePurpose::Quiet && placed%2==0;
        prop.materialId=stone?"fantasy_neutral.travel.stone_seat":bedroll?"fantasy_neutral.travel.bedroll":"fantasy_neutral.travel.supply_crate";
        prop.name=usesGermanNarrative(recipe)?(stone?"Alter Stein":bedroll?"Schlafplatz":"Vorräte"):(stone?"Old stone":bedroll?"Bedroll":"Supplies");
        prop.points={{x+.5f,y+.5f}};prop.width=1.f;prop.gmOnly=gmOnly;prop.discovered=discovered;
        map.generation.graph.features.push_back(std::move(prop));
        if(++placed>=4)break;
    }
    if(recipe.purpose==TravelScenePurpose::Discovery) {
        // The discovery is a small surviving site, not an isolated symbol.
        for(int oy=-3;oy<=3;++oy)for(int ox=-3;ox<=3;++ox) {
            if(ox*ox+oy*oy>9)continue;
            auto *cell=mapCellAt(map,cx+ox,cy+oy);
            if(!cell || cell->wayKind!=MapWayKind::None ||
               (cell->kind!=MapTileKind::Empty && cell->kind!=MapTileKind::Dirt && cell->kind!=MapTileKind::Stone))continue;
            if(std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){
                return f.locked && std::any_of(f.points.begin(),f.points.end(),[&](auto p){return int(p.x)==cx+ox && int(p.y)==cy+oy;});
            }))continue;
            applyMapCellTerrainDefaults(*cell,MapTileKind::Stone);
        }
        MapGeneratedFeature site;site.id=map.metadata.id+"::travel_discovery_site";site.kind=MapGeneratedFeatureKind::TacticalSetpiece;
        site.materialId="fantasy_neutral.travel.discovery_site";site.name=usesGermanNarrative(recipe)?"Verwitterter Steinkreis":"Weathered stone circle";
        site.width=2.5f;site.points={{cx+.5f,cy+.5f}};site.gmOnly=gmOnly;site.discovered=discovered;
        map.generation.graph.features.push_back(std::move(site));
    }
    if(recipe.purpose==TravelScenePurpose::Social) {
        // A roadside gathering gets a usable canvas shelter, rather than a
        // lone cart floating in an empty field. Its entire footprint is cover.
        const std::array<MapCellCoordinate,8> shelterOffsets{{{-5,-4},{4,3},{-5,3},{4,-4},{-7,0},{6,0},{0,-6},{0,5}}};
        for(const auto &offset:shelterOffsets) {
            const int x=cx+offset.x,y=cy+offset.y;
            bool clear=true;
            for(int oy=0;oy<2;++oy)for(int ox=0;ox<2;++ox) {
                const auto *cell=mapCellAt(map,x+ox,y+oy);
                clear=clear && cell && cell->wayKind==MapWayKind::None &&
                    (cell->kind==MapTileKind::Empty || cell->kind==MapTileKind::Dirt || cell->kind==MapTileKind::Stone);
            }
            if(!clear || std::any_of(map.generation.graph.features.begin(),map.generation.graph.features.end(),[&](const auto &f){
                return std::any_of(f.points.begin(),f.points.end(),[&](auto p){return std::abs(p.x-(x+1.f))<2.f && std::abs(p.y-(y+1.f))<2.f;});
            }))continue;
            MapGeneratedFeature shelter;shelter.id=map.metadata.id+"::travel_shelter";
            shelter.kind=MapGeneratedFeatureKind::TacticalSetpiece;shelter.materialId="fantasy_neutral.travel.shelter";
            shelter.name=usesGermanNarrative(recipe)?"Reisendenunterstand":"Travelers shelter";
            shelter.width=2.f;shelter.gmOnly=gmOnly;shelter.discovered=discovered;
            for(int oy=0;oy<2;++oy)for(int ox=0;ox<2;++ox) {
                auto *cell=mapCellAt(map,x+ox,y+oy);applyMapCellTerrainDefaults(*cell,MapTileKind::Cover);cell->blocksLineOfSight=false;
                shelter.points.push_back({x+ox+.5f,y+oy+.5f});
            }
            map.generation.graph.features.push_back(std::move(shelter));break;
        }
    }
}
} // namespace TravelSceneGenerator::detail

