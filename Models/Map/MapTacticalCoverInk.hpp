#pragma once
#include "Models/Map/MapTacticalStructureInk.hpp"
namespace gm::map_render {
inline bool tacticalCoverInkAt(const MapModel &map,int x,int y) {
    if(map.atlasLevel!=MapLevel::Tactical || map.generation.recipe.generatorVersion<11 ||
       map.generation.recipe.creationIntent!=MapCreationIntent::TacticalEncounter) return false;
    const auto type=map.metadata.customProperties.find("tactical.template_kind");
    if(type!=map.metadata.customProperties.end() && (type->second=="6" || type->second=="7")) return false;
    const auto *cell=mapCellAt(map,x,y);
    return cell && (cell->kind==MapTileKind::Cover || cell->kind==MapTileKind::Rubble || cell->kind==MapTileKind::Hazard) &&
           !tacticalWaystoneAt(map,x,y) && !tacticalShrineAt(map,x,y);
}
// Resolved-cell cover art replaces the bright-edged miniature rubble stamps.
// Every fragment stays inside the actual cover cell; audience/fog filtering
// happens before this helper receives the shared render-plan model.
inline std::vector<AtlasInkPrimitive> buildTacticalCoverInk(const MapModel &map,bool applyFog) {
    std::vector<AtlasInkPrimitive> ink;
    const auto stroke=[&](std::vector<MapFeaturePoint> points,OverviewRenderColor color,float width) {
        ink.push_back({std::move(points),color,width,false});
    };
    const auto polygon=[&](const std::vector<MapFeaturePoint> &p,OverviewRenderColor color) {
        for(std::size_t i=1;i+1<p.size();++i) ink.push_back({{p[0],p[i],p[i+1]},color,.02f,true});
    };
    for(int y=0;y<map.height;++y) for(int x=0;x<map.width;++x) {
        if(!tacticalCoverInkAt(map,x,y)) continue;
        const auto *cell=mapCellAt(map,x,y);if(applyFog && cell->fogged) continue;
        const unsigned h=map.generation.recipe.seed+unsigned(x)*73856093u+unsigned(y)*19349663u;
        const float cx=x+.5f,cy=y+.5f;
        if(cell->kind==MapTileKind::Hazard) {
            std::vector<MapFeaturePoint> rim;
            for(int i=0;i<8;++i) {
                const float a=(i+.5f)*6.2831853f/8.f;
                rim.push_back({cx+std::cos(a)*.43f,cy+std::sin(a)*.37f});
            }
            polygon(rim,{83,91,80,255});rim.push_back(rim.front());
            stroke(rim,{40,49,43,255},.04f);
            stroke({{cx-.26f,cy-.16f},{cx+.24f,cy-.16f},{cx+.27f,cy+.13f}},{176,155,105,255},.045f);
            stroke({{cx-.20f,cy+.20f},{cx,cy-.18f},{cx+.20f,cy+.20f},{cx-.20f,cy+.20f}},{222,164,82,255},.045f);
            stroke({{cx,cy-.06f},{cx,cy+.07f}},{245,204,129,255},.04f);
            stroke({{cx+.11f,cy+.23f},{cx+.26f,cy+.06f},{cx+.38f,cy+.12f}},{39,45,40,255},.04f);
            continue;
        }
        const bool rubble=cell->kind==MapTileKind::Rubble;
        if(!rubble && h%3u==0u) {
            // Broken low masonry, with a readable top and shaded near face.
            polygon({{cx-.43f,cy-.22f},{cx+.37f,cy-.22f},{cx+.44f,cy+.10f},{cx-.39f,cy+.15f}},{139,134,106,255});
            polygon({{cx-.39f,cy+.15f},{cx+.44f,cy+.10f},{cx+.40f,cy+.30f},{cx-.37f,cy+.32f}},{76,83,72,255});
            stroke({{cx-.43f,cy-.22f},{cx+.37f,cy-.22f},{cx+.44f,cy+.10f},{cx+.40f,cy+.30f},{cx-.37f,cy+.32f},{cx-.43f,cy-.22f}},{47,53,46,255},.035f);
            stroke({{cx-.13f,cy-.21f},{cx-.09f,cy+.12f},{cx-.10f,cy+.30f}},{62,66,56,255},.025f);
            stroke({{cx+.19f,cy-.21f},{cx+.23f,cy+.12f}},{62,66,56,255},.025f);
            stroke({{cx-.39f,cy-.18f},{cx+.32f,cy-.18f}},{194,181,139,245},.035f);
        } else {
            const int count=rubble?3:2;
            for(int i=0;i<count;++i) {
                const float ox=i==0?-.18f:(i==1?.20f:.10f),oy=i==0?.04f:(i==1?-.14f:.26f);
                const float r=i==0?.26f:(i==1?.22f:.13f);
                std::vector<MapFeaturePoint> edge;
                for(int k=0;k<6;++k) {
                    const float a=(k+.12f)*6.2831853f/6.f;
                    const float rr=r*(.87f+float((h>>(k*3))%3u)*.065f);
                    edge.push_back({cx+ox+std::cos(a)*rr,cy+oy+std::sin(a)*rr});
                }
                polygon(edge,{109,118,99,255});
                polygon({edge[2],edge[3],edge[4],{cx+ox,cy+oy}},{162,162,128,255});
                polygon({edge[0],edge[1],{cx+ox,cy+oy},edge[5]},{73,87,76,255});
                edge.push_back(edge.front());stroke(edge,{45,58,49,255},.028f);
                stroke({{cx+ox-r*.45f,cy+oy-r*.40f},{cx+ox+r*.18f,cy+oy-r*.47f}},{192,183,141,230},.025f);
            }
        }
    }
    return ink;
}
}
