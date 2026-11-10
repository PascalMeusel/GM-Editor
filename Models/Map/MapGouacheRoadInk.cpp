#include "Models/Map/MapGouacheAtlasInk.hpp"
#include "Models/Map/MapIllustratedAtlasRiverGeometry.hpp"
#include <array>

namespace gm::map_render
{
std::vector<AtlasInkPrimitive> buildGouacheAtlasRoadInk(
    const MapModel &map, const MapGeneratedFeature &feature)
{
    std::vector<AtlasInkPrimitive> ink;
    if (!usesGouacheAtlasArtwork(map) || feature.points.size()<2 ||
        (feature.kind!=MapGeneratedFeatureKind::Road && feature.kind!=MapGeneratedFeatureKind::Trail)) return ink;
    const auto controls=overviewFinalRoadCurvePoints(map,feature);
    if (controls.size()<2) return ink;
    float total=0.f;
    for (std::size_t i=1;i<controls.size();++i)
        total+=std::hypot(controls[i].x-controls[i-1].x,controls[i].y-controls[i-1].y);
    if (!std::isfinite(total) || total<.00001f) return ink;
    const bool trail=feature.kind==MapGeneratedFeatureKind::Trail;
    const float width=illustratedAtlasV2RoadWidth(feature,32.f)/32.f;
    // At very small scale the material collapses to a readable ochre stroke.
    // At play scale the mesh has feathered shoulders and no dark tube outline.
    ink.push_back({controls,{170,139,88,235},width,false,0.f,7.f});
    const float spacing=std::max(.26f,total/1024.f);
    std::vector<MapFeaturePoint> points{controls.front()};
    float next=spacing;
    for (std::size_t i=1;i<controls.size();++i) {
        const auto a=controls[i-1],b=controls[i];
        const float distance=std::hypot(b.x-a.x,b.y-a.y);
        if (distance<.000001f) continue;
        while (next<distance && points.size()<1026u) {
            const float t=next/distance;
            points.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}); next+=spacing;
        }
        next-=distance;
    }
    if (std::hypot(points.back().x-controls.back().x,points.back().y-controls.back().y)>.000001f)
        points.push_back(controls.back());
    struct Section { std::array<MapFeaturePoint,5> p; std::array<OverviewRenderColor,5> c; };
    std::vector<Section> sections;
    sections.reserve(points.size());
    constexpr std::array<float,5> across{{-1.f,-.76f,0.f,.76f,1.f}};
    const auto color=[&](MapFeaturePoint p,float cross) {
        const float pigment=illustrationPigmentNoise(map.generation.recipe.seed^0x5213u,p.x*2.3f,p.y*2.3f)*7.f;
        const float shoulder=std::abs(cross);
        const auto channel=[&](float value){return std::uint8_t(std::clamp(std::lround(value+pigment),0l,255l));};
        return OverviewRenderColor{channel(trail?185:187),channel(trail?158:150),channel(trail?108:94),
            std::uint8_t(shoulder>.99f?0:255)};
    };
    const auto triangle=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,
                            OverviewRenderColor ac,OverviewRenderColor bc,OverviewRenderColor cc) {
        AtlasInkPrimitive item{{a,b,c},ac,0.f,true,7.f}; item.vertexColors={ac,bc,cc};
        ink.push_back(std::move(item));
    };
    for (std::size_t i=0;i<points.size();++i) {
        const auto before=points[i?i-1:i],after=points[std::min(i+1,points.size()-1)];
        float dx=after.x-before.x,dy=after.y-before.y;
        const float length=std::hypot(dx,dy);
        if (length<.00001f) { dx=1.f;dy=0.f; } else { dx/=length;dy/=length; }
        const auto center=points[i];
        const float irregular=illustrationPigmentNoise(map.generation.recipe.seed^0x738fu,center.x*3.2f,center.y*3.2f);
        const float half=width*.5f*(1.f+irregular*.09f)+.020f;
        Section section;
        for (std::size_t j=0;j<across.size();++j) {
            section.p[j]={center.x-dy*half*across[j],center.y+dx*half*across[j]};
            section.c[j]=color(center,across[j]);
        }
        sections.push_back(section);
    }
    for (std::size_t i=1;i<sections.size();++i)
        for (std::size_t j=1;j<across.size();++j) {
            const auto &a=sections[i-1],&b=sections[i];
            triangle(a.p[j-1],a.p[j],b.p[j],a.c[j-1],a.c[j],b.c[j]);
            triangle(a.p[j-1],b.p[j],b.p[j-1],a.c[j-1],b.c[j],b.c[j-1]);
        }
    // Continuous opaque centres overlap cleanly at semantic joins. No inset or
    // separate flat bridge rectangle is needed to hide dark end caps.
    if (std::hypot(points.front().x-points.back().x,points.front().y-points.back().y)>.001f)
        for (const std::size_t end:{std::size_t{0},points.size()-1}) {
            const auto c=points[end],neighbor=points[end?end-1:1];
            const float radius=std::hypot(sections[end].p[4].x-c.x,sections[end].p[4].y-c.y);
            const float direction=std::atan2(c.y-neighbor.y,c.x-neighbor.x);
            for (int i=0;i<10;++i) {
                const float a=direction-1.5707963f+i*3.1415926f/10.f,b=a+3.1415926f/10.f;
                const auto point=[&](float angle,float scale){return MapFeaturePoint{c.x+std::cos(angle)*radius*scale,c.y+std::sin(angle)*radius*scale};};
                const auto pa=point(a,.76f),pb=point(b,.76f),qa=point(a,1.f),qb=point(b,1.f);
                triangle(c,pa,pb,color(c,0),color(c,.76f),color(c,.76f));
                triangle(pa,qa,qb,color(c,.76f),color(c,1),color(c,1));
                triangle(pa,qb,pb,color(c,.76f),color(c,1),color(c,.76f));
            }
        }
    // Sparse worn tracks follow the existing road rather than adding parallel
    // decorative routes. Fine marks disappear at overview scale.
    if (!trail) for (std::size_t i=2;i+2<points.size();i+=4) {
        const auto &a=sections[i],&b=sections[i+1];
        const float shift=(i%3==0?-.30f:.28f);
        const auto track=[&](const Section &s){return MapFeaturePoint{
            s.p[2].x+(s.p[4].x-s.p[2].x)*shift,s.p[2].y+(s.p[4].y-s.p[2].y)*shift};};
        ink.push_back({{track(a),track(b)},{131,112,76,95},.014f,false,25.f});
    }
    return ink;
}
}
