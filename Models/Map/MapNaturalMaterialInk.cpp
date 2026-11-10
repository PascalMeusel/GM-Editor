#include "Models/Map/MapNaturalMaterialInk.hpp"
#include "Models/Map/MapLocalIllustration.hpp"
#include "Models/Map/MapTacticalStructureInk.hpp"

namespace gm::map_render
{
void appendSanctuaryPavingInk(std::vector<AtlasInkPrimitive> &ink,const MapModel &map)
{
    for(const auto &feature:map.generation.graph.features)
    {
        if(feature.materialId!="fantasy_neutral.tactical.sanctuary" || feature.points.empty())continue;
        const auto center=feature.points.front();
        const auto onStone=[&](MapFeaturePoint p) {
            const auto *cell=mapCellAt(map,static_cast<int>(std::floor(p.x)),static_cast<int>(std::floor(p.y)));
            if(!cell || cell->wayKind!=MapWayKind::None)return false;
            const int x=static_cast<int>(std::floor(p.x)),y=static_cast<int>(std::floor(p.y));
            // The original sanctuary floor continues beneath its setpiece
            // props. They are painted afterwards, over the paving. Removed
            // stone and unrelated cover never acquire a false floor.
            return cell->kind==MapTileKind::Stone || tacticalWaystoneAt(map,x,y) || tacticalShrineAt(map,x,y);
        };
        const int rings=std::clamp(static_cast<int>(std::ceil(feature.width/.85f)),2,12);
        const float band=feature.width*.96f/rings;
        for(int ring=0;ring<rings;++ring)
        {
            const float inner=ring*band+.027f,outer=(ring+1)*band-.027f;
            const int stones=std::max(6,static_cast<int>((inner+outer)*3.1415926f/.85f));
            for(int stone=0;stone<stones;++stone)
            {
                const unsigned h=localIllustrationHash(map.generation.recipe.seed,ring,stone,0xa563u);
                const float step=6.2831853f/stones;
                const float start=(stone+(ring%2)*.45f)*step+.014f/std::max(.2f,outer);
                const float finish=start+step-.032f/std::max(.2f,outer);
                const auto point=[&](float angle,float radius) {
                    return MapFeaturePoint{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};
                };
                const auto base=static_cast<std::uint8_t>(183u+h%23u);
                const OverviewRenderColor body{base,static_cast<std::uint8_t>(base-5),static_cast<std::uint8_t>(base-26),255};
                for(int slice=0;slice<4;++slice)
                {
                    const float a=start+(finish-start)*slice/4.f,b=start+(finish-start)*(slice+1)/4.f;
                    const auto ia=point(a,inner),oa=point(a,outer),ob=point(b,outer),ib=point(b,inner);
                    // Reject pieces at edited roads, removed floor, props or
                    // fog-filtered cells. Every radial/circumferential sample
                    // must belong to real stone ground before painting it.
                    bool allowed=true;
                    for(int angular=0;angular<=4 && allowed;++angular)
                        for(int radial=0;radial<=6;++radial)
                            allowed=allowed && onStone(point(a+(b-a)*angular/4.f,inner+(outer-inner)*radial/6.f));
                    if(!allowed)continue;
                    const OverviewRenderColor light{static_cast<std::uint8_t>(base+10),static_cast<std::uint8_t>(base+4),static_cast<std::uint8_t>(base-13),255};
                    AtlasInkPrimitive first{{ia,oa,ob},body,0.f,true};
                    first.vertexColors={body,light,light};ink.push_back(std::move(first));
                    AtlasInkPrimitive second{{ia,ob,ib},body,0.f,true};
                    second.vertexColors={body,light,body};ink.push_back(std::move(second));
                    ink.push_back({{oa,ob},{123,126,103,145},.026f,false,8.f});
                    if(slice==0)ink.push_back({{ia,oa},{126,130,105,165},.026f,false,8.f});
                    if(slice==3)ink.push_back({{ib,ob},{230,220,185,170},.021f,false,10.f});
                    if(slice==1 && h%4u==0)
                    {
                        const auto p=point(a+(b-a)*.45f,inner+(outer-inner)*.33f);
                        const auto q=point(a+(b-a)*.61f,inner+(outer-inner)*.56f);
                        const auto r=point(a+(b-a)*.42f,inner+(outer-inner)*.82f);
                        ink.push_back({{p,q,r},{132,136,113,130},.020f,false,14.f});
                    }
                }
            }
        }
    }
}

static void appendNaturalRoadSurfaceInk(std::vector<AtlasInkPrimitive> &ink, const MapModel &map,
                                const std::vector<MapFeaturePoint> &controls, float width,bool stoneSurface)
{
    if(controls.size()<2) return;
    float total=0.f;
    for(std::size_t i=1;i<controls.size();++i)
        total+=std::hypot(controls[i].x-controls[i-1].x,controls[i].y-controls[i-1].y);
    if(!std::isfinite(total) || total<.0001f) return;
    // Continuous arc-length sampling avoids changing the texture when an
    // editor inserts a redundant point. Cap work for very long authored ways.
    const float spacing=std::max(.20f,total/2048.f);
    std::vector<MapFeaturePoint> points{controls.front()};
    float next=spacing;
    for(std::size_t i=1;i<controls.size();++i)
    {
        const auto a=controls[i-1],b=controls[i];
        const float length=std::hypot(b.x-a.x,b.y-a.y);
        if(length<.00001f) continue;
        while(next<length && points.size()<2050u)
        {
            points.push_back({a.x+(b.x-a.x)*next/length,a.y+(b.y-a.y)*next/length});
            next+=spacing;
        }
        next-=length;
    }
    if(std::hypot(points.back().x-controls.back().x,points.back().y-controls.back().y)>.00001f)
        points.push_back(controls.back());
    if(points.size()<2) return;
    struct Section { std::array<MapFeaturePoint,7> points; std::array<OverviewRenderColor,7> colors; };
    const auto vertexColor=[&](float cross,MapFeaturePoint p) {
        const float wash=illustrationPigmentNoise(map.generation.recipe.seed^0xb671u,p.x*1.1f,p.y*1.1f)*5.f;
        const float edge=std::abs(cross);
        const auto channel=[&](float center,float rim) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(center+(rim-center)*edge+wash),0l,255l));
        };
        // Equal-value soil across the section avoids the raised metallic
        // tube produced by a bright centre and dark symmetrical shoulders.
        return OverviewRenderColor{channel(stoneSurface?182:202,stoneSurface?190:199),
            channel(stoneSurface?180:179,stoneSurface?186:176),channel(stoneSurface?156:132,stoneSurface?163:131),
            static_cast<std::uint8_t>(edge>.97f?0:255)};
    };
    const auto triangle=[&](MapFeaturePoint a,MapFeaturePoint b,MapFeaturePoint c,
                            OverviewRenderColor ac,OverviewRenderColor bc,OverviewRenderColor cc) {
        AtlasInkPrimitive piece{{a,b,c},ac,0.f,true};piece.vertexColors={ac,bc,cc};
        ink.push_back(std::move(piece));
    };
    constexpr std::array<float,7> across{{-1.f,-.84f,-.51f,0.f,.51f,.84f,1.f}};
    std::vector<Section> sections;
    sections.reserve(points.size());
    for(std::size_t i=0;i<points.size();++i)
    {
        const auto previous=points[i?i-1:i],after=points[std::min(i+1,points.size()-1)];
        float dx=after.x-previous.x,dy=after.y-previous.y;
        const float length=std::hypot(dx,dy);
        if(length<.00001f) {dx=1.f;dy=0.f;} else {dx/=length;dy/=length;}
        const auto center=points[i];
        const float half=width*.5f*(1.f+.075f*illustrationPigmentNoise(
            map.generation.recipe.seed^0x7ba91u,center.x*1.9f,center.y*1.9f))+.035f;
        Section section;
        for(std::size_t j=0;j<across.size();++j)
        {
            section.points[j]={center.x-dy*half*across[j],center.y+dx*half*across[j]};
            section.colors[j]=vertexColor(across[j],center);
        }
        sections.push_back(section);
    }
    for(std::size_t i=1;i<sections.size();++i)
        for(std::size_t j=1;j<across.size();++j)
        {
            const auto &a=sections[i-1],&b=sections[i];
            triangle(a.points[j-1],a.points[j],b.points[j],a.colors[j-1],a.colors[j],b.colors[j]);
            triangle(a.points[j-1],b.points[j],b.points[j-1],a.colors[j-1],b.colors[j],b.colors[j-1]);
        }
    // Round endpoints retain the connection contract at junctions. Closed
    // paths share their seam and therefore do not need endpoint caps.
    if(std::hypot(points.front().x-points.back().x,points.front().y-points.back().y)>.01f)
        for(const std::size_t end:{std::size_t{0},points.size()-1})
        {
            const auto center=points[end];
            const float radius=std::hypot(sections[end].points.back().x-center.x,
                                          sections[end].points.back().y-center.y);
            const auto neighbor=points[end?end-1:1];
            const float direction=std::atan2(center.y-neighbor.y,center.x-neighbor.x);
            for(int slice=0;slice<16;++slice)
            {
                const float a=direction-1.5707963f+slice*3.1415926f/16.f;
                const float b=direction-1.5707963f+(slice+1)*3.1415926f/16.f;
                const MapFeaturePoint pa{center.x+std::cos(a)*radius*.84f,center.y+std::sin(a)*radius*.84f};
                const MapFeaturePoint pb{center.x+std::cos(b)*radius*.84f,center.y+std::sin(b)*radius*.84f};
                const MapFeaturePoint qa{center.x+std::cos(a)*radius,center.y+std::sin(a)*radius};
                const MapFeaturePoint qb{center.x+std::cos(b)*radius,center.y+std::sin(b)*radius};
                triangle(center,pa,pb,vertexColor(0,center),vertexColor(.84f,center),vertexColor(.84f,center));
                triangle(pa,qa,qb,vertexColor(.84f,center),vertexColor(1,center),vertexColor(1,center));
                triangle(pa,qb,pb,vertexColor(.84f,center),vertexColor(1,center),vertexColor(.84f,center));
            }
        }
}

