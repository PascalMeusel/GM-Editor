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
void composeTravelRoad(MapModel &map, const TravelSceneRecipe &recipe, std::uint32_t seed)
{
    if(recipe.schemaVersion<3)return;
    // Dry routes preserve their authored exits and junctions. Replace long
    // raster-axis runs with a gently varying, fully projected walking path.
    for(auto &c:map.cells) if(mapGenWayKindIsRoadLike(c.wayKind) && c.kind!=MapTileKind::Bridge) {
        c.wayKind=MapWayKind::None;
        if(c.kind==MapTileKind::Road)applyMapCellTerrainDefaults(c,MapTileKind::Dirt);
    }
    unsigned road=0;
    for(auto &f:map.generation.graph.features) {
        if((f.kind!=MapGeneratedFeatureKind::Road && f.kind!=MapGeneratedFeatureKind::Trail) || f.points.size()<2)continue;
        const auto a=f.points.front(),z=f.points.back();const float dx=z.x-a.x,dy=z.y-a.y,length=std::hypot(dx,dy);
        if(length<1.f)continue;
        const float nx=-dy/length,ny=dx/length;
        const float swing=std::min(3.5f,length*.09f)*((seed+(++road)*17)%2?1.f:-1.f);
        const int steps=std::max(2,static_cast<int>(std::ceil(length*3.f)));
        std::vector<MapFeaturePoint> points;std::vector<MapCellCoordinate> cells;
        for(int i=0;i<=steps;++i) {
            const float q=static_cast<float>(i)/steps;
            const float frequency=1.2f+static_cast<float>((seed/13u+road)%7u)*.16f;
            const float phase=static_cast<float>((seed/41u)%11u)*.23f;
            const float wave=swing*std::sin(q*3.14159265f)*std::sin(q*frequency*3.14159265f+phase);
            MapFeaturePoint p{std::clamp(a.x+dx*q+nx*wave,.5f,map.width-.5f),std::clamp(a.y+dy*q+ny*wave,.5f,map.height-.5f)};
            if(i==0)p=a;if(i==steps)p=z;
            points.push_back(p);MapCellCoordinate c{static_cast<int>(p.x),static_cast<int>(p.y)};
            if(cells.empty() || !(cells.back()==c))cells.push_back(c);
        }
        const bool safe=std::none_of(cells.begin(),cells.end(),[&](const auto &p){
            const auto *c=mapCellAt(map,p.x,p.y);
            return !c || mapGenRiverInfluenceCell(c) || c->kind==MapTileKind::Bridge || c->kind==MapTileKind::Building || c->kind==MapTileKind::Wall;
        });
        if(safe)f.points=std::move(points);
        else {
            // A curved alternative may not cut a building or invent a water
            // crossing. Retain the existing validated route in those cases.
            // Keep crossings and building approaches fixed, but soften safe
            // long spans independently instead of making the whole route rigid.
            // Raster-derived graphs often contain one point per cell; coalesce
            // collinear runs before deciding whether a span is long enough.
            std::vector<MapFeaturePoint> controls;
            for(const auto &p:f.points){
                while(controls.size()>1){
                    const auto a=controls[controls.size()-2],z=controls.back();
                    const float ux=z.x-a.x,uy=z.y-a.y,vx=p.x-z.x,vy=p.y-z.y;
                    if(std::abs(ux*vy-uy*vx)>.0001f || ux*vx+uy*vy<=0.f)break;
                    controls.pop_back();
                }
                controls.push_back(p);
            }
            if(recipe.schemaVersion>=4 && controls.size()>2) {
                std::vector<MapFeaturePoint> rounded{controls.front()};
                for(std::size_t k=1;k+1<controls.size();++k) {
                    const auto a=controls[k-1],b=controls[k],z=controls[k+1];
                    const float before=std::hypot(a.x-b.x,a.y-b.y),after=std::hypot(z.x-b.x,z.y-b.y);
                    const float radius=std::min(1.4f,std::min(before,after)*.24f);
                    if(radius<.12f){rounded.push_back(b);continue;}
                    const MapFeaturePoint entry{b.x+(a.x-b.x)*radius/before,b.y+(a.y-b.y)*radius/before};
                    const MapFeaturePoint exit{b.x+(z.x-b.x)*radius/after,b.y+(z.y-b.y)*radius/after};
                    std::vector<MapFeaturePoint> arc;bool clear=true;
                    for(int j=0;j<=10;++j) {
                        const float t=j/10.f,u=1.f-t;
                        MapFeaturePoint p{u*u*entry.x+2*u*t*b.x+t*t*exit.x,u*u*entry.y+2*u*t*b.y+t*t*exit.y};
                        const auto *c=mapCellAt(map,static_cast<int>(p.x),static_cast<int>(p.y));
                        clear=clear && c && !mapGenRiverInfluenceCell(c) && c->kind!=MapTileKind::Bridge &&
                              c->kind!=MapTileKind::Building && c->kind!=MapTileKind::Wall && c->kind!=MapTileKind::Mountain;
                        arc.push_back(p);
                    }
                    if(clear)rounded.insert(rounded.end(),arc.begin(),arc.end());else rounded.push_back(b);
                }
                rounded.push_back(controls.back());controls=std::move(rounded);
            }
            f.points=std::move(controls);
            std::vector<MapFeaturePoint> softened{f.points.front()};
            for(std::size_t i=1;i<f.points.size();++i) {
                const auto a=f.points[i-1],z=f.points[i];
                const float dx=z.x-a.x,dy=z.y-a.y,length=std::hypot(dx,dy);
                if(length<4.f){softened.push_back(z);continue;}
                std::vector<MapFeaturePoint> span;
                bool clear=true; const int samples=static_cast<int>(std::ceil(length*4.f));
                const float bend=std::min(1.15f,length*.065f)*((seed+i)%2?1.f:-1.f);
                for(int j=1;j<=samples;++j){
                    const float q=static_cast<float>(j)/samples;
                    const float wave=bend*std::sin(q*3.14159265f)*std::sin(q*3.14159265f);
                    MapFeaturePoint p{a.x+dx*q-dy/length*wave,a.y+dy*q+dx/length*wave};
                    if(j==samples)p=z;
                    const auto *c=mapCellAt(map,static_cast<int>(p.x),static_cast<int>(p.y));
                    clear=clear && c && !mapGenRiverInfluenceCell(c) && c->kind!=MapTileKind::Bridge && c->kind!=MapTileKind::Building && c->kind!=MapTileKind::Wall;
                    span.push_back(p);
                }
                if(clear)softened.insert(softened.end(),span.begin(),span.end());else softened.push_back(z);
            }
            f.points=std::move(softened);
            cells.clear();
            for(std::size_t i=1;i<f.points.size();++i) {
                const auto a=f.points[i-1],z=f.points[i];const int count=std::max(1,static_cast<int>(std::ceil(std::hypot(z.x-a.x,z.y-a.y)*4.f)));
                for(int j=0;j<=count;++j){const float q=static_cast<float>(j)/count;MapCellCoordinate p{static_cast<int>(a.x+(z.x-a.x)*q),static_cast<int>(a.y+(z.y-a.y)*q)};if(cells.empty() || !(cells.back()==p))cells.push_back(p);}
            }
        }
        for(const auto &p:cells)for(int oy=-1;oy<=1;++oy)for(int ox=-1;ox<=1;++ox) {
            auto *c=mapCellAt(map,p.x+ox,p.y+oy);
            if(c && c->kind==MapTileKind::Forest) applyMapCellTerrainDefaults(*c,ox==0 && oy==0?MapTileKind::Dirt:MapTileKind::Empty);
        }
        mapGenV2RasterizeRoad(map,cells,MapWayKind::DirtRoad);
    }
}
} // namespace TravelSceneGenerator::detail

