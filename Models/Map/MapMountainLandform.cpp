#include "Models/Map/MapIllustratedAtlas.hpp"

namespace gm::map_render
{
// Recipe 8: the valley is a broad lower slope beneath the upright summits,
// not a row of clipped summit glyphs or two continuous retaining walls.
std::vector<AtlasInkPrimitive> buildAtlasMountainLandformInk(
    const MapModel &map, const std::vector<gm::map_geometry::MapRoadCorridor> &corridors)
{
    std::vector<AtlasInkPrimitive> ink;
    const auto ground = buildOverviewGroundProjection(map);
    const auto land = [&](MapFeaturePoint p) {
        return p.x >= 0 && p.y >= 0 && p.x < map.width && p.y < map.height &&
            overviewGroundKindIsLand(overviewGroundKindFromProjection(map, ground, int(p.x), int(p.y)));
    };
    const auto mountain = [&](MapFeaturePoint p) {
        return land(p) && overviewGroundKindFromProjection(map, ground, int(p.x), int(p.y)) == MapTileKind::Mountain;
    };
    for (const auto &route : corridors)
    {
        const auto feature = std::find_if(map.generation.graph.features.begin(), map.generation.graph.features.end(),
            [&](const auto &f) { return f.id == route.featureId; });
        if (feature == map.generation.graph.features.end() ||
            (feature->kind != MapGeneratedFeatureKind::Road && feature->kind != MapGeneratedFeatureKind::River)) continue;
        const bool river = feature->kind == MapGeneratedFeatureKind::River;
        const float edge = river ? std::max(.20f, feature->width*.12f+.10f) : std::max(.19f, feature->width*.15f+.10f);
        auto clearance = corridors;
        for (auto &c : clearance) if (c.featureId == route.featureId) c.radiusCells = edge;
        unsigned salt = map.generation.recipe.seed;
        for (unsigned char ch : route.featureId) salt = (salt ^ ch)*16777619u;
        const float phase = (salt % 4096)*.00153398f;
        const float length = detail::overviewPathLength(route.centerline);
        const auto frame = [&](float d, float offset) {
            d = std::clamp(d, 0.f, length);
            const auto p = detail::sampleOverviewPath(route.centerline, d).point;
            const auto a = detail::sampleOverviewPath(route.centerline, std::max(0.f,d-.18f)).point;
            const auto b = detail::sampleOverviewPath(route.centerline, std::min(length,d+.18f)).point;
            const float n = std::hypot(b.x-a.x,b.y-a.y);
            return n < .001f ? p : MapFeaturePoint{p.x-(b.y-a.y)/n*offset,p.y+(b.x-a.x)/n*offset};
        };
        const auto valid = [&](const std::vector<MapFeaturePoint> &polygon) {
            return std::all_of(polygon.begin(),polygon.end(),land) && atlasSilhouetteClearsRoads(polygon,clearance,.025f);
        };
        // Independent, long wavelength shoulders avoid paired canal walls.
        // Each small strip shares exact end samples with its neighbour. The
        // floor, exposed face and outer apron all use the same cross section.
        for (int side : {-1,1})
        {
            const float sidePhase = phase + (side < 0 ? .7f : 3.1f);
            const auto section = [&](float d) {
                const float wave = .5f+.5f*std::sin(d*.73f+sidePhase);
                const float swell = .5f+.5f*std::sin(d*.31f+sidePhase*1.7f);
                const float floor = edge+.09f+.38f*wave+.24f*swell;
                const float shoulder = floor+.38f+.46f*swell;
                const float reach = shoulder+.85f+.70f*wave;
                return std::array<MapFeaturePoint,3>{frame(d,side*floor),frame(d,side*shoulder),frame(d,side*reach)};
            };
            constexpr float step = .24f;
            struct Slice { std::array<MapFeaturePoint,3> points; float distance; };
            std::vector<Slice> slices;
            for (float d=.16f;d<length-.16f;d+=step) slices.push_back({section(d),d});
            std::vector<bool> usable(slices.size(),false);
            for (std::size_t i=1;i<slices.size();++i) {
                const auto &a=slices[i-1].points,&b=slices[i].points;
                const float d=slices[i].distance-step*.5f;
                if (!mountain(frame(d,0)) && !(mountain(a[2]) && mountain(frame(d,-side*(edge+1.5f))))) continue;
                const auto unfolded=[&](int inner,int outer) {
                    const std::array<MapFeaturePoint,4> q{a[inner],b[inner],b[outer],a[outer]};
                    for(int j=0;j<4;++j) {
                        const auto p=q[j],r=q[(j+1)%4],s=q[(j+2)%4];
                        if(side*((r.x-p.x)*(s.y-r.y)-(r.y-p.y)*(s.x-r.x))<=.00001f)return false;
                    }
                    return true;
                };
                usable[i]=unfolded(0,1) && unfolded(1,2) &&
                    valid({a[0],b[0],b[1],a[1]}) && valid({a[1],b[1],b[2],a[2]});
            }
            // Find continuous pieces first. Every bend, coastal rejection and
            // mountain-range entrance receives a true zero-opacity end cap.
            // Independent triangle rejection must never leave square cut ends.
            for (std::size_t start=1;start<slices.size();) {
                if (!usable[start]) { ++start;continue; }
                std::size_t end=start;while(end+1<slices.size() && usable[end+1])++end;
                const float beginning=slices[start-1].distance,ending=slices[end].distance;
                const auto strength=[&](float d) {
                    const float fade=std::clamp(std::min(d-beginning,ending-d)/1.25f,0.f,1.f);
                    const float wave=.5f+.5f*std::sin(d*.73f+sidePhase);
                    return fade*fade*(3.f-2.f*fade)*(25.f+135.f*wave*wave);
                };
                for(std::size_t i=start;i<=end;++i) {
                    const auto &a=slices[i-1].points,&b=slices[i].points;
                    const float d=slices[i-1].distance;
                    const float alphaA=strength(d),alphaB=strength(slices[i].distance);
                    const auto add=[&](int inner,int outer,OverviewRenderColor color,bool outerSlope) {
                        auto ca=color,cb=color,zero=color;
                        ca.a=static_cast<std::uint8_t>(alphaA);cb.a=static_cast<std::uint8_t>(alphaB);zero.a=0;
                        const auto ai=outerSlope?ca:zero,bi=outerSlope?cb:zero;
                        const auto ao=outerSlope?zero:ca,bo=outerSlope?zero:cb;
                        AtlasInkPrimitive first{{a[inner],b[inner],b[outer]},color,0,true};
                        first.vertexColors={ai,bi,bo};
                        AtlasInkPrimitive second{{a[inner],b[outer],a[outer]},color,0,true};
                        second.vertexColors={ai,bo,ao};
                        ink.push_back(std::move(first));ink.push_back(std::move(second));
                    };
                    add(1,2,{164,172,150,210},true);
                    add(0,1,{139,154,145,235},false);
                    if(std::min(alphaA,alphaB)>105.f) {
                        ink.push_back({{a[1],b[1]},{116,128,109,145},.026f,false});
                        if(i%5==0) {
                            const auto c=section(std::min(d+.48f,ending));
                            std::vector<MapFeaturePoint> crease{a[1],{a[0].x*.65f+c[1].x*.35f,a[0].y*.65f+c[1].y*.35f},c[0]};
                            if(valid(crease))ink.push_back({crease,{116,128,109,105},.022f,false});
                        }
                    }
                }
                start=end+1;
            }
        }
    }
    return ink;
}
}