std::vector<AtlasInkPrimitive> buildIllustratedLocalRoadDetailInk(
    const MapModel &map, const MapGeneratedFeature &feature, const std::vector<MapFeaturePoint> &points) {
    std::vector<AtlasInkPrimitive> ink;
    if (!usesIllustratedLocalV6(map) || map.generation.recipe.recipeVersion < 5 || points.size() < 2 ||
        (feature.kind != MapGeneratedFeatureKind::Road && feature.kind != MapGeneratedFeatureKind::Trail)) return ink;
    const float width = std::max(.10f, feature.width * atlasPresentationProfile(map).roadWidthCells *
        (feature.kind == MapGeneratedFeatureKind::Road ? 1.14f : 1.f));
    const bool stoneSurface=map.generation.recipe.recipeVersion>=10 && feature.materialId.find(".stone")!=std::string::npos;
    if(usesNaturalLocalRoadSurface(map))appendNaturalRoadSurfaceInk(ink,map,points,width,stoneSurface);
    // Scattered earth flecks and interrupted ruts add scale without changing the
    // actual route, masking junctions, or placing decoration outside its edge.
    float next = .45f;
    int sample = 0;
    for (std::size_t i=1;i<points.size() && sample<384;++i) {
        const auto a=points[i-1],b=points[i];
        const float length=std::hypot(b.x-a.x,b.y-a.y);
        if(length<.00001f) continue;
        const float tx=(b.x-a.x)/length,ty=(b.y-a.y)/length;
        while(next<length && sample<384) {
            const auto h=localIllustrationHash(map.generation.recipe.seed,sample,static_cast<int>(feature.kind),311u);
            const MapFeaturePoint center{a.x+tx*next,a.y+ty*next};
            if(stoneSurface) {
                // Crosswise masonry courses distinguish stone streets from
                // soil tracks while preserving the exact curved corridor.
                const float half=width*.36f;
                ink.push_back({{{center.x-ty*half,center.y+tx*half},
                                {center.x+ty*half,center.y-tx*half}},
                               {110,121,103,100},.024f,false,10.f});
                next+=.45f+float(h%7u)*.016f;++sample;
                continue;
            }
            const float halfLength=std::min(.04f+float(h%19)*.006f,std::min(next,length-next));
            const float drift=(float((h>>17)%101)/100.f-.5f)*width*.58f;
            const MapFeaturePoint worn{center.x-ty*drift,center.y+tx*drift};
            const float fleckWidth=width*.035f;
            ink.push_back({{{worn.x-tx*halfLength,worn.y-ty*halfLength},
                            {worn.x+tx*halfLength,worn.y+ty*halfLength},
                            {worn.x-ty*fleckWidth,worn.y+tx*fleckWidth}},
                {139,115,78,static_cast<std::uint8_t>(24+(h>>11)%24)},.01f,true});
            for(int side : {-1,1}) {
                if (((h>>(side<0?3:9))&3u)!=0) continue;
                const float offset=side*width*.23f;
                const MapFeaturePoint p{center.x-ty*offset,center.y+tx*offset};
                ink.push_back({{{p.x-tx*halfLength,p.y-ty*halfLength},{p.x+tx*halfLength,p.y+ty*halfLength}},
                    {139,115,78,static_cast<std::uint8_t>(22+h%20)},std::clamp(width*.035f,.018f,.045f),false});
            }
            next+=.19f+float((h>>8)%53)*.017f;
            ++sample;
        }
        next-=length;
    }
    return ink;
}

} // namespace gm::map_render
