#include "Models/Map/MapIllustratedAtlas.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace gm::map_render
{
std::vector<AtlasInkPrimitive> buildAtlasMountainButtressInk(const MapModel &map,
    const std::vector<gm::map_geometry::MapRoadCorridor> &corridors,
    const std::vector<AtlasMountainFootprint> &mountains)
{
    std::vector<AtlasInkPrimitive> ink;
    const auto ground=buildOverviewGroundProjection(map);
    const auto land=[&](MapFeaturePoint p) {
        return p.x>=0 && p.y>=0 && p.x<map.width && p.y<map.height &&
            overviewGroundKindIsLand(overviewGroundKindFromProjection(map,ground,int(p.x),int(p.y)));
    };
    std::vector<MapFeaturePoint> occupied;
    auto ordered=mountains;
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto &a,const auto &b){return a.main && !b.main;});
    for(const auto &mountain:ordered) {
        MapFeaturePoint root{mountain.base.x,mountain.base.y-mountain.height*.20f};
        float best=std::numeric_limits<float>::max();
        const gm::map_geometry::MapRoadCorridor *route=nullptr;
        MapFeaturePoint q{},tangent{};
        for(const auto &candidate:corridors)for(std::size_t i=1;i<candidate.centerline.size();++i) {
            const auto a=candidate.centerline[i-1],b=candidate.centerline[i];
            const float dx=b.x-a.x,dy=b.y-a.y,n2=dx*dx+dy*dy;if(n2<.0001f)continue;
            const float t=std::clamp(((root.x-a.x)*dx+(root.y-a.y)*dy)/n2,0.f,1.f);
            const MapFeaturePoint p{a.x+dx*t,a.y+dy*t};
            const float d=std::hypot(root.x-p.x,root.y-p.y);
            if(d<best) {best=d;route=&candidate;q=p;const float n=std::sqrt(n2);tangent={dx/n,dy/n};}
        }
        if(!route || best<.6f || best>(mountain.main?3.8f:2.4f))continue;
        // Attach at the exposed lower silhouette, not at the centre of a
        // mountain and not at its snow-capped summit. This limits long tails.
        auto edgeRoot=root;float edgeDistance=best;
        for(const auto p:mountain.outline) {
            if(p.y<mountain.base.y-mountain.height*.68f)continue;
            const float distance=std::hypot(p.x-q.x,p.y-q.y);
            if(distance<edgeDistance){edgeDistance=distance;edgeRoot=p;}
        }
        const MapFeaturePoint attachment{root.x*.32f+edgeRoot.x*.68f,root.y*.32f+edgeRoot.y*.68f};
        if(atlasPointInsidePolygon(mountain.outline,attachment))root=attachment;
        if(std::hypot(root.x-q.x,root.y-q.y)>3.1f)continue;
        const auto feature=std::find_if(map.generation.graph.features.begin(),map.generation.graph.features.end(),
            [&](const auto &f){return f.id==route->featureId;});
        if(feature==map.generation.graph.features.end() ||
           (feature->kind!=MapGeneratedFeatureKind::Road && feature->kind!=MapGeneratedFeatureKind::River))continue;
        const bool river=feature->kind==MapGeneratedFeatureKind::River;
        const float edge=river?std::max(.20f,feature->width*.12f+.10f):std::max(.19f,feature->width*.15f+.10f);
        auto clearance=corridors;for(auto &c:clearance)if(c.featureId==route->featureId)c.radiusCells=edge;
        const float side=((root.x-q.x)*-tangent.y+(root.y-q.y)*tangent.x)<0?-1.f:1.f;
        const MapFeaturePoint normal{-tangent.y*side,tangent.x*side};
        float half=std::min(.65f,mountain.width*.65f);
        const auto atRoot=[&](float offset){return MapFeaturePoint{root.x+tangent.x*offset,root.y+tangent.y*offset};};
        while(half>.12f && (!atlasPointInsidePolygon(mountain.outline,atRoot(-half)) ||
                           !atlasPointInsidePolygon(mountain.outline,atRoot(half))))half*=.8f;
        if(half<=.12f) {
            // A near-edge root may be too shallow for a vertical attachment.
            // Fall back inside the same visible mountain instead of dropping
            // every crossing in that orientation.
            root={mountain.base.x,mountain.base.y-mountain.height*.20f};
            half=std::min(.65f,mountain.width*.65f);
            while(half>.12f && (!atlasPointInsidePolygon(mountain.outline,atRoot(-half)) ||
                               !atlasPointInsidePolygon(mountain.outline,atRoot(half))))half*=.8f;
            if(half<=.12f)continue;
        }
        if(std::any_of(occupied.begin(),occupied.end(),[&](auto p){return std::hypot(p.x-q.x,p.y-q.y)<1.15f;}))continue;
        const MapFeaturePoint toe{q.x+normal.x*(edge+.12f),q.y+normal.y*(edge+.12f)};
        const auto valid=[&](const std::vector<MapFeaturePoint> &polygon) {
            if(!std::all_of(polygon.begin(),polygon.end(),land) || !atlasSilhouetteClearsRoads(polygon,clearance,.03f))return false;
            for(std::size_t i=0;i<polygon.size();++i) {
                const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
                const int steps=std::max(1,int(std::ceil(std::hypot(b.x-a.x,b.y-a.y)*4.f)));
                for(int j=1;j<steps;++j)if(!land({a.x+(b.x-a.x)*j/steps,a.y+(b.y-a.y)*j/steps}))return false;
            }
            for(const auto &f:map.generation.graph.features) {
                if((f.kind!=MapGeneratedFeatureKind::Settlement && f.kind!=MapGeneratedFeatureKind::Landmark) || f.points.empty())continue;
                for(auto p:polygon)if(std::hypot(p.x-f.points[0].x,p.y-f.points[0].y)<.65f)return false;
                if(atlasPointInsidePolygon(polygon,f.points[0]))return false;
            }
            return true;
        };
        const auto before=ink.size();
        // Broken descending contours connect the visible mountain foundation
        // to its lower outcrops without filling the whole gap with a ribbon.
        for(int sideLine:{-1,1}) {
            std::vector<MapFeaturePoint> contour;
            for(float t:sideLine<0?std::array<float,4>{0.f,.19f,.37f,.58f}:std::array<float,4>{.29f,.47f,.66f,.87f}) {
                const float offset=sideLine*(.13f+.14f*(1.f-t))+std::sin(t*7.f)*.065f;
                contour.push_back({root.x+(toe.x-root.x)*t+tangent.x*offset,root.y+(toe.y-root.y)*t+tangent.y*offset});
            }
            if(valid(contour))ink.push_back({contour,river?OverviewRenderColor{113,132,121,155}:OverviewRenderColor{137,139,103,140},.022f,false});
        }
        const int count=river?3:2;
        // Low upright outcrops keep the atlas light/projection. Extending a
        // shaded face in route coordinates creates sideways peaks or ribbons.
        const std::array<MapFeaturePoint,9> ledge{{{-1,0},{-.89f,-.45f},{-.61f,-.49f},
            {-.51f,-.80f},{-.06f,-.74f},{.22f,-.88f},{.31f,-.52f},{.76f,-.40f},{1,0}}};
        const std::array<MapFeaturePoint,9> scree{{{-1,0},{-.85f,-.22f},{-.57f,-.28f},
            {-.37f,-.38f},{-.07f,-.32f},{.17f,-.45f},{.48f,-.38f},{.73f,-.12f},{1,0}}};
        for(int j=0;j<=count;++j) {
            const unsigned h=mountain.variant*1664525u+unsigned(j)*1013904223u;
            const float t=j==0?0.f:j==count?1.f:std::clamp(float(j)/count+(int(h%17)-8)*.012f,.12f,.86f);
            const float scatter=std::sin(t*3.14159265f)*(int((h>>8)%19)-9)*.032f;
            MapFeaturePoint c{root.x+(toe.x-root.x)*t+tangent.x*scatter,root.y+(toe.y-root.y)*t+tangent.y*scatter};
            const float variation=.80f+((h>>12)%11)*.04f;
            const float width=(j==0?std::min(.70f,mountain.width*.8f):river?.44f:.22f)*variation;
            const float height=(j==0?.36f:river?.31f:.20f)*variation;
            const auto &rock=river?ledge:scree;
            std::vector<MapFeaturePoint> shape;
            for(auto p:rock)shape.push_back({c.x+p.x*width,c.y+p.y*height});
            // Fit the complete outcrop outside the open channel; do not clip
            // its top into a horizontal plateau. Mountain-facing feet overlap.
            float nearest=100.f;
            for(auto p:shape)nearest=std::min(nearest,(p.x-q.x)*normal.x+(p.y-q.y)*normal.y);
            const float shift=std::max(0.f,edge+.055f-nearest);
            for(auto &p:shape){p.x+=normal.x*shift;p.y+=normal.y*shift;}
            c.x+=normal.x*shift;c.y+=normal.y*shift;
            if(!valid(shape))continue;
            const MapFeaturePoint fold{c.x-width*.08f,c.y-height*.08f};
            const auto shade=river?OverviewRenderColor{142,158,157,255}:OverviewRenderColor{177,176,148,255};
            const auto light=river?OverviewRenderColor{209,211,186,255}:OverviewRenderColor{229,218,181,255};
            for(std::size_t k=1;k<shape.size();++k)
                ink.push_back({{shape[k-1],shape[k],fold},k<5?shade:light,0,true});
            ink.push_back({std::vector<MapFeaturePoint>(shape.begin(),shape.end()),{116,124,92,195},.026f,false});
            AtlasInkPrimitive crease{{shape[3],fold},{107,119,88,155},.022f,false};
            crease.minimumCellPixels=18.f;ink.push_back(std::move(crease));
            if(river) {
                AtlasInkPrimitive strata{{shape[1],shape[2],fold},{101,127,125,145},.022f,false};
                strata.minimumCellPixels=18.f;ink.push_back(std::move(strata));
                if(j==count) {
                    AtlasInkPrimitive edgeLight{{shape[2],shape[3],shape[4]},{214,223,198,190},.025f,false};
                    edgeLight.minimumCellPixels=16.f;ink.push_back(std::move(edgeLight));
                }
            } else if(j>0) {
                const std::vector<MapFeaturePoint> chip{{c.x+width*.6f,c.y+.05f},{c.x+width*.9f,c.y+.03f},{c.x+width*.85f,c.y-.015f}};
                if(valid(chip)) {
                    AtlasInkPrimitive gravel{chip,{165,157,126,190},0,true};gravel.minimumCellPixels=18.f;ink.push_back(std::move(gravel));
                }
            }
        }
        if(ink.size()>before) {
            occupied.push_back(q);
            if(!river) {
                const float shoulder=edge+.055f;
                std::vector<MapFeaturePoint> verge{{q.x-tangent.x*.32f+normal.x*shoulder,q.y-tangent.y*.32f+normal.y*shoulder},
                    {q.x+normal.x*(shoulder+.04f),q.y+normal.y*(shoulder+.04f)},
                    {q.x+tangent.x*.24f+normal.x*shoulder,q.y+tangent.y*.24f+normal.y*shoulder}};
                if(valid(verge))ink.push_back({verge,{190,169,121,170},.026f,false});
            }
        }
    }
    return ink;
}
}
